// FinalBurn Neo Cinematronics vector hardware driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "ccpu_intf.h"
#include "z80_intf.h"
#include "z80daisy.h"
#include "ay8910.h"
#include "samples.h"
#include "vector.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80RAM;
static UINT16 *DrvDataRAM;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[16];
static UINT8 DrvJoy5[16];
static UINT8 DrvDips[3];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;
static INT16 DrvAnalog[2];

// game configuration
enum { COLOR_BW = 0, COLOR_16LEVEL, COLOR_64LEVEL, COLOR_RGB, COLOR_QB3 };
enum { SND_NONE = 0, SND_SPACEWAR, SND_BARRIER, SND_SPEEDFRK, SND_STARHAWK, SND_SUNDANCE, SND_TAILG,
	   SND_WARRIOR, SND_ARMORA, SND_RIPOFF, SND_STARCAS, SND_SOLARQ, SND_BOXINGB, SND_WOTW, SND_DEMON };
enum { INP_NORMAL = 0, INP_SPEEDFRK, INP_SUNDANCE, INP_BOXINGB, INP_TAILG, INP_QB3, INP_BARRIER };

static INT32 prg_type;
static INT32 use_jmi;
static INT32 color_mode;
static INT32 vis_w, vis_h;
static INT32 flip_x;
static INT32 sound_type;
static INT32 input_type;
static INT32 has_mux;
static INT32 is_qb3;
static INT32 has_samples;
static INT32 has_z80;

// machine state
static UINT8 outlatch;
static UINT8 coin_detected;
static UINT8 coin_last;
static UINT8 mux_select;
static INT32 vector_color;
static INT16 lastx, lasty;
static UINT8 gear;
static INT32 nExtraCycles;
static INT32 frame_counter;
static INT32 qb3_lastx, qb3_lasty;
static INT32 qb3_bank;

// sound state
static UINT32 current_shift;
static UINT32 last_shift;
static UINT32 last_shift2;
static UINT32 current_pitch;
static INT32 last_frame;
static double target_volume;
static double current_volume;
static UINT8 sound_fifo[16];
static UINT8 sound_fifo_in;
static UINT8 sound_fifo_out;
static UINT8 last_portb_write;

// per-frame input helpers
static INT32 wheel_delta;
static UINT8 boxingb_dial;
static UINT32 analog_x, analog_y;
static UINT16 sundance_pad[2];

static const INT32 nCcpuClock = 19923000 / 4;	// 4.98075 MHz
static const INT32 nZ80Clock  = 3579545;

#define SAMPLE_VOL	0.60

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

// ---------------------------------------------------------------------------
// Inputs

static struct BurnInputInfo SpacewarInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 15,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 3"	},

	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 14,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 3"	},

	{"Option 0",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 start"	},
	{"Option 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"Option 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"keyb_2"	},
	{"Option 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"keyb_3"	},
	{"Option 4",		BIT_DIGITAL,	DrvJoy1 + 2,	"keyb_4"	},
	{"Option 5",		BIT_DIGITAL,	DrvJoy1 + 10,	"keyb_5"	},
	{"Option 6",		BIT_DIGITAL,	DrvJoy1 + 5,	"keyb_6"	},
	{"Option 7",		BIT_DIGITAL,	DrvJoy1 + 7,	"keyb_7"	},
	{"Option 8",		BIT_DIGITAL,	DrvJoy1 + 1,	"keyb_8"	},
	{"Option 9",		BIT_DIGITAL,	DrvJoy1 + 3,	"keyb_9"	},
	{"Reset Playfield",	BIT_DIGITAL,	DrvJoy2 + 6,	"service2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Spacewar)

static struct BurnInputInfo SpaceshpInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 15,	"p1 up"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 up"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 14,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},

	{"Option 0",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 start"	},
	{"Option 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"Option 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"keyb_2"	},
	{"Option 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"keyb_3"	},
	{"Option 4",		BIT_DIGITAL,	DrvJoy1 + 2,	"keyb_4"	},
	{"Option 5",		BIT_DIGITAL,	DrvJoy1 + 10,	"keyb_5"	},
	{"Option 6",		BIT_DIGITAL,	DrvJoy1 + 5,	"keyb_6"	},
	{"Option 7",		BIT_DIGITAL,	DrvJoy1 + 7,	"keyb_7"	},
	{"Option 8",		BIT_DIGITAL,	DrvJoy1 + 1,	"keyb_8"	},
	{"Option 9",		BIT_DIGITAL,	DrvJoy1 + 3,	"keyb_9"	},
	{"Reset Playfield",	BIT_DIGITAL,	DrvJoy2 + 6,	"service2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Spaceshp)

static struct BurnInputInfo BarrierInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 12,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 14,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 right"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 13,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 right"	},

	{"Skill A",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"Skill B",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"Skill C",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Barrier)

static struct BurnInputInfo SpeedfrkInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	A("P1 Wheel",		BIT_ANALOG_REL, &DrvAnalog[0],	"p1 x-axis"	),
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 right"	},
	{"P1 Gas",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 1"	},
	{"P1 1st Gear",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 2"	},
	{"P1 2nd Gear",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 3"	},
	{"P1 3rd Gear",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 4"	},
	{"P1 4th Gear",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 5"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Speedfrk)

static struct BurnInputInfo StarhawkInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 14,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 4"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Starhawk)

static struct BurnInputInfo SundanceInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Pad 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 2"	},
	{"P1 Pad 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 up"		},
	{"P1 Pad 3",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 3"	},
	{"P1 Pad 4",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 left"	},
	{"P1 Pad 5",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 fire 4"	},
	{"P1 Pad 6",		BIT_DIGITAL,	DrvJoy4 + 5,	"p1 right"	},
	{"P1 Pad 7",		BIT_DIGITAL,	DrvJoy4 + 6,	"p1 fire 5"	},
	{"P1 Pad 8",		BIT_DIGITAL,	DrvJoy4 + 7,	"p1 down"	},
	{"P1 Pad 9",		BIT_DIGITAL,	DrvJoy4 + 8,	"p1 fire 6"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 1"	},
	{"P2 Pad 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"p2 fire 2"	},
	{"P2 Pad 2",		BIT_DIGITAL,	DrvJoy5 + 1,	"p2 up"		},
	{"P2 Pad 3",		BIT_DIGITAL,	DrvJoy5 + 2,	"p2 fire 3"	},
	{"P2 Pad 4",		BIT_DIGITAL,	DrvJoy5 + 3,	"p2 left"	},
	{"P2 Pad 5",		BIT_DIGITAL,	DrvJoy5 + 4,	"p2 fire 4"	},
	{"P2 Pad 6",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 right"	},
	{"P2 Pad 7",		BIT_DIGITAL,	DrvJoy5 + 6,	"p2 fire 5"	},
	{"P2 Pad 8",		BIT_DIGITAL,	DrvJoy5 + 7,	"p2 down"	},
	{"P2 Pad 9",		BIT_DIGITAL,	DrvJoy5 + 8,	"p2 fire 6"	},

	{"2 Suns",			BIT_DIGITAL,	DrvJoy1 + 11,	"keyb_2"	},
	{"3 Suns",			BIT_DIGITAL,	DrvJoy1 + 4,	"keyb_3"	},
	{"4 Suns",			BIT_DIGITAL,	DrvJoy1 + 6,	"keyb_4"	},
	{"Toggle Grid",		BIT_DIGITAL,	DrvJoy1 + 5,	"keyb_G"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Sundance)

static struct BurnInputInfo TailgInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	A("P1 Stick X",		BIT_ANALOG_REL, &DrvAnalog[0],	"p1 x-axis"	),
	A("P1 Stick Y",		BIT_ANALOG_REL, &DrvAnalog[1],	"p1 y-axis"	),
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Tailg)

static struct BurnInputInfo WarriorInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 10,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 11,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 9,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Warrior)

// armora, ripoff
static struct BurnInputInfo ArmoraInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 12,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 14,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 15,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Armora)

// starcas, wotw
static struct BurnInputInfo StarcasInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 2"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Starcas)

static struct BurnInputInfo StarcascInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Starcasc)

static struct BurnInputInfo SolarqInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Fire",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Thrust",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Hyperspace",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},
	{"P1 Nova",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Solarq)

static struct BurnInputInfo BoxingbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	A("P1 Dial",		BIT_ANALOG_REL, &DrvAnalog[0],	"p1 x-axis"	),
	{"P1 Dial Left",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"	},
	{"P1 Dial Right",	BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},

	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Boxingb)

static struct BurnInputInfo DemonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 7,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 8,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Demon)

static struct BurnInputInfo Qb3InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left Stick Left",	BIT_DIGITAL,	DrvJoy1 + 13,	"p1 left"	},
	{"P1 Left Stick Right",	BIT_DIGITAL,	DrvJoy1 + 11,	"p1 right"	},
	{"P1 Right Stick Up",	BIT_DIGITAL,	DrvJoy1 + 3,	"p2 up"		},
	{"P1 Right Stick Down",	BIT_DIGITAL,	DrvJoy1 + 5,	"p2 down"	},
	{"P1 Right Stick Left",	BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"	},
	{"P1 Right Stick Right",BIT_DIGITAL,	DrvJoy1 + 4,	"p2 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 2"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 4"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Qb3)

#undef A

// ---------------------------------------------------------------------------
// DIPs  (Dip A = SWITCHES port, Dip B = QB-3 dips in INPUTS port, Dip C = config)

static struct BurnDIPInfo CinematCfgDIPList[]=
{
	{0x02, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    2, "Hires Mode"		},
	{0x02, 0x01, 0x01, 0x00, "No"				},
	{0x02, 0x01, 0x01, 0x01, "Yes"				},
};

static struct BurnDIPInfo CinematOvlDIPList[]=
{
	{0x02, 0xff, 0xff, 0x02, NULL				},


	{0   , 0xfe, 0   ,    2, "Hires Mode"		},
	{0x02, 0x01, 0x01, 0x00, "No"				},
	{0x02, 0x01, 0x01, 0x01, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Color Overlay"	},
	{0x02, 0x01, 0x02, 0x00, "Off"				},
	{0x02, 0x01, 0x02, 0x02, "On"				},
};

static struct BurnDIPInfo SpacewarDIPList[]=
{
	DIP_OFFSET(0x17)
	{0x00, 0xff, 0xff, 0xfc, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Time"				},
	{0x00, 0x01, 0x03, 0x03, "0:45/coin"		},
	{0x00, 0x01, 0x03, 0x00, "1:00/coin"		},
	{0x00, 0x01, 0x03, 0x01, "1:30/coin"		},
	{0x00, 0x01, 0x03, 0x02, "2:00/coin"		},
};

STDDIPINFOEXT(Spacewar, Spacewar, CinematCfg)

static struct BurnDIPInfo SpaceshpDIPList[]=
{
	DIP_OFFSET(0x17)
	{0x00, 0xff, 0xff, 0xfc, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Time"				},
	{0x00, 0x01, 0x03, 0x00, "1:00/coin"		},
	{0x00, 0x01, 0x03, 0x01, "1:30/coin"		},
	{0x00, 0x01, 0x03, 0x02, "2:00/coin"		},
	{0x00, 0x01, 0x03, 0x03, "2:30/coin"		},
};

STDDIPINFOEXT(Spaceshp, Spaceshp, CinematCfg)

static struct BurnDIPInfo BarrierDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0xfe, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x00, 0x01, 0x01, 0x00, "3"				},
	{0x00, 0x01, 0x01, 0x01, "5"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x00, 0x01, 0x02, 0x00, "Off"				},
	{0x00, 0x01, 0x02, 0x02, "On"				},
};

STDDIPINFOEXT(Barrier, Barrier, CinematOvl)

static struct BurnDIPInfo SpeedfrkDIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0xfe, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Extra Time"		},
	{0x00, 0x01, 0x03, 0x00, "69"				},
	{0x00, 0x01, 0x03, 0x01, "99"				},
	{0x00, 0x01, 0x03, 0x02, "129"				},
	{0x00, 0x01, 0x03, 0x03, "159"				},
};

STDDIPINFOEXT(Speedfrk, Speedfrk, CinematCfg)

static struct BurnDIPInfo StarhawkDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0xff, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Game Time"		},
	{0x00, 0x01, 0x03, 0x03, "2:00/4:00"		},
	{0x00, 0x01, 0x03, 0x01, "1:30/3:00"		},
	{0x00, 0x01, 0x03, 0x02, "1:00/2:00"		},
	{0x00, 0x01, 0x03, 0x00, "0:45/1:30"		},
};

STDDIPINFOEXT(Starhawk, Starhawk, CinematCfg)

static struct BurnDIPInfo SundanceDIPList[]=
{
	DIP_OFFSET(0x1c)
	{0x00, 0xff, 0xff, 0xfa, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Time"				},
	{0x00, 0x01, 0x03, 0x00, "0:45/coin"		},
	{0x00, 0x01, 0x03, 0x02, "1:00/coin"		},
	{0x00, 0x01, 0x03, 0x01, "1:30/coin"		},
	{0x00, 0x01, 0x03, 0x03, "2:00/coin"		},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x00, 0x01, 0x04, 0x04, "Japanese"			},
	{0x00, 0x01, 0x04, 0x00, "English"			},
};

STDDIPINFOEXT(Sundance, Sundance, CinematOvl)

static struct BurnDIPInfo TailgDIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0xff, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Shield Points"	},
	{0x00, 0x01, 0x23, 0x00, "15"				},
	{0x00, 0x01, 0x23, 0x02, "20"				},
	{0x00, 0x01, 0x23, 0x01, "30"				},
	{0x00, 0x01, 0x23, 0x03, "40"				},
	{0x00, 0x01, 0x23, 0x20, "50"				},
	{0x00, 0x01, 0x23, 0x22, "60"				},
	{0x00, 0x01, 0x23, 0x21, "70"				},
	{0x00, 0x01, 0x23, 0x23, "80"				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x00, 0x01, 0x04, 0x00, "2 Coins 1 Credit"	},
	{0x00, 0x01, 0x04, 0x04, "1 Coin  1 Credit"	},
};

STDDIPINFOEXT(Tailg, Tailg, CinematOvl)

static struct BurnDIPInfo WarriorDIPList[]=
{
	DIP_OFFSET(0x0c)
	{0x00, 0xff, 0xff, 0x8a, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Time"				},
	{0x00, 0x01, 0x03, 0x00, "0:30/coin"		},
	{0x00, 0x01, 0x03, 0x02, "1:00/coin"		},
	{0x00, 0x01, 0x03, 0x01, "1:30/coin"		},
	{0x00, 0x01, 0x03, 0x03, "2:00/coin"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x04, 0x00, "Off"				},
	{0x00, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x00, 0x01, 0x08, 0x00, "2 Coins 1 Credit"	},
	{0x00, 0x01, 0x08, 0x08, "1 Coin  1 Credit"	},
};

STDDIPINFOEXT(Warrior, Warrior, CinematCfg)

static struct BurnDIPInfo ArmoraDIPList[]=
{
	DIP_OFFSET(0x0c)
	{0x00, 0xff, 0xff, 0xaf, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x00, 0x01, 0x03, 0x00, "2"				},
	{0x00, 0x01, 0x03, 0x02, "3"				},
	{0x00, 0x01, 0x03, 0x01, "4"				},
	{0x00, 0x01, 0x03, 0x03, "5"				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Credit"	},
	{0x00, 0x01, 0x0c, 0x00, "4 Coins 3 Credits"},
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"	},
	{0x00, 0x01, 0x0c, 0x08, "2 Coins 3 Credits"},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x00, 0x01, 0x10, 0x10, "Off"				},
	{0x00, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x40, 0x00, "Off"				},
	{0x00, 0x01, 0x40, 0x40, "On"				},
};

STDDIPINFOEXT(Armora, Armora, CinematCfg)

static struct BurnDIPInfo RipoffDIPList[]=
{
	DIP_OFFSET(0x0c)
	{0x00, 0xff, 0xff, 0xd3, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x00, 0x01, 0x03, 0x01, "4"				},
	{0x00, 0x01, 0x03, 0x03, "8"				},
	{0x00, 0x01, 0x03, 0x00, "12"				},
	{0x00, 0x01, 0x03, 0x02, "16"				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Credit"	},
	{0x00, 0x01, 0x0c, 0x0c, "4 Coins 3 Credits"},
	{0x00, 0x01, 0x0c, 0x00, "1 Coin  1 Credit"	},
	{0x00, 0x01, 0x0c, 0x08, "2 Coins 3 Credits"},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x00, 0x01, 0x10, 0x00, "Off"				},
	{0x00, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Scores"			},
	{0x00, 0x01, 0x20, 0x00, "Individual"		},
	{0x00, 0x01, 0x20, 0x20, "Combined"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x40, 0x40, "Off"				},
	{0x00, 0x01, 0x40, 0x00, "On"				},
};

STDDIPINFOEXT(Ripoff, Ripoff, CinematCfg)

static struct BurnDIPInfo StarcasDIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0xbf, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x00, 0x01, 0x03, 0x03, "3"				},
	{0x00, 0x01, 0x03, 0x01, "4"				},
	{0x00, 0x01, 0x03, 0x02, "5"				},
	{0x00, 0x01, 0x03, 0x00, "6"				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Credit"	},
	{0x00, 0x01, 0x0c, 0x00, "4 Coins 3 Credits"},
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"	},
	{0x00, 0x01, 0x0c, 0x08, "2 Coins 3 Credits"},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x40, 0x00, "Off"				},
	{0x00, 0x01, 0x40, 0x40, "On"				},
};

STDDIPINFOEXT(Starcas, Starcas, CinematOvl)

static struct BurnDIPInfo StarcascDIPList[]=
{
	DIP_OFFSET(0x0c)
	{0x00, 0xff, 0xff, 0xbf, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x00, 0x01, 0x03, 0x03, "3"				},
	{0x00, 0x01, 0x03, 0x01, "4"				},
	{0x00, 0x01, 0x03, 0x02, "5"				},
	{0x00, 0x01, 0x03, 0x00, "6"				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Credit"	},
	{0x00, 0x01, 0x0c, 0x00, "4 Coins 3 Credits"},
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"	},
	{0x00, 0x01, 0x0c, 0x08, "2 Coins 3 Credits"},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x40, 0x00, "Off"				},
	{0x00, 0x01, 0x40, 0x40, "On"				},
};

STDDIPINFOEXT(Starcasc, Starcasc, CinematOvl)

static struct BurnDIPInfo SolarqDIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0xb7, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x00, 0x01, 0x05, 0x01, "2 Coins 1 Credit"	},
	{0x00, 0x01, 0x05, 0x00, "4 Coins 3 Credits"},
	{0x00, 0x01, 0x05, 0x05, "1 Coin  1 Credit"	},
	{0x00, 0x01, 0x05, 0x04, "2 Coins 3 Credits"},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x00, 0x01, 0x02, 0x02, "25 captures"		},
	{0x00, 0x01, 0x02, 0x00, "40 captures"		},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x00, 0x01, 0x18, 0x18, "2"				},
	{0x00, 0x01, 0x18, 0x08, "3"				},
	{0x00, 0x01, 0x18, 0x10, "4"				},
	{0x00, 0x01, 0x18, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x00, 0x01, 0x20, 0x20, "Off"				},
	{0x00, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x40, 0x00, "Off"				},
	{0x00, 0x01, 0x40, 0x40, "On"				},
};

STDDIPINFOEXT(Solarq, Solarq, CinematOvl)

static struct BurnDIPInfo BoxingbDIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0xe3, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Credit"	},
	{0x00, 0x01, 0x03, 0x00, "4 Coins 3 Credits"},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Credit"	},
	{0x00, 0x01, 0x03, 0x02, "2 Coins 3 Credits"},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x00, 0x01, 0x04, 0x04, "3"				},
	{0x00, 0x01, 0x04, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x00, 0x01, 0x08, 0x00, "30,000"			},
	{0x00, 0x01, 0x08, 0x08, "50,000"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x00, 0x01, 0x10, 0x10, "Off"				},
	{0x00, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x00, 0x01, 0x20, 0x20, "Off"				},
	{0x00, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x40, 0x40, "Off"				},
	{0x00, 0x01, 0x40, 0x00, "On"				},
};

STDDIPINFOEXT(Boxingb, Boxingb, CinematCfg)

static struct BurnDIPInfo WotwDIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0xfd, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x00, 0x01, 0x02, 0x00, "3"				},
	{0x00, 0x01, 0x02, 0x02, "5"				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x00, 0x01, 0x08, 0x08, "1 Coin  1 Credit"	},
	{0x00, 0x01, 0x08, 0x00, "2 Coins 3 Credits"},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x00, 0x01, 0x20, 0x20, "Off"				},
	{0x00, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x40, 0x40, "Off"				},
	{0x00, 0x01, 0x40, 0x00, "On"				},
};

STDDIPINFOEXT(Wotw, Wotw, CinematOvl)
STDDIPINFOEXT(Wotwc, Wotw, CinematCfg)

static struct BurnDIPInfo DemonDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xf3, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Credit"	},
	{0x00, 0x01, 0x03, 0x00, "4 Coins 3 Credits"},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Credit"	},
	{0x00, 0x01, 0x03, 0x02, "2 Coins 3 Credits"},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x00, 0x01, 0x0c, 0x00, "3"				},
	{0x00, 0x01, 0x0c, 0x04, "4"				},
	{0x00, 0x01, 0x0c, 0x08, "5"				},
	{0x00, 0x01, 0x0c, 0x0c, "6"				},

	{0   , 0xfe, 0   ,    3, "Starting Difficulty"	},
	{0x00, 0x01, 0x30, 0x30, "1"				},
	{0x00, 0x01, 0x30, 0x10, "5"				},
	{0x00, 0x01, 0x30, 0x00, "10"				},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x00, 0x01, 0x40, 0x40, "Off"				},
	{0x00, 0x01, 0x40, 0x00, "On"				},
};

