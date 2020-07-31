// FinalBurn Neo Eolith 32 bits hardware: Gradation 2D system driver module
// Based on MAME driver by Tomasz Slanina,Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "e132xs_intf.h"
#include "mcs51.h"
#include "qs1000.h"
#include "eeprom.h"
#include "burn_pal.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvBootROM;
static UINT8 *DrvI8032ROM;
static UINT8 *DrvQSROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvExtraRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 soundbank;
static INT32 soundlatch;
static INT32 vidrambank;

static UINT32 speedhack_address = 0;
static INT32 idle_cpu = 0;
static INT32 vblank;
static INT32 cpu_clock;

static INT32 uses_gun = 0; // hidctch3
static INT16 Analog[4];

static UINT8 DrvJoy1[32];
static UINT32 DrvInputs[1];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static struct BurnInputInfo LinkypipInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Linkypip)

static struct BurnInputInfo IronfortInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Ironfort)

static struct BurnInputInfo IronfortcInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Ironfortc)

static struct BurnInputInfo HidnctchInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 15,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Hidnctch)

static struct BurnInputInfo Hidctch2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 15,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Hidctch2)

static struct BurnInputInfo CandyInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 15,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Candy)

static struct BurnInputInfo RaccoonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Raccoon)

static struct BurnInputInfo LandbrkInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 15,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Landbrk)

static struct BurnInputInfo PenfanInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 15,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Penfan)

static struct BurnInputInfo StealseeInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 15,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Stealsee)

static struct BurnInputInfo PuzzlekgInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Puzzlekg)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo Hidctch3InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	A("P1 Stick X",     BIT_ANALOG_REL, Analog + 0,		"p1 x-axis"),
	A("P1 Stick Y",     BIT_ANALOG_REL, Analog + 1,		"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 16,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	A("P2 Stick X",     BIT_ANALOG_REL, Analog + 2,		"p2 x-axis"),
	A("P2 Stick Y",     BIT_ANALOG_REL, Analog + 3,		"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 24,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 15,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};
#undef A

STDINPUTINFO(Hidctch3)

static struct BurnDIPInfo LinkypipDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Don't Touch"				},
	{0x10, 0x01, 0x03, 0x00, "Fail (0)"					},
	{0x10, 0x01, 0x03, 0x01, "Fail (1)"					},
	{0x10, 0x01, 0x03, 0x02, "Fail (2)"					},
	{0x10, 0x01, 0x03, 0x03, "Working (3)"				},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x10, 0x01, 0x0c, 0x00, "4 Coins 1 Credits"		},
	{0x10, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
};

STDDIPINFO(Linkypip)

static struct BurnDIPInfo IronfortDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Show Dip Info"			},
	{0x10, 0x01, 0x01, 0x01, "Off"						},
	{0x10, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x10, 0x01, 0x02, 0x00, "Off"						},
	{0x10, 0x01, 0x02, 0x02, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x10, 0x01, 0x0c, 0x08, "1"						},
	{0x10, 0x01, 0x0c, 0x04, "2"						},
	{0x10, 0x01, 0x0c, 0x0c, "3"						},
	{0x10, 0x01, 0x0c, 0x00, "4"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x10, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x30, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x10, 0x01, 0xc0, 0x00, "Very Hard"				},
	{0x10, 0x01, 0xc0, 0x40, "Hard"						},
	{0x10, 0x01, 0xc0, 0xc0, "Normal"					},
	{0x10, 0x01, 0xc0, 0x80, "Easy"						},
};

STDDIPINFO(Ironfort)

static struct BurnDIPInfo IronfortcDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Show Dip Info"			},
	{0x10, 0x01, 0x01, 0x01, "Off"						},
	{0x10, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x10, 0x01, 0x02, 0x02, "Off"						},
	{0x10, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x10, 0x01, 0x0c, 0x08, "1"						},
	{0x10, 0x01, 0x0c, 0x04, "2"						},
	{0x10, 0x01, 0x0c, 0x0c, "3"						},
	{0x10, 0x01, 0x0c, 0x00, "4"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x10, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x10, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x10, 0x01, 0xc0, 0x00, "Very Hard"				},
	{0x10, 0x01, 0xc0, 0x40, "Hard"						},
	{0x10, 0x01, 0xc0, 0xc0, "Normal"					},
	{0x10, 0x01, 0xc0, 0x80, "Easy"						},
};

STDDIPINFO(Ironfortc)

static struct BurnDIPInfo HidnctchDIPList[]=
{
	{0x13, 0xff, 0xff, 0x03, NULL						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x13, 0x01, 0x01, 0x01, "Off"						},
	{0x13, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Show Counters"			},
	{0x13, 0x01, 0x02, 0x02, "Off"						},
	{0x13, 0x01, 0x02, 0x00, "On"						},
};

STDDIPINFO(Hidnctch)

static struct BurnDIPInfo Hidctch2DIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x13, 0x01, 0x01, 0x01, "Off"						},
	{0x13, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Show Counters"			},
	{0x13, 0x01, 0x02, 0x02, "Off"						},
	{0x13, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Skip ROM DATA Check"		},
	{0x13, 0x01, 0x08, 0x08, "Off"						},
	{0x13, 0x01, 0x08, 0x00, "On"						},
};

STDDIPINFO(Hidctch2)

static struct BurnDIPInfo CandyDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x13, 0x01, 0x01, 0x01, "Off"						},
	{0x13, 0x01, 0x01, 0x00, "On"						},
};

STDDIPINFO(Candy)

static struct BurnDIPInfo Hidctch3DIPList[]=
{
	{0x0c, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x0c, 0x01, 0x01, 0x01, "Off"						},
	{0x0c, 0x01, 0x01, 0x00, "On"						},
};

STDDIPINFO(Hidctch3)

static struct BurnDIPInfo RaccoonDIPList[]=
{
	{0x11, 0xff, 0xff, 0x0f, NULL						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x11, 0x01, 0x0f, 0x0d, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x0e, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x0c, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
};

STDDIPINFO(Raccoon)

static struct BurnDIPInfo LandbrkDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x12, 0x01, 0x01, 0x01, "Off"						},
	{0x12, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Show Counters"			},
	{0x12, 0x01, 0x02, 0x02, "Off"						},
	{0x12, 0x01, 0x02, 0x00, "On"						},
};

STDDIPINFO(Landbrk)

static struct BurnDIPInfo PenfanDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x12, 0x01, 0x08, 0x08, "Off"						},
	{0x12, 0x01, 0x08, 0x00, "On"						},
};

STDDIPINFO(Penfan)

static struct BurnDIPInfo StealseeDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL						},
};

STDDIPINFO(Stealsee)

static struct BurnDIPInfo PuzzlekgDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x10, 0x01, 0x0f, 0x0f, "Off"						},
	{0x10, 0x01, 0x0f, 0x00, "On"						},
};

STDDIPINFO(Puzzlekg)

static void set_vidrambank(INT32 data)
{
	if (vidrambank != (data & 0x80) >> 7)
	{
		vidrambank = (data & 0x80) >> 7;

		E132XSMapMemory(DrvVidRAM + (vidrambank * 0x40000), 0x90000000, 0x9003ffff, MAP_ROM);
	}
}

static inline void soundcpu_sync()
{
	INT32 todo = ((double)E132XSTotalCycles() * (1000000) / cpu_clock) - mcs51TotalCycles();

	if (todo > 0) mcs51Run(todo);
}

