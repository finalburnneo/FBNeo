// FB Alpha Taito System-L driver module
// Based on MAME driver by Oliver Galibert

#include "tiles_generic.h"
#include "z80_intf.h"
#include "taito_ic.h"
#include "burn_ym2203.h"
#include "burn_ym2610.h"
#include "msm5205.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvMcuROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSampleROM;
static UINT8 *DrvGfxRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvShareRAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvCharRAM;
static UINT8 *DrvBgRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *cur_rombank;
static UINT8 *cur_rambank;
static UINT8 *char_banks;
static UINT8 *irq_adr_table;

static UINT8 horshoes_bank;
static UINT8 irq_enable;
static UINT8 last_irq_level;
static UINT8 current_control;
static UINT8 flipscreen;
static UINT8 mux_control;
static UINT8 mcu_position;

static UINT32 adpcm_pos;
static INT32 adpcm_data;

static INT32 nGfxRomLen = 0;
static INT32 nmi_enable = 0;
static INT32 has_ym2610 = 0;
static INT32 has_adpcm = 0;
static INT32 has_track = 0;

// trackball stuff
static INT32 DrvAnalogPort0 = 0;
static INT32 DrvAnalogPort1 = 0;
static UINT32 track_x = 0;
static UINT32 track_y = 0;
static INT32 track_x_last = 0;
static INT32 track_y_last = 0;

static INT32 fhawkmode = 0;
static INT32 plgirls2bmode = 0;

typedef void (*ram_function)(INT32 offset, UINT16 address, UINT8 data);
static ram_function ram_write_table[4] = { NULL, NULL, NULL, NULL };

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo FhawkInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 1,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 0,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Fhawk)

static struct BurnInputInfo ChampwrInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 7,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 6,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Champwr)

static struct BurnInputInfo EvilstonInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 1,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy2 + 0,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Evilston)

static struct BurnInputInfo PlottingInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy2 + 0,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Plotting)

static struct BurnInputInfo PuzznicInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Puzznic)

static struct BurnInputInfo PalamedInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Palamed)

static struct BurnInputInfo CachatInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"}	,
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Cachat)

static struct BurnInputInfo CubybopInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Cubybop)

static struct BurnInputInfo TubeitInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tubeit)

static struct BurnInputInfo PlgirlsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Plgirls)

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }
static struct BurnInputInfo HorshoesInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 2"	},

	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy2 + 0,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Horshoes)
#undef A

static struct BurnDIPInfo FhawkDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0x33, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x00, "Upright"		},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"		},

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
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x20, "4"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},
	{0x14, 0x01, 0x30, 0x00, "6"			},
};

STDDIPINFO(Fhawk)

static struct BurnDIPInfo FhawkjDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0x33, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x00, "Upright"		},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"		},

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
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x20, "4"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},
	{0x14, 0x01, 0x30, 0x00, "6"			},
};

STDDIPINFO(Fhawkj)

static struct BurnDIPInfo ChampwrDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xfc, NULL			},

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
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Time"			},
	{0x14, 0x01, 0x0c, 0x08, "2 minutes"		},
	{0x14, 0x01, 0x0c, 0x0c, "3 minutes"		},
	{0x14, 0x01, 0x0c, 0x04, "4 minutes"		},
	{0x14, 0x01, 0x0c, 0x00, "5 minutes"		},

	{0   , 0xfe, 0   ,    4, "'1 minute' Lasts:"	},
	{0x14, 0x01, 0x30, 0x00, "30 sec"		},
	{0x14, 0x01, 0x30, 0x10, "40 sec"		},
	{0x14, 0x01, 0x30, 0x30, "50 sec"		},
	{0x14, 0x01, 0x30, 0x20, "60 sec"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Champwr)

static struct BurnDIPInfo ChampwruDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xfc, NULL			},

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
	{0x13, 0x01, 0xc0, 0x00, "3 Coins 1 Credit"	},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credit"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coins 1 Credit"	},
	{0x13, 0x01, 0xc0, 0xc0, "Same as Start"	},

	{0   , 0xfe, 0   ,    4, "Time"			},
	{0x14, 0x01, 0x0c, 0x08, "2 minutes"		},
	{0x14, 0x01, 0x0c, 0x0c, "3 minutes"		},
	{0x14, 0x01, 0x0c, 0x04, "4 minutes"		},
	{0x14, 0x01, 0x0c, 0x00, "5 minutes"		},

	{0   , 0xfe, 0   ,    4, "'1 minute' Lasts:"	},
	{0x14, 0x01, 0x30, 0x00, "30 sec"		},
	{0x14, 0x01, 0x30, 0x10, "40 sec"		},
	{0x14, 0x01, 0x30, 0x30, "50 sec"		},
	{0x14, 0x01, 0x30, 0x20, "60 sec"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Champwru)

static struct BurnDIPInfo ChampwrjDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xfc, NULL			},

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
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credit"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credit"	},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credit"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credit"	},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Time"			},
	{0x14, 0x01, 0x0c, 0x08, "2 minutes"		},
	{0x14, 0x01, 0x0c, 0x0c, "3 minutes"		},
	{0x14, 0x01, 0x0c, 0x04, "4 minutes"		},
	{0x14, 0x01, 0x0c, 0x00, "5 minutes"		},

	{0   , 0xfe, 0   ,    4, "'1 minute' Lasts:"	},
	{0x14, 0x01, 0x30, 0x00, "30 sec"		},
	{0x14, 0x01, 0x30, 0x10, "40 sec"		},
	{0x14, 0x01, 0x30, 0x30, "50 sec"		},
	{0x14, 0x01, 0x30, 0x20, "60 sec"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Champwrj)

static struct BurnDIPInfo KurikintDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x00, "Upright"		},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"		},

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
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Bosses' messages"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x80, 0x80, "5 Times"		},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Kurikint)

static struct BurnDIPInfo KurikintjDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x00, "Upright"		},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"		},

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

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Bosses' messages"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x80, 0x80, "5 Times"		},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Kurikintj)

static struct BurnDIPInfo KurikintaDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfc, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x00, "Upright"		},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Level Select (Cheat)"	},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x14, 0x01, 0x02, 0x02, "Off"			},
	{0x14, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x0c, 0x08, "Easy"			},
	{0x14, 0x01, 0x0c, 0x0c, "Medium"		},
	{0x14, 0x01, 0x0c, 0x04, "Hard"			},
	{0x14, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x14, 0x01, 0x10, 0x10, "Off"			},
	{0x14, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Bosses' messages"	},
	{0x14, 0x01, 0x20, 0x00, "Off"			},
	{0x14, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Slow Motion (Cheat)"	},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Kurikinta)

static struct BurnDIPInfo EvilstonDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0x3f, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x00, "Upright"		},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"		},

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
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x20, "2"			},
	{0x14, 0x01, 0x30, 0x10, "1"			},
	{0x14, 0x01, 0x30, 0x00, "4"			},

	{0   , 0xfe, 0   ,    4, "Language"		},
	{0x14, 0x01, 0xc0, 0x00, "English"		},
	{0x14, 0x01, 0xc0, 0x80, "English"		},
	{0x14, 0x01, 0xc0, 0x40, "English"		},
	{0x14, 0x01, 0xc0, 0xc0, "Japanese"		},
};

STDDIPINFO(Evilston)

static struct BurnDIPInfo RaimaisDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0x7f, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x00, "Upright"		},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"		},

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
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x08, "80k and 160k"		},
	{0x14, 0x01, 0x0c, 0x0c, "80k only"		},
	{0x14, 0x01, 0x0c, 0x04, "160k only"		},
	{0x14, 0x01, 0x0c, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x20, "4"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},
	{0x14, 0x01, 0x30, 0x00, "6"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Raimais)

static struct BurnDIPInfo RaimaisjDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0x7f, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x00, "Upright"		},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"		},

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
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credit"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credit"	},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credit"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credit"	},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x08, "80k and 160k"		},
	{0x14, 0x01, 0x0c, 0x0c, "80k only"		},
	{0x14, 0x01, 0x0c, 0x04, "160k only"		},
	{0x14, 0x01, 0x0c, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x20, "4"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},
	{0x14, 0x01, 0x30, 0x00, "6"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Raimaisj)

static struct BurnDIPInfo PlottingDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Play Mode"		},
	{0x11, 0x01, 0x01, 0x00, "1 Player"		},
	{0x11, 0x01, 0x01, 0x01, "2 Player"		},

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
	{0x11, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x03, 0x02, "Easy"			},
	{0x12, 0x01, 0x03, 0x03, "Medium"		},
	{0x12, 0x01, 0x03, 0x01, "Hard"			},
	{0x12, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Wild Blocks"		},
	{0x12, 0x01, 0x30, 0x20, "1"			},
	{0x12, 0x01, 0x30, 0x30, "2"			},
	{0x12, 0x01, 0x30, 0x10, "3"			},
	{0x12, 0x01, 0x30, 0x00, "4"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Plotting)

static struct BurnDIPInfo PlottinguDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Play Mode"		},
	{0x11, 0x01, 0x01, 0x00, "1 Player"		},
	{0x11, 0x01, 0x01, 0x01, "2 Player"		},

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
	{0x11, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Price to Continue"	},
	{0x11, 0x01, 0xc0, 0x00, "3 Coins 1 Credit"	},
	{0x11, 0x01, 0xc0, 0x40, "2 Coins 1 Credit"	},
	{0x11, 0x01, 0xc0, 0x80, "1 Coin  1 Credit"	},
	{0x11, 0x01, 0xc0, 0xc0, "Same as Start"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x03, 0x02, "Easy"			},
	{0x12, 0x01, 0x03, 0x03, "Medium"		},
	{0x12, 0x01, 0x03, 0x01, "Hard"			},
	{0x12, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Wild Blocks"		},
	{0x12, 0x01, 0x30, 0x20, "1"			},
	{0x12, 0x01, 0x30, 0x30, "2"			},
	{0x12, 0x01, 0x30, 0x10, "3"			},
	{0x12, 0x01, 0x30, 0x00, "4"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Plottingu)

static struct BurnDIPInfo PuzznicDIPList[]=
{
	{0x11, 0xff, 0xff, 0xfe, NULL			},
	{0x12, 0xff, 0xff, 0xfc, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x01, 0x00, "Upright"		},
	{0x11, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x02, 0x02, "Off"			},
	{0x11, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x04, 0x04, "Off"			},
	{0x11, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x08, 0x00, "Off"			},
	{0x11, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x11, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Retries"		},
	{0x12, 0x01, 0x0c, 0x00, "0"			},
	{0x12, 0x01, 0x0c, 0x04, "1"			},
	{0x12, 0x01, 0x0c, 0x0c, "2"			},
	{0x12, 0x01, 0x0c, 0x08, "3"			},

	{0   , 0xfe, 0   ,    2, "Bombs"		},
	{0x12, 0x01, 0x10, 0x10, "0"			},
	{0x12, 0x01, 0x10, 0x00, "2"			},

	{0   , 0xfe, 0   ,    2, "Girls"		},
	{0x12, 0x01, 0x20, 0x00, "No"			},
	{0x12, 0x01, 0x20, 0x20, "Yes"			},

	{0   , 0xfe, 0   ,    3, "Terms of Replay"	},
	{0x12, 0x01, 0xc0, 0x40, "One step back/Continuous"		},
	{0x12, 0x01, 0xc0, 0xc0, "Reset to start/Continuous"		},
	{0x12, 0x01, 0xc0, 0x80, "Reset to start/Reset to start"	},
};

STDDIPINFO(Puzznic)

static struct BurnDIPInfo PalamedDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

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
	{0x13, 0x01, 0x30, 0x00, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x00, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Games for VS Victory"	},
	{0x14, 0x01, 0x0c, 0x08, "1 Game"		},
	{0x14, 0x01, 0x0c, 0x0c, "2 Games"		},
	{0x14, 0x01, 0x0c, 0x04, "3 Games"		},
	{0x14, 0x01, 0x0c, 0x00, "4 Games"		},

	{0   , 0xfe, 0   ,    4, "Dice Appear at"	},
	{0x14, 0x01, 0x30, 0x20, "500 Lines"		},
	{0x14, 0x01, 0x30, 0x30, "1000 Lines"		},
	{0x14, 0x01, 0x30, 0x10, "2000 Lines"		},
	{0x14, 0x01, 0x30, 0x00, "3000 Lines"		},

	{0   , 0xfe, 0   ,    2, "Versus Mode"		},
	{0x14, 0x01, 0x80, 0x00, "No"			},
	{0x14, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Palamed)

static struct BurnDIPInfo CachatDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfd, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x00, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},
};

STDDIPINFO(Cachat)

static struct BurnDIPInfo CubybopDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},
};

STDDIPINFO(Cubybop)

static struct BurnDIPInfo TubeitDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfd, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},
};