STDDIPINFOEXT(Demon, Demon, CinematOvl)

static struct BurnDIPInfo Qb3DIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0xea, NULL				},
	{0x01, 0xff, 0xff, 0x12, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x00, 0x01, 0x03, 0x00, "2"				},
	{0x00, 0x01, 0x03, 0x02, "3"				},
	{0x00, 0x01, 0x03, 0x01, "4"				},
	{0x00, 0x01, 0x03, 0x03, "5"				},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x00, 0x01, 0x08, 0x08, "Off"				},
	{0x00, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x40, 0x40, "Off"				},
	{0x00, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Debug"			},
	{0x01, 0x01, 0x02, 0x02, "Off"				},
	{0x01, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Infinite Lives"	},
	{0x01, 0x01, 0x10, 0x10, "Off"				},
	{0x01, 0x01, 0x10, 0x00, "On"				},
};

STDDIPINFOEXT(Qb3, Qb3, CinematCfg)

// ---------------------------------------------------------------------------
// Color overlays (from MAME's layouts, blend="multiply")

struct overlay_prim {
	INT32 disk;
	float x0, y0, x1, y1;	// element coordinates
	float r, g, b;
};

struct overlay_def {
	const overlay_prim *prims;
	INT32 count;
	float offx, offy;		// screen (0..1) -> element: off + u * span
	float spanx, spany;
};

static const overlay_prim starcas_ovl_prims[] = {
	{ 0, 0.0000f, 0.1250f, 1.0000f, 0.8750f, 0.0f, 0.25f, 1.0f    },
	{ 1, 0.3775f, 0.3775f, 0.6225f, 0.6225f, 1.0f, 0.125f, 0.125f },
	{ 1, 0.4050f, 0.4050f, 0.5950f, 0.5950f, 1.0f, 0.5f, 0.0625f  },
	{ 1, 0.4275f, 0.4275f, 0.5725f, 0.5725f, 1.0f, 1.0f, 0.125f   },
};

static const overlay_prim solarq_ovl_prims[] = {
	{ 0, 0.00f, 0.125f, 1.00f, 0.200f, 1.0f, 0.125f, 0.125f },
	{ 0, 0.00f, 0.200f, 1.00f, 0.875f, 0.125f, 0.125f, 1.0f },
	{ 1, 0.47f, 0.470f, 0.53f, 0.530f, 1.0f, 1.0f, 0.125f   },
};

static const overlay_prim demon_ovl_prims[] = {
	{ 0,   0,   0, 400, 300, 0.0f, 0.25f, 1.0f },
	{ 0,   0,   0, 400,  22, 1.0f, 0.25f, 0.0f },
	{ 0, 115,  50, 149, 118, 1.0f, 1.0f,  0.0f },
	{ 0, 154, 178, 243, 215, 1.0f, 1.0f,  0.0f },
};

static const overlay_prim wotw_ovl_prims[] = {
	{ 0, 0,   0, 500, 300, 1.0f, 0.15f, 0.15f },
	{ 1, 0,  66, 500, 236, 1.0f, 0.29f, 0.15f },
	{ 1, 0,  68, 500, 238, 1.0f, 0.43f, 0.15f },
	{ 1, 0,  70, 500, 240, 1.0f, 0.57f, 0.15f },
	{ 1, 0,  72, 500, 242, 1.0f, 0.71f, 0.15f },
	{ 1, 0,  74, 500, 244, 1.0f, 0.85f, 0.15f },
	{ 1, 0,  76, 500, 246, 1.0f, 1.00f, 0.15f },
	{ 0, 0, 166, 500, 300, 1.0f, 1.00f, 0.15f },
	{ 0, 0, 223, 500, 300, 1.0f, 0.30f, 0.50f },
};

static const overlay_prim barrier_ovl_prims[]  = { { 0, 0, 0, 1, 1, 0.25f,  0.4f, 1.0f   } };
static const overlay_prim sundance_ovl_prims[] = { { 0, 0, 0, 1, 1, 1.0f,   1.0f, 0.125f } };
static const overlay_prim tailg_ovl_prims[]    = { { 0, 0, 0, 1, 1, 0.125f, 1.0f, 1.0f   } };

#define OVL(p, ox, oy, sx, sy) { p, sizeof(p) / sizeof(p[0]), ox, oy, sx, sy }

static const overlay_def starcas_overlay  = OVL(starcas_ovl_prims,  0.0f, 0.125f, 1.0f, 0.75f);
static const overlay_def solarq_overlay   = OVL(solarq_ovl_prims,   0.0f, 0.125f, 1.0f, 0.75f);
static const overlay_def demon_overlay    = OVL(demon_ovl_prims,    0.0f, 0.0f, 400.0f, 300.0f);
static const overlay_def wotw_overlay     = OVL(wotw_ovl_prims,    50.0f, 0.0f, 400.0f, 300.0f);
static const overlay_def barrier_overlay  = OVL(barrier_ovl_prims,  0.0f, 0.0f, 1.0f, 1.0f);
static const overlay_def sundance_overlay = OVL(sundance_ovl_prims, 0.0f, 0.0f, 1.0f, 1.0f);
static const overlay_def tailg_overlay    = OVL(tailg_ovl_prims,    0.0f, 0.0f, 1.0f, 1.0f);

#undef OVL

static const overlay_def *overlay = NULL;
static UINT32 *overlay_bitmap = NULL;
static INT32 overlay_w = 0, overlay_h = 0;

static void overlay_build()
{
	BurnFree(overlay_bitmap);

	overlay_w = nScreenWidth;
	overlay_h = nScreenHeight;
	overlay_bitmap = (UINT32*)BurnMalloc(overlay_w * overlay_h * sizeof(UINT32));

	for (INT32 y = 0; y < overlay_h; y++)
	{
		float ey = overlay->offy + ((y + 0.5f) / overlay_h) * overlay->spany;

		for (INT32 x = 0; x < overlay_w; x++)
		{
			float ex = overlay->offx + ((x + 0.5f) / overlay_w) * overlay->spanx;
			float r = 1.0f, g = 1.0f, b = 1.0f;

			for (INT32 i = 0; i < overlay->count; i++)
			{
				const overlay_prim *p = &overlay->prims[i];
				INT32 inside;

				if (p->disk) {
					float rx = (p->x1 - p->x0) / 2, ry = (p->y1 - p->y0) / 2;
					float dx = (ex - (p->x0 + rx)) / rx, dy = (ey - (p->y0 + ry)) / ry;
					inside = (dx * dx + dy * dy) <= 1.0f;
				} else {
					inside = (ex >= p->x0 && ex < p->x1 && ey >= p->y0 && ey < p->y1);
				}

				if (inside) { r = p->r; g = p->g; b = p->b; }
			}

			overlay_bitmap[y * overlay_w + x] = ((INT32)(r * 255) << 16) | ((INT32)(g * 255) << 8) | (INT32)(b * 255);
		}
	}
}

static UINT32 overlay_pix_cb(INT32 x, INT32 y, UINT32 color)
{
	if (overlay == NULL || (DrvDips[2] & 2) == 0 || BurnArtworkActive()) return color; // external artwork replaces the built-in overlay

	if (overlay_bitmap == NULL || overlay_w != nScreenWidth || overlay_h != nScreenHeight)
		overlay_build();

	UINT32 o = overlay_bitmap[y * overlay_w + x];

	INT32 r = (((color >> 16) & 0xff) * ((o >> 16) & 0xff)) / 255;
	INT32 g = (((color >>  8) & 0xff) * ((o >>  8) & 0xff)) / 255;
	INT32 b = (((color >>  0) & 0xff) * ((o >>  0) & 0xff)) / 255;

	return (r << 16) | (g << 8) | b;
}

// ---------------------------------------------------------------------------
// Sound (samples)

#define RISING_EDGE(bit, changed, val)  (((changed) & (bit)) && ((val) & (bit)))
#define FALLING_EDGE(bit, changed, val) (((changed) & (bit)) && !((val) & (bit)))

#define SHIFTREG_RISING_EDGE(bit)       RISING_EDGE(bit, (last_shift ^ current_shift), current_shift)
#define SHIFTREG_FALLING_EDGE(bit)      FALLING_EDGE(bit, (last_shift ^ current_shift), current_shift)

#define SHIFTREG2_RISING_EDGE(bit)      RISING_EDGE(bit, (last_shift2 ^ current_shift), current_shift)
#define SHIFTREG2_FALLING_EDGE(bit)     FALLING_EDGE(bit, (last_shift2 ^ current_shift), current_shift)

#define Q(n)	((outlatch >> (n)) & 1)

static inline void sample_start(INT32 ch, INT32 sam, INT32 loop = 0)
{
	BurnSampleChannelPlay(ch, sam, loop);
}

static inline void sample_stop(INT32 ch)
{
	BurnSampleChannelStop(ch, true);
}

static inline INT32 sample_playing(INT32 ch)
{
	return (BurnSampleGetChannelStatus(ch) == SAMPLE_PLAYING);
}

// MAME's set_frequency(): playback frequency in Hz, sample-indexed here
static void sample_set_frequency(INT32 sam, UINT32 freq)
{
	UINT32 src = BurnSampleGetSourceRate(sam);
	if (src == 0) return;
	INT32 rate = (INT32)(((UINT64)freq * 100) / src);
	if (rate > 4000) rate = 4000;
	BurnSampleSetPlaybackRate(sam, rate);
}

static void sample_set_volume(INT32 sam, double vol)
{
	BurnSampleSetAllRoutesFade(sam, vol * SAMPLE_VOL, BURN_SND_ROUTE_BOTH);
}

static void spacewar_sound_w(INT32 bit, INT32 state)
{
	switch (bit)
	{
		case 0: // explosion - rising edge
			if (state) sample_start(0, (BurnRandom() & 1) ? 0 : 6);
		break;

		case 1: // fire - rising edge
			if (state) sample_start(1, (BurnRandom() & 1) ? 1 : 7);
		break;

		case 2: // player 1 thrust - 0=on, 1=off
			if (!state) sample_start(3, 3, 1); else sample_stop(3);
		break;

		case 3: // player 2 thrust - 0=on, 1=off
			if (!state) sample_start(4, 4, 1); else sample_stop(4);
		break;

		case 4: // mute - 0=off, 1=on
			if (!state) {
				sample_start(2, 2, 1); // idle sound
			} else {
				for (INT32 i = 0; i < 5; i++)
					if (i != 2) sample_stop(i);

				sample_start(2, 5); // pop when board is shut off
			}
		break;
	}
}

static void barrier_sound_w(INT32 bit, INT32 state)
{
	switch (bit)
	{
		case 0: if (state) sample_start(0, 0); break;	// player die - rising edge
		case 1: if (!state) sample_start(1, 1); break;	// player move - falling edge
		case 2: if (!state) sample_start(2, 2); break;	// enemy move - falling edge
	}
}

static void speedfrk_sound_w(INT32 bit, INT32 state)
{
	switch (bit)
	{
		case 3:
			// on the falling edge of bit 0x08, clock the inverse of bit 0x04 into the top of the shiftreg
			// (engine pitch/volume, not emulated with samples)
			if (!state)
				current_shift = ((current_shift >> 1) & 0x7fff) | ((~Q(2) << 13) & 1);
		break;

		case 4: // off-road - 1=on, 0=off
			if (state) sample_start(0, 0, 1); else sample_stop(0);
		break;
	}
}

static void starhawk_sound_w(INT32 bit, INT32 state)
{
	switch (bit)
	{
		case 0: if (!state) sample_start(0, 0); break;	// explosion - falling edge
		case 1: if (!state) sample_start(1, 1); break;	// right laser - falling edge
		case 2: if (!state) sample_start(2, 2); break;	// left laser - falling edge

		case 3: // K - 0=on, 1=off
			if (!state) sample_start(3, 3, 1); else sample_stop(3);
		break;

		case 4: // master - 0=on, 1=off
			if (!state) sample_start(4, 4, 1); else sample_stop(4);
		break;

		case 7: // K exit - 1=on, 0=off
			if (state && !Q(3)) sample_start(3, 5, 1);
			if (!state) sample_stop(3);
		break;
	}
}

static void sundance_sound_w(INT32 bit, INT32 state)
{
	switch (bit)
	{
		case 0: if (!state) sample_start(0, 0); break;	// bong
		case 1: if (!state) sample_start(1, 1); break;	// whoosh
		case 2: if (!state) sample_start(2, 2); break;	// explosion
		case 3: if (!state) sample_start(3, 3); break;	// ping
		case 4: if (!state) sample_start(4, 4); break;	// ping
		case 7: if (!state) sample_start(5, 5); break;	// hatch
	}
}

static void tailg_sound_w(INT32 bit, INT32 state)
{
	// the falling edge of bit 0x10 clocks bit 0x08 into the mux selected by bits 0x07
	if (bit != 4 || state) return;

	INT32 sel = outlatch & 7;
	current_shift = (current_shift & ~(1 << sel)) | (Q(3) << sel);

	// explosion - falling edge
	if (SHIFTREG_FALLING_EDGE(0x01)) sample_start(0, 0);

	// rumble - 0=on, 1=off
	if (SHIFTREG_FALLING_EDGE(0x02)) sample_start(1, 1, 1);
	if (SHIFTREG_RISING_EDGE(0x02)) sample_stop(1);

	// laser - 0=on, 1=off
	if (SHIFTREG_FALLING_EDGE(0x04)) sample_start(2, 2, 1);
	if (SHIFTREG_RISING_EDGE(0x04)) sample_stop(2);

	// shield - 0=on, 1=off
	if (SHIFTREG_FALLING_EDGE(0x08)) sample_start(3, 3, 1);
	if (SHIFTREG_RISING_EDGE(0x08)) sample_stop(3);

	// bounce - falling edge
	if (SHIFTREG_FALLING_EDGE(0x10)) sample_start(4, 4);

	// hyperspace - falling edge
	if (SHIFTREG_FALLING_EDGE(0x20)) sample_start(5, 5);

	last_shift = current_shift;
}

static void warrior_sound_w(INT32 bit, INT32 state)
{
	switch (bit)
	{
		case 0: // normal level - 0=on, 1=off
			if (!state) sample_start(0, 0, 1); else sample_stop(0);
		break;

		case 1: // hi level - 0=on, 1=off
			if (!state) sample_start(1, 1, 1); else sample_stop(1);
		break;

		case 2: if (!state) sample_start(2, 2); break;	// explosion
		case 3: if (!state) sample_start(3, 3); break;	// fall
		case 4: if (!state) sample_start(4, 4); break;	// appear
	}
}

static void armora_sound_w(INT32 bit, INT32 state)
{
	switch (bit)
	{
		case 4: // on the rising edge of bit 0x10, clock bit 0x80 into the shift register
			if (state) current_shift = ((current_shift >> 1) & 0x7f) | (Q(7) << 7);
		break;

		case 0: // execute on the rising edge of bit 0x01
			if (state) {
				if (SHIFTREG_FALLING_EDGE(0x10)) sample_start(0, 0);	// lo explosion
				if (SHIFTREG_FALLING_EDGE(0x20)) sample_start(1, 1);	// jeep fire
				if (SHIFTREG_FALLING_EDGE(0x40)) sample_start(2, 2);	// hi explosion
				if (SHIFTREG_FALLING_EDGE(0x80)) sample_start(3, 3);	// tank fire
				last_shift = current_shift;
			}
		break;

		case 1: // tank sound - 0=on, 1=off
			if (!state) sample_start(4, 4, 1); else sample_stop(4);
		break;

		case 2: // beep sound - 0=on, 1=off
			if (!state) sample_start(5, 5, 1); else sample_stop(5);
		break;

		case 3: // chopper sound - 0=on, 1=off
			if (!state) sample_start(6, 6, 1); else sample_stop(6);
		break;
	}
}

static void ripoff_sound_w(INT32 bit, INT32 state)
{
	switch (bit)
	{
		case 1: // on the rising edge of bit 0x02, clock bit 0x01 into the shift register
			if (state) current_shift = ((current_shift >> 1) & 0x7f) | (Q(0) << 7);
		break;

		case 2: // execute on the rising edge of bit 0x04
			if (state) {
				// background - 0=on, 1=off, selected by bits 0x38
				if ((((current_shift ^ last_shift) & 0x38) && !(current_shift & 0x04)) || SHIFTREG_FALLING_EDGE(0x04))
					sample_start(5, 5 + ((current_shift >> 5) & 7), 1);
				if (SHIFTREG_RISING_EDGE(0x04))
					sample_stop(5);

				// beep - falling edge
				if (SHIFTREG_FALLING_EDGE(0x02)) sample_start(0, 0);

				// motor - 0=on, 1=off
				if (SHIFTREG_FALLING_EDGE(0x01)) sample_start(1, 1, 1);
				if (SHIFTREG_RISING_EDGE(0x01)) sample_stop(1);

				last_shift = current_shift;
			}
		break;

		case 3: if (!state) sample_start(2, 2); break;	// torpedo
		case 4: if (!state) sample_start(3, 3); break;	// laser
		case 7: if (!state) sample_start(4, 4); break;	// explosion
	}
}

// star castle & war of the worlds share the same board (different drone pitch)
static void starcas_wotw_sound_w(INT32 bit, INT32 state, UINT32 pitch_base, UINT32 pitch_dn, UINT32 pitch_up)
{
	switch (bit)
	{
		case 4: // on the rising edge of bit 0x10, clock bit 0x80 into the shift register
			if (state) current_shift = ((current_shift >> 1) & 0x7f) | (Q(7) << 7);
		break;

		case 0: // execute on the rising edge of bit 0x01
			if (state) {
				// fireball - falling edge
				if (SHIFTREG_FALLING_EDGE(0x80)) sample_start(0, 0);

				// shield hit - falling edge
				if (SHIFTREG_FALLING_EDGE(0x40)) sample_start(1, 1);

				// star sound - 0=off, 1=on
				if (SHIFTREG_RISING_EDGE(0x20)) sample_start(2, 2, 1);
				if (SHIFTREG_FALLING_EDGE(0x20)) sample_stop(2);

				// thrust sound - 1=off, 0=on
				if (SHIFTREG_FALLING_EDGE(0x10)) sample_start(3, 3, 1);
				if (SHIFTREG_RISING_EDGE(0x10)) sample_stop(3);

				// drone - 1=off, 0=on
				if (SHIFTREG_FALLING_EDGE(0x08)) sample_start(4, 4, 1);
				if (SHIFTREG_RISING_EDGE(0x08)) sample_stop(4);

				// latch the drone pitch
				UINT32 target_pitch = (current_shift & 7) + ((current_shift & 2) << 2);
				target_pitch = pitch_base + (target_pitch << 12);

				// once per frame slide the pitch toward the target
				if (frame_counter > last_frame)
				{
					if (current_pitch > target_pitch) current_pitch -= pitch_dn;
					if (current_pitch < target_pitch) current_pitch += pitch_up;
					sample_set_frequency(4, current_pitch);
					last_frame = frame_counter;
				}

				last_shift = current_shift;
			}
		break;

		case 1: if (!state) sample_start(5, 5); break;	// loud explosion
		case 2: if (!state) sample_start(6, 6); break;	// soft explosion
		case 3: if (!state) sample_start(7, 7); break;	// player fire
	}
}

static void solarq_sound_w(INT32 bit, INT32 state)
{
	switch (bit)
	{
		case 4: // on the rising edge of bit 0x10, clock bit 0x80 into the shift register
			if (state) current_shift = ((current_shift >> 1) & 0x7fff) | (Q(7) << 15);
		break;

		case 1: // execute on the rising edge of bit 0x02
			if (state) {
				// only the upper 8 bits matter
				current_shift >>= 8;

				// loud explosion - falling edge
				if (SHIFTREG_FALLING_EDGE(0x80)) sample_start(0, 0);

				// soft explosion - falling edge
				if (SHIFTREG_FALLING_EDGE(0x40)) sample_start(1, 1);

				// thrust - 0=on, 1=off
				if (SHIFTREG_FALLING_EDGE(0x20)) {
					target_volume = 1.0;
					if (!sample_playing(2)) sample_start(2, 2, 1);
				}
				if (SHIFTREG_RISING_EDGE(0x20))
					target_volume = 0;

				// ramp the thrust volume
				if (sample_playing(2) && frame_counter > last_frame)
				{
					if (current_volume > target_volume) current_volume -= 0.078;
					if (current_volume < target_volume) current_volume += 0.078;
					if (current_volume > 0)
						sample_set_volume(2, current_volume);
					else
						sample_stop(2);
					last_frame = frame_counter;
				}

				// fire - falling edge
				if (SHIFTREG_FALLING_EDGE(0x10)) sample_start(3, 3);

				// capture - falling edge
				if (SHIFTREG_FALLING_EDGE(0x08)) sample_start(4, 4);

				// nuke - 1=on, 0=off
				if (SHIFTREG_RISING_EDGE(0x04)) sample_start(5, 5, 1);
				if (SHIFTREG_FALLING_EDGE(0x04)) sample_stop(5);

				// photon - falling edge
				if (SHIFTREG_FALLING_EDGE(0x02)) sample_start(6, 6);

				last_shift = current_shift;
			}
		break;

		case 0: // clock music data on the rising edge of bit 0x01
			if (state) {
				// start/stop the music sample on the high bit
				if (SHIFTREG2_RISING_EDGE(0x8000)) sample_start(7, 7, 1);
				if (SHIFTREG2_FALLING_EDGE(0x8000)) sample_stop(7);

				// set the frequency
				INT32 freq = (INT32)(56818.181818 / (4096 - (current_shift & 0xfff)));
				sample_set_frequency(7, 44100 * freq / 1050);

				// set the volume
				INT32 vol = (~current_shift >> 12) & 7;
				sample_set_volume(7, vol / 7.0);

				last_shift2 = current_shift;
			}
		break;
	}
}

static void boxingb_sound_w(INT32 bit, INT32 state)
{
	switch (bit)
	{
		case 4: // on the rising edge of bit 0x10, clock bit 0x80 into the shift register
			if (state) current_shift = ((current_shift >> 1) & 0x7fff) | (Q(7) << 15);
		break;

		case 1: // execute on the rising edge of bit 0x02
			if (state) {
				// only the upper 8 bits matter
				current_shift >>= 8;

				if (SHIFTREG_FALLING_EDGE(0x80)) sample_start(0, 0);	// soft explosion
				if (SHIFTREG_FALLING_EDGE(0x40)) sample_start(1, 1);	// loud explosion

				// chirping birds - 0=on, 1=off
				if (SHIFTREG_FALLING_EDGE(0x20)) sample_start(2, 2);
				if (SHIFTREG_RISING_EDGE(0x20)) sample_stop(2);

				if (SHIFTREG_FALLING_EDGE(0x10)) sample_start(3, 3);	// egg cracking
				if (SHIFTREG_RISING_EDGE(0x08)) sample_start(4, 4);		// bug pushing A
				if (SHIFTREG_RISING_EDGE(0x04)) sample_start(5, 5);		// bug pushing B
				if (SHIFTREG_FALLING_EDGE(0x02)) sample_start(6, 6);	// bug dying
				if (SHIFTREG_FALLING_EDGE(0x01)) sample_start(7, 7);	// beetle on screen

				last_shift = current_shift;
			}
		break;

		case 0: // clock music data on the rising edge of bit 0x01
			if (state) {
				// start/stop the music sample on the high bit
				if (SHIFTREG2_RISING_EDGE(0x8000)) sample_start(8, 8, 1);
				if (SHIFTREG2_FALLING_EDGE(0x8000)) sample_stop(8);

				// set the frequency
				INT32 freq = (INT32)(56818.181818 / (4096 - (current_shift & 0xfff)));
				sample_set_frequency(8, 44100 * freq / 1050);

				// set the volume
				INT32 vol = (~current_shift >> 12) & 3;
				sample_set_volume(8, vol / 3.0);

				// cannon
				if (SHIFTREG2_RISING_EDGE(0x4000)) sample_start(9, 9);

				last_shift2 = current_shift;
			}
		break;

		case 2: if (state) sample_start(10, 10); break;	// bounce - rising edge
		case 3: if (state) sample_start(11, 11); break;	// bell
	}
}

static void demon_sound4_w(INT32 state)
{
	// watch for a 0->1 edge on bit 4 ("shift in") to clock in the new data
	if (state) {
		sound_fifo[sound_fifo_in] = ~outlatch & 0x0f;
		sound_fifo_in = (sound_fifo_in + 1) % 16;
	}
}

static void sound_w(INT32 bit, INT32 state)
{
	switch (sound_type)
	{
		case SND_SPACEWAR: spacewar_sound_w(bit, state); break;
		case SND_BARRIER:  barrier_sound_w(bit, state); break;
		case SND_SPEEDFRK: speedfrk_sound_w(bit, state); break;
		case SND_STARHAWK: starhawk_sound_w(bit, state); break;
		case SND_SUNDANCE: sundance_sound_w(bit, state); break;
		case SND_TAILG:    tailg_sound_w(bit, state); break;
		case SND_WARRIOR:  warrior_sound_w(bit, state); break;
		case SND_ARMORA:   armora_sound_w(bit, state); break;
		case SND_RIPOFF:   ripoff_sound_w(bit, state); break;
		case SND_STARCAS:  starcas_wotw_sound_w(bit, state, 0x5800, 225, 150); break;
		case SND_WOTW:     starcas_wotw_sound_w(bit, state, 0x10000, 300, 200); break;
		case SND_SOLARQ:   solarq_sound_w(bit, state); break;
		case SND_BOXINGB:  boxingb_sound_w(bit, state); break;
		case SND_DEMON:    if (bit == 4 && !is_qb3) demon_sound4_w(state); break;
	}
}

// ---------------------------------------------------------------------------
// Demon / QB-3 sound board (Z80, CTC, 3x AY8910)

static UINT8 demon_sound_porta_r(UINT32)
{
	// bits 0-3 are the sound data; bit 4 is the data ready
	return sound_fifo[sound_fifo_out] | ((sound_fifo_in != sound_fifo_out) << 4);
}

static UINT8 demon_sound_portb_r(UINT32)
{
	return last_portb_write;
}

static void demon_sound_portb_w(UINT32, UINT32 data)
{
	// watch for a 0->1 edge on bit 0 ("shift out") to advance the data pointer
	if ((data & 1) != (last_portb_write & 1) && (data & 1) != 0)
		sound_fifo_out = (sound_fifo_out + 1) % 16;

	// watch for a 0->1 edge of bit 1 ("hard reset") to reset the FIFO
	if ((data & 2) != (last_portb_write & 2) && (data & 2) != 0)
		sound_fifo_in = sound_fifo_out = 0;

	// bit 2 controls the global mute (checked at render time)
	last_portb_write = data;
}

static void __fastcall demon_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4002:
		case 0x4003:
		case 0x5002:
		case 0x5003:
		case 0x6002:
		case 0x6003:
			AY8910Write(((address >> 12) & 7) - 4, ~address & 1, data);
		return;

		case 0x7000: // watchdog?
		return;
	}
}

