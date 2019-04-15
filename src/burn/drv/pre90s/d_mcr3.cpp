// FB Alpha Midway MCR-3 system driver module
// Based on MAME driver by Christopher Kirmse, Aaron Giles

// demoderm		- good
// sarge		- good
// maxrpm		- needs proper inputs
// rampage		- good
// rampage2		- good
// powerdrv		- needs toggle-inputs and fix for slow-mo issue
// stargrds		- good
// spyhunt		- good
// spyhuntp		- good
// crater		- good
// turbotag		- should we bother? looks like a buggy mess

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m68000_intf.h"
#include "m6809_intf.h"
#include "midsg.h"
#include "midtcs.h"
#include "midcsd.h"
#include "midssio.h"
#include "dac.h"
#include "ay8910.h"
#include "watchdog.h"
#include "burn_pal.h"
#include "burn_gun.h" // for dial (crater) etc.
#include "burn_shift.h"
#include "flt_rc.h" // spyhunt mixing
#include "lowpass2.h" // fur spyhunt beefy-engine v1.0 -dink

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *Drv68KROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSndRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvZ80RAM1;
static UINT16 *DrvPalRAM16;
static UINT8 *DrvTransTab[2];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 flipscreen;
static INT32 scrollx;
static INT32 scrolly;
static INT32 input_mux;
static INT32 latched_input;
static UINT8 lamp;
static UINT8 last_op4;

static INT32 sound_status_bit = 8; // default to disabled
static INT32 sound_input_bank = 0;
static INT32 (*port_write_handler)(UINT8 address, UINT8 data) = NULL;
static INT32 (*port_read_handler)(UINT8 address) = NULL;
static INT32 sprite_color_mask = 0;
static INT32 flip_screen_x = 0;
static INT32 nGraphicsLen[3];

static INT32 nExtraCycles[3];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvDips[6];
static UINT8 DrvInputs[6];
static UINT8 DrvReset;
static UINT8 dip_service = 0x20;

static INT32 is_demoderm = 0;
static INT32 is_spyhunt = 0;
static INT32 has_shift = 0;
static INT32 has_dial = 0;
static UINT8 DrvJoy4f[8]; // fake, for digital-dial
static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;
static INT16 DrvAnalogPort2 = 0;
static INT16 DrvAnalogPort3 = 0;

// For spyhunt's engine effect
static class LowPass2 *LP1 = NULL;
#define SampleFreq 44100.0
#define CutFreq 1000.0
#define Q 0.4
#define Gain 1.3
#define CutFreq2 1000.0
#define Q2 0.3
#define Gain2 1.475

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo RampageInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Up",				BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",				BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Up",				BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Down",				BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",				BIT_DIGITAL,	DrvJoy3 + 3,	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"P3 Up",				BIT_DIGITAL,	DrvJoy5 + 0,	"p3 up"		},
	{"P3 Down",				BIT_DIGITAL,	DrvJoy5 + 2,	"p3 down"	},
	{"P3 Left",				BIT_DIGITAL,	DrvJoy5 + 3,	"p3 left"	},
	{"P3 Right",			BIT_DIGITAL,	DrvJoy5 + 1,	"p3 right"	},
	{"P3 Button 1",			BIT_DIGITAL,	DrvJoy5 + 4,	"p3 fire 1"	},
	{"P3 Button 2",			BIT_DIGITAL,	DrvJoy5 + 5,	"p3 fire 2"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",				BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip E",				BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
	{"Dip F",				BIT_DIPSWITCH,	DrvDips + 5,	"dip"		},
};

STDINPUTINFO(Rampage)