void Drvqs1000_serial_in(UINT8 data)
{
	// from soundcpu's serial port to qs1000's serialport.  we need tight sync.

	{ // bring qs1000's cpu up to soundcpu cycle-wise
		INT32 sndcpu_cyc = mcs51TotalCycles(); // calling from sndcpu.. (1)
		mcs51Close();
		mcs51Open(0);
		INT32 todo = ((double)sndcpu_cyc * (2000000) / 1000000) - mcs51TotalCycles();

		if (todo > 0) mcs51Run(todo);
		mcs51Close();
		mcs51Open(1); // back to sndcpu
	}

	// send data to qs1000's serial port
	qs1000_serial_in(data);
}

static void eolith_write_long(UINT32 address, UINT32 data)
{
	if ((address & 0xfffc0000) == 0x90000000) {
		UINT32 x = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvVidRAM + (address & 0x3fffc) + (vidrambank * 0x40000))));

		data = (data << 16) | (data >> 16);

		UINT32 xmask = ((data & 0x8000) ? 0xffff : 0) | ((data & 0x80000000) ? 0xffff0000 : 0);
		UINT32 dmask = xmask ^ 0xffffffff;

		*((UINT32*)(DrvVidRAM + (address & 0x3fffc) + (vidrambank * 0x40000))) = BURN_ENDIAN_SWAP_INT32((x & xmask) | (data & dmask));
		return;
	}

	switch (address)
	{
		case 0xfc400000:
			set_vidrambank(data);
			EEPROMWriteBit(data & 0x08);
			EEPROMSetCSLine((data & 0x02) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
			EEPROMSetClockLine((data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0xfc800000:
			soundcpu_sync();
			soundlatch = data;
			mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_HOLD);
		return;
	}
}

static void eolith_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffc0000) == 0x90000000) {
		UINT16 x = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM + (address & 0x3fffe) + (vidrambank * 0x40000))));

		UINT16 xmask = ((data & 0x8000) ? 0xffff : 0);
		UINT16 dmask = xmask ^ 0xffff;

		*((UINT16*)(DrvVidRAM + (address & 0x3fffe) + (vidrambank * 0x40000))) = BURN_ENDIAN_SWAP_INT16((x & xmask) | (data & dmask));
		return;
	}

	switch (address)
	{
		case 0xfc400000:
		case 0xfc400002:
			set_vidrambank(data);
			EEPROMWriteBit(data & 0x08);
			EEPROMSetCSLine((data & 0x02) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
			EEPROMSetClockLine((data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0xfc800000:
		case 0xfc800002:
			soundcpu_sync();
			soundlatch = data;
			mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_HOLD);
		return;
	}
}

static void eolith_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffc0000) == 0x90000000) {

		return;
	}

	switch (address & ~3)
	{
		case 0xfc400000:
			set_vidrambank(data);
			EEPROMWriteBit(data & 0x08);
			EEPROMSetCSLine((data & 0x02) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
			EEPROMSetClockLine((data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0xfc800000:
			soundcpu_sync();
			soundlatch = data;
			mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_HOLD);
		return;
	}
}

static UINT32 gun_read(INT32 playernum)
{
	UINT32 x = BurnGunReturnX(playernum) * 160 / 255;
	UINT32 y = BurnGunReturnY(playernum) * 120 / 255;
	return x + (y * 336);
}

static UINT32 eolith_read_long(UINT32 address)
{
	switch (address)
	{
		case 0xfc000000:
		{
			if (vblank == 0)
			{
				if (E132XSGetPC(0) == speedhack_address)
				{
					E132XSRunEndBurnAllCycles();
					idle_cpu = 1;
				}
			}

			return (DrvInputs[0] & ~0x348) | (BurnRandom() & 0x300) | (vblank ? 0 : 0x40) | (EEPROMRead() ? 8 : 0);
		}

		case 0xfca00000:
			return DrvDips[0] | 0xffffff00;

		case 0xfce00000:
		case 0xfce80000:
			return gun_read(0);

		case 0xfcf00000:
		case 0xfcf80000:
			return gun_read(1);
	}

	return 0;
}

static UINT16 eolith_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xfc000000:
			return eolith_read_long(0xfc000000);

		case 0xfc000002:
			return eolith_read_long(0xfc000000) >> 16;

		case 0xfca00000:
			return DrvDips[0] | 0xff00;

		case 0xfca00002:
			return 0xffff;
	}

	return 0;
}

static UINT8 eolith_read_byte(UINT32 address)
{
	// probably not used..

	return 0;
}

static UINT8 sound_read_port(INT32 port)
{
	if (port <= 0x7fff) {
		return DrvI8032ROM[0x10000 + port + (soundbank * 0x8000)];
	}

	switch (port)
	{
		case 0x8000:
			mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static void sound_write_port(INT32 port, UINT8 data)
{
	switch (port)
	{
		case MCS51_PORT_P1:
			soundbank = data & 0xf;
		return;
	}
}

static UINT8 qs1000_p1_read()
{
	return 1;
}

static void hidctch3_eeprom_new()
{
	UINT8 hidctch3_dat[] = {
		0xcb,0x06,0x03,0x02,0x00,0x01,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		0x00,0x02,0x00,0x02,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
	};

	EEPROMFill(hidctch3_dat, 0, 0x40);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	E132XSOpen(0);
	set_vidrambank(0);
	E132XSReset();
	E132XSClose();

	qs1000_reset();

	mcs51Open(1);
	mcs51_reset();
	mcs51_set_irq_line(MCS51_INT1_LINE, CPU_IRQSTATUS_ACK);
	mcs51Close();

	EEPROMReset();
	if (EEPROMAvailable() == 0 && uses_gun == 1) {
		hidctch3_eeprom_new();
	}

	soundbank = 0;
	soundlatch = 0;
	idle_cpu = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	= Next; Next += 0x2000000;
	DrvBootROM	= Next; Next += 0x080000;

	DrvI8032ROM	= Next; Next += 0x090000;

	DrvQSROM	= Next; Next += 0x080000;

	DrvSndROM	= Next; Next += 0x1000000;

	DrvPalette	= (UINT32*)Next; Next += 0x8000 * sizeof(UINT32);

	AllRam		= Next;

	DrvMainRAM	= Next; Next += 0x200000;
	DrvExtraRAM	= Next; Next += 0x200000;
	DrvVidRAM	= Next; Next += 0x080000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *mLoad = DrvMainROM;
	UINT8 *iLoad = DrvI8032ROM;
	UINT8 *sLoad = DrvSndROM;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (BurnLoadRomExt(DrvBootROM, i, 1, LD_BYTESWAP)) return 1;
			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRomExt(mLoad + 0, i+0, 4, LD_GROUP(2))) return 1;
			if (BurnLoadRomExt(mLoad + 2, i+1, 4, LD_GROUP(2))) return 1;
			mLoad += ri.nLen * 2;
			i++;
			continue;
		}

		if ((ri.nType & 7) == 3) {
			if (BurnLoadRom(iLoad, i, 1)) return 1;
			iLoad += 0x10000; // sound data loaded here

			continue;
		}

		if ((ri.nType & 7) == 4) {
			if (BurnLoadRom(DrvQSROM, i, 1)) return 1;
			continue;
		}

		if ((ri.nType & 7) == 5) {
			if (BurnLoadRom(sLoad, i, 1)) return 1;
			sLoad += ri.nLen;
			continue;
		}
	}

	return 0;
}