STDDIPINFO(Tubeit)

static struct BurnDIPInfo PlgirlsDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xfc, NULL			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x01, 0x00, "Off"			},
	{0x13, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    7, "Coinage"		},
	{0x13, 0x01, 0x38, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x18, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x38, 0x08, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "P1+P2 Start to Clear Round (Cheat)"	},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Plgirls)

static struct BurnDIPInfo Plgirls2DIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xfc, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},
	
	{0   , 0xfe, 0   ,    2, "Coin Mode"		},
	{0x13, 0x01, 0x08, 0x08, "Mode A"		},
	{0x13, 0x01, 0x08, 0x00, "Mode B"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Time"			},
	{0x14, 0x01, 0x04, 0x04, "2 Seconds"		},
	{0x14, 0x01, 0x04, 0x00, "3 Seconds"		},

	{0   , 0xfe, 0   ,    4, "Lives for Joe/Lady/Jack"	},
	{0x14, 0x01, 0x18, 0x10, "3/2/3"		},
	{0x14, 0x01, 0x18, 0x18, "4/3/4"		},
	{0x14, 0x01, 0x18, 0x08, "5/4/5"		},
	{0x14, 0x01, 0x18, 0x00, "6/5/6"		},

	{0   , 0xfe, 0   ,    2, "Character Speed"	},
	{0x14, 0x01, 0x20, 0x20, "Normal"		},
	{0x14, 0x01, 0x20, 0x00, "Fast"			},
};

STDDIPINFO(Plgirls2)

static struct BurnDIPInfo HorshoesDIPList[]=
{
	{0x08, 0xff, 0xff, 0xff, NULL			},
	{0x09, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Beer Frame Message"	},
	{0x08, 0x01, 0x01, 0x01, "Break Time"		},
	{0x08, 0x01, 0x01, 0x00, "Beer Frame"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x08, 0x01, 0x02, 0x02, "Off"			},
	{0x08, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x08, 0x01, 0x04, 0x04, "Off"			},
	{0x08, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x08, 0x01, 0x08, 0x00, "Off"			},
	{0x08, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x08, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x08, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x08, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x08, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Price to Continue"	},
	{0x08, 0x01, 0xc0, 0x00, "3 Coins 1 Credits"	},
	{0x08, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x08, 0x01, 0xc0, 0x80, "1 Coin  1 Credits"	},
	{0x08, 0x01, 0xc0, 0xc0, "Same as Start"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x09, 0x01, 0x03, 0x02, "Easy"			},
	{0x09, 0x01, 0x03, 0x03, "Medium"		},
	{0x09, 0x01, 0x03, 0x01, "Hard"			},
	{0x09, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Time"			},
	{0x09, 0x01, 0x0c, 0x08, "20 sec"		},
	{0x09, 0x01, 0x0c, 0x0c, "30 sec"		},
	{0x09, 0x01, 0x0c, 0x04, "40 sec"		},
	{0x09, 0x01, 0x0c, 0x00, "60 sec"		},

	{0   , 0xfe, 0   ,    2, "Innings"		},
	{0x09, 0x01, 0x10, 0x10, "3 per Credit"		},
	{0x09, 0x01, 0x10, 0x00, "9 per Credit"		},

	{0   , 0xfe, 0   ,    2, "Bonus Advantage"	},
	{0x09, 0x01, 0x20, 0x20, "Off"			},
	{0x09, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Scoring Speed"	},
	{0x09, 0x01, 0x40, 0x40, "Normal"		},
	{0x09, 0x01, 0x40, 0x00, "Fast"			},

	{0   , 0xfe, 0   ,    2, "Grip/Angle Select"	},
	{0x09, 0x01, 0x80, 0x80, "2 Buttons"		},
	{0x09, 0x01, 0x80, 0x00, "1 Button"		},
};

STDDIPINFO(Horshoes)

// decode a single byte written to the graphics ram
static inline void graphics_deocde_one(INT32 offset, UINT8 data)
{
	offset &= 0x7fff;
	INT32 k = (offset & 1) * 2;
	offset = (offset & ~1) * 2;

	for (INT32 i = 0; i < 8; i++)
	{
		DrvGfxROM2[offset + (i & 3)] &= ~(1 << ((i >> 2) + k));
		DrvGfxROM2[offset + (i & 3)] |= ((data >> i) & 1) << ((i >> 2) + k);
	}
}

// decode one palette entry
static inline void palette_update_one(INT32 offset)
{
	if (offset >= 0x200) return;

	offset &= 0x1fe;

	UINT8 r = DrvPalRAM[offset] & 0xf;
	UINT8 g = DrvPalRAM[offset] >> 4;
	UINT8 b = DrvPalRAM[offset+1] & 0xf;

	DrvPalette[offset/2] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
}

static void chargfx_write(INT32 offset, UINT16 address, UINT8 data)
{
	INT32 ram_bank = cur_rambank[offset];
	ram_bank = ((ram_bank & 3) + ((ram_bank & 8) >> 1)) << 12;
	address &= 0xfff;
	INT32 prev = DrvGfxRAM[address + ram_bank];

	if (data != prev) {
		DrvGfxRAM[address + ram_bank] = data;
		graphics_deocde_one(address + ram_bank, data);
	}
}

static void palette_write(INT32, UINT16 address, UINT8 data)
{
	INT32 prev = DrvPalRAM[address & 0xfff];

	if (data != prev) {
		DrvPalRAM[address & 0xfff] = data;

		palette_update_one(address & 0xffe);
	}
}

static void rambankswitch(UINT8 offset, UINT8 data)
{
	offset &= 3;

	INT32 ramaddr = 0xc000 + (offset * 0x1000);
	INT32 ramaddr_end = ramaddr + ((offset == 3) ? 0xdff : 0xfff);

	cur_rambank[offset] = data;

	switch (data)
	{
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
			ram_write_table[offset] = chargfx_write;
			ZetUnmapMemory(ramaddr, ramaddr_end, MAP_WRITE);
			ZetMapMemory(DrvGfxRAM + ((data & 3) * 0x1000), ramaddr, ramaddr_end, MAP_ROM);
		return;

		case 0x18:
		case 0x19:
			ZetMapMemory(DrvBgRAM + ((data & 1) * 0x1000), ramaddr, ramaddr_end, MAP_RAM);
		return;

		case 0x1a:
			ZetMapMemory(DrvCharRAM, ramaddr, ramaddr_end, MAP_RAM);
		return;

		case 0x1b:
			ZetMapMemory(DrvSprRAM, ramaddr, ramaddr_end, MAP_RAM);
		return;

		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x1f:
			ram_write_table[offset] = chargfx_write;
			ZetUnmapMemory(ramaddr, ramaddr_end, MAP_WRITE);
			ZetMapMemory(DrvGfxRAM + ((data & 3) * 0x1000) + 0x4000, ramaddr, ramaddr_end, MAP_ROM);
		return;

		case 0x80:
			ram_write_table[offset] = palette_write;
			ZetUnmapMemory(ramaddr, ramaddr_end, MAP_WRITE);
			ZetMapMemory(DrvPalRAM, ramaddr, ramaddr_end, MAP_ROM);
		return;

		default:
			ram_write_table[offset] = NULL;
			ZetUnmapMemory(ramaddr, ramaddr_end, MAP_RAM);
		break;
	}
}

static void rombankswitch0(UINT8 data)
{
	cur_rombank[0] = data;

	ZetMapMemory(DrvZ80ROM0 + (cur_rombank[0] * 0x2000), 0x6000, 0x7fff, MAP_ROM);
}

static void __fastcall fhawk_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0xc000 && address <= 0xfdff) {
		INT32 bank_select = (address >> 12) & 3;
		if (ram_write_table[bank_select]) {
			ram_write_table[bank_select](bank_select, address, data);
			return;
		}
	}

	switch (address)
	{
		case 0xfe00:
		case 0xfe01:
		case 0xfe02:
		case 0xfe03:
			char_banks[address & 3] = data;
		return;

		case 0xfe04:
			current_control = data;
			flipscreen = data & 0x10;
		return;

		case 0xff00:
		case 0xff01:
		case 0xff02:
			irq_adr_table[address & 3] = data;
		return;

		case 0xff03:
		{
			irq_enable = data;

			if ((irq_enable & (1 << last_irq_level)) == 0) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			}
		}
		return;

		case 0xff04:
		case 0xff05:
		case 0xff06:
		case 0xff07:
			rambankswitch(address, data);
		return;

		case 0xff08:
		case 0xfff8:
			rombankswitch0(data);
		return;
	}
}

static UINT8 __fastcall fhawk_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xfe00:
		case 0xfe01:
		case 0xfe02:
		case 0xfe03:
			return char_banks[address & 3];

		case 0xfe04:
			return current_control;
	
		case 0xff00:
		case 0xff01:
		case 0xff02:
			return irq_adr_table[address & 3];

		case 0xff03:
			return irq_enable;

		case 0xff04:
		case 0xff05:
		case 0xff06:
		case 0xff07:
			return cur_rambank[address & 3];

		case 0xff08:
		case 0xfff8:
			return cur_rombank[0];
	}

	return 0;
}

static void __fastcall evilston_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa800:
		return;	// nop

		case 0xa804:
		return; // nop

		case 0xfe04:
			current_control = data | 0x08; // hack!!!
			flipscreen = data & 0x10;
		return;
	}

	fhawk_main_write(address,data);
}

static UINT8 __fastcall evilston_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa800:
		case 0xa801:
			return DrvDips[address & 1];

		case 0xa802:
		case 0xa803:
			return DrvInputs[address & 1];

		case 0xa807:
			return DrvInputs[2];
	}

	return fhawk_main_read(address);
}

static UINT8 mux_read()
{
	switch (mux_control)
	{
		case 0:
		case 1:
			return DrvDips[mux_control & 1];

		case 2:
		case 3:
			return DrvInputs[mux_control & 1];

		case 7:
			return DrvInputs[2] ^ 0x0c;
	}

	return 0xff;
}

static void __fastcall raimais_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8800:
			if (mux_control == 4) {
				// coin counters, coin lockout
				return;
			}
		return;

		case 0x8801:
			mux_control = data;
		return;

		case 0x8c00:
			TC0140SYTPortWrite(data);
		return;

		case 0x8c01:
			ZetClose();
			TC0140SYTCommWrite(data);
			ZetOpen(0);
		return;
	}

	fhawk_main_write(address, data);
}

static UINT8 __fastcall raimais_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x8800:
			return mux_read();

		case 0x8801:
			return 0; // nop

		case 0x8c01:
			return TC0140SYTCommRead();
	}

	return fhawk_main_read(address);
}


static void __fastcall kurikint_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa800:
			if (mux_control == 4) {
				// coin counters, coin lockout
				return;
			}
		return;

		case 0xa801:
			mux_control = data;
		return;
	}

	fhawk_main_write(address,data);
}

