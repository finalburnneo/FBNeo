// FB Neo Nemesis driver module
// Based on MAME driver by Bryan McPhail w/additions by Hau, hap, and likely othes

// fixed:
// rf2: checkpoint tunnel
// rf2/konamigt: steering improvement
// salamander: music speed
// kittenk/nyanpani: fix slot machine minigame
// hcrash: bad sounds, steering

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "k051649.h"
#include "k007232.h"
#include "k005289.h"
#include "vlm5030.h"
#include "burn_ym2151.h"
#include "flt_rc.h"
#include "ay8910.h"
#include "burn_shift.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *K005289ROM;
static UINT8 *DrvVLMROM;
static UINT8 *K007232ROM;
static UINT8 *Drv68KRAM0;
static UINT8 *Drv68KRAM1;
static UINT8 *Drv68KRAM2;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvColRAM0;
static UINT8 *DrvColRAM1;
static UINT8 *DrvCharRAM;
static UINT8 *DrvCharRAMExp;
static UINT8 *DrvScrollRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT16 *mcu_control;
static UINT8 *soundlatch;
static UINT8 *flipscreen;
static UINT8 *m68k_irq_enable;
static UINT8 *m68k_irq_enable2;
static UINT8 *m68k_irq_enable4;
static UINT8 *tilemap_flip_x;
static UINT8 *tilemap_flip_y;

static UINT16 *xscroll1;
static UINT16 *yscroll1;
static UINT16 *xscroll2;
static UINT16 *yscroll2;
static INT32 scanline_counter;
static UINT8 selected_ip;

static INT32 is_bubble_system = 0;

static INT32 watchdog;
static void (*palette_write)(INT32) = NULL;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvDips[4];
static UINT8 DrvReset;
static UINT16 DrvInputs[4];
static INT16 AnalogPort0 = 0;
static INT16 DrvDial1;
static INT16 last_dial;
static INT32 center_dial_timer;

static INT32 nCyclesExtra[2];

static INT32 ay8910_enable = 0;
static INT32 ym2151_enable = 0;
static INT32 ym3812_enable = 0;
static INT32 k005289_enable = 0;
static INT32 k007232_enable = 0;
static INT32 k051649_enable = 0;
static INT32 vlm5030_enable = 0;
static INT32 vlm5030_enable_ram = 0;
static INT32 rcflt_enable = 0;
static INT32 hcrash_mode = 0;
static INT32 bUseShifter = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static struct BurnInputInfo NemesisInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Nemesis)

static struct BurnInputInfo BubsysInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Bubsys)

static struct BurnInputInfo SalamandInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Salamand)

static struct BurnInputInfo LifefrcejInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Lifefrcej)

static struct BurnInputInfo TwinbeeInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Twinbee)

static struct BurnInputInfo GradiusInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Gradius)

static struct BurnInputInfo GwarriorInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Gwarrior)

static struct BurnInputInfo NyanpaniInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Nyanpani)

static struct BurnInputInfo BlkpnthrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Blkpnthr)

static struct BurnInputInfo CitybombInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Citybomb)

static struct BurnInputInfo KonamigtInputList[] = {
	{"P1 Coin",     	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"		},
	{"P1 Accelerator",	BIT_DIGITAL,	DrvJoy4 + 6,	"p1 fire 1"		},
	{"P1 Brake",    	BIT_DIGITAL,	DrvJoy4 + 5,	"p1 fire 2"		},
	{"P1 Gear Shift",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 3"		},

	A("P1 Wheel",   	BIT_ANALOG_REL, &AnalogPort0,	"p1 x-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"			},
};

STDINPUTINFO(Konamigt)

static struct BurnInputInfo HcrashInputList[] = {
	{"P1 Coin",     	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"		},
	{"P1 Start",    	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"		},
	{"P1 Accelerator",	BIT_DIGITAL,	DrvJoy4 + 6,	"p1 fire 1"		},
	{"P1 Jump",    		BIT_DIGITAL,	DrvJoy4 + 5,	"p1 fire 2"		},
	{"P1 Brake",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 fire 3"		},

	A("P1 Wheel",   	BIT_ANALOG_REL, &AnalogPort0,	"p1 x-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
};

STDINPUTINFO(Hcrash)

static struct BurnDIPInfo NemesisDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x5b, NULL					},
	{0x02, 0xff, 0xff, 0xff, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x00, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0xf0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "3"					},
	{0x01, 0x01, 0x03, 0x02, "4"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x04, 0x00, "Upright"				},
	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x18, "50k and every 100k"	},
	{0x01, 0x01, 0x18, 0x10, "30k"					},
	{0x01, 0x01, 0x18, 0x08, "50k"					},
	{0x01, 0x01, 0x18, 0x00, "100k"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x60, "Easy"					},
	{0x01, 0x01, 0x60, 0x40, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Hard"					},
	{0x01, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x02, 0x01, 0x01, 0x01, "Off"					},
//	{0x02, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Upright Controls"		},
	{0x02, 0x01, 0x02, 0x02, "Single"				},
	{0x02, 0x01, 0x02, 0x00, "Dual"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x04, 0x04, "Off"					},
	{0x02, 0x01, 0x04, 0x00, "On"					},

	{0,    0xfe, 0,       3, "Color Settings"		},
	{0x03, 0x01, 0x03, 0x00, "Proper Colors (Dark)"	},
	{0x03, 0x01, 0x03, 0x01, "Light Colors"			},
	{0x03, 0x01, 0x03, 0x02, "MAMEUIFx Colors (Mid)"},
};

STDDIPINFO(Nemesis)

static struct BurnDIPInfo SalamandDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x42, NULL					},
	{0x02, 0xff, 0xff, 0x0e, NULL					},

	{0   , 0xfe, 0   ,   16, "Coinage"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Disabled"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "2"					},
	{0x01, 0x01, 0x03, 0x02, "3"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Coin Slot(s)"			},
	{0x01, 0x01, 0x04, 0x04, "1"					},
	{0x01, 0x01, 0x04, 0x00, "2"					},

	{0   , 0xfe, 0   ,    4, "Max Credit(s)"		},
	{0x01, 0x01, 0x18, 0x18, "1"					},
	{0x01, 0x01, 0x18, 0x10, "3"					},
	{0x01, 0x01, 0x18, 0x08, "5"					},
	{0x01, 0x01, 0x18, 0x00, "9"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x60, "Easy"					},
	{0x01, 0x01, 0x60, 0x40, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Hard"					},
	{0x01, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x02, 0x01, 0x02, 0x02, "Off"					},
//	{0x02, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x08, 0x08, "Off"					},
	{0x02, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x02, 0x01, 0x80, 0x00, "Upright"				},
	{0x02, 0x01, 0x80, 0x80, "Cocktail"				},
};

STDDIPINFO(Salamand)

static struct BurnDIPInfo LifefrcejDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x43, NULL					},
	{0x02, 0xff, 0xff, 0x0e, NULL					},

	{0   , 0xfe, 0   ,   16, "Coinage"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Disabled"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "3"					},
	{0x01, 0x01, 0x03, 0x02, "4"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "6"					},

	{0   , 0xfe, 0   ,    2, "Coin Slot(s)"			},
	{0x01, 0x01, 0x04, 0x04, "1"					},
	{0x01, 0x01, 0x04, 0x00, "2"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x18, "70k and every 200k"	},
	{0x01, 0x01, 0x18, 0x10, "100k and every 300k"	},
	{0x01, 0x01, 0x18, 0x08, "70k only"				},
	{0x01, 0x01, 0x18, 0x00, "100k only"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x60, "Easy"					},
	{0x01, 0x01, 0x60, 0x40, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Hard"					},
	{0x01, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x02, 0x01, 0x02, 0x02, "Off"					},
//	{0x02, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x08, 0x08, "Off"					},
	{0x02, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x02, 0x01, 0x80, 0x00, "Upright"				},
	{0x02, 0x01, 0x80, 0x80, "Cocktail"				},
};

STDDIPINFO(Lifefrcej)

static struct BurnDIPInfo TwinbeeDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x56, NULL					},
	{0x02, 0xff, 0xff, 0xfd, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x00, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0xf0, 0x00, "No Coin B"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "2"					},
	{0x01, 0x01, 0x03, 0x02, "3"					},
	{0x01, 0x01, 0x03, 0x01, "4"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x18, "20k 100k"				},
	{0x01, 0x01, 0x18, 0x10, "30k 120k"				},
	{0x01, 0x01, 0x18, 0x08, "40k 140k"				},
	{0x01, 0x01, 0x18, 0x00, "50k 160k"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x60, "Easy"					},
	{0x01, 0x01, 0x60, 0x40, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Hard"					},
	{0x01, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x02, 0x01, 0x01, 0x01, "Off"					},
//	{0x02, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Players"				},
	{0x02, 0x01, 0x02, 0x02, "1"					},
	{0x02, 0x01, 0x02, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x04, 0x04, "Off"					},
	{0x02, 0x01, 0x04, 0x00, "On"					},

	{0,    0xfe, 0,       3, "Color Settings"		},
	{0x03, 0x01, 0x03, 0x00, "Proper Colors (Dark)"	},
	{0x03, 0x01, 0x03, 0x01, "Light Colors"			},
	{0x03, 0x01, 0x03, 0x02, "MAMEUIFx Colors (Mid)"},
};

STDDIPINFO(Twinbee)

static struct BurnDIPInfo GradiusDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x53, NULL					},
	{0x02, 0xff, 0xff, 0xff, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x00, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0xf0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "3"					},
	{0x01, 0x01, 0x03, 0x02, "4"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},
	
	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x04, 0x00, "Upright"				},
	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x18, "20k and every 70k"	},
	{0x01, 0x01, 0x18, 0x10, "30k and every 80k"	},
	{0x01, 0x01, 0x18, 0x08, "20k only"				},
	{0x01, 0x01, 0x18, 0x00, "30k only"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x60, "Easy"					},
	{0x01, 0x01, 0x60, 0x40, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Hard"					},
	{0x01, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x02, 0x01, 0x01, 0x01, "Off"					},
//	{0x02, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Upright Controls"		},
	{0x02, 0x01, 0x02, 0x02, "Single"				},
	{0x02, 0x01, 0x02, 0x00, "Dual"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x04, 0x04, "Off"					},
	{0x02, 0x01, 0x04, 0x00, "On"					},

	{0,    0xfe, 0,       3, "Color Settings"		},
	{0x03, 0x01, 0x03, 0x00, "Proper Colors (Dark)"	},
	{0x03, 0x01, 0x03, 0x01, "Light Colors"			},
	{0x03, 0x01, 0x03, 0x02, "MAMEUIFx Colors (Mid)"},
};

STDDIPINFO(Gradius)

static struct BurnDIPInfo GwarriorDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x5d, NULL					},
	{0x02, 0xff, 0xff, 0xfd, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   16, "No Coin B"			},
	{0x00, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0xf0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "1"					},
	{0x01, 0x01, 0x03, 0x02, "2"					},
	{0x01, 0x01, 0x03, 0x01, "3"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x18, "30k 100k 200k 400k"	},
	{0x01, 0x01, 0x18, 0x10, "40k 120k 240k 480k"	},
	{0x01, 0x01, 0x18, 0x08, "50k 150k 300k 600k"	},
	{0x01, 0x01, 0x18, 0x00, "100k 200k 400k 800k"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x60, "Easy"					},
	{0x01, 0x01, 0x60, 0x40, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Hard"					},
	{0x01, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x02, 0x01, 0x01, 0x01, "Off"					},
//	{0x02, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Players"				},
	{0x02, 0x01, 0x02, 0x02, "1"					},
	{0x02, 0x01, 0x02, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x04, 0x04, "Off"					},
	{0x02, 0x01, 0x04, 0x00, "On"					},

	{0,    0xfe, 0,       3, "Color Settings"		},
	{0x03, 0x01, 0x03, 0x00, "Proper Colors (Dark)"	},
	{0x03, 0x01, 0x03, 0x01, "Light Colors"			},
	{0x03, 0x01, 0x03, 0x02, "MAMEUIFx Colors (Mid)"},
};

STDDIPINFO(Gwarrior)

static struct BurnDIPInfo NyanpaniDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x42, NULL					},
	{0x02, 0xff, 0xff, 0x8e, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   16, "No Coin B"			},
	{0x00, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0xf0, 0x00, "Invalid"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "2"					},
	{0x01, 0x01, 0x03, 0x02, "3"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x60, "Easy"					},
	{0x01, 0x01, 0x60, 0x40, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Hard"					},
	{0x01, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x02, 0x01, 0x02, 0x02, "Off"					},
//	{0x02, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x08, 0x08, "Off"					},
	{0x02, 0x01, 0x08, 0x00, "On"					},
};

STDDIPINFO(Nyanpani)

static struct BurnDIPInfo BlkpnthrDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x53, NULL					},
	{0x02, 0xff, 0xff, 0xa8, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x00, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0xf0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "3"					},
	{0x01, 0x01, 0x03, 0x02, "4"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x04, 0x00, "Upright"				},
	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x18, "50k 100k"				},
	{0x01, 0x01, 0x18, 0x10, "20k 50k"				},
	{0x01, 0x01, 0x18, 0x08, "30k 70k"				},
	{0x01, 0x01, 0x18, 0x00, "80k 150k"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x60, "Easy"					},
	{0x01, 0x01, 0x60, 0x40, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Hard"					},
	{0x01, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x08, 0x08, "Off"					},
	{0x02, 0x01, 0x08, 0x00, "On"					},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x02, 0x01, 0x20, 0x20, "Off"					},
//	{0x02, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Continue"				},
	{0x02, 0x01, 0xc0, 0xc0, "Off"					},
	{0x02, 0x01, 0xc0, 0x80, "2 Areas"				},
	{0x02, 0x01, 0xc0, 0x40, "3 Areas"				},
	{0x02, 0x01, 0xc0, 0x00, "4 Areas"				},
};

STDDIPINFO(Blkpnthr)

static struct BurnDIPInfo CitybombDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x53, NULL					},
	{0x02, 0xff, 0xff, 0x8c, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x00, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0xf0, 0x00, "Invalid"				},