static eeprom_interface eeprom_interface_93C66 =
{
	9,				// address bits 9
	8,				// data bits    8
	"*110",			// read         110 aaaaaaaaa
	"*101",			// write        101 aaaaaaaaa dddddddd
	"*111",			// erase        111 aaaaaaaaa
	"*10000xxxxxx",	// lock         100 00xxxxxxx
	"*10011xxxxxx",	// unlock       100 11xxxxxxx
	0,0
};

static INT32 CommonInit(INT32 clock, void (*pLoadCallback)())
{
	BurnAllocMemIndex();

	if (DrvLoadRoms()) return 1;

	cpu_clock = clock;

	E132XSInit(0, TYPE_E132N, cpu_clock);
	E132XSOpen(0);
	E132XSMapMemory(DrvMainRAM,			0x00000000, 0x001fffff, MAP_RAM);
	E132XSMapMemory(DrvExtraRAM,		0x40000000, 0x401fffff, MAP_RAM);
//	E132XSMapMemory(DrvVidRAM,			0x90000000, 0x9003ffff, MAP_ROM); // handler ??
	E132XSMapMemory(DrvMainROM,			0xfd000000, 0xfeffffff, MAP_ROM);
	E132XSMapMemory(DrvBootROM,			0xfff80000, 0xffffffff, MAP_ROM);
	E132XSSetWriteLongHandler(eolith_write_long);
	E132XSSetWriteWordHandler(eolith_write_word);
	E132XSSetWriteByteHandler(eolith_write_byte);
	E132XSSetReadLongHandler(eolith_read_long);
	E132XSSetReadWordHandler(eolith_read_word);
	E132XSSetReadByteHandler(eolith_read_byte);
	E132XSClose();

	EEPROMInit(&eeprom_interface_93C66);

	qs1000_init(DrvQSROM, DrvSndROM, 0x1000000);
	qs1000_set_read_handler(1, qs1000_p1_read);
	qs1000_set_volume(0.50);

	i8052Init(1); // i8032
	mcs51Open(1);
	mcs51_set_program_data(DrvI8032ROM);
	mcs51_set_read_handler(sound_read_port);
	mcs51_set_write_handler(sound_write_port);
	mcs51_set_serial_tx_callback(Drvqs1000_serial_in);
	mcs51Close();

	GenericTilesInit();

	if (pLoadCallback) {
		pLoadCallback();
	}

	if (uses_gun) {
		BurnGunInit(2, 1);
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	E132XSExit();

	EEPROMExit();

	qs1000_exit();

	if (uses_gun) {
		BurnGunExit();
	}

	BurnFreeMemIndex();

	speedhack_address = 0;
	uses_gun = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x8000; i++)
	{
		DrvPalette[i] = BurnHighCol(pal5bit(i >> 10), pal5bit(i >> 5), pal5bit(i), 0);
	}
}