static UINT8 __fastcall kurikint_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa800:
			return mux_read();

		case 0xa801:
			return 0;
	}

	return fhawk_main_read(address);
}

static void __fastcall plotting_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
		case 0xa002:
		case 0xa003:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0xa800:
		case 0xb800:
		return; // nop
	}

	fhawk_main_write(address,data);
}

static UINT8 __fastcall plotting_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
		case 0xa002:
		case 0xa003:
			mux_control = (address / 2) & 1;
			return BurnYM2203Read(0, address & 1);
	}

	return fhawk_main_read(address);
}

static void __fastcall puzznic_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
		case 0xa002:
		case 0xa003:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0xb800: // mcu data
			if (data == 0x43) mcu_position = 0;
		return;

		case 0xb801:
		return; // mcu control

		case 0xbc00:
		return; // nop
	}

	fhawk_main_write(address,data);
}

static UINT8 __fastcall puzznic_main_read(UINT16 address)
{
	static UINT8 mcu_reply[8] = { 0x50, 0x1f, 0xb6, 0xba, 0x06, 0x03, 0x47, 0x05 };

	switch (address)
	{
		case 0xa000:
		case 0xa001:
		case 0xa002:
		case 0xa003:
			mux_control = (address / 2) & 1;
			return BurnYM2203Read(0, address & 1);

		case 0xa800:
			return 0; // nop

		case 0xb800:
			if (mcu_position >= 8) return 0;
			return mcu_reply[mcu_position++]; // mcu data

		case 0xb801:
			return 1; // mcu status
	}

	return fhawk_main_read(address);
}


static void __fastcall palamed_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
		case 0xa002:
		case 0xa003:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0xa803:
		case 0xb000:
		return; // nop

	}

	fhawk_main_write(address,data);
}

static UINT8 __fastcall palamed_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
		case 0xa002:
		case 0xa003:
			mux_control = (address / 2) & 1;
			return BurnYM2203Read(0, address & 1);

		case 0xa800:
		case 0xa801:
		case 0xa802:
			return DrvInputs[address & 3];

		case 0xb001:
			return 0; // nop
	}

	return fhawk_main_read(address);
}

static void __fastcall horshoes_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
		case 0xa002:
		case 0xa003:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0xb801:
		case 0xbc00:
		return; // nop

		case 0xb802:
			horshoes_bank = data;
		return;

	}

	fhawk_main_write(address,data);
}

// horshoes trackball stuff
static UINT32 scalerange(UINT32 x, UINT32 in_min, UINT32 in_max, UINT32 out_min, UINT32 out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static UINT16 ananice(UINT16 anaval)
{
	UINT8 Temp = 0x7f - (anaval >> 4);
	if (Temp < 0x01) Temp = 0x01;
	if (Temp > 0xfe) Temp = 0xfe;
	UINT16 pad = scalerange(Temp, 0x3f, 0xc0, 0x01, 0xff);
	if (pad > 0xff) pad = 0xff;
	if (pad > 0x75 && pad < 0x85) pad = 0x7f;
	return pad;
}

static void trackball_tick()
{
	INT32 padx = ananice(DrvAnalogPort0) - 0x7f;
	track_x -= padx/2;  // (-=) reversed :)

	INT32 pady = ananice(DrvAnalogPort1) - 0x7f;
	track_y += pady/2;
}

static UINT8 __fastcall horshoes_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
		case 0xa002:
		case 0xa003:
			mux_control = (address / 2) & 1;
			return BurnYM2203Read(0, address & 1);

		case 0xa800:
			return (track_y - track_y_last) & 0xff; // tracky_lo_r

		case 0xa802: track_y_last = track_y;
			return 0; // tracky_reset_r

		case 0xa803: track_x_last = track_x;
			return 0; // trackx_reset_r

		case 0xa804:
			return (track_y - track_y_last) >> 8; // tracky_hi_r

		case 0xa808:
			return (track_x - track_x_last) & 0xff; // trackx_lo_r

		case 0xa80c:
			return (track_x - track_x_last) >> 8; // trackx_hi_r
	}

	return fhawk_main_read(address);
}

static void rombankswitch1(UINT8 data)
{
	cur_rombank[1] = data & 0xf;

	ZetMapMemory(DrvZ80ROM1 + (cur_rombank[1] * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall fhawk_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
			rombankswitch1(data);
		return;

		case 0xc800:
			TC0140SYTPortWrite(data);
		return;

		case 0xc801:
			ZetClose();
			TC0140SYTCommWrite(data);
			ZetOpen(1);
		return;

		case 0xd000:
		return;

		case 0xd004:
			// coin counters & lockout
		return;

		case 0xd005:
		return;	// nop
	}
}

static UINT8 __fastcall fhawk_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			return cur_rombank[1];

		case 0xc801:
			return TC0140SYTCommRead();

		case 0xd000:
		case 0xd001:
			return DrvDips[address & 1];

		case 0xd002:
		case 0xd003:
			return DrvInputs[address & 1];

		case 0xd007:
			return DrvInputs[2];
	}

	return 0;
}

static void __fastcall champwr_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
		return; // nop

		case 0xe004:
			// coin counters & lockout
		return;

		case 0xe800:
			TC0140SYTPortWrite(data);
		return;

		case 0xe801:
			ZetClose();
			TC0140SYTCommWrite(data);
			ZetOpen(1);
		return;

		case 0xf000:
			rombankswitch1(data);
		return;
	}
}

static UINT8 __fastcall champwr_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
		case 0xe001:
			return DrvDips[address & 1];

		case 0xe002:
		case 0xe003:
			return DrvInputs[address & 1];

		case 0xe007:
			return DrvInputs[2];

		case 0xe801:
			return TC0140SYTCommRead();

		case 0xf000:
			return cur_rombank[1];
	}

	return 0;
}

static void __fastcall fhawk_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			TC0140SYTSlavePortWrite(data);
		return;

		case 0xe001:
			TC0140SYTSlaveCommWrite(data);
		return;

		case 0xf000:
		case 0xf001:
			BurnYM2203Write(0, address & 1, data);
		return;
	}
}

static UINT8 __fastcall fhawk_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe001:
			return TC0140SYTSlaveCommRead();

		case 0xf000:
		case 0xf001:
			return BurnYM2203Read(0, address & 1);
	}

	return 0;
}

static void __fastcall champwr_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			TC0140SYTSlavePortWrite(data);
		return;

		case 0xa001:
			TC0140SYTSlaveCommWrite(data);
		return;

		case 0x9000:
		case 0x9001:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0xb000:
			adpcm_pos = ((adpcm_pos & 0x00ffff) | (data << 16)) & 0x1ffff;
		return;

		case 0xc000:
			adpcm_pos = (adpcm_pos & 0xff00ff) | (data << 8);
		return;

		case 0xd000:
			MSM5205ResetWrite(0,0);
		return;

		case 0xe000:
			MSM5205ResetWrite(0, 1);
			adpcm_pos &= 0x1ff00;
		return;
	}
}

static UINT8 __fastcall champwr_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa001:
			return TC0140SYTSlaveCommRead();

		case 0x9000:
		case 0x9001:
			return BurnYM2203Read(0, address & 1);
	}

	return 0;
}

static void __fastcall evilston_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe800:
		case 0xe801:
			BurnYM2203Write(0, address & 1, data);
		return;
	}
}

static UINT8 __fastcall evilston_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe800:
		case 0xe801:
			return BurnYM2203Read(0, address & 1);
	}

	return 0;
}

static void raimais_rombankswitch2(UINT8 data)
{
	cur_rombank[2] = data & 0x3;

	ZetMapMemory(DrvZ80ROM2 + (cur_rombank[2] * 0x4000), 0x4000, 0x7fff, MAP_ROM);
}

static void __fastcall raimais_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe003:
		case 0xe002:
		case 0xe001:
		case 0xe000:
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
		case 0xe403:
		case 0xe600:
		case 0xee00:
		case 0xf000:
		return;	// nop

		case 0xf200:
			raimais_rombankswitch2(data);
		return;
	}
}

static UINT8 __fastcall raimais_sound_read(UINT16 address)
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
	}
	
	return 0;
}

static void champwr_msm5205_vck()
{
	if (adpcm_data != -1)
	{
		MSM5205DataWrite(0, adpcm_data & 0x0f);
		adpcm_data = -1;
	}
	else
	{
		adpcm_data = DrvSampleROM[adpcm_pos];
		adpcm_pos = (adpcm_pos + 1) & 0x1ffff;
		MSM5205DataWrite(0, adpcm_data >> 4);
	}
}

static void rombankswitch2(INT32 data)
{
	if (ZetGetActive() == -1) return;
	cur_rombank[2] = data & 0x03;

	ZetMapMemory(DrvZ80ROM2 + (cur_rombank[2] * 0x4000), 0x4000, 0x7fff, MAP_ROM);
}

static void ym2203_write_portA(UINT32, UINT32 data)
{
	rombankswitch2(data);
}

static void champwr_ym2203_write_portB(UINT32,UINT32 data)
{
	MSM5205SetLeftVolume(0,	 data / 255.0);
	MSM5205SetRightVolume(0, data / 255.0);
}

static UINT8 ym2203_read_portA(UINT32)
{
	return DrvDips[mux_control];
}

static UINT8 ym2203_read_portB(UINT32)
{
	return DrvInputs[mux_control];
}

static UINT8 ym2203_read_portA1(UINT32)
{
	return DrvDips[0];
}

static UINT8 ym2203_read_portB1(UINT32)
{
	return DrvDips[1];
}

inline static void DrvIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 4000000;
}

inline static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 4000000.0;
}

inline static INT32 DrvSynchroniseStream2(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 6665280;
}

inline static double DrvGetTime2()
{
	return (double)ZetTotalCycles() / 6665280.0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	memset (DrvPalette, 0, 0x100 * sizeof(UINT32));
	memset (DrvGfxROM2, 0, 0x10000);
	memset (cur_rombank, 0xff, 3);

	ZetOpen(0);
	ZetReset();
	for (INT32 i = 0; i < 4; i++) {
		rambankswitch(i, 0xff); // unmap
	}
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	ZetOpen(2);
	ZetReset();
	if (has_ym2610) {
		BurnYM2610Reset();
	} else {
		BurnYM2203Reset();
	}
	if (has_adpcm) {
		MSM5205Reset();
	}
	ZetClose();

	TaitoICReset();

	horshoes_bank = 0;
	irq_enable = 0;
	last_irq_level = 0;
	current_control = 0;
	flipscreen = 0;
	mux_control = 0;
	mcu_position = 0;
	adpcm_pos = 0;
	adpcm_data = -1;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x100000;
	DrvZ80ROM1	= Next; Next += 0x020000;
	DrvZ80ROM2	= Next; Next += 0x020000;

	DrvMcuROM	= Next; Next += 0x000800;

	DrvGfxROM0	= Next; Next += nGfxRomLen * 2;
	DrvGfxROM1	= Next; Next += nGfxRomLen * 2;
	DrvGfxROM2	= Next; Next += 0x010000;

	DrvSampleROM	= Next; Next += 0x080000;

	DrvPalette	= (UINT32*)Next; Next += 0x00100 * sizeof(UINT32);

	AllRam		= Next;

	DrvGfxRAM	= Next; Next += 0x008000;

	DrvZ80RAM0	= Next; Next += 0x002000;
	DrvZ80RAM1	= Next; Next += 0x002000;
	DrvZ80RAM2	= Next; Next += 0x002000;
	DrvShareRAM1	= Next; Next += 0x002000;

	DrvPalRAM	= Next; Next += 0x001000;
	DrvSprRAM	= Next; Next += 0x001000;
	DrvSprBuf	= Next; Next += 0x000400;

	DrvCharRAM	= Next; Next += 0x0010000;

	DrvBgRAM	= Next; Next += 0x0020000;

	char_banks	= Next; Next += 0x000004;
	irq_adr_table	= Next; Next += 0x000003;
	cur_rombank	= Next; Next += 0x000003;
	cur_rambank	= Next; Next += 0x000004;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void DrvGetGfxRomLen()
{
	char* pRomName;
	struct BurnRomInfo ri;
	INT32 i,nGfxLen = 0;

	for (i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 4)
		{
			nGfxLen += ri.nLen;
		}
	}

	for (i = 16; i < 28; i++) {
		if (nGfxLen <= (1 << i)) {
			nGfxLen = 1 << i;
			break;
		}
	}	

	nGfxRomLen = nGfxLen;
}