static UINT8 __fastcall demon_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x4000:
		case 0x4001:
		case 0x5000:
		case 0x5001:
		case 0x6000:
		case 0x6001:
			return AY8910Read(((address >> 12) & 7) - 4);
	}

	return 0;
}

static void __fastcall demon_sound_write_port(UINT16 port, UINT8 data)
{
	port &= 0xff;

	if (port <= 0x03 || (port >= 0x1c && port <= 0x1f)) {
		z80ctc_write(port & 3, data);
		return;
	}
}

static UINT8 __fastcall demon_sound_read_port(UINT16)
{
	return 0;
}

static void demon_ctc_interrupt(INT32 state)
{
	ZetSetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

// ---------------------------------------------------------------------------
// Main cpu io

static const UINT8 switch_shuffle[8] = { 2,5,4,3,0,1,6,7 };

static const struct { INT32 pad; UINT16 mask; } sundance_port_map[16] =
{
	{ 1, 0x155 },	// bit  0 is set if P1 1,3,5,7,9 is pressed
	{ 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 2, 0x1a1 },	// bit  8 is set if P2 1,6,8,9 is pressed
	{ 1, 0x1a1 },	// bit  9 is set if P1 1,6,8,9 is pressed
	{ 2, 0x155 },	// bit 10 is set if P2 1,3,5,7,9 is pressed
	{ 0, 0 },
	{ 1, 0x093 },	// bit 12 is set if P1 1,2,5,8 is pressed
	{ 2, 0x093 },	// bit 13 is set if P2 1,2,5,8 is pressed
	{ 1, 0x048 },	// bit 14 is set if P1 4,8 is pressed
	{ 2, 0x048 },	// bit 15 is set if P2 4,8 is pressed
};

static UINT8 qb3_frame_r()
{
	// MAME 0.215: approximation, low during the last 10% of the frame
	INT32 total = nCcpuClock * 100 / nBurnFPS;
	INT32 pos = ccpuTotalCycles() + nExtraCycles;
	INT32 remaining = total - pos;
	return (remaining * 100 / total) >= 10;
}

static UINT8 cinemat_read_port(UINT8 port)
{
	if (port < 0x10)
	{
		switch (input_type)
		{
			case INP_SPEEDFRK:
				if (port < 4) {
					static const UINT8 speedfrk_steer[] = { 0xe, 0x6, 0x2, 0x0, 0x3, 0x7, 0xf };
					return (speedfrk_steer[wheel_delta + 3] >> port) & 1;
				}
				if (port < 7)
					return (gear != (port - 4));
			break;

			case INP_SUNDANCE:
				if (sundance_port_map[port].pad)
					return (sundance_pad[sundance_port_map[port].pad - 1] & sundance_port_map[port].mask) ? 0 : 1;
			break;

			case INP_BOXINGB:
				if (port >= 0x0c) {
					INT32 offset = port - 0x0c;
					if (!mux_select) offset += 4;
					return (boxingb_dial >> offset) & 1;
				}
			break;

			case INP_QB3:
				if (port == 0x0f)
					return qb3_frame_r();
			break;
		}

		return (DrvInputs[0] >> port) & 1;
	}

	if (port < 0x17)
		return (DrvInputs[1] >> switch_shuffle[port - 0x10]) & 1;

	return !coin_detected;
}

static UINT8 cinemat_joystick_read()
{
	INT32 xval = (INT16)(ccpuGetReg(CCPU_X) << 4) >> 4;
	return ((mux_select ? analog_x : analog_y) - (UINT32)xval) < 0x800;
}

static void vector_control_w(INT32 state)
{
	switch (color_mode)
	{
		case COLOR_BW:
			// color is either bright or dim, selected by the value sent to the port
			vector_color = state ? 1 : 0;
		break;

		case COLOR_16LEVEL:
			// on the rising edge, latch bits 0-3 of the X register as the intensity
			if (state) vector_color = ccpuGetReg(CCPU_X) & 0x0f;
		break;

		case COLOR_64LEVEL:
			// on the rising edge, latch bits 2-7 of the X register as the intensity
			if (state) vector_color = (~ccpuGetReg(CCPU_X) >> 2) & 0x3f;
		break;

		case COLOR_RGB:
			// on the rising edge, latch the X register as 4-4-4 BGR
			if (state) vector_color = ccpuGetReg(CCPU_X) & 0xfff;
		break;

		case COLOR_QB3:
			// on the falling edge remember the beam X,Y, restore them on the rising edge;
			// the Rockola color hardware did not overwrite the beam position on IV
			if (!state) {
				qb3_lastx = ccpuGetReg(CCPU_X);
				qb3_lasty = ccpuGetReg(CCPU_Y);
			} else {
				// latch the Y register as 2-3-3 BGR
				vector_color = ccpuGetReg(CCPU_Y) & 0xff;
				ccpuSetReg(CCPU_X, qb3_lastx);
				ccpuSetReg(CCPU_Y, qb3_lasty);
			}
		break;
	}
}

static void cinemat_write_port(UINT8 port, UINT8 data)
{
	if (is_qb3)
	{
		// not mapped through the LS259
		if (port == 0) {
			qb3_bank = ccpuGetReg(CCPU_P) & 3;
			ccpuMapData(DrvDataRAM + qb3_bank * 0x100);
			return;
		}

		if (port == 4) {
			sound_fifo[sound_fifo_in] = ccpuGetReg(CCPU_A) & 0x0f;
			sound_fifo_in = (sound_fifo_in + 1) % 16;
			return;
		}
	}

	// LS259 output latch
	port &= 7;
	data &= 1;

	if (((outlatch >> port) & 1) == data) return;

	outlatch = (outlatch & ~(1 << port)) | (data << port);

	switch (port)
	{
		case 5: // coin reset, rising edge clears the coin flag
			if (data) coin_detected = 0;
		break;

		case 6:
			vector_control_w(data);
		break;

		case 7:
			if (has_mux) mux_select = data;
		break;
	}

	sound_w(port, data);
}

// ---------------------------------------------------------------------------
// Video

static inline void add_point(INT32 x, INT32 y, INT32 color, INT32 intensity)
{
	if (flip_x) x = (vis_w - 1) - x;
	y = (vis_h - 1) - y;

	vector_add_point(x << 16, y << 16, color, intensity);
}

static void cinemat_vector_cb(INT16 sx, INT16 sy, INT16 ex, INT16 ey, UINT8 shift)
{
	INT32 intensity = 0xff;

	// point intensity is determined by the shift value
	if (sx == ex && sy == ey)
		intensity = 0x1ff * shift / 8;

	// move to the starting position if we're not there already
	if (sx != lastx || sy != lasty)
		add_point(sx, sy, 0, 0);

	// draw the vector
	add_point(ex, ey, vector_color, intensity);

	lastx = ex;
	lasty = ey;
}

static void DrvPaletteInit()
{
	INT32 colors = 0x1000;

	for (INT32 c = 0; c < colors; c++)
	{
		INT32 r = 0, g = 0, b = 0;

		switch (color_mode)
		{
			case COLOR_BW:
				r = g = b = (c == 0) ? 0xff : 0x80;
			break;

			case COLOR_16LEVEL:
				r = g = b = ((c & 0x0f) + 1) * 255 / 16;
			break;

			case COLOR_64LEVEL:
				r = g = b = ((c & 0x3f) + 1) * 255 / 64;
			break;

			case COLOR_RGB:
				r = ((~c >> 0) & 0x0f) * 255 / 15;
				g = ((~c >> 4) & 0x0f) * 255 / 15;
				b = ((~c >> 8) & 0x0f) * 255 / 15;
			break;

			case COLOR_QB3:
				r = ((~c >> 0) & 0x07) * 255 / 7;
				g = ((~c >> 3) & 0x07) * 255 / 7;
				b = ((~c >> 6) & 0x03) * 255 / 3;
			break;
		}

		for (INT32 j = 0; j < 256; j++)
		{
			DrvPalette[c * 256 + j] = ((r * j / 255) << 16) | ((g * j / 255) << 8) | (b * j / 255); // 32bit palette (see vector.cpp)
		}
	}
}

static INT32 res_check()
{
	INT32 Width, Height;
	BurnDrvGetFullSize(&Width, &Height); // native (unrotated) size

	INT32 target = (DrvDips[2] & 1) ? 1080 : 768;

	if (Height != target) {
		vector_rescale((target * 4) / 3, target);
		return 1;
	}

	return 0;
}

// ---------------------------------------------------------------------------

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	qb3_bank = 0;
	ccpuMapData(DrvDataRAM);
	ccpuReset();

	if (has_z80) {
		ZetOpen(0);
		ZetReset();
		ZetClose();

		AY8910Reset(0);
		AY8910Reset(1);
		AY8910Reset(2);
	}

	if (has_samples) {
		BurnSampleReset();
	}

	outlatch = 0;
	coin_detected = 0;
	mux_select = 0;
	gear = 0;

	switch (color_mode) {
		case COLOR_16LEVEL: vector_color = 0x0f; break;
		case COLOR_64LEVEL: vector_color = 0x3f; break;
		default:            vector_color = 0; break; // white
	}

	lastx = lasty = 0x7fff;
	qb3_lastx = qb3_lasty = 0;

	current_shift = 0xffff;
	last_shift = 0xffff;
	last_shift2 = 0xffff;
	last_frame = 0;
	frame_counter = 0;
	current_pitch = 0x10000;
	target_volume = 0;
	current_volume = 0;

	memset (sound_fifo, 0, sizeof(sound_fifo));
	sound_fifo_in = sound_fifo_out = 0;
	last_portb_write = 0xff;

	nExtraCycles = 0;

	vector_reset();

	res_check();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x008000;
	DrvZ80ROM		= Next; Next += 0x002000;

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * 256 * sizeof(UINT32);

	AllRam			= Next;

	DrvDataRAM		= (UINT16*)Next; Next += 0x000400 * sizeof(UINT16);
	DrvZ80RAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;

	INT32 prg_size = 0x1000 << prg_type;	// 4k/8k/16k/32k
	INT32 npairs = 0, len = 0, nibble = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);
		if ((ri.nType & 7) == 1) { npairs++; len = ri.nLen; }
		if ((ri.nType & 7) == 4) { nibble = 1; }
	}
	npairs /= 2;

	if (nibble)
	{
		// space ship: 4-bit roms, groups of 4 (even hi, even lo, odd hi, odd lo) per 0x800 bytes
		UINT8 *tmp = (UINT8*)BurnMalloc(0x400 * 4);
		INT32 group = 0;

		for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
			BurnDrvGetRomInfo(&ri, i);
			if ((ri.nType & 7) != 4) continue;

			if (BurnLoadRom(tmp + (group & 3) * 0x400, i, 1)) { BurnFree(tmp); return 1; }

			if ((group & 3) == 3) {
				UINT8 *dst = DrvMainROM + (group / 4) * 0x800;
				for (INT32 j = 0; j < 0x400; j++) {
					dst[j * 2 + 0] = ((tmp[0x000 + j] & 0x0f) << 4) | (tmp[0x400 + j] & 0x0f);
					dst[j * 2 + 1] = ((tmp[0x800 + j] & 0x0f) << 4) | (tmp[0xc00 + j] & 0x0f);
				}
			}
			group++;
		}

		BurnFree(tmp);
		return 0;
	}

	INT32 stride = prg_size / npairs;
	INT32 pair = 0, z80off = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			INT32 base = (pair / 2) * stride;
			if (BurnLoadRom(DrvMainROM + base + (pair & 1), i, 2)) return 1;
			pair++;

			if ((pair & 1) == 0) {
				// mirror smaller roms through their slot (armorar)
				for (INT32 j = len * 2; j < stride; j += len * 2)
					memcpy (DrvMainROM + base + j, DrvMainROM + base, len * 2);
			}
			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(DrvZ80ROM + z80off, i, 1)) return 1;
			z80off += ri.nLen;
			continue;
		}
	}

	return 0;
}