//	{0   , 0xfe, 0   ,    2, "Cabinet"				},
//	{0x01, 0x01, 0x04, 0x00, "Upright"				},
//	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Qualify"				},
	{0x01, 0x01, 0x18, 0x18, "Long"					},
	{0x01, 0x01, 0x18, 0x10, "Normal"				},
	{0x01, 0x01, 0x18, 0x08, "Short"				},
	{0x01, 0x01, 0x18, 0x00, "Very Short"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x60, "Easy"					},
	{0x01, 0x01, 0x60, 0x40, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Hard"					},
	{0x01, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x02, 0x01, 0x02, 0x00, "Off"					},
//	{0x02, 0x01, 0x02, 0x02, "On"					},

	{0   , 0xfe, 0   ,    2, "Upright Controls"		},
	{0x02, 0x01, 0x04, 0x04, "Single"				},
	{0x02, 0x01, 0x04, 0x00, "Dual"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x08, 0x08, "Off"					},
	{0x02, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Device Type"			},
	{0x02, 0x01, 0x80, 0x00, "Handle (broken!)"		},
	{0x02, 0x01, 0x80, 0x80, "Joystick"				},
};

STDDIPINFO(Citybomb)

static struct BurnDIPInfo KonamigtDIPList[]=
{
	DIP_OFFSET(0x07)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x6f, NULL					},
	{0x02, 0xff, 0xff, 0xff, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x00, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0xf0, 0x00, "No Coin B"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x30, 0x30, "Easy"					},
	{0x01, 0x01, 0x30, 0x20, "Normal"				},
	{0x01, 0x01, 0x30, 0x10, "Hard"					},
	{0x01, 0x01, 0x30, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x02, 0x01, 0x01, 0x01, "Off"					},
//	{0x02, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x04, 0x04, "Off"					},
	{0x02, 0x01, 0x04, 0x00, "On"					},

	{0,    0xfe, 0,       3, "Color Settings"		},
	{0x03, 0x01, 0x03, 0x00, "Proper Colors (Dark)"	},
	{0x03, 0x01, 0x03, 0x01, "Light Colors"			},
	{0x03, 0x01, 0x03, 0x02, "MAMEUIFx Colors (Mid)"},
};

STDDIPINFO(Konamigt)

static struct BurnDIPInfo HcrashDIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x42, NULL					},
	{0x02, 0xff, 0xff, 0x0d, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x00, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0xf0, 0x00, "Invalid"				},

//	{0   , 0xfe, 0   ,    2, "Cabinet"				},
//	{0x01, 0x01, 0x03, 0x01, "Konami GT with brake"	},
//	{0x01, 0x01, 0x03, 0x02, "WEC Lemans"			}, // only WEC controls work

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x60, "Easy"					},
	{0x01, 0x01, 0x60, 0x40, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Hard"					},
	{0x01, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x02, 0x01, 0x01, 0x01, "Off"					},
//	{0x02, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Quantity of Initials"	},
	{0x02, 0x01, 0x02, 0x00, "3"					},
	{0x02, 0x01, 0x02, 0x02, "7"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x04, 0x04, "Off"					},
	{0x02, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Speed Unit"			},
	{0x02, 0x01, 0x08, 0x08, "km/h"					},
	{0x02, 0x01, 0x08, 0x00, "M.P.H."				},
};

STDDIPINFO(Hcrash)

static struct BurnDIPInfo BubsysDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x5b, NULL					},
	{0x02, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x00, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x00, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x00, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x00, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0xf0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "3"					},
	{0x01, 0x01, 0x03, 0x02, "4"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x04, 0x00, "Upright"				},
	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x18, "50k and every 100k"	},
	{0x01, 0x01, 0x18, 0x10, "30k"					},
	{0x01, 0x01, 0x18, 0x08, "50k"					},
	{0x01, 0x01, 0x18, 0x00, "100k"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x60, "Easy"					},
	{0x01, 0x01, 0x60, 0x40, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Hard"					},
	{0x01, 0x01, 0x60, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x02, 0x01, 0x01, 0x01, "Off"					},
	{0x02, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Upright Controls"		},
	{0x02, 0x01, 0x02, 0x02, "Single"				},
	{0x02, 0x01, 0x02, 0x00, "Dual"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x04, 0x04, "Off"					},
	{0x02, 0x01, 0x04, 0x00, "On"					},
};

STDDIPINFO(Bubsys)

static UINT8 konamigt_read_wheel()
{
	static UINT8 CURVE_Logy[] = {
		0x00, 0x01, 0x13, 0x1d, 0x25, 0x2b, 0x2f, 0x33, 0x37, 0x3a, 0x3d, 0x3f, 0x41, 0x44, 0x46, 0x47,
		0x49, 0x4b, 0x4c, 0x4d, 0x4f, 0x50, 0x51, 0x52, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x59, 0x5a,
		0x5b, 0x5c, 0x5d, 0x5d, 0x5e, 0x5f, 0x60, 0x60, 0x61, 0x62, 0x62, 0x63, 0x63, 0x64, 0x65, 0x65,
		0x66, 0x66, 0x67, 0x67, 0x68, 0x68, 0x69, 0x69, 0x6a, 0x6a, 0x6b, 0x6b, 0x6c, 0x6c, 0x6c, 0x6d,
		0x6d, 0x6e, 0x6e, 0x6e, 0x6f, 0x6f, 0x70, 0x70, 0x70, 0x71, 0x71, 0x71, 0x72, 0x72, 0x72, 0x73,
		0x73, 0x73, 0x74, 0x74, 0x74, 0x75, 0x75, 0x75, 0x76, 0x76, 0x76, 0x76, 0x77, 0x77, 0x77, 0x78,
		0x78, 0x78, 0x78, 0x79, 0x79, 0x79, 0x79, 0x7a, 0x7a, 0x7a, 0x7a, 0x7b, 0x7b, 0x7b, 0x7b, 0x7c,
		0x7c, 0x7c, 0x7c, 0x7d, 0x7d, 0x7d, 0x7d, 0x7d, 0x7e, 0x7e, 0x7e, 0x7e, 0x7f, 0x7f, 0x7f, 0x80,
		0x80, 0x81, 0x81, 0x81, 0x82, 0x82, 0x82, 0x82, 0x83, 0x83, 0x83, 0x83, 0x83, 0x84, 0x84, 0x84,
		0x84, 0x85, 0x85, 0x85, 0x85, 0x86, 0x86, 0x86, 0x86, 0x87, 0x87, 0x87, 0x87, 0x88, 0x88, 0x88,
		0x88, 0x89, 0x89, 0x89, 0x8a, 0x8a, 0x8a, 0x8a, 0x8b, 0x8b, 0x8b, 0x8c, 0x8c, 0x8c, 0x8d, 0x8d,
		0x8d, 0x8e, 0x8e, 0x8e, 0x8f, 0x8f, 0x8f, 0x90, 0x90, 0x90, 0x91, 0x91, 0x92, 0x92, 0x92, 0x93,
		0x93, 0x94, 0x94, 0x94, 0x95, 0x95, 0x96, 0x96, 0x97, 0x97, 0x98, 0x98, 0x99, 0x99, 0x9a, 0x9a,
		0x9b, 0x9b, 0x9c, 0x9d, 0x9d, 0x9e, 0x9e, 0x9f, 0xa0, 0xa0, 0xa1, 0xa2, 0xa3, 0xa3, 0xa4, 0xa5,
		0xa6, 0xa7, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xae, 0xaf, 0xb0, 0xb1, 0xb3, 0xb4, 0xb5, 0xb7,
		0xb9, 0xba, 0xbc, 0xbf, 0xc1, 0xc3, 0xc6, 0xc9, 0xcd, 0xd1, 0xd5, 0xdb, 0xe3, 0xed, 0xff, 0xff };

	return CURVE_Logy[ProcessAnalog(AnalogPort0, 0, 0, 0x00, 0xff)];
}

static UINT16 konamigt_read_analog(int /*Offset*/)
{
	UINT16 nRet = 0;

	if (DrvInputs[3] & 0x20) nRet |= 0x0300; // break
	if (DrvInputs[3] & 0x40) nRet |= 0xf000; // accel
	nRet |= DrvDial1 & 0x7f;

	return nRet;
}

static void __fastcall nemesis_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x05c001:
			*soundlatch = data;
		return;

		case 0x05c801:
			watchdog = 0;
		return;

		case 0x05e000:
		case 0x05e002:
			// coin counter
		return;

		case 0x05e001:
			*m68k_irq_enable = data;
		return;

		case 0x05e004:
			if (data & 1) {
				ZetSetVector(0xff);
				ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			}
		return;

		case 0x05e005:
			*flipscreen = data & 0x01;
			*tilemap_flip_x = data & 0x01;
		return;

		case 0x05e007:
			*tilemap_flip_y = data & 0x01;
		return;
	}

	//bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static void __fastcall nemesis_main_write_word(UINT32 address, UINT16 data)
{
   // if (address && data) return; // kill warnings

	bprintf (0, _T("------------WW %5.5x, %4.4x\n"), address, data);
}


static UINT8 __fastcall nemesis_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x05c401:
			return DrvDips[0];

		case 0x05c403:
			return DrvDips[1];

		case 0x05cc01:
			return DrvInputs[0];

		case 0x05cc03:
			return DrvInputs[1];

		case 0x05cc05:
			return DrvInputs[2];

		case 0x05cc07:
			return  DrvDips[2];

		case 0x070000:
			return konamigt_read_analog(0) >> 8;

		case 0x070001:
			return konamigt_read_analog(1) & 0xff;
	}

	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}

static UINT16 __fastcall nemesis_main_read_word(UINT32 address)
{
	if ((address & 0xfffff8) == 0x040000) {
		return mcu_control[(address / 2) & 3];
	}

	switch (address)
	{
		case 0x070000:
		case 0x0c2000:
			return konamigt_read_analog(0);
	}

	bprintf (0, _T("RW %5.5x\n"), address);

	return 0;
}

static void __fastcall konamigt_main_write_byte(UINT32 address, UINT8 data)
{
//	bprintf(0, _T("wb %x  %x\n"), address, data);
	switch (address)
	{
		case 0x05e001:
			*m68k_irq_enable2 = data;
		return;

		case 0x05e003:
			*m68k_irq_enable = data;
		return;
	}

	nemesis_main_write_byte(address, data);
}

static void __fastcall salamand_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x0a0000:
			if (data & 0x08) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x0a0001:
		{
			*m68k_irq_enable = data & 0x01;
			*m68k_irq_enable2 = data & 0x02;
			*flipscreen = data & 0x04;
			*tilemap_flip_x = data & 0x04;
			*tilemap_flip_y = data & 0x08;
		}
		return;

		case 0x0c0001:
			*soundlatch = data;
		return;

		case 0x0c0005:
		case 0x0c0008:
		case 0x0c0009:
			watchdog = 0;
		return;
	}

//	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static void __fastcall salamand_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x0a0000:
		{
			*m68k_irq_enable = data & 0x01;
			*m68k_irq_enable2 = data & 0x02;
			*flipscreen = data & 0x04;
			*tilemap_flip_x = data & 0x04;
			*tilemap_flip_y = data & 0x08;
			if (data & 0x0800) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		}
		return;
	}

//	bprintf (0, _T("WW %5.5x, %4.4x\n"), address, data);
}

static UINT8 __fastcall salamand_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x0c0003:
			return DrvDips[0];

		case 0x0c2001:
			return DrvInputs[0];

		case 0x0c2003:
			return DrvInputs[1];

		case 0x0c2005:
			return DrvInputs[2];

		case 0x0c2007:
			return DrvDips[1];
	}

//	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall hcrash_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x0c0003:
			return DrvDips[0];

		case 0x0c0005:
			return DrvDips[1];

		case 0x0c0007:
			return DrvDips[2]; //DrvInputs[2];

		case 0x0c000b:
			return DrvInputs[0];

		case 0x0c2000:
			return konamigt_read_analog(0) >> 8;

		case 0x0c2001:
			return konamigt_read_analog(1) & 0xff;

		case 0x0c4001:
			return (DrvInputs[3] & 0x30) >> 1;

		//case 0x0c4002:
		case 0x0c4003:
			switch (selected_ip & 0xf)
			{                               				// From WEC Le Mans Schems:
				case 0xc:  return (DrvInputs[3] & 0x40)<<1;	// Accel - Schems: Accelevr
				case 0:    return (DrvInputs[3] & 0x40)<<1;
				case 0xd:  return konamigt_read_wheel();	// Wheel - Schems: Handlevr
				case 1:    return konamigt_read_wheel();

			    default: return ~0;
			}
	}

//	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}

static void __fastcall hcrash_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x0a0000:
			if (data & 0x08) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x0a0001:
		{
			*m68k_irq_enable = data & 0x01;
			*m68k_irq_enable2 = data & 0x02;
			*flipscreen = data & 0x04;
			*tilemap_flip_x = data & 0x04;
			*tilemap_flip_y = data & 0x08;
		}
		return;

		case 0x0c0001:
			*soundlatch = data;
		return;

		case 0x0c0005:
		case 0x0c0008:
		case 0x0c0009:
			watchdog = 0;
		return;

		case 0x0c2801:
		return;

		case 0x0c2802:
		return;

		case 0x0c2803:
			*m68k_irq_enable2 = data & 1;
		return;

		case 0x0c2805:
		return;

		case 0x0c4000:
		case 0x0c4001: //bprintf(0, _T("s_ip ad[%X:%X]"), address & 1, data);
			selected_ip = data;
		return;
	}

//	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static void bubsys_mcu_write(INT32 offset)
{
	if (is_bubble_system == 0) return;

	if (offset == 1)
	{
		if (mcu_control[1] == 1)
		{
			INT32 page = (mcu_control[0] & 0x7ff) * 0x90;

			memcpy (Drv68KRAM0 + 0xf00, Drv68KROM + page, 0x80);
			BurnByteswap(Drv68KRAM0 + 0xf00, 0x80);

			mcu_control[0] = Drv68KROM[page + 0x81] | (Drv68KROM[page + 0x80] << 8);

			SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		}
	}
}

static void __fastcall gx400_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffff8) == 0x040000) {
		INT32 offset = (address / 2) & 3;
		mcu_control[offset] = data;
		bubsys_mcu_write(offset);
		return;
	}

//	bprintf (0, _T("WW %5.5x, %4.4x\n"), address, data);
}