static INT32 DrvGfxDecode(UINT8 *src, UINT8 *dst, UINT32 len, INT32 type)
{
	INT32 Plane[4]  = { 8, 12, 0, 4 };
	INT32 XOffs[16] = { STEP4(3,-1), STEP4(19,-1), STEP4(256+3,-1), STEP4(256+19,-1) };
	INT32 YOffs[16] = { STEP8(0,32), STEP8(512,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);
	if (tmp == NULL) {
		return 1;
	}

	INT32 size = 8 << type; // 8, 16

	memcpy (tmp, src, len);

	GfxDecode((len * 2) / (size * size), 4, size, size, Plane, XOffs, YOffs, (type) ? 0x400 : 0x100, tmp, dst);

	BurnFree (tmp);

	return 0;
}

static INT32 FhawkInit()
{
	DrvGetGfxRomLen();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x80000,  5, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, DrvGfxROM1, nGfxRomLen, 1);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, nGfxRomLen, 0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x5fff, MAP_ROM);
//	bank1 = 0x6000-7fff
	ZetMapMemory(DrvShareRAM1,	0x8000, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xa000, 0xbfff, MAP_RAM);
//	bank2 = 0xc000-cfff
//	bank3 = 0xd000-dfff
//	bank4 = 0xe000-efff
//	bank5 = 0xf000-fdff
	ZetSetWriteHandler(fhawk_main_write);
	ZetSetReadHandler(fhawk_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
//	bank6 - 0x8000-bfff
	ZetMapMemory(DrvShareRAM1,	0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(fhawk_sub_write);
	ZetSetReadHandler(fhawk_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x3fff, MAP_ROM);
//	bank7 - 0x4000-7fff
	ZetMapMemory(DrvZ80RAM2,	0x8000, 0x9fff, MAP_RAM);
	ZetSetWriteHandler(fhawk_sound_write);
	ZetSetReadHandler(fhawk_sound_read);
	ZetClose();

	BurnYM2203Init(1, 3000000, &DrvIRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnYM2203SetPorts(0, NULL, NULL, &ym2203_write_portA, NULL);
	BurnTimerAttachZet(4000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.80, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.20, BURN_SND_ROUTE_BOTH);

	has_adpcm = 0;

	fhawkmode = 1;

	TC0140SYTInit(2);

	DrvDoReset();

	GenericTilesInit();

	return 0;
}

static INT32 ChampwrInit()
{
	DrvGetGfxRomLen();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x80000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100000, 6, 1)) return 1;

		if (BurnLoadRom(DrvSampleROM,          7, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, DrvGfxROM1, nGfxRomLen, 1);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, nGfxRomLen, 0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x5fff, MAP_ROM);
//	bank1 = 0x6000-7fff
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvShareRAM1,	0xa000, 0xbfff, MAP_RAM);
//	bank2 = 0xc000-cfff
//	bank3 = 0xd000-dfff
//	bank4 = 0xe000-efff
//	bank5 = 0xf000-fdff
	ZetSetWriteHandler(fhawk_main_write);
	ZetSetReadHandler(fhawk_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
//	bank6 - 0x8000-bfff
	ZetMapMemory(DrvShareRAM1,	0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(champwr_sub_write);
	ZetSetReadHandler(champwr_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x3fff, MAP_ROM);
//	bank7 - 0x4000-7fff
	ZetMapMemory(DrvZ80RAM2,	0x8000, 0x8fff, MAP_RAM);
	ZetSetWriteHandler(champwr_sound_write);
	ZetSetReadHandler(champwr_sound_read);
	ZetClose();

	BurnYM2203Init(1, 3000000, &DrvIRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnYM2203SetPorts(0, NULL, NULL, &ym2203_write_portA, &champwr_ym2203_write_portB);
	BurnTimerAttachZet(4000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.80, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.20, BURN_SND_ROUTE_BOTH);

	has_adpcm = 1;
	MSM5205Init(0, DrvSynchroniseStream, 384000, champwr_msm5205_vck, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);
	MSM5205SetSeperateVolumes(0, 1);

	TC0140SYTInit(2);

	DrvDoReset();

	GenericTilesInit();

	return 0;
}

static INT32 EvilstonInit()
{
	DrvGetGfxRomLen();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x80000,  4, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, DrvGfxROM1, nGfxRomLen, 1);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, nGfxRomLen, 0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x5fff, MAP_ROM);
//	bank1 = 0x6000-7fff
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvShareRAM1,	0xa000, 0xa7ff, MAP_RAM);
//	bank2 = 0xc000-cfff
//	bank3 = 0xd000-dfff
//	bank4 = 0xe000-efff
//	bank5 = 0xf000-fdff
	ZetSetWriteHandler(evilston_main_write);
	ZetSetReadHandler(evilston_main_read);
	ZetClose();

	ZetInit(1); // not used

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvShareRAM1,	0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM2+0xf000,	0xf000, 0xf7ff, MAP_ROM); // wtf?
//	bank7 - 0xf000-f7ff
	ZetSetWriteHandler(evilston_sound_write);
	ZetSetReadHandler(evilston_sound_read);
	ZetClose();

	BurnYM2203Init(1, 3000000, NULL, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.80, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.20, BURN_SND_ROUTE_BOTH);

	nmi_enable = 1;

	TC0140SYTInit(2); // not used

	DrvDoReset();

	GenericTilesInit();

	return 0;
}

static INT32 RaimaisInit()
{
	DrvGetGfxRomLen();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x80000,  5, 1)) return 1;

		if (BurnLoadRom(DrvSampleROM,          6, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, DrvGfxROM1, nGfxRomLen, 1);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, nGfxRomLen, 0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x5fff, MAP_ROM);
//	bank1 = 0x6000-7fff
	ZetMapMemory(DrvShareRAM1,	0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xa000, 0xbfff, MAP_RAM);
//	bank2 = 0xc000-cfff
//	bank3 = 0xd000-dfff
//	bank4 = 0xe000-efff
//	bank5 = 0xf000-fdff
	ZetSetWriteHandler(raimais_main_write);
	ZetSetReadHandler(raimais_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvShareRAM1,	0xe000, 0xe7ff, MAP_RAM);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x3fff, MAP_ROM);
//	bank7 - 0x4000-7fff
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(raimais_sound_write);
	ZetSetReadHandler(raimais_sound_read);
	ZetClose();

	has_ym2610 = 1;
	INT32 DrvSndROMLen = 0x80000;
	BurnYM2610Init(8000000, DrvSampleROM, &DrvSndROMLen, DrvSampleROM, &DrvSndROMLen, &DrvIRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 0.25, BURN_SND_ROUTE_BOTH);

	TC0140SYTInit(2);

	DrvDoReset();

	GenericTilesInit();

	return 0;
}

static INT32 KurikintCommonInit(INT32 loadtype)
{
	DrvGetGfxRomLen();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (loadtype == 0) // Kurikint
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x80000,  4, 1)) return 1;
	}
	else		// Kurikinta
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x40000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x80000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0xc0000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x00001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x40001,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x80001,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0xc0001, 10, 2)) return 1;
	}

	DrvGfxDecode(DrvGfxROM0, DrvGfxROM1, nGfxRomLen, 1);
	DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, nGfxRomLen, 0);

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x5fff, MAP_ROM);
//	bank1 = 0x6000-7fff
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvShareRAM1,	0xa000, 0xa7ff, MAP_RAM);
//	bank2 = 0xc000-cfff
//	bank3 = 0xd000-dfff
//	bank4 = 0xe000-efff
//	bank5 = 0xf000-fdff
	ZetSetWriteHandler(kurikint_main_write);
	ZetSetReadHandler(kurikint_main_read);
	ZetClose();

	ZetInit(1); // not used

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvShareRAM1,	0xe000, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(evilston_sound_write);
	ZetSetReadHandler(evilston_sound_read);
	ZetClose();

	BurnYM2203Init(1, 3000000, NULL, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.80, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.20, BURN_SND_ROUTE_BOTH);

	TC0140SYTInit(2); // not used

	DrvDoReset();

	GenericTilesInit();

	return 0;
}

static INT32 KurikintInit()
{
	return KurikintCommonInit(0);
}

static INT32 KurikintaInit()
{
	return KurikintCommonInit(1);
}

static INT32 commonSingleZ80(INT32 (*pRomLoadCallback)(), void (*write)(UINT16,UINT8), UINT8 (*read)(UINT16), INT32 type)
{
	DrvGetGfxRomLen();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (pRomLoadCallback) {
		if (pRomLoadCallback()) return 1;

		DrvGfxDecode(DrvGfxROM0, DrvGfxROM1, nGfxRomLen, 1);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, nGfxRomLen, 0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x5fff, MAP_ROM);
//	bank1 = 0x6000-7fff
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x9fff, MAP_RAM);
	// puzznic needs this
	if (type & 1) 	ZetMapMemory(DrvShareRAM1,	0xb000, 0xb7ff, MAP_RAM); // shared w/MCU?
//	bank2 = 0xc000-cfff
//	bank3 = 0xd000-dfff
//	bank4 = 0xe000-efff
//	bank5 = 0xf000-fdff
	ZetSetWriteHandler((void (__fastcall*) (UINT16,UINT8))write);
	ZetSetReadHandler((UINT8 (__fastcall*) (UINT16))read);
	ZetClose();

	ZetInit(1); // not used
	ZetInit(2); // not used

	BurnYM2203Init(1, 13330560/4, NULL, DrvSynchroniseStream2, DrvGetTime2, 0);

	BurnTimerAttachZet(13330560/2);

	if (type & 2) {
		BurnYM2203SetPorts(0, &ym2203_read_portA1, &ym2203_read_portB1, NULL, NULL);
	} else {
		BurnYM2203SetPorts(0, &ym2203_read_portA, &ym2203_read_portB, NULL, NULL);
	}

	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.80, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.20, BURN_SND_ROUTE_BOTH);

	TC0140SYTInit(2); // not used

	DrvDoReset();

	GenericTilesInit();

	return 0;
}

static INT32 PlottingRomLoad()
{
	if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  1, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x00001,  2, 2)) return 1;

	return 0;
}

static INT32 PlottingInit()
{
	return commonSingleZ80(PlottingRomLoad, (void (__cdecl *)(UINT16,UINT8))plotting_main_write, (UINT8 (__cdecl *)(UINT16))plotting_main_read, 0);
}

static INT32 PlottingaRomLoad()
{
	if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  1, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x00001,  2, 2)) return 1;

	for (INT32 i = 0; i < 0x10000; i++) {
		DrvZ80ROM0[i] = BITSWAP08(DrvZ80ROM0[i],0,1,2,3,4,5,6,7);
	}

	return 0;
}

static INT32 PlottingaInit()
{
	return commonSingleZ80(PlottingaRomLoad, (void (__cdecl *)(UINT16,UINT8))plotting_main_write, (UINT8 (__cdecl *)(UINT16))plotting_main_read, 0);
}

static INT32 PuzznicInit()
{
	return commonSingleZ80(PlottingRomLoad, (void (__cdecl *)(UINT16,UINT8))puzznic_main_write, (UINT8 (__cdecl *)(UINT16))puzznic_main_read, 1);
}