struct cinemat_config {
	INT32 prg_type;
	INT32 jmi;
	INT32 color;
	INT32 sound;
	INT32 input;
	INT32 mux;
	INT32 vis_w, vis_h;
	INT32 flip_x;
	const overlay_def *ovl;
};

static INT32 CinematInit(const cinemat_config *cfg)
{
	BurnSetRefreshRate(38.00);	// 19.923MHz / 4 / 16 / 16 / 16 / 16 / 2

	prg_type    = cfg->prg_type;
	use_jmi     = cfg->jmi;
	color_mode  = cfg->color;
	sound_type  = cfg->sound;
	input_type  = cfg->input;
	has_mux     = cfg->mux;
	vis_w       = cfg->vis_w;
	vis_h       = cfg->vis_h;
	flip_x      = cfg->flip_x;
	overlay     = cfg->ovl;
	is_qb3      = (color_mode == COLOR_QB3);
	has_z80     = (sound_type == SND_DEMON);
	has_samples = (sound_type != SND_NONE && sound_type != SND_DEMON);

	BurnAllocMemIndex();

	if (DrvLoadRoms()) return 1;

	if (is_qb3) {
		// this patch prevents the sound ROM from eating itself when command $0A is sent on a cube rotate
		DrvZ80ROM[0x11dc] = 0x09;
	}

	ccpuInit();
	ccpuOpen(0);
	ccpuMapProgram(DrvMainROM, prg_type);
	ccpuMapData(DrvDataRAM);
	ccpuSetReadPortHandler(cinemat_read_port);
	ccpuSetWritePortHandler(cinemat_write_port);
	ccpuSetExternalHandler(use_jmi ? NULL : cinemat_joystick_read);
	ccpuSetVectorHandler(cinemat_vector_cb);
	ccpuClose();

	if (has_z80)
	{
		ZetInit(0);
		ZetOpen(0);
		ZetDaisyInit(Z80_CTC, 0);
		ZetMapMemory(DrvZ80ROM,		0x0000, 0x1fff, MAP_ROM);
		for (INT32 i = 0x3000; i < 0x4000; i += 0x400) // 1k ram, mirror up to 0x3fff
			ZetMapMemory(DrvZ80RAM,	i, i + 0x3ff, MAP_RAM);
		ZetSetWriteHandler(demon_sound_write);
		ZetSetReadHandler(demon_sound_read);
		ZetSetOutHandler(demon_sound_write_port);
		ZetSetInHandler(demon_sound_read_port);
		z80ctc_init(nZ80Clock, nZ80Clock, 0, demon_ctc_interrupt, NULL, NULL, NULL);
		ZetClose();

		AY8910Init(0, nZ80Clock, 0);
		AY8910Init(1, nZ80Clock, 1);
		AY8910Init(2, nZ80Clock, 1);
		AY8910SetPorts(0, &demon_sound_porta_r, &demon_sound_portb_r, NULL, &demon_sound_portb_w);
		AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
		AY8910SetRoute(0, BURN_SND_AY8910_ROUTE_1, 0.00, BURN_SND_ROUTE_BOTH); // channel A is used as a low-pass filter
		AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
		AY8910SetAllRoutes(2, 0.25, BURN_SND_ROUTE_BOTH);
		AY8910SetBuffered(ZetTotalCycles, nZ80Clock);
	}

	if (has_samples)
	{
		BurnSampleInit(0);
		BurnSampleSetAllRoutesAllSamples(SAMPLE_VOL, BURN_SND_ROUTE_BOTH);
		BurnSampleSetBuffered(ccpuTotalCycles, nCcpuClock);
	}

	if (input_type == INP_BOXINGB) {
		BurnTrackballInit(1);
	}

	DrvPaletteInit();

	vector_init();
	vector_set_scale(vis_w, vis_h);
	if (overlay) vector_set_pix_cb(overlay_pix_cb);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	vector_exit();

	ccpuExit();

	if (has_z80) {
		ZetExit();
		AY8910Exit(0);
		AY8910Exit(1);
		AY8910Exit(2);
	}

	if (has_samples) {
		BurnSampleExit();
	}

	if (input_type == INP_BOXINGB) {
		BurnTrackballExit();
	}

	BurnFree(overlay_bitmap);
	overlay_bitmap = NULL;
	overlay_w = overlay_h = 0;
	overlay = NULL;

	BurnFreeMemIndex();

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (res_check()) return 0; // resolution was changed

	draw_vector(DrvPalette);

	return 0;
}