static void __fastcall gx400_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffff8) == 0x040000) {
		UINT8 *ctrl = (UINT8*)mcu_control;
		ctrl[(address & 0x7) ^ 1] = data;
		bubsys_mcu_write((address / 2) & 3);
		return;
	}

	if ((address & 0xff8001) == 0x020001) {
		DrvShareRAM[(address & 0x7ffe)/2] = data & 0xff;
		return;
	}

	switch (address)
	{
		case 0x05c001:
			*soundlatch = data;
		return;

		case 0x05c801: // doesn't actually seem to work well as watchdog, use $5e008 instead?
			watchdog = 0;
		return;

		case 0x05e000:
			// coin counter
		return;

		case 0x05e001:
			*m68k_irq_enable2 = data & 1;
		return;

		case 0x05e003:
			*m68k_irq_enable = data & 1;
		return;

		case 0x05e004:
			if (data & 1) {
				ZetSetVector(0xff);
				ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			}
		return;

		case 0x05e005:
			*flipscreen = data & 0x01;
			*tilemap_flip_x = data & 0x01;
		return;

		case 0x05e007:
			*tilemap_flip_y = data & 0x01;
		return;

		case 0x05e008: // nop
			watchdog = 0; // ?
		return;

		case 0x05e00e:
			*m68k_irq_enable4 = data & 0x01;
		return;

		case 0x78004:
			//DrvDial1 = 0;
			return;
	}

//	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT8 __fastcall gx400_main_read_byte(UINT32 address)
{
	if ((address & 0xfffff8) == 0x040000) {
		UINT8 *ctrl = (UINT8*)mcu_control;
		return ctrl[(address & 7) ^ 1];
	}

	if ((address & 0xff8001) == 0x020001) {
		return DrvShareRAM[(address & 0x7ffe)/2];
	}

	switch (address)
	{
		case 0x05c403:
			return DrvDips[0];

		case 0x05c405:
			return DrvDips[1];

		case 0x05c407:
			return DrvDips[2];

		case 0x05cc01:
			return DrvInputs[0];

		case 0x05cc03:
			return DrvInputs[1];

		case 0x05cc05:
			return DrvInputs[2];

		case 0x070000:
			return konamigt_read_analog(0) >> 8;

		case 0x070001:
			return konamigt_read_analog(1) & 0xff;
	}

//	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}

static void __fastcall citybomb_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x070011:
		case 0x0f0011:
			*soundlatch = data;
		return;

		case 0x070019:
		case 0x0f0019:
			watchdog = 0;
		return;

		case 0x078000:
		case 0x0f8000:
			// coin counters
			if (data & 0x08) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			selected_ip = (~data & 0x10) >> 4;
		return;

		case 0x078001:
		case 0x0f8001:
		{
			//if (data & 0x0c) bprintf (0, _T("WW %5.5x, %4.4x\n"), address, data);
			*m68k_irq_enable = data & 0x01;
			*m68k_irq_enable2 = data & 0x02;
			*flipscreen = data & 0x04;
			*tilemap_flip_x = data & 0x04;
			*tilemap_flip_y = data & 0x08;
		}
		return;
	}

//	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static void __fastcall citybomb_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x078000:
		case 0x0f8000:
		{
			*m68k_irq_enable = data & 0x01;
			*m68k_irq_enable2 = data & 0x02;
			*flipscreen = data & 0x04;
			*tilemap_flip_x = data & 0x04;
			*tilemap_flip_y = data & 0x08;
			if (data & 0x0800) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			selected_ip = (~data & 0x1000) >> 12;
		}
		return;
	}

//	bprintf (0, _T("WW %5.5x, %4.4x\n"), address, data);
}

static UINT8 __fastcall citybomb_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x0c2000:
			return konamigt_read_analog(0) >> 8;

		case 0x0c2001:
			return konamigt_read_analog(1) & 0xff;

		case 0x070001:
		case 0x0f0001:
			return DrvDips[1];

		case 0x070003:
		case 0x0f0003:
			return DrvInputs[2];

		case 0x070005:
		case 0x0f0005:
			return DrvInputs[1];

		case 0x070007:
		case 0x0f0007:
			return DrvInputs[0];

		case 0x070009:
		case 0x0f0009:
			return DrvDips[0];

		case 0x0f0021: // WEC Le Mans inputs...
			return 0;
	}

//	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}

static void nemesis_filter_w(UINT16 /*offset*/)
{
#if 0
	// not used right now..
	INT32 C1 = /* offset & 0x1000 ? 4700 : */ 0; // is this right? 4.7uF seems too large
	INT32 C2 = offset & 0x0800 ? 33 : 0;         // 0.033uF = 33 nF
	INT32 AY8910_INTERNAL_RESISTANCE = 356;

	filter_rc_set_RC(0, FLT_RC_LOWPASS, (AY8910_INTERNAL_RESISTANCE + 12000) / 3, 0, 0, CAP_N(C1)); // unused?
	filter_rc_set_RC(1, FLT_RC_LOWPASS, (AY8910_INTERNAL_RESISTANCE + 12000) / 3, 0, 0, CAP_N(C1)); // unused?
	filter_rc_set_RC(2, FLT_RC_LOWPASS, (AY8910_INTERNAL_RESISTANCE + 12000) / 3, 0, 0, CAP_N(C1)); // unused?

	filter_rc_set_RC(3, FLT_RC_LOWPASS, AY8910_INTERNAL_RESISTANCE + 1000, 10000, 0, CAP_N(C2));
	filter_rc_set_RC(4, FLT_RC_LOWPASS, AY8910_INTERNAL_RESISTANCE + 1000, 10000, 0, CAP_N(C2));
	filter_rc_set_RC(5, FLT_RC_LOWPASS, AY8910_INTERNAL_RESISTANCE + 1000, 10000, 0, CAP_N(C2));
#endif
}

static void __fastcall nemesis_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0xa000) {
		K005289Ld1Write(address);
		return;
	}

	if ((address & 0xf000) == 0xc000) {
		K005289Ld2Write(address);
		return;
	}

	if ((address & ~0x78) == 0xe000 && vlm5030_enable) {
		INT32 offset = address - 0xe000;
		vlm5030_rst(0, (address & 0x10) >> 4);
		vlm5030_st(0, (address & 0x20) >> 5);
		if (offset == 0) {
			vlm5030_data_write(0, data);
		}
		return;
	}

	if ((address & ~0x1ff8) == 0xe007) {
		nemesis_filter_w(address);
		return;
	}

	switch (address)
	{
		case 0xe003:
			K005289Tg1Write();
		return;

		case 0xe004:
			K005289Tg2Write();
		return;

		case 0xe005:
			AY8910Write(1, 0, data);
		return;

		case 0xe006:
			AY8910Write(0, 0, data);
		return;

		case 0xe106:
			AY8910Write(0, 1, data);
		return;

		case 0xe405:
			AY8910Write(1, 1, data);
		return;
	}
//	bprintf(0, _T("sw(%X, %X);..\n"), address, data);
}

static UINT8 __fastcall nemesis_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe001:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch;

		case 0xe086:
			return AY8910Read(0);

		case 0xe205:
			return AY8910Read(1);
	}

	return 0;
}

static void __fastcall salamand_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0xb000) {
		K007232WriteReg(0, address & 0x0f, data);
		return;
	}

	switch (address)
	{
		case 0xc000:
			BurnYM2151SelectRegister(data);
		return;

		case 0xc001:
			BurnYM2151WriteRegister(data);
		return;

		case 0xd000:
			if (vlm5030_enable) {
				vlm5030_data_write(0, data);
			}
		return;

		case 0xf000:
			if (vlm5030_enable) {
				vlm5030_rst(0, data & 1);
				vlm5030_st(0, (data >> 1) & 1);
			}
		return;
	}
}

static UINT8 __fastcall salamand_sound_read(UINT16 address)
{
	if ((address & 0xfff0) == 0xb000) {
		return K007232ReadReg(0, address & 0x0f);
	}

	switch (address)
	{
		case 0xa000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch;

		case 0xc000:
		case 0xc001:
			return BurnYM2151Read();

		case 0xe000:
			return vlm5030_bsy(0);
	}

	return 0;
}

static void sound_bankswitch(INT32 data)
{
	INT32 bank_A = (data >> 0) & 3;
	INT32 bank_B = (data >> 2) & 3;

	k007232_set_bank(0, bank_A, bank_B);
}

static void __fastcall citybomb_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff00) == 0x9800) {
		K051649Write(address & 0xff, data);
		return;
	}

	if ((address & 0xfff0) == 0xb000) {
		K007232WriteReg(0, address & 0x0f, data);
		return;
	}

	switch (address)
	{
		case 0xa000:
		case 0xa001:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0xc000:
			sound_bankswitch(data);
		return;
	}
}

static UINT8 __fastcall citybomb_sound_read(UINT16 address)
{
	if ((address & 0xff00) == 0x9800) {
		return K051649Read(address & 0xff);
	}

	if ((address & 0xfff0) == 0xb000) {
		K007232ReadReg(0, address & 0x0f);
	}

	switch (address)
	{
		case 0xa000:
		case 0xa001:
			return BurnYM3812Read(0, address & 1);

		case 0xd000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch;
	}

	return 0;
}

static UINT8 nemesis_AY8910_0_portA(UINT32)
{
	INT32 ret = (ZetTotalCycles() >> 10) & 0x0f;

	if (vlm5030_enable) {
		if (vlm5030_bsy(0)) {
			ret |= 0x20;
		}
	}

	return ret | 0xd0;
}

static void k005289_control_A_write(UINT32, UINT32 data)
{
	K005289ControlAWrite(data);
}

static void k005289_control_B_write(UINT32, UINT32 data)
{
	K005289ControlBWrite(data);
}

static UINT32 salamand_vlm_sync(INT32 samples_rate)
{
	return (samples_rate * ZetTotalCycles()) / 3579545;
}

static void DrvK007232VolCallback(INT32 v)
{
	K007232SetVolume(0, 0, (v >> 0x4) * 0x11, 0);
	K007232SetVolume(0, 1, 0, (v & 0x0f) * 0x11);
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 3579545;
}

static inline void update_char_tiles(UINT32 offset)
{
	offset &= 0xfffe;

	INT32 data = *((UINT16*)(DrvCharRAM + offset));

	offset *= 2;

	DrvCharRAMExp[offset + 0] = (data >> 12);
	DrvCharRAMExp[offset + 1] = (data >>  8) & 0xf;
	DrvCharRAMExp[offset + 2] = (data >>  4) & 0xf;
	DrvCharRAMExp[offset + 3] = (data & 0x0f);
}

static void __fastcall nemesis_charram_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvCharRAM + (address & 0xfffe))) = data;

	update_char_tiles(address);
}

static void __fastcall nemesis_charram_write_byte(UINT32 address, UINT8 data)
{
	DrvCharRAM[(address & 0xffff)^1] = data;

	update_char_tiles(address);
}

static void nemesis_palette_update(INT32 i)
{
	static const UINT8 color_table[3][32] =
	{
		{	// correct colors (calculated by resnet system in MAME)
			0x00, 0x01, 0x02, 0x04, 0x05, 0x06, 0x08, 0x09, 0x0b, 0x0d, 0x0f, 0x12, 0x14, 0x16, 0x19, 0x1c, 
			0x21, 0x24, 0x29, 0x2e, 0x33, 0x39, 0x40, 0x49, 0x50, 0x5b, 0x68, 0x78, 0x8e, 0xa8, 0xcc, 0xff
		},
		{	// previous, incorrect (lighter) colors
			0x00, 0x08, 0x11, 0x19, 0x21, 0x29, 0x32, 0x3a, 0x40, 0x48, 0x51, 0x59, 0x61, 0x69, 0x72, 0x7a,
			0x85, 0x8d, 0x96, 0x9e, 0xa6, 0xae, 0xb7, 0xbf, 0xc5, 0xcd, 0xd6, 0xde, 0xe6, 0xee, 0xf7, 0xff
		},
		{	// MAMEUIFX colors (mid)
			0x00, 0x00, 0x01, 0x02, 0x04, 0x06, 0x09, 0x0d, 0x10, 0x14, 0x19, 0x1f, 0x24, 0x2b, 0x32, 0x3a,
			0x45, 0x4d, 0x58, 0x61, 0x6c, 0x76, 0x83, 0x8f, 0x98, 0xa4, 0xb3, 0xc1, 0xcf, 0xde, 0xef, 0xff
		}
	};

	i &= 0xffe;

	INT32 p = (DrvPalRAM[i+1] << 8) | DrvPalRAM[i];

	INT32 b = color_table[DrvDips[3]&3][(p >> 10) & 0x1f];
	INT32 g = color_table[DrvDips[3]&3][(p >>  5) & 0x1f];
	INT32 r = color_table[DrvDips[3]&3][(p >>  0) & 0x1f];

	DrvPalette[i/2] = BurnHighCol(r,g,b,0);
}	

static void salamand_palette_update(INT32 i)
{
	i &= 0x1ffc;

	INT32 p = (DrvPalRAM[i+0] << 8) | DrvPalRAM[i+2];

	INT32 b = (p >> 10) & 0x1f;
	INT32 g = (p >>  5) & 0x1f;
	INT32 r = (p >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[i/4] = BurnHighCol(r,g,b,0);
}

static void __fastcall nemesis_palette_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvPalRAM + (address & 0x1ffe))) = data;

	palette_write(address);
}

static void __fastcall nemesis_palette_write_byte(UINT32 address, UINT8 data)
{
	DrvPalRAM[(address & 0x1fff) ^ 1] = data;

	palette_write(address);
}