static INT32 PalamedInit()
{
	return commonSingleZ80(PlottingRomLoad, (void (__cdecl *)(UINT16,UINT8))palamed_main_write, (UINT8 (__cdecl *)(UINT16))palamed_main_read, 2);
}

static INT32 CachatRomLoad()
{
	if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  1, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x40000,  2, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x00001,  3, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x40001,  4, 2)) return 1;

	return 0;
}

static INT32 CachatInit()
{
	return commonSingleZ80(CachatRomLoad, (void (__cdecl *)(UINT16,UINT8))palamed_main_write, (UINT8 (__cdecl *)(UINT16))palamed_main_read, 2);
}

static INT32 CubybopRomLoad()
{
	if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  1, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x80000,  2, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x00001,  3, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x80001,  4, 2)) return 1;

	return 0;
}

static INT32 CubybopInit()
{
	return commonSingleZ80(CubybopRomLoad, (void (__cdecl *)(UINT16,UINT8))palamed_main_write, (UINT8 (__cdecl *)(UINT16))palamed_main_read, 2);
}

static INT32 LagirlRomLoad()
{
	if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00003,  1, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x00001,  2, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x00002,  3, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 4)) return 1;

	return 0;
}

static INT32 LagirlInit()
{
	return commonSingleZ80(LagirlRomLoad, (void (__cdecl *)(UINT16,UINT8))palamed_main_write, (UINT8 (__cdecl *)(UINT16))palamed_main_read, 2);
}

static INT32 Plgirls2bInit()
{
	plgirls2bmode = 1;

	return LagirlInit();
}

static INT32 HorshoesRomLoad()
{
	if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

	UINT8 *tmp = (UINT8*)malloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	if (BurnLoadRom(tmp + 0x00000,  1, 2)) return 1;
	if (BurnLoadRom(tmp + 0x40000,  2, 2)) return 1;
	if (BurnLoadRom(tmp + 0x00001,  3, 2)) return 1;
	if (BurnLoadRom(tmp + 0x40001,  4, 2)) return 1;

	memcpy (DrvGfxROM0 + 0x00000, tmp + 0x00000, 0x20000);
	memcpy (DrvGfxROM0 + 0x20000, tmp + 0x40000, 0x20000);
	memcpy (DrvGfxROM0 + 0x40000, tmp + 0x20000, 0x20000);
	memcpy (DrvGfxROM0 + 0x60000, tmp + 0x60000, 0x20000);

	BurnFree (tmp);

	return 0;
}

static INT32 HorshoesInit()
{
	has_track = 1;
	return commonSingleZ80(HorshoesRomLoad, (void (__cdecl *)(UINT16,UINT8))horshoes_main_write, (UINT8 (__cdecl *)(UINT16))horshoes_main_read, 0);
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	if (has_ym2610) {
		BurnYM2610Exit();
	} else {
		BurnYM2203Exit();
	}

	if (has_adpcm) {
		MSM5205Exit();
	}
	has_adpcm = 0;
	has_ym2610 = 0;
	nmi_enable = 0;
	has_track = 0;

	fhawkmode = 0;
	plgirls2bmode = 0;

	TaitoICExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x200; i+=2)
	{
		UINT8 r = DrvPalRAM[i+0] & 0xf;
		UINT8 g = DrvPalRAM[i+0] >> 4;
		UINT8 b = DrvPalRAM[i+1] & 0xf;

		DrvPalette[i/2] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static void draw_layer(UINT8 *ram, UINT8 *gfx, INT32 layer, INT32 prio) // 1=bg,0=fg,2=chars
{
	INT32 scrollx = 0;
	INT32 scrolly = 0;

	if (layer != 2) {
		INT32 sxoffset[2] = { 28+8, 38+8 };
		UINT8 *scroll = DrvSprRAM + 0x3f0 + ((layer^1) * 8);
		scrollx = ((scroll[4] + (scroll[5] << 8)) + sxoffset[layer^1]) & 0x1ff;
		scrolly =  scroll[6];

		scrollx = 0 - scrollx;
		scrolly *= -1;
	}

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		sx -= scrollx + 8;
		if (plgirls2bmode && layer==0) {
			sx -= scrollx - 8; //kludge for plgirls2b bootleg bg offset
		}
		sy -= scrolly + 16;
		if (sx >= nScreenWidth) sx -= 512;
		if (sy >= nScreenHeight) sy -= 256;
		if (sx < -7 || sy < -7) continue;

		INT32 attr  = ram[offs * 2 + 1];
		INT32 code  = ram[offs * 2 + 0];
		INT32 color = attr >> 4;

		if (layer == 2) {
			code |= ((attr & 1) << 8) | ((attr & 4) << 7);
		} else {
			code |= ((attr & 3) << 8) | (char_banks[(attr >> 2) & 3] << 10) | (horshoes_bank << 12);
			code &= ((nGfxRomLen * 2) / 0x40) - 1;
		}

		if (sx >= 0 && sx < (nScreenWidth - 7) && sy >= 0 && sy < (nScreenHeight - 7)) {
			if (layer) {
				Render8x8Tile_Prio_Mask(pTransDraw, code, sx, sy, color, 4, 0, 0, prio, gfx);
			} else {
				Render8x8Tile_Prio(pTransDraw, code, sx, sy, color, 4, 0, prio, gfx);
			}
		} else {
			if (layer) {
				Render8x8Tile_Prio_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, prio, gfx);
			} else {
				Render8x8Tile_Prio_Clip(pTransDraw, code, sx, sy, color, 4, 0, prio, gfx);
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x400 - 3 * 8; offs += 8)
	{
		INT32 color = DrvSprBuf[offs + 2] & 0x0f;

		INT32 code  = DrvSprBuf[offs] | (DrvSprBuf[offs + 1] << 8) | ((horshoes_bank & 3) << 10);
		code &= ((nGfxRomLen * 2) / 0x100) - 1;

		INT32 sx = DrvSprBuf[offs + 4] | ((DrvSprBuf[offs + 5] & 1) << 8);
		INT32 sy = DrvSprBuf[offs + 6];
		if (sx >= 320) sx -= 512;

		INT32 flipx = DrvSprBuf[offs + 3] & 0x01;
		INT32 flipy = DrvSprBuf[offs + 3] & 0x02;

		if (flipscreen)
		{
			sx = 304 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 16;

		RenderPrioSprite(pTransDraw, DrvGfxROM1, code, color<<4, 0, sx, sy, flipx, flipy, 16, 16, (color & 0x08) ? 0xaa : 0x00);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (current_control & 0x20)
	{
		BurnTransferClear();
		if (nBurnLayer & 1) draw_layer(DrvBgRAM + 0x1000, DrvGfxROM0, 0, 0); // BG

		if (current_control & 0x08) {
			if (nBurnLayer & 2) draw_layer(DrvBgRAM + 0x0000, DrvGfxROM0, 1, 0); // FG
		} else {
			if (nBurnLayer & 2) draw_layer(DrvBgRAM + 0x0000, DrvGfxROM0, 1, 1); // FG
		}

		if (nSpriteEnable & 1) draw_sprites();

		if (nBurnLayer & 4) draw_layer(DrvCharRAM,        DrvGfxROM2, 2, 0); // CHARS
	}
	else
	{
		BurnTransferClear();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void scanline_update(INT32 scanline)
{
	if (ActiveZ80GetIM() != 2)
		return;

	if (scanline == 120 && (irq_enable & 1))
	{
		last_irq_level = 0;
		ZetSetVector(irq_adr_table[last_irq_level]);
		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}
	else if (scanline == 0 && (irq_enable & 2))
	{
		last_irq_level = 1;
		ZetSetVector(irq_adr_table[last_irq_level]);
		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}
	else if (scanline == 240 && (irq_enable & 4))
	{
		last_irq_level = 2;
		ZetSetVector(irq_adr_table[last_irq_level]);
		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}

	if (scanline == 240) {

		if (pBurnDraw) {
			DrvDraw();
		}

		memcpy (DrvSprBuf, DrvSprRAM, 0x400);
	}
}

static INT32 Z80x3Frame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] =  { 6665280 / 60, ((fhawkmode) ? 6665280 : 4000000) / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	if (has_adpcm)
		MSM5205NewFrame(0, 4000000, nInterleave);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		scanline_update(i);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[2] / nInterleave));
		if (has_adpcm)
			MSM5205UpdateScanline(i);
		ZetClose();
	}
	
	ZetOpen(2);

	BurnTimerEndFrame(nCyclesTotal[2]);

	if (pBurnSoundOut) {
		if (has_ym2610) {
			BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
		} else {
			BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		}
		if (has_adpcm) {
			MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
		}
	}

	ZetClose();

	return 0;
}

static INT32 Z80x2Frame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] =  { 6665280 / 60, 0, 4000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };
	INT32 nNext = 0;
	INT32 nCyclesSegment = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[0];
		nCyclesDone[0] += ZetRun(nCyclesSegment);
		scanline_update(i-15);
		ZetClose();

		ZetOpen(2);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[2] / nInterleave));
		if (i == (nInterleave - 4) && nmi_enable) ZetNmi(); // 60hz
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}
	
	ZetOpen(2);

	BurnTimerEndFrame(nCyclesTotal[2]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x400);

	return 0;
}

static INT32 Z80x1Frame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (has_track) trackball_tick();
	}

	INT32 nInterleave = 256/4;
	INT32 nCyclesTotal[1] =  { 6665280 / 60 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		BurnTimerUpdate(i * (nCyclesTotal[0] / nInterleave));
		scanline_update(i*4);
		ZetClose();
	}
	
	ZetOpen(0);

	BurnTimerEndFrame(nCyclesTotal[0]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x400);

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029697;
	}

	if (nAction & ACB_VOLATILE)
	{
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);

		ZetScan(nAction);
		ZetOpen(2);
		if (has_ym2610) {
			BurnYM2610Scan(nAction, pnMin);
		} else {
			BurnYM2203Scan(nAction, pnMin);
		}
		if (has_adpcm) {
			MSM5205Scan(nAction, pnMin);
		}
		ZetClose();
		TaitoICScan(nAction);

		SCAN_VAR(flipscreen);
		SCAN_VAR(irq_enable);
		SCAN_VAR(current_control);
		SCAN_VAR(last_irq_level);
		SCAN_VAR(adpcm_pos);
		SCAN_VAR(mux_control);
		SCAN_VAR(mcu_position);
		SCAN_VAR(horshoes_bank);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		rambankswitch(0, cur_rambank[0]);
		rambankswitch(1, cur_rambank[1]);
		rambankswitch(2, cur_rambank[2]);
		rambankswitch(3, cur_rambank[3]);
		if (cur_rombank[0] != 0xff) rombankswitch0(cur_rombank[0]);
		ZetClose();

		if (cur_rombank[1] != 0xff) {
			ZetOpen(1);
			rombankswitch1(cur_rombank[1]);
			ZetClose();
		}

		if (cur_rombank[2] != 0xff)
		{
			ZetOpen(2);
			if (has_ym2610) {
				raimais_rombankswitch2(cur_rombank[2]);
			} else {
				rombankswitch2(cur_rombank[2]);
			}
			ZetClose();
		}

		DrvGfxDecode(DrvGfxRAM, DrvGfxROM2, 0x8000, 0);
	}

	return 0;
}


// Raimais (World)

static struct BurnRomInfo raimaisRomDesc[] = {
	{ "b36-11-1.bin",	0x20000, 0xf19fb0d5, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "b36-09.bin",		0x20000, 0x9c466e43, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b36-07.bin",		0x10000, 0x4f3737e6, 2 | BRF_PRG | BRF_ESS }, //  2 sub z80 code