static void draw_bitmap()
{
	UINT16 *ram = (UINT16*)(DrvVidRAM + (vidrambank ^ 1) * 0x40000);
	UINT16 *dst = pTransDraw;

	for (INT32 y = 0; y < 240; y++)
	{
		for (INT32 x = 0; x < 320; x++)
		{
			dst[x] = BURN_ENDIAN_SWAP_INT16(ram[x]) & 0x7fff;
		}

		ram += 336;
		dst += 320;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_bitmap();

	BurnTransferCopy(DrvPalette);
	BurnGunDrawTargets();

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	E132XSNewFrame();
	mcs51NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

		if (uses_gun) { // hidctch3
			BurnGunMakeInputs(0, Analog[0], Analog[1]);
			BurnGunMakeInputs(1, Analog[2], Analog[3]);
			if (DrvJoy1[16]) {
				DrvInputs[0] &= 0xff00ffff;
			}
			if (DrvJoy1[24]) {
				DrvInputs[0] &= 0x00ffffff;
			}
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[3] = { cpu_clock / 60, 1000000 / 60, 2000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	E132XSOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		mcs51Open(1);
		if (idle_cpu) {
			CPU_IDLE(0, E132XS);
		} else {
			CPU_RUN(0, E132XS);
		}

		CPU_RUN_SYNCINT(1, mcs51); // sound rom
		mcs51Close();

		mcs51Open(0);
		CPU_RUN_SYNCINT(2, mcs51); // qs
		mcs51Close();

		if (i == 239) {
			vblank = 1;
			idle_cpu = 0;
		}
	}

	if (pBurnSoundOut) {
		qs1000_update(pBurnSoundOut, nBurnSoundLen);
	}

	E132XSClose();

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
		E132XSScan(nAction);
		mcs51_scan(nAction);

		qs1000_scan(nAction, pnMin);

		if (uses_gun) BurnGunScan();

		SCAN_VAR(soundbank);
		SCAN_VAR(vidrambank);
		SCAN_VAR(soundlatch);
	}

	if (nAction & ACB_WRITE) {
		E132XSOpen(0);
		set_vidrambank(vidrambank);
		E132XSClose();
	}

	EEPROMScan(nAction, pnMin);

	return 0;
}

static void patch_mcu_protection(UINT32 address)
{
	UINT32 *rombase = (UINT32*)DrvBootROM;
	UINT32 Temp = BURN_ENDIAN_SWAP_INT32(rombase[address/4]);
	rombase[address/4] = BURN_ENDIAN_SWAP_INT32((Temp & 0xffff0000) | 0x0300); // Change BR to NOP
}


// Iron Fortress

static struct BurnRomInfo ironfortRomDesc[] = {
	{ "u43",				0x080000, 0x29f55825, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "if00-00.u39",		0x400000, 0x63b74601, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "if00-01.u34",		0x400000, 0x890470b3, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "if00-02.u40",		0x400000, 0x63b5cca5, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "if00-03.u35",		0x400000, 0x54a76cb5, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "u111",				0x008000, 0x5d1d1387, 3 | BRF_PRG | BRF_ESS }, //  5 I8032 Code
	{ "u108",				0x080000, 0x89233144, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "u107",				0x008000, 0x89450a2f, 4 | BRF_PRG | BRF_ESS }, //  7 QS1000 Code

	{ "u97",				0x080000, 0x47b9d43a, 5 | BRF_SND },           //  8 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           //  9
};

STD_ROM_PICK(ironfort)
STD_ROM_FN(ironfort)

static INT32 IronfortInit()
{
	speedhack_address = 0x40020854;

	return CommonInit(49000000, NULL);
}

struct BurnDriver BurnDrvIronfort = {
	"ironfort", NULL, NULL, NULL, "1998",
	"Iron Fortress\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, ironfortRomInfo, ironfortRomName, NULL, NULL, NULL, NULL, IronfortInputInfo, IronfortDIPInfo,
	IronfortInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Gongtit Jiucoi Iron Fortress (Hong Kong)

static struct BurnRomInfo ironfortcRomDesc[] = {
	{ "u43.bin",			0x080000, 0xf1f19c9a, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "if00-00.u39",		0x400000, 0x63b74601, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "if00-01.u34",		0x400000, 0x890470b3, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "if00-02.u40",		0x400000, 0x63b5cca5, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "if00-03.u35",		0x400000, 0x54a76cb5, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "u111",				0x008000, 0x5d1d1387, 3 | BRF_PRG | BRF_ESS }, //  5 I8032 Code
	{ "u108",				0x080000, 0x89233144, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "u107",				0x008000, 0x89450a2f, 4 | BRF_PRG | BRF_ESS }, //  7 QS1000 Code

	{ "u97",				0x080000, 0x47b9d43a, 5 | BRF_SND },           //  8 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           //  9
};

STD_ROM_PICK(ironfortc)
STD_ROM_FN(ironfortc)

static INT32 IronfortcInit()
{
	speedhack_address = 0x40020234;

	return CommonInit(49000000, NULL);
}

struct BurnDriver BurnDrvIronfortc = {
	"ironfortc", "ironfort", NULL, NULL, "1998",
	"Gongtit Jiucoi Iron Fortress (Hong Kong)\0", NULL, "Eolith (Excellent Competence Ltd. license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, ironfortcRomInfo, ironfortcRomName, NULL, NULL, NULL, NULL, IronfortcInputInfo, IronfortcDIPInfo,
	IronfortcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Linky Pipe

static struct BurnRomInfo linkypipRomDesc[] = {
	{ "u43.bin",			0x080000, 0x9ef937de, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "lp00.u39",			0x400000, 0xc1e71bbc, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "lp01.u34",			0x400000, 0x30780c08, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "lp02.u40",			0x400000, 0x3381bd2c, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "lp03.u35",			0x400000, 0x459008f3, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "u111.bin",			0x008000, 0x52f419ea, 3 | BRF_PRG | BRF_ESS }, //  5 I8032 Code
	{ "u108.bin",			0x080000, 0xca65856f, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "u107.bin",			0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, //  7 QS1000 Code

	{ "u97.bin",			0x080000, 0x4465fe8d, 5 | BRF_SND },           //  8 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           //  9
};

STD_ROM_PICK(linkypip)
STD_ROM_FN(linkypip)

static INT32 LinkypipInit()
{
	speedhack_address = 0x4000825c;

	return CommonInit(45000000, NULL);
}

struct BurnDriver BurnDrvLinkypip = {
	"linkypip", NULL, NULL, NULL, "1998",
	"Linky Pipe\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, linkypipRomInfo, linkypipRomName, NULL, NULL, NULL, NULL, LinkypipInputInfo, LinkypipDIPInfo,
	LinkypipInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Hidden Catch (World) / Tul Lin Gu Lim Chat Ki '98 (Korea) (pcb ver 3.03)

static struct BurnRomInfo hidnctchRomDesc[] = {
	{ "hc_u43.bin",			0x080000, 0x635b4478, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "hc0_u39.bin",		0x400000, 0xeefb6add, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "hc1_u34.bin",		0x400000, 0x482f3e52, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "hc2_u40.bin",		0x400000, 0x914a1544, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "hc3_u35.bin",		0x400000, 0x80c59133, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "hc4_u41.bin",		0x400000, 0x9a9e2203, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "hc5_u36.bin",		0x400000, 0x74b1719d, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "hc_u111.bin",		0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, //  7 I8032 Code
	{ "hc_u108.bin",		0x080000, 0x2bae46cb, 3 | BRF_PRG | BRF_ESS }, //  8

	{ "hc_u107.bin",		0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, //  9 QS1000 Code

	{ "hc_u97.bin",			0x080000, 0xebf9f77b, 5 | BRF_SND },           // 10 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 11
};

STD_ROM_PICK(hidnctch)
STD_ROM_FN(hidnctch)

static INT32 HidnctchInit()
{
	speedhack_address = 0x4000bba0;

	return CommonInit(50000000, NULL);
}

struct BurnDriver BurnDrvHidnctch = {
	"hidnctch", NULL, NULL, NULL, "1998",
	"Hidden Catch (World) / Tul Lin Gu Lim Chat Ki '98 (Korea) (pcb ver 3.03)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hidnctchRomInfo, hidnctchRomName, NULL, NULL, NULL, NULL, HidnctchInputInfo, HidnctchDIPInfo,
	HidnctchInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Hidden Catch (World) / Tul Lin Gu Lim Chat Ki '98 (Korea) (pcb ver 3.02)

static struct BurnRomInfo hidnctchaRomDesc[] = {
	{ "3.02.u43",			0x080000, 0x9bb260a8, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "hc0_u39.bin",		0x400000, 0xeefb6add, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "hc1_u34.bin",		0x400000, 0x482f3e52, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "hc2_u40.bin",		0x400000, 0x914a1544, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "hc3_u35.bin",		0x400000, 0x80c59133, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "hc4_u41.bin",		0x400000, 0x9a9e2203, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "hc5_u36.bin",		0x400000, 0x74b1719d, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "hc_u111.bin",		0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, //  7 I8032 Code
	{ "hc_u108.bin",		0x080000, 0x2bae46cb, 3 | BRF_PRG | BRF_ESS }, //  8

	{ "hc_u107.bin",		0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, //  9 QS1000 Code

	{ "hc_u97.bin",			0x080000, 0xebf9f77b, 5 | BRF_SND },           // 10 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 11
};

STD_ROM_PICK(hidnctcha)
STD_ROM_FN(hidnctcha)

struct BurnDriver BurnDrvHidnctcha = {
	"hidnctcha", "hidnctch", NULL, NULL, "1998",
	"Hidden Catch (World) / Tul Lin Gu Lim Chat Ki '98 (Korea) (pcb ver 3.02)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hidnctchaRomInfo, hidnctchaRomName, NULL, NULL, NULL, NULL, HidnctchInputInfo, HidnctchDIPInfo,
	HidnctchInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// New Hidden Catch (World) / New Tul Lin Gu Lim Chat Ki '98 (Korea) (pcb ver 3.02)

static struct BurnRomInfo nhidctchRomDesc[] = {
	{ "u43",				0x080000, 0x44296fdb, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "hc000.u39",			0x400000, 0xeefb6add, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "hc001.u34",			0x400000, 0x482f3e52, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "hc002.u40",			0x400000, 0x914a1544, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "hc003.u35",			0x400000, 0x80c59133, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "hc004.u41",			0x400000, 0x9a9e2203, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "hc005.u36",			0x400000, 0x74b1719d, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "hc006.u42",			0x400000, 0x2ec58049, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "hc007.u37",			0x400000, 0x07e25def, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "u111",				0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, //  9 I8032 Code
	{ "u108",				0x080000, 0x2bae46cb, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "u107",				0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, // 11 QS1000 Code

	{ "u97",				0x080000, 0xebf9f77b, 5 | BRF_SND },           // 12 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 13
};

STD_ROM_PICK(nhidctch)
STD_ROM_FN(nhidctch)

static INT32 NhidctchInit()
{
	speedhack_address = 0x40012778;

	return CommonInit(50000000, NULL);
}

struct BurnDriver BurnDrvNhidctch = {
	"nhidctch", NULL, NULL, NULL, "1999",
	"New Hidden Catch (World) / New Tul Lin Gu Lim Chat Ki '98 (Korea) (pcb ver 3.02)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, nhidctchRomInfo, nhidctchRomName, NULL, NULL, NULL, NULL, Hidctch2InputInfo, Hidctch2DIPInfo,
	NhidctchInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Hidden Catch 2 (pcb ver 3.03) (Kor/Eng) (AT89c52 protected)

static struct BurnRomInfo hidctch2RomDesc[] = {
	{ "u43",				0x080000, 0x326d1dbc, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "00",					0x200000, 0xc6b1bc84, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "01",					0x200000, 0x5a1c1ab3, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "02",					0x200000, 0x3f7815aa, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03",					0x200000, 0xd686c59b, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "04",					0x200000, 0xd35cb515, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "05",					0x200000, 0x7870e5c6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "06",					0x200000, 0x10184a21, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "07",					0x200000, 0xb6c4879f, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "08",					0x200000, 0x670204d1, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "09",					0x200000, 0x28c0f55c, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "10",					0x200000, 0x45f374f4, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "11",					0x200000, 0xcac54db3, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "12",					0x200000, 0x66e681ff, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "13",					0x200000, 0x14bd38a9, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "14",					0x200000, 0x8eb1b01b, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "15",					0x200000, 0x3b06fe4e, 2 | BRF_PRG | BRF_ESS }, // 16

	{ "hc2.103",			0x000800, 0x92797034, 3 | BRF_PRG | BRF_ESS }, // 17 I8032 Code
	{ "u108",				0x080000, 0x75fc7a65, 3 | BRF_PRG | BRF_ESS }, // 18

	{ "u107",				0x008000, 0x89450a2f, 4 | BRF_PRG | BRF_ESS }, // 19 QS1000 Code

	{ "u97",				0x080000, 0xa7a1627e, 5 | BRF_SND },           // 20 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 21
};

STD_ROM_PICK(hidctch2)
STD_ROM_FN(hidctch2)

static void hidctch2_hack()
{
	patch_mcu_protection(0x0bcc8);
}

static INT32 Hidctch2Init()
{
	speedhack_address = 0x4001f6a0;

	return CommonInit(50000000, hidctch2_hack);
}

struct BurnDriver BurnDrvHidctch2 = {
	"hidctch2", NULL, NULL, NULL, "1999",
	"Hidden Catch 2 (pcb ver 3.03) (Kor/Eng) (AT89c52 protected)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hidctch2RomInfo, hidctch2RomName, NULL, NULL, NULL, NULL, Hidctch2InputInfo, Hidctch2DIPInfo,
	Hidctch2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Hidden Catch 2 (pcb ver 1.00) (Kor/Eng/Jpn/Chi)

static struct BurnRomInfo hidctch2aRomDesc[] = {
	{ "hc2j.u43",			0x080000, 0x8d3b8394, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "00",					0x200000, 0xc6b1bc84, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "01",					0x200000, 0x5a1c1ab3, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "02",					0x200000, 0x3f7815aa, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03",					0x200000, 0xd686c59b, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "04",					0x200000, 0xd35cb515, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "05",					0x200000, 0x7870e5c6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "06",					0x200000, 0x10184a21, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "07",					0x200000, 0xb6c4879f, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "08",					0x200000, 0x670204d1, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "09",					0x200000, 0x28c0f55c, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "10",					0x200000, 0x45f374f4, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "11",					0x200000, 0xcac54db3, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "12",					0x200000, 0x66e681ff, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "13",					0x200000, 0x14bd38a9, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "14",					0x200000, 0x8eb1b01b, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "15",					0x200000, 0x3b06fe4e, 2 | BRF_PRG | BRF_ESS }, // 16

	{ "hc2j.u111",			0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, // 17 I8032 Code
	{ "u108",				0x080000, 0x75fc7a65, 3 | BRF_PRG | BRF_ESS }, // 18

	{ "u107",				0x008000, 0x89450a2f, 4 | BRF_PRG | BRF_ESS }, // 19 QS1000 Code

	{ "u97",				0x080000, 0xa7a1627e, 5 | BRF_SND },           // 20 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 21
};

STD_ROM_PICK(hidctch2a)
STD_ROM_FN(hidctch2a)

static void hidctch2a_hack()
{
	patch_mcu_protection(0x17b2c);
}

static INT32 Hidctch2aInit()
{
	speedhack_address = 0x40029B58;

	return CommonInit(50000000, hidctch2a_hack);
}

struct BurnDriver BurnDrvHidctch2a = {
	"hidctch2a", "hidctch2", NULL, NULL, "1999",
	"Hidden Catch 2 (pcb ver 1.00) (Kor/Eng/Jpn/Chi)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hidctch2aRomInfo, hidctch2aRomName, NULL, NULL, NULL, NULL, Hidctch2InputInfo, Hidctch2DIPInfo,
	Hidctch2aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Hidden Catch 2000 (AT89c52 protected)

static struct BurnRomInfo hidnc2kRomDesc[] = {
	{ "27c040.u43",			0x080000, 0x05063136, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "hc2000-0.u39",		0x400000, 0x10d4fd9a, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "hc2000-1.u34",		0x400000, 0x6e029c0a, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "hc2000-2.u40",		0x400000, 0x1dc3fb7f, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "hc2000-3.u35",		0x400000, 0x665a884e, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "hc2000-4.u41",		0x400000, 0x242ec5b6, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "hc2000-5.u36",		0x400000, 0xf9c514cd, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "hc2000-6.u42",		0x400000, 0x8be32533, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "hc2000-7.u37",		0x400000, 0xbaa9eb90, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "sound.mcu",			0x000800, 0x92797034, 3 | BRF_PRG | BRF_ESS }, //  9 I8032 Code
	{ "27c4000.u108",		0x080000, 0x776c7906, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "275308.u107",		0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, // 11 QS1000 Code

	{ "27c040.u97",			0x080000, 0x0997a385, 5 | BRF_SND },           // 12 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 13

	{ "27c040.u1top",		0x080000, 0xd2fece37, 6 | BRF_SND },           // 14 oki
};

STD_ROM_PICK(hidnc2k)
STD_ROM_FN(hidnc2k)

static INT32 Hidnc2kInit()
{
	speedhack_address = 0x40016824;

	return CommonInit(50000000, hidctch2a_hack);
}

struct BurnDriver BurnDrvHidnc2k = {
	"hidnc2k", NULL, NULL, NULL, "1999",
	"Hidden Catch 2000 (AT89c52 protected)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hidnc2kRomInfo, hidnc2kRomName, NULL, NULL, NULL, NULL, Hidctch2InputInfo, Hidctch2DIPInfo,
	Hidnc2kInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Raccoon World

static struct BurnRomInfo raccoonRomDesc[] = {
	{ "racoon-u.43",		0x080000, 0x711ee026, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "racoon.u10",			0x200000, 0xf702390e, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "racoon.u1",			0x200000, 0x49775125, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "racoon.u11",			0x200000, 0x3f23f368, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "racoon.u2",			0x200000, 0x1eb00529, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "racoon.u14",			0x200000, 0x870fe45e, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "racoon.u5",			0x200000, 0x5fbac174, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "racoon-u.111",		0x008000, 0x52f419ea, 3 | BRF_PRG | BRF_ESS }, //  7 I8032 Code
	{ "racoon-u.108",		0x080000, 0xfc4f30ee, 3 | BRF_PRG | BRF_ESS }, //  8

	{ "racoon-u.107",		0x008000, 0x89450a2f, 4 | BRF_PRG | BRF_ESS }, //  9 QS1000 Code

	{ "racoon-u.97",		0x080000, 0xfef828b1, 5 | BRF_SND },           // 10 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 11
};

STD_ROM_PICK(raccoon)
STD_ROM_FN(raccoon)

static INT32 RaccoonInit()
{
	speedhack_address = 0x40008204;

	return CommonInit(45000000, NULL);
}

struct BurnDriver BurnDrvRaccoon = {
	"raccoon", NULL, NULL, NULL, "1998",
	"Raccoon World\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, raccoonRomInfo, raccoonRomName, NULL, NULL, NULL, NULL, RaccoonInputInfo, RaccoonDIPInfo,
	RaccoonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Land Breaker (World) / Miss Tang Ja Ru Gi (Korea) (pcb ver 3.02)

static struct BurnRomInfo landbrkRomDesc[] = {
	{ "rom3.u43",			0x080000, 0x8429189a, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "00.bin",				0x200000, 0xa803aace, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "01.bin",				0x200000, 0x439c95cc, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "02.bin",				0x200000, 0xa0c2828c, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03.bin",				0x200000, 0x5106b61a, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "04.bin",				0x200000, 0xfbe4a215, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "05.bin",				0x200000, 0x61d422b2, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "06.bin",				0x200000, 0xbc4f7f30, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "07.bin",				0x200000, 0xd4d108ce, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "08.bin",				0x200000, 0x36237654, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "09.bin",				0x200000, 0x9171828b, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "10.bin",				0x200000, 0xfe51744d, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "11.bin",				0x200000, 0xfe6fd5e0, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "12.bin",				0x200000, 0x9f0f17f3, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "13.bin",				0x200000, 0xc7d51bbe, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "14.bin",				0x200000, 0xf30f7bc5, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "15.bin",				0x200000, 0xbc1664e3, 2 | BRF_PRG | BRF_ESS }, // 16

	{ "rom1.u111",			0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, // 17 I8032 Code
	{ "rom2.u108",			0x080000, 0xf3b327ef, 3 | BRF_PRG | BRF_ESS }, // 18

	{ "lb.107",				0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, // 19 QS1000 Code

	{ "lb_3.u97",			0x080000, 0x5b34dff0, 5 | BRF_SND },           // 20 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 21
};

STD_ROM_PICK(landbrk)
STD_ROM_FN(landbrk)

static INT32 LandbrkInit()
{
	speedhack_address = 0x40023574;

	return CommonInit(45000000, NULL);
}

struct BurnDriver BurnDrvLandbrk = {
	"landbrk", NULL, NULL, NULL, "1999",
	"Land Breaker (World) / Miss Tang Ja Ru Gi (Korea) (pcb ver 3.02)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, landbrkRomInfo, landbrkRomName, NULL, NULL, NULL, NULL, LandbrkInputInfo, LandbrkDIPInfo,
	LandbrkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Land Breaker (World) / Miss Tang Ja Ru Gi (Korea) (pcb ver 3.03) (AT89c52 protected)

static struct BurnRomInfo landbrkaRomDesc[] = {
	{ "lb_1.u43",			0x080000, 0xf8bbcf44, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "lb2-000.u39",		0x400000, 0xb37faf7a, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "lb2-001.u34",		0x400000, 0x07e620c9, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "lb2-002.u40",		0x400000, 0x3bb4bca6, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "lb2-003.u35",		0x400000, 0x28ce863a, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "lb2-004.u41",		0x400000, 0xcbe84b06, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "lb2-005.u36",		0x400000, 0x350c77a3, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "lb2-006.u42",		0x400000, 0x22c57cd8, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "lb2-007.u37",		0x400000, 0x31f957b3, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "lb.103",				0x000800, 0x92797034, 3 | BRF_PRG | BRF_ESS }, //  9 I8032 Code
	{ "lb_2.108",			0x080000, 0xa99182d7, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "lb.107",				0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, // 11 QS1000 Code

	{ "lb_3.u97",			0x080000, 0x5b34dff0, 5 | BRF_SND },           // 12 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 13
};

STD_ROM_PICK(landbrka)
STD_ROM_FN(landbrka)

static void landbrka_hack()
{
	patch_mcu_protection(0x14f00);
}

static INT32 LandbrkaInit()
{
	speedhack_address = 0x4002446c;

	return CommonInit(45000000, landbrka_hack);
}

struct BurnDriver BurnDrvLandbrka = {
	"landbrka", "landbrk", NULL, NULL, "1999",
	"Land Breaker (World) / Miss Tang Ja Ru Gi (Korea) (pcb ver 3.03) (AT89c52 protected)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, landbrkaRomInfo, landbrkaRomName, NULL, NULL, NULL, NULL, LandbrkInputInfo, LandbrkDIPInfo,
	LandbrkaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Land Breaker (World) / Miss Tang Ja Ru Gi (Korea) (pcb ver 1.0) (AT89c52 protected)

static struct BurnRomInfo landbrkbRomDesc[] = {
	{ "lb_040.u43",			0x080000, 0xa81d681b, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "lb2-000.u39",		0x400000, 0xb37faf7a, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "lb2-001.u34",		0x400000, 0x07e620c9, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "lb2-002.u40",		0x400000, 0x3bb4bca6, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "lb2-003.u35",		0x400000, 0x28ce863a, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "lb2-004.u41",		0x400000, 0xcbe84b06, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "lb2-005.u36",		0x400000, 0x350c77a3, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "lb2-006.u42",		0x400000, 0x22c57cd8, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "lb2-007.u37",		0x400000, 0x31f957b3, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "lb.103",				0x000800, 0x92797034, 3 | BRF_PRG | BRF_ESS }, //  9 I8032 Code
	{ "lb_2.108",			0x080000, 0xa99182d7, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "lb.107",				0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, // 11 QS1000 Code

	{ "lb_3.u97",			0x080000, 0x5b34dff0, 5 | BRF_SND },           // 12 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 13
};

STD_ROM_PICK(landbrkb)
STD_ROM_FN(landbrkb)

static void landbrkb_hack()
{
	patch_mcu_protection(0x14da8);
}

static INT32 LandbrkbInit()
{
	speedhack_address = 0x40023B28;

	return CommonInit(45000000, landbrkb_hack);
}

struct BurnDriver BurnDrvLandbrkb = {
	"landbrkb", "landbrk", NULL, NULL, "1999",
	"Land Breaker (World) / Miss Tang Ja Ru Gi (Korea) (pcb ver 1.0) (AT89c52 protected)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, landbrkbRomInfo, landbrkbRomName, NULL, NULL, NULL, NULL, LandbrkInputInfo, LandbrkDIPInfo,
	LandbrkbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Penfan Girls - Step1. Mild Mind (set 1)

static struct BurnRomInfo penfanRomDesc[] = {
	{ "pfg.u43",			0x080000, 0x84977191, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "00.u5",				0x200000, 0x77ce2855, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "01.u1",				0x200000, 0xa7d7299d, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "02.u6",				0x200000, 0x79b05a66, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03.u2",				0x200000, 0xbddd9ad8, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "04.u7",				0x200000, 0x9fba61bd, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "05.u3",				0x200000, 0xed34befb, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "06.u8",				0x200000, 0xcc3dff54, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "07.u4",				0x200000, 0x9e33a480, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "08.u15",				0x200000, 0x89449f18, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "09.u10",				0x200000, 0x2b07cba0, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "10.u16",				0x200000, 0x738abbaa, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "11.u11",				0x200000, 0xddcd2bae, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "12.u17",				0x200000, 0x2eed0f64, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "13.u12",				0x200000, 0xcc3068a8, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "14.u18",				0x200000, 0x20a9a08e, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "15.u13",				0x200000, 0x872fa9c4, 2 | BRF_PRG | BRF_ESS }, // 16

	{ "pfg.u111",			0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, // 17 I8032 Code
	{ "pfg.u108",			0x080000, 0xac97c23b, 3 | BRF_PRG | BRF_ESS }, // 18

	{ "pfg.u107",			0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, // 19 QS1000 Code

	{ "pfg.u97",			0x080000, 0x0c713eef, 5 | BRF_SND },           // 20 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 21
};

STD_ROM_PICK(penfan)
STD_ROM_FN(penfan)

static INT32 PenfanInit()
{
	speedhack_address = 0x4001FA66;

	return CommonInit(45000000, NULL);
}

struct BurnDriver BurnDrvPenfan = {
	"penfan", NULL, NULL, NULL, "1999",
	"Penfan Girls - Step1. Mild Mind (set 1)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, penfanRomInfo, penfanRomName, NULL, NULL, NULL, NULL, PenfanInputInfo, PenfanDIPInfo,
	PenfanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Penfan Girls - Step1. Mild Mind (set 2)

static struct BurnRomInfo penfanaRomDesc[] = {
	{ "27c040.u43",			0x080000, 0xa7637f8b, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "00.u5",				0x200000, 0x77ce2855, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "01.u1",				0x200000, 0xa7d7299d, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "02.u6",				0x200000, 0x79b05a66, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03.u2",				0x200000, 0xbddd9ad8, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "04.u7",				0x200000, 0x9fba61bd, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "05.u3",				0x200000, 0xed34befb, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "06.u8",				0x200000, 0xcc3dff54, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "07.u4",				0x200000, 0x9e33a480, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "08.u15",				0x200000, 0x89449f18, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "09.u10",				0x200000, 0x2b07cba0, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "10.u16",				0x200000, 0x738abbaa, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "11.u11",				0x200000, 0xddcd2bae, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "12.u17",				0x200000, 0x2eed0f64, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "13.u12",				0x200000, 0xcc3068a8, 2 | BRF_PRG | BRF_ESS }, // 14

	{ "pfg.u111",			0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, // 15 I8032 Code
	{ "pfg.u108",			0x080000, 0xac97c23b, 3 | BRF_PRG | BRF_ESS }, // 16

	{ "pfg.u107",			0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, // 17 QS1000 Code

	{ "pfg.u97",			0x080000, 0x0c713eef, 5 | BRF_SND },           // 18 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 19
};

STD_ROM_PICK(penfana)
STD_ROM_FN(penfana)

static INT32 PenfanaInit()
{
	speedhack_address = 0x4001FAb6;

	return CommonInit(45000000, NULL);
}

struct BurnDriver BurnDrvPenfana = {
	"penfana", "penfan", NULL, NULL, "1999",
	"Penfan Girls - Step1. Mild Mind (set 2)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, penfanaRomInfo, penfanaRomName, NULL, NULL, NULL, NULL, PenfanInputInfo, PenfanDIPInfo,
	PenfanaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Steal See

static struct BurnRomInfo stealseeRomDesc[] = {
	{ "ss.u43",				0x080000, 0xb0a1a965, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "00.u5",				0x200000, 0x59d247a7, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "01.u1",				0x200000, 0x255764f2, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "02.u6",				0x200000, 0xebc33180, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03.u2",				0x200000, 0xa81f9b6d, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "04.u7",				0x200000, 0x24bf37ad, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "05.u3",				0x200000, 0xef6b9450, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "06.u8",				0x200000, 0x284629c6, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "07.u4",				0x200000, 0xa64d4434, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "08.u15",				0x200000, 0x861eab08, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "09.u10",				0x200000, 0xfd96282e, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "10.u16",				0x200000, 0x761e0a26, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "11.u11",				0x200000, 0x574571e7, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "12.u17",				0x200000, 0x9a4109e5, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "13.u12",				0x200000, 0x9a4109e5, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "14.u18",				0x200000, 0x9a4109e5, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "15.u13",				0x200000, 0x9a4109e5, 2 | BRF_PRG | BRF_ESS }, // 16

	{ "ss.u111",			0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, // 17 I8032 Code
	{ "ss.u108",			0x080000, 0x95bd136d, 3 | BRF_PRG | BRF_ESS }, // 18

	{ "ss.u107",			0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, // 19 QS1000 Code

	{ "ss.u97",				0x080000, 0x56c9f4a4, 5 | BRF_SND },           // 20 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 21
};

STD_ROM_PICK(stealsee)
STD_ROM_FN(stealsee)

static INT32 StealseeInit()
{
	speedhack_address = 0x400081ec;

	return CommonInit(45000000, NULL);
}

struct BurnDriver BurnDrvStealsee = {
	"stealsee", NULL, NULL, NULL, "2000",
	"Steal See\0", NULL, "Moov Generation / Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, stealseeRomInfo, stealseeRomName, NULL, NULL, NULL, NULL, StealseeInputInfo, StealseeDIPInfo,
	StealseeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Candy Candy

static struct BurnRomInfo candyRomDesc[] = {
	{ "cc.u43",				0x080000, 0x837c9967, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "00.u5",				0x200000, 0x04bc53e4, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "01.u1",				0x200000, 0x288229f1, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "02.u6",				0x200000, 0x5d3b130c, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03.u2",				0x200000, 0xe36cc85c, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "04.u7",				0x200000, 0x5090624c, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "05.u3",				0x200000, 0xc9e7c5d3, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "06.u8",				0x200000, 0xe3b65d10, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "07.u4",				0x200000, 0x5bf4cb29, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "08.u15",				0x200000, 0x296f3ec3, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "09.u10",				0x200000, 0x2ab8f847, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "10.u16",				0x200000, 0xf382e9e8, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "11.u11",				0x200000, 0x545ef2bf, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "12.u17",				0x200000, 0x8d125f1a, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "13.u12",				0x200000, 0xa10ea2e8, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "14.u18",				0x200000, 0x7cdd8639, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "15.u13",				0x200000, 0x8b0cf884, 2 | BRF_PRG | BRF_ESS }, // 16

	{ "cc.u111",			0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, // 17 I8032 Code
	{ "cc.u108",			0x080000, 0x6eedb497, 3 | BRF_PRG | BRF_ESS }, // 18

	{ "hc_u107.bin",		0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, // 19 QS1000 Code

	{ "cc.u97",				0x080000, 0x1400c878, 5 | BRF_SND },           // 20 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 21
};

STD_ROM_PICK(candy)
STD_ROM_FN(candy)

static INT32 CandyInit()
{
	speedhack_address = 0x4001990c;

	return CommonInit(50000000, NULL);
}

struct BurnDriver BurnDrvCandy = {
	"candy", NULL, NULL, NULL, "1999",
	"Candy Candy\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, candyRomInfo, candyRomName, NULL, NULL, NULL, NULL, CandyInputInfo, CandyDIPInfo,
	CandyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Fortress 2 Blue Arcade (World) (ver 1.01 / pcb ver 3.05)

static struct BurnRomInfo fort2bRomDesc[] = {
	{ "1.u43",				0x080000, 0xb2279485, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "00.u5",				0x200000, 0x4437b595, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "01.u1",				0x200000, 0x2a410aed, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "02.u6",				0x200000, 0x12f0e4c0, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03.u2",				0x200000, 0xaaa7c45a, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "04.u7",				0x200000, 0x428070d2, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "05.u3",				0x200000, 0xa66f9ba9, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "06.u8",				0x200000, 0x899d318e, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "07.u4",				0x200000, 0xc4644798, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "08.u15",				0x200000, 0xce0cccfc, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "09.u10",				0x200000, 0x5b7de0f1, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "10.u16",				0x200000, 0xb47fc014, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "11.u11",				0x200000, 0x7113d3f9, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "12.u17",				0x200000, 0x8c4b63a6, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "13.u12",				0x200000, 0x1d9b9995, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "14.u18",				0x200000, 0x450fa784, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "15.u13",				0x200000, 0xc1f02d5c, 2 | BRF_PRG | BRF_ESS }, // 16

	{ "4.u111",				0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, // 17 I8032 Code
	{ "3.u108",				0x080000, 0x9b996b60, 3 | BRF_PRG | BRF_ESS }, // 18

	{ "5.u107",				0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, // 19 QS1000 Code

	{ "2.u97",				0x080000, 0x8a431b14, 5 | BRF_SND },           // 20 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 21
};

STD_ROM_PICK(fort2b)
STD_ROM_FN(fort2b)

static INT32 Fort2bInit()
{
	speedhack_address = 0x000081e0;

	return CommonInit(50000000, NULL);
}

struct BurnDriver BurnDrvFort2b = {
	"fort2b", NULL, NULL, NULL, "2001",
	"Fortress 2 Blue Arcade (World) (ver 1.01 / pcb ver 3.05)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, fort2bRomInfo, fort2bRomName, NULL, NULL, NULL, NULL, PenfanInputInfo, NULL,
	Fort2bInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Fortress 2 Blue Arcade (Korea) (ver 1.00 / pcb ver 3.05)

static struct BurnRomInfo fort2baRomDesc[] = {
	{ "ftii012.u43",		0x080000, 0x6424e05f, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "ftii000.u39",		0x400000, 0xbe74121d, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "ftii004.u34",		0x400000, 0xd4399f98, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "ftii001.u40",		0x400000, 0x35c396ff, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "ftii005.u35",		0x400000, 0xff553679, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "ftii002.u41",		0x400000, 0x1d79ed5a, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "ftii006.u36",		0x400000, 0xc6049bbc, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "ftii003.u42",		0x400000, 0x3cac1efe, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "ftii007.u37",		0x400000, 0xa583a672, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "ftii008.u111",		0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, //  9 I8032 Code
	{ "ftii009.u108",		0x080000, 0x9b996b60, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "ftii010.u107",		0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, // 11 QS1000 Code

	{ "ftii011.u97",		0x080000, 0x8a431b14, 5 | BRF_SND },           // 12 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 13
};

STD_ROM_PICK(fort2ba)
STD_ROM_FN(fort2ba)

struct BurnDriver BurnDrvFort2ba = {
	"fort2ba", "fort2b", NULL, NULL, "2001",
	"Fortress 2 Blue Arcade (Korea) (ver 1.00 / pcb ver 3.05)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, fort2baRomInfo, fort2baRomName, NULL, NULL, NULL, NULL, PenfanInputInfo, NULL,
	Fort2bInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Puzzle King (Dance & Puzzle)

static struct BurnRomInfo puzzlekgRomDesc[] = {
	{ "u43.bin",			0x080000, 0xc3db7424, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "u10.bin",			0x200000, 0xc9c3064b, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "u1.bin",				0x200000, 0x6b4b369d, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "u11.bin",			0x200000, 0x92615236, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "u2.bin",				0x200000, 0xe76bbd1d, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "u14.bin",			0x200000, 0xf5aa39d1, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "u5.bin",				0x200000, 0x88bb70cf, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "u15.bin",			0x200000, 0xbcc1f74d, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "u6.bin",				0x200000, 0xab2248ff, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "u12.bin",			0x200000, 0x1794973d, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "u3.bin",				0x200000, 0x0980e877, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "u13.bin",			0x200000, 0x31de6d19, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "u4.bin",				0x200000, 0x2706f23c, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "u16.bin",			0x200000, 0xc2d09171, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "u7.bin",				0x200000, 0x52405e69, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "u17.bin",			0x200000, 0x234b7261, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "u8.bin",				0x200000, 0x8f4e50d7, 2 | BRF_PRG | BRF_ESS }, // 16

	{ "u111.bin",			0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, // 17 I8032 Code
	{ "u108.bin",			0x080000, 0xe4555c6b, 3 | BRF_PRG | BRF_ESS }, // 18

	{ "u107.bin",			0x008000, 0xf3add818, 4 | BRF_PRG | BRF_ESS }, // 19 QS1000 Code

	{ "u97.bin",			0x080000, 0xf4604ce8, 5 | BRF_SND },           // 20 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 21
};

STD_ROM_PICK(puzzlekg)
STD_ROM_FN(puzzlekg)

static INT32 PuzzlekgInit()
{
	speedhack_address = 0x40029458;

	return CommonInit(45000000, NULL);
}

struct BurnDriver BurnDrvPuzzlekg = {
	"puzzlekg", NULL, NULL, NULL, "1998",
	"Puzzle King (Dance & Puzzle)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, puzzlekgRomInfo, puzzlekgRomName, NULL, NULL, NULL, NULL, PuzzlekgInputInfo, PuzzlekgDIPInfo,
	PuzzlekgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Hidden Catch 3 (ver 1.00 / pcb ver 3.05)

static struct BurnRomInfo hidctch3RomDesc[] = {
	{ "u43",				0x080000, 0x7b861339, 1 | BRF_PRG | BRF_ESS }, //  0 E132N Boot ROM

	{ "00",					0x200000, 0x7fe5cd46, 2 | BRF_PRG | BRF_ESS }, //  1 E132N Main ROM
	{ "01",					0x200000, 0x09463ec9, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "02",					0x200000, 0xe5a08ebf, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "03",					0x200000, 0xb942b041, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "04",					0x200000, 0xd25256de, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "05",					0x200000, 0x6ea89d41, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "06",					0x200000, 0x67d3df6c, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "07",					0x200000, 0x74698a22, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "08",					0x200000, 0x0bf28c1f, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "09",					0x200000, 0x8a5960ce, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "10",					0x200000, 0x58999d26, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "11",					0x200000, 0x25f21007, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "12",					0x200000, 0xc39eba49, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "13",					0x200000, 0x20feaaf1, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "14",					0x200000, 0x6c042967, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "15",					0x200000, 0xa49c0834, 2 | BRF_PRG | BRF_ESS }, // 16

	{ "u111",				0x008000, 0x79012474, 3 | BRF_PRG | BRF_ESS }, // 17 I8032 Code
	{ "u108",				0x080000, 0x4a7de2e1, 3 | BRF_PRG | BRF_ESS }, // 18

	{ "u107",				0x008000, 0xafd5263d, 4 | BRF_PRG | BRF_ESS }, // 19 QS1000 Code

	{ "u97",				0x080000, 0x6d37aa1a, 5 | BRF_SND },           // 20 QS1000 Samples
	{ "qs1001a.u96",		0x080000, 0xd13c6407, 5 | BRF_SND },           // 21
};

STD_ROM_PICK(hidctch3)
STD_ROM_FN(hidctch3)

static INT32 Hidctch3Init()
{
	speedhack_address = 0x4001f6a0;

	uses_gun = 1;

	return CommonInit(45000000, NULL);
}

struct BurnDriver BurnDrvHidctch3 = {
	"hidctch3", NULL, NULL, NULL, "2000",
	"Hidden Catch 3 (ver 1.00 / pcb ver 3.05)\0", NULL, "Eolith", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hidctch3RomInfo, hidctch3RomName, NULL, NULL, NULL, NULL, Hidctch3InputInfo, Hidctch3DIPInfo,
	Hidctch3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};