static INT32 MemIndex()
{
	UINT8 *Next; Next	= AllMem;

	Drv68KROM			= Next; Next += 0x100000;
	DrvZ80ROM			= Next; Next += 0x010000;

	K005289ROM			= Next; Next += 0x000200;
	DrvVLMROM			= Next; Next += 0x004000;
	K007232ROM			= Next; Next += 0x080000;

	DrvCharRAMExp		= Next; Next += 0x020000;

	DrvPalette			= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam				= Next;

	Drv68KRAM0			= Next; Next += 0x020000;
	Drv68KRAM1			= Next; Next += 0x020000;
	Drv68KRAM2			= Next; Next += 0x001000;
	DrvPalRAM			= Next; Next += 0x002000;
	DrvSprRAM			= Next; Next += 0x001000;
	DrvVidRAM0			= Next; Next += 0x001000;
	DrvVidRAM1			= Next; Next += 0x001000;
	DrvColRAM0			= Next; Next += 0x001000;
	DrvColRAM1			= Next; Next += 0x001000;
	DrvCharRAM			= Next; Next += 0x010000;
	DrvScrollRAM		= Next; Next += 0x002000;

	DrvZ80RAM			= Next; Next += 0x000800;
	DrvShareRAM			= Next; Next += 0x004000;

	soundlatch 			= Next; Next += 0x000001;
	flipscreen			= Next; Next += 0x000001;

	m68k_irq_enable		= Next; Next += 0x000001;
	m68k_irq_enable2	= Next; Next += 0x000001;
	m68k_irq_enable4	= Next; Next += 0x000001;
	tilemap_flip_x		= Next; Next += 0x000001;
	tilemap_flip_y		= Next; Next += 0x000001;

	mcu_control			= (UINT16*)Next; Next += 0x00004 * sizeof(UINT16);

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);

	if (is_bubble_system)
	{
		BurnLoadRom(Drv68KRAM0, 0, 1); // load bubsys bios
		BurnByteswap(Drv68KRAM0, 0x1e0);

		mcu_control[3] = 0x240;
	}

	SekReset(0);
	ZetReset(0);

	ZetOpen(0);
	if (ay8910_enable) AY8910Reset(0);
	if (ay8910_enable) AY8910Reset(1);
	if (ym2151_enable) BurnYM2151Reset();
	if (ym3812_enable) BurnYM3812Reset();
	if (vlm5030_enable) vlm5030Reset(0);
	if (k007232_enable) K007232Reset(0);
	if (k005289_enable) K005289Reset();
	if (k051649_enable) K051649Reset();
	ZetClose();

	watchdog = 0;
	selected_ip = 0;

	if (bUseShifter)
		BurnShiftReset();

	DrvDial1 = 0x3f;
	last_dial = 0x3f;
	center_dial_timer = 0;

	scanline_counter = 0;

	nCyclesExtra[0] = nCyclesExtra[1] = 0;

	HiscoreReset();

	return 0;
}

static void NemesisSoundInit(INT32 konamigtmode)
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x4000, 0x47ff, MAP_RAM);
	ZetSetWriteHandler(nemesis_sound_write);
	ZetSetReadHandler(nemesis_sound_read);
	ZetClose();
	
	K005289Init(3579545, K005289ROM);
	K005289SetRoute(BURN_SND_K005289_ROUTE_1, 0.35, BURN_SND_ROUTE_BOTH);

	AY8910Init(0, 14318180/8, 0);
	AY8910Init(1, 14318180/8, 1);
	AY8910SetPorts(0, &nemesis_AY8910_0_portA, NULL, NULL, NULL);
	AY8910SetPorts(1, NULL, NULL, &k005289_control_A_write, &k005289_control_B_write);
	AY8910SetBuffered(ZetTotalCycles, 3579545);
	AY8910SetAllRoutes(0, 0.35, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, (konamigtmode) ? 0.20 : 1.00, BURN_SND_ROUTE_BOTH);

#if 0
	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 0); // ay 0
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);

	filter_rc_init(3, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1); // ay 1
	filter_rc_init(4, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(5, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);

	filter_rc_set_src_gain(0, 0.35);
	filter_rc_set_src_gain(1, 0.35);
	filter_rc_set_src_gain(2, 0.35);
	filter_rc_set_src_gain(3, 1.00);
	filter_rc_set_src_gain(4, 1.00);
	filter_rc_set_src_gain(5, 1.00);
	rcflt_enable = 1;
#endif

	ay8910_enable = 1;
	k005289_enable = 1;
}

static void Gx400SoundInit(INT32 rf2mode)
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,		0x4000, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvVLMROM,			0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(nemesis_sound_write);
	ZetSetReadHandler(nemesis_sound_read);
	ZetClose();

	K005289Init(3579545, K005289ROM);
	K005289SetRoute(BURN_SND_K005289_ROUTE_1, 0.35, BURN_SND_ROUTE_BOTH);

	AY8910Init(0, 14318180/8, 0);
	AY8910Init(1, 14318180/8, 1);
	AY8910SetPorts(0, &nemesis_AY8910_0_portA, NULL, NULL, NULL);
	AY8910SetPorts(1, NULL, NULL, &k005289_control_A_write, &k005289_control_B_write);
	AY8910SetBuffered(ZetTotalCycles, 3579545);
	AY8910SetAllRoutes(0, (rf2mode) ? 0.20 : 0.35, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, (rf2mode) ? 0.15 : 1.00, BURN_SND_ROUTE_BOTH);

	vlm5030Init(0,  3579545, salamand_vlm_sync, DrvVLMROM, 0x0800, 1);
	vlm5030SetAllRoutes(0, 1.70, BURN_SND_ROUTE_BOTH);

	ay8910_enable = 1;
	k005289_enable = 1;
	vlm5030_enable = 1;
	vlm5030_enable_ram = 1;
}

static void TwinbeeGx400SoundInit()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,		0x4000, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvVLMROM,			0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(nemesis_sound_write);
	ZetSetReadHandler(nemesis_sound_read);
	ZetClose();

	K005289Init(3579545, K005289ROM);
	K005289SetRoute(BURN_SND_K005289_ROUTE_1, 0.20, BURN_SND_ROUTE_BOTH);

	AY8910Init(0, 14318180/8, 0);
	AY8910Init(1, 14318180/8, 1);
	AY8910SetPorts(0, &nemesis_AY8910_0_portA, NULL, NULL, NULL);
	AY8910SetPorts(1, NULL, NULL, &k005289_control_A_write, &k005289_control_B_write);
	AY8910SetBuffered(ZetTotalCycles, 3579545);
	AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH); // melody & hat
	AY8910SetAllRoutes(1, 1.00, BURN_SND_ROUTE_BOTH); // drums, explosions

	vlm5030Init(0,  3579545, salamand_vlm_sync, DrvVLMROM, 0x0800, 1);
	vlm5030SetAllRoutes(0, 3.10, BURN_SND_ROUTE_BOTH);

	ay8910_enable = 1;
	k005289_enable = 1;
	vlm5030_enable = 1;
	vlm5030_enable_ram = 1;
}

static void SalamandSoundInit()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(salamand_sound_write);
	ZetSetReadHandler(salamand_sound_read);
	ZetClose();

	BurnYM2151InitBuffered(3579545, 1, NULL, 0);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.20, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.20, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachZet(3579545);

	K007232Init(0,  3579545, K007232ROM, 0x20000);
	K007232SetPortWriteHandler(0, DrvK007232VolCallback);
	K007232PCMSetAllRoutes(0, 0.08, BURN_SND_ROUTE_BOTH);

	if (DrvVLMROM[1] || DrvVLMROM[2]) {
		vlm5030Init(0,  3579545, salamand_vlm_sync, DrvVLMROM, 0x4000, 1);
		vlm5030SetAllRoutes(0, (hcrash_mode) ? 4.00 : 2.50, BURN_SND_ROUTE_BOTH);
		vlm5030_enable = 1;
	}

	ym2151_enable = 1;
	k007232_enable = 1;
}

static void CitybombSoundInit()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(citybomb_sound_write);
	ZetSetReadHandler(citybomb_sound_read);
	ZetClose();

	BurnYM3812Init(1, 3579545, NULL, DrvSynchroniseStream, 0);
	BurnTimerAttach(&ZetConfig, 3579545);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	K007232Init(0,  3579545, K007232ROM, 0x80000);
	K007232SetPortWriteHandler(0, DrvK007232VolCallback);
	K007232PCMSetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	K051649Init(3579545/2);
	K051649SetRoute(0.38, BURN_SND_ROUTE_BOTH);

	ym3812_enable = 1;
	k007232_enable = 1;
	k051649_enable = 1;
}