	{ "b36-06.bin",		0x10000, 0x29bbc4f8, 3 | BRF_PRG | BRF_ESS }, //  3 sound z80 code

	{ "b36-01.bin",		0x80000, 0x89355cb2, 4 | BRF_GRA },           //  4 graphics data
	{ "b36-02.bin",		0x80000, 0xe71da5db, 4 | BRF_GRA },           //  5

	{ "b36-03.bin",		0x80000, 0x96166516, 5 | BRF_SND },           //  6 samples
};

STD_ROM_PICK(raimais)
STD_ROM_FN(raimais)

struct BurnDriver BurnDrvRaimais = {
	"raimais", NULL, NULL, NULL, "1988",
	"Raimais (World)\0", NULL, "Taito Corporation Japan", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, raimaisRomInfo, raimaisRomName, NULL, NULL, FhawkInputInfo, RaimaisDIPInfo,
	RaimaisInit, DrvExit, Z80x3Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Raimais (Japan)

static struct BurnRomInfo raimaisjRomDesc[] = {
	{ "b36-08-1.bin",	0x20000, 0x6cc8f79f, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "b36-09.bin",		0x20000, 0x9c466e43, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b36-07.bin",		0x10000, 0x4f3737e6, 2 | BRF_PRG | BRF_ESS }, //  2 sub z80 code

	{ "b36-06.bin",		0x10000, 0x29bbc4f8, 3 | BRF_PRG | BRF_ESS }, //  3 sound z80 code

	{ "b36-01.bin",		0x80000, 0x89355cb2, 4 | BRF_GRA },           //  4 graphics data
	{ "b36-02.bin",		0x80000, 0xe71da5db, 4 | BRF_GRA },           //  5

	{ "b36-03.bin",		0x80000, 0x96166516, 5 | BRF_SND },           //  6 samples
};

STD_ROM_PICK(raimaisj)
STD_ROM_FN(raimaisj)

struct BurnDriver BurnDrvRaimaisj = {
	"raimaisj", "raimais", NULL, NULL, "1988",
	"Raimais (Japan)\0", NULL, "Taito Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, raimaisjRomInfo, raimaisjRomName, NULL, NULL, FhawkInputInfo, RaimaisjDIPInfo,
	RaimaisInit, DrvExit, Z80x3Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Raimais (Japan, first revision)

static struct BurnRomInfo raimaisjoRomDesc[] = {
	{ "b36-08.bin",		0x20000, 0xf40b9178, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "b36-09.bin",		0x20000, 0x9c466e43, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b36-07.bin",		0x10000, 0x4f3737e6, 2 | BRF_PRG | BRF_ESS }, //  2 sub z80 code

	{ "b36-06.bin",		0x10000, 0x29bbc4f8, 3 | BRF_PRG | BRF_ESS }, //  3 sound z80 code

	{ "b36-01.bin",		0x80000, 0x89355cb2, 4 | BRF_GRA },           //  4 graphics data
	{ "b36-02.bin",		0x80000, 0xe71da5db, 4 | BRF_GRA },           //  5

	{ "b36-03.bin",		0x80000, 0x96166516, 5 | BRF_SND },           //  6 samples
};

STD_ROM_PICK(raimaisjo)
STD_ROM_FN(raimaisjo)

struct BurnDriver BurnDrvRaimaisjo = {
	"raimaisjo", "raimais", NULL, NULL, "1988",
	"Raimais (Japan, first revision)\0", NULL, "Taito Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, raimaisjoRomInfo, raimaisjoRomName, NULL, NULL, FhawkInputInfo, RaimaisjDIPInfo,
	RaimaisInit, DrvExit, Z80x3Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Fighting Hawk (World)

static struct BurnRomInfo fhawkRomDesc[] = {
	{ "b70-11.ic3",		0x20000, 0x7d9f7583, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "b70-03.ic2",		0x80000, 0x42d5a9b8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b70-08.ic12",	0x20000, 0x4d795f48, 2 | BRF_PRG | BRF_ESS }, //  2 sub z80 code

	{ "b70-09.ic31",	0x10000, 0x85cccaa2, 3 | BRF_PRG | BRF_ESS }, //  3 sound z80 code

	{ "b70-01.ic1",		0x80000, 0xfcdf67e2, 4 | BRF_GRA },           //  4 graphics data
	{ "b70-02.ic2",		0x80000, 0x35f7172e, 4 | BRF_GRA },           //  5
};

STD_ROM_PICK(fhawk)
STD_ROM_FN(fhawk)

struct BurnDriver BurnDrvFhawk = {
	"fhawk", NULL, NULL, NULL, "1988",
	"Fighting Hawk (World)\0", NULL, "Taito Corporation Japan", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, fhawkRomInfo, fhawkRomName, NULL, NULL, FhawkInputInfo, FhawkDIPInfo,
	FhawkInit, DrvExit, Z80x3Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 320, 3, 4
};


// Fighting Hawk (Japan)

static struct BurnRomInfo fhawkjRomDesc[] = {
	{ "b70-07.ic3",		0x20000, 0x939114af, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "b70-03.ic2",		0x80000, 0x42d5a9b8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b70-08.ic12",	0x20000, 0x4d795f48, 2 | BRF_PRG | BRF_ESS }, //  2 sub z80 code

	{ "b70-09.ic31",	0x10000, 0x85cccaa2, 3 | BRF_PRG | BRF_ESS }, //  3 sound z80 code

	{ "b70-01.ic1",		0x80000, 0xfcdf67e2, 4 | BRF_GRA },           //  4 graphics data
	{ "b70-02.ic2",		0x80000, 0x35f7172e, 4 | BRF_GRA },           //  5
};

STD_ROM_PICK(fhawkj)
STD_ROM_FN(fhawkj)

struct BurnDriver BurnDrvFhawkj = {
	"fhawkj", "fhawk", NULL, NULL, "1988",
	"Fighting Hawk (Japan)\0", NULL, "Taito Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, fhawkjRomInfo, fhawkjRomName, NULL, NULL, FhawkInputInfo, FhawkjDIPInfo,
	FhawkInit, DrvExit, Z80x3Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 320, 3, 4
};


// Champion Wrestler (World)

static struct BurnRomInfo champwrRomDesc[] = {
	{ "c01-13.rom",		0x20000, 0x7ef47525, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "c01-04.rom",		0x20000, 0x358bd076, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "c01-07.rom",		0x20000, 0x5117c98f, 2 | BRF_PRG | BRF_ESS }, //  2 sub z80 code

	{ "c01-08.rom",		0x10000, 0x810efff8, 3 | BRF_PRG | BRF_ESS }, //  3 sound z80 code
	
	{ "c01-01.rom",		0x80000, 0xf302e6e9, 4 | BRF_GRA },           //  4 graphics data
	{ "c01-02.rom",		0x80000, 0x1e0476c4, 4 | BRF_GRA },           //  5
	{ "c01-03.rom",		0x80000, 0x2a142dbc, 4 | BRF_GRA },           //  6

	{ "c01-05.rom",		0x20000, 0x22efad4a, 5 | BRF_SND },           //  7 samples
};

STD_ROM_PICK(champwr)
STD_ROM_FN(champwr)

struct BurnDriver BurnDrvChampwr = {
	"champwr", NULL, NULL, NULL, "1989",
	"Champion Wrestler (World)\0", NULL, "Taito Corporation Japan", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, champwrRomInfo, champwrRomName, NULL, NULL, ChampwrInputInfo, ChampwrDIPInfo,
	ChampwrInit, DrvExit, Z80x3Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Champion Wrestler (US)

static struct BurnRomInfo champwruRomDesc[] = {
	{ "c01-12.rom",		0x20000, 0x09f345b3, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "c01-04.rom",		0x20000, 0x358bd076, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "c01-07.rom",		0x20000, 0x5117c98f, 2 | BRF_PRG | BRF_ESS }, //  2 sub z80 code

	{ "c01-08.rom",		0x10000, 0x810efff8, 3 | BRF_PRG | BRF_ESS }, //  3 sound z80 code

	{ "c01-01.rom",		0x80000, 0xf302e6e9, 4 | BRF_GRA },           //  4 graphics data
	{ "c01-02.rom",		0x80000, 0x1e0476c4, 4 | BRF_GRA },           //  5
	{ "c01-03.rom",		0x80000, 0x2a142dbc, 4 | BRF_GRA },           //  6

	{ "c01-05.rom",		0x20000, 0x22efad4a, 5 | BRF_SND },           //  7 samples
};

STD_ROM_PICK(champwru)
STD_ROM_FN(champwru)

struct BurnDriver BurnDrvChampwru = {
	"champwru", "champwr", NULL, NULL, "1989",
	"Champion Wrestler (US)\0", NULL, "Taito America Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, champwruRomInfo, champwruRomName, NULL, NULL, ChampwrInputInfo, ChampwruDIPInfo,
	ChampwrInit, DrvExit, Z80x3Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Champion Wrestler (Japan)

static struct BurnRomInfo champwrjRomDesc[] = {
	{ "c01-06.bin",		0x20000, 0x90fa1409, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "c01-04.rom",		0x20000, 0x358bd076, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "c01-07.rom",		0x20000, 0x5117c98f, 2 | BRF_PRG | BRF_ESS }, //  2 sub z80 code

	{ "c01-08.rom",		0x10000, 0x810efff8, 3 | BRF_PRG | BRF_ESS }, //  3 sound z80 code

	{ "c01-01.rom",		0x80000, 0xf302e6e9, 4 | BRF_GRA },           //  4 graphics data
	{ "c01-02.rom",		0x80000, 0x1e0476c4, 4 | BRF_GRA },           //  5
	{ "c01-03.rom",		0x80000, 0x2a142dbc, 4 | BRF_GRA },           //  6

	{ "c01-05.rom",		0x20000, 0x22efad4a, 5 | BRF_SND },           //  7 samples
};

STD_ROM_PICK(champwrj)
STD_ROM_FN(champwrj)

struct BurnDriver BurnDrvChampwrj = {
	"champwrj", "champwr", NULL, NULL, "1989",
	"Champion Wrestler (Japan)\0", NULL, "Taito Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, champwrjRomInfo, champwrjRomName, NULL, NULL, ChampwrInputInfo, ChampwrjDIPInfo,
	ChampwrInit, DrvExit, Z80x3Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Kuri Kinton (World)

static struct BurnRomInfo kurikintRomDesc[] = {
	{ "b42-09.ic2",		0x20000, 0xe97c4394, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "b42-06.ic6",		0x20000, 0xfa15fd65, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b42-07.ic22",	0x10000, 0x0f2719c0, 3 | BRF_PRG | BRF_ESS }, //  2 sound z80 code

	{ "b42-01.ic1",		0x80000, 0x7d1a1fec, 4 | BRF_GRA },           //  3 graphics data
	{ "b42-02.ic5",		0x80000, 0x1a52e65c, 4 | BRF_GRA },           //  4
};

STD_ROM_PICK(kurikint)
STD_ROM_FN(kurikint)

struct BurnDriver BurnDrvKurikint = {
	"kurikint", NULL, NULL, NULL, "1988",
	"Kuri Kinton (World)\0", NULL, "Taito Corporation Japan", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, kurikintRomInfo, kurikintRomName, NULL, NULL, FhawkInputInfo, KurikintDIPInfo,
	KurikintInit, DrvExit, Z80x2Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Kuri Kinton (US)

static struct BurnRomInfo kurikintuRomDesc[] = {
	{ "b42-08.ic2",		0x20000, 0x7075122e, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "b42-06.ic6",		0x20000, 0xfa15fd65, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b42-07.ic22",	0x10000, 0x0f2719c0, 3 | BRF_PRG | BRF_ESS }, //  2 sound z80 code