static void DrvMakeInputs()
{
	DrvInputs[0] = 0xffff;
	DrvInputs[1] = DrvDips[0];

	if (input_type == INP_BARRIER) {
		// 4-way joysticks
		UINT8 p1 = (DrvJoy1[12] << 0) | (DrvJoy1[3]  << 1) | (DrvJoy1[14] << 2) | (DrvJoy1[9]  << 3);
		UINT8 p2 = (DrvJoy1[13] << 0) | (DrvJoy1[10] << 1) | (DrvJoy1[8]  << 2) | (DrvJoy1[15] << 3);
		ProcessJoystick(&p1, 0, 0, 1, 2, 3, INPUT_4WAY | INPUT_CLEAROPPOSITES);
		ProcessJoystick(&p2, 1, 0, 1, 2, 3, INPUT_4WAY | INPUT_CLEAROPPOSITES);
		DrvJoy1[12] = (p1 >> 0) & 1; DrvJoy1[3]  = (p1 >> 1) & 1; DrvJoy1[14] = (p1 >> 2) & 1; DrvJoy1[9]  = (p1 >> 3) & 1;
		DrvJoy1[13] = (p2 >> 0) & 1; DrvJoy1[10] = (p2 >> 1) & 1; DrvJoy1[8]  = (p2 >> 2) & 1; DrvJoy1[15] = (p2 >> 3) & 1;
	}

	for (INT32 i = 0; i < 16; i++) {
		if (DrvJoy1[i]) DrvInputs[0] &= ~(1 << i);
	}

	for (INT32 i = 0; i < 7; i++) {
		if (DrvJoy2[i]) DrvInputs[1] &= ~(1 << i);
	}

	if (is_qb3) {
		DrvInputs[0] = (DrvInputs[0] & ~0x1200) | ((DrvDips[1] << 8) & 0x1200);
	}

	// coin: falling edge sets the coin detected flag
	if (DrvJoy2[7] && !coin_last) coin_detected = 1;
	coin_last = DrvJoy2[7];

	switch (input_type)
	{
		case INP_SPEEDFRK:
		{
			INT32 delta = ((INT32)ProcessAnalog(DrvAnalog[0], 0, INPUT_DEADZONE, 0x00, 0xff) - 0x80) / 32;
			if (DrvJoy3[4]) delta = -2;
			if (DrvJoy3[5]) delta = 2;
			if (delta > 3) delta = 3;
			if (delta < -3) delta = -3;
			wheel_delta = delta;

			for (INT32 i = 0; i < 4; i++)
				if (DrvJoy3[i]) gear = i;
		}
		break;

		case INP_SUNDANCE:
			sundance_pad[0] = sundance_pad[1] = 0;
			for (INT32 i = 0; i < 9; i++) {
				sundance_pad[0] |= (DrvJoy4[i] & 1) << i;
				sundance_pad[1] |= (DrvJoy5[i] & 1) << i;
			}
		break;

		case INP_TAILG:
			analog_x = 0x200 + (ProcessAnalog(DrvAnalog[0], 0, INPUT_DEADZONE, 0x00, 0xff) * 0xc00) / 0xff;
			analog_y = 0x200 + (ProcessAnalog(DrvAnalog[1], 0, INPUT_DEADZONE, 0x00, 0xff) * 0xc00) / 0xff;
			if (DrvJoy3[0]) analog_y = 0x200;
			if (DrvJoy3[1]) analog_y = 0xe00;
			if (DrvJoy3[2]) analog_x = 0x200;
			if (DrvJoy3[3]) analog_x = 0xe00;
		break;

		case INP_BOXINGB:
			BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
			BurnTrackballFrame(0, DrvAnalog[0], 0, 0x01, 0x07);
			BurnTrackballUDLR(0, 0, 0, DrvJoy3[2], DrvJoy3[3], 4);
			BurnTrackballUpdate(0);
			boxingb_dial = BurnTrackballRead(0, 0);
		break;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	ccpuNewFrame();
	if (has_z80) ZetNewFrame();

	DrvMakeInputs();

	vector_reset();
	lastx = lasty = 0x7fff; // force a beam move at the start of the list

	INT32 nInterleave = (has_z80) ? 256 : 32;
	INT32 nCyclesTotal[2] = { (INT32)((INT64)nCcpuClock * 100 / nBurnFPS), (INT32)((INT64)nZ80Clock * 100 / nBurnFPS) };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	ccpuOpen(0);
	if (has_z80) ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, ccpu);
		if (has_z80) CPU_RUN(1, Zet);
	}

	if (has_z80) ZetClose();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	// vblank: release FRM wait, update watchdog
	frame_counter++;
	ccpuWatchdogTrigger();

	ccpuClose();

	if (pBurnSoundOut) {
		if (has_z80) {
			AY8910Render(pBurnSoundOut, nBurnSoundLen);
			if (last_portb_write & 4) // global mute
				memset (pBurnSoundOut, 0, nBurnSoundLen * 2 * sizeof(INT16));
		} else if (has_samples) {
			BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
		} else {
			BurnSoundClear();
		}
	}

	if (pBurnDraw) {
		DrvDraw();
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

		ccpuScan(nAction);

		if (has_z80) {
			ZetScan(nAction);
			AY8910Scan(nAction, pnMin);
		}

		if (has_samples) {
			BurnSampleScan(nAction, pnMin);
		}

		if (input_type == INP_BOXINGB) {
			BurnTrackballScan();
		}

		BurnRandomScan(nAction);

		SCAN_VAR(outlatch);
		SCAN_VAR(coin_detected);
		SCAN_VAR(coin_last);
		SCAN_VAR(mux_select);
		SCAN_VAR(vector_color);
		SCAN_VAR(lastx);
		SCAN_VAR(lasty);
		SCAN_VAR(gear);
		SCAN_VAR(nExtraCycles);
		SCAN_VAR(frame_counter);
		SCAN_VAR(qb3_lastx);
		SCAN_VAR(qb3_lasty);
		SCAN_VAR(qb3_bank);

		SCAN_VAR(current_shift);
		SCAN_VAR(last_shift);
		SCAN_VAR(last_shift2);
		SCAN_VAR(current_pitch);
		SCAN_VAR(last_frame);
		SCAN_VAR(target_volume);
		SCAN_VAR(current_volume);
		SCAN_VAR(sound_fifo);
		SCAN_VAR(sound_fifo_in);
		SCAN_VAR(sound_fifo_out);
		SCAN_VAR(last_portb_write);

		vector_scan(nAction);
	}

	if (nAction & ACB_WRITE) {
		ccpuMapData(DrvDataRAM + qb3_bank * 0x100);
	}

	return 0;
}