static INT32 NemesisInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x010001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x010000,  3, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x020001,  4, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x020000,  5, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x030001,  6, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x030000,  7, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x000000,  8, 1)) return 1;

		if (BurnLoadRom(K005289ROM + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(K005289ROM + 0x00100, 10, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvCharRAM,		0x040000, 0x04ffff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,		0x050000, 0x051fff, MAP_RAM);
	xscroll1 = (UINT16*)(DrvScrollRAM + 0x00000);
	xscroll2 = (UINT16*)(DrvScrollRAM + 0x00400);
	yscroll2 = (UINT16*)(DrvScrollRAM + 0x00f00);
	yscroll1 = (UINT16*)(DrvScrollRAM + 0x00f80);
	SekMapMemory(DrvVidRAM0,		0x052000, 0x052fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x053000, 0x053fff, MAP_RAM);
	SekMapMemory(DrvColRAM0,		0x054000, 0x054fff, MAP_RAM);
	SekMapMemory(DrvColRAM1,		0x055000, 0x055fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x056000, 0x056fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x05a000, 0x05afff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,		0x060000, 0x067fff, MAP_RAM);
	SekSetWriteWordHandler(0,		nemesis_main_write_word);
	SekSetWriteByteHandler(0,		nemesis_main_write_byte);
	SekSetReadWordHandler(0,		nemesis_main_read_word);
	SekSetReadByteHandler(0,		nemesis_main_read_byte);
	SekMapHandler(1,			0x040000, 0x04ffff, MAP_WRITE);
	SekSetWriteWordHandler(1, 		nemesis_charram_write_word);
	SekSetWriteByteHandler(1, 		nemesis_charram_write_byte);
	SekMapHandler(2,			0x05a000, 0x05afff, MAP_WRITE);
	SekSetWriteWordHandler(2, 		nemesis_palette_write_word);
	SekSetWriteByteHandler(2, 		nemesis_palette_write_byte);
	SekClose();

	NemesisSoundInit(0);

	palette_write = nemesis_palette_update;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static void ShifterSetup()
{
	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_GREEN, 80);
	bUseShifter = 1;
}

static INT32 KonamigtInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x010001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x010000,  3, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x020001,  4, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x020000,  5, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x030001,  6, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x030000,  7, 2)) return 1;

		//BurnDump("kgt_fbn.bin", Drv68KROM, 0x40000);

		if (BurnLoadRom(DrvZ80ROM + 0x000000,  8, 1)) return 1;

		if (BurnLoadRom(K005289ROM + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(K005289ROM + 0x00100, 10, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvCharRAM,		0x040000, 0x04ffff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,		0x050000, 0x051fff, MAP_RAM);
	xscroll1 = (UINT16*)(DrvScrollRAM + 0x00000);
	xscroll2 = (UINT16*)(DrvScrollRAM + 0x00400);
	yscroll2 = (UINT16*)(DrvScrollRAM + 0x00f00);
	yscroll1 = (UINT16*)(DrvScrollRAM + 0x00f80);
	SekMapMemory(DrvVidRAM0,		0x052000, 0x052fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x053000, 0x053fff, MAP_RAM);
	SekMapMemory(DrvColRAM0,		0x054000, 0x054fff, MAP_RAM);
	SekMapMemory(DrvColRAM1,		0x055000, 0x055fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x056000, 0x056fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x05a000, 0x05afff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,		0x060000, 0x067fff, MAP_RAM);
	SekSetWriteWordHandler(0,		nemesis_main_write_word);
	SekSetWriteByteHandler(0,		konamigt_main_write_byte);
	SekSetReadWordHandler(0,		nemesis_main_read_word);
	SekSetReadByteHandler(0,		nemesis_main_read_byte);
	SekMapHandler(1,			0x040000, 0x04ffff, MAP_WRITE);
	SekSetWriteWordHandler(1, 		nemesis_charram_write_word);
	SekSetWriteByteHandler(1, 		nemesis_charram_write_byte);
	SekMapHandler(2,			0x05a000, 0x05afff, MAP_WRITE);
	SekSetWriteWordHandler(2, 		nemesis_palette_write_word);
	SekSetWriteByteHandler(2, 		nemesis_palette_write_byte);
	SekClose();

	NemesisSoundInit(1);

	palette_write = nemesis_palette_update;

	GenericTilesInit();

	ShifterSetup();

	DrvDoReset();

	return 0;
}

static INT32 SalamandInit()
{
	BurnAllocMemIndex();

	{
		INT32 nIndex = 0;

		if (BurnLoadRom(Drv68KROM + 0x000001, nIndex++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000, nIndex++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040001, nIndex++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040000, nIndex++, 2)) return 1;

		if (0 == strcmp(BurnDrvGetTextA(DRV_NAME), "salamandt")) {
			if (BurnLoadRom(Drv68KROM + 0x060001, nIndex++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM + 0x060000, nIndex++, 2)) return 1;
		}

		if (BurnLoadRom(DrvZ80ROM + 0x000000, nIndex++, 1)) return 1;

		if (BurnLoadRom(DrvVLMROM + 0x000000, nIndex++, 1)) return 1;

		if (BurnLoadRom(K007232ROM + 0x00000, nIndex++, 1)) return 1;

		if (0 == strcmp(BurnDrvGetTextA(DRV_NAME), "salamandt")) {
			if (BurnLoadRom(K007232ROM + 0x10000, nIndex++, 1)) return 1;
		}
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM0,		0x080000, 0x087fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x090000, 0x091fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x101000, 0x101fff, MAP_RAM);
	SekMapMemory(DrvColRAM1,		0x102000, 0x102fff, MAP_RAM);
	SekMapMemory(DrvColRAM0,		0x103000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvCharRAM,		0x120000, 0x12ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x180000, 0x180fff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,		0x190000, 0x191fff, MAP_RAM);
	xscroll2 = (UINT16*)(DrvScrollRAM + 0x00000);
	xscroll1 = (UINT16*)(DrvScrollRAM + 0x00400);
	yscroll1 = (UINT16*)(DrvScrollRAM + 0x00f00);
	yscroll2 = (UINT16*)(DrvScrollRAM + 0x00f80);
	SekSetWriteWordHandler(0,		salamand_main_write_word);
	SekSetWriteByteHandler(0,		salamand_main_write_byte);
	SekSetReadWordHandler(0,		nemesis_main_read_word);
	SekSetReadByteHandler(0,		salamand_main_read_byte);
	SekMapHandler(1,			0x120000, 0x12ffff, MAP_WRITE);
	SekSetWriteWordHandler(1, 		nemesis_charram_write_word);
	SekSetWriteByteHandler(1, 		nemesis_charram_write_byte);
	SekMapHandler(2,			0x090000, 0x091fff, MAP_WRITE);
	SekSetWriteWordHandler(2, 		nemesis_palette_write_word);
	SekSetWriteByteHandler(2, 		nemesis_palette_write_byte);
	SekClose();

	SalamandSoundInit();

	palette_write = salamand_palette_update;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 BlkpnthrInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(K007232ROM + 0x00000,  5, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x080000, 0x081fff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,		0x090000, 0x097fff, MAP_RAM);
	SekMapMemory(DrvColRAM0,		0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(DrvColRAM1,		0x101000, 0x101fff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x102000, 0x102fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x103000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvCharRAM,		0x120000, 0x12ffff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,		0x180000, 0x181fff, MAP_RAM);
	xscroll1 = (UINT16*)(DrvScrollRAM + 0x00000);
	xscroll2 = (UINT16*)(DrvScrollRAM + 0x00400);
	yscroll2 = (UINT16*)(DrvScrollRAM + 0x00f00);
	yscroll1 = (UINT16*)(DrvScrollRAM + 0x00f80);
	SekMapMemory(DrvSprRAM,			0x190000, 0x190fff, MAP_RAM);
	SekSetWriteWordHandler(0,		salamand_main_write_word);
	SekSetWriteByteHandler(0,		salamand_main_write_byte);
	SekSetReadWordHandler(0,		nemesis_main_read_word);
	SekSetReadByteHandler(0,		salamand_main_read_byte);
	SekMapHandler(1,			0x120000, 0x12ffff, MAP_WRITE);
	SekSetWriteWordHandler(1, 		nemesis_charram_write_word);
	SekSetWriteByteHandler(1, 		nemesis_charram_write_byte);
	SekMapHandler(2,			0x080000, 0x081fff, MAP_WRITE);
	SekSetWriteWordHandler(2, 		nemesis_palette_write_word);
	SekSetWriteByteHandler(2, 		nemesis_palette_write_byte);
	SekClose();

	SalamandSoundInit();

	palette_write = salamand_palette_update;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 HcrashInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvVLMROM + 0x004000,  5, 1)) return 1;
		memmove(DrvVLMROM, DrvVLMROM + 0x08000, 0x4000);
		memset(DrvVLMROM + 0x08000, 0, 0x4000);

		if (BurnLoadRom(K007232ROM + 0x00000,  6, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM0,		0x080000, 0x087fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x090000, 0x091fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x101000, 0x101fff, MAP_RAM);
	SekMapMemory(DrvColRAM1,		0x102000, 0x102fff, MAP_RAM);
	SekMapMemory(DrvColRAM0,		0x103000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvCharRAM,		0x120000, 0x12ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x180000, 0x180fff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,		0x190000, 0x191fff, MAP_RAM);
	xscroll2 = (UINT16*)(DrvScrollRAM + 0x00000);
	xscroll1 = (UINT16*)(DrvScrollRAM + 0x00400);
	yscroll1 = (UINT16*)(DrvScrollRAM + 0x00f00);
	yscroll2 = (UINT16*)(DrvScrollRAM + 0x00f80);
	SekSetWriteWordHandler(0,		salamand_main_write_word);
	SekSetWriteByteHandler(0,		hcrash_main_write_byte);
	SekSetReadWordHandler(0,		nemesis_main_read_word);
	SekSetReadByteHandler(0,		hcrash_main_read_byte);
	SekMapHandler(1,			0x120000, 0x12ffff, MAP_WRITE);
	SekSetWriteWordHandler(1, 		nemesis_charram_write_word);
	SekSetWriteByteHandler(1, 		nemesis_charram_write_byte);
	SekMapHandler(2,			0x090000, 0x091fff, MAP_WRITE);
	SekSetWriteWordHandler(2, 		nemesis_palette_write_word);
	SekSetWriteByteHandler(2, 		nemesis_palette_write_byte);
	SekClose();

	hcrash_mode = 1;
	SalamandSoundInit();

	palette_write = salamand_palette_update;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 Gx400Init()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x010001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x010000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(K005289ROM + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(K005289ROM + 0x00100,  6, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x00ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM0,		0x010000, 0x01ffff, MAP_RAM);
	SekMapMemory(DrvCharRAM,		0x030000, 0x03ffff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,		0x050000, 0x051fff, MAP_RAM);
	xscroll1 = (UINT16*)(DrvScrollRAM + 0x00000);
	xscroll2 = (UINT16*)(DrvScrollRAM + 0x00400);
	yscroll2 = (UINT16*)(DrvScrollRAM + 0x00f00);
	yscroll1 = (UINT16*)(DrvScrollRAM + 0x00f80);
	SekMapMemory(DrvVidRAM0,		0x052000, 0x052fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x053000, 0x053fff, MAP_RAM);
	SekMapMemory(DrvColRAM0,		0x054000, 0x054fff, MAP_RAM);
	SekMapMemory(DrvColRAM1,		0x055000, 0x055fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x056000, 0x056fff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,		0x057000, 0x057fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x05a000, 0x05afff, MAP_RAM);
	SekMapMemory(Drv68KRAM1,		0x060000, 0x07ffff, MAP_RAM);
	SekMapMemory(Drv68KROM + 0x10000,	0x080000, 0x0bffff, MAP_ROM);
	SekSetWriteWordHandler(0,		nemesis_main_write_word);
	SekSetWriteByteHandler(0,		gx400_main_write_byte);
	SekSetReadWordHandler(0,		nemesis_main_read_word);
	SekSetReadByteHandler(0,		gx400_main_read_byte);
	SekMapHandler(1,			0x030000, 0x03ffff, MAP_WRITE);
	SekSetWriteWordHandler(1, 		nemesis_charram_write_word);
	SekSetWriteByteHandler(1, 		nemesis_charram_write_byte);
	SekMapHandler(2,			0x05a000, 0x05afff, MAP_WRITE);
	SekSetWriteWordHandler(2, 		nemesis_palette_write_word);
	SekSetWriteByteHandler(2, 		nemesis_palette_write_byte);
	SekClose();
	if (strstr(BurnDrvGetTextA(DRV_NAME), "twin")) {
		TwinbeeGx400SoundInit();
	} else {
		Gx400SoundInit((strstr(BurnDrvGetTextA(DRV_NAME), "gwarr")) ? 1 : 0);
	}

	palette_write = nemesis_palette_update;

	ay8910_enable = 1;
	k005289_enable = 1;
	vlm5030_enable = 1;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 Rf2_gx400Init()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x010001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x010000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(K005289ROM + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(K005289ROM + 0x00100,  6, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x00ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM0,		0x010000, 0x01ffff, MAP_RAM);
	SekMapMemory(DrvCharRAM,		0x030000, 0x03ffff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,		0x050000, 0x051fff, MAP_RAM);
	xscroll1 = (UINT16*)(DrvScrollRAM + 0x00000);
	xscroll2 = (UINT16*)(DrvScrollRAM + 0x00400);
	yscroll2 = (UINT16*)(DrvScrollRAM + 0x00f00);
	yscroll1 = (UINT16*)(DrvScrollRAM + 0x00f80);
	SekMapMemory(DrvVidRAM0,		0x052000, 0x052fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x053000, 0x053fff, MAP_RAM);
	SekMapMemory(DrvColRAM0,		0x054000, 0x054fff, MAP_RAM);
	SekMapMemory(DrvColRAM1,		0x055000, 0x055fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x056000, 0x056fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x05a000, 0x05afff, MAP_RAM);
	SekMapMemory(Drv68KRAM1,		0x060000, 0x067fff, MAP_RAM);
	SekMapMemory(Drv68KROM + 0x10000,	0x080000, 0x0bffff, MAP_ROM);
	SekSetWriteWordHandler(0,		nemesis_main_write_word);
	SekSetWriteByteHandler(0,		gx400_main_write_byte);
	SekSetReadWordHandler(0,		nemesis_main_read_word);
	SekSetReadByteHandler(0,		gx400_main_read_byte);
	SekMapHandler(1,			0x030000, 0x03ffff, MAP_WRITE);
	SekSetWriteWordHandler(1, 		nemesis_charram_write_word);
	SekSetWriteByteHandler(1, 		nemesis_charram_write_byte);
	SekMapHandler(2,			0x05a000, 0x05afff, MAP_WRITE);
	SekSetWriteWordHandler(2, 		nemesis_palette_write_word);
	SekSetWriteByteHandler(2, 		nemesis_palette_write_byte);
	SekClose();

	Gx400SoundInit(1);

	palette_write = nemesis_palette_update;

	GenericTilesInit();

	ShifterSetup();

	DrvDoReset();

	return 0;
}

static INT32 CitybombInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040000,  3, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x080001,  4, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x080000,  5, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0c0001,  6, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0c0000,  7, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x000000,  8, 1)) return 1;

		if (BurnLoadRom(K007232ROM + 0x00000,  9, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM0,		0x080000, 0x087fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x0e0000, 0x0e1fff, MAP_RAM);
	SekMapMemory(Drv68KROM + 0x40000,	0x100000, 0x1bffff, MAP_ROM);
	SekMapMemory(DrvCharRAM,		0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x210000, 0x210fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x211000, 0x211fff, MAP_RAM);
	SekMapMemory(DrvColRAM0,		0x212000, 0x212fff, MAP_RAM);
	SekMapMemory(DrvColRAM1,		0x213000, 0x213fff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,		0x300000, 0x301fff, MAP_RAM);
	xscroll1 = (UINT16*)(DrvScrollRAM + 0x00000);
	xscroll2 = (UINT16*)(DrvScrollRAM + 0x00400);
	yscroll2 = (UINT16*)(DrvScrollRAM + 0x00f00);
	yscroll1 = (UINT16*)(DrvScrollRAM + 0x00f80);
	SekMapMemory(DrvSprRAM,			0x310000, 0x310fff, MAP_RAM);
	SekSetWriteWordHandler(0,		citybomb_main_write_word);
	SekSetWriteByteHandler(0,		citybomb_main_write_byte);
	SekSetReadWordHandler(0,		nemesis_main_read_word);
	SekSetReadByteHandler(0,		citybomb_main_read_byte);
	SekMapHandler(2,			0x0e0000, 0x0e1fff, MAP_WRITE);
	SekSetWriteWordHandler(2, 		nemesis_palette_write_word);
	SekSetWriteByteHandler(2, 		nemesis_palette_write_byte);
	SekMapHandler(1,			0x200000, 0x20ffff, MAP_WRITE);
	SekSetWriteWordHandler(1, 		nemesis_charram_write_word);
	SekSetWriteByteHandler(1, 		nemesis_charram_write_byte);
	SekClose();

	CitybombSoundInit();

	palette_write = salamand_palette_update;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 NyanpaniInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(K007232ROM + 0x00000,  5, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM0,		0x040000, 0x047fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x060000, 0x061fff, MAP_RAM);
	SekMapMemory(Drv68KROM + 0x40000,	0x100000, 0x13ffff, MAP_ROM);
	SekMapMemory(DrvVidRAM0,		0x200000, 0x200fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x201000, 0x201fff, MAP_RAM);
	SekMapMemory(DrvColRAM0,		0x202000, 0x202fff, MAP_RAM);
	SekMapMemory(DrvColRAM1,		0x203000, 0x203fff, MAP_RAM);
	SekMapMemory(DrvCharRAM,		0x210000, 0x21ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x300000, 0x300fff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,		0x310000, 0x311fff, MAP_RAM);
	xscroll1 = (UINT16*)(DrvScrollRAM + 0x00000);
	xscroll2 = (UINT16*)(DrvScrollRAM + 0x00400);
	yscroll2 = (UINT16*)(DrvScrollRAM + 0x00f00);
	yscroll1 = (UINT16*)(DrvScrollRAM + 0x00f80);
	SekSetWriteWordHandler(0,		citybomb_main_write_word);
	SekSetWriteByteHandler(0,		citybomb_main_write_byte);
	SekSetReadWordHandler(0,		nemesis_main_read_word);
	SekSetReadByteHandler(0,		citybomb_main_read_byte);
	SekMapHandler(2,			0x060000, 0x061fff, MAP_WRITE);
	SekSetWriteWordHandler(2, 		nemesis_palette_write_word);
	SekSetWriteByteHandler(2, 		nemesis_palette_write_byte);
	SekMapHandler(1,			0x210000, 0x21ffff, MAP_WRITE);
	SekSetWriteWordHandler(1, 		nemesis_charram_write_word);
	SekSetWriteByteHandler(1, 		nemesis_charram_write_byte);
	SekClose();

	CitybombSoundInit();

	palette_write = salamand_palette_update;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 BubsysInit()
{
	is_bubble_system = 1;

	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM, 1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM, 3, 1)) return 1;

		if (BurnLoadRom(K005289ROM + 0x000, 4, 1)) return 1;
		if (BurnLoadRom(K005289ROM + 0x100, 5, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRAM0,			0x000000, 0x000fff, MAP_RAM);
	SekMapMemory(Drv68KRAM0 + 0x10000,	0x010000, 0x01ffff, MAP_RAM);
	SekMapMemory(DrvCharRAM,			0x030000, 0x03ffff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,			0x050000, 0x051fff, MAP_RAM);
	xscroll1 = (UINT16*)(DrvScrollRAM + 0x00000);
	xscroll2 = (UINT16*)(DrvScrollRAM + 0x00400);
	yscroll2 = (UINT16*)(DrvScrollRAM + 0x00f00);
	yscroll1 = (UINT16*)(DrvScrollRAM + 0x00f80);
	SekMapMemory(DrvVidRAM0,			0x052000, 0x052fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,			0x053000, 0x053fff, MAP_RAM);
	SekMapMemory(DrvColRAM0,			0x054000, 0x054fff, MAP_RAM);
	SekMapMemory(DrvColRAM1,			0x055000, 0x055fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,				0x056000, 0x056fff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,			0x057000, 0x057fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,				0x05a000, 0x05afff, MAP_RAM);
	SekMapMemory(Drv68KRAM1,			0x060000, 0x073fff, MAP_RAM);
	//SekMapMemory(??,					0x078000, 0x07ffff, MAP_ROM); // diagnostics rom (empty)

	SekSetWriteWordHandler(0,			gx400_main_write_word);
	SekSetWriteByteHandler(0,			gx400_main_write_byte);
	SekSetReadWordHandler(0,			nemesis_main_read_word);
	SekSetReadByteHandler(0,			gx400_main_read_byte);

	SekMapHandler(1,					0x030000, 0x03ffff, MAP_WRITE);
	SekSetWriteWordHandler(1, 			nemesis_charram_write_word);
	SekSetWriteByteHandler(1, 			nemesis_charram_write_byte);

	SekMapHandler(2,					0x05a000, 0x05afff, MAP_WRITE);
	SekSetWriteWordHandler(2, 			nemesis_palette_write_word);
	SekSetWriteByteHandler(2, 			nemesis_palette_write_byte);
	SekClose();

	Gx400SoundInit(0);

	palette_write = nemesis_palette_update;

	ay8910_enable = 1;
	k005289_enable = 1;
	vlm5030_enable = 1;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static void bubsys_twinbeeb_init()
{
	UINT8 *src = (UINT8*)BurnMalloc(0x50000);
	UINT8 *dst = (UINT8*)Drv68KROM;

	memcpy(src, dst,  0x50000);
	memset(dst, 0x00, 0x50000);

	for (INT32 i = 0; i < 0x806; i++)
	{
		INT32 sourcebase = i * 0x80;
		INT32 destbase = i * 0x90;

		for (int j = 0; j < 0x80; j++)
		{
			UINT8 dat = src[sourcebase + j];
			dst[destbase + j + 0] |= (dat >> 6) & 0x03;
			dst[destbase + j + 1] |= (dat << 2) & 0xfc;
		}

		for (int j = 0; j < 0x82; j += 2)
		{
			UINT8 temp1 = dst[destbase + j + 0];
			dst[destbase + j + 0] = dst[destbase + j + 1];
			dst[destbase + j + 1] = temp1;
		}

		dst[destbase+0x83] = i >> 8;
		dst[destbase+0x82] = i & 0xff;
	}
	BurnByteswap(dst, 0x50000);

	BurnFree(src);
}

static INT32 TwinbeebInit()
{
	INT32 nRet = BubsysInit();

	if (nRet == 0) {
		bubsys_twinbeeb_init();
	}

	return nRet;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	ZetExit();

	if (ay8910_enable) AY8910Exit(0);
	if (ay8910_enable) AY8910Exit(1);
	if (ym2151_enable) BurnYM2151Exit();
	if (ym3812_enable) BurnYM3812Exit();
	if (vlm5030_enable) vlm5030Exit();
	if (k007232_enable) K007232Exit();
	if (k005289_enable) K005289Exit();
	if (k051649_enable) K051649Exit();
	if (rcflt_enable) filter_rc_exit();
	if (bUseShifter) BurnShiftExit();
	bUseShifter = 0;

	BurnFreeMemIndex();

	palette_write = NULL;

	ay8910_enable = 0;
	ym2151_enable = 0;
	ym3812_enable = 0;
	k005289_enable = 0;
	k007232_enable = 0;
	k051649_enable = 0;
	vlm5030_enable = 0;
	vlm5030_enable_ram = 0;
	hcrash_mode = 0;
	is_bubble_system = 0;

	return 0;
}

static void draw_layer(UINT8 *vidram, UINT8 *colram, UINT16 *scrollx, UINT16 *scrolly, INT32 priority, INT32 tpri)
{
	UINT16 *vram = (UINT16*)vidram;
	UINT16 *cram = (UINT16*)colram;

	for (INT32 y = 0; y < 256; y++)
	{
		INT32 xscroll = (scrollx[y] & 0xff) | ((scrollx[y+0x100] & 1) * 256);

		INT32 sx_off = xscroll & 0x07;

		for (INT32 x = 0; x < nScreenWidth + (sx_off); x+=8)
		{
			INT32 scry = scrolly[x/8] & 0xff;

			INT32 offs = ((((scry/8)+(y/8)) & 0x1f) * 64) + (((x/8)+(xscroll/8)) & 0x3f);

			INT32 sy = y - (scry & 0x07);

			INT32 dy = sy;
			dy -= 16;
			if (dy < 0 || dy >= nScreenHeight) continue;

			if (*tilemap_flip_y) dy = (nScreenHeight - 1) - dy;

			INT32 code  = vram[offs];
			INT32 color = cram[offs];
			INT32 flipx = (color & 0x0080) ? 0x07 : 0;
			INT32 flipy = (code  & 0x0800) ? 0x38 : 0;
			INT32 mask  = (code  & 0x1000) >> 12;
			INT32 layer = (code  & 0x4000) >> 14;

			INT32 transparency = 0;

			if (mask && layer == 0) layer = 1;
			if ((code & 0x2000) == 0 || (code & 0xc000) == 0x4000) { transparency = 0x100; }

			if (priority != (mask + (layer<<1))) continue;

			if (code & 0xf800)
			{
				code = (code & 0x7ff) * 0x40;
				color = (color & 0x7f) * 0x10;

				UINT8 *rom = DrvCharRAMExp + code + (((y & 0x7)*8) ^ flipy);

				for (INT32 xx = 0; xx < 8; xx++)
				{
					int xxx = (xx + x) - sx_off;

					if (xxx >= 0 && xxx < nScreenWidth)
					{
						INT32 pxl = (rom[xx^flipx] & 0x0f);

						if (pxl!=transparency || transparency == 0x100) {
							pTransDraw[dy * nScreenWidth + xxx] = color + pxl;
							pPrioDraw[dy * nScreenWidth + xxx] |= tpri;
						}
					}
				}
			}
		}
	}
}

static void draw_sprites()
{
	static INT32 table[8][2] = {
		{ 32, 32 }, { 16, 32 }, { 32, 16 }, { 64, 64 }, { 8, 8 }, { 16, 8 }, { 8, 16 }, { 16, 16 }
	};

	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	for (INT32 priority = 256 - 1; priority >= 0; priority--)
	{
		for (INT32 address = (0x1000/2) - 8; address >= 0; address -= 8)
		{
			if((spriteram[address] & 0xff) != priority)
				continue;

			INT32 zoom = spriteram[address + 2] & 0xff;

			INT32 code;
			if (!(spriteram[address + 2] & 0xff00) && ((spriteram[address + 3] & 0xff00) != 0xff00))
				code = spriteram[address + 3] + ((spriteram[address + 4] & 0xc0) << 2);
			else
				code = (spriteram[address + 3] & 0xff) + ((spriteram[address + 4] & 0xc0) << 2);

			if (zoom != 0xff || code != 0)
			{

				INT32 size = spriteram[address + 1];
				zoom += (size & 0xc0) << 2;

				if (zoom == 0) continue;

				INT32 sx = spriteram[address + 5] & 0xff;
				INT32 sy = spriteram[address + 6] & 0xff;
				if (spriteram[address + 4] & 0x01)
					sx-=0x100;

				INT32 color = (spriteram[address + 4] & 0x1e) >> 1;
				INT32 flipx = spriteram[address + 1] & 0x01;
				INT32 flipy = spriteram[address + 4] & 0x20;

				INT32 w = table[(size >> 3) & 7][0];
				INT32 h = table[(size >> 3) & 7][1];
				code = code * 8 * 16 / (w * h);

				zoom = ((1 << 16) * 0x80 / zoom) + 0x02ab;

				if (*flipscreen)
				{
					sx = 256 - ((zoom * w) >> 16) - sx;
					sy = 256 - ((zoom * h) >> 16) - sy;
					flipx = !flipx;
					flipy = !flipy;
				}

				RenderZoomedPrioSprite(pTransDraw, DrvCharRAMExp, code, color*16, 0, sx, sy - 16, flipx, flipy, w, h, zoom, zoom, 0xffcc);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		if (palette_write != NULL) {
			for (INT32 i = 0; i < 0x2000; i+=2) { // a little hackish, but works for salamander & nemesis palette types
				palette_write(i);
			}

			DrvRecalc = 0;
		}
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer(DrvVidRAM1, DrvColRAM1, xscroll2, yscroll2, 0|0, 1); // bg
	if (nBurnLayer & 2) draw_layer(DrvVidRAM1, DrvColRAM1, xscroll2, yscroll2, 0|1, 2); // bg

	if (nBurnLayer & 4) draw_layer(DrvVidRAM0, DrvColRAM0, xscroll1, yscroll1, 0|0, 1); // fg
	if (nBurnLayer & 8) draw_layer(DrvVidRAM0, DrvColRAM0, xscroll1, yscroll1, 0|1, 2); // fg

	if (nSpriteEnable & 1) draw_layer(DrvVidRAM1, DrvColRAM1, xscroll2, yscroll2, 2|0, 1); // bg
	if (nSpriteEnable & 2) draw_layer(DrvVidRAM1, DrvColRAM1, xscroll2, yscroll2, 2|1, 2); // bg

	if (nSpriteEnable & 4) draw_layer(DrvVidRAM0, DrvColRAM0, xscroll1, yscroll1, 2|0, 1); // fg
	if (nSpriteEnable & 8) draw_layer(DrvVidRAM0, DrvColRAM0, xscroll1, yscroll1, 2|1, 2); // fg

	if (nSpriteEnable & 16) draw_sprites();

	BurnTransferCopy(DrvPalette);
	if (bUseShifter)
		BurnShiftRender();

	return 0;
}

static INT32 NemesisFrame()
{
	watchdog++;
	if (watchdog > 180) {
		DrvDoReset();
	}

	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset(DrvInputs, 0, 4 * sizeof(INT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 9216000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], nCyclesExtra[1] };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (i == 240 && *m68k_irq_enable)
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);

		CPU_RUN(1, Zet);
	}

	ZetClose();
	SekClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		K005289Update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 spinner_centered(INT32 no_last)
{
	UINT8 target = konamigt_read_wheel() / 4;

	INT32 ret = ((last_dial != DrvDial1 || no_last) && DrvDial1 == target && DrvDial1 == (0x80/4));

	last_dial = DrvDial1;

	return ret;
}

static void spinner_update(INT32 tweakaddr)
{ // spinner calculation stuff. (wheel) for rf2 / konamigt & hcrash -dink
	UINT8 INCREMENT = 0x02;
	UINT8 target = konamigt_read_wheel() / 4;

	if (DrvDial1+INCREMENT < target) { // go "right" in blocks of "INCREMENT"
		DrvDial1 += INCREMENT;
	}
	else
	if (DrvDial1 < target) { // take care of remainder
		DrvDial1++;
	}
	else
	if (DrvDial1-INCREMENT > target) { // go "left" in blocks of "INCREMENT"
		DrvDial1 -= INCREMENT;
	}
	else
	if (DrvDial1 > target) { // take care of remainder
		DrvDial1--;
	}

	// tweakaddr: UINT16 - computed steering value that the game uses:
	// far left  center  far right
	//   -0x34     0      0x34
	// It's computed from the analog input at 0x70000
	// Problem is, you can go, say, 10 clicks to the right, then come back
	// to center with 10 clicks left, but the game will not actually be centered..

	// this crafty hack will detect when the controller is center, then write
	// 0 to the tweakaddr, so the car will actually be driving straight.

	if (tweakaddr) {
		if (spinner_centered(0) && SekReadWord(tweakaddr)) {
			center_dial_timer = 5;
		}

		if (center_dial_timer) {
			center_dial_timer--;
			if (spinner_centered(1)) {
				//bprintf(0, _T("%d correcting wheel-center\n"), nCurrentFrame);
				SekWriteWord(tweakaddr, 0x0000);
			} else {
				center_dial_timer = 0;
			}
		}
	}
}

static INT32 KonamigtFrame()
{
	watchdog++;
	if (watchdog > 180) {
		DrvDoReset();
	}

	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset(DrvInputs, 0, 4 * sizeof(INT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		DrvInputs[1] = DrvInputs[1] & ~0x10;
		DrvInputs[1] |= ((BurnShiftInputCheckToggle(DrvJoy2[4])) ? 0x10 : 0x00);

		spinner_update(0x60a70);
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[2] = { 9216000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], nCyclesExtra[1] };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (*m68k_irq_enable && i == 240 && (nCurrentFrame & 1) == 0) {
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		}

		if (*m68k_irq_enable2 && i == 0) {
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		}

		CPU_RUN(1, Zet);
	}

	ZetClose();
	SekClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		K005289Update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 SalamandFrame()
{
	watchdog++;
	if (watchdog > 180) {
		DrvDoReset();
	}

	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset(DrvInputs, 0, 4 * sizeof(INT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		DrvInputs[0] |= (DrvDips[2] & 0x0a) << 4;
	//	DrvInputs[0] ^= 0x80; // service mode low
		DrvInputs[1] |= (DrvDips[2] & 0x80);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 9216000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == (nInterleave - 1) && *m68k_irq_enable) // should be 2500...
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);

		CPU_RUN_TIMER(1);
	}

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
		K007232Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 HcrashFrame()
{
	watchdog++;
	if (watchdog > 180) {
		DrvDoReset();
	}

	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset(DrvInputs, 0, 4 * sizeof(INT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] |= 1 << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		spinner_update(0);

	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 6144000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (*m68k_irq_enable && i == 240 && (nCurrentFrame & 1) == 0)
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);

		if (*m68k_irq_enable2 && i == 0)
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);

		CPU_RUN_TIMER(1);
	}

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
		K007232Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 BlkpnthrFrame()
{
	watchdog++;
	if (watchdog > 180) {
		DrvDoReset();
	}

	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset(DrvInputs, 0, 4 * sizeof(INT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		DrvInputs[0] |= (DrvDips[2] & 0xe0);
		DrvInputs[1] |= (DrvDips[2] & 0x08) << 4;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 9216000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (i == (nInterleave - 1) && *m68k_irq_enable)
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);

		CPU_RUN_TIMER(1);
	}

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		K007232Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 Gx400Frame()
{
	watchdog++;

	if (watchdog > 180) {
		DrvDoReset();
	}

	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset(DrvInputs, 0xff, 3 * sizeof(INT16));
		DrvInputs[3] = 0;
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (bUseShifter) { // rf2
			DrvInputs[0] &= ~0x20; // rf2 ??
			DrvInputs[1] = DrvInputs[1] & ~0x10;
			DrvInputs[1] |= ((BurnShiftInputCheckToggle(DrvJoy2[4])) ? 0x00 : 0x10); // different from the rest.

			spinner_update(0x1ea70);
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 9216000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], nCyclesExtra[1] };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (*m68k_irq_enable && (i == 240) && (nCurrentFrame & 1) == 0) // BUBSYS slightly different?
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);

		if (*m68k_irq_enable2 && (i == 0))
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);

		if (is_bubble_system) {
			if (scanline_counter++ >= 72) {
				scanline_counter = 0;
				if (*m68k_irq_enable4)
					SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
			}
		} else {
			if (*m68k_irq_enable4 && (i == 120))
				SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}

		CPU_RUN(1, Zet);
		if (i == (nInterleave - 1)) ZetNmi();
	}

	ZetClose();
	SekClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
		K005289Update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 CitybombFrame()
{
	watchdog++;
	if (watchdog > 180) {
		DrvDoReset();
	}

	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset(DrvInputs, 0xff, 4 * sizeof(INT16));
		DrvInputs[3] = 0;
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		DrvInputs[0] = (DrvInputs[0] & 0x1f) | ((DrvDips[2] & 0x0e)<<4);
		DrvInputs[1] = (DrvInputs[1] & 0x7f) | (DrvDips[2] & 0x80);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 9216000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (*m68k_irq_enable && (i == nInterleave - 1))
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);

		CPU_RUN_TIMER(1);
	}

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		K007232Update(0, pBurnSoundOut, nBurnSoundLen);
		K051649Update(pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();
	ZetClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnDraw) {
		DrvDraw();
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
		ZetScan(nAction);

		if (ym2151_enable) BurnYM2151Scan(nAction, pnMin);
		if (ym3812_enable) BurnYM3812Scan(nAction, pnMin);
		if (ay8910_enable) AY8910Scan(nAction, pnMin);
		if (k005289_enable) K005289Scan(nAction, pnMin);
		if (k007232_enable) K007232Scan(nAction, pnMin);
		if (k051649_enable) K051649Scan(nAction, pnMin);
		if (vlm5030_enable) vlm5030Scan(nAction, pnMin);
		if (vlm5030_enable_ram) ScanVar(DrvVLMROM, 0x800, "VLM5030 Ram");
		if (bUseShifter) BurnShiftScan(nAction);

		SCAN_VAR(scanline_counter);
		SCAN_VAR(selected_ip);
		SCAN_VAR(watchdog);
		SCAN_VAR(DrvDial1);
		SCAN_VAR(last_dial);
		SCAN_VAR(center_dial_timer);

		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_WRITE) {
		for (INT32 i = 0; i < 0x10000; i+=2) {
			update_char_tiles(i);
		}
	}

	return 0;
}