static struct BurnInputInfo PowerdrvInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"	},
	{"P1 Button 2",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 2"	},
	{"P1 Button 3",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 3"	},
	{"P1 Button 4",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 4"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 1"	},
	{"P2 Button 2",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 2"	},
	{"P2 Button 3",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 3"	},
	{"P2 Button 4",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 4"	},

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy1 + 2,	"p3 coin"	},
	{"P3 Button 1",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 fire 1"	},
	{"P3 Button 2",			BIT_DIGITAL,	DrvJoy3 + 3,	"p3 fire 2"	},
	{"P3 Button 3",			BIT_DIGITAL,	DrvJoy3 + 0,	"p3 fire 3"	},
	{"P3 Button 4",			BIT_DIGITAL,	DrvJoy3 + 1,	"p3 fire 4"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy1 + 5,	"diag"		},
	{"Tilt",				BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip E",				BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
	{"Dip F",				BIT_DIPSWITCH,	DrvDips + 5,	"dip"		},
};

STDINPUTINFO(Powerdrv)

static struct BurnInputInfo DemodermInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	A("P1 Dial", 		BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	A("P2 Dial", 		BIT_ANALOG_REL, &DrvAnalogPort1,"p2 x-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy5 + 2,	"p3 start"	},
	A("P3 Dial", 		BIT_ANALOG_REL, &DrvAnalogPort2,"p3 x-axis"),
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy5 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy5 + 5,	"p3 fire 2"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy5 + 1,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy5 + 3,	"p4 start"	},
	A("P4 Dial", 		BIT_ANALOG_REL, &DrvAnalogPort3,"p4 x-axis"),
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy5 + 6,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy5 + 7,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 6,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip E",			BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
	{"Dip F",			BIT_DIPSWITCH,	DrvDips + 5,	"dip"		},
};

STDINPUTINFO(Demoderm)

static struct BurnInputInfo SargeInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Right Stick Up",	BIT_DIGITAL,	DrvJoy2 + 2,	"p3 up"		},
	{"P1 Right Stick Down",	BIT_DIGITAL,	DrvJoy2 + 3,	"p3 down"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Left Stick Up",	BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Left Stick Down",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Right Stick Up",	BIT_DIGITAL,	DrvJoy3 + 2,	"p4 up"		},
	{"P2 Right Stick Down",	BIT_DIGITAL,	DrvJoy3 + 3,	"p4 down"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 2"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Tilt",				BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip E",				BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
	{"Dip F",				BIT_DIPSWITCH,	DrvDips + 5,	"dip"		},
};

STDINPUTINFO(Sarge)

static struct BurnInputInfo SpyhuntInputList[] = {
	{"Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"Weapons Van",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"Gear Shift",			BIT_DIGITAL,	DrvJoy4f + 4,	"p1 fire 2"	},
	{"Missiles",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 3"	},
	{"Oil Slick",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 4"	},
	{"Smoke Screen",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 5"	},
	{"Machine Gun",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 6"	},

	A("P1 Wheel",       	BIT_ANALOG_REL, &DrvAnalogPort0, "p1 x-axis"),
	A("P1 Accelerator", 	BIT_ANALOG_REL, &DrvAnalogPort1, "p1 fire 1"),

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
    {"Tilt",				BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip E",				BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
	{"Dip F",				BIT_DIPSWITCH,	DrvDips + 5,	"dip"		},
};

STDINPUTINFO(Spyhunt)

static struct BurnInputInfo CraterInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",				BIT_DIGITAL,	DrvJoy3 + 2,	"p1 up"		},
	{"P1 Down",				BIT_DIGITAL,	DrvJoy3 + 3,	"p1 down"	},
	{"P1 Left",		    	BIT_DIGITAL,	DrvJoy4f + 0,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy4f + 1,	"p1 right"	},
	A("P1 Dial", 			BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 2"	},
	{"P1 Button 3",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 3"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",				BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip E",				BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
	{"Dip F",				BIT_DIPSWITCH,	DrvDips + 5,	"dip"		},
};

STDINPUTINFO(Crater)
#undef A

static struct BurnInputInfo StargrdsInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
    {"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"	},
    {"P1 Left Stick Left",	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"	},
	{"P1 Left Stick Right",	BIT_DIGITAL,	DrvJoy2 + 7,	"p1 right"	},
    {"P1 Right Stick Up",	BIT_DIGITAL,	DrvJoy2 + 0,	"p3 up"		},
	{"P1 Right Stick Down",	BIT_DIGITAL,	DrvJoy2 + 1,	"p3 down"	},
	{"P1 Right Stick Left",	BIT_DIGITAL,	DrvJoy2 + 2,	"p3 left"	},
	{"P1 Right Stick Right",BIT_DIGITAL,	DrvJoy2 + 3,	"p3 right"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
    {"P2 Left Stick Up",	BIT_DIGITAL,	DrvJoy3 + 4,	"p2 up"		},
	{"P2 Left Stick Down",	BIT_DIGITAL,	DrvJoy3 + 5,	"p2 down"	},
    {"P2 Left Stick Left",	BIT_DIGITAL,	DrvJoy3 + 6,	"p2 left"	},
	{"P2 Left Stick Right",	BIT_DIGITAL,	DrvJoy3 + 7,	"p2 right"	},
    {"P2 Right Stick Up",	BIT_DIGITAL,	DrvJoy3 + 0,	"p4 up"		},
	{"P2 Right Stick Down",	BIT_DIGITAL,	DrvJoy3 + 1,	"p4 down"	},
	{"P2 Right Stick Left",	BIT_DIGITAL,	DrvJoy3 + 2,	"p4 left"	},
	{"P2 Right Stick Right",BIT_DIGITAL,	DrvJoy3 + 3,	"p4 right"	},

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy6 + 1,	"p3 coin"	},
	{"P3 Start",			BIT_DIGITAL,	DrvJoy6 + 3,	"p3 start"	},
    {"P3 Left Stick Up",	BIT_DIGITAL,	DrvJoy5 + 4,	"p5 up"		},
	{"P3 Left Stick Down",	BIT_DIGITAL,	DrvJoy5 + 5,	"p5 down"	},
    {"P3 Left Stick Left",	BIT_DIGITAL,	DrvJoy5 + 6,	"p5 left"	},
	{"P3 Left Stick Right",	BIT_DIGITAL,	DrvJoy5 + 7,	"p5 right"	},
    {"P3 Right Stick Up",	BIT_DIGITAL,	DrvJoy5 + 0,	"p6 up"		},
	{"P3 Right Stick Down",	BIT_DIGITAL,	DrvJoy5 + 1,	"p6 down"	},
	{"P3 Right Stick Left",	BIT_DIGITAL,	DrvJoy5 + 2,	"p6 left"	},
	{"P3 Right Stick Right",BIT_DIGITAL,	DrvJoy5 + 3,	"p6 right"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",				BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip E",				BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
	{"Dip F",				BIT_DIPSWITCH,	DrvDips + 5,	"dip"		},
};

STDINPUTINFO(Stargrds)

static struct BurnDIPInfo StargrdsDIPList[]=
{
	{0x21, 0xff, 0xff, 0xff, NULL							},
	{0x22, 0xff, 0xff, 0xff, NULL							},
	{0x23, 0xff, 0xff, 0xff, NULL							},
	{0x24, 0xff, 0xff, 0xff, NULL							},
	{0x25, 0xff, 0xff, 0xff, NULL							},
	{0x26, 0xff, 0xff, 0x80, NULL							},

	{0   , 0xfe, 0   ,    4, "Energy Units"					},
	{0x24, 0x01, 0x0c, 0x08, "8"							},
	{0x24, 0x01, 0x0c, 0x0c, "10"							},
	{0x24, 0x01, 0x0c, 0x04, "12"							},
	{0x24, 0x01, 0x0c, 0x00, "14"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x24, 0x01, 0x20, 0x00, "Off"							},
	{0x24, 0x01, 0x20, 0x20, "On"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x24, 0x01, 0xc0, 0x80, "Easy"							},
	{0x24, 0x01, 0xc0, 0xc0, "Medium"						},
	{0x24, 0x01, 0xc0, 0x40, "Hard"							},
	{0x24, 0x01, 0xc0, 0x00, "Hardest"						},

    {0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x26, 0x01, 0x80, 0x80, "Off"							},
	{0x26, 0x01, 0x80, 0x00, "On"							},
};

STDDIPINFO(Stargrds)

static struct BurnDIPInfo PowerdrvDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL							},
	{0x14, 0xff, 0xff, 0xff, NULL							},
	{0x15, 0xff, 0xff, 0xff, NULL							},
	{0x16, 0xff, 0xff, 0xff, NULL							},
	{0x17, 0xff, 0xff, 0xff, NULL							},
	{0x18, 0xff, 0xff, 0x20, NULL							},

	{0   , 0xfe, 0   ,    3, "Coinage"						},
	{0x16, 0x01, 0x03, 0x02, "2 Coins 1 Credits"			},
	{0x16, 0x01, 0x03, 0x03, "1 Coin  1 Credits"			},
	{0x16, 0x01, 0x03, 0x01, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0x30, 0x20, "Easy"							},
	{0x16, 0x01, 0x30, 0x30, "Factory"						},
	{0x16, 0x01, 0x30, 0x10, "Hard"							},
	{0x16, 0x01, 0x30, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x16, 0x01, 0x40, 0x00, "Off"							},
	{0x16, 0x01, 0x40, 0x40, "On"							},

	{0   , 0xfe, 0   ,    2, "Rack Advance (Cheat)"			},
	{0x16, 0x01, 0x80, 0x80, "Off"							},
	{0x16, 0x01, 0x80, 0x00, "On"							},

    {0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x18, 0x01, 0x20, 0x20, "Off"							},
	{0x18, 0x01, 0x20, 0x00, "On"							},
};

STDDIPINFO(Powerdrv)

static struct BurnDIPInfo RampageDIPList[]=
{
	{0x17, 0xff, 0xff, 0xff, NULL							},
	{0x18, 0xff, 0xff, 0xff, NULL							},
	{0x19, 0xff, 0xff, 0xff, NULL							},
	{0x1a, 0xff, 0xff, 0xff, NULL							},
	{0x1b, 0xff, 0xff, 0xff, NULL							},
	{0x1c, 0xff, 0xff, 0x20, NULL							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x1a, 0x01, 0x03, 0x02, "Easy"							},
	{0x1a, 0x01, 0x03, 0x03, "Normal"						},
	{0x1a, 0x01, 0x03, 0x01, "Hard"							},
	{0x1a, 0x01, 0x03, 0x00, "Free Play"					},

	{0   , 0xfe, 0   ,    2, "Score Option"					},
	{0x1a, 0x01, 0x04, 0x04, "Keep score when continuing"	},
	{0x1a, 0x01, 0x04, 0x00, "Lose score when continuing"	},

	{0   , 0xfe, 0   ,    2, "Coin A"						},
	{0x1a, 0x01, 0x08, 0x00, "2 Coins 1 Credits"			},
	{0x1a, 0x01, 0x08, 0x08, "1 Coin  1 Credits"			},

	{0   , 0xfe, 0   ,    8, "Coin B"						},
	{0x1a, 0x01, 0x70, 0x00, "3 Coins 1 Credits"			},
	{0x1a, 0x01, 0x70, 0x10, "2 Coins 1 Credits"			},
	{0x1a, 0x01, 0x70, 0x70, "1 Coin  1 Credits"			},
	{0x1a, 0x01, 0x70, 0x60, "1 Coin  2 Credits"			},
	{0x1a, 0x01, 0x70, 0x50, "1 Coin  3 Credits"			},
	{0x1a, 0x01, 0x70, 0x40, "1 Coin  4 Credits"			},
	{0x1a, 0x01, 0x70, 0x30, "1 Coin  5 Credits"			},
	{0x1a, 0x01, 0x70, 0x20, "1 Coin  6 Credits"			},

	{0   , 0xfe, 0   ,    2, "Rack Advance (Cheat)"			},
	{0x1a, 0x01, 0x80, 0x80, "Off"							},
	{0x1a, 0x01, 0x80, 0x00, "On"							},

    {0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x1c, 0x01, 0x20, 0x20, "Off"							},
	{0x1c, 0x01, 0x20, 0x00, "On"							},
};

STDDIPINFO(Rampage)

static struct BurnDIPInfo DemodermDIPList[]=
{
	{0x17, 0xff, 0xff, 0xff, NULL							},
	{0x18, 0xff, 0xff, 0xff, NULL							},
	{0x19, 0xff, 0xff, 0xff, NULL							},
	{0x1a, 0xff, 0xff, 0xff, NULL							},
	{0x1b, 0xff, 0xff, 0xff, NULL							},
	{0x1c, 0xff, 0xff, 0x20, NULL							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x1a, 0x01, 0x01, 0x01, "2P Upright"					},
	{0x1a, 0x01, 0x01, 0x00, "4P Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"					},
	{0x1a, 0x01, 0x02, 0x02, "Normal"						},
	{0x1a, 0x01, 0x02, 0x00, "Harder"						},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x1a, 0x01, 0x04, 0x04, "Off"							},
	{0x1a, 0x01, 0x04, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Reward Screen"				},
	{0x1a, 0x01, 0x08, 0x08, "Expanded"						},
	{0x1a, 0x01, 0x08, 0x00, "Limited"						},

	{0   , 0xfe, 0   ,    4, "Coinage"						},
	{0x1a, 0x01, 0x30, 0x20, "2 Coins 1 Credits"			},
	{0x1a, 0x01, 0x30, 0x00, "2 Coins 2 Credits"			},
	{0x1a, 0x01, 0x30, 0x30, "1 Coin  1 Credits"			},
	{0x1a, 0x01, 0x30, 0x10, "1 Coin  2 Credits"			},

    {0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x1c, 0x01, 0x20, 0x20, "Off"							},
	{0x1c, 0x01, 0x20, 0x00, "On"							},
};

STDDIPINFO(Demoderm)

static struct BurnDIPInfo SargeDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL							},
	{0x14, 0xff, 0xff, 0xff, NULL							},
	{0x15, 0xff, 0xff, 0xff, NULL							},
	{0x16, 0xff, 0xff, 0xff, NULL							},
	{0x17, 0xff, 0xff, 0xff, NULL							},
	{0x18, 0xff, 0xff, 0x20, NULL							},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x16, 0x01, 0x08, 0x08, "Off"							},
	{0x16, 0x01, 0x08, 0x00, "On"							},

	{0   , 0xfe, 0   ,    3, "Coinage"						},
	{0x16, 0x01, 0x30, 0x20, "2 Coins 1 Credits"			},
	{0x16, 0x01, 0x30, 0x30, "1 Coin  1 Credits"			},
	{0x16, 0x01, 0x30, 0x10, "1 Coin  2 Credits"			},

    {0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x18, 0x01, 0x20, 0x20, "Off"							},
	{0x18, 0x01, 0x20, 0x00, "On"							},
};

STDDIPINFO(Sarge)

static struct BurnDIPInfo SpyhuntDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xff, NULL							},
	{0x0d, 0xff, 0xff, 0xff, NULL							},
	{0x0e, 0xff, 0xff, 0xff, NULL							},
	{0x0f, 0xff, 0xff, 0xff, NULL							},
	{0x10, 0xff, 0xff, 0xff, NULL							},
	{0x11, 0xff, 0xff, 0x80, NULL							},

	{0   , 0xfe, 0   ,    2, "Game Timer"					},
	{0x0f, 0x01, 0x01, 0x00, "1:00"							},
	{0x0f, 0x01, 0x01, 0x01, "1:30"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x0f, 0x01, 0x02, 0x02, "Off"							},
	{0x0f, 0x01, 0x02, 0x00, "On"							},

    {0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x11, 0x01, 0x80, 0x80, "Off"							},
	{0x11, 0x01, 0x80, 0x00, "On"							},
};

STDDIPINFO(Spyhunt)

static struct BurnDIPInfo CraterDIPList[]=
{
	{0x0d, 0xff, 0xff, 0xff, NULL							},
	{0x0e, 0xff, 0xff, 0xff, NULL							},
	{0x0f, 0xff, 0xff, 0xff, NULL							},
	{0x10, 0xff, 0xff, 0xff, NULL							},
	{0x11, 0xff, 0xff, 0xff, NULL							},
	{0x12, 0xff, 0xff, 0x80, NULL							},

    {0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x12, 0x01, 0x80, 0x80, "Off"							},
	{0x12, 0x01, 0x80, 0x00, "On"							},
};

STDDIPINFO(Crater)

static UINT8 soundsgood_input_read(UINT8 address)
{
	address &= 7;
	UINT8 ret = DrvInputs[address];

	if (sound_input_bank == address)
	{
		ret &= ~(1 << sound_status_bit);
		if (soundsgood_status_read()) ret |= 1 << sound_status_bit;
	}

//	bprintf (0, _T("Input: %d, %2.2x\n"), address, ret);

	return ret;
}

static void __fastcall mcrmono_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0xec00) {
		DrvPalRAM16[(address >> 1) & 0x3f] = data | ((address & 1) << 8);
		return;
	}

	bprintf (0, _T("MW: %4.4x, %2.2x\n"), address, data);
}

static UINT8 __fastcall mcrmono_read(UINT16 address)
{
	bprintf (0, _T("MR: %4.4x\n"), address);

	return 0;
}

static void __fastcall mcrmono_write_port(UINT16 address, UINT8 data)
{
	address &= 0xff;

//	bprintf (0, _T("WP: %2.2x, %2.2x\n"), address & 0xff, data);

	if (port_write_handler) {
		if (port_write_handler(address, data) != -1) return;
	}

	switch (address)
	{
		case 0x05:
			// coin counter = data & 0x01
			flipscreen = (data & 0x40) >> 6;
		return;

		//case 0x06:
		//return;

		case 0x07:
			BurnWatchdogWrite();
		return;

		case 0xf0:
		case 0xf1:
		case 0xf2:
		case 0xf3:
			z80ctc_write(address & 3, data);
		return;
	}
}

static UINT8 __fastcall mcrmono_read_port(UINT16 address)
{
	address &= 0xff;

//	bprintf (0, _T("RP: %2.2x\n"), address & 0xff);

	if (port_read_handler) {
		INT32 ret = port_read_handler(address);
		if (ret != -1) return ret;
	}

	switch (address & ~3)
	{
		case 0x00:
		case 0x04:
			return soundsgood_input_read(address);

		case 0xf0:
			return z80ctc_read(address & 3);
	}

	return 0;
}
static void __fastcall spyhunt_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfe00) == 0xfa00) {
		DrvPalRAM16[(address >> 1) & 0x3f] = data | ((address & 1) << 8);
		return;
	}
}

static void __fastcall spyhunt_write_port(UINT16 address, UINT8 data)
{
	switch (address & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			// coin counter = data & 0x01
			flipscreen = (data & 0x40) >> 6;
		break; // yes!

		case 0x84:
			scrollx = (scrollx & 0x700) | data;
		return;

		case 0x85:
			scrollx = (scrollx & 0x0ff) | ((data & 7) << 8);
			scrolly = (scrolly & 0x0ff) | ((data >> 7) << 8);
		return;

		case 0x86:
			scrolly = (scrolly & 0x100) | data;
		return;

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

static UINT8 __fastcall spyhunt_read_port(UINT16 address)
{
	switch (address & ~3)
	{
		case 0xf0:
			return z80ctc_read(address & 3);
	}

	return ssio_read_ports(address);
}

static void ctc_interrupt(INT32 state)
{
    if (state) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
    //ZetSetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[offs * 2 + 1];
	INT32 code = DrvVidRAM[offs * 2 + 0] | ((attr & 0x03) << 8) | ((attr & 0x40) << 4);

	TILE_SET_INFO(0, code, (attr >> 4) ^ 3, TILE_FLIPYX(attr >> 2));
}

static tilemap_scan( spybg )
{
	return (row & 0x0f) | ((col & 0x3f) << 4) | ((row & 0x10) << 6);
}

static tilemap_callback( spybg )
{
	INT32 attr = DrvVidRAM[offs];

	TILE_SET_INFO(0, (attr & 0x3f) | ((attr >> 7) << 6), 0, (attr & 0x40) ? TILE_FLIPY : 0);
}

static tilemap_callback( txt )
{
	TILE_SET_INFO(1, DrvTxtRAM[offs], 0, 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	csd_reset();
	tcs_reset();
	soundsgood_reset();
	ssio_reset();

	BurnWatchdogReset();

    if (has_shift) BurnShiftReset();

	input_mux = 0;
	flipscreen = 0;
	scrollx = 0;
	scrolly = 0;
	latched_input = 0;

    lamp = 0;
    last_op4 = 0;

    nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = 0;

    return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x010000;
	DrvZ80ROM1		= Next; Next += 0x010000;
	DrvSndROM		= Next;
	Drv68KROM		= Next; Next += 0x040000;

	DrvGfxROM0		= Next; Next += 0x080000;
	DrvGfxROM1		= Next; Next += 0x080000 + (32 * 32 * 8);
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvSndPROM		= Next; Next += 0x000020;

	DrvTransTab[0]	= Next; Next += 0x000040;
	DrvTransTab[1]	= Next; Next += 0x000040;

	DrvPalette		= (UINT32*)Next; Next += 0x044 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000800;

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvPalRAM16		= (UINT16*)Next; Next += 0x000040 * sizeof(UINT16);
	DrvSndRAM		= Next; Next += 0x001000;
	Drv68KRAM		= Next; Next += 0x001000;
	DrvTxtRAM		= Next; Next += 0x000400;
	DrvZ80RAM1		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvBuildTransTab()
{
	for (INT32 i = 0; i < 16*4; i++) { // 4bpp pixels * 4 color banks
		DrvTransTab[0][i] = (0x0101 & (1 << (i & 0xf))) ? 0xff : 0;
		DrvTransTab[1][i] = (0xfeff & (1 << (i & 0xf))) ? 0xff : 0;
	}
}

static void copy_and_rotate(UINT8 *src, UINT8 *dst)
{
	for (INT32 y = 0; y < 32; y++)
	{
		for (INT32 x = 0; x < 32; x++)
		{
			dst[(y * 32) + x] = src[(31-x)*32+y];
		}
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { (nGraphicsLen[0]/2)*8+0, (nGraphicsLen[0]/2)*8+1, 0, 1 };
	INT32 XOffs0[16] = { STEP8(0,2) };
	INT32 YOffs0[16] = { STEP8(0,16) };

	INT32 L = (nGraphicsLen[1]/4)*8;
	INT32 Plane1[4]  = { STEP4(0,1) };
	INT32 XOffs1[32] = {
		L*0+0, L*0+4, L*1+0, L*1+4, L*2+0, L*2+4, L*3+0, L*3+4,
		L*0+0+8, L*0+4+8, L*1+0+8, L*1+4+8, L*2+0+8, L*2+4+8, L*3+0+8, L*3+4+8,
		L*0+0+16, L*0+4+16, L*1+0+16, L*1+4+16, L*2+0+16, L*2+4+16, L*3+0+16, L*3+4+16,
		L*0+0+24, L*0+4+24, L*1+0+24, L*1+4+24, L*2+0+24, L*2+4+24, L*3+0+24, L*3+4+24
	};
	INT32 YOffs1[32] = { STEP32(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	GfxDecode((nGraphicsLen[0] * 2) / (8 * 8), 4, 8, 8, Plane0, XOffs0, YOffs0, 0x080, DrvGfxROM0, tmp);

	for (INT32 i = 0; i < nGraphicsLen[0] * 2; i+=0x40) { // 2x size and invert pixel
		for (INT32 y = 0; y < 16; y++) {
			for (INT32 x = 0; x < 16; x++) {
				DrvGfxROM0[(i * 4) + (y * 16) + x] = tmp[i + ((y / 2) * 8) + (x / 2)] ^ 0xf;
			}
		}
	}

	memcpy (tmp, DrvGfxROM1, nGraphicsLen[1]);

	GfxDecode((nGraphicsLen[1] * 2) / (32 * 32), 4, 32, 32, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 SpyhuntGfxDecode()
{
	INT32 Plane0[2] = { 0, 1 };
	INT32 XOffs0[16] = { 0, 0, 2, 2, 4, 4, 6, 6, 8, 8, 10, 10, 12, 12, 14, 14 };
	INT32 YOffs0[16] = { 0, 0, 16, 16, 32, 32, 48, 48, 64, 64, 80, 80, 96, 96, 112, 112 };

	INT32 L = (nGraphicsLen[1]/4)*8;
	INT32 Plane1[4]  = { STEP4(0,1) };
	INT32 XOffs1[32] = {
		L*0+0, L*0+4, L*1+0, L*1+4, L*2+0, L*2+4, L*3+0, L*3+4,
		L*0+0+8, L*0+4+8, L*1+0+8, L*1+4+8, L*2+0+8, L*2+4+8, L*3+0+8, L*3+4+8,
		L*0+0+16, L*0+4+16, L*1+0+16, L*1+4+16, L*2+0+16, L*2+4+16, L*3+0+16, L*3+4+16,
		L*0+0+24, L*0+4+24, L*1+0+24, L*1+4+24, L*2+0+24, L*2+4+24, L*3+0+24, L*3+4+24
	};
	INT32 YOffs1[32] = { STEP32(0,32) };

	INT32 Plane2[4]  = { (nGraphicsLen[0]/2)*8+0, (nGraphicsLen[0]/2)*8+1, 0, 1 };
	INT32 XOffs2[64] = {
			0,  0,  2,  2,  4,  4,  6,  6,  8,  8, 10, 10, 12, 12, 14, 14,
			16, 16, 18, 18, 20, 20, 22, 22, 24, 24, 26, 26, 28, 28, 30, 30,
			32, 32, 34, 34, 36, 36, 38, 38, 40, 40, 42, 42, 44, 44, 46, 46,
			48, 48, 50, 50, 52, 52, 54, 54, 56, 56, 58, 58, 60, 60, 62, 62
	};
	INT32 YOffs2[32] = {
			0*32,  0*32,  2*32,  2*32,  4*32,  4*32,  6*32,  6*32,
			8*32,  8*32, 10*32, 10*32, 12*32, 12*32, 14*32, 14*32,
			16*32, 16*32, 18*32, 18*32, 20*32, 20*32, 22*32, 22*32,
			24*32, 24*32, 26*32, 26*32, 28*32, 28*32, 30*32, 30*32
	};

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, nGraphicsLen[0]);

	GfxDecode((nGraphicsLen[0] * 8) / (64 * 32), 4, 64, 32, Plane2, XOffs2, YOffs2, 0x400, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, nGraphicsLen[1]);

	GfxDecode((nGraphicsLen[1] * 2) / (32 * 32), 4, 32, 32, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, nGraphicsLen[2]);

	GfxDecode((nGraphicsLen[2] * 8 * 2) / (16 * 16), 2, 16, 16, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static void ctc_trigger(INT32 , UINT8 data)
{
	z80ctc_trg_write(1, data);
}

static void sound_system_init(INT32 sound_system)
{
	BurnLoadRom(DrvSndPROM + 0x00000, 0x80, 1);

	switch (sound_system)
	{
		case 0: soundsgood_init(0, 0, Drv68KROM, Drv68KRAM); break;
		case 1: tcs_init(0, 0, 0, DrvSndROM, DrvSndRAM); break;

		case 2:
		{
			csd_init(Drv68KROM, Drv68KRAM);
			ssio_init(DrvZ80ROM1, DrvZ80RAM1, DrvSndPROM);
		}
		break;

		case 3:
		{
			ssio_init(DrvZ80ROM1, DrvZ80RAM1, DrvSndPROM);
		}
		break;

        case 4: csd_init(Drv68KROM, Drv68KRAM); break;
	}

	ssio_inputs = DrvInputs;
	ssio_dips = 0xff; // unused in mcr3
}

static INT32 DrvLoadRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[4] = { DrvZ80ROM0, DrvZ80ROM1, DrvSndROM, Drv68KROM };
	UINT8 *gLoad[3] = { DrvGfxROM0, DrvGfxROM1, DrvGfxROM2 };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && ((ri.nType & 7) <= 2)) {
			INT32 type = (ri.nType - 1) & 1;
            if (bLoad) if (BurnLoadRom(pLoad[type], i, 1)) return 1;
			pLoad[type] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_PRG) && ((ri.nType & 7) == 3)) {
			INT32 type = 2;
            if (bLoad) {
				memmove (pLoad[type], pLoad[type] + ri.nLen, 0x10000 - ri.nLen); // m6809 loaded at end of mem space
				if (BurnLoadRom(pLoad[type] + 0x10000 - ri.nLen, i, 1)) return 1;
			}
			continue;
		}

		if ((ri.nType & BRF_PRG) && ((ri.nType & 7) == 4)) {
			INT32 type = 3;
            if (bLoad) {
				if (BurnLoadRom(pLoad[type] + 1, i, 2)) return 1;
				if (BurnLoadRom(pLoad[type] + 0, i+1, 2)) return 1;
			}
			i++;
			pLoad[type] += ri.nLen * 2;
			continue;
		}

		if ((ri.nType & BRF_GRA) && ((ri.nType & 7) <= 3)) {
			INT32 type = (ri.nType - 1) & 3;
			if (bLoad) if (BurnLoadRom(gLoad[type], i, 1)) return 1;
			gLoad[type] += ri.nLen;
			continue;
		}
	}

	nGraphicsLen[0] = gLoad[0] - DrvGfxROM0;
	nGraphicsLen[1] = gLoad[1] - DrvGfxROM1;
	nGraphicsLen[2] = gLoad[2] - DrvGfxROM2;

	return 0;
}

static INT32 DrvInit(INT32 sound_system)
{
	BurnSetRefreshRate(30.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvLoadRoms(true)) return 1;

	DrvGfxDecode();
	DrvBuildTransTab();

	memset (DrvZ80ROM0 + 0xf800, 0xff, 0x800); // unmapped

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,			0x0000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvNVRAM,				0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0xe800, 0xebff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,				0xf000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM0 + 0xf800,	0xf800, 0xffff, MAP_ROM);
	ZetSetWriteHandler(mcrmono_write);
	ZetSetReadHandler(mcrmono_read);
	ZetSetInHandler(mcrmono_read_port);
	ZetSetOutHandler(mcrmono_write_port);

    ZetDaisyInit(Z80_CTC, 0);
	z80ctc_init(5000000, 0, ctc_interrupt, ctc_trigger, NULL, NULL);
	ZetClose();

	sound_system_init(sound_system);

	BurnWatchdogInit(DrvDoReset, -1); // power drive doesn't like 16

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 30);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 16, 16, (nGraphicsLen[0] * 2 * 2 * 2), 0, 3);

    BurnTrackballInit(2);

	DrvDoReset(1);

	return 0;
}

static INT32 SpyhuntCommonInit(INT32 sound_system)
{
	BurnSetRefreshRate(30.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvLoadRoms(true)) return 1;

	SpyhuntGfxDecode();
	DrvBuildTransTab();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,			0xe800, 0xebff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,			0xec00, 0xefff, MAP_RAM);
	ZetMapMemory(DrvNVRAM,			0xf000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xf800, 0xf9ff, MAP_RAM);
	ZetSetWriteHandler(spyhunt_write);
	ZetSetOutHandler(spyhunt_write_port);
	ZetSetInHandler(spyhunt_read_port);

    ZetDaisyInit(Z80_CTC, 0);
	z80ctc_init(5000000, 0, ctc_interrupt, ctc_trigger, NULL, NULL);
	ZetClose();

    if (is_spyhunt) {
        ssio_spyhunter = 1;
        filter_rc_init(0, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 0);
        filter_rc_init(1, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
        filter_rc_init(2, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
        filter_rc_init(3, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
        filter_rc_init(4, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
        filter_rc_init(5, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);

        filter_rc_set_route(0, 1.00, FLT_RC_PANNEDLEFT);
        filter_rc_set_route(1, 1.00, FLT_RC_PANNEDLEFT);
        filter_rc_set_route(2, 1.00, FLT_RC_PANNEDLEFT);
        filter_rc_set_route(3, 1.00, FLT_RC_PANNEDRIGHT);
        filter_rc_set_route(4, 1.00, FLT_RC_PANNEDRIGHT);
        filter_rc_set_route(5, 1.00, FLT_RC_PANNEDRIGHT);

        LP1 = new LowPass2(CutFreq, SampleFreq, Q, Gain, CutFreq2, Q2, Gain2);
    }

    sound_system_init(sound_system);

	BurnWatchdogInit(DrvDoReset, -1);

	GenericTilesInit();
	GenericTilemapInit(0, spybg_map_scan , spybg_map_callback, 64, 32, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, txt_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 64, 32, 0x40000, 0x30, 0);
	GenericTilemapSetGfx(1, DrvGfxROM2, 2, 16, 16, 0x10000, 0x40, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(0, (sound_system == 3) ? -96 : -16, 0); 
	GenericTilemapSetOffsets(1, -16, 0);

    if (is_spyhunt) {
        copy_and_rotate(DrvGfxROM1 + 0x29*0x20*0x20, DrvGfxROM1 + (0x200*0x20*0x20)+(0x20*0x20*0));
        copy_and_rotate(DrvGfxROM1 + 0x2a*0x20*0x20, DrvGfxROM1 + (0x200*0x20*0x20)+(0x20*0x20*1));
        copy_and_rotate(DrvGfxROM1 + 0x45*0x20*0x20, DrvGfxROM1 + (0x200*0x20*0x20)+(0x20*0x20*2));
        copy_and_rotate(DrvGfxROM1 + 0x7b*0x20*0x20, DrvGfxROM1 + (0x200*0x20*0x20)+(0x20*0x20*3));
        copy_and_rotate(DrvGfxROM1 + 0x0f*0x20*0x20, DrvGfxROM1 + (0x200*0x20*0x20)+(0x20*0x20*4));
    }

    BurnTrackballInit(2);

    if (has_shift)
        BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_RED, 80);

    DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();

	csd_exit();
	tcs_exit();
	soundsgood_exit();
	ssio_exit();

    if (is_spyhunt) {
        filter_rc_exit();
        delete LP1;
        LP1 = NULL;
    }

	BurnWatchdogExit();

    BurnTrackballExit();
    if (has_shift) BurnShiftExit();

	sound_status_bit = 8; // default to disabled
	sound_input_bank = 0;
	port_write_handler = NULL;
	port_read_handler = NULL;
	sprite_color_mask = 0;
	flip_screen_x = 0;

    is_demoderm = 0;
    is_spyhunt = 0;
    has_dial = 0;
    has_shift = 0;

    dip_service = 0x20;

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x40; i++)
	{
		UINT8 r = pal3bit(DrvPalRAM16[i] >> 6);
		UINT8 g = pal3bit(DrvPalRAM16[i] & 7);
		UINT8 b = pal3bit((DrvPalRAM16[i] >> 3) & 7);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}

	// spyhunt hardware
	DrvPalette[0x40] = 0;
	DrvPalette[0x41] = BurnHighCol(0x00, 0xff, 0x00, 0);
	DrvPalette[0x42] = BurnHighCol(0x00, 0x00, 0xff, 0);
	DrvPalette[0x43] = BurnHighCol(0xff, 0xff, 0xff, 0);
}

static void draw_sprites(INT32 color_mask, INT32 dx, INT32 dy)
{
	INT32 codemask = (nGraphicsLen[1] * 2) / (32 * 32);

	for (INT32 offs = 0x200 - 4; offs >= 0; offs -= 4)
	{
		if (DrvSprRAM[offs] == 0) continue;

		INT32 flags = DrvSprRAM[offs + 1];
		INT32 code = DrvSprRAM[offs + 2] + 256 * ((flags >> 3) & 0x01);
		INT32 color = ~flags & color_mask;
		INT32 flipx = flags & 0x10;
		INT32 flipy = flags & 0x20;
		INT32 sx = (DrvSprRAM[offs + 3] - 3) * 2;
		INT32 sy = (241 - DrvSprRAM[offs]) * 2;

		sx += dx;
		sy += dy;

		if (flip_screen_x) {
			sx = (nScreenWidth - 32) - sx;
			flipx ^= 0x10;
		}

		if (!flipscreen)
		{
			if (nSpriteEnable & 2) RenderPrioMaskTranstabSprite(pTransDraw, DrvGfxROM1, code % codemask, (color * 16), 0xff, sx, sy, flipx, flipy, 32, 32, DrvTransTab[0], 0);
			if (nSpriteEnable & 4) RenderPrioMaskTranstabSprite(pTransDraw, DrvGfxROM1, code % codemask, (color * 16), 0xff, sx, sy, flipx, flipy, 32, 32, DrvTransTab[1], 2);
		}
		else
		{
			RenderPrioMaskTranstabSprite(pTransDraw, DrvGfxROM1, code % codemask, (color * 16), 0xff, 480 - sx, 452 - sy, !flipx, !flipy, 32, 32, DrvTransTab[0], 0);
			RenderPrioMaskTranstabSprite(pTransDraw, DrvGfxROM1, code % codemask, (color * 16), 0xff, 480 - sx, 452 - sy, !flipx, !flipy, 32, 32, DrvTransTab[1], 2);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

    BurnTransferClear();

	GenericTilemapSetFlip(0, flipscreen ? (TMAP_FLIPX | TMAP_FLIPY) : 0);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 1/*must be this!*/);

	if (nSpriteEnable & 1) draw_sprites(0x03, 0, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 SpyhuntDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

    BurnTransferClear();

	GenericTilemapSetFlip(TMAP_GLOBAL, (flipscreen ? (TMAP_FLIPX | TMAP_FLIPY) : 0) ^ flip_screen_x);
	GenericTilemapSetScrollX(0, scrollx * 2);
	GenericTilemapSetScrollY(0, scrolly * 2);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites(sprite_color_mask, -12, 0);

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

    if (is_spyhunt) {
        if (lamp&0x04) {
            // 29+2a, weapons van (pre-rotated)
            RenderZoomedTile(pTransDraw, DrvGfxROM1, 0x200, 0, 0, nScreenWidth-16, 32, 0, 0, 32, 32, 0x10000/2, 0x10000/2);
            RenderZoomedTile(pTransDraw, DrvGfxROM1, 0x201, 0, 0, nScreenWidth-16, 48, 0, 0, 32, 32, 0x10000/2, 0x10000/2);
        }
        if (lamp&0x01) {
            // oil
            RenderZoomedTile(pTransDraw, DrvGfxROM1, 0x016, 0, 0, nScreenWidth-16, 64, 0, 0, 32, 32, 0x10000/2, 0x10000/2);
        }
        if (lamp&0x08) {
            // smoke
            RenderZoomedTile(pTransDraw, DrvGfxROM1, 0x019, 0, 0, nScreenWidth-16, 80, 0, 0, 32, 32, 0x10000/2, 0x10000/2);
        }
        if (lamp&0x02) {
            // missile (pre-rotated)
            RenderZoomedTile(pTransDraw, DrvGfxROM1, 0x202, 0, 0, nScreenWidth-24, 96, 0, 0, 32, 32, 0x10000, 0x10000);
        }
        if (lamp&0x10) {
            // machine guns (pre-rotated)
            //RenderZoomedTile(pTransDraw, DrvGfxROM1, 0x204, 0, 0, nScreenWidth-16, 128, 1, 0, 32, 32, 0x10000/2, 0x10000/2);
            //RenderZoomedTile(pTransDraw, DrvGfxROM1, 0x203, 0, 0, nScreenWidth-16, 144, 1, 0, 32, 32, 0x10000/2, 0x10000/2);
        }
    }
	BurnTransferCopy(DrvPalette);

    if (has_shift) BurnShiftRenderDoubleSize();

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

static void build_inputs()
{
	memcpy (DrvInputs, DrvDips, 5); // 0xff-filled?
	DrvInputs[5] = 0xff;

	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
	}

    DrvInputs[0] = (DrvInputs[0] & ~dip_service) | (DrvDips[5] & dip_service);

    { // analog stuff
        if (has_dial) { // etc
            BurnTrackballConfig(0, AXIS_REVERSED, AXIS_REVERSED);
            BurnTrackballFrame(0, DrvAnalogPort0, DrvAnalogPort1, 7, 10);
            BurnTrackballUDLR(0, 0, 0, DrvJoy4f[0], DrvJoy4f[1]);
            BurnTrackballUpdate(0);
        }

        if (is_demoderm) {
            BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
            BurnTrackballFrame(0, DrvAnalogPort0, DrvAnalogPort1, 2, 5);
            BurnTrackballUpdate(0);

            BurnTrackballConfig(1, AXIS_NORMAL, AXIS_NORMAL);
            BurnTrackballFrame(1, DrvAnalogPort2, DrvAnalogPort3, 2, 5);
            BurnTrackballUpdate(1);
        }

        if (has_shift) { // gear shifter stuff
			BurnShiftInputCheckToggle(DrvJoy4f[4]);

			DrvInputs[0] &= ~0x10;
            DrvInputs[0] |= (bBurnShiftStatus) ? 0x00 : 0x10;
		}
    }
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();
	SekNewFrame();

	build_inputs();

	INT32 nInterleave = 480;
	INT32 nCyclesTotal[2] = { 5000000 / 30, 8000000 / 30 };
	INT32 nCyclesDone[2] = { nExtraCycles[0], 0 };

	ZetOpen(0);
	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
        nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		mcr_interrupt(i);

		INT32 nCycles = (((i * 1) * nCyclesTotal[1]) / nInterleave) - SekTotalCycles();

		if (soundsgood_reset_status())
		{
            nCyclesDone[1] += SekIdle(nCycles);
		}
		else
		{
			nCyclesDone[1] += SekRun(nCycles);
		}
	}

    if (pBurnSoundOut) {
        BurnSoundClear();
        DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();
	ZetClose();

    nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 TcsFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();
	M6809NewFrame();

	build_inputs();

	INT32 nInterleave = 480;
	INT32 nCyclesTotal[2] = { 5000000 / 30, 2000000 / 30 };
	INT32 nCyclesDone[2] = { nExtraCycles[0], 0 };

	ZetOpen(0);
	M6809Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
        nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
        mcr_interrupt(i);

		if (tcs_reset_status())
		{
            nCyclesDone[1] += M6809Idle(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		}
		else
		{
            nCyclesDone[1] += M6809Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		}
	}

	if (pBurnSoundOut) {
        BurnSoundClear();
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();
	ZetClose();

    nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 CSDSSIOFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	INT32 has_ssio = ssio_initialized();
	INT32 has_csd = csd_initialized();

	ZetNewFrame();
	if (has_csd) SekNewFrame();

	build_inputs();

	INT32 nInterleave = 480;
	INT32 nCyclesTotal[3] = { 5000000 / 30, 8000000 / 30, 2000000 / 30 };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2] };
	INT32 nSoundBufferPos = 0;

    if (has_csd) {
        SekOpen(0);
        SekIdle(nExtraCycles[1]);
        nExtraCycles[1] = 0;
    }

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
        nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
        mcr_interrupt(i);

		if (has_csd)
		{
			INT32 nCycles = (((i * 1) * nCyclesTotal[1]) / nInterleave) - SekTotalCycles();
			if (csd_reset_status())
			{
				SekIdle(nCycles);
				nCyclesDone[1] += nCyclesTotal[1] / nInterleave;
			}
			else
			{
				nCyclesDone[1] += SekRun(nCycles);
			}
		}
		ZetClose();

		if (has_ssio)
		{
			ZetOpen(1);
            nCyclesDone[2] += ZetRun(((i + 1) * nCyclesTotal[2] / nInterleave) - nCyclesDone[2]);
			ssio_14024_clock(nInterleave);
			ZetClose();
		}
		// Render Sound Segment
		if (is_spyhunt && pBurnSoundOut && (i%32)==31) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 32);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910RenderInternal(nSegmentLength);
			nSoundBufferPos += nSegmentLength;

            filter_rc_update(0, pAY8910Buffer[0], pSoundBuf, nSegmentLength);
			filter_rc_update(1, pAY8910Buffer[1], pSoundBuf, nSegmentLength);
			filter_rc_update(2, pAY8910Buffer[2], pSoundBuf, nSegmentLength);
			filter_rc_update(3, pAY8910Buffer[3], pSoundBuf, nSegmentLength);
			filter_rc_update(4, pAY8910Buffer[4], pSoundBuf, nSegmentLength);
            if (LP1) {
                LP1->FilterMono(pAY8910Buffer[5], nSegmentLength); // Engine
            }
            filter_rc_update(5, pAY8910Buffer[5], pSoundBuf, nSegmentLength);
        }
	}

    if (pBurnSoundOut) {
        if (is_spyhunt) {
            INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
            INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
            if (nSegmentLength) {
                AY8910RenderInternal(nSegmentLength);

                filter_rc_update(0, pAY8910Buffer[0], pSoundBuf, nSegmentLength);
                filter_rc_update(1, pAY8910Buffer[1], pSoundBuf, nSegmentLength);
                filter_rc_update(2, pAY8910Buffer[2], pSoundBuf, nSegmentLength);
                filter_rc_update(3, pAY8910Buffer[3], pSoundBuf, nSegmentLength);
                filter_rc_update(4, pAY8910Buffer[4], pSoundBuf, nSegmentLength);
                if (LP1) {
                    LP1->FilterMono(pAY8910Buffer[5], nSegmentLength); // Engine
                }
                filter_rc_update(5, pAY8910Buffer[5], pSoundBuf, nSegmentLength);
            }

            DACUpdate(pBurnSoundOut, nBurnSoundLen);
        } else {
            BurnSoundClear();
            if (has_ssio) AY8910Render(pBurnSoundOut, nBurnSoundLen);
            if (has_csd) DACUpdate(pBurnSoundOut, nBurnSoundLen);
        }
	}

    if (has_csd) {
        nExtraCycles[1] = nCyclesDone[1] - SekTotalCycles();
        SekClose();
    }

    nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
    nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];

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

		tcs_scan(nAction, pnMin);
		csd_scan(nAction, pnMin);
		soundsgood_scan(nAction, pnMin);
		ssio_scan(nAction, pnMin);

		BurnWatchdogScan(nAction);

        BurnTrackballScan();

        if (has_shift) BurnShiftScan(nAction);

		SCAN_VAR(flipscreen);
		SCAN_VAR(latched_input);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
        SCAN_VAR(input_mux);

        SCAN_VAR(lamp);
        SCAN_VAR(last_op4);

        SCAN_VAR(nExtraCycles);
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
#if !defined (ROM_VERIFY)
	{ "82s123.12d",						0x0020, 0xe1281ee9, 0 | BRF_SND },
#else
	{ "",  0x000000, 0x00000000, BRF_ESS | BRF_PRG | BRF_BIOS },
#endif
};


// Demolition Derby (MCR-3 Mono Board Version)

static struct BurnRomInfo demodermRomDesc[] = {
	{ "pro0.3b",									0x8000, 0x2e24527b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pro1.5b",									0x8000, 0x034c00fc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tcs_u5.bin",									0x2000, 0xeca33b2c, 3 | BRF_PRG | BRF_ESS }, //  2 M6809 Code
	{ "tcs_u4.bin",									0x2000, 0x3490289a, 3 | BRF_PRG | BRF_ESS }, //  3

	{ "bg0.15a",									0x2000, 0xa35d13b8, 1 | BRF_GRA },           //  4 Background Tiles
	{ "bg1.14b",									0x2000, 0x22ca93f3, 1 | BRF_GRA },           //  5

	{ "dd_fg-0.a4",									0x4000, 0xe57a4de6, 2 | BRF_GRA },           //  6 Sprites
	{ "dd_fg-4.a3",									0x4000, 0x55aa667f, 2 | BRF_GRA },           //  7
	{ "dd_fg-1.a6",									0x4000, 0x70259651, 2 | BRF_GRA },           //  8
	{ "dd_fg-5.a5",									0x4000, 0x5fe99007, 2 | BRF_GRA },           //  9
	{ "dd_fg-2.a8",									0x4000, 0x6cab7b95, 2 | BRF_GRA },           // 10
	{ "dd_fg-6.a7",									0x4000, 0xabfb9a8b, 2 | BRF_GRA },           // 11
	{ "dd_fg-3.a10",								0x4000, 0x801d9b86, 2 | BRF_GRA },           // 12
	{ "dd_fg-7.a9",									0x4000, 0x0ec3f60a, 2 | BRF_GRA },           // 13
};

STD_ROM_PICK(demoderm)
STD_ROM_FN(demoderm)

static INT32 demoderm_read_callback(UINT8 address)
{
	switch (address)
	{
		case 0x01:
		{
            UINT8 ipt = DrvInputs[1] & 0x03;
            ipt |= (((input_mux) ? ~BurnTrackballRead(1, 0) : ~BurnTrackballRead(0, 0)) << 2) & 0x3f;

            BurnTrackballUpdate(input_mux);

            return ipt;
		}

		case 0x02:
		{
            UINT8 ipt = DrvInputs[2] & 0x03;
            ipt |= (((input_mux) ? ~BurnTrackballRead(1, 1) : ~BurnTrackballRead(0, 1)) << 2) & 0x3f;

            BurnTrackballUpdate(input_mux);

            return ipt;
		}
	}

	return -1;
}

static INT32 demoderm_write_callback(UINT8 address, UINT8 data)
{
	switch (address)
	{
		case 0x06:
		{
			if (data & 0x80) input_mux = 0;
			if (data & 0x40) input_mux = 1;

			INT32 cycles = (ZetTotalCycles() * 2) / 5;
			M6809Run(cycles - M6809TotalCycles());
			tcs_data_write(data);
		}
		return 0;
	}

	return -1;
}

static INT32 DemodermInit()
{
	port_write_handler = demoderm_write_callback;
	port_read_handler = demoderm_read_callback;
    is_demoderm = 1;

	return DrvInit(1);
}

struct BurnDriver BurnDrvDemoderm = {
	"demoderm", "demoderb", NULL, NULL, "1984",
	"Demolition Derby (MCR-3 Mono Board Version)\0", NULL, "Bally Midway", "MCR3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, demodermRomInfo, demodermRomName, NULL, NULL, NULL, NULL, DemodermInputInfo, DemodermDIPInfo,
	DemodermInit, DrvExit, TcsFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Sarge

static struct BurnRomInfo sargeRomDesc[] = {
	{ "cpu_3b.bin",									0x8000, 0xda31a58f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "cpu_5b.bin",									0x8000, 0x6800e746, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tcs_u5.bin",									0x2000, 0xa894ef8a, 3 | BRF_PRG | BRF_ESS }, //  2 M6809 Code
	{ "tcs_u4.bin",									0x2000, 0x6ca6faf3, 3 | BRF_PRG | BRF_ESS }, //  3

	{ "til_15a.bin",								0x2000, 0x685001b8, 1 | BRF_GRA },           //  4 Background Tiles
	{ "til_14b.bin",								0x2000, 0x8449eb45, 1 | BRF_GRA },           //  5

	{ "spr_8e.bin",									0x8000, 0x93fac29d, 2 | BRF_GRA },           //  6 Sprites
	{ "spr_6e.bin",									0x8000, 0x7cc6fb28, 2 | BRF_GRA },           //  7
	{ "spr_5e.bin",									0x8000, 0xc832375c, 2 | BRF_GRA },           //  8
	{ "spr_4e.bin",									0x8000, 0xc382267d, 2 | BRF_GRA },           //  9

	{ "a59a26axlcxhd.13j.bin",						0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 10 PALs
	{ "a59a26axlbxhd.2j.bin",						0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 11
	{ "a59a26axlaxhd.3j.bin",						0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 12
	{ "0066-314bx-xxqx.6h.bin",						0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 13
	{ "0066-316bx-xxqx.5h.bin",						0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 14
	{ "0066-315bx-xxqx.5g.bin",						0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 15
	{ "0066-313bx-xxqx.4g.bin",						0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 16
};

STD_ROM_PICK(sarge)
STD_ROM_FN(sarge)

static INT32 SargeInit()
{
	port_write_handler = demoderm_write_callback;

	return DrvInit(1);
}

struct BurnDriver BurnDrvSarge = {
	"sarge", NULL, NULL, NULL, "1985",
	"Sarge\0", NULL, "Bally Midway", "MCR3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sargeRomInfo, sargeRomName, NULL, NULL, NULL, NULL, SargeInputInfo, SargeDIPInfo,
	SargeInit, DrvExit, TcsFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Max RPM (ver 2)

static struct BurnRomInfo maxrpmRomDesc[] = {
	{ "pro.0",									0x8000, 0x3f9ec35f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "pro.1",									0x8000, 0xf628bb30, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "turbskwk.u5",							0x4000, 0x55c3b759, 3 | BRF_PRG | BRF_ESS }, //  2 M6809 Code
	{ "turbskwk.u4",							0x4000, 0x31a2da2e, 3 | BRF_PRG | BRF_ESS }, //  3

	{ "bg-0",									0x4000, 0xe3fb693a, 1 | BRF_GRA },           //  4 Background Tiles
	{ "bg-1",									0x4000, 0x50d1db6c, 1 | BRF_GRA },           //  5

	{ "fg-0",									0x8000, 0x1d1435c1, 2 | BRF_GRA },           //  6 Sprites
	{ "fg-1",									0x8000, 0xe54b7f2a, 2 | BRF_GRA },           //  7
	{ "fg-2",									0x8000, 0x38be8505, 2 | BRF_GRA },           //  8
	{ "fg-3",									0x8000, 0x9ae3eb52, 2 | BRF_GRA },           //  9
};

STD_ROM_PICK(maxrpm)
STD_ROM_FN(maxrpm)

static INT32 maxrpm_read_callback(UINT8 address)
{
	switch (address)
	{
		case 1:
			return latched_input;

	//	case 2:
//			return maxrpm_ipt2_read();

	}

	return -1;
}

static INT32 maxrpm_write_callback(UINT8 address, UINT8 data)
{
	switch (address)
	{
		case 0x05:
		{

		}
		return -1; // fall through

		case 0x06:
		{
			INT32 cycles = (ZetTotalCycles() * 2) / 5;
			M6809Run(cycles - M6809TotalCycles());
			tcs_data_write(data);
		}
		return 0;
	}

	return -1;
}

static INT32 MaxrpmInit()
{
    dip_service = 0x80;

	port_write_handler = maxrpm_write_callback;
	port_read_handler = maxrpm_read_callback;

	return DrvInit(1);
}

struct BurnDriver BurnDrvMaxrpm = {
	"maxrpm", NULL, NULL, NULL, "1986",
	"Max RPM (ver 2)\0", NULL, "Bally Midway", "MCR3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, maxrpmRomInfo, maxrpmRomName, NULL, NULL, NULL, NULL, RampageInputInfo, RampageDIPInfo, //MaxrpmInputInfo, MaxrpmDIPInfo,
	MaxrpmInit, DrvExit, TcsFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Rampage (Rev 3, 8/27/86)

static struct BurnRomInfo rampageRomDesc[] = {
	{ "pro-0_3b_rev_3_8-27-86.3b",					0x08000, 0x2f7ca03c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "pro-1_5b_rev_3_8-27-86.5b",					0x08000, 0xd89bd9a4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u-7_rev.2_8-14-86.u7",						0x08000, 0xcffd7fa5, 4 | BRF_PRG | BRF_ESS }, //  2 68K Code
	{ "u-17_rev.2_8-14-86.u17",						0x08000, 0xe92c596b, 4 | BRF_PRG | BRF_ESS }, //  3
	{ "u-8_rev.2_8-14-86.u8",						0x08000, 0x11f787e4, 4 | BRF_PRG | BRF_ESS }, //  4
	{ "u-18_rev.2_8-14-86.u18",						0x08000, 0x6b8bf5e1, 4 | BRF_PRG | BRF_ESS }, //  5

	{ "bg-0_u15_7-23-86.15a",						0x04000, 0xc0d8b7a5, 1 | BRF_GRA },           //  6 Background Tiles
	{ "bg-1_u14_7-23-86.14b",						0x04000, 0x2f6e3aa1, 1 | BRF_GRA },           //  7

	{ "fg-0_8e_6-30-86.8e",							0x10000, 0x0974be5d, 2 | BRF_GRA },           //  8 Sprites
	{ "fg-1_6e_6-30-86.6e",							0x10000, 0x8728532b, 2 | BRF_GRA },           //  9
	{ "fg-2_5e_6-30-86.5e",							0x10000, 0x9489f714, 2 | BRF_GRA },           // 10
	{ "fg-3_4e_6-30-86.4e",							0x10000, 0x81e1de40, 2 | BRF_GRA },           // 11

	{ "e36a31axnaxqd.u15.bin",						0x00001, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 12 Sounds Good PAL
};

STD_ROM_PICK(rampage)
STD_ROM_FN(rampage)

static INT32 rampage_write_callback(UINT8 address, UINT8 data)
{
	switch (address)
	{
		case 0x06:
		{
			INT32 cycles = (ZetTotalCycles() * 8) / 5;
			SekRun(cycles - SekTotalCycles());
			soundsgood_reset_write((~data & 0x20) >> 5);
			soundsgood_data_write(data);
		}
		return 0;
	}

	return -1;
}

static INT32 RampageInit()
{
	sound_status_bit = 7;
	sound_input_bank = 4;
	port_write_handler = rampage_write_callback;

    soundsgood_rampage = 1; // for sound-cleanup in soundsgood

    return DrvInit(0);
}

struct BurnDriver BurnDrvRampage = {
	"rampage", NULL, NULL, NULL, "1986",
	"Rampage (Rev 3, 8/27/86)\0", NULL, "Bally Midway", "MCR3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, rampageRomInfo, rampageRomName, NULL, NULL, NULL, NULL, RampageInputInfo, RampageDIPInfo,
	RampageInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Rampage (Rev 2, 8/4/86)

static struct BurnRomInfo rampage2RomDesc[] = {
	{ "pro-0_3b_rev_2_8-4-86.3b",					0x08000, 0x3f1d0293, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "pro-1_5b_rev_2_8-4-86.5b",					0x08000, 0x58523d75, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u-7_rev.2_8-14-86.u7",						0x08000, 0xcffd7fa5, 4 | BRF_PRG | BRF_ESS }, //  2 68K Code
	{ "u-17_rev.2_8-14-86.u17",						0x08000, 0xe92c596b, 4 | BRF_PRG | BRF_ESS }, //  3
	{ "u-8_rev.2_8-14-86.u8",						0x08000, 0x11f787e4, 4 | BRF_PRG | BRF_ESS }, //  4
	{ "u-18_rev.2_8-14-86.u18",						0x08000, 0x6b8bf5e1, 4 | BRF_PRG | BRF_ESS }, //  5

	{ "bg-0_u15_7-23-86.15a",						0x04000, 0xc0d8b7a5, 1 | BRF_GRA },           //  6 Background Tiles
	{ "bg-1_u14_7-23-86.14b",						0x04000, 0x2f6e3aa1, 1 | BRF_GRA },           //  7

	{ "fg-0_8e_6-30-86.8e",							0x10000, 0x0974be5d, 2 | BRF_GRA },           //  8 Sprites
	{ "fg-1_6e_6-30-86.6e",							0x10000, 0x8728532b, 2 | BRF_GRA },           //  9
	{ "fg-2_5e_6-30-86.5e",							0x10000, 0x9489f714, 2 | BRF_GRA },           // 10
	{ "fg-3_4e_6-30-86.4e",							0x10000, 0x81e1de40, 2 | BRF_GRA },           // 11

	{ "e36a31axnaxqd.u15.bin",						0x00001, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 12 Sounds Good PAL
};

STD_ROM_PICK(rampage2)
STD_ROM_FN(rampage2)

struct BurnDriver BurnDrvRampage2 = {
	"rampage2", "rampage", NULL, NULL, "1986",
	"Rampage (Rev 2, 8/4/86)\0", NULL, "Bally Midway", "MCR3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, rampage2RomInfo, rampage2RomName, NULL, NULL, NULL, NULL, RampageInputInfo, RampageDIPInfo,
	RampageInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Power Drive

static struct BurnRomInfo powerdrvRomDesc[] = {
	{ "pdrv3b.bin",									0x08000, 0xd870b704, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "pdrv5b.bin",									0x08000, 0xfa0544ad, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "power_drive_snd_u7.u7",						0x08000, 0x78713e78, 4 | BRF_PRG | BRF_ESS }, //  2 68K Code
	{ "power_drive_snd_u17.u17",					0x08000, 0xc41de6e4, 4 | BRF_PRG | BRF_ESS }, //  3
	{ "power_drive_snd_u8.u8",						0x08000, 0x15714036, 4 | BRF_PRG | BRF_ESS }, //  4
	{ "power_drive_snd_u18.u18",					0x08000, 0xcae14c70, 4 | BRF_PRG | BRF_ESS }, //  5

	{ "pdrv15a.bin",								0x04000, 0xb858b5a8, 1 | BRF_GRA },           //  6 Background Tiles
	{ "pdrv14b.bin",								0x04000, 0x12ee7fc2, 1 | BRF_GRA },           //  7

	{ "pdrv8e.bin",									0x10000, 0xdd3a2adc, 2 | BRF_GRA },           //  8 Sprites
	{ "pdrv6e.bin",									0x10000, 0x1a1f7f81, 2 | BRF_GRA },           //  9
	{ "pdrv5e.bin",									0x10000, 0x4cb4780e, 2 | BRF_GRA },           // 10
	{ "pdrv4e.bin",									0x10000, 0xde400335, 2 | BRF_GRA },           // 11
};

STD_ROM_PICK(powerdrv)
STD_ROM_FN(powerdrv)

static INT32 PowerdrvInit()
{
	sound_status_bit = 7;
	sound_input_bank = 2;
	port_write_handler = rampage_write_callback;

	return DrvInit(0);
}

struct BurnDriver BurnDrvPowerdrv = {
	"powerdrv", NULL, NULL, NULL, "1986",
	"Power Drive\0", NULL, "Bally Midway", "MCR3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, powerdrvRomInfo, powerdrvRomName, NULL, NULL, NULL, NULL, PowerdrvInputInfo, PowerdrvDIPInfo,
	PowerdrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Star Guards

static struct BurnRomInfo stargrdsRomDesc[] = {
	{ "pro-0.3b",									0x08000, 0x3ad94aa2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "pro-1.5b",									0x08000, 0xdba428b0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "snd0.u7",									0x08000, 0x7755a493, 4 | BRF_PRG | BRF_ESS }, //  2 68K Code
	{ "snd1.u17",									0x08000, 0xd98d14ae, 4 | BRF_PRG | BRF_ESS }, //  3

	{ "bg-0.15a",									0x08000, 0xecfaef3e, 1 | BRF_GRA },           //  4 Background Tiles
	{ "bg-1.14b",									0x08000, 0x2c75569d, 1 | BRF_GRA },           //  5

	{ "fg-0.8e",									0x10000, 0x22797aaa, 2 | BRF_GRA },           //  6 Sprites
	{ "fg-1.6e",									0x10000, 0x413fa119, 2 | BRF_GRA },           //  7
	{ "fg-2.5e",									0x10000, 0x7036cfe6, 2 | BRF_GRA },           //  8
	{ "fg-3.4e",									0x10000, 0xcc5cc003, 2 | BRF_GRA },           //  9
};

STD_ROM_PICK(stargrds)
STD_ROM_FN(stargrds)

static INT32 stargrds_write_callback(UINT8 address, UINT8 data)
{
	switch (address)
	{
		case 0x05:
			// coin counter = data & 0x01;
			input_mux = (data & 0x02) >> 1;
			// leds 0x1c
			flipscreen = (data & 0x40) >> 6;
		return 0;

		case 0x06:
		{
			INT32 cycles = (ZetTotalCycles() * 8) / 5;
			SekRun(cycles - SekTotalCycles());
			soundsgood_reset_write((~data & 0x40) >> 6);
			soundsgood_data_write((data << 1) | (data >> 7));
		}
		return 0;
	}

	return -1;
}

static INT32 stargrds_read_callback(UINT8 address)
{
	switch (address)
	{
		case 0x00:
		{
			UINT8 ret = soundsgood_input_read(0) & 0xff;
			if (input_mux) ret = (ret & 0xf5) | (soundsgood_input_read(5) & 0x0a);
			return ret;
		}
	}

	return -1;
}

static INT32 StargrdsInit()
{
    dip_service = 0x80;

	sound_status_bit = 4;
	sound_input_bank = 0;
	port_write_handler = stargrds_write_callback;
	port_read_handler = stargrds_read_callback;

	return DrvInit(0);
}

struct BurnDriver BurnDrvStargrds = {
	"stargrds", NULL, NULL, NULL, "1987",
	"Star Guards\0", NULL, "Bally Midway", "MCR3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, stargrdsRomInfo, stargrdsRomName, NULL, NULL, NULL, NULL, StargrdsInputInfo, StargrdsDIPInfo,
	StargrdsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};



// Spy Hunter

static struct BurnRomInfo spyhuntRomDesc[] = {
	{ "spy-hunter_cpu_pg0_2-9-84.6d",				0x2000, 0x1721b88f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "spy-hunter_cpu_pg1_2-9-84.7d",				0x2000, 0x909d044f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "spy-hunter_cpu_pg2_2-9-84.8d",				0x2000, 0xafeeb8bd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "spy-hunter_cpu_pg3_2-9-84.9d",				0x2000, 0x5e744381, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "spy-hunter_cpu_pg4_2-9-84.10d",				0x2000, 0xa3033c15, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "spy-hunter_cpu_pg5_2-9-84.11d",				0x4000, 0x88aa1e99, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "spy-hunter_snd_0_sd_11-18-83.a7",			0x1000, 0xc95cf31e, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #1 Code (SSIO)
	{ "spy-hunter_snd_1_sd_11-18-83.a8",			0x1000, 0x12aaa48e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "spy-hunter_cs_deluxe_u7_a_11-18-83.u7",		0x2000, 0x6e689fe7, 4 | BRF_PRG | BRF_ESS }, //  8 68K Code
	{ "spy-hunter_cs_deluxe_u17_b_11-18-83.u17",	0x2000, 0x0d9ddce6, 4 | BRF_PRG | BRF_ESS }, //  9
	{ "spy-hunter_cs_deluxe_u8_c_11-18-83.u8",		0x2000, 0x35563cd0, 4 | BRF_PRG | BRF_ESS }, // 10
	{ "spy-hunter_cs_deluxe_u18_d_11-18-83.u18",	0x2000, 0x63d3f5b1, 4 | BRF_PRG | BRF_ESS }, // 11

	{ "spy-hunter_cpu_bg0_11-18-83.3a",				0x2000, 0xdea34fed, 1 | BRF_GRA },           // 12 Background Tiles
	{ "spy-hunter_cpu_bg1_11-18-83.4a",				0x2000, 0x8f64525f, 1 | BRF_GRA },           // 13
	{ "spy-hunter_cpu_bg2_11-18-83.5a",				0x2000, 0xba0fd626, 1 | BRF_GRA },           // 14
	{ "spy-hunter_cpu_bg3_11-18-83.6a",				0x2000, 0x7b482d61, 1 | BRF_GRA },           // 15

	{ "spy-hunter_video_1fg_11-18-83.a7",			0x4000, 0x9fe286ec, 2 | BRF_GRA },           // 16 Sprites
	{ "spy-hunter_video_0fg_11-18-83.a8",			0x4000, 0x292c5466, 2 | BRF_GRA },           // 17
	{ "spy-hunter_video_3fg_11-18-83.a5",			0x4000, 0xb894934d, 2 | BRF_GRA },           // 18
	{ "spy-hunter_video_2fg_11-18-83.a6",			0x4000, 0x62c8bfa5, 2 | BRF_GRA },           // 19
	{ "spy-hunter_video_5fg_11-18-83.a3",			0x4000, 0x2d9fbcec, 2 | BRF_GRA },           // 20
	{ "spy-hunter_video_4fg_11-18-83.a4",			0x4000, 0x7ca4941b, 2 | BRF_GRA },           // 21
	{ "spy-hunter_video_7fg_11-18-83.a1",			0x4000, 0x940fe17e, 2 | BRF_GRA },           // 22
	{ "spy-hunter_video_6fg_11-18-83.a2",			0x4000, 0x8cb8a066, 2 | BRF_GRA },           // 23

	{ "spy-hunter_cpu_alpha-n_11-18-83",			0x1000, 0x936dc87f, 3 | BRF_GRA },           // 24 Characters
};

STDROMPICKEXT(spyhunt, spyhunt, Ssioprom)
STD_ROM_FN(spyhunt)

static UINT8 spyhunt_ip1_read(UINT8)
{
	return (DrvInputs[1] & ~0x60) | (csd_status_read() << 5);
}

static UINT8 spyhunt_ip2_read(UINT8)
{
    UINT8 temp = 0;

    switch (input_mux) {
        case 1: {
            temp = ProcessAnalog(DrvAnalogPort0, 0, INPUT_DEADZONE, 0x34, 0xb4);
            break;
        }
        case 0: {
            temp = ProcessAnalog(DrvAnalogPort1, 0, INPUT_LINEAR | INPUT_MIGHTBEDIGITAL | INPUT_DEADZONE, 0x30, 0xff);
            break;
        }
    }
    return temp;
}

static void spyhunt_op4_write(UINT8 , UINT8 data)
{
    if (last_op4 & 0x20 && ~data & 0x20) {
        if (data&8) lamp |= 1<<(data&7);
        if (~data&8) lamp &= ~(1<<(data&7));
    }
    last_op4 = data;

    input_mux = data >> 7;

    csd_data_write(data);
}

static void spyhunt_rom_swap()
{
	for (INT32 i = 0; i < 0x2000; i++) { // swap halves of last program rom
		INT32 t = DrvZ80ROM0[0xa000 + i];
		DrvZ80ROM0[0xa000 + i] = DrvZ80ROM0[0xc000 + i];
		DrvZ80ROM0[0xc000 + i] = t;
	}
}

static INT32 SpyhuntInit()
{
    dip_service = 0x80;

    sprite_color_mask = 0;
    has_shift = 1; // must be before init!
    is_spyhunt = 1;
	INT32 nRet = SpyhuntCommonInit(2);

	if (nRet == 0)
	{
		ssio_set_custom_input(1, 0x60, spyhunt_ip1_read);
		ssio_set_custom_input(2, 0xff, spyhunt_ip2_read);
		ssio_set_custom_output(4, 0xff, spyhunt_op4_write);

		spyhunt_rom_swap();
	}

	return nRet;
}

struct BurnDriver BurnDrvSpyhunt = {
	"spyhunt", NULL, "midssio", NULL, "1983",
	"Spy Hunter\0", NULL, "Bally Midway", "MCR3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SHOOT, 0,
	NULL, spyhuntRomInfo, spyhuntRomName, NULL, NULL, NULL, NULL, SpyhuntInputInfo, SpyhuntDIPInfo,
	SpyhuntInit, DrvExit, CSDSSIOFrame, SpyhuntDraw, DrvScan, &DrvRecalc, 0x40,
	480, 480, 3, 4
};


// Spy Hunter (Playtronic license)

static struct BurnRomInfo spyhuntpRomDesc[] = {
	{ "sh_mb_2.bin",								0x2000, 0xdf6cd642, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "spy-hunter_cpu_pg1_2-9-84.7d",				0x2000, 0x909d044f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "spy-hunter_cpu_pg2_2-9-84.8d",				0x2000, 0xafeeb8bd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "spy-hunter_cpu_pg3_2-9-84.9d",				0x2000, 0x5e744381, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sh_mb_6.bin",								0x2000, 0x8cbf04d8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "spy-hunter_cpu_pg5_2-9-84.11d",				0x4000, 0x88aa1e99, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "spy-hunter_snd_0_sd_11-18-83.a7",			0x1000, 0xc95cf31e, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #1 Code (SSIO)
	{ "spy-hunter_snd_1_sd_11-18-83.a8",			0x1000, 0x12aaa48e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "spy-hunter_cs_deluxe_u7_a_11-18-83.u7",		0x2000, 0x6e689fe7, 4 | BRF_PRG | BRF_ESS }, //  8 68K Code
	{ "spy-hunter_cs_deluxe_u17_b_11-18-83.u17",	0x2000, 0x0d9ddce6, 4 | BRF_PRG | BRF_ESS }, //  9
	{ "spy-hunter_cs_deluxe_u8_c_11-18-83.u8",		0x2000, 0x35563cd0, 4 | BRF_PRG | BRF_ESS }, // 10
	{ "spy-hunter_cs_deluxe_u18_d_11-18-83.u18",	0x2000, 0x63d3f5b1, 4 | BRF_PRG | BRF_ESS }, // 11

	{ "spy-hunter_cpu_bg0_11-18-83.3a",				0x2000, 0xdea34fed, 1 | BRF_GRA },           // 12 Background Tiles
	{ "spy-hunter_cpu_bg1_11-18-83.4a",				0x2000, 0x8f64525f, 1 | BRF_GRA },           // 13
	{ "spy-hunter_cpu_bg2_11-18-83.5a",				0x2000, 0xba0fd626, 1 | BRF_GRA },           // 14
	{ "spy-hunter_cpu_bg3_11-18-83.6a",				0x2000, 0x7b482d61, 1 | BRF_GRA },           // 15

	{ "spy-hunter_video_1fg_11-18-83.a7",			0x4000, 0x9fe286ec, 2 | BRF_GRA },           // 16 Sprites
	{ "spy-hunter_video_0fg_11-18-83.a8",			0x4000, 0x292c5466, 2 | BRF_GRA },           // 17
	{ "spy-hunter_video_3fg_11-18-83.a5",			0x4000, 0xb894934d, 2 | BRF_GRA },           // 18
	{ "spy-hunter_video_2fg_11-18-83.a6",			0x4000, 0x62c8bfa5, 2 | BRF_GRA },           // 19
	{ "spy-hunter_video_5fg_11-18-83.a3",			0x4000, 0x2d9fbcec, 2 | BRF_GRA },           // 20
	{ "spy-hunter_video_4fg_11-18-83.a4",			0x4000, 0x7ca4941b, 2 | BRF_GRA },           // 21
	{ "spy-hunter_video_7fg_11-18-83.a1",			0x4000, 0x940fe17e, 2 | BRF_GRA },           // 22
	{ "spy-hunter_video_6fg_11-18-83.a2",			0x4000, 0x8cb8a066, 2 | BRF_GRA },           // 23

	{ "sh_mb_1.bin",								0x0800, 0x5c74c9f0, 3 | BRF_GRA },           // 24 Characters
};

STDROMPICKEXT(spyhuntp, spyhuntp, Ssioprom)
STD_ROM_FN(spyhuntp)

struct BurnDriver BurnDrvSpyhuntp = {
	"spyhuntp", "spyhunt", "midssio", NULL, "1983",
	"Spy Hunter (Playtronic license)\0", NULL, "Bally Midway (Playtronic license)", "MCR3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SHOOT, 0,
	NULL, spyhuntpRomInfo, spyhuntpRomName, NULL, NULL, NULL, NULL, SpyhuntInputInfo, SpyhuntDIPInfo,
	SpyhuntInit, DrvExit, CSDSSIOFrame, SpyhuntDraw, DrvScan, &DrvRecalc, 0x40,
	480, 480, 3, 4
};


// Crater Raider

static struct BurnRomInfo craterRomDesc[] = {
	{ "crcpu.6d",									0x2000, 0xad31f127, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "crcpu.7d",									0x2000, 0x3743c78f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "crcpu.8d",									0x2000, 0xc95f9088, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "crcpu.9d",									0x2000, 0xa03c4b11, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "crcpu.10d",									0x2000, 0x44ae4cbd, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "crsnd4.a7",									0x1000, 0xfd666cb5, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code (SSIO)
	{ "crsnd1.a8",									0x1000, 0x90bf2c4c, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "crsnd2.a9",									0x1000, 0x3b8deef1, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "crsnd3.a10",									0x1000, 0x05803453, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "crcpu.3a",									0x2000, 0x9d73504a, 1 | BRF_GRA },           //  9 Background Tiles
	{ "crcpu.4a",									0x2000, 0x42a47dff, 1 | BRF_GRA },           // 10
	{ "crcpu.5a",									0x2000, 0x2fe4a6e1, 1 | BRF_GRA },           // 11
	{ "crcpu.6a",									0x2000, 0xd0659042, 1 | BRF_GRA },           // 12

	{ "crvid.a4",									0x4000, 0x579a8e36, 2 | BRF_GRA },           // 13 Sprites
	{ "crvid.a3",									0x4000, 0x2c2f5b29, 2 | BRF_GRA },           // 14
	{ "crvid.a6",									0x4000, 0x5bf954e0, 2 | BRF_GRA },           // 15
	{ "crvid.a5",									0x4000, 0x9bdec312, 2 | BRF_GRA },           // 16
	{ "crvid.a8",									0x4000, 0x4b913498, 2 | BRF_GRA },           // 17
	{ "crvid.a7",									0x4000, 0x9fa307d5, 2 | BRF_GRA },           // 18
	{ "crvid.a10",									0x4000, 0x7a22d6bc, 2 | BRF_GRA },           // 19
	{ "crvid.a9",									0x4000, 0x811f152d, 2 | BRF_GRA },           // 20

	{ "crcpu.10g",									0x1000, 0x6fe53c8d, 3 | BRF_GRA },           // 21 Characters
};

STDROMPICKEXT(crater, crater, Ssioprom)
STD_ROM_FN(crater)

static UINT8 crater_ip1_read(UINT8)
{
    return BurnTrackballRead(0, 0);
}

static INT32 CraterInit()
{
    dip_service = 0x80;

	INT32 nRet = SpyhuntCommonInit(3);

	if (nRet == 0)
	{
        flip_screen_x = TMAP_FLIPX;
        sprite_color_mask = 3;
        has_dial = 1;

        ssio_set_custom_input(1, 0xff, crater_ip1_read);
    }

    return nRet;
}

struct BurnDriver BurnDrvCrater = {
	"crater", NULL, "midssio", NULL, "1984",
	"Crater Raider\0", NULL, "Bally Midway", "MCR3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, craterRomInfo, craterRomName, NULL, NULL, NULL, NULL, CraterInputInfo, CraterDIPInfo,
	CraterInit, DrvExit, CSDSSIOFrame, SpyhuntDraw, DrvScan, &DrvRecalc, 0x40,
	480, 480, 4, 3
};