// ---------------------------------------------------------------------------
// Machine configurations

//                                         prg            jmi color          sound         input         mux  visible     flipx overlay
static const cinemat_config spacewar_cfg = { CCPU_PRG_4K,  0, COLOR_BW,      SND_SPACEWAR, INP_NORMAL,   0, 1024, 768,  0, NULL };
static const cinemat_config barrier_cfg  = { CCPU_PRG_4K,  1, COLOR_BW,      SND_BARRIER,  INP_BARRIER,  0, 1024, 768,  0, &barrier_overlay };
static const cinemat_config speedfrk_cfg = { CCPU_PRG_8K,  0, COLOR_BW,      SND_SPEEDFRK, INP_SPEEDFRK, 0, 1024, 768,  0, NULL };
static const cinemat_config starhawk_cfg = { CCPU_PRG_4K,  1, COLOR_BW,      SND_STARHAWK, INP_NORMAL,   0, 1024, 768,  0, NULL };
static const cinemat_config sundance_cfg = { CCPU_PRG_8K,  1, COLOR_16LEVEL, SND_SUNDANCE, INP_SUNDANCE, 0, 1024, 768,  0, &sundance_overlay };
static const cinemat_config tailg_cfg    = { CCPU_PRG_8K,  0, COLOR_BW,      SND_TAILG,    INP_TAILG,    1, 1024, 768,  0, &tailg_overlay };
static const cinemat_config warrior_cfg  = { CCPU_PRG_8K,  1, COLOR_BW,      SND_WARRIOR,  INP_NORMAL,   0, 1024, 768,  0, NULL };
static const cinemat_config armora_cfg   = { CCPU_PRG_16K, 1, COLOR_BW,      SND_ARMORA,   INP_NORMAL,   0, 1024, 768,  0, NULL };
static const cinemat_config ripoff_cfg   = { CCPU_PRG_8K,  1, COLOR_BW,      SND_RIPOFF,   INP_NORMAL,   0, 1024, 768,  0, NULL };
static const cinemat_config starcas_cfg  = { CCPU_PRG_8K,  1, COLOR_BW,      SND_STARCAS,  INP_NORMAL,   0, 1024, 768,  0, &starcas_overlay };
static const cinemat_config solarq_cfg   = { CCPU_PRG_16K, 1, COLOR_64LEVEL, SND_SOLARQ,   INP_NORMAL,   0, 1024, 768,  1, &solarq_overlay };
static const cinemat_config boxingb_cfg  = { CCPU_PRG_32K, 1, COLOR_RGB,     SND_BOXINGB,  INP_BOXINGB,  1, 1025, 789,  0, NULL };
static const cinemat_config wotw_cfg     = { CCPU_PRG_16K, 1, COLOR_BW,      SND_WOTW,     INP_NORMAL,   0, 1121, 768,  0, &wotw_overlay };
static const cinemat_config wotwc_cfg    = { CCPU_PRG_16K, 1, COLOR_RGB,     SND_WOTW,     INP_NORMAL,   0, 1024, 768,  0, NULL };
static const cinemat_config demon_cfg    = { CCPU_PRG_16K, 1, COLOR_BW,      SND_DEMON,    INP_NORMAL,   0, 1025, 806,  0, &demon_overlay };
static const cinemat_config qb3_cfg      = { CCPU_PRG_32K, 1, COLOR_QB3,     SND_DEMON,    INP_QB3,      0, 1121, 781,  0, NULL };

static INT32 SpacewarInit() { return CinematInit(&spacewar_cfg); }
static INT32 BarrierInit()  { return CinematInit(&barrier_cfg); }
static INT32 SpeedfrkInit() { return CinematInit(&speedfrk_cfg); }
static INT32 StarhawkInit() { return CinematInit(&starhawk_cfg); }
static INT32 SundanceInit() { return CinematInit(&sundance_cfg); }
static INT32 TailgInit()    { return CinematInit(&tailg_cfg); }
static INT32 WarriorInit()  { return CinematInit(&warrior_cfg); }
static INT32 ArmoraInit()   { return CinematInit(&armora_cfg); }
static INT32 RipoffInit()   { return CinematInit(&ripoff_cfg); }
static INT32 StarcasInit()  { return CinematInit(&starcas_cfg); }
static INT32 SolarqInit()   { return CinematInit(&solarq_cfg); }
static INT32 BoxingbInit()  { return CinematInit(&boxingb_cfg); }
static INT32 WotwInit()     { return CinematInit(&wotw_cfg); }
static INT32 WotwcInit()    { return CinematInit(&wotwc_cfg); }
static INT32 DemonInit()    { return CinematInit(&demon_cfg); }
static INT32 Qb3Init()      { return CinematInit(&qb3_cfg); }

// ---------------------------------------------------------------------------
// Samples