// Nemesis (ROM version)

static struct BurnRomInfo nemesisRomDesc[] = {
	{ "456-d01.12a",	0x08000, 0x35ff1aaa, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "456-d05.12c",	0x08000, 0x23155faa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "456-d02.13a",	0x08000, 0xac0cf163, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "456-d06.13c",	0x08000, 0x023f22a9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "456-d03.14a",	0x08000, 0x8cefb25f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "456-d07.14c",	0x08000, 0xd50b82cb, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "456-d04.15a",	0x08000, 0x9ca75592, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "456-d08.15c",	0x08000, 0x03c0b7f5, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "456-d09.9c",		0x04000, 0x26bf9636, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 Code

	{ "400-a01.fse",	0x00100, 0x5827b1e8, 3 | BRF_SND },           //  9 K005289 Samples
	{ "400-a02.fse",	0x00100, 0x2f44f970, 3 | BRF_SND },           // 10
};

STD_ROM_PICK(nemesis)
STD_ROM_FN(nemesis)

struct BurnDriver BurnDrvNemesis = {
	"nemesis", NULL, NULL, NULL, "1985",
	"Nemesis (ROM version)\0", NULL, "Konami", "GX400",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_HORSHOOT, 0,
	NULL, nemesisRomInfo, nemesisRomName, NULL, NULL, NULL, NULL, NemesisInputInfo, NemesisDIPInfo,
	NemesisInit, DrvExit, NemesisFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Nemesis (World?, ROM version)

static struct BurnRomInfo nemesisukRomDesc[] = {
	{ "456-e01.12a",	0x08000, 0xe1993f91, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "456-e05.12c",	0x08000, 0xc9761c78, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "456-e02.13a",	0x08000, 0xf6169c4b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "456-e06.13c",	0x08000, 0xaf58c548, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "456-e03.14a",	0x08000, 0x8cefb25f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "456-e07.14c",	0x08000, 0xd50b82cb, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "456-e04.15a",	0x08000, 0x322423d0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "456-e08.15c",	0x08000, 0xeb656266, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "456-b09.9c",		0x04000, 0x26bf9636, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 Code