	{ "b42-01.ic1",		0x80000, 0x7d1a1fec, 4 | BRF_GRA },           //  3 graphics data
	{ "b42-02.ic5",		0x80000, 0x1a52e65c, 4 | BRF_GRA },           //  4
};

STD_ROM_PICK(kurikintu)
STD_ROM_FN(kurikintu)

struct BurnDriver BurnDrvKurikintu = {
	"kurikintu", "kurikint", NULL, NULL, "1988",
	"Kuri Kinton (US)\0", NULL, "Taito America Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, kurikintuRomInfo, kurikintuRomName, NULL, NULL, FhawkInputInfo, KurikintjDIPInfo,
	KurikintInit, DrvExit, Z80x2Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Kuri Kinton (Japan)

static struct BurnRomInfo kurikintjRomDesc[] = {
	{ "b42-05.ic2",		0x20000, 0x077222b8, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "b42-06.ic6",		0x20000, 0xfa15fd65, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b42-07.ic22",	0x10000, 0x0f2719c0, 3 | BRF_PRG | BRF_ESS }, //  2 sound z80 code

	{ "b42-01.ic1",		0x80000, 0x7d1a1fec, 4 | BRF_GRA },           //  3 graphics data
	{ "b42-02.ic5",		0x80000, 0x1a52e65c, 4 | BRF_GRA },           //  4
};

STD_ROM_PICK(kurikintj)
STD_ROM_FN(kurikintj)

struct BurnDriver BurnDrvKurikintj = {
	"kurikintj", "kurikint", NULL, NULL, "1988",
	"Kuri Kinton (Japan)\0", NULL, "Taito Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, kurikintjRomInfo, kurikintjRomName, NULL, NULL, FhawkInputInfo, KurikintjDIPInfo,
	KurikintInit, DrvExit, Z80x2Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Kuri Kinton (World, prototype?)

static struct BurnRomInfo kurikintaRomDesc[] = {
	{ "kk_ic2.ic2",		0x20000, 0x908603f2, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "kk_ic6.ic6",		0x20000, 0xa4a957b1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b42-07.ic22",	0x10000, 0x0f2719c0, 3 | BRF_PRG | BRF_ESS }, //  2 sound z80 code