static struct BurnSampleInfo spacewarSampleDesc[] = {
	{ "explode1",	SAMPLE_NOLOOP },
	{ "fire1",		SAMPLE_NOLOOP },
	{ "idle",		SAMPLE_NOLOOP },
	{ "thrust1",	SAMPLE_NOLOOP },
	{ "thrust2",	SAMPLE_NOLOOP },
	{ "pop",		SAMPLE_NOLOOP },
	{ "explode2",	SAMPLE_NOLOOP },
	{ "fire2",		SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(spacewar)
STD_SAMPLE_FN(spacewar)

static struct BurnSampleInfo barrierSampleDesc[] = {
	{ "playrdie",	SAMPLE_NOLOOP },
	{ "playmove",	SAMPLE_NOLOOP },
	{ "enemmove",	SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(barrier)
STD_SAMPLE_FN(barrier)

static struct BurnSampleInfo speedfrkSampleDesc[] = {
	{ "offroad",	SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(speedfrk)
STD_SAMPLE_FN(speedfrk)

static struct BurnSampleInfo starhawkSampleDesc[] = {
	{ "explode",	SAMPLE_NOLOOP },
	{ "rlaser",		SAMPLE_NOLOOP },
	{ "llaser",		SAMPLE_NOLOOP },
	{ "k",			SAMPLE_NOLOOP },
	{ "master",		SAMPLE_NOLOOP },
	{ "kexit",		SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(starhawk)
STD_SAMPLE_FN(starhawk)

static struct BurnSampleInfo sundanceSampleDesc[] = {
	{ "bong",		SAMPLE_NOLOOP },
	{ "whoosh",		SAMPLE_NOLOOP },
	{ "explsion",	SAMPLE_NOLOOP },
	{ "ping1",		SAMPLE_NOLOOP },
	{ "ping2",		SAMPLE_NOLOOP },
	{ "hatch",		SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(sundance)
STD_SAMPLE_FN(sundance)

static struct BurnSampleInfo tailgSampleDesc[] = {
	{ "sexplode",	SAMPLE_NOLOOP },
	{ "thrust1",	SAMPLE_NOLOOP },
	{ "slaser",		SAMPLE_NOLOOP },
	{ "shield",		SAMPLE_NOLOOP },
	{ "bounce",		SAMPLE_NOLOOP },
	{ "hypersp",	SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(tailg)
STD_SAMPLE_FN(tailg)

static struct BurnSampleInfo warriorSampleDesc[] = {
	{ "bgmhum1",	SAMPLE_NOLOOP },
	{ "bgmhum2",	SAMPLE_NOLOOP },
	{ "killed",		SAMPLE_NOLOOP },
	{ "fall",		SAMPLE_NOLOOP },
	{ "appear",		SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(warrior)
STD_SAMPLE_FN(warrior)

static struct BurnSampleInfo armoraSampleDesc[] = {
	{ "loexp",		SAMPLE_NOLOOP },
	{ "jeepfire",	SAMPLE_NOLOOP },
	{ "hiexp",		SAMPLE_NOLOOP },
	{ "tankfire",	SAMPLE_NOLOOP },
	{ "tankeng",	SAMPLE_NOLOOP },
	{ "beep",		SAMPLE_NOLOOP },
	{ "chopper",	SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(armora)
STD_SAMPLE_FN(armora)

static struct BurnSampleInfo ripoffSampleDesc[] = {
	{ "bonuslvl",	SAMPLE_NOLOOP },
	{ "eattack",	SAMPLE_NOLOOP },
	{ "shipfire",	SAMPLE_NOLOOP },
	{ "efire",		SAMPLE_NOLOOP },
	{ "explosn",	SAMPLE_NOLOOP },
	{ "bg1",		SAMPLE_NOLOOP },
	{ "bg2",		SAMPLE_NOLOOP },
	{ "bg3",		SAMPLE_NOLOOP },
	{ "bg4",		SAMPLE_NOLOOP },
	{ "bg5",		SAMPLE_NOLOOP },
	{ "bg6",		SAMPLE_NOLOOP },
	{ "bg7",		SAMPLE_NOLOOP },
	{ "bg8",		SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(ripoff)
STD_SAMPLE_FN(ripoff)

static struct BurnSampleInfo starcasSampleDesc[] = {
	{ "cfire",		SAMPLE_NOLOOP },
	{ "shield",		SAMPLE_NOLOOP },
	{ "star",		SAMPLE_NOLOOP },
	{ "thrust",		SAMPLE_NOLOOP },
	{ "drone",		SAMPLE_NOLOOP },
	{ "lexplode",	SAMPLE_NOLOOP },
	{ "sexplode",	SAMPLE_NOLOOP },
	{ "pfire",		SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(starcas)
STD_SAMPLE_FN(starcas)

static struct BurnSampleInfo solarqSampleDesc[] = {
	{ "bigexpl",	SAMPLE_NOLOOP },
	{ "smexpl",		SAMPLE_NOLOOP },
	{ "lthrust",	SAMPLE_NOLOOP },
	{ "slaser",		SAMPLE_NOLOOP },
	{ "pickup",		SAMPLE_NOLOOP },
	{ "nuke2",		SAMPLE_NOLOOP },
	{ "nuke1",		SAMPLE_NOLOOP },
	{ "music",		SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(solarq)
STD_SAMPLE_FN(solarq)

static struct BurnSampleInfo boxingbSampleDesc[] = {
	{ "softexpl",	SAMPLE_NOLOOP },
	{ "loudexpl",	SAMPLE_NOLOOP },
	{ "chirp",		SAMPLE_NOLOOP },
	{ "eggcrack",	SAMPLE_NOLOOP },
	{ "bugpusha",	SAMPLE_NOLOOP },
	{ "bugpushb",	SAMPLE_NOLOOP },
	{ "bugdie",		SAMPLE_NOLOOP },
	{ "beetle",		SAMPLE_NOLOOP },
	{ "music",		SAMPLE_NOLOOP },
	{ "cannon",		SAMPLE_NOLOOP },
	{ "bounce",		SAMPLE_NOLOOP },
	{ "bell",		SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(boxingb)
STD_SAMPLE_FN(boxingb)

// wotw uses the same sample names as star castle, in its own set
static struct BurnSampleInfo wotwSampleDesc[] = {
	{ "cfire",		SAMPLE_NOLOOP },
	{ "shield",		SAMPLE_NOLOOP },
	{ "star",		SAMPLE_NOLOOP },
	{ "thrust",		SAMPLE_NOLOOP },
	{ "drone",		SAMPLE_NOLOOP },
	{ "lexplode",	SAMPLE_NOLOOP },
	{ "sexplode",	SAMPLE_NOLOOP },
	{ "pfire",		SAMPLE_NOLOOP },
	{ "",			0 }
};

STD_SAMPLE_PICK(wotw)
STD_SAMPLE_FN(wotw)

// ---------------------------------------------------------------------------
// ROM definitions

#define CCPU_PROMS \
	{ "prom.f14",	0x0100, 0x9edbf536, 3 | BRF_OPT }, \
	{ "prom.e14",	0x0020, 0x29dbfb87, 3 | BRF_OPT }, \
	{ "prom.d14",	0x0020, 0x9a05afbf, 3 | BRF_OPT }, \
	{ "prom.c14",	0x0020, 0x07492cda, 3 | BRF_OPT }, \
	{ "prom.j14",	0x0020, 0xa481ca71, 3 | BRF_OPT }, \
	{ "prom.e8",	0x0020, 0x791ec9e1, 3 | BRF_OPT },


// Space Wars

static struct BurnRomInfo spacewarRomDesc[] = {
	{ "spacewar.1l",	0x0800, 0xedf0fd53, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "spacewar.2r",	0x0800, 0x4f21328b, 1 | BRF_PRG | BRF_ESS }, //  1

	CCPU_PROMS
};

STD_ROM_PICK(spacewar)
STD_ROM_FN(spacewar)

struct BurnDriver BurnDrvSpacewar = {
	"spacewar", NULL, NULL, "spacewar", "1977",
	"Space Wars\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, spacewarRomInfo, spacewarRomName, NULL, NULL, spacewarSampleInfo, spacewarSampleName, SpacewarInputInfo, SpacewarDIPInfo,
	SpacewarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Space Ship

static struct BurnRomInfo spaceshpRomDesc[] = {
	{ "pr08.61",	0x0400, 0x556c4ff4, 4 | BRF_PRG | BRF_ESS }, //  0 CCPU Code (nibbles)
	{ "pr07.63",	0x0400, 0xba7747d1, 4 | BRF_PRG | BRF_ESS }, //  1
	{ "pr04.83",	0x0400, 0x19966799, 4 | BRF_PRG | BRF_ESS }, //  2
	{ "pr03.85",	0x0400, 0xd6557503, 4 | BRF_PRG | BRF_ESS }, //  3
	{ "pr10.62",	0x0400, 0x3ee163f9, 4 | BRF_PRG | BRF_ESS }, //  4
	{ "pr09.64",	0x0400, 0x7946086c, 4 | BRF_PRG | BRF_ESS }, //  5
	{ "pr06.84",	0x0400, 0xf19c8eb0, 4 | BRF_PRG | BRF_ESS }, //  6
	{ "pr05.86",	0x0400, 0x3dbc6360, 4 | BRF_PRG | BRF_ESS }, //  7

	{ "pr13.139",	0x0100, 0x9edbf536, 3 | BRF_OPT },           //  8 CCPU PROMs
	{ "pr17.138",	0x0020, 0x29dbfb87, 3 | BRF_OPT },           //  9
	{ "pr18.137",	0x0020, 0x98b7bd46, 3 | BRF_OPT },           // 10
	{ "pr19.136",	0x0020, 0x07492cda, 3 | BRF_OPT },           // 11
	{ "pr21.143",	0x0020, 0xa481ca71, 3 | BRF_OPT },           // 12
	{ "pr20.72",	0x0020, 0x791ec9e1, 3 | BRF_OPT },           // 13
};

STD_ROM_PICK(spaceshp)
STD_ROM_FN(spaceshp)

struct BurnDriver BurnDrvSpaceshp = {
	"spaceshp", "spacewar", NULL, "spacewar", "1978",
	"Space Ship\0", NULL, "Cinematronics (Sega license)", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, spaceshpRomInfo, spaceshpRomName, NULL, NULL, spacewarSampleInfo, spacewarSampleName, SpaceshpInputInfo, SpaceshpDIPInfo,
	SpacewarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Barrier

static struct BurnRomInfo barrierRomDesc[] = {
	{ "barrier.t7",	0x0800, 0x7c3d68c8, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "barrier.p7",	0x0800, 0xaec142b5, 1 | BRF_PRG | BRF_ESS }, //  1

	CCPU_PROMS
};

STD_ROM_PICK(barrier)
STD_ROM_FN(barrier)

struct BurnDriver BurnDrvBarrier = {
	"barrier", NULL, NULL, "barrier", "1979",
	"Barrier\0", NULL, "Cinematronics (Vectorbeam license)", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_VECTOR, 0,
	NULL, barrierRomInfo, barrierRomName, NULL, NULL, barrierSampleInfo, barrierSampleName, BarrierInputInfo, BarrierDIPInfo,
	BarrierInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	768, 1024, 3, 4
};


// Speed Freak

static struct BurnRomInfo speedfrkRomDesc[] = {
	{ "speedfrk.t7",	0x0800, 0x3552c03f, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "speedfrk.p7",	0x0800, 0x4b90cdec, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "speedfrk.u7",	0x0800, 0x616c7cf9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "speedfrk.r7",	0x0800, 0xfbe90d63, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(speedfrk)
STD_ROM_FN(speedfrk)

struct BurnDriver BurnDrvSpeedfrk = {
	"speedfrk", NULL, NULL, "speedfrk", "1979",
	"Speed Freak\0", "Engine sound not emulated", "Vectorbeam", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_VECTOR, 0,
	NULL, speedfrkRomInfo, speedfrkRomName, NULL, NULL, speedfrkSampleInfo, speedfrkSampleName, SpeedfrkInputInfo, SpeedfrkDIPInfo,
	SpeedfrkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Starhawk

static struct BurnRomInfo starhawkRomDesc[] = {
	{ "u7",			0x0800, 0x376e6c5c, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "r7",			0x0800, 0xbb71144f, 1 | BRF_PRG | BRF_ESS }, //  1

	CCPU_PROMS
};

STD_ROM_PICK(starhawk)
STD_ROM_FN(starhawk)

struct BurnDriver BurnDrvStarhawk = {
	"starhawk", NULL, NULL, "starhawk", "1979",
	"Starhawk\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, starhawkRomInfo, starhawkRomName, NULL, NULL, starhawkSampleInfo, starhawkSampleName, StarhawkInputInfo, StarhawkDIPInfo,
	StarhawkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Sundance

static struct BurnRomInfo sundanceRomDesc[] = {
	{ "sundance.t7",	0x0800, 0xd5b9cb19, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "sundance.p7",	0x0800, 0x445c4f20, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sundance.u7",	0x0800, 0x67887d48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sundance.r7",	0x0800, 0x10b77ebd, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(sundance)
STD_ROM_FN(sundance)

struct BurnDriver BurnDrvSundance = {
	"sundance", NULL, NULL, "sundance", "1979",
	"Sundance\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_VECTOR, 0,
	NULL, sundanceRomInfo, sundanceRomName, NULL, NULL, sundanceSampleInfo, sundanceSampleName, SundanceInputInfo, SundanceDIPInfo,
	SundanceInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10 * 256,
	768, 1024, 3, 4
};


// Tailgunner

static struct BurnRomInfo tailgRomDesc[] = {
	{ "tgunner.t70",	0x0800, 0x21ec9a04, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "tgunner.p70",	0x0800, 0x8d7410b3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tgunner.t71",	0x0800, 0x2c954ab6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tgunner.p71",	0x0800, 0x8e2c8494, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(tailg)
STD_ROM_FN(tailg)

struct BurnDriver BurnDrvTailg = {
	"tailg", NULL, NULL, "tailg", "1979",
	"Tailgunner\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, tailgRomInfo, tailgRomName, NULL, NULL, tailgSampleInfo, tailgSampleName, TailgInputInfo, TailgDIPInfo,
	TailgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Sky Fire (bootleg of Tailgunner)
// found on a set of 2 PCBs manufactured by Microhard, title rom area 0-ed out

static struct BurnRomInfo skyfireRomDesc[] = {
	{ "rom3-x1-2.3t",	0x0400, 0x211182e2, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "x9-1.3t",		0x0400, 0x99bd1c6b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom4-y1-2.4t",	0x0400, 0xe915fae5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "y9-1.4t",		0x0400, 0xe22adbd2, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rom1-v1-2.1t",	0x0400, 0x9850ba5e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "v9-1.1t",		0x0400, 0x51509bc6, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rom2-w1-2.2t",	0x0400, 0x85bd4353, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "w9-1.2t",		0x0400, 0xb5f45d46, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "f14",			0x0100, 0x025996b1, 3 | BRF_OPT },           //  8 CCPU PROMs
	{ "e14",			0x0020, 0x29dbfb87, 3 | BRF_OPT },           //  9
	{ "d14",			0x0020, 0x9a05afbf, 3 | BRF_OPT },           // 10
	{ "c14",			0x0020, 0x07492cda, 3 | BRF_OPT },           // 11
	{ "j14",			0x0020, 0xa481ca71, 3 | BRF_OPT },           // 12
	{ "e8",				0x0020, 0x791ec9e1, 3 | BRF_OPT },           // 13
};

STD_ROM_PICK(skyfire)
STD_ROM_FN(skyfire)

struct BurnDriver BurnDrvSkyfire = {
	"skyfire", "tailg", NULL, "tailg", "1979",
	"Sky Fire\0", NULL, "bootleg (Microhard)", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 1, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, skyfireRomInfo, skyfireRomName, NULL, NULL, tailgSampleInfo, tailgSampleName, TailgInputInfo, TailgDIPInfo,
	TailgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Warrior

static struct BurnRomInfo warriorRomDesc[] = {
	{ "warrior.t7",	0x0800, 0xac3646f9, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "warrior.p7",	0x0800, 0x517d3021, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "warrior.u7",	0x0800, 0x2e39340f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "warrior.r7",	0x0800, 0x8e91b502, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(warrior)
STD_ROM_FN(warrior)

struct BurnDriver BurnDrvWarrior = {
	"warrior", NULL, NULL, "warrior", "1979",
	"Warrior\0", NULL, "Vectorbeam", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_VSFIGHT | GBF_VECTOR, 0,
	NULL, warriorRomInfo, warriorRomName, NULL, NULL, warriorSampleInfo, warriorSampleName, WarriorInputInfo, WarriorDIPInfo,
	WarriorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Armor Attack

static struct BurnRomInfo armoraRomDesc[] = {
	{ "ar414le.t6",	0x1000, 0xd7e71f84, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "ar414lo.p6",	0x1000, 0xdf1c2370, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ar414ue.u6",	0x1000, 0xb0276118, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ar414uo.r6",	0x1000, 0x229d779f, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(armora)
STD_ROM_FN(armora)

struct BurnDriver BurnDrvArmora = {
	"armora", NULL, NULL, "armora", "1980",
	"Armor Attack\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, armoraRomInfo, armoraRomName, NULL, NULL, armoraSampleInfo, armoraSampleName, ArmoraInputInfo, ArmoraDIPInfo,
	ArmoraInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Armor Attack (prototype)

static struct BurnRomInfo armorapRomDesc[] = {
	{ "ar414le.t6",	0x1000, 0xd7e71f84, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "ar414lo.p6",	0x1000, 0xdf1c2370, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "armorp.u7",	0x1000, 0x4a86bd8a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "armorp.r7",	0x1000, 0xd2dd4eae, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(armorap)
STD_ROM_FN(armorap)

struct BurnDriver BurnDrvArmorap = {
	"armorap", "armora", NULL, "armora", "1980",
	"Armor Attack (prototype)\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, armorapRomInfo, armorapRomName, NULL, NULL, armoraSampleInfo, armoraSampleName, ArmoraInputInfo, ArmoraDIPInfo,
	ArmoraInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Armor Attack (Rock-Ola)

static struct BurnRomInfo armorarRomDesc[] = {
	{ "armorr.t7",	0x0800, 0x256d1ed9, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "armorr.p7",	0x0800, 0xbf75c158, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "armorr.u7",	0x0800, 0xba68331d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "armorr.r7",	0x0800, 0xfa14c0b3, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(armorar)
STD_ROM_FN(armorar)

struct BurnDriver BurnDrvArmorar = {
	"armorar", "armora", NULL, "armora", "1980",
	"Armor Attack (Rock-Ola)\0", NULL, "Cinematronics (Rock-Ola license)", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, armorarRomInfo, armorarRomName, NULL, NULL, armoraSampleInfo, armoraSampleName, ArmoraInputInfo, ArmoraDIPInfo,
	ArmoraInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Rip Off

static struct BurnRomInfo ripoffRomDesc[] = {
	{ "ripoff.t7",	0x0800, 0x40c2c5b8, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "ripoff.p7",	0x0800, 0xa9208afb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ripoff.u7",	0x0800, 0x29c13701, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ripoff.r7",	0x0800, 0x150bd4c8, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(ripoff)
STD_ROM_FN(ripoff)

struct BurnDriver BurnDrvRipoff = {
	"ripoff", NULL, NULL, "ripoff", "1980",
	"Rip Off\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, ripoffRomInfo, ripoffRomName, NULL, NULL, ripoffSampleInfo, ripoffSampleName, ArmoraInputInfo, RipoffDIPInfo,
	RipoffInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Star Castle (version 3)

static struct BurnRomInfo starcasRomDesc[] = {
	{ "starcas3.t7",	0x0800, 0xb5838b5d, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "starcas3.p7",	0x0800, 0xf6bc2f4d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "starcas3.u7",	0x0800, 0x188cd97c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "starcas3.r7",	0x0800, 0xc367b69d, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(starcas)
STD_ROM_FN(starcas)

struct BurnDriver BurnDrvStarcas = {
	"starcas", NULL, NULL, "starcas", "1980",
	"Star Castle (version 3)\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, starcasRomInfo, starcasRomName, NULL, NULL, starcasSampleInfo, starcasSampleName, StarcasInputInfo, StarcasDIPInfo,
	StarcasInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Star Castle (older)

static struct BurnRomInfo starcas1RomDesc[] = {
	{ "starcast.t7",	0x0800, 0x65d0a225, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "starcast.p7",	0x0800, 0xd8f58d9a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "starcast.u7",	0x0800, 0xd4f35b82, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "starcast.r7",	0x0800, 0x9fd3de54, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(starcas1)
STD_ROM_FN(starcas1)

struct BurnDriver BurnDrvStarcas1 = {
	"starcas1", "starcas", NULL, "starcas", "1980",
	"Star Castle (older)\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, starcas1RomInfo, starcas1RomName, NULL, NULL, starcasSampleInfo, starcasSampleName, StarcasInputInfo, StarcasDIPInfo,
	StarcasInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Star Castle (cocktail)

static struct BurnRomInfo starcascRomDesc[] = {
	{ "ctsc926_ue_3265.t7",	0x0800, 0xc140a1bb, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "ctsc926_le_deac.p7",	0x0800, 0x8a074f6c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ctsc926_u0_48e7.u7",	0x0800, 0xed136f11, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ctsc926_l0_f707.r7",	0x0800, 0x1930b1fb, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(starcasc)
STD_ROM_FN(starcasc)

struct BurnDriver BurnDrvStarcasc = {
	"starcasc", "starcas", NULL, "starcas", "1980",
	"Star Castle (cocktail)\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, starcascRomInfo, starcascRomName, NULL, NULL, starcasSampleInfo, starcasSampleName, StarcascInputInfo, StarcascDIPInfo,
	StarcasInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Star Castle (prototype)

static struct BurnRomInfo starcaspRomDesc[] = {
	{ "starcasp.t7",	0x0800, 0xd2c551a2, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "starcasp.p7",	0x0800, 0xbaa4e422, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "starcasp.u7",	0x0800, 0x26941991, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "starcasp.r7",	0x0800, 0x5dd151e5, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(starcasp)
STD_ROM_FN(starcasp)

struct BurnDriver BurnDrvStarcasp = {
	"starcasp", "starcas", NULL, "starcas", "1980",
	"Star Castle (prototype)\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, starcaspRomInfo, starcaspRomName, NULL, NULL, starcasSampleInfo, starcasSampleName, StarcasInputInfo, StarcasDIPInfo,
	StarcasInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Star Castle (Mottoeis)

static struct BurnRomInfo starcaseRomDesc[] = {
	{ "starcast.t7",	0x0800, 0x65d0a225, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "starcast.p7",	0x0800, 0xd8f58d9a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "starcast.u7",	0x0800, 0xd4f35b82, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mottoeis.r7",	0x0800, 0xa2c1ed52, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(starcase)
STD_ROM_FN(starcase)

struct BurnDriver BurnDrvStarcase = {
	"starcase", "starcas", NULL, "starcas", "1980",
	"Star Castle (Mottoeis)\0", NULL, "Cinematronics (Mottoeis license)", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, starcaseRomInfo, starcaseRomName, NULL, NULL, starcasSampleInfo, starcasSampleName, StarcasInputInfo, StarcasDIPInfo,
	StarcasInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Stellar Castle (Elettronolo)

static struct BurnRomInfo stellcasRomDesc[] = {
	{ "starcast.t7",	0x0800, 0x65d0a225, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "starcast.p7",	0x0800, 0xd8f58d9a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "elttron.u7",		0x0800, 0xd5b44050, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "elttron.r7",		0x0800, 0x6f1f261e, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(stellcas)
STD_ROM_FN(stellcas)

struct BurnDriver BurnDrvStellcas = {
	"stellcas", "starcas", NULL, "starcas", "1980",
	"Stellar Castle (Elettronolo)\0", NULL, "bootleg (Elettronolo)", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, stellcasRomInfo, stellcasRomName, NULL, NULL, starcasSampleInfo, starcasSampleName, StarcasInputInfo, StarcasDIPInfo,
	StarcasInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Space Fortress (Zaccaria)

static struct BurnRomInfo spaceftrRomDesc[] = {
	{ "fortrest7.7t",	0x0800, 0x65d0a225, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "fortresp7.7p",	0x0800, 0xd8f58d9a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fortresu7.7u",	0x0800, 0x13b0287c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fortresr7.7r",	0x0800, 0xa2c1ed52, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "prom.f14",		0x0100, 0x9edbf536, 3 | BRF_OPT },           //  4 CCPU PROMs
	{ "prom.e14",		0x0020, 0x29dbfb87, 3 | BRF_OPT },           //  5
	{ "prom.d14",		0x0020, 0x9a05afbf, 3 | BRF_OPT },           //  6
	{ "prom.c14",		0x0020, 0x07492cda, 3 | BRF_OPT },           //  7
	{ "prom.j14",		0x0020, 0xa481ca71, 3 | BRF_OPT },           //  8
	{ "prom6331-1j.e8",	0x0020, 0x8c85e786, 3 | BRF_OPT },           //  9
};

STD_ROM_PICK(spaceftr)
STD_ROM_FN(spaceftr)

struct BurnDriver BurnDrvSpaceftr = {
	"spaceftr", "starcas", NULL, "starcas", "1981",
	"Space Fortress (Zaccaria)\0", NULL, "Cinematronics (Zaccaria license)", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, spaceftrRomInfo, spaceftrRomName, NULL, NULL, starcasSampleInfo, starcasSampleName, StarcasInputInfo, StarcasDIPInfo,
	StarcasInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// Solar Quest (rev 10 8 81)

static struct BurnRomInfo solarqRomDesc[] = {
	{ "sq-2_le.6t",	0x1000, 0x1f3c5333, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "sq-2_lo.6p",	0x1000, 0xd6c16bcc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sq-2_ue.6u",	0x1000, 0xa5970e5c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sq-2_uo.6r",	0x1000, 0xb763fff2, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(solarq)
STD_ROM_FN(solarq)

struct BurnDriver BurnDrvSolarq = {
	"solarq", NULL, NULL, "solarq", "1981",
	"Solar Quest (rev 10 8 81)\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, solarqRomInfo, solarqRomName, NULL, NULL, solarqSampleInfo, solarqSampleName, SolarqInputInfo, SolarqDIPInfo,
	SolarqInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40 * 256,
	1024, 768, 4, 3
};


// Boxing Bugs

static struct BurnRomInfo boxingbRomDesc[] = {
	{ "u1a",		0x1000, 0xd3115b0f, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "u1b",		0x1000, 0x3a44268d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u2a",		0x1000, 0xc97a9cbb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "u2b",		0x1000, 0x98d34ff5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "u3a",		0x1000, 0x5bb3269b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "u3b",		0x1000, 0x85bf83ad, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "u4a",		0x1000, 0x25b51799, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "u4b",		0x1000, 0x7f41de6a, 1 | BRF_PRG | BRF_ESS }, //  7

	CCPU_PROMS
};

STD_ROM_PICK(boxingb)
STD_ROM_FN(boxingb)

struct BurnDriver BurnDrvBoxingb = {
	"boxingb", NULL, NULL, "boxingb", "1981",
	"Boxing Bugs\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_VECTOR, 0,
	NULL, boxingbRomInfo, boxingbRomName, NULL, NULL, boxingbSampleInfo, boxingbSampleName, BoxingbInputInfo, BoxingbDIPInfo,
	BoxingbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000 * 256,
	1024, 768, 4, 3
};


// War of the Worlds

static struct BurnRomInfo wotwRomDesc[] = {
	{ "wow_le.t7",	0x1000, 0xb16440f9, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "wow_lo.p7",	0x1000, 0xbfdf4a5a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wow_ue.u7",	0x1000, 0x9b5cea48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wow_uo.r7",	0x1000, 0xc9d3c866, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(wotw)
STD_ROM_FN(wotw)

struct BurnDriver BurnDrvWotw = {
	"wotw", NULL, NULL, "wotw", "1981",
	"War of the Worlds\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, wotwRomInfo, wotwRomName, NULL, NULL, wotwSampleInfo, wotwSampleName, StarcasInputInfo, WotwDIPInfo,
	WotwInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// War of the Worlds (color)

static struct BurnRomInfo wotwcRomDesc[] = {
	{ "wow_le.t7",	0x1000, 0xb16440f9, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "wow_lo.p7",	0x1000, 0xbfdf4a5a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wow_ue.u7",	0x1000, 0x9b5cea48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wow_uo.r7",	0x1000, 0xc9d3c866, 1 | BRF_PRG | BRF_ESS }, //  3

	CCPU_PROMS
};

STD_ROM_PICK(wotwc)
STD_ROM_FN(wotwc)

struct BurnDriver BurnDrvWotwc = {
	"wotwc", "wotw", NULL, "wotw", "1981",
	"War of the Worlds (color)\0", NULL, "Cinematronics", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, wotwcRomInfo, wotwcRomName, NULL, NULL, wotwSampleInfo, wotwSampleName, StarcasInputInfo, WotwcDIPInfo,
	WotwcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000 * 256,
	1024, 768, 4, 3
};


// Demon

static struct BurnRomInfo demonRomDesc[] = {
	{ "demon.7t",	0x1000, 0x866596c1, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "demon.7p",	0x1000, 0x1109e2f1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "demon.7u",	0x1000, 0xd447a3c3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "demon.7r",	0x1000, 0x64b515f0, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "demon.snd",	0x1000, 0x1e2cc262, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	CCPU_PROMS
};

STD_ROM_PICK(demon)
STD_ROM_FN(demon)

struct BurnDriver BurnDrvDemon = {
	"demon", NULL, NULL, NULL, "1982",
	"Demon\0", NULL, "Rock-Ola", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, demonRomInfo, demonRomName, NULL, NULL, NULL, NULL, DemonInputInfo, DemonDIPInfo,
	DemonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02 * 256,
	1024, 768, 4, 3
};


// QB-3 (prototype)

static struct BurnRomInfo qb3RomDesc[] = {
	{ "qb3_le_t7.bin",		0x2000, 0xadaaee4c, 1 | BRF_PRG | BRF_ESS }, //  0 CCPU Code
	{ "qb3_lo_p7.bin",		0x2000, 0x72f6199f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qb3_ue_u7.bin",		0x2000, 0x050a996d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "qb3_uo_r7.bin",		0x2000, 0x33fa77a2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "qb3_snd_u12.bin",	0x1000, 0xf86663de, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "qb3_snd_u11.bin",	0x1000, 0x32ed58fc, 2 | BRF_PRG | BRF_ESS }, //  5

	CCPU_PROMS
};

STD_ROM_PICK(qb3)
STD_ROM_FN(qb3)

struct BurnDriver BurnDrvQb3 = {
	"qb3", NULL, NULL, NULL, "1982",
	"QB-3 (prototype)\0", "Imperfect graphics", "Rock-Ola", "Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_VECTOR, 0,
	NULL, qb3RomInfo, qb3RomName, NULL, NULL, NULL, NULL, Qb3InputInfo, Qb3DIPInfo,
	Qb3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100 * 256,
	1024, 768, 4, 3
};