	{ "400-a01.fse",	0x00100, 0x5827b1e8, 3 | BRF_SND },           //  9 K005289 Samples
	{ "400-a02.fse",	0x00100, 0x2f44f970, 3 | BRF_SND },           // 10
};

STD_ROM_PICK(nemesisuk)
STD_ROM_FN(nemesisuk)

struct BurnDriver BurnDrvNemesisuk = {
	"nemesisuk", "nemesis", NULL, NULL, "1985",
	"Nemesis (World?, ROM version)\0", NULL, "Konami", "GX400",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_HORSHOOT, 0,
	NULL, nemesisukRomInfo, nemesisukRomName, NULL, NULL, NULL, NULL, NemesisInputInfo, NemesisDIPInfo,
	NemesisInit, DrvExit, NemesisFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Konami GT

static struct BurnRomInfo konamigtRomDesc[] = {
	{ "561-c01.12a",	0x08000, 0x56245bfd, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "561-c05.12c",	0x08000, 0x8d651f44, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "561-c02.13a",	0x08000, 0x3407b7cb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "561-c06.13c",	0x08000, 0x209942d4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "561-b03.14a",	0x08000, 0xaef7df48, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "561-b07.14c",	0x08000, 0xe9bd6250, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "561-b04.15a",	0x08000, 0x94bd4bd7, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "561-b08.15c",	0x08000, 0xb7236567, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "561-b09.9c",		0x04000, 0x539d0c49, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 Code

	{ "400-a01.fse",	0x00100, 0x5827b1e8, 3 | BRF_SND },           //  9 K005289 Samples
	{ "400-a02.fse",	0x00100, 0x2f44f970, 3 | BRF_SND },           // 10
};

STD_ROM_PICK(konamigt)
STD_ROM_FN(konamigt)

struct BurnDriver BurnDrvKonamigt = {
	"konamigt", NULL, NULL, NULL, "1985",
	"Konami GT\0", NULL, "Konami", "GX561",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_RACING, 0,
	NULL, konamigtRomInfo, konamigtRomName, NULL, NULL, NULL, NULL, KonamigtInputInfo, KonamigtDIPInfo,
	KonamigtInit, DrvExit, KonamigtFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Salamander (version D)

static struct BurnRomInfo salamandRomDesc[] = {
	{ "587-d02.18b",	0x10000, 0xa42297f9, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "587-d05.18c",	0x10000, 0xf9130b0a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "587-c03.17b",	0x20000, 0xe5caf6e6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "587-c06.17c",	0x20000, 0xc2f567ea, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "587-d09.11j",	0x08000, 0x5020972c, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "587-d08.8g",		0x04000, 0xf9ac6b82, 4 | BRF_SND },           //  5 VLM5030 Samples

	{ "587-c01.10a",	0x20000, 0x09fe0632, 5 | BRF_SND },           //  6 K007232 Samples
};

STD_ROM_PICK(salamand)
STD_ROM_FN(salamand)

struct BurnDriver BurnDrvSalamand = {
	"salamand", NULL, NULL, NULL, "1986",
	"Salamander (version D)\0", NULL, "Konami", "GX587",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_HORSHOOT, 0,
	NULL, salamandRomInfo, salamandRomName, NULL, NULL, NULL, NULL, SalamandInputInfo, SalamandDIPInfo,
	SalamandInit, DrvExit, SalamandFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Salamander (version J)

static struct BurnRomInfo salamandjRomDesc[] = {
	{ "587-j02.18b",	0x10000, 0xf68ee99a, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "587-j05.18c",	0x10000, 0x72c16128, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "587-c03.17b",	0x20000, 0xe5caf6e6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "587-c06.17c",	0x20000, 0xc2f567ea, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "587-d09.11j",	0x08000, 0x5020972c, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "587-d08.8g",		0x04000, 0xf9ac6b82, 4 | BRF_SND },           //  5 VLM5030 Samples