	{ "kk_1-1l.rom",	0x20000, 0xdf1d4fcd, 4 | BRF_GRA },           //  3 graphics data
	{ "kk_2-2l.rom",	0x20000, 0xfca7f647, 4 | BRF_GRA },           //  4
	{ "kk_5-3l.rom",	0x20000, 0xd080fde1, 4 | BRF_GRA },           //  5
	{ "kk_7-4l.rom",	0x20000, 0xf5bf6829, 4 | BRF_GRA },           //  6
	{ "kk_3-1h.rom",	0x20000, 0x71af848e, 4 | BRF_GRA },           //  7
	{ "kk_4-2h.rom",	0x20000, 0xcebb5bac, 4 | BRF_GRA },           //  8
	{ "kk_6-3h.rom",	0x20000, 0x322e3752, 4 | BRF_GRA },           //  9
	{ "kk_8-4h.rom",	0x20000, 0x117bde99, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(kurikinta)
STD_ROM_FN(kurikinta)

struct BurnDriver BurnDrvKurikinta = {
	"kurikinta", "kurikint", NULL, NULL, "1988",
	"Kuri Kinton (World, prototype?)\0", NULL, "Taito Corporation Japan", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, kurikintaRomInfo, kurikintaRomName, NULL, NULL, FhawkInputInfo, KurikintaDIPInfo,
	KurikintaInit, DrvExit, Z80x2Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Plotting (World set 1)

static struct BurnRomInfo plottingRomDesc[] = {
	{ "ic10",		0x10000, 0xbe240921, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "b96-07.ic9",		0x10000, 0x0713a387, 4 | BRF_GRA },           //  1 graphics data
	{ "b96-08.ic8",		0x10000, 0x55b8e294, 4 | BRF_GRA },           //  2

	{ "gal16v8-b86-04.bin",	0x00117, 0xbf8c0ea0, 0 | BRF_OPT },           //  3 plds
};

STD_ROM_PICK(plotting)
STD_ROM_FN(plotting)

struct BurnDriver BurnDrvPlotting = {
	"plotting", NULL, NULL, NULL, "1989",
	"Plotting (World set 1)\0", NULL, "Taito Corporation Japan", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, plottingRomInfo, plottingRomName, NULL, NULL, PlottingInputInfo, PlottingDIPInfo,
	PlottingInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Plotting (World set 2, protected)

static struct BurnRomInfo plottingaRomDesc[] = {
	{ "plot01.ic10",	0x10000, 0x5b30bc25, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "b96-02.ic9",		0x10000, 0x6e0bad2a, 4 | BRF_GRA },           //  1 graphics data
	{ "b96-03.ic8",		0x10000, 0xfb5f3ca4, 4 | BRF_GRA },           //  2

	{ "gal16v8-b86-04.bin",	0x00117, 0xbf8c0ea0, 0 | BRF_OPT },           //  3 plds
};

STD_ROM_PICK(plottinga)
STD_ROM_FN(plottinga)

struct BurnDriver BurnDrvPlottinga = {
	"plottinga", "plotting", NULL, NULL, "1989",
	"Plotting (World set 2, protected)\0", NULL, "Taito Corporation Japan", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, plottingaRomInfo, plottingaRomName, NULL, NULL, PlottingInputInfo, PlottingDIPInfo,
	PlottingaInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Plotting (World set 3, earliest version)

static struct BurnRomInfo plottingbRomDesc[] = {
	{ "b96-06.ic10",	0x10000, 0xf89a54b1, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "b96-02.ic9",		0x10000, 0x6e0bad2a, 4 | BRF_GRA },           //  1 graphics data
	{ "b96-03.ic8",		0x10000, 0xfb5f3ca4, 4 | BRF_GRA },           //  2

	{ "gal16v8-b86-04.bin",	0x00117, 0xbf8c0ea0, 0 | BRF_OPT },           //  3 plds
};

STD_ROM_PICK(plottingb)
STD_ROM_FN(plottingb)

struct BurnDriver BurnDrvPlottingb = {
	"plottingb", "plotting", NULL, NULL, "1989",
	"Plotting (World set 3, earliest version)\0", NULL, "Taito Corporation Japan", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, plottingbRomInfo, plottingbRomName, NULL, NULL, PlottingInputInfo, PlottingDIPInfo,
	PlottingInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Plotting (US)

static struct BurnRomInfo plottinguRomDesc[] = {
	{ "b96-05.ic10",	0x10000, 0xafb99d1f, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "b96-02.ic9",		0x10000, 0x6e0bad2a, 4 | BRF_GRA },           //  1 graphics data
	{ "b96-03.ic8",		0x10000, 0xfb5f3ca4, 4 | BRF_GRA },           //  2

	{ "b96-04.ic12",	0x00104, 0x9390a782, 0 | BRF_OPT },           //  3 plds
};

STD_ROM_PICK(plottingu)
STD_ROM_FN(plottingu)

struct BurnDriver BurnDrvPlottingu = {
	"plottingu", "plotting", NULL, NULL, "1989",
	"Plotting (US)\0", NULL, "Taito America Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, plottinguRomInfo, plottinguRomName, NULL, NULL, PlottingInputInfo, PlottinguDIPInfo,
	PlottingInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Flipull (Japan)

static struct BurnRomInfo flipullRomDesc[] = {
	{ "b96-01.ic10",	0x10000, 0x65993978, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "b96-07.ic9",		0x10000, 0x0713a387, 4 | BRF_GRA },           //  1 graphics data
	{ "b96-08.ic8",		0x10000, 0x55b8e294, 4 | BRF_GRA },           //  2

	{ "gal16v8-b86-04.bin",	0x00117, 0xbf8c0ea0, 0 | BRF_OPT },           //  3 plds
};

STD_ROM_PICK(flipull)
STD_ROM_FN(flipull)

struct BurnDriver BurnDrvFlipull = {
	"flipull", "plotting", NULL, NULL, "1989",
	"Flipull (Japan)\0", NULL, "Taito Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, flipullRomInfo, flipullRomName, NULL, NULL, PlottingInputInfo, PlottingDIPInfo,
	PlottingInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Puzznic (World)

static struct BurnRomInfo puzznicRomDesc[] = {
	{ "c20-09.ic11",	0x20000, 0x156d6de1, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "c20-07.ic10",	0x10000, 0xbe12749a, 4 | BRF_GRA },           //  1 graphics data
	{ "c20-06.ic9",		0x10000, 0xac85a9c5, 4 | BRF_GRA },           //  2

	{ "c20-01.ic4",		0x00800, 0x085f68b4, 9 | BRF_PRG | BRF_ESS }, //  3 mcu code

	{ "mmipal20l8.ic3",	0x00800, 0x00000000, 0 | BRF_NODUMP },        //  4 pals
};

STD_ROM_PICK(puzznic)
STD_ROM_FN(puzznic)

struct BurnDriver BurnDrvPuzznic = {
	"puzznic", NULL, NULL, NULL, "1989",
	"Puzznic (World)\0", NULL, "Taito Corporation Japan", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, puzznicRomInfo, puzznicRomName, NULL, NULL, PuzznicInputInfo, PuzznicDIPInfo,
	PuzznicInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Puzznic (US)

static struct BurnRomInfo puzznicuRomDesc[] = {
	{ "c20-10.ic11",	0x20000, 0x3522d2e5, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "c20-03.ic10",	0x20000, 0x4264056c, 4 | BRF_GRA },           //  1 graphics data
	{ "c20-02.ic9",		0x20000, 0x3c115f8b, 4 | BRF_GRA },           //  2

	{ "c20-01.ic4",		0x00800, 0x085f68b4, 9 | BRF_PRG | BRF_ESS }, //  3 mcu code

	{ "mmipal20l8.ic3",	0x00800, 0x00000000, 0 | BRF_NODUMP },        //  4 pals
};

STD_ROM_PICK(puzznicu)
STD_ROM_FN(puzznicu)

struct BurnDriver BurnDrvPuzznicu = {
	"puzznicu", "puzznic", NULL, NULL, "1989",
	"Puzznic (US)\0", NULL, "Taito America Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, puzznicuRomInfo, puzznicuRomName, NULL, NULL, PuzznicInputInfo, PuzznicDIPInfo,
	PuzznicInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Puzznic (Japan)

static struct BurnRomInfo puzznicjRomDesc[] = {
	{ "c20-04.ic11",	0x20000, 0xa4150b6c, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "c20-03.ic10",	0x20000, 0x4264056c, 4 | BRF_GRA },           //  1 graphics data
	{ "c20-02.ic9",		0x20000, 0x3c115f8b, 4 | BRF_GRA },           //  2

	{ "c20-01.ic4",		0x00800, 0x085f68b4, 9 | BRF_PRG | BRF_ESS }, //  3 mcu code

	{ "c20-05.ic3",		0x00144, 0xf90e5594, 0 | BRF_GRA },           //  4 pals
};

STD_ROM_PICK(puzznicj)
STD_ROM_FN(puzznicj)

struct BurnDriver BurnDrvPuzznicj = {
	"puzznicj", "puzznic", NULL, NULL, "1989",
	"Puzznic (Japan)\0", NULL, "Taito Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, puzznicjRomInfo, puzznicjRomName, NULL, NULL, PuzznicInputInfo, PuzznicDIPInfo,
	PuzznicInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Puzznic (Italian bootleg)

static struct BurnRomInfo puzzniciRomDesc[] = {
	{ "1.ic11",		0x20000, 0x4612f5e0, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "u10.ic10",		0x20000, 0x4264056c, 4 | BRF_GRA },           //  1 graphics data
	{ "3.ic9",		0x20000, 0x2bf5232a, 4 | BRF_GRA },           //  2
};

STD_ROM_PICK(puzznici)
STD_ROM_FN(puzznici)

struct BurnDriver BurnDrvPuzznici = {
	"puzznici", "puzznic", NULL, NULL, "1989",
	"Puzznic (Italian bootleg)\0", NULL, "bootleg", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, puzzniciRomInfo, puzzniciRomName, NULL, NULL, PuzznicInputInfo, PuzznicDIPInfo,
	PuzznicInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Puzznic (bootleg, set 1)

static struct BurnRomInfo puzznicbRomDesc[] = {
	{ "ic11.bin",		0x20000, 0x2510df4d, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "ic10.bin",		0x10000, 0xbe12749a, 4 | BRF_GRA },           //  1 graphics data
	{ "ic9.bin",		0x10000, 0x0f183340, 4 | BRF_GRA },           //  2
};

STD_ROM_PICK(puzznicb)
STD_ROM_FN(puzznicb)

struct BurnDriver BurnDrvPuzznicb = {
	"puzznicb", "puzznic", NULL, NULL, "1989",
	"Puzznic (bootleg, set 1)\0", NULL, "bootleg", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, puzznicbRomInfo, puzznicbRomName, NULL, NULL, PuzznicInputInfo, PuzznicDIPInfo,
	PuzznicInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Puzznic (bootleg, set 2)

static struct BurnRomInfo puzznicbaRomDesc[] = {
	{ "18.ic10",		0x20000, 0x8349eb3b, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "19.ic9",		0x20000, 0x4264056c, 4 | BRF_GRA },           //  1 graphics data
	{ "20.ic8",		0x20000, 0x3c115f8b, 4 | BRF_GRA },           //  2
};

STD_ROM_PICK(puzznicba)
STD_ROM_FN(puzznicba)

struct BurnDriver BurnDrvPuzznicba = {
	"puzznicba", "puzznic", NULL, NULL, "1989",
	"Puzznic (bootleg, set 2)\0", NULL, "bootleg", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, puzznicbaRomInfo, puzznicbaRomName, NULL, NULL, PuzznicInputInfo, PuzznicDIPInfo,
	PuzznicInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Palamedes (Japan)

static struct BurnRomInfo palamedRomDesc[] = {
	{ "c63.02",		0x20000, 0x55a82bb2, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "c63.04",		0x20000, 0xc7bbe460, 4 | BRF_GRA },           //  1 graphics data
	{ "c63.03",		0x20000, 0xfcd86e44, 4 | BRF_GRA },           //  2
};

STD_ROM_PICK(palamed)
STD_ROM_FN(palamed)

struct BurnDriver BurnDrvPalamed = {
	"palamed", NULL, NULL, NULL, "1990",
	"Palamedes (Japan)\0", NULL, "Taito Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, palamedRomInfo, palamedRomName, NULL, NULL, PalamedInputInfo, PalamedDIPInfo,
	PalamedInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Cachat (Japan)

static struct BurnRomInfo cachatRomDesc[] = {
	{ "cac6",		0x20000, 0x8105cf5f, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "cac9",		0x20000, 0xbc462914, 4 | BRF_GRA },           //  1 graphics data
	{ "cac10",		0x20000, 0xecc64b31, 4 | BRF_GRA },           //  2
	{ "cac7",		0x20000, 0x7fb71578, 4 | BRF_GRA },           //  3
	{ "cac8",		0x20000, 0xd2a63799, 4 | BRF_GRA },           //  4

	{ "pal20l8b-c63-01.14",	0x00144, 0x14a7dd2a, 0 | BRF_OPT },           //  5 plds
};

STD_ROM_PICK(cachat)
STD_ROM_FN(cachat)

struct BurnDriver BurnDrvCachat = {
	"cachat", NULL, NULL, NULL, "1993",
	"Cachat (Japan)\0", NULL, "Taito Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, cachatRomInfo, cachatRomName, NULL, NULL, CachatInputInfo, CachatDIPInfo,
	CachatInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Cuby Bop (location test)

static struct BurnRomInfo cubybopRomDesc[] = {
	{ "cb06.6",		0x40000, 0x66b89a85, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "cb09.9",		0x40000, 0x5f831e59, 4 | BRF_GRA },           //  1 graphics data
	{ "cb10.10",		0x40000, 0x430510fc, 4 | BRF_GRA },           //  2
	{ "cb07.7",		0x40000, 0x3582de99, 4 | BRF_GRA },           //  3
	{ "cb08.8",		0x40000, 0x09e18a51, 4 | BRF_GRA },           //  4
};

STD_ROM_PICK(cubybop)
STD_ROM_FN(cubybop)

struct BurnDriver BurnDrvCubybop = {
	"cubybop", NULL, NULL, NULL, "199?",
	"Cuby Bop (location test)\0", NULL, "Hot-B", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, cubybopRomInfo, cubybopRomName, NULL, NULL, CubybopInputInfo, CubybopDIPInfo,
	CubybopInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Tube-It

static struct BurnRomInfo tubeitRomDesc[] = {
	{ "t-i_02.6",		0x20000, 0x54730669, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "t-i_04.9",		0x40000, 0xb4a6e31d, 4 | BRF_GRA },           //  1 graphics data
	{ "t-i_03.7",		0x40000, 0xe1c3fed0, 4 | BRF_GRA },           //  2

	{ "pal20l8b-c63-01.14",	0x00144, 0x14a7dd2a, 0 | BRF_OPT },           //  3 plds
};

STD_ROM_PICK(tubeit)
STD_ROM_FN(tubeit)

struct BurnDriver BurnDrvTubeit = {
	"tubeit", "cachat", NULL, NULL, "1993",
	"Tube-It\0", NULL, "bootleg", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, tubeitRomInfo, tubeitRomName, NULL, NULL, TubeitInputInfo, TubeitDIPInfo,
	PalamedInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Play Girls

static struct BurnRomInfo plgirlsRomDesc[] = {
	{ "pg03.ic6",		0x40000, 0x6ca73092, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "pg02.ic9",		0x40000, 0x3cf05ca9, 4 | BRF_GRA },           //  1 graphics data
	{ "pg01.ic7",		0x40000, 0x79e41e74, 4 | BRF_GRA },           //  2
};

STD_ROM_PICK(plgirls)
STD_ROM_FN(plgirls)

struct BurnDriver BurnDrvPlgirls = {
	"plgirls", NULL, NULL, NULL, "1992",
	"Play Girls\0", NULL, "Hot-B", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, plgirlsRomInfo, plgirlsRomName, NULL, NULL, PlgirlsInputInfo, PlgirlsDIPInfo,
	PalamedInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 320, 3, 4
};


// LA Girl

static struct BurnRomInfo lagirlRomDesc[] = {
	{ "rom1",		0x40000, 0xba1acfdb, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "rom2",		0x20000, 0x4c739a30, 4 | BRF_GRA },           //  1 graphics data
	{ "rom3",		0x20000, 0x4cf22a4b, 4 | BRF_GRA },           //  2
	{ "rom4",		0x20000, 0x7dcd6696, 4 | BRF_GRA },           //  3
	{ "rom5",		0x20000, 0xb1782816, 4 | BRF_GRA },           //  4
};

STD_ROM_PICK(lagirl)
STD_ROM_FN(lagirl)

struct BurnDriver BurnDrvLagirl = {
	"lagirl", "plgirls", NULL, NULL, "1992",
	"LA Girl\0", NULL, "bootleg", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, lagirlRomInfo, lagirlRomName, NULL, NULL, PlgirlsInputInfo, PlgirlsDIPInfo,
	LagirlInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 320, 3, 4
};


// Play Girls 2

static struct BurnRomInfo plgirls2RomDesc[] = {
	{ "pg2_1j.ic6",		0x40000, 0xf924197a, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "cho-l.ic9",		0x80000, 0x956384ec, 4 | BRF_GRA },           //  1 graphics data
	{ "cho-h.ic7",		0x80000, 0x992f99b1, 4 | BRF_GRA },           //  2
};

STD_ROM_PICK(plgirls2)
STD_ROM_FN(plgirls2)

struct BurnDriver BurnDrvPlgirls2 = {
	"plgirls2", NULL, NULL, NULL, "1993",
	"Play Girls 2\0", NULL, "Hot-B", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, plgirls2RomInfo, plgirls2RomName, NULL, NULL, PlgirlsInputInfo, Plgirls2DIPInfo,
	PalamedInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 320, 3, 4
};


// Play Girls 2 (bootleg)

static struct BurnRomInfo plgirls2bRomDesc[] = {
	{ "playgirls2b.d1",	0x40000, 0xd58159fa, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "playgirls2b.d8",	0x40000, 0x22df48b5, 4 | BRF_GRA },           //  1 graphics data
	{ "playgirls2b.d4",	0x40000, 0xbc9e2192, 4 | BRF_GRA },           //  2
	{ "playgirls2b.d3",	0x40000, 0x75d82fab, 4 | BRF_GRA },           //  3
	{ "playgirls2b.b6",	0x40000, 0xaac6c90b, 4 | BRF_GRA },           //  4
};

STD_ROM_PICK(plgirls2b)
STD_ROM_FN(plgirls2b)

struct BurnDriver BurnDrvPlgirls2b = {
	"plgirls2b", "plgirls2", NULL, NULL, "1993",
	"Play Girls 2 (bootleg)\0", NULL, "bootleg", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, plgirls2bRomInfo, plgirls2bRomName, NULL, NULL, PlgirlsInputInfo, Plgirls2DIPInfo,
	Plgirls2bInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 320, 3, 4
};


// American Horseshoes (US)

static struct BurnRomInfo horshoesRomDesc[] = {
	{ "c47-03.ic6",		0x20000, 0x37e15b20, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code

	{ "c47-02.ic5",		0x20000, 0x35f96526, 4 | BRF_GRA },           //  1 graphics data
	{ "c47-01.ic11",	0x20000, 0x031c73d8, 4 | BRF_GRA },           //  2
	{ "c47-04.ic4",		0x20000, 0xaeac7121, 4 | BRF_GRA },           //  3
	{ "c47-05.ic10",	0x20000, 0xb2a3dafe, 4 | BRF_GRA },           //  4

	{ "c47-06.ic12",	0x00144, 0x4342ca6c, 0 | BRF_OPT },           //  5 plds
};

STD_ROM_PICK(horshoes)
STD_ROM_FN(horshoes)

struct BurnDriverD BurnDrvHorshoes = {
	"horshoes", NULL, NULL, NULL, "1990",
	"American Horseshoes (US)\0", NULL, "Taito America Corporation", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_SPORTSMISC, 0,
	NULL, horshoesRomInfo, horshoesRomName, NULL, NULL, HorshoesInputInfo, HorshoesDIPInfo,
	HorshoesInit, DrvExit, Z80x1Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 320, 3, 4
};


// Evil Stone

static struct BurnRomInfo evilstonRomDesc[] = {
	{ "c67-03.ic2",		0x20000, 0x53419982, 1 | BRF_PRG | BRF_ESS }, //  0 main z80 code
	{ "c67-04.ic6",		0x20000, 0x55d57e19, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "c67-05.ic22",	0x20000, 0x94d3a642, 2 | BRF_PRG | BRF_ESS }, //  2 sound z80 code

	{ "c67-01.ic1",		0x80000, 0x2f351bf4, 4 | BRF_GRA },           //  3 graphics data
	{ "c67-02.ic5",		0x80000, 0xeb4f895c, 4 | BRF_GRA },           //  4
};

STD_ROM_PICK(evilston)
STD_ROM_FN(evilston)

struct BurnDriver BurnDrvEvilston = {
	"evilston", NULL, NULL, NULL, "1990",
	"Evil Stone\0", NULL, "Spacy Industrial, Ltd.", "L-System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, evilstonRomInfo, evilstonRomName, NULL, NULL, EvilstonInputInfo, EvilstonDIPInfo,
	EvilstonInit, DrvExit, Z80x2Frame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 320, 3, 4
};