	{ "587-c01.10a",	0x20000, 0x09fe0632, 5 | BRF_SND },           //  6 K007232 Samples
};

STD_ROM_PICK(salamandj)
STD_ROM_FN(salamandj)

struct BurnDriver BurnDrvSalamandj = {
	"salamandj", "salamand", NULL, NULL, "1986",
	"Salamander (version J)\0", NULL, "Konami", "GX587",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_HORSHOOT, 0,
	NULL, salamandjRomInfo, salamandjRomName, NULL, NULL, NULL, NULL, SalamandInputInfo, SalamandDIPInfo,
	SalamandInit, DrvExit, SalamandFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Salamander (Tecfri license)
// Original Konami PCBs PWB (B) 201012A GX587 + GX400PWD(A)200204C

static struct BurnRomInfo salamandtRomDesc[] = {
	// Same program ROMs content as 'salamand', but with smaller ROMs
	{ "5_27512.18b",			0x10000, 0xa42297f9, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "6_27512.18c",			0x10000, 0xf9130b0a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "10_27512.17b",			0x10000, 0xb83e8724, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4_27512.17c",			0x10000, 0xa6ef6dc4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "9_27512.17b",			0x10000, 0xb4d2fec9, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "3_27512.17c",			0x10000, 0x6ea59643, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "2_tmm24256ap.11j",		0x08000, 0x7eb8bb88, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code

	{ "1_27128.8g",				0x04000, 0xf9ac6b82, 4 | BRF_SND },           //  7 VLM5030 Samples

	{ "8_27512.10a",			0x10000, 0xcf477da4, 5 | BRF_SND },           //  8 K007232 Samples
	{ "7_27512.10a",			0x10000, 0x52384e79, 5 | BRF_SND },           //  9

	{ "007366_pal8l14a.19d",	0x00020, 0x77304735, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(salamandt)
STD_ROM_FN(salamandt)

struct BurnDriver BurnDrvsalamandt = {
	"salamandt", "salamand", NULL, NULL, "1986",
	"Salamander (Tecfri license)\0", NULL, "Konami (Tecfri license)", "GX587",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_HORSHOOT, 0,
	NULL, salamandtRomInfo, salamandtRomName, NULL, NULL, NULL, NULL, SalamandInputInfo, SalamandDIPInfo,
	SalamandInit, DrvExit, SalamandFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Lifeforce (US)

static struct BurnRomInfo lifefrceRomDesc[] = {
	{ "587-l02.18b",	0x10000, 0x4a44da18, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "587-l05.18c",	0x10000, 0x2f8c1cbd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "6107.17b",		0x20000, 0xe5caf6e6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6108.17c",		0x20000, 0xc2f567ea, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "587-k09.11j",	0x08000, 0x2255fe8c, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "587-k08.8g",		0x04000, 0x7f0e9b41, 4 | BRF_SND },           //  5 VLM5030 Samples

	{ "6106.10a",		0x20000, 0x09fe0632, 5 | BRF_SND },           //  6 K007232 Samples
};

STD_ROM_PICK(lifefrce)
STD_ROM_FN(lifefrce)

struct BurnDriver BurnDrvLifefrce = {
	"lifefrce", "salamand", NULL, NULL, "1986",
	"Lifeforce (US)\0", NULL, "Konami", "GX587",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_HORSHOOT, 0,
	NULL, lifefrceRomInfo, lifefrceRomName, NULL, NULL, NULL, NULL, SalamandInputInfo, SalamandDIPInfo,
	SalamandInit, DrvExit, SalamandFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Lifeforce (Japan)

static struct BurnRomInfo lifefrcejRomDesc[] = {
	{ "587-n02.18b",	0x10000, 0x235dba71, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "587-n05.18c",	0x10000, 0x054e569f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "587-n03.17b",	0x20000, 0x9041f850, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "587-n06.17c",	0x20000, 0xfba8b6aa, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "587-n09.11j",	0x08000, 0xe8496150, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "587-n08.8g",		0x04000, 0x7f0e9b41, 4 | BRF_SND },           //  5 VLM5030 Samples

	{ "587-c01.10a",	0x20000, 0x09fe0632, 5 | BRF_SND },           //  6 K007232 Samples
};

STD_ROM_PICK(lifefrcej)
STD_ROM_FN(lifefrcej)

struct BurnDriver BurnDrvLifefrcej = {
	"lifefrcej", "salamand", NULL, NULL, "1987",
	"Lifeforce (Japan)\0", NULL, "Konami", "GX587",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_HORSHOOT, 0,
	NULL, lifefrcejRomInfo, lifefrcejRomName, NULL, NULL, NULL, NULL, LifefrcejInputInfo, LifefrcejDIPInfo,
	SalamandInit, DrvExit, SalamandFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// TwinBee (ROM version)

static struct BurnRomInfo twinbeeRomDesc[] = {
	{ "400-a06.15l",	0x08000, 0xb99d8cff, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "400-a04.10l",	0x08000, 0xd02c9552, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "412-a07.17l",	0x20000, 0xd93c5499, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "412-a05.12l",	0x20000, 0x2b357069, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "400-e03.5l",		0x02000, 0xa5a8e57d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "400-a01.fse",	0x00100, 0x5827b1e8, 3 | BRF_SND },           //  5 K005289 Samples
	{ "400-a02.fse",	0x00100, 0x2f44f970, 3 | BRF_SND },           //  6
};

STD_ROM_PICK(twinbee)
STD_ROM_FN(twinbee)

struct BurnDriver BurnDrvTwinbee = {
	"twinbee", NULL, NULL, NULL, "1985",
	"TwinBee (ROM version)\0", NULL, "Konami", "GX412",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_VERSHOOT, 0,
	NULL, twinbeeRomInfo, twinbeeRomName, NULL, NULL, NULL, NULL, TwinbeeInputInfo, TwinbeeDIPInfo,
	Gx400Init, DrvExit, Gx400Frame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};


// Gradius (Japan, ROM version)

static struct BurnRomInfo gradiusRomDesc[] = {
	{ "400-a06.15l",	0x08000, 0xb99d8cff, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "400-a04.10l",	0x08000, 0xd02c9552, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "456-a07.17l",	0x20000, 0x92df792c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "456-a05.12l",	0x20000, 0x5cafb263, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "400-e03.5l",		0x02000, 0xa5a8e57d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "400-a01.fse",	0x00100, 0x5827b1e8, 3 | BRF_SND },           //  5 K005289 Samples
	{ "400-a02.fse",	0x00100, 0x2f44f970, 3 | BRF_SND },           //  6
};

STD_ROM_PICK(gradius)
STD_ROM_FN(gradius)

struct BurnDriver BurnDrvGradius = {
	"gradius", "nemesis", NULL, NULL, "1985",
	"Gradius (Japan, ROM version)\0", NULL, "Konami", "GX456",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_HORSHOOT, 0,
	NULL, gradiusRomInfo, gradiusRomName, NULL, NULL, NULL, NULL, GradiusInputInfo, GradiusDIPInfo,
	Gx400Init, DrvExit, Gx400Frame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Galactic Warriors

static struct BurnRomInfo gwarriorRomDesc[] = {
	{ "400-a06.15l",	0x08000, 0xb99d8cff, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "400-a04.10l",	0x08000, 0xd02c9552, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "578-a07.17l",	0x20000, 0x0aedacb5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "578-a05.12l",	0x20000, 0x76240e2e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "400-e03.5l",		0x02000, 0xa5a8e57d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "400-a01.fse",	0x00100, 0x5827b1e8, 3 | BRF_SND },           //  5 K005289 Samples
	{ "400-a02.fse",	0x00100, 0x2f44f970, 3 | BRF_SND },           //  6
};

STD_ROM_PICK(gwarrior)
STD_ROM_FN(gwarrior)

struct BurnDriver BurnDrvGwarrior = {
	"gwarrior", NULL, NULL, NULL, "1985",
	"Galactic Warriors\0", NULL, "Konami", "GX578",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_VSFIGHT, 0,
	NULL, gwarriorRomInfo, gwarriorRomName, NULL, NULL, NULL, NULL, GwarriorInputInfo, GwarriorDIPInfo,
	Gx400Init, DrvExit, Gx400Frame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Konami RF2 - Red Fighter

static struct BurnRomInfo rf2RomDesc[] = {
	{ "400-a06.15l",	0x08000, 0xb99d8cff, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "400-a04.10l",	0x08000, 0xd02c9552, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "561-a07.17l",	0x20000, 0xed6e7098, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "561-a05.12l",	0x20000, 0xdfe04425, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "400-e03.5l",		0x02000, 0xa5a8e57d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "400-a01.fse",	0x00100, 0x5827b1e8, 3 | BRF_SND },           //  5 K005289 Samples
	{ "400-a02.fse",	0x00100, 0x2f44f970, 3 | BRF_SND },           //  6
};

STD_ROM_PICK(rf2)
STD_ROM_FN(rf2)

struct BurnDriver BurnDrvRf2 = {
	"rf2", "konamigt", NULL, NULL, "1985",
	"Konami RF2 - Red Fighter\0", NULL, "Konami", "GX561",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_RACING, 0,
	NULL, rf2RomInfo, rf2RomName, NULL, NULL, NULL, NULL, KonamigtInputInfo, KonamigtDIPInfo,
	Rf2_gx400Init, DrvExit, Gx400Frame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Black Panther

static struct BurnRomInfo blkpnthrRomDesc[] = {
	{ "604-f02.18b",	0x10000, 0x487bf8da, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "604-f05.18c",	0x10000, 0xb08f8ca2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "604-c03.17b",	0x20000, 0x815bc3b0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "604-c06.17c",	0x20000, 0x4af6bf7f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "604-a08.11j",	0x08000, 0xaff88a2b, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "604-a01.10a",	0x20000, 0xeceb64a6, 5 | BRF_SND },           //  5 K007232 Samples
};

STD_ROM_PICK(blkpnthr)
STD_ROM_FN(blkpnthr)

struct BurnDriver BurnDrvBlkpnthr = {
	"blkpnthr", NULL, NULL, NULL, "1987",
	"Black Panther\0", NULL, "Konami", "GX604",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SCRFIGHT, 0,
	NULL, blkpnthrRomInfo, blkpnthrRomName, NULL, NULL, NULL, NULL, BlkpnthrInputInfo, BlkpnthrDIPInfo,
	BlkpnthrInit, DrvExit, BlkpnthrFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// City Bomber (World)

static struct BurnRomInfo citybombRomDesc[] = {
	{ "787-g10.15k",	0x10000, 0x26207530, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "787-g09.15h",	0x10000, 0xce7de262, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "787-g08.15f",	0x20000, 0x6242ef35, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "787-g07.15d",	0x20000, 0x21be5e9e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "787-e06.14f",	0x20000, 0xc251154a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "787-e05.14d",	0x20000, 0x0781e22d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "787-g04.13f",	0x20000, 0x137cf39f, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "787-g03.13d",	0x20000, 0x0cc704dc, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "787-e02.4h",		0x08000, 0xf4591e46, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 Code

	{ "787-e01.1k",		0x80000, 0xedc34d01, 5 | BRF_SND },           //  9 K007232 Samples
};

STD_ROM_PICK(citybomb)
STD_ROM_FN(citybomb)

struct BurnDriver BurnDrvCitybomb = {
	"citybomb", NULL, NULL, NULL, "1987",
	"City Bomber (World)\0", NULL, "Konami", "GX787",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SHOOT, 0,
	NULL, citybombRomInfo, citybombRomName, NULL, NULL, NULL, NULL, CitybombInputInfo, CitybombDIPInfo,
	CitybombInit, DrvExit, CitybombFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};


// City Bomber (Japan)

static struct BurnRomInfo citybombjRomDesc[] = {
	{ "787-h10.15k",	0x10000, 0x66fecf69, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "787-h09.15h",	0x10000, 0xa0e29468, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "787-g08.15f",	0x20000, 0x6242ef35, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "787-g07.15d",	0x20000, 0x21be5e9e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "787-e06.14f",	0x20000, 0xc251154a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "787-e05.14d",	0x20000, 0x0781e22d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "787-g04.13f",	0x20000, 0x137cf39f, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "787-g03.13d",	0x20000, 0x0cc704dc, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "787-e02.4h",		0x08000, 0xf4591e46, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 Code

	{ "787-e01.1k",		0x80000, 0xedc34d01, 5 | BRF_SND },           //  9 K007232 Samples
};

STD_ROM_PICK(citybombj)
STD_ROM_FN(citybombj)

struct BurnDriver BurnDrvCitybombj = {
	"citybombj", "citybomb", NULL, NULL, "1987",
	"City Bomber (Japan)\0", NULL, "Konami", "GX787",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_SHOOT, 0,
	NULL, citybombjRomInfo, citybombjRomName, NULL, NULL, NULL, NULL, CitybombInputInfo, CitybombDIPInfo,
	CitybombInit, DrvExit, CitybombFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};


// Kitten Kaboodle

static struct BurnRomInfo kittenkRomDesc[] = {
	{ "712-b10.15k",	0x10000, 0x8267cb2b, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "712-b09.15h",	0x10000, 0xeb41cfa5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "712-b08.15f",	0x20000, 0xe6d71611, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "712-b07.15d",	0x20000, 0x30f75c9f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "712-e02.4h",		0x08000, 0xba76f310, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "712-b01.1k",		0x80000, 0xf65b5d95, 5 | BRF_SND },           //  5 K007232 Samples
};

STD_ROM_PICK(kittenk)
STD_ROM_FN(kittenk)

struct BurnDriver BurnDrvKittenk = {
	"kittenk", NULL, NULL, NULL, "1988",
	"Kitten Kaboodle\0", NULL, "Konami", "GX712",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_PUZZLE, 0,
	NULL, kittenkRomInfo, kittenkRomName, NULL, NULL, NULL, NULL, NyanpaniInputInfo, NyanpaniDIPInfo,
	NyanpaniInit, DrvExit, CitybombFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Nyan Nyan Panic (Japan)

static struct BurnRomInfo nyanpaniRomDesc[] = {
	{ "712-j10.15k",	0x10000, 0x924b27ec, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "712-j09.15h",	0x10000, 0xa9862ea1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "712-b08.15f",	0x20000, 0xe6d71611, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "712-b07.15d",	0x20000, 0x30f75c9f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "712-e02.4h",		0x08000, 0xba76f310, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "712-b01.1k",		0x80000, 0xf65b5d95, 5 | BRF_SND },           //  5 K007232 Samples
};

STD_ROM_PICK(nyanpani)
STD_ROM_FN(nyanpani)

struct BurnDriver BurnDrvNyanpani = {
	"nyanpani", "kittenk", NULL, NULL, "1988",
	"Nyan Nyan Panic (Japan)\0", NULL, "Konami", "GX712",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_PUZZLE, 0,
	NULL, nyanpaniRomInfo, nyanpaniRomName, NULL, NULL, NULL, NULL, NyanpaniInputInfo, NyanpaniDIPInfo,
	NyanpaniInit, DrvExit, CitybombFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Hyper Crash (version D)

static struct BurnRomInfo hcrashRomDesc[] = {
	{ "790-d03.t9",		0x08000, 0x10177dce, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "790-d06.t7",		0x08000, 0xfca5ab3e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "790-c02.s9",		0x10000, 0x8ae6318f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "790-c05.s7",		0x10000, 0xc214f77b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "790-c09.n2",		0x08000, 0xa68a8cce, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "790-c08.j4",		0x08000, 0xcfb844bc, 4 | BRF_SND },           //  5 VLM5030 Samples

	{ "790-c01.m10",	0x20000, 0x07976bc3, 5 | BRF_SND },           //  6 K007232 Samples
};

STD_ROM_PICK(hcrash)
STD_ROM_FN(hcrash)

struct BurnDriver BurnDrvHcrash = {
	"hcrash", NULL, NULL, NULL, "1987",
	"Hyper Crash (version D)\0", NULL, "Konami", "GX790",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_RACING, 0,
	NULL, hcrashRomInfo, hcrashRomName, NULL, NULL, NULL, NULL, HcrashInputInfo, HcrashDIPInfo,
	HcrashInit, DrvExit, HcrashFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Hyper Crash (version C)

static struct BurnRomInfo hcrashcRomDesc[] = {
	{ "790-c03.t9",		0x08000, 0xd98ec625, 1 | BRF_PRG | BRF_ESS }, //  0 m68000 Code
	{ "790-c06.t7",		0x08000, 0x1d641a86, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "790-c02.s9",		0x10000, 0x8ae6318f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "790-c05.s7",		0x10000, 0xc214f77b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "790-c09.n2",		0x08000, 0xa68a8cce, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "790-c08.j4",		0x08000, 0xcfb844bc, 4 | BRF_SND },           //  5 VLM5030 Samples

	{ "790-c01.m10",	0x20000, 0x07976bc3, 5 | BRF_SND },          //  6 K007232 Samples
};

STD_ROM_PICK(hcrashc)
STD_ROM_FN(hcrashc)

struct BurnDriver BurnDrvHcrashc = {
	"hcrashc", "hcrash", NULL, NULL, "1987",
	"Hyper Crash (version C)\0", NULL, "Konami", "GX790",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_RACING, 0,
	NULL, hcrashcRomInfo, hcrashcRomName, NULL, NULL, NULL, NULL, HcrashInputInfo, HcrashDIPInfo,
	HcrashInit, DrvExit, HcrashFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Bubble System BIOS

static struct BurnRomInfo emptyRomDesc[] = { { "", 0, 0, 0 }, }; // For BIOS handling

static struct BurnRomInfo bubsysRomDesc[] = {
	{ "boot.bin",		0x01e0, 0xf0774fc2, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },              //  0 m68000 Vectors

	{ "mcu",			0x1000, 0x00000000, 2 | BRF_PRG | BRF_ESS | BRF_BIOS | BRF_NODUMP }, //  1 MCU Code

	{ "400b03.8g",		0x2000, 0x85c2afc5, 3 | BRF_PRG | BRF_ESS | BRF_BIOS },              //  2 Z80 Code

	{ "400a1.2b",		0x0100, 0x5827b1e8, 4 | BRF_SND | BRF_BIOS },                        //  3 K005289 Wavetables 
	{ "400a2.1b",		0x0100, 0x2f44f970, 4 | BRF_SND | BRF_BIOS },                        //  4
};

STD_ROM_PICK(bubsys)
STD_ROM_FN(bubsys)

struct BurnDriver BurnDrvBubsys = {
	"bubsys", NULL, NULL, NULL, "1985",
	"Bubble System BIOS\0", NULL, "Konami", "Bios Only",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 2, HARDWARE_KONAMI_68K_Z80, GBF_BIOS, 0,
	NULL, bubsysRomInfo, bubsysRomName, NULL, NULL, NULL, NULL, BubsysInputInfo, BubsysDIPInfo,
	BubsysInit, DrvExit, Gx400Frame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Gradius (Bubble System)

static struct BurnRomInfo gradiusbRomDesc[] = {
	{ "boot.bin",		0x001e0, 0xf0774fc2, 1 | BRF_PRG | BRF_ESS },              //  0 m68000 Vectors

	{ "gradius.bin",	0x48360, 0xf83b9607, 2 | BRF_PRG | BRF_ESS },              //  1 Bubble Memory Data

	{ "mcu",			0x01000, 0x00000000, 3 | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  2 MCU COde

	{ "400b03.8g",		0x02000, 0x85c2afc5, 4 | BRF_PRG | BRF_ESS },              //  3 Z80 Code

	{ "400a1.2b",		0x00100, 0x5827b1e8, 5 | BRF_SND },                        //  4 K005289 Wavetables
	{ "400a2.1b",		0x00100, 0x2f44f970, 5 | BRF_SND },                        //  5
};

STDROMPICKEXT(gradiusb, gradiusb, bubsys)
STD_ROM_FN(gradiusb)

struct BurnDriver BurnDrvGradiusb = {
	"gradiusb", NULL, "bubsys", NULL, "1985",
	"Gradius (Bubble System)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_HORSHOOT, 0,
	NULL, gradiusbRomInfo, gradiusbRomName, NULL, NULL, NULL, NULL, BubsysInputInfo, BubsysDIPInfo,
	BubsysInit, DrvExit, Gx400Frame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// TwinBee (Bubble System)

static struct BurnRomInfo twinbeebRomDesc[] = {
	{ "boot.bin",		0x001e0, 0xee6e93d7, 1 | BRF_PRG | BRF_ESS },              //  0 m68000 Vectors

	{ "twinbee.bin",	0x40300, 0x4d396a0a, 2 | BRF_PRG | BRF_ESS },              //  1 Bubble Memory Data

	{ "mcu",			0x01000, 0x00000000, 3 | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  2 MCU Code

	{ "400-e03.5l",		0x02000, 0xa5a8e57d, 4 | BRF_PRG | BRF_ESS },              //  3 Z80 Code

	{ "400-a01.fse",	0x00100, 0x5827b1e8, 5 | BRF_SND },                        //  4 K005289 Wavetables
	{ "400-a02.fse",	0x00100, 0x2f44f970, 5 | BRF_SND },                        //  5
};

STD_ROM_PICK(twinbeeb)
STD_ROM_FN(twinbeeb)

struct BurnDriver BurnDrvTwinbeeb = {
	"twinbeeb", NULL, NULL, NULL, "1985",
	"TwinBee (Bubble System)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KONAMI_68K_Z80, GBF_VERSHOOT, 0,
	NULL, twinbeebRomInfo, twinbeebRomName, NULL, NULL, NULL, NULL, BubsysInputInfo, BubsysDIPInfo,
	TwinbeebInit, DrvExit, Gx400Frame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};
