// FB Alpha (SSV) Seta, Sammy, and Visco driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "v60_intf.h"
#include "upd7725.h"
#include "es5506.h"
#include "st0020.h"
#include "eeprom.h"
#include "math.h"

/*
	srmp7 - no music/sfx
	gundam - verify layer should be OVER sprites and not behind
	analog inputs not hooked up at all
	clipping is not hooked up (broken) - marked with "iq_132" test with twin eagle
*/

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvV60ROM;
static UINT8 *DrvDSPROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvSndROM2;
static UINT8 *DrvSndROM3;
static UINT8 *DrvV60RAM0;
static UINT8 *DrvV60RAM1;
static UINT8 *DrvV60RAM2;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvDspRAM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvScrollRAM;
static UINT8 *DrvVectors;

// GDFS
static UINT8 *DrvTMAPRAM;
static UINT8 *DrvTMAPScroll;
static UINT8 *DrvGfxROM2;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

// required for save states
static UINT16 requested_int;
static UINT16 enable_video;
static UINT16 irq_enable;
static UINT8 input_select;
static UINT16 sexyreact_previous_dial;
static UINT32 sexyreact_serial_read;

static UINT8 *eaglshot_bank;

// not required for save states
static INT32 watchdog;
static INT32 vblank;
static INT32 shadow_pen_shift;
static INT32 shadow_pen_mask;
static INT32 tile_code[16];
static INT32 Gclip_min_x;
static INT32 Gclip_max_x;
static INT32 Gclip_min_y;
static INT32 Gclip_max_y;
static INT32 interrupt_ultrax = 0;
static INT32 watchdog_disable = 0;
static INT32 is_gdfs = 0;
static INT32 vbl_kludge = 0; // late vbl, for flicker issues in vertical games
static INT32 vbl_invert = 0; // invert vblank register, for drifto94

static INT32 dsp_enable = 0;

static INT32 nDrvSndROMLen[4];
static INT32 nDrvGfxROMLen;
static INT32 nDrvGfxROM2Len;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvJoy7[8];
static UINT8 DrvJoy8[8];
static UINT8 DrvInputs[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnInputInfo VasaraInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Vasara)

static struct BurnInputInfo SurvartsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 6"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 6"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Survarts)

static struct BurnInputInfo KeithlcyInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 4"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Keithlcy)

static struct BurnInputInfo Twineag2InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Twineag2)

static struct BurnInputInfo MeosismInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 5"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy2 + 5,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Meosism)

static struct BurnInputInfo RyoriohInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 4"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ryorioh)

static struct BurnInputInfo GdfsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},

	// placeholders for analog inputs
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 8,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy3 + 9,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy3 + 10,	"p1 fire 6"	},
	{"P1 Button 7",		BIT_DIGITAL,	DrvJoy3 + 11,	"p1 fire 7"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"	},
	{"P2 Button 7",		BIT_DIGITAL,	DrvJoy3 + 11,	"p2 fire 7"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Gdfs)

static struct BurnInputInfo MahjongInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},
	{"A",			BIT_DIGITAL,	DrvJoy8 + 5,	"mah a"		},
	{"B",			BIT_DIGITAL,	DrvJoy7 + 0,	"mah b"		},
	{"C",			BIT_DIGITAL,	DrvJoy6 + 2,	"mah c"		},
	{"D",			BIT_DIGITAL,	DrvJoy5 + 5,	"mah d"		},
	{"E",			BIT_DIGITAL,	DrvJoy8 + 4,	"mah e"		},
	{"F",			BIT_DIGITAL,	DrvJoy7 + 4,	"mah f"		},
	{"G",			BIT_DIGITAL,	DrvJoy6 + 4,	"mah g"		},
	{"H",			BIT_DIGITAL,	DrvJoy5 + 4,	"mah h"		},
	{"I",			BIT_DIGITAL,	DrvJoy8 + 3,	"mah i"		},
	{"J",			BIT_DIGITAL,	DrvJoy7 + 3,	"mah j"		},
	{"K",			BIT_DIGITAL,	DrvJoy6 + 3,	"mah k"		},
	{"L",			BIT_DIGITAL,	DrvJoy5 + 3,	"mah l"		},
	{"M",			BIT_DIGITAL,	DrvJoy8 + 2,	"mah m"		},
	{"N",			BIT_DIGITAL,	DrvJoy7 + 2,	"mah n"		},
	{"Pon",			BIT_DIGITAL,	DrvJoy5 + 2,	"mah pon"	},
	{"Chi",			BIT_DIGITAL,	DrvJoy6 + 2,	"mah chi"	},
	{"Kan",			BIT_DIGITAL,	DrvJoy8 + 1,	"mah kan"	},
	{"Ron",			BIT_DIGITAL,	DrvJoy6 + 1,	"mah ron"	},
	{"Reach",		BIT_DIGITAL,	DrvJoy7 + 1,	"mah reach"	},
	{"Bet",			BIT_DIGITAL,	DrvJoy7 + 0,	"mah bet"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Mahjong)

static struct BurnInputInfo Srmp4InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy8 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},
	{"A",			BIT_DIGITAL,	DrvJoy8 + 5,	"mah a"		},
	{"B",			BIT_DIGITAL,	DrvJoy7 + 0,	"mah b"		},
	{"C",			BIT_DIGITAL,	DrvJoy6 + 2,	"mah c"		},
	{"D",			BIT_DIGITAL,	DrvJoy5 + 5,	"mah d"		},
	{"E",			BIT_DIGITAL,	DrvJoy8 + 4,	"mah e"		},
	{"F",			BIT_DIGITAL,	DrvJoy7 + 4,	"mah f"		},
	{"G",			BIT_DIGITAL,	DrvJoy6 + 4,	"mah g"		},
	{"H",			BIT_DIGITAL,	DrvJoy5 + 4,	"mah h"		},
	{"I",			BIT_DIGITAL,	DrvJoy8 + 3,	"mah i"		},
	{"J",			BIT_DIGITAL,	DrvJoy7 + 3,	"mah j"		},
	{"K",			BIT_DIGITAL,	DrvJoy6 + 3,	"mah k"		},
	{"L",			BIT_DIGITAL,	DrvJoy5 + 3,	"mah l"		},
	{"M",			BIT_DIGITAL,	DrvJoy8 + 2,	"mah m"		},
	{"N",			BIT_DIGITAL,	DrvJoy7 + 2,	"mah n"		},
	{"Pon",			BIT_DIGITAL,	DrvJoy5 + 2,	"mah pon"	},
	{"Chi",			BIT_DIGITAL,	DrvJoy6 + 2,	"mah chi"	},
	{"Kan",			BIT_DIGITAL,	DrvJoy8 + 1,	"mah kan"	},
	{"Ron",			BIT_DIGITAL,	DrvJoy6 + 1,	"mah ron"	},
	{"Reach",		BIT_DIGITAL,	DrvJoy7 + 1,	"mah reach"	},
	{"Bet",			BIT_DIGITAL,	DrvJoy7 + 0,	"mah bet"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Srmp4)

static struct BurnInputInfo HypreactInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 right"	},
	{"A",			BIT_DIGITAL,	DrvJoy4 + 0,	"mah a"		},
	{"B",			BIT_DIGITAL,	DrvJoy5 + 0,	"mah b"		},
	{"C",			BIT_DIGITAL,	DrvJoy1 + 1,	"mah c"		},
	{"D",			BIT_DIGITAL,	DrvJoy7 + 0,	"mah d"		},
	{"E",			BIT_DIGITAL,	DrvJoy4 + 1,	"mah e"		},
	{"F",			BIT_DIGITAL,	DrvJoy5 + 1,	"mah f"		},
	{"G",			BIT_DIGITAL,	DrvJoy6 + 1,	"mah g"		},
	{"H",			BIT_DIGITAL,	DrvJoy7 + 1,	"mah h"		},
	{"I",			BIT_DIGITAL,	DrvJoy4 + 2,	"mah i"		},
	{"J",			BIT_DIGITAL,	DrvJoy5 + 2,	"mah j"		},
	{"K",			BIT_DIGITAL,	DrvJoy1 + 3,	"mah k"		},
	{"L",			BIT_DIGITAL,	DrvJoy7 + 2,	"mah l"		},
	{"M",			BIT_DIGITAL,	DrvJoy4 + 3,	"mah m"		},
	{"N",			BIT_DIGITAL,	DrvJoy5 + 3,	"mah n"		},
	{"Pon",			BIT_DIGITAL,	DrvJoy1 + 2,	"mah pon"	},
	{"Chi",			BIT_DIGITAL,	DrvJoy1 + 1,	"mah chi"	},
	{"Kan",			BIT_DIGITAL,	DrvJoy1 + 3,	"mah kan"	},
	{"Ron",			BIT_DIGITAL,	DrvJoy2 + 2,	"mah ron"	},
	{"Reach",		BIT_DIGITAL,	DrvJoy2 + 1,	"mah reach"	},
	{"Bet",			BIT_DIGITAL,	DrvJoy5 + 5,	"mah bet"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Hypreact)

static struct BurnInputInfo Hypreac2InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},
	{"A",			BIT_DIGITAL,	DrvJoy4 + 0,	"mah a"		},
	{"B",			BIT_DIGITAL,	DrvJoy5 + 0,	"mah b"		},
	{"C",			BIT_DIGITAL,	DrvJoy6 + 0,	"mah c"		},
	{"D",			BIT_DIGITAL,	DrvJoy7 + 0,	"mah d"		},
	{"E",			BIT_DIGITAL,	DrvJoy4 + 1,	"mah e"		},
	{"F",			BIT_DIGITAL,	DrvJoy5 + 1,	"mah f"		},
	{"G",			BIT_DIGITAL,	DrvJoy6 + 1,	"mah g"		},
	{"H",			BIT_DIGITAL,	DrvJoy7 + 1,	"mah h"		},
	{"I",			BIT_DIGITAL,	DrvJoy4 + 2,	"mah i"		},
	{"J",			BIT_DIGITAL,	DrvJoy5 + 2,	"mah j"		},
	{"K",			BIT_DIGITAL,	DrvJoy4 + 4,	"mah k"		},
	{"L",			BIT_DIGITAL,	DrvJoy7 + 2,	"mah l"		},
	{"M",			BIT_DIGITAL,	DrvJoy4 + 3,	"mah m"		},
	{"N",			BIT_DIGITAL,	DrvJoy5 + 3,	"mah n"		},
	{"Pon",			BIT_DIGITAL,	DrvJoy7 + 3,	"mah pon"	},
	{"Chi",			BIT_DIGITAL,	DrvJoy6 + 3,	"mah chi"	},
	{"Kan",			BIT_DIGITAL,	DrvJoy4 + 4,	"mah kan"	},
	{"Ron",			BIT_DIGITAL,	DrvJoy6 + 4,	"mah ron"	},
	{"Reach",		BIT_DIGITAL,	DrvJoy5 + 4,	"mah reach"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Hypreac2)

static struct BurnInputInfo Srmp7InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy7 + 0,	"p1 start"	},
	{"A",			BIT_DIGITAL,	DrvJoy7 + 5,	"mah a"		},
	{"B",			BIT_DIGITAL,	DrvJoy6 + 5,	"mah b"		},
	{"C",			BIT_DIGITAL,	DrvJoy5 + 2,	"mah c"		},
	{"D",			BIT_DIGITAL,	DrvJoy8 + 5,	"mah d"		},
	{"E",			BIT_DIGITAL,	DrvJoy7 + 4,	"mah e"		},
	{"F",			BIT_DIGITAL,	DrvJoy6 + 4,	"mah f"		},
	{"G",			BIT_DIGITAL,	DrvJoy5 + 4,	"mah g"		},
	{"H",			BIT_DIGITAL,	DrvJoy8 + 4,	"mah h"		},
	{"I",			BIT_DIGITAL,	DrvJoy7 + 3,	"mah i"		},
	{"J",			BIT_DIGITAL,	DrvJoy6 + 3,	"mah j"		},
	{"K",			BIT_DIGITAL,	DrvJoy5 + 3,	"mah k"		},
	{"L",			BIT_DIGITAL,	DrvJoy8 + 3,	"mah l"		},
	{"M",			BIT_DIGITAL,	DrvJoy7 + 2,	"mah m"		},
	{"N",			BIT_DIGITAL,	DrvJoy6 + 2,	"mah n"		},
	{"Pon",			BIT_DIGITAL,	DrvJoy8 + 2,	"mah pon"	},
	{"Chi",			BIT_DIGITAL,	DrvJoy5 + 2,	"mah chi"	},
	{"Kan",			BIT_DIGITAL,	DrvJoy7 + 1,	"mah kan"	},
	{"Ron",			BIT_DIGITAL,	DrvJoy5 + 1,	"mah ron"	},
	{"Reach",		BIT_DIGITAL,	DrvJoy6 + 1,	"mah reach"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Srmp7)

static struct BurnInputInfo SxyreactInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},

	// placeholder for analog inputs
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sxyreact)

static struct BurnInputInfo EaglshotInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},

	// placeholder for analog inputs
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 5"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Eaglshot)

static struct BurnDIPInfo VasaraDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0x7f, NULL			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x02, 0x02, "Off"			},
	{0x14, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x04, 0x04, "Off"			},
	{0x14, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x08, 0x00, "Off"			},
	{0x14, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x02, "Easy"			},
	{0x15, 0x01, 0x03, 0x03, "Normal"		},
	{0x15, 0x01, 0x03, 0x01, "Hard"			},
	{0x15, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bomber Stock"		},
	{0x15, 0x01, 0x0c, 0x00, "0"			},
	{0x15, 0x01, 0x0c, 0x04, "1"			},
	{0x15, 0x01, 0x0c, 0x0c, "2"			},
	{0x15, 0x01, 0x0c, 0x08, "3"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x30, 0x00, "1"			},
	{0x15, 0x01, 0x30, 0x10, "2"			},
	{0x15, 0x01, 0x30, 0x30, "3"			},
	{0x15, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "Game Voice"		},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "English Subtitles"	},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Vasara)

static struct BurnDIPInfo Vasara2DIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x02, 0x02, "Off"			},
	{0x14, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x04, 0x04, "Off"			},
	{0x14, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x08, 0x00, "Off"			},
	{0x14, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x02, "Easy"			},
	{0x15, 0x01, 0x03, 0x03, "Normal"		},
	{0x15, 0x01, 0x03, 0x01, "Hard"			},
	{0x15, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x0c, 0x00, "1"			},
	{0x15, 0x01, 0x0c, 0x04, "2"			},
	{0x15, 0x01, 0x0c, 0x0c, "3"			},
	{0x15, 0x01, 0x0c, 0x08, "5"			},

	{0   , 0xfe, 0   ,    2, "Game Voice"		},
	{0x15, 0x01, 0x10, 0x00, "Off"			},
	{0x15, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Vasara Stock"		},
	{0x15, 0x01, 0x20, 0x20, "2"			},
	{0x15, 0x01, 0x20, 0x00, "3"			},

	{0   , 0xfe, 0   ,    2, "English Subtitles"	},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Secret Character"	},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Vasara2)

static struct BurnDIPInfo SurvartsDIPList[]=
{
	{0x1b, 0xff, 0xff, 0xff, NULL					},
	{0x1c, 0xff, 0xff, 0xfd, NULL					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x1b, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"			},
	{0x1b, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"			},
	{0x1b, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"			},
	{0x1b, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"			},
	{0x1b, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"			},
	{0x1b, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"			},
	{0x1b, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"			},
	{0x1b, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"			},
	{0x1b, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"			},
	{0x1b, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"			},
	{0x1b, 0x01, 0x0f, 0x05, "Multiple Coin Feature A"		},
	{0x1b, 0x01, 0x0f, 0x04, "Multiple Coin Feature B"		},
	{0x1b, 0x01, 0x0f, 0x03, "Multiple Coin Feature C"		},
	{0x1b, 0x01, 0x0f, 0x02, "Multiple Coin Feature D"		},
	{0x1b, 0x01, 0x0f, 0x01, "Multiple Coin Feature E"		},
	{0x1b, 0x01, 0x0f, 0x00, "2 Credits Start, 1 to continue"	},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x1b, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"			},
	{0x1b, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"			},
	{0x1b, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"			},
	{0x1b, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"			},
	{0x1b, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"			},
	{0x1b, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"			},
	{0x1b, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"			},
	{0x1b, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"			},
	{0x1b, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"			},
	{0x1b, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"			},
	{0x1b, 0x01, 0xf0, 0x50, "Multiple Coin Feature A"		},
	{0x1b, 0x01, 0xf0, 0x40, "Multiple Coin Feature B"		},
	{0x1b, 0x01, 0xf0, 0x30, "Multiple Coin Feature C"		},
	{0x1b, 0x01, 0xf0, 0x20, "Multiple Coin Feature D"		},
	{0x1b, 0x01, 0xf0, 0x10, "Multiple Coin Feature E"		},
	{0x1b, 0x01, 0xf0, 0x00, "2 Credits Start, 1 to continue"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x1c, 0x01, 0x01, 0x01, "Off"					},
	{0x1c, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x1c, 0x01, 0x02, 0x02, "Off"					},
	{0x1c, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Timer Speed"				},
	{0x1c, 0x01, 0x04, 0x04, "Normal"				},
	{0x1c, 0x01, 0x04, 0x00, "Fast"					},

	{0   , 0xfe, 0   ,    2, "Damage Level"				},
	{0x1c, 0x01, 0x08, 0x08, "Normal"				},
	{0x1c, 0x01, 0x08, 0x00, "High"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x1c, 0x01, 0x30, 0x10, "Easy"					},
	{0x1c, 0x01, 0x30, 0x30, "Normal"				},
	{0x1c, 0x01, 0x30, 0x20, "Hard"					},
	{0x1c, 0x01, 0x30, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Fatal Damage"				},
	{0x1c, 0x01, 0xc0, 0x40, "Light"				},
	{0x1c, 0x01, 0xc0, 0xc0, "Normal"				},
	{0x1c, 0x01, 0xc0, 0x80, "Heavy"				},
	{0x1c, 0x01, 0xc0, 0x00, "Heaviest"				},
};

STDDIPINFO(Survarts)

static struct BurnDIPInfo DynagearDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL				},
	{0x16, 0xff, 0xff, 0xfd, NULL				},

	{0   , 0xfe, 0   ,    15, "Coin A"			},
	{0x15, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x15, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x15, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x15, 0x01, 0x0f, 0x05, "Multiple Coin Feature A"	},
	{0x15, 0x01, 0x0f, 0x04, "Multiple Coin Feature B"	},
	{0x15, 0x01, 0x0f, 0x03, "Multiple Coin Feature C"	},
	{0x15, 0x01, 0x0f, 0x02, "Multiple Coin Feature D"	},
	{0x15, 0x01, 0x0f, 0x01, "Multiple Coin Feature E"	},

	{0   , 0xfe, 0   ,    15, "Coin B"			},
	{0x15, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x15, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x15, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x15, 0x01, 0xf0, 0x50, "Multiple Coin Feature A"	},
	{0x15, 0x01, 0xf0, 0x40, "Multiple Coin Feature B"	},
	{0x15, 0x01, 0xf0, 0x30, "Multiple Coin Feature C"	},
	{0x15, 0x01, 0xf0, 0x20, "Multiple Coin Feature D"	},
	{0x15, 0x01, 0xf0, 0x10, "Multiple Coin Feature E"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x16, 0x01, 0x01, 0x01, "Off"				},
	{0x16, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x02, 0x02, "Off"				},
	{0x16, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x16, 0x01, 0x0c, 0x08, "Easy"				},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"			},
	{0x16, 0x01, 0x0c, 0x04, "Hard"				},
	{0x16, 0x01, 0x0c, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x16, 0x01, 0x30, 0x10, "1"				},
	{0x16, 0x01, 0x30, 0x30, "2"				},
	{0x16, 0x01, 0x30, 0x20, "3"				},
	{0x16, 0x01, 0x30, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x16, 0x01, 0x40, 0x40, "Off"				},
	{0x16, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Health"			},
	{0x16, 0x01, 0x80, 0x00, "3 Hearts"			},
	{0x16, 0x01, 0x80, 0x80, "4 Hearts"			},
};

STDDIPINFO(Dynagear)

static struct BurnDIPInfo KeithlcyDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xff, NULL			},
	{0x10, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0f, 0x01, 0x02, 0x02, "Off"			},
	{0x0f, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0f, 0x01, 0x04, 0x04, "Off"			},
	{0x0f, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0f, 0x01, 0x08, 0x00, "Off"			},
	{0x0f, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0f, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x10, 0x01, 0x03, 0x02, "Easy"			},
	{0x10, 0x01, 0x03, 0x03, "Normal"		},
	{0x10, 0x01, 0x03, 0x01, "Hard"			},
	{0x10, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x0c, 0x08, "2"			},
	{0x10, 0x01, 0x0c, 0x0c, "3"			},
	{0x10, 0x01, 0x0c, 0x04, "4"			},
	{0x10, 0x01, 0x0c, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x30, 0x30, "Every 100k"		},
	{0x10, 0x01, 0x30, 0x20, "Every 150k"		},
	{0x10, 0x01, 0x30, 0x10, "100k & Every 200K"	},
	{0x10, 0x01, 0x30, 0x00, "Every 200k"		},
};

STDDIPINFO(Keithlcy)

static struct BurnDIPInfo Twineag2DIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL				},
	{0x16, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    15, "Coin A"			},
	{0x15, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x15, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x15, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x15, 0x01, 0x0f, 0x05, "Multiple Coin Feature A"	},
	{0x15, 0x01, 0x0f, 0x04, "Multiple Coin Feature B"	},
	{0x15, 0x01, 0x0f, 0x03, "Multiple Coin Feature C"	},
	{0x15, 0x01, 0x0f, 0x02, "Multiple Coin Feature D"	},
	{0x15, 0x01, 0x0f, 0x01, "Multiple Coin Feature E"	},

	{0   , 0xfe, 0   ,    15, "Coin B"			},
	{0x15, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x15, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x15, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x15, 0x01, 0xf0, 0x50, "Multiple Coin Feature A"	},
	{0x15, 0x01, 0xf0, 0x40, "Multiple Coin Feature B"	},
	{0x15, 0x01, 0xf0, 0x30, "Multiple Coin Feature C"	},
	{0x15, 0x01, 0xf0, 0x20, "Multiple Coin Feature D"	},
	{0x15, 0x01, 0xf0, 0x10, "Multiple Coin Feature E"	},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x16, 0x01, 0x07, 0x06, "Easiest"			},
	{0x16, 0x01, 0x07, 0x05, "Easier"			},
	{0x16, 0x01, 0x07, 0x04, "Easy"				},
	{0x16, 0x01, 0x07, 0x07, "Normal"			},
	{0x16, 0x01, 0x07, 0x03, "Medium"			},
	{0x16, 0x01, 0x07, 0x02, "Hard"				},
	{0x16, 0x01, 0x07, 0x01, "Harder"			},
	{0x16, 0x01, 0x07, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x16, 0x01, 0x08, 0x08, "Off"				},
	{0x16, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x16, 0x01, 0x10, 0x00, "2"				},
	{0x16, 0x01, 0x10, 0x10, "3"				},

	{0   , 0xfe, 0   ,    2, "Pause"			},
	{0x16, 0x01, 0x20, 0x20, "Off"				},
	{0x16, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x16, 0x01, 0x40, 0x40, "Off"				},
	{0x16, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x16, 0x01, 0x80, 0x80, "Off"				},
	{0x16, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Twineag2)

static struct BurnDIPInfo Drifto94DIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x01, 0x01, "Off"			},
	{0x15, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x02, 0x02, "Off"			},
	{0x15, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Sound Test"		},
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
	{0x16, 0x01, 0x03, 0x03, "Normal"		},
	{0x16, 0x01, 0x03, 0x02, "Easy"			},
	{0x16, 0x01, 0x03, 0x01, "Hard"			},
	{0x16, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Music Volume"		},
	{0x16, 0x01, 0x10, 0x00, "Quiet"		},
	{0x16, 0x01, 0x10, 0x10, "Loud"			},

	{0   , 0xfe, 0   ,    2, "Sound Volume"		},
	{0x16, 0x01, 0x20, 0x00, "Quiet"		},
	{0x16, 0x01, 0x20, 0x20, "Loud"			},

	{0   , 0xfe, 0   ,    2, "Save Best Time"	},
	{0x16, 0x01, 0x40, 0x00, "No"			},
	{0x16, 0x01, 0x40, 0x40, "Yes"			},
};

STDDIPINFO(Drifto94)

static struct BurnDIPInfo MeosismDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL			},
	{0x0f, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0e, 0x01, 0x02, 0x02, "Off"			},
	{0x0e, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0e, 0x01, 0x03, 0x03, "1 Medal/1 Credit"	},
	{0x0e, 0x01, 0x03, 0x01, "1 Medal/5 Credits"	},
	{0x0e, 0x01, 0x03, 0x02, "1 Medal/10 Credits"	},
	{0x0e, 0x01, 0x03, 0x00, "1 Medal/20 Credits"	},

	{0   , 0xfe, 0   ,    4, "Demo Sounds"		},
	{0x0e, 0x01, 0x04, 0x00, "Off"			},
	{0x0e, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Attendant Pay"	},
	{0x0e, 0x01, 0x08, 0x00, "No"			},
	{0x0e, 0x01, 0x08, 0x08, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Medals Payout"	},
	{0x0e, 0x01, 0x10, 0x10, "400"			},
	{0x0e, 0x01, 0x10, 0x00, "800"			},

	{0   , 0xfe, 0   ,    2, "Max Credits"		},
	{0x0e, 0x01, 0x20, 0x20, "5000"			},
	{0x0e, 0x01, 0x20, 0x00, "9999"			},

	{0   , 0xfe, 0   ,    2, "Hopper"		},
	{0x0e, 0x01, 0x40, 0x00, "No"			},
	{0x0e, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Reel Speed"		},
	{0x0e, 0x01, 0x80, 0x80, "Low"			},
	{0x0e, 0x01, 0x80, 0x00, "High"			},

	{0   , 0xfe, 0   ,    4, "Game Rate"		},
	{0x0f, 0x01, 0x03, 0x00, "80%"			},
	{0x0f, 0x01, 0x03, 0x02, "85%"			},
	{0x0f, 0x01, 0x03, 0x03, "90%"			},
	{0x0f, 0x01, 0x03, 0x01, "95%"			},

	{0   , 0xfe, 0   ,    2, "Controls"		},
	{0x0f, 0x01, 0x20, 0x20, "Simple"		},
	{0x0f, 0x01, 0x20, 0x00, "Complex"		},

	{0   , 0xfe, 0   ,    2, "Coin Sensor"		},
	{0x0f, 0x01, 0x40, 0x40, "Active High"		},
	{0x0f, 0x01, 0x40, 0x00, "Active Low"		},

	{0   , 0xfe, 0   ,    2, "Hopper Sensor"	},
	{0x0f, 0x01, 0x80, 0x80, "Off"			},
	{0x0f, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Meosism)

static struct BurnDIPInfo CairbladDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x15, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x07, 0x02, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x07, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x15, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x38, 0x20, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x38, 0x18, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x38, 0x10, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x38, 0x08, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x38, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x01, 0x01, "Off"			},
	{0x16, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x16, 0x01, 0x02, 0x00, "Off"			},
	{0x16, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x16, 0x01, 0x0c, 0x08, "Easy"			},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x16, 0x01, 0x0c, 0x04, "Hard"			},
	{0x16, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x16, 0x01, 0x10, 0x10, "Off"			},
	{0x16, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x16, 0x01, 0x60, 0x40, "Every 2 Mil"		},
	{0x16, 0x01, 0x60, 0x60, "2 Mil/6 Mil"		},
	{0x16, 0x01, 0x60, 0x20, "4 Million"		},
	{0x16, 0x01, 0x60, 0x00, "None"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x80, 0x80, "Off"			},
	{0x16, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Cairblad)

static struct BurnDIPInfo UltraxDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xef, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x15, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x15, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x16, 0x01, 0x03, 0x02, "Easy"			},
	{0x16, 0x01, 0x03, 0x03, "Normal"		},
	{0x16, 0x01, 0x03, 0x01, "Hard"			},
	{0x16, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x16, 0x01, 0x08, 0x08, "Off"			},
	{0x16, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    3, "Region"		},
	{0x16, 0x01, 0x14, 0x00, "China"		},
	{0x16, 0x01, 0x14, 0x14, "Japan"		},
	{0x16, 0x01, 0x14, 0x04, "World"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x16, 0x01, 0x20, 0x00, "Off"			},
	{0x16, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x40, 0x40, "Off"			},
	{0x16, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x80, 0x80, "Off"			},
	{0x16, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Ultrax)

static struct BurnDIPInfo StmbladeDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xfe, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x15, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x07, 0x02, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x07, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x15, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x38, 0x20, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x38, 0x18, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x38, 0x10, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x38, 0x08, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x38, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Rapid Fire"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x01, 0x01, "Off"			},
	{0x16, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x16, 0x01, 0x02, 0x00, "Off"			},
	{0x16, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x16, 0x01, 0x0c, 0x08, "Easy"			},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x16, 0x01, 0x0c, 0x04, "Hard"			},
	{0x16, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x16, 0x01, 0x30, 0x20, "1"			},
	{0x16, 0x01, 0x30, 0x10, "2"			},
	{0x16, 0x01, 0x30, 0x30, "3"			},
	{0x16, 0x01, 0x30, 0x00, "4"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x16, 0x01, 0x40, 0x40, "600000"		},
	{0x16, 0x01, 0x40, 0x00, "800000"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x80, 0x80, "Off"			},
	{0x16, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Stmblade)

static struct BurnDIPInfo RyoriohDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xff, NULL			},
	{0x10, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0f, 0x01, 0x02, 0x02, "Off"			},
	{0x0f, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0f, 0x01, 0x04, 0x04, "Off"			},
	{0x0f, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0f, 0x01, 0x08, 0x00, "Off"			},
	{0x0f, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x10, 0x01, 0x03, 0x02, "Easy"			},
	{0x10, 0x01, 0x03, 0x03, "Normal"		},
	{0x10, 0x01, 0x03, 0x01, "Hard"			},
	{0x10, 0x01, 0x03, 0x00, "Hardest"		},
};

STDDIPINFO(Ryorioh)

static struct BurnDIPInfo MsliderDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xfe, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x15, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x07, 0x02, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x07, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x15, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x38, 0x20, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x38, 0x18, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x38, 0x10, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x38, 0x08, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x38, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x40, 0x40, "Off"			},
	{0x15, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x01, 0x01, "On"			},
	{0x16, 0x01, 0x01, 0x00, "Off"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x16, 0x01, 0x02, 0x00, "Off"			},
	{0x16, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x16, 0x01, 0x0c, 0x08, "Easy"			},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x16, 0x01, 0x0c, 0x04, "Hard"			},
	{0x16, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Rounds (Vs Mode)"	},
	{0x16, 0x01, 0x30, 0x00, "1"			},
	{0x16, 0x01, 0x30, 0x30, "2"			},
	{0x16, 0x01, 0x30, 0x20, "3"			},
	{0x16, 0x01, 0x30, 0x10, "4"			},
};

STDDIPINFO(Mslider)

static struct BurnDIPInfo GdfsDIPList[]=
{
	{0x1a, 0xff, 0xff, 0xff, NULL				},
	{0x1b, 0xff, 0xff, 0xf7, NULL				},

	{0   , 0xfe, 0   ,    2, "Controls"			},
	{0x1a, 0x01, 0x01, 0x01, "Joystick"			},
	{0x1a, 0x01, 0x01, 0x00, "Light Gun"			},

	{0   , 0xfe, 0   ,    2, "Light Gun Calibration"	},
	{0x1a, 0x01, 0x02, 0x02, "Off"				},
	{0x1a, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Level Select"			},
	{0x1a, 0x01, 0x04, 0x04, "Off"				},
	{0x1a, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    3, "Coinage"			},
	{0x1a, 0x01, 0x18, 0x10, "2 Coins 1 Credits"		},
	{0x1a, 0x01, 0x18, 0x18, "1 Coin  1 Credits"		},
	{0x1a, 0x01, 0x18, 0x08, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Save Scores"			},
	{0x1a, 0x01, 0x20, 0x00, "No"				},
	{0x1a, 0x01, 0x20, 0x20, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x1a, 0x01, 0x40, 0x40, "Off"				},
	{0x1a, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Invert X Axis"		},
	{0x1b, 0x01, 0x01, 0x01, "Off"				},
	{0x1b, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x1b, 0x01, 0x08, 0x00, "English"			},
	{0x1b, 0x01, 0x08, 0x08, "Japanese"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x1b, 0x01, 0x10, 0x00, "Off"				},
	{0x1b, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Damage From Machine Gun"	},
	{0x1b, 0x01, 0x20, 0x20, "Light"			},
	{0x1b, 0x01, 0x20, 0x00, "Heavy"			},

	{0   , 0xfe, 0   ,    2, "Damage From Beam Cannon"	},
	{0x1b, 0x01, 0x40, 0x40, "Light"			},
	{0x1b, 0x01, 0x40, 0x00, "Heavy"			},

	{0   , 0xfe, 0   ,    2, "Damage From Missle"		},
	{0x1b, 0x01, 0x80, 0x80, "Light"			},
	{0x1b, 0x01, 0x80, 0x00, "Heavy"			},
};

STDDIPINFO(Gdfs)

static struct BurnDIPInfo Janjans1DIPList[]=
{
	{0x29, 0xff, 0xff, 0xff, NULL			},
	{0x2a, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x29, 0x01, 0x02, 0x02, "Off"			},
	{0x29, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x29, 0x01, 0x04, 0x04, "Off"			},
	{0x29, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x29, 0x01, 0x08, 0x00, "Off"			},
	{0x29, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x29, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x29, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x29, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x29, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Voice"		},
	{0x29, 0x01, 0x40, 0x00, "Off"			},
	{0x29, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x2a, 0x01, 0x03, 0x02, "Easy"			},
	{0x2a, 0x01, 0x03, 0x03, "Normal"		},
	{0x2a, 0x01, 0x03, 0x01, "Hard"			},
	{0x2a, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Nudity"		},
	{0x2a, 0x01, 0x04, 0x00, "Off"			},
	{0x2a, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Mini Game"		},
	{0x2a, 0x01, 0x08, 0x00, "Off"			},
	{0x2a, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Initial Score"	},
	{0x2a, 0x01, 0x30, 0x20, "1000"			},
	{0x2a, 0x01, 0x30, 0x30, "1500"			},
	{0x2a, 0x01, 0x30, 0x10, "2000"			},
	{0x2a, 0x01, 0x30, 0x00, "3000"			},

	{0   , 0xfe, 0   ,    3, "Communication"	},
	{0x2a, 0x01, 0xc0, 0xc0, "None"			},
	{0x2a, 0x01, 0xc0, 0x40, "Board 1 (Main)"	},
	{0x2a, 0x01, 0xc0, 0x00, "Board 2 (Sub)"	},
};

STDDIPINFO(Janjans1)

static struct BurnDIPInfo Janjans2DIPList[]=
{
	{0x29, 0xff, 0xff, 0xff, NULL			},
	{0x2a, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x29, 0x01, 0x02, 0x02, "Off"			},
	{0x29, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x29, 0x01, 0x04, 0x04, "Off"			},
	{0x29, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x29, 0x01, 0x08, 0x00, "Off"			},
	{0x29, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x29, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x29, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x29, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x29, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Nudity"		},
	{0x29, 0x01, 0x40, 0x00, "Off"			},
	{0x29, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x2a, 0x01, 0x03, 0x02, "Easy"			},
	{0x2a, 0x01, 0x03, 0x03, "Normal"		},
	{0x2a, 0x01, 0x03, 0x01, "Hard"			},
	{0x2a, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Initial Score (vs)"	},
	{0x2a, 0x01, 0x0c, 0x08, "10000"		},
	{0x2a, 0x01, 0x0c, 0x04, "15000"		},
	{0x2a, 0x01, 0x0c, 0x0c, "20000"		},
	{0x2a, 0x01, 0x0c, 0x00, "25000"		},

	{0   , 0xfe, 0   ,    4, "Initial Score (solo)"	},
	{0x2a, 0x01, 0x30, 0x20, "1000"			},
	{0x2a, 0x01, 0x30, 0x30, "1500"			},
	{0x2a, 0x01, 0x30, 0x10, "2000"			},
	{0x2a, 0x01, 0x30, 0x00, "3000"			},

	{0   , 0xfe, 0   ,    3, "Communication"	},
	{0x2a, 0x01, 0xc0, 0xc0, "None"			},
	{0x2a, 0x01, 0xc0, 0x40, "Transmitter"		},
	{0x2a, 0x01, 0xc0, 0x00, "Receiver"		},
};

STDDIPINFO(Janjans2)

static struct BurnDIPInfo Koikois2DIPList[]=
{
	{0x29, 0xff, 0xff, 0xff, NULL			},
	{0x2a, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x29, 0x01, 0x02, 0x02, "Off"			},
	{0x29, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x29, 0x01, 0x04, 0x04, "Off"			},
	{0x29, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x29, 0x01, 0x08, 0x00, "Off"			},
	{0x29, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x29, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x29, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x29, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x29, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Voice"		},
	{0x29, 0x01, 0x40, 0x00, "Off"			},
	{0x29, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Controls"		},
	{0x29, 0x01, 0x80, 0x80, "Joystick"		},
	{0x29, 0x01, 0x80, 0x00, "Keyboard"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x2a, 0x01, 0x03, 0x02, "Easy"			},
	{0x2a, 0x01, 0x03, 0x03, "Normal"		},
	{0x2a, 0x01, 0x03, 0x01, "Hard"			},
	{0x2a, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Nudity"		},
	{0x2a, 0x01, 0x04, 0x00, "No"			},
	{0x2a, 0x01, 0x04, 0x04, "Yes"			},

	{0   , 0xfe, 0   ,    3, "Communication"	},
	{0x2a, 0x01, 0xc0, 0xc0, "None"			},
	{0x2a, 0x01, 0xc0, 0x40, "Board 1 (Main)"	},
	{0x2a, 0x01, 0xc0, 0x00, "Board 2 (Sub)"	},
};

STDDIPINFO(Koikois2)

static struct BurnDIPInfo Srmp4DIPList[]=
{
	{0x29, 0xff, 0xff, 0xff, NULL			},
	{0x2a, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x29, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x29, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x29, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x29, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x29, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x29, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x29, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x29, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x29, 0x01, 0x38, 0x00, "5 Coins 1 Credits"	},
	{0x29, 0x01, 0x38, 0x08, "4 Coins 1 Credits"	},
	{0x29, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x29, 0x01, 0x38, 0x18, "2 Coins 1 Credits"	},
	{0x29, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x29, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x29, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x29, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x29, 0x01, 0x40, 0x40, "Off"			},
	{0x29, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x2a, 0x01, 0x07, 0x06, "Easiest"		},
	{0x2a, 0x01, 0x07, 0x05, "Easier"		},
	{0x2a, 0x01, 0x07, 0x04, "Easy"			},
	{0x2a, 0x01, 0x07, 0x07, "Normal"		},
	{0x2a, 0x01, 0x07, 0x03, "Medium"		},
	{0x2a, 0x01, 0x07, 0x02, "Hard"			},
	{0x2a, 0x01, 0x07, 0x01, "Harder"		},
	{0x2a, 0x01, 0x07, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x2a, 0x01, 0x08, 0x08, "Off"			},
	{0x2a, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x2a, 0x01, 0x10, 0x00, "Off"			},
	{0x2a, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x2a, 0x01, 0x20, 0x20, "Off"			},
	{0x2a, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x2a, 0x01, 0x40, 0x00, "No"			},
	{0x2a, 0x01, 0x40, 0x40, "Yes"			},
};

STDDIPINFO(Srmp4)

static struct BurnDIPInfo HypreactDIPList[]=
{
	{0x1d, 0xff, 0xff, 0xff, NULL				},
	{0x1e, 0xff, 0xff, 0xef, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x1d, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x1d, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x1d, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x1d, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x1d, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x1d, 0x01, 0x07, 0x02, "1 Coin  4 Credits"	},
	{0x1d, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},
	{0x1d, 0x01, 0x07, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x1d, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x1d, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x1d, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x1d, 0x01, 0x38, 0x20, "1 Coin  2 Credits"	},
	{0x1d, 0x01, 0x38, 0x18, "1 Coin  3 Credits"	},
	{0x1d, 0x01, 0x38, 0x10, "1 Coin  4 Credits"	},
	{0x1d, 0x01, 0x38, 0x08, "1 Coin  5 Credits"	},
	{0x1d, 0x01, 0x38, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Half Coins To Continue"	},
	{0x1d, 0x01, 0x40, 0x40, "No"				},
	{0x1d, 0x01, 0x40, 0x00, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x1d, 0x01, 0x80, 0x80, "Off"				},
	{0x1d, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x1e, 0x01, 0x01, 0x01, "Off"				},
	{0x1e, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x1e, 0x01, 0x02, 0x00, "Off"				},
	{0x1e, 0x01, 0x02, 0x02, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x1e, 0x01, 0x0c, 0x08, "Easy"				},
	{0x1e, 0x01, 0x0c, 0x0c, "Normal"			},
	{0x1e, 0x01, 0x0c, 0x04, "Hard"				},
	{0x1e, 0x01, 0x0c, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Controls"			},
	{0x1e, 0x01, 0x10, 0x10, "Keyboard"			},
	{0x1e, 0x01, 0x10, 0x00, "Joystick"			},

	{0   , 0xfe, 0   ,    2, "Multiple coins"		},
	{0x1e, 0x01, 0x20, 0x00, "Off"				},
	{0x1e, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Keep Status On Continue"	},
	{0x1e, 0x01, 0x40, 0x00, "No"				},
	{0x1e, 0x01, 0x40, 0x40, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x1e, 0x01, 0x80, 0x80, "Off"				},
	{0x1e, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Hypreact)

static struct BurnDIPInfo Hypreac2DIPList[]=
{
	{0x28, 0xff, 0xff, 0xff, NULL			},
	{0x29, 0xff, 0xff, 0xef, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x28, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x28, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x28, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x28, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x28, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x28, 0x01, 0x07, 0x02, "1 Coin  4 Credits"	},
	{0x28, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},
	{0x28, 0x01, 0x07, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x28, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x28, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x28, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x28, 0x01, 0x38, 0x20, "1 Coin  2 Credits"	},
	{0x28, 0x01, 0x38, 0x18, "1 Coin  3 Credits"	},
	{0x28, 0x01, 0x38, 0x10, "1 Coin  4 Credits"	},
	{0x28, 0x01, 0x38, 0x08, "1 Coin  5 Credits"	},
	{0x28, 0x01, 0x38, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "1/2 Coins To Continue"},
	{0x28, 0x01, 0x40, 0x40, "No"			},
	{0x28, 0x01, 0x40, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x28, 0x01, 0x80, 0x80, "Off"			},
	{0x28, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x29, 0x01, 0x01, 0x01, "Off"			},
	{0x29, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x29, 0x01, 0x02, 0x00, "Off"			},
	{0x29, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x29, 0x01, 0x0c, 0x08, "Easy"			},
	{0x29, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x29, 0x01, 0x0c, 0x04, "Hard"			},
	{0x29, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Controls"		},
	{0x29, 0x01, 0x10, 0x10, "Keyboard"		},
	{0x29, 0x01, 0x10, 0x00, "Joystick"		},

	{0   , 0xfe, 0   ,    2, "Communication"	},
	{0x29, 0x01, 0x20, 0x20, "Off"			},
	{0x29, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Communication Mode"	},
	{0x29, 0x01, 0x40, 0x40, "SLAVE"		},
	{0x29, 0x01, 0x40, 0x00, "MASTER"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x29, 0x01, 0x80, 0x80, "Off"			},
	{0x29, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Hypreac2)

static struct BurnDIPInfo Srmp7DIPList[]=
{
	{0x18, 0xff, 0xff, 0xc7, NULL			},
	{0x19, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x18, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x18, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x18, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x18, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x18, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x18, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x18, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x18, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Re-cloth"		},
	{0x18, 0x01, 0x40, 0x00, "Off"			},
	{0x18, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Nudity"		},
	{0x18, 0x01, 0x80, 0x00, "Off"			},
	{0x18, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x19, 0x01, 0x07, 0x06, "Easiest"		},
	{0x19, 0x01, 0x07, 0x05, "Easier"		},
	{0x19, 0x01, 0x07, 0x04, "Easy"			},
	{0x19, 0x01, 0x07, 0x07, "Normal"		},
	{0x19, 0x01, 0x07, 0x03, "Medium"		},
	{0x19, 0x01, 0x07, 0x02, "Hard"			},
	{0x19, 0x01, 0x07, 0x01, "Harder"		},
	{0x19, 0x01, 0x07, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Kuitan"		},
	{0x19, 0x01, 0x08, 0x00, "Off"			},
	{0x19, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x19, 0x01, 0x10, 0x00, "Off"			},
	{0x19, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x19, 0x01, 0x20, 0x00, "Off"			},
	{0x19, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x19, 0x01, 0x40, 0x40, "Off"			},
	{0x19, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x19, 0x01, 0x80, 0x80, "Off"			},
	{0x19, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Srmp7)

static struct BurnDIPInfo SxyreactDIPList[]=
{
	{0x17, 0xff, 0xff, 0xff, NULL			},
	{0x18, 0xff, 0xff, 0xef, NULL			},

	{0   , 0xfe, 0   ,    7, "Coin A"		},
	{0x17, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x17, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x17, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x17, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x17, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x17, 0x01, 0x07, 0x02, "1 Coin  4 Credits"	},
	{0x17, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    2, "Credits To Play"	},
	{0x17, 0x01, 0x40, 0x40, "1"			},
	{0x17, 0x01, 0x40, 0x00, "2"			},

	{0   , 0xfe, 0   ,    2, "Buy Balls With Credits"},
	{0x17, 0x01, 0x80, 0x00, "Off"			},
	{0x17, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x18, 0x01, 0x01, 0x01, "Off"			},
	{0x18, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x18, 0x01, 0x02, 0x00, "Off"			},
	{0x18, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x18, 0x01, 0x0c, 0x08, "Easy"			},
	{0x18, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x18, 0x01, 0x0c, 0x04, "Hard"			},
	{0x18, 0x01, 0x0c, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Controls"		},
	{0x18, 0x01, 0x10, 0x10, "Dial"			},
	{0x18, 0x01, 0x10, 0x00, "Joystick"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x18, 0x01, 0x20, 0x20, "Off"			},
	{0x18, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x18, 0x01, 0x40, 0x40, "Off"			},
	{0x18, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Sxyreact)

static struct BurnDIPInfo JskDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x15, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x08, 0x08, "Off"			},
	{0x15, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x10, 0x10, "Off"			},
	{0x15, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x20, 0x00, "Off"			},
	{0x15, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    4, "Minutes"		},
	{0x15, 0x01, 0xc0, 0x80, "3"			},
	{0x15, 0x01, 0xc0, 0xc0, "4"			},
	{0x15, 0x01, 0xc0, 0x40, "5"			},
	{0x15, 0x01, 0xc0, 0x00, "6"			},

	{0   , 0xfe, 0   ,    8, "Difficulty A"		},
	{0x16, 0x01, 0x07, 0x00, "1 (Novice)"		},
	{0x16, 0x01, 0x07, 0x01, "2"			},
	{0x16, 0x01, 0x07, 0x02, "3"			},
	{0x16, 0x01, 0x07, 0x03, "4"			},
	{0x16, 0x01, 0x07, 0x07, "5 (Medium)"		},
	{0x16, 0x01, 0x07, 0x06, "6"			},
	{0x16, 0x01, 0x07, 0x05, "7"			},
	{0x16, 0x01, 0x07, 0x04, "8 (expert)"		},

	{0   , 0xfe, 0   ,    2, "Difficulty Switch"	},
	{0x16, 0x01, 0x08, 0x08, "A (8 Levels)"		},
	{0x16, 0x01, 0x08, 0x00, "B (4 Levels)"		},

	{0   , 0xfe, 0   ,    4, "Difficulty B"		},
	{0x16, 0x01, 0x30, 0x20, "Easy"			},
	{0x16, 0x01, 0x30, 0x30, "Normal"		},
	{0x16, 0x01, 0x30, 0x10, "Hard"			},
	{0x16, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Campaign"		},
	{0x16, 0x01, 0x40, 0x40, "Available"		},
	{0x16, 0x01, 0x40, 0x00, "Finished"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x16, 0x01, 0x80, 0x80, "Off"			},
	{0x16, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Jsk)

static struct BurnDIPInfo EaglshotDIPList[]=
{
	{0x17, 0xff, 0xff, 0xdf, NULL				},
	{0x18, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,   16, "Coinage"			},
	{0x17, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x17, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x17, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x17, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x17, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x17, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x17, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x17, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x17, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x17, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x17, 0x01, 0x0f, 0x05, "Multiple Coin Feature A"	},
	{0x17, 0x01, 0x0f, 0x04, "Multiple Coin Feature B"	},
	{0x17, 0x01, 0x0f, 0x03, "Multiple Coin Feature C"	},
	{0x17, 0x01, 0x0f, 0x02, "Multiple Coin Feature D"	},
	{0x17, 0x01, 0x0f, 0x01, "Multiple Coin Feature E"	},
	{0x17, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Credits To Start"		},
	{0x17, 0x01, 0x10, 0x10, "1"				},
	{0x17, 0x01, 0x10, 0x00, "2"				},

	{0   , 0xfe, 0   ,    2, "Controls"			},
	{0x17, 0x01, 0x20, 0x20, "Trackball"			},
	{0x17, 0x01, 0x20, 0x00, "Joystick"			},

	{0   , 0xfe, 0   ,    2, "Trackball Type"		},
	{0x17, 0x01, 0x40, 0x40, "24 Counts (USA)"		},
	{0x17, 0x01, 0x40, 0x00, "12 Counts (Japan)"		},

	{0   , 0xfe, 0   ,    4, "Number Of Holes"		},
	{0x18, 0x01, 0x03, 0x02, "2"				},
	{0x18, 0x01, 0x03, 0x03, "3"				},
	{0x18, 0x01, 0x03, 0x01, "4"				},
	{0x18, 0x01, 0x03, 0x00, "5"				},

	{0   , 0xfe, 0   ,    3, "Difficulty"			},
	{0x18, 0x01, 0x0c, 0x08, "Easy"				},
	{0x18, 0x01, 0x0c, 0x0c, "Normal"			},
	{0x18, 0x01, 0x0c, 0x04, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x18, 0x01, 0x10, 0x00, "Off"				},
	{0x18, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x18, 0x01, 0x20, 0x00, "Off"				},
	{0x18, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x18, 0x01, 0x40, 0x40, "Off"				},
	{0x18, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x18, 0x01, 0x80, 0x80, "Off"				},
	{0x18, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Eaglshot)

static inline void palette_update(INT32 offset)
{
	offset &= 0x1fffc;

	UINT16 *pal = (UINT16*)(DrvPalRAM + offset);

	INT32 r = pal[1] & 0xff;
	INT32 g = pal[0] >> 8;
	INT32 b = pal[0] & 0xff;

	DrvPalette[offset/4] = BurnHighCol(r,g,b,0);
}

static INT32 ssv_irq_callback(INT32 /*state*/)
{
	for (INT32 i = 0; i < 8; i++)
	{
		if (requested_int & (1 << i))
		{
			return DrvVectors[i * 0x10] & 7;
		}
	}

	return 0;
}

static void update_irq_state()
{
	v60SetIRQLine(0, (requested_int & irq_enable) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void irq_ack_write(UINT8 offset)
{
	INT32 level = (offset & 0x70) >> 4;

	requested_int &= ~(1 << level);

	update_irq_state();
}

static void dsp_write(INT32 offset, UINT8 data)
{
	UINT16 *ram = (UINT16*)DrvDspRAM;

	offset = (offset & 0xffe)/2;

	UINT16 temp = ram[offset/2];

	if (offset & 1) {
		temp &= 0xff;
		temp |= data << 8;
	} else {
		temp &= 0xff00;
		temp |= data;
	}

	ram[offset/2] = temp;
}

static UINT16 dsp_read(INT32 offset)
{
	UINT16 *ram = (UINT16*)DrvDspRAM;

	offset = (offset & 0xffe)/2;

	UINT16 temp = ram[offset/2];

	if (offset & 1) {
		temp >>= 8;
	} else {
		temp &= 0xff;
	}

	return temp;
}

static void common_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffff80) == 0x300000) {
		ES5506Write((address & 0x7e)/2, data);
		return;
	}

	if ((address & 0xffe0000) == 0x140000) {
		DrvPalRAM[(address & 0x1ffff)] = data;
		palette_update(address);
		return;
	}

	if ((address & 0xffff80) == 0x1c0000) {
		DrvScrollRAM[(address & 0x7f)] = data;
		return;
	}

	if (address >= 0x230000 && address <= 0x230071) {
		DrvVectors[(address & 0x7f)] = data;
		return;
	}

	if (address >= 0x240000 && address <= 0x240071) {
		irq_ack_write(address);
		return;
	}

	if ((address & 0xfff000) == 0x482000) {
		dsp_write(address,data);
		return;
	}

	switch (address)
	{
		case 0x210000:
		case 0x210001:
			watchdog = 0;
		return;

		case 0x21000e:
		case 0x21000f:	// lockout 1 & 2, counters 4 & 8
			enable_video = data & 0x80;
		return;

		case 0x210010:
		case 0x210011:
			// nop
		return;

		case 0x260000:
		case 0x260001:
			irq_enable = data;
		return;

		case 0x480000:
		case 0x480001:
			if (dsp_enable) snesdsp_write(true, data);
		return;
	}
}

static void common_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffe0000) == 0x140000) {
		UINT16 *p = (UINT16*)(DrvPalRAM + (address & 0x1ffff));
		*p = data;
		palette_update(address);
		return;
	}

	if ((address & 0xffff80) == 0x1c0000) {
		UINT16 *p = (UINT16*)(DrvScrollRAM + (address & 0x7f));
		*p = data;
		return;
	}

	if ((address & 0xffff80) == 0x300000) {
		ES5506Write((address & 0x7e)/2, data);
		return;
	}

	if (address >= 0x230000 && address <= 0x230071) {
		UINT16 *p = (UINT16*)(DrvVectors + (address & 0x7f));
		*p = data;
		return;
	}

	if (address >= 0x240000 && address <= 0x240071) {
		irq_ack_write(address);
		return;
	}

	if ((address & 0xfff000) == 0x482000) {
		dsp_write(address,data);
		return;
	}

	switch (address)
	{
		case 0x210000:
			watchdog = 0;
		return;

		case 0x21000e:
			// lockout 1 & 2, counters 4 & 8
			enable_video = data & 0x80;
		return;

		case 0x210010:
			// nop
		return;

		case 0x260000:
			irq_enable = data;
		return;

		case 0x480000:
		case 0x480001:
			if (dsp_enable) snesdsp_write(true, data);
		return;
	}
}

static UINT16 common_main_read_word(UINT32 address)
{
	if ((address & 0xfff000) == 0x482000) {
		return dsp_read(address);
	}

	if ((address & 0xffff80) == 0x300000) {
		return ES5506Read((address & 0x7e)/2);
	}

	if ((address & 0xffff00) == 0x8c0000) {
		return st0020_blitram_read_word(address);
	}

	if ((address & 0xffff00) == 0x4f000) {
		return 0; // NOP
	}

	switch (address & ~1)
	{
		case 0x1c0000:
			if (vbl_invert) {
				return (vblank) ? 0 : 0x3000;
			} else {
				return (vblank) ? 0x3000 : 0;
			}

		case 0x1c0002:
			return 0;

		case 0x210000:
			watchdog = 0;
			return 0;

		case 0x210002:
			return DrvDips[0];

		case 0x210004:
			return DrvDips[1];

		case 0x210008:
			return DrvInputs[0];

		case 0x21000a:
			return DrvInputs[1];

		case 0x21000c:
			return DrvInputs[2];

		case 0x21000e:
			return 0;

		case 0x210010: // NOP
			return 0;

		case 0x480000:
		case 0x480001:
			if (dsp_enable) return snesdsp_read(true);
			return 0;

		case 0x500008: // survarts
			return DrvInputs[3];

		case 0x510000: // drifto94
		case 0x510001:
		case 0x520000:
		case 0x520001:
			return rand();
	}

	bprintf (0, _T("RW Unmapped: %5.5x\n"), address);

	return 0;
}

static UINT8 common_main_read_byte(UINT32 address)
{
	if ((address & 0xfff000) == 0x482000) {
		return dsp_read(address);
	}

	if ((address & 0xffff80) == 0x300000) {
		return ES5506Read((address & 0x7e)/2);
	}

	switch (address & ~1)
	{
		case 0x1c0000:
			return (vblank) ? 0x3000 : 0;

		case 0x210000:
			watchdog = 0;
			return 0;

		case 0x210002:
			return DrvDips[0];

		case 0x210004:
			return DrvDips[1];

		case 0x210008:
			return DrvInputs[0];

		case 0x21000a:
			return DrvInputs[1];

		case 0x21000c:
			return DrvInputs[2];

		case 0x21000e:
			return 0;

		case 0x480000:
		case 0x480001:
			if (dsp_enable) return snesdsp_read(true);
			return 0;

		case 0x500002: // nop?
		case 0x500004: // nop?
			return 0;

		case 0x500008: // survarts
			return DrvInputs[3];

		case 0x510000: // drifto94
		case 0x510001:
		case 0x520000:
		case 0x520001:
			return rand();
	}

	bprintf (0, _T("RB Unmapped: %5.5x\n"), address);

	return 0;
}

#if 0
static UINT16 gdfs_eeprom_read()
{
	return (((m_gdfs_lightgun_select & 1) ? 0 : 0xff) ^ m_io_gun[m_gdfs_lightgun_select]->read()) | (EEPROMRead() << 8);
}
#endif

static void gdfs_eeprom_write(UINT16 data)
{
	EEPROMWriteBit((data & 0x4000) >> 14);
	EEPROMSetCSLine((data & 0x1000) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
	EEPROMSetClockLine((data & 0x2000) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);

//	if (!(gdfs_eeprom_old & 0x0800) && (data & 0x0800))   // rising clock
//		gdfs_lightgun_select = (data & 0x0300) >> 8;
}

static UINT16 gdfs_read_word(UINT32 address)
{
	if ((address & 0xf00000) == 0x900000) {
		return st0020GfxramReadWord(address);
	}

	switch (address)
	{
		case 0x540000:
		case 0x540001:	// eeprom
			return (EEPROMRead() << 8);
	}

	return common_main_read_word(address);
}

static UINT8 gdfs_read_byte(UINT32 address)
{
	if ((address & 0xf00000) == 0x900000) {
		return st0020GfxramReadByte(address);
	}

	switch (address)
	{
		case 0x540000:
		case 0x540001:	// eeprom
			return (EEPROMRead() << 0);
	}

	return common_main_read_byte(address);
}

static void gdfs_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff00) == 0x8c0000) {
		st0020_blitram_write_word(address, data);
		return;
	}

	if ((address & 0xf00000) == 0x900000) {
		st0020GfxramWriteWord(address, data);
		return;
	}

	if ((address & 0xffffc0) == 0x440000) {
		*((UINT16*)(DrvTMAPScroll + (address & 0x3f))) = data;
		return;
	}

	switch (address)
	{
		case 0x500000:
		case 0x500001:
			gdfs_eeprom_write(data);
		return;
	}

	common_main_write_word(address,data);
}

static void gdfs_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffff00) == 0x8c0000) {
		st0020_blitram_write_byte(address, data);
		return;
	}

	if ((address & 0xf00000) == 0x900000) {
		st0020GfxramWriteByte(address, data);
		return;
	}

	if ((address & 0xffffc0) == 0x440000) {
		bprintf (0, _T("Scroll b: %5.5x, %2.2x\n"), address,data);
		DrvTMAPScroll[address & 0x3f] = data;
		return;
	}

	switch (address)
	{
		case 0x500000:
		case 0x500001:
			bprintf (0, _T("EEPROM write %x %x\n"),address,data);
		return;
	}

	common_main_write_word(address,data);
}

static UINT16 srmp4_inputs()
{
	for (INT32 i = 0; i < 5; i++) {
		if (input_select & (1 << i)) return DrvInputs[i+3];
	}

	return 0xffff;
}

static void janjan1_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x800000: // janjan1 / janjan2 / koikois2
		case 0x800001:
		case 0xc00006: // hypreact
		case 0xc00007:
		case 0xc0000e: // srmp4
		case 0xc0000f:
			input_select = data;
		return;
	}

	common_main_write_byte(address,data);
}

static void janjan1_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x800000: // janjan1 / janjan2 / koikois2
		case 0xc00006: // hypreact
		case 0xc0000e: // srmp4
			input_select = data & 0xff;
		return;
	}

	common_main_write_word(address,data);
}

static UINT8 janjan1_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x800002: // janjan1 / janjan2 / koikois2
		case 0x800003:
		case 0xc00000: // hypreact
		case 0xc00001:
		case 0xc0000a: // srmp4
		case 0xc0000b:
			return srmp4_inputs();
	}

	return common_main_read_byte(address);
}

static UINT16 janjan1_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x800002: // janjan1 / janjan2 / koikois2
		case 0xc00000: // hypreact
		case 0xc0000a: // srmp4
			return srmp4_inputs();
	}

	return common_main_read_word(address);
}


static void hypreac2_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x520000:
		case 0x520001:
			input_select = data;
		return;
	}

	common_main_write_word(address,data);
}

static void hypreac2_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x520000:
		case 0x520001:
			input_select = data;
		return;
	}

	common_main_write_byte(address,data);
}

static UINT16 hypreac2_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x500000:
		case 0x500001:
		case 0x500002:
		case 0x500003:
			return srmp4_inputs();
	}

	return common_main_read_word(address);
}

static UINT8 hypreac2_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x500000:
		case 0x500001:
		case 0x500002:
		case 0x500003:
			return srmp4_inputs();
	}

	return common_main_read_byte(address);
}

static void sound_bank(INT32 data)
{
	INT32 bank = (data & 1) * (0x400000 / 2);

	for (INT32 v = 0; v < 32; v++) {
		es5505_voice_bank_w(v, bank);
	}
}

static void srmp7_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x21000e:
		case 0x21000f:
			// lockout
		return;

		case 0x580000:
		case 0x580001:
			sound_bank(data);
		return;

		case 0x680000:
		case 0x680001:
			input_select = data;
		return;
	}

	common_main_write_word(address,data);
}

static void srmp7_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x21000e:
		case 0x21000f:
			// lockout
		return;

		case 0x580000:
		case 0x580001:
			sound_bank(data);
		return;

		case 0x680000:
		case 0x680001:
			input_select = data;
		return;
	}

	common_main_write_byte(address,data);
}

static UINT16 srmp7_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x300076:
		case 0x300077:
			return 0x0080;

		case 0x600000:
		case 0x600001:
			return srmp4_inputs();
	}

	return common_main_read_word(address);
}

static UINT8 srmp7_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x300076:
		case 0x300077:
			return 0x0080;

		case 0x600000:
		case 0x600001:
			return srmp4_inputs();
	}

	return common_main_read_byte(address);
}

static void sxyreact_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x21000e:
		case 0x21000f:
			// lockout
		return;

		case 0x500004:
		case 0x500005:
			// motor
		return;

		case 0x520000:
		case 0x520001:
		{
			if ((data & 0x20) == 0x20)
				sexyreact_serial_read = 0; // analog port for paddle

			if ((data & 0x40) == 0x00 && (sexyreact_previous_dial & 0x40) == 0x40)
				sexyreact_serial_read<<=1;

			sexyreact_previous_dial = data;
		}
		return;
	}

	common_main_write_word(address,data);
}

static void sxyreact_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x21000e:
		case 0x21000f:
			// lockout
		return;

		case 0x500004:
		case 0x500005:
			// motor
		return;

		case 0x520000:
		case 0x520001:
		{
			if ((data & 0x20) == 0x20)
				sexyreact_serial_read = 0; // analog port for paddle

			if ((data & 0x40) == 0x00 && (sexyreact_previous_dial & 0x40) == 0x40)
				sexyreact_serial_read<<=1;

			sexyreact_previous_dial = data;
		}
		return;
	}

	common_main_write_byte(address,data);
}

static UINT16 sxyreact_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x500002: // ballswitch
		case 0x500003:
			return DrvInputs[3];

		case 0x500004:
		case 0x500005:
			return ((sexyreact_serial_read >> 1) & 0x80);
	}

	return common_main_read_word(address);
}

static UINT8 sxyreact_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x500002: // ballswitch
		case 0x500003:
			return DrvInputs[3];

		case 0x500004:
		case 0x500005:
			return ((sexyreact_serial_read >> 1) & 0x80);
	}

	return common_main_read_byte(address);
}

static void eaglshot_gfxram_bank(INT32 data)
{
	INT32 bank = ((data & 0x0f) * 0x40000);

	eaglshot_bank[0] = data;

	v60MapMemory(DrvGfxROM + bank,	0x180000, 0x1bffff, MAP_RAM);
}

static void eaglshot_gfxrom_bank(INT32 data)
{
	INT32 bank = ((data < 6) ? data : 6) * 0x200000;

	eaglshot_bank[1] = data;

	v60MapMemory(DrvGfxROM2 + bank, 0xa00000, 0xbfffff, MAP_ROM);
}

static UINT16 eaglshot_trackball_read()
{
	switch (input_select)
	{
		case 0x60:  return (0/*m_io_trackx->read()*/ >> 8) & 0xff;
		case 0x40:  return (0/*m_io_trackx->read()*/ >> 0) & 0xff;

		case 0x70:  return (0/*m_io_tracky->read()*/ >> 8) & 0xff;
		case 0x50:  return (0/*m_io_tracky->read()*/ >> 0) & 0xff;
	}

	return 0;
}

static void eaglshot_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x1c0076:
		case 0x1c0077:
			eaglshot_gfxram_bank(data);
			*((UINT16*)(DrvScrollRAM + 0x76)) = data;
		return;

		case 0x21000e:
		case 0x21000f:
			// lockout
		return;

		case 0x800000:
		case 0x800001:
			eaglshot_gfxrom_bank(data);
		return;

		case 0x900000:
		case 0x900001:
			input_select = data;
		return;
	}

	common_main_write_word(address,data);
}

static void eaglshot_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x1c0076:
		case 0x1c0077:
			eaglshot_gfxram_bank(data);
			DrvScrollRAM[(address & 0x7f)] = data;
		return;

		case 0x21000e:
		case 0x21000f:
			// lockout
		return;

		case 0x800000:
		case 0x800001:
			eaglshot_gfxrom_bank(data);
		return;

		case 0x900000:
		case 0x900001:
			input_select = data;
		return;
	}

	common_main_write_byte(address,data);
}

static UINT16 eaglshot_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xd00000:
		case 0xd00001:
			return eaglshot_trackball_read();
	}

	return common_main_read_word(address);
}

static UINT8 eaglshot_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xd00000:
		case 0xd00001:
			return eaglshot_trackball_read();
	}

	return common_main_read_byte(address);
}


static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	v60Open(0);
	v60Reset();
	v60Close();

	ES5506Reset();

	if (is_gdfs) EEPROMReset();

	requested_int = 0;
	enable_video = 1;
	irq_enable = 0;
	input_select = 0;
	sexyreact_previous_dial = 0;
	sexyreact_serial_read = 0;

	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvV60ROM		= Next; Next += 0x0400000;
	DrvDSPROM		= Next; Next += 0x0011000;

	if (is_gdfs)
	{
		st0020GfxROM	= Next; Next += st0020GfxROMLen;
	}

	DrvGfxROM2		= Next; Next += nDrvGfxROM2Len; // gdfs / eaglshot
	DrvGfxROM		= Next; Next += nDrvGfxROMLen;

	DrvSndROM0		= Next; Next += nDrvSndROMLen[0];
	DrvSndROM1		= Next; Next += nDrvSndROMLen[1];
	DrvSndROM2		= Next; Next += nDrvSndROMLen[2];
	DrvSndROM3		= Next; Next += nDrvSndROMLen[3];

	DrvPalette		= (UINT32*)Next; Next += 0x8000 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x010000;

	AllRam			= Next;

	// gdfs
	if (is_gdfs)
	{
		st0020BlitRAM	= Next; Next += 0x000100;
		st0020SprRAM	= Next; Next += 0x080000;
		st0020GfxRAM	= Next; Next += 0x400000;
		DrvTMAPRAM	= Next; Next += 0x040000;
		DrvTMAPScroll	= Next; Next += 0x000040;
	}

	DrvV60RAM0		= Next; Next += 0x010000;
	DrvV60RAM1		= Next; Next += 0x020000;
	DrvV60RAM2		= Next; Next += 0x050000;
	DrvSprRAM		= Next; Next += 0x040000;
	DrvPalRAM		= Next; Next += 0x020000;
	DrvDspRAM		= Next; Next += 0x001000;

	DrvVectors		= Next; Next += 0x000080;
	DrvScrollRAM		= Next; Next += 0x000080;

	eaglshot_bank		= Next; Next += 0x000002;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void st010Expand(INT32 rom_offset)
{
	dsp_enable = 1;

	UINT8 *dspsrc = (UINT8*)BurnMalloc(0x11000);
	UINT32 *dspprg = (UINT32 *)DrvDSPROM;
	UINT16 *dspdata = (UINT16*)(DrvDSPROM + 0x10000);

	BurnLoadRom(dspsrc, rom_offset, 1);

	memset (DrvDSPROM, 0xff, 0x11000);

	// copy DSP program
	for (int i = 0; i < 0x10000; i+= 4)
	{
		*dspprg = dspsrc[0+i]<<24 | dspsrc[1+i]<<16 | dspsrc[2+i]<<8;
		dspprg++;
	}

	// copy DSP data
	for (int i = 0; i < 0x1000; i+= 2)
	{
		*dspdata++ = dspsrc[0x10000+i]<<8 | dspsrc[0x10001+i];
	}

	BurnFree(dspsrc);
}

static void DrvMemSwap(UINT8 *src0, UINT8 *src1, INT32 len)
{
	for (INT32 i = 0; i < len; i++ ) {
		INT32 t = src0[i];
		src0[i] = src1[i];
		src1[i] = t;
	}
}

static void DrvComputeTileCode(INT32 version)
{
	if (version)
	{
		for (INT32 i = 0; i < 16; i++) {
			tile_code[i] = (i << 16);
		}
	}
	else
	{
		for (INT32 i = 0; i < 16; i++) {
			tile_code[i] = ((i & 8) << 13) | ((i & 4) << 15) | ((i & 2) << 17) | ((i & 1) << 19); 
		}
	}
}

static void gfxdecode(UINT8 *src, UINT8 *dst, INT32 ofst, INT32 len)
{
	INT32 plane  = ofst / (nDrvGfxROMLen / 4);
	INT32 offset = ofst % (nDrvGfxROMLen / 4);

	for (INT32 i = offset * 8; i < (offset + len) * 8; i++)
	{
		INT32 d = (src[(i / 8) - offset] >> (i & 7)) & 1;

		dst[(7 - (i & 7)) | ((i & ~0xf) >> 1)] |= d << (((i & 8) >> 3) | (plane << 1));
	}
}

static INT32 DrvGetRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *V60Load = DrvV60ROM;
	UINT8 *GfxLoad = DrvGfxROM;
	UINT8 *GfxLoad2 = DrvGfxROM2;
	UINT8 *SNDLoad[4] = { DrvSndROM0, DrvSndROM1, DrvSndROM2, DrvSndROM3 };

	INT32 gfxrom_count = 0;
	INT32 prev_type = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 1) {
			if (bLoad) BurnLoadRom(V60Load, i, 1);
			V60Load += ri.nLen;
			prev_type = 1;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 2) {
			if (bLoad) {
				BurnLoadRom(V60Load + 0, i+0, 2);
				BurnLoadRom(V60Load + 1, i+1, 2);

				if (prev_type == 1 && ri.nLen == 0x80000) {
					memcpy (V60Load + 0x100000, V60Load, 0x100000);
				}
			}

			if (prev_type == 1 && ri.nLen == 0x80000) V60Load += 0x100000;
			V60Load += ri.nLen * 2;
			prev_type = 2;
			i++;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 3) {
			if (bLoad) {
				UINT8 *tmp = (UINT8*)BurnMalloc(ri.nLen);
				if (BurnLoadRom(tmp, i, 1)) return 1;
				gfxdecode(tmp, DrvGfxROM, GfxLoad - DrvGfxROM, ri.nLen);
				BurnFree(tmp);
			}

			GfxLoad += ri.nLen;
			gfxrom_count++;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 8) {
			if (bLoad) BurnLoadRom(GfxLoad2, i, 1);
			GfxLoad2 += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_SND) && (ri.nType & 0x1c) == 0) {
			if (bLoad) BurnLoadRom(SNDLoad[ri.nType & 3] + 1, i, 2);
			SNDLoad[ri.nType & 3] += ri.nLen * 2;
			continue;
		}

		if ((ri.nType & BRF_SND) && (ri.nType & 0x1c) == 4) {
			if (bLoad) BurnLoadRom(SNDLoad[ri.nType & 3], i, 1);
			SNDLoad[ri.nType & 3] += ri.nLen;
			continue;
		}
	}

	if (bLoad == false)
	{
		// get gfx rom length and then make sure it can be
		// decoded as 8 bpp.
		nDrvGfxROMLen = GfxLoad - DrvGfxROM;
		if (nDrvGfxROMLen == 0) nDrvGfxROMLen = 0x400000;

		INT32 div = (gfxrom_count & 3) ? 3 : 4;
		nDrvGfxROMLen = (nDrvGfxROMLen / div) * 4;

		// get gfx 2 rom length and make sure it can be
		// masked properly
		nDrvGfxROM2Len = GfxLoad2 - DrvGfxROM2;
		for (INT32 i = 1; i < 0x8000000; i<<=1) {
			if (nDrvGfxROM2Len <= (1 << i)) {
				nDrvGfxROM2Len = 1 << i;
				break;
			}
		}

		nDrvSndROMLen[0] = SNDLoad[0] - DrvSndROM0;
		nDrvSndROMLen[1] = SNDLoad[1] - DrvSndROM1;
		nDrvSndROMLen[2] = SNDLoad[2] - DrvSndROM2;
		nDrvSndROMLen[3] = SNDLoad[3] - DrvSndROM3;
		if (nDrvSndROMLen[0] && nDrvSndROMLen[0] < 0x400000) nDrvSndROMLen[0] = 0x400000;
		if (nDrvSndROMLen[1] && nDrvSndROMLen[1] < 0x400000) nDrvSndROMLen[1] = 0x400000;
		if (nDrvSndROMLen[2] && nDrvSndROMLen[2] < 0x400000) nDrvSndROMLen[2] = 0x400000;
		if (nDrvSndROMLen[3] && nDrvSndROMLen[3] < 0x400000) nDrvSndROMLen[3] = 0x400000;
	}

	return 0;
}

static INT32 DrvCommonInit(void (*pV60Callback)(), void (*pRomLoadCallback)(), INT32 compute, INT32 s0, INT32 s1, INT32 s2, INT32 s3, double volume, INT32 funky_vbl)
{
	DrvGetRoms(false);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	DrvGetRoms(true);

	if (pRomLoadCallback) {
		pRomLoadCallback();
	}

	v60Init();
	v60Open(0);
	pV60Callback();
	v60SetIRQCallback(ssv_irq_callback);
	v60Close();

	upd96050Init(96050, DrvDSPROM, DrvDSPROM + 0x10000, DrvDspRAM, NULL, NULL);

	UINT8 *snd[5] = { NULL, DrvSndROM0, DrvSndROM1, DrvSndROM2, DrvSndROM3 };

	ES5506Init(16000000, snd[s0+1], snd[s1+1], snd[s2+1], snd[s3+1], /*IRQCallback*/NULL);
	ES5506SetRoute(0, volume, BURN_SND_ES5506_ROUTE_BOTH);

	DrvComputeTileCode(compute);

	GenericTilesInit();

	vbl_kludge = funky_vbl;

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	v60Exit();

	ES5506Exit();

	BurnFree(AllMem);

	if (is_gdfs) EEPROMExit();

	interrupt_ultrax = 0;
	watchdog_disable = 0;
	is_gdfs = 0;
	dsp_enable = 0;
	vbl_kludge = 0;
	vbl_invert = 0;

	return 0;
}

static void DrvPaletteInit()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x20000/2; i+=2)
	{
		INT32 r = pal[i+1] & 0xff;
		INT32 g = pal[i+0] >> 8;
		INT32 b = pal[i+0] & 0xff;

		DrvPalette[i/2] = BurnHighCol(r,g,b,0);
	}
}

static void drawgfx(INT32 gfx, UINT32 code, UINT32 color, int flipx, int flipy, int x0, int y0, int shadow)
{
	const UINT8 *addr, *source;
	UINT8 pen;
	UINT16 *dest;
	int sx, x1, dx;
	int sy, y1, dy;
	int penmask = gfx-1;

	addr    =   DrvGfxROM + ((code * 16 * 8) % nDrvGfxROMLen);
	color   =   (color * 0x40) & 0x7fc0;

	if ( flipx )    {   x1 = x0-1;     x0 += 16-1; dx = -1; }
	else            {   x1 = x0 + 16;              dx =  1; }

	if ( flipy )    {   y1 = y0-1;     y0 += 8-1;  dy = -1; }
	else            {   y1 = y0 + 8;               dy =  1; }

#define SSV_DRAWGFX(SETPIXELCOLOR)                                              \
	for ( sy = y0; sy != y1; sy += dy )                                         \
	{                                                                           \
		if ( sy >= Gclip_min_y && sy <= Gclip_max_y )                 \
		{                                                                       \
			source  =   addr;                                                   \
			dest    =   pTransDraw + (sy * nScreenWidth);                          \
												\
			for ( sx = x0; sx != x1; sx += dx )                                 \
			{                                                                   \
				pen = (*source++) & penmask;                                     \
												\
				if ( pen && sx >= Gclip_min_y && sx <= Gclip_max_x )  \
					SETPIXELCOLOR                                            \
			}                                                                   \
		}                                                                       \
											\
		addr    +=  16;                                            \
	}

	if (shadow)
	{
		SSV_DRAWGFX( { dest[sx] = ((dest[sx] & shadow_pen_mask) | (pen << shadow_pen_shift)) & 0x7fff; } )
	}
	else
	{
		SSV_DRAWGFX( { dest[sx] = (color + pen) & 0x7fff; } )
	}
}

static void draw_row(int sx, int sy, int scroll)
{
	UINT16 *ssv_scroll = (UINT16*)DrvScrollRAM;
	UINT16 *spriteram16 = (UINT16*)DrvSprRAM;

	int attr, code, color, mode, size, page, shadow;
	int x, x1, sx1, flipx, xnum, xstart, xend, xinc;
	int y, y1, sy1, flipy, ynum, ystart, yend, yinc;
	UINT16 *s3;

	INT32 clip_min_x = Gclip_min_x;
	INT32 clip_max_x = Gclip_max_x;
	INT32 clip_min_y = Gclip_min_y;
	INT32 clip_max_y = Gclip_max_y;

	xnum = 0x20;        // width in tiles (screen-wide)
	ynum = 0x8;         // height in tiles (always 64 pixels?)

	scroll &= 0x7;      // scroll register index

	/* Sign extend the position */
	sx = 0;
	sy = (sy & 0x1ff) - (sy & 0x200);

	/* Set up a clipping region for the tilemap slice .. */
	int clip[4] = { Gclip_min_x, Gclip_max_x, Gclip_min_y, Gclip_max_y };
//	clip.set(sx, sx + xnum * 0x10 - 1, sy, sy + ynum * 0x8  - 1);

	clip_min_x = sx;
	clip_max_x = sx + xnum * 0x10 - 1;
	clip_min_y = sy;
	clip_max_y = sy + ynum * 0x8  - 1;

	/* .. and clip it against the visible screen */
	if (clip_min_x >= nScreenWidth) return;
	if (clip_min_y >= nScreenHeight) return;
	if (clip_max_x < 0) return;
	if (clip_max_y < 0) return;

	if (clip_min_x < 0)  clip_min_x = 0;
	if (clip_max_x >= nScreenWidth) clip_max_x = nScreenWidth - 1;
	if (clip_min_y < 0)  clip_min_y = 0;
	if (clip_max_y >= nScreenHeight) clip_max_y = nScreenHeight - 1;

	/* Get the scroll data */
	x    = ssv_scroll[ scroll * 4 + 0 ];    // x scroll
	y    = ssv_scroll[ scroll * 4 + 1 ];    // y scroll
	//     ssv_scroll[ scroll * 4 + 2 ];    // ???
	mode = ssv_scroll[ scroll * 4 + 3 ];    // layer disabled, shadow, depth etc.

	/* Background layer disabled */
	if ((mode & 0xe000) == 0)
		return;

#if 0 // iq_132
	Gclip_min_x = clip_min_x;
	Gclip_min_y = clip_min_y;
	Gclip_max_x = clip_max_x;
	Gclip_max_y = clip_max_y;
#endif

	shadow = (mode & 0x0800);

	/* Decide the actual size of the tilemap */
	size    =   1 << (8 + ((mode & 0xe000) >> 13));
	page    =   (x & 0x7fff) / size;

	/* Given a fixed scroll value, the portion of tilemap displayed changes with the sprite position */
	x += sx;
	y += sy;

	/* Tweak the scroll values */
	y += ((ssv_scroll[0x70/2] & 0x1ff) - (ssv_scroll[0x70/2] & 0x200) + ssv_scroll[0x6a/2] + 2);

	// Kludge for eaglshot
	if ((ssv_scroll[ scroll * 4 + 2 ] & 0x05ff) == 0x0440) x += -0x10;
	if ((ssv_scroll[ scroll * 4 + 2 ] & 0x05ff) == 0x0401) x += -0x20;

	/* Draw the rows */
	x1  = x;
	y1  = y;
	sx1 = sx - (x & 0xf);
	sy1 = sy - (y & 0xf);

	for (sx=sx1,x=x1; sx <= clip_max_x; sx+=0x10,x+=0x10)
	{
		for (sy=sy1,y=y1; sy <= clip_max_y; sy+=0x10,y+=0x10)
		{
			int tx, ty, gfx;

			s3  =   &spriteram16[page * (size * 4) + ((x & ((size -1) & ~0xf)) << 2) + ((y & (0x1ff & ~0xf)) >> 3)];

			code    =   s3[0];  // code high bits
			attr    =   s3[1];  // code low  bits + color

			/* Code's high bits are scrambled */
			code    +=  tile_code[(attr & 0x3c00)>>10];
			flipy   =   (attr & 0x4000);
			flipx   =   (attr & 0x8000);

			if ((ssv_scroll[0x74/2] & 0x1000) && ((ssv_scroll[0x74/2] & 0x2000) == 0))
			{
				if (flipx == 0) flipx = 1; else flipx = 0;
			}

			if ((ssv_scroll[0x74/2] & 0x4000) && ((ssv_scroll[0x74/2] & 0x2000) == 0))
			{
				if (flipy == 0) flipy = 1; else flipy = 0;
			}

			color   =   attr;

			/* Select 256 or 64 color tiles */
			gfx =   ((mode & 0x0100) ? 0x100 : 0x40);

			/* Force 16x16 tiles ? */
			if (flipx)  { xstart = 1-1;  xend = -1; xinc = -1; }
			else        { xstart = 0;    xend = 1;  xinc = +1; }

			if (flipy)  { ystart = 2-1;  yend = -1; yinc = -1; }
			else        { ystart = 0;    yend = 2;  yinc = +1; }

			/* Draw a tile (16x16) */
			for (tx = xstart; tx != xend; tx += xinc)
			{
				for (ty = ystart; ty != yend; ty += yinc)
				{
					drawgfx(gfx, code++, color, flipx, flipy, (sx + tx * 16), (sy + ty * 8), shadow);
				} /* ty */
			} /* tx */

		} /* sy */
	} /* sx */

	Gclip_min_x = clip[0];
	Gclip_min_y = clip[2];
	Gclip_max_x = clip[1];
	Gclip_max_y = clip[3];
}

static void draw_layer(int  nr)
{
	for (INT32 sy = 0; sy < nScreenHeight; sy += 0x40)
		if (nBurnLayer & 4) draw_row(0, sy, nr);
}

static void draw_sprites()
{
	/* Sprites list */
	UINT16 *ssv_scroll = (UINT16*)DrvScrollRAM;
	UINT16 *spriteram16 = (UINT16*)DrvSprRAM;

	UINT16 *s1  	=   spriteram16;
	UINT16 *end1    =   spriteram16 + 0x02000/2;
	UINT16 *end2    =   spriteram16 + 0x40000/2;
	UINT16 *s2;

	for ( ; s1 < end1; s1+=4 )
	{
		INT32 attr, code, color, num, sprite;
		INT32 sx, x, xoffs, flipx, xnum, xstart, xend, xinc, sprites_offsx;
		INT32 sy, y, yoffs, flipy, ynum, ystart, yend, yinc, sprites_offsy, tilemaps_offsy;
		INT32 mode,global_depth,global_xnum,global_ynum;

		mode   = s1[ 0 ];
		sprite = s1[ 1 ];
		xoffs  = s1[ 2 ];
		yoffs  = s1[ 3 ];

		/* Last sprite */
		if (sprite & 0x8000) break;

		/* Single-sprite address */
		s2 = &spriteram16[ (sprite & 0x7fff) * 4 ];
		tilemaps_offsy = ((s2[3] & 0x1ff) - (s2[3] & 0x200));

		/* Every single sprite is offset by x & yoffs, and additionally
		by one of the 8 x & y offsets in the 1c0040-1c005f area   */

		xoffs   +=      ssv_scroll[((mode & 0x00e0) >> 4) + 0x40/2];
		yoffs   +=      ssv_scroll[((mode & 0x00e0) >> 4) + 0x42/2];

		/* Number of single-sprites (1-32) */
		num             =   (mode & 0x001f) + 1;
		global_ynum     =   (mode & 0x0300) << 2;
		global_xnum     =   (mode & 0x0c00);
		global_depth    =   (mode & 0xf000);

		for( ; num > 0; num--,s2+=4 )
		{
			INT32 depth, local_depth, local_xnum, local_ynum;

			if (s2 >= end2) break;

			sx      =       s2[ 2 ];
			sy      =       s2[ 3 ];

			local_depth     =   sx & 0xf000;
			local_xnum      =   sx & 0x0c00;
			local_ynum      =   sy & 0x0c00;

			if (ssv_scroll[0x76/2] & 0x4000)
			{
				xnum    =   local_xnum;
				ynum    =   local_ynum;
				depth   =   local_depth;
			}
			else
			{
				xnum    =   global_xnum;
				ynum    =   global_ynum;
				depth   =   global_depth;
			}

			if ( s2[0] <= 7 && s2[1] == 0 && xnum == 0 && ynum == 0x0c00)
			{
				INT32 scroll  =   s2[ 0 ];    // scroll index

				if (ssv_scroll[0x76/2] & 0x1000)
					sy -= 0x20;                     // eaglshot
				else
				{
					if (ssv_scroll[0x7a/2] & 0x0800)
					{
						if (ssv_scroll[0x7a/2] & 0x1000)    // drifto94, dynagear, keithlcy, mslider, stmblade, gdfs, ultrax, twineag2
							sy -= tilemaps_offsy;
						else                        // srmp4
							sy += tilemaps_offsy;
					}
				}

				if ((mode & 0x001f) != 0)
					if (nBurnLayer & 2) draw_row(sx, sy, scroll);
			}
			else
			{
				INT32 shadow, gfx;
				if (s2 >= end2) break;

				code    =   s2[0];  // code high bits
				attr    =   s2[1];  // code low  bits + color

				/* Code's high bits are scrambled */
				code    +=  tile_code[(attr & 0x3c00)>>10];
				flipy   =   (attr & 0x4000);
				flipx   =   (attr & 0x8000);

				if ((ssv_scroll[0x74/2] & 0x1000) && ((ssv_scroll[0x74/2] & 0x2000) == 0))
				{
					if (flipx == 0) flipx = 1; else flipx = 0;
				}
				if ((ssv_scroll[0x74/2] & 0x4000) && ((ssv_scroll[0x74/2] & 0x2000) == 0))
				{
					if (flipy == 0) flipy = 1; else flipy = 0;
				}

				color   =   attr;

				/* Select 256 or 64 color tiles */
				gfx     =   ((depth & 0x1000) ? 0x100 : 0x40);
				shadow  =   (depth & 0x8000);

				/* Single-sprite tile size */
				xnum = 1 << (xnum >> 10);   // 1, 2, 4 or 8 tiles
				ynum = 1 << (ynum >> 10);   // 1, 2, 4 tiles (8 means tilemap sprite?)

				if (flipx)  { xstart = xnum-1;  xend = -1;    xinc = -1; }
				else        { xstart = 0;       xend = xnum;  xinc = +1; }

				if (flipy)  { ystart = ynum-1;  yend = -1;    yinc = -1; }
				else        { ystart = 0;       yend = ynum;  yinc = +1; }

				/* Apply global offsets */
				sx  +=  xoffs;
				sy  +=  yoffs;

				/* Sign extend the position */
				sx  =   (sx & 0x1ff) - (sx & 0x200);
				sy  =   (sy & 0x1ff) - (sy & 0x200);

				sprites_offsx =  ((ssv_scroll[0x74/2] & 0x7f) - (ssv_scroll[0x74/2] & 0x80));

				sprites_offsy = -((ssv_scroll[0x70/2] & 0x1ff) - (ssv_scroll[0x70/2] & 0x200) + ssv_scroll[0x6a/2] + 1);

				if (ssv_scroll[0x74/2] & 0x4000) // flipscreen y
				{
					sy = -sy;
					if (ssv_scroll[0x74/2] & 0x8000)
						sy += 0x00;         //
					else
						sy -= 0x10;         // vasara (hack)
				}

				if (ssv_scroll[0x74/2] & 0x1000) // flipscreen x
				{
					sx = -sx + 0x100;
				}

				if (ssv_scroll[0x7a/2] == 0x7140)
				{
					// srmp7
					sx  =   sprites_offsx + sx;
					sy  =   sprites_offsy - sy;
				}
				else if (ssv_scroll[0x7a/2] & 0x0800)
				{
					// dynagear, drifto94, eaglshot, keithlcy, mslider, srmp4, stmblade, twineag2, ultrax
					sx  =   sprites_offsx + sx - (xnum * 8)    ;
					sy  =   sprites_offsy - sy - (ynum * 8) / 2;
				}
				else
				{
					// hypreact, hypreac2, janjans1, meosism, ryorioh, survarts, sxyreact, sxyreac2, vasara, vasara2
					sx  =   sprites_offsx + sx;
					sy  =   sprites_offsy - sy - (ynum * 8);
				}

				if (xnum == 2 && ynum == 4) // needed by hypreact
				{
					code &= ~7;
				}

				for (x = xstart; x != xend; x += xinc)
				{
					for (y = ystart; y != yend; y += yinc)
					{
						if (nBurnLayer & 1) drawgfx(gfx, code++, color, flipx, flipy, sx + x * 16, sy + y * 8, shadow );
					}
				}
			}
		}
	}
}

static void gdfs_draw_layer()
{
	UINT16 *ram = (UINT16*)DrvTMAPRAM;
	UINT16 *sram = (UINT16*)DrvTMAPScroll;

	INT32 scrollx = sram[0x0c/2] & 0xfff;
	INT32 scrolly = sram[0x10/2] & 0xfff;

	INT32 yy = scrolly & 0xf;
	INT32 xx = scrollx & 0xf;

	for (INT32 y = 0; y < (240 + 16); y+= 16)
	{
		INT32 sy = ((scrolly + y) & 0xff0) * 0x10;

		for (INT32 x = 0; x < (336 + 16); x+=16)
		{
			INT32 offs = (((scrollx + x) & 0xff0) / 0x10) + sy;

			INT32 attr  = ram[offs];
			INT32 code  = attr & 0x3fff;
			INT32 color = 0;
			INT32 flipx = attr & 0x8000;
			INT32 flipy = attr & 0x4000;

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, x - xx, y - yy, color, 8, 0, 0, DrvGfxROM2);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, x - xx, y - yy, color, 8, 0, 0, DrvGfxROM2);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, x - xx, y - yy, color, 8, 0, 0, DrvGfxROM2);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, x - xx, y - yy, color, 8, 0, 0, DrvGfxROM2);
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (enable_video)
	{
		UINT16 *scroll = (UINT16*)DrvScrollRAM;

		// Shadow
		if (scroll[0x76/2] & 0x0080)	// 4 bit shadows (mslider, stmblade)
		{
			shadow_pen_shift = 11;
		}
		else				// 2 bit shadows
		{
			shadow_pen_shift = 13;
		}

		shadow_pen_mask = (1 << shadow_pen_shift) - 1;

		// used by twineag2 and ultrax
		Gclip_min_x = ((nScreenWidth / 2) + scroll[0x62/2]) * 2 - scroll[0x64/2] * 2 + 2;
		Gclip_max_x = ((nScreenWidth / 2) + scroll[0x62/2]) * 2 - scroll[0x62/2] * 2 + 1;
		Gclip_min_y = (nScreenHeight + scroll[0x6a/2]) - scroll[0x6c/2] + 1;
		Gclip_max_y = (nScreenHeight + scroll[0x6a/2]) - scroll[0x6a/2]        ;

		if (Gclip_min_x < 0) Gclip_min_x = 0;
		if (Gclip_min_y < 0) Gclip_min_y = 0;
		if (Gclip_max_x >= nScreenWidth)  Gclip_max_x = nScreenWidth - 1;
		if (Gclip_max_y >= nScreenHeight) Gclip_max_y = nScreenHeight - 1;

		if (Gclip_min_x > Gclip_max_x) Gclip_min_x = Gclip_max_x;
		if (Gclip_min_y > Gclip_max_y) Gclip_min_y = Gclip_max_y;

#if 1
		// iq_132
		Gclip_min_x = 0;
		Gclip_max_x = nScreenWidth - 1;
		Gclip_min_y = 0;
		Gclip_max_y = nScreenHeight - 1;
#endif
		draw_layer(0);
		draw_sprites();
	}

	if (is_gdfs)
	{
		if (nSpriteEnable & 1) st0020Draw();
		if (nSpriteEnable & 2) gdfs_draw_layer();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180 && watchdog_disable == 0) {
		bprintf(0, _T("Watchdog tripped.\n"));
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, 8);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
			DrvInputs[6] ^= (DrvJoy7[i] & 1) << i;
			DrvInputs[7] ^= (DrvJoy8[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { (16000000 * 100) / 6018, (10000000 * 100) / 6018 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSegment = 0;

	v60Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = (nCyclesTotal[0] - nCyclesDone[0]) / (nInterleave - i);
		nCyclesDone[0] += v60Run(nSegment);

		if (dsp_enable)
			nCyclesDone[1] += upd96050Run(nCyclesTotal[1] / nInterleave);

		if (i == 0 && interrupt_ultrax) {
			requested_int |= 1 << 1;
			update_irq_state();
		}

		if ((i & 0x3f) == 0 && is_gdfs) {
			requested_int |= 1 << 6;
			update_irq_state();
		}

		if (i == ((vbl_kludge) ? (nInterleave-1) : 240)) {
			vblank = 1;
			requested_int |= 1 << 3;
			update_irq_state();
		}
	}

	v60Close();

	if (pBurnSoundOut) {
		ES5506Update(pBurnSoundOut, nBurnSoundLen);
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

	if (nAction & ACB_DRIVER_DATA) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		v60Scan(nAction);

		ES5506Scan(nAction,pnMin);
		if (dsp_enable) upd96050Scan(nAction);

		SCAN_VAR(requested_int);
		SCAN_VAR(enable_video);
		SCAN_VAR(irq_enable);
		SCAN_VAR(input_select);
		SCAN_VAR(sexyreact_previous_dial);
		SCAN_VAR(sexyreact_serial_read);

		if (is_gdfs) EEPROMScan(nAction, pnMin);
	}

	return 0;
}

static INT32 eaglshtScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (nAction & ACB_DRIVER_DATA) {
		ba.Data	  = DrvGfxROM;
		ba.nLen	  = 0x400000;
		ba.szName = "Gfx Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE)
	{
		v60Open(0);
		eaglshot_gfxram_bank(eaglshot_bank[0]);
		eaglshot_gfxram_bank(eaglshot_bank[1]);
		v60Close();
	}

	return DrvScan(nAction,pnMin);
}


// Vasara

static struct BurnRomInfo vasaraRomDesc[] = {
	{ "data.u34",		0x200000, 0x7704cc7e, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "prg-l.u30",		0x080000, 0xf0547886, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "prg-h.u31",		0x080000, 0x6a39bba9, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "a0.u1",		0x800000, 0x673230a6, 3 | BRF_GRA },           //  3 Graphics
	{ "b0.u2",		0x800000, 0x31a2da7f, 3 | BRF_GRA },           //  4
	{ "c0.u3",		0x800000, 0xd110dacf, 3 | BRF_GRA },           //  5
	{ "d0.u4",		0x800000, 0x82d0ca55, 3 | BRF_GRA },           //  6

	{ "s0.u36",		0x200000, 0x754fca02, 0 | BRF_SND },           //  7 Ensoniq samples 0

	{ "s1.u37",		0x200000, 0x5f303698, 1 | BRF_SND },           //  8 Ensoniq samples 1
};

STD_ROM_PICK(vasara)
STD_ROM_FN(vasara)

static void VasaraV60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xc00000, 0xffffff, MAP_ROM);	
	v60SetWriteWordHandler(common_main_write_word);
	v60SetWriteByteHandler(common_main_write_byte);
	v60SetReadWordHandler(common_main_read_word);
	v60SetReadByteHandler(common_main_read_byte);
}

static INT32 VasaraInit()
{
	return DrvCommonInit(VasaraV60Map, NULL, 0, 0, 1, -1, -1, 0.80, 1);
}

struct BurnDriver BurnDrvVasara = {
	"vasara", NULL, NULL, NULL, "2000",
	"Vasara\0", NULL, "Visco", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA_SSV, GBF_VERSHOOT, 0,
	NULL, vasaraRomInfo, vasaraRomName, NULL, NULL, VasaraInputInfo, VasaraDIPInfo,
	VasaraInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	240, 336, 3, 4
};


// Vasara 2 (set 1)

static struct BurnRomInfo vasara2RomDesc[] = {
	{ "data.u34",		0x200000, 0x493d0103, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "prg-l.u30",		0x080000, 0x40e6f5f6, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "prg-h.u31",		0x080000, 0xc958e146, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "a0.u1",		0x800000, 0xa6306c75, 3 | BRF_GRA },           //  3 Graphics
	{ "b0.u2",		0x800000, 0x227cbd9f, 3 | BRF_GRA },           //  4
	{ "c0.u3",		0x800000, 0x54ede017, 3 | BRF_GRA },           //  5
	{ "d0.u4",		0x800000, 0x4be8479d, 3 | BRF_GRA },           //  6

	{ "s0.u36",		0x200000, 0x2b381b33, 0 | BRF_SND },           //  7 Ensoniq samples 0

	{ "s1.u37",		0x200000, 0x11cd7098, 1 | BRF_SND },           //  8 Ensoniq samples 1
};

STD_ROM_PICK(vasara2)
STD_ROM_FN(vasara2)

struct BurnDriver BurnDrvVasara2 = {
	"vasara2", NULL, NULL, NULL, "2001",
	"Vasara 2 (set 1)\0", NULL, "Visco", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA_SSV, GBF_VERSHOOT, 0,
	NULL, vasara2RomInfo, vasara2RomName, NULL, NULL, VasaraInputInfo, Vasara2DIPInfo,
	VasaraInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	240, 336, 3, 4
};


// Vasara 2 (set 2)

static struct BurnRomInfo vasara2aRomDesc[] = {
	{ "data.u34",		0x200000, 0x493d0103, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "basara-l.u30",	0x080000, 0xfd88b068, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "basara-h.u31",	0x080000, 0x91d641e6, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "a0.u1",		0x800000, 0xa6306c75, 3 | BRF_GRA },           //  3 Graphics
	{ "b0.u2",		0x800000, 0x227cbd9f, 3 | BRF_GRA },           //  4
	{ "c0.u3",		0x800000, 0x54ede017, 3 | BRF_GRA },           //  5
	{ "d0.u4",		0x800000, 0x4be8479d, 3 | BRF_GRA },           //  6

	{ "s0.u36",		0x200000, 0x2b381b33, 0 | BRF_SND },           //  7 Ensoniq samples 0

	{ "s1.u37",		0x200000, 0x11cd7098, 1 | BRF_SND },           //  8 Ensoniq samples 1
};

STD_ROM_PICK(vasara2a)
STD_ROM_FN(vasara2a)

struct BurnDriver BurnDrvVasara2a = {
	"vasara2a", "vasara2", NULL, NULL, "2001",
	"Vasara 2 (set 2)\0", NULL, "Visco", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA_SSV, GBF_VERSHOOT, 0,
	NULL, vasara2aRomInfo, vasara2aRomName, NULL, NULL, VasaraInputInfo, Vasara2DIPInfo,
	VasaraInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	240, 336, 3, 4
};


// Survival Arts (World)

static struct BurnRomInfo survartsRomDesc[] = {
	{ "prl-r6.u4",		0x080000, 0xef5f6e17, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "prh-r5.u3",		0x080000, 0xd446f010, 2 | BRF_PRG | BRF_ESS }, //  1

	{ "si001-01.u27",	0x200000, 0x8b38fbab, 3 | BRF_GRA },           //  2 Graphics
	{ "si001-04.u26",	0x200000, 0x34248b54, 3 | BRF_GRA },           //  3
	{ "si001-07.u25",	0x200000, 0x497d6151, 3 | BRF_GRA },           //  4
	{ "si001-02.u23",	0x200000, 0xcb4a2dbd, 3 | BRF_GRA },           //  5
	{ "si001-05.u22",	0x200000, 0x8f092381, 3 | BRF_GRA },           //  6
	{ "si001-08.u21",	0x200000, 0x182b88c4, 3 | BRF_GRA },           //  7
	{ "si001-03.u17",	0x200000, 0x92fdf652, 3 | BRF_GRA },           //  8
	{ "si001-06.u16",	0x200000, 0x9a62f532, 3 | BRF_GRA },           //  9
	{ "si001-09.u15",	0x200000, 0x0955e393, 3 | BRF_GRA },           // 10

	{ "si001-10.u9",	0x100000, 0x5642b333, 6 | BRF_SND },           // 11 Ensoniq samples 2
	{ "si001-11.u8",	0x100000, 0xa81e6ea6, 6 | BRF_SND },           // 12
	{ "si001-12.u7",	0x100000, 0xe9b2b45b, 6 | BRF_SND },           // 13
	{ "si001-13.u6",	0x100000, 0xd66a7e26, 6 | BRF_SND },           // 14

	{ "gal16v8b.u5",	0x000117, 0x378ce368, 0 | BRF_OPT },           // 15 PLDs
};

STD_ROM_PICK(survarts)
STD_ROM_FN(survarts)

static void SurvartsV60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvV60RAM2,		0x400000, 0x43ffff, MAP_RAM); // more ram
	v60MapMemory(DrvV60ROM,			0xf00000, 0xffffff, MAP_ROM);	
	v60SetWriteWordHandler(common_main_write_word);
	v60SetWriteByteHandler(common_main_write_byte);
	v60SetReadWordHandler(common_main_read_word);
	v60SetReadByteHandler(common_main_read_byte);
}

static INT32 SurvartsInit()
{
	return DrvCommonInit(SurvartsV60Map, NULL, 0, -1, -1, 2, -1, 0.30, 0);
}

static INT32 DynagearInit()
{
	return DrvCommonInit(SurvartsV60Map, NULL, 0, -1, -1, 2, -1, 0.20, 1);
}

struct BurnDriver BurnDrvSurvarts = {
	"survarts", NULL, NULL, NULL, "1993",
	"Survival Arts (World)\0", NULL, "Sammy", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_VSFIGHT, 0,
	NULL, survartsRomInfo, survartsRomName, NULL, NULL, SurvartsInputInfo, SurvartsDIPInfo,
	SurvartsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 240, 4, 3
};


// Survival Arts (USA)

static struct BurnRomInfo survartsuRomDesc[] = {
	{ "usa-pr-l.u4",	0x080000, 0xfa328673, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "usa-pr-h.u3",	0x080000, 0x6bee2635, 2 | BRF_PRG | BRF_ESS }, //  1

	{ "si001-01.u27",	0x200000, 0x8b38fbab, 3 | BRF_GRA },           //  2 Graphics
	{ "si001-04.u26",	0x200000, 0x34248b54, 3 | BRF_GRA },           //  3
	{ "si001-07.u25",	0x200000, 0x497d6151, 3 | BRF_GRA },           //  4
	{ "si001-02.u23",	0x200000, 0xcb4a2dbd, 3 | BRF_GRA },           //  5
	{ "si001-05.u22",	0x200000, 0x8f092381, 3 | BRF_GRA },           //  6
	{ "si001-08.u21",	0x200000, 0x182b88c4, 3 | BRF_GRA },           //  7
	{ "si001-03.u17",	0x200000, 0x92fdf652, 3 | BRF_GRA },           //  8
	{ "si001-06.u16",	0x200000, 0x9a62f532, 3 | BRF_GRA },           //  9
	{ "si001-09.u15",	0x200000, 0x0955e393, 3 | BRF_GRA },           // 10

	{ "si001-10.u9",	0x100000, 0x5642b333, 6 | BRF_SND },           // 11 Ensoniq samples 2
	{ "si001-11.u8",	0x100000, 0xa81e6ea6, 6 | BRF_SND },           // 12
	{ "si001-12.u7",	0x100000, 0xe9b2b45b, 6 | BRF_SND },           // 13
	{ "si001-13.u6",	0x100000, 0xd66a7e26, 6 | BRF_SND },           // 14
};

STD_ROM_PICK(survartsu)
STD_ROM_FN(survartsu)

struct BurnDriver BurnDrvSurvartsu = {
	"survartsu", "survarts", NULL, NULL, "1993",
	"Survival Arts (USA)\0", NULL, "American Sammy", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SETA_SSV, GBF_VSFIGHT, 0,
	NULL, survartsuRomInfo, survartsuRomName, NULL, NULL, SurvartsInputInfo, SurvartsDIPInfo,
	SurvartsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 240, 4, 3
};


// Survival Arts (Japan)

static struct BurnRomInfo survartsjRomDesc[] = {
	{ "jpn-pr-l.u4",	0x080000, 0xe5a52e8c, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "jan-pr-h.u3",	0x080000, 0x051c9bca, 2 | BRF_PRG | BRF_ESS }, //  1

	{ "si001-01.u27",	0x200000, 0x8b38fbab, 3 | BRF_GRA },           //  2 Graphics
	{ "si001-04.u26",	0x200000, 0x34248b54, 3 | BRF_GRA },           //  3
	{ "si001-07.u25",	0x200000, 0x497d6151, 3 | BRF_GRA },           //  4
	{ "si001-02.u23",	0x200000, 0xcb4a2dbd, 3 | BRF_GRA },           //  5
	{ "si001-05.u22",	0x200000, 0x8f092381, 3 | BRF_GRA },           //  6
	{ "si001-08.u21",	0x200000, 0x182b88c4, 3 | BRF_GRA },           //  7
	{ "si001-03.u17",	0x200000, 0x92fdf652, 3 | BRF_GRA },           //  8
	{ "si001-06.u16",	0x200000, 0x9a62f532, 3 | BRF_GRA },           //  9
	{ "si001-09.u15",	0x200000, 0x0955e393, 3 | BRF_GRA },           // 10

	{ "si001-10.u9",	0x100000, 0x5642b333, 6 | BRF_SND },           // 11 Ensoniq samples 2
	{ "si001-11.u8",	0x100000, 0xa81e6ea6, 6 | BRF_SND },           // 12
	{ "si001-12.u7",	0x100000, 0xe9b2b45b, 6 | BRF_SND },           // 13
	{ "si001-13.u6",	0x100000, 0xd66a7e26, 6 | BRF_SND },           // 14
};

STD_ROM_PICK(survartsj)
STD_ROM_FN(survartsj)

struct BurnDriver BurnDrvSurvartsj = {
	"survartsj", "survarts", NULL, NULL, "1993",
	"Survival Arts (Japan)\0", NULL, "Sammy", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SETA_SSV, GBF_VSFIGHT, 0,
	NULL, survartsjRomInfo, survartsjRomName, NULL, NULL, SurvartsInputInfo, SurvartsDIPInfo,
	SurvartsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 240, 4, 3
};


// Dyna Gear

static struct BurnRomInfo dynagearRomDesc[] = {
	{ "si002-prl.u4",	0x080000, 0x71ba29c6, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "si002-prh.u3",	0x080000, 0xd0947a12, 2 | BRF_PRG | BRF_ESS }, //  1

	{ "si002-01.u27",	0x200000, 0x0060a521, 3 | BRF_GRA },           //  2 Graphics
	{ "si002-04.u26",	0x200000, 0x6140f47d, 3 | BRF_GRA },           //  3
	{ "si002-02.u23",	0x200000, 0xc22f2a41, 3 | BRF_GRA },           //  4
	{ "si002-05.u22",	0x200000, 0x482412fd, 3 | BRF_GRA },           //  5
	{ "si002-03.u17",	0x200000, 0x4261a6b8, 3 | BRF_GRA },           //  6
	{ "si002-06.u16",	0x200000, 0x0e1f23f6, 3 | BRF_GRA },           //  7

	{ "si002-07.u9",	0x100000, 0x30d2bf11, 6 | BRF_SND },           //  8 Ensoniq samples 2
	{ "si002-08.u8",	0x100000, 0x253704ee, 6 | BRF_SND },           //  9
	{ "si002-09.u7",	0x100000, 0x1ea86db7, 6 | BRF_SND },           // 10
	{ "si002-10.u6",	0x100000, 0xe369c177, 6 | BRF_SND },           // 11
};

STD_ROM_PICK(dynagear)
STD_ROM_FN(dynagear)

struct BurnDriver BurnDrvDynagear = {
	"dynagear", NULL, NULL, NULL, "1993",
	"Dyna Gear\0", NULL, "Sammy", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_PLATFORM, 0,
	NULL, dynagearRomInfo, dynagearRomName, NULL, NULL, DrvInputInfo, DynagearDIPInfo,
	DynagearInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 240, 4, 3
};


// Dramatic Adventure Quiz Keith & Lucy (Japan)

static struct BurnRomInfo keithlcyRomDesc[] = {
	{ "vg002-07.u28",	0x100000, 0x57f80ff5, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "kl-p0l.u26",		0x080000, 0xd7b177fb, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "kl-p0h.u27",		0x080000, 0x9de7add4, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "vg002-01.u13",	0x200000, 0xb44d85b2, 3 | BRF_GRA },           //  3 Graphics
	{ "vg002-02.u16",	0x200000, 0xaa05fd14, 3 | BRF_GRA },           //  4
	{ "vg002-03.u21",	0x200000, 0x299a8a7d, 3 | BRF_GRA },           //  5
	{ "vg002-04.u34",	0x200000, 0xd3633f9b, 3 | BRF_GRA },           //  6

	{ "vg002-05.u29",	0x200000, 0x66aecd79, 4 | BRF_SND },           //  7 Ensoniq samples 0
	{ "vg002-06.u33",	0x200000, 0x75d8c8ea, 4 | BRF_SND },           //  8
};

STD_ROM_PICK(keithlcy)
STD_ROM_FN(keithlcy)

static void KeithlcyV60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xe00000, 0xffffff, MAP_ROM);	
	v60SetWriteWordHandler(common_main_write_word);
	v60SetWriteByteHandler(common_main_write_byte);
	v60SetReadWordHandler(common_main_read_word);
	v60SetReadByteHandler(common_main_read_byte);
}

static INT32 KeithlcyInit()
{
	watchdog_disable = 1;

	return DrvCommonInit(KeithlcyV60Map, NULL, 0, 0, -1, -1, -1, 0.80, 0);
}

struct BurnDriver BurnDrvKeithlcy = {
	"keithlcy", NULL, NULL, NULL, "1993",
	"Dramatic Adventure Quiz Keith & Lucy (Japan)\0", NULL, "Visco", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_MISC, 0,
	NULL, keithlcyRomInfo, keithlcyRomName, NULL, NULL, KeithlcyInputInfo, KeithlcyDIPInfo,
	KeithlcyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 238, 4, 3
};


// Twin Eagle II - The Rescue Mission

static struct BurnRomInfo twineag2RomDesc[] = {
	{ "sx002-12.u22",	0x200000, 0x846044dc, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code

	{ "sx002-01.u32",	0x200000, 0x6d6896b5, 3 | BRF_GRA },           //  1 Graphics
	{ "sx002-02.u28",	0x200000, 0x3f47e97a, 3 | BRF_GRA },           //  2
	{ "sx002-03.u25",	0x200000, 0x544f18bf, 3 | BRF_GRA },           //  3
	{ "sx002-04.u19",	0x200000, 0x58c270e2, 3 | BRF_GRA },           //  4
	{ "sx002-05.u16",	0x200000, 0x3c310229, 3 | BRF_GRA },           //  5
	{ "sx002-06.u13",	0x200000, 0x46d5b1f3, 3 | BRF_GRA },           //  6
	{ "sx002-07.u6",	0x200000, 0xc30fa397, 3 | BRF_GRA },           //  7
	{ "sx002-08.u3",	0x200000, 0x64edcefa, 3 | BRF_GRA },           //  8
	{ "sx002-09.u2",	0x200000, 0x51527c56, 3 | BRF_GRA },           //  9

	{ "sx002-10.u14",	0x200000, 0xb0669dfa, 0 | BRF_SND },           // 10 Ensoniq samples 0

	{ "sx002-11.u7",	0x200000, 0xb8dd621a, 1 | BRF_SND },           // 11 Ensoniq samples 1

	{ "st010.bin",		0x011000, 0xaa11ee2d, 0 | BRF_PRG | BRF_ESS }, // 12 ST010 Code
};

STD_ROM_PICK(twineag2)
STD_ROM_FN(twineag2)

static void Twineag2V60Map()
{
	v60MapMemory(DrvV60RAM2,		0x000000, 0x03ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xe00000, 0xffffff, MAP_ROM);	
	v60SetWriteWordHandler(common_main_write_word);
	v60SetWriteByteHandler(common_main_write_byte);
	v60SetReadWordHandler(common_main_read_word);
	v60SetReadByteHandler(common_main_read_byte);

	st010Expand(12);
}

static INT32 Twineag2Init()
{
	interrupt_ultrax = 1;
	watchdog_disable = 1;

	return DrvCommonInit(Twineag2V60Map, NULL, 0, 0, 1, 0, 1, 0.80, 1);
}

struct BurnDriver BurnDrvTwineag2 = {
	"twineag2", NULL, NULL, NULL, "1994",
	"Twin Eagle II - The Rescue Mission\0", NULL, "Seta", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA_SSV, GBF_VERSHOOT, 0,
	NULL, twineag2RomInfo, twineag2RomName, NULL, NULL, Twineag2InputInfo, Twineag2DIPInfo,
	Twineag2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	240, 336, 3, 4
};


// Drift Out '94 - The Hard Order (Japan)

static struct BurnRomInfo drifto94RomDesc[] = {
	{ "vg003-19.u26",	0x200000, 0x238e5e2b, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "visco-37.u37",	0x080000, 0x78fa3ccb, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "visco-33.u33",	0x080000, 0x88351146, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "vg003-01.a0",	0x200000, 0x2812aa1a, 3 | BRF_GRA },           //  3 Graphics
	{ "vg003-05.a1",	0x200000, 0x1a1dd910, 3 | BRF_GRA },           //  4
	{ "vg003-09.a2",	0x200000, 0x198f1c06, 3 | BRF_GRA },           //  5
	{ "vg003-13.a3",	0x200000, 0xb45b2267, 3 | BRF_GRA },           //  6
	{ "vg003-02.b0",	0x200000, 0xd7402027, 3 | BRF_GRA },           //  7
	{ "vg003-06.b1",	0x200000, 0x518c509f, 3 | BRF_GRA },           //  8
	{ "vg003-10.b2",	0x200000, 0xc1ee9d8b, 3 | BRF_GRA },           //  9
	{ "vg003-14.b3",	0x200000, 0x645b672b, 3 | BRF_GRA },           // 10
	{ "vg003-03.c0",	0x200000, 0x1ca7163d, 3 | BRF_GRA },           // 11
	{ "vg003-07.c1",	0x200000, 0x2ff113bb, 3 | BRF_GRA },           // 12
	{ "vg003-11.c2",	0x200000, 0xf924b105, 3 | BRF_GRA },           // 13
	{ "vg003-15.c3",	0x200000, 0x83623b01, 3 | BRF_GRA },           // 14
	{ "vg003-04.d0",	0x200000, 0x6be9bc62, 3 | BRF_GRA },           // 15
	{ "vg003-08.d1",	0x200000, 0xa7113cdb, 3 | BRF_GRA },           // 16
	{ "vg003-12.d2",	0x200000, 0xac0fd855, 3 | BRF_GRA },           // 17
	{ "vg003-16.d3",	0x200000, 0x1a5fd312, 3 | BRF_GRA },           // 18

	{ "vg003-17.u22",	0x200000, 0x6f9294ce, 0 | BRF_SND },           // 19 Ensoniq samples 0

	{ "vg003-18.u15",	0x200000, 0x511b3e93, 1 | BRF_SND },           // 20 Ensoniq samples 1

	{ "st010.bin",		0x011000, 0xaa11ee2d, 0 | BRF_PRG | BRF_ESS }, // 21 ST010 Code
};

STD_ROM_PICK(drifto94)
STD_ROM_FN(drifto94)

static void Drifto94V60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvNVRAM,			0x580000, 0x5807ff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xc00000, 0xffffff, MAP_ROM);
	v60SetWriteWordHandler(common_main_write_word);
	v60SetWriteByteHandler(common_main_write_byte);
	v60SetReadWordHandler(common_main_read_word);
	v60SetReadByteHandler(common_main_read_byte);

	st010Expand(21);
}

static INT32 Drifto94Init()
{
	watchdog_disable = 1;
	vbl_invert = 1;

	return DrvCommonInit(Drifto94V60Map, NULL, 0, 0, 1, -1, -1, 0.80, 0);
}

struct BurnDriver BurnDrvDrifto94 = {
	"drifto94", NULL, NULL, NULL, "1994",
	"Drift Out '94 - The Hard Order (Japan)\0", NULL, "Visco", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_RACING, 0,
	NULL, drifto94RomInfo, drifto94RomName, NULL, NULL, DrvInputInfo, Drifto94DIPInfo,
	Drifto94Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 238, 4, 3
};


// Meosis Magic (Japan)

static struct BurnRomInfo meosismRomDesc[] = {
	{ "s15-2-2.u47",	0x080000, 0x2ab0373f, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "s15-2-1.u46",	0x080000, 0xa4bce148, 2 | BRF_PRG | BRF_ESS }, //  1

	{ "s15-1-7.u7",		0x200000, 0xec5023cb, 3 | BRF_GRA },           //  2 Graphics
	{ "s15-1-8.u6",		0x200000, 0xf04b0836, 3 | BRF_GRA },           //  3
	{ "s15-1-5.u9",		0x200000, 0xc0414b97, 3 | BRF_GRA },           //  4
	{ "s15-1-6.u8",		0x200000, 0xd721aeb6, 3 | BRF_GRA },           //  5

	{ "s15-1-4.u45",	0x200000, 0x0c6738a7, 6 | BRF_SND },           //  6 Ensoniq samples 2
	{ "s15-1-3.u43",	0x200000, 0xd7e83178, 6 | BRF_SND },           //  7
};

STD_ROM_PICK(meosism)
STD_ROM_FN(meosism)

static void MeosismV60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvNVRAM,			0x580000, 0x58ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xf00000, 0xffffff, MAP_ROM);
	v60SetWriteWordHandler(common_main_write_word);
	v60SetWriteByteHandler(common_main_write_byte);
	v60SetReadWordHandler(common_main_read_word);
	v60SetReadByteHandler(common_main_read_byte);
}

static INT32 MeosismInit()
{
	return DrvCommonInit(MeosismV60Map, NULL, 0, -1, -1, 2, -1, 0.80, 0);
}

struct BurnDriver BurnDrvMeosism = {
	"meosism", NULL, NULL, NULL, "1996?",
	"Meosis Magic (Japan)\0", NULL, "Sammy", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_CASINO, 0,
	NULL, meosismRomInfo, meosismRomName, NULL, NULL, MeosismInputInfo, MeosismDIPInfo,
	MeosismInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	338, 236, 4, 3
};


// Change Air Blade (Japan)

static struct BurnRomInfo cairbladRomDesc[] = {
	{ "ac1810e0.u32",	0x200000, 0x13a0b4c2, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code

	{ "ac1801m0.u6",	0x400000, 0x1b2b6943, 3 | BRF_GRA },           //  1 Graphics
	{ "ac1802m0.u9",	0x400000, 0xe053b087, 3 | BRF_GRA },           //  2
	{ "ac1803m0.u7",	0x400000, 0x45484866, 3 | BRF_GRA },           //  3
	{ "ac1804m0.u10",	0x400000, 0x5e0b2285, 3 | BRF_GRA },           //  4
	{ "ac1805m0.u8",	0x400000, 0x19771f43, 3 | BRF_GRA },           //  5
	{ "ac1806m0.u11",	0x400000, 0x816b97dc, 3 | BRF_GRA },           //  6

	{ "ac1410m0.u41",	0x400000, 0xecf1f255, 4 | BRF_SND },           //  7 Ensoniq samples 0
};

STD_ROM_PICK(cairblad)
STD_ROM_FN(cairblad)

static void CairbladV60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvNVRAM,			0x580000, 0x58ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xe00000, 0xffffff, MAP_ROM);
	v60SetWriteWordHandler(common_main_write_word);
	v60SetWriteByteHandler(common_main_write_byte);
	v60SetReadWordHandler(common_main_read_word);
	v60SetReadByteHandler(common_main_read_byte);
}

static INT32 CairbladInit()
{
	return DrvCommonInit(CairbladV60Map, NULL, 1, 0, -1, -1, -1, 0.80, 1);
}

struct BurnDriver BurnDrvCairblad = {
	"cairblad", NULL, NULL, NULL, "1999",
	"Change Air Blade (Japan)\0", NULL, "Sammy", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA_SSV, GBF_VERSHOOT, 0,
	NULL, cairbladRomInfo, cairbladRomName, NULL, NULL, DrvInputInfo, CairbladDIPInfo,
	CairbladInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	240, 338, 3, 4
};


// Ultra X Weapons / Ultra Keibitai

static struct BurnRomInfo ultraxRomDesc[] = {
	{ "71047-11.u64",	0x080000, 0x593b2678, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "71047-09.u65",	0x080000, 0x08ea8d91, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "71047-12.u62",	0x080000, 0x76a77ab2, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "71047-10.u63",	0x080000, 0x7c79faf9, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "71047-01.u73",	0x200000, 0x66662b08, 3 | BRF_GRA },           //  4 Graphics
	{ "71047-02.u74",	0x100000, 0x6b00dc0c, 3 | BRF_GRA },           //  5
	{ "71047-03.u76",	0x200000, 0x00fcd6c2, 3 | BRF_GRA },           //  6
	{ "71047-04.u77",	0x100000, 0xd9e710d1, 3 | BRF_GRA },           //  7
	{ "71047-05.u75",	0x200000, 0x10848193, 3 | BRF_GRA },           //  8
	{ "71047-06.u88",	0x100000, 0xb8ac2942, 3 | BRF_GRA },           //  9

	{ "71047-07.u59",	0x200000, 0xd9828b62, 0 | BRF_SND },           // 10 Ensoniq samples 0

	{ "71047-08.u60",	0x200000, 0x30ebff6d, 1 | BRF_SND },           // 11 Ensoniq samples 1
};

STD_ROM_PICK(ultrax)
STD_ROM_FN(ultrax)

static void UltraxV60Map()
{
	v60MapMemory(DrvV60RAM2,		0x000000, 0x03ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xe00000, 0xffffff, MAP_ROM);	
	v60SetWriteWordHandler(common_main_write_word);
	v60SetWriteByteHandler(common_main_write_byte);
	v60SetReadWordHandler(common_main_read_word);
	v60SetReadByteHandler(common_main_read_byte);
}

static INT32 UltraxInit()
{
	interrupt_ultrax = 1;

	return DrvCommonInit(UltraxV60Map, NULL, 0, 0, 1, 0, 1, 0.80, 1);
}

struct BurnDriver BurnDrvUltrax = {
	"ultrax", NULL, NULL, NULL, "1995",
	"Ultra X Weapons / Ultra Keibitai\0", NULL, "Banpresto / Tsuburaya Productions", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA_SSV, GBF_VERSHOOT, 0,
	NULL, ultraxRomInfo, ultraxRomName, NULL, NULL, DrvInputInfo, UltraxDIPInfo,
	UltraxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	240, 336, 3, 4
};


// Ultra X Weapons / Ultra Keibitai (GAMEST review build)

static struct BurnRomInfo ultraxgRomDesc[] = {
	{ "sx010-11.5h",	0x080000, 0x58554bdd, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "sx010-09.5c",	0x080000, 0x153e79b2, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "sx010-12.5k",	0x080000, 0x14ad58c9, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "sx010-10.5d",	0x080000, 0x7e64473e, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "71047-01.u73",	0x200000, 0x66662b08, 3 | BRF_GRA },           //  4 Graphics
	{ "71047-02.u74",	0x100000, 0x6b00dc0c, 3 | BRF_GRA },           //  5
	{ "71047-03.u76",	0x200000, 0x00fcd6c2, 3 | BRF_GRA },           //  6
	{ "71047-04.u77",	0x100000, 0xd9e710d1, 3 | BRF_GRA },           //  7
	{ "71047-05.u75",	0x200000, 0x10848193, 3 | BRF_GRA },           //  8
	{ "71047-06.u88",	0x100000, 0xb8ac2942, 3 | BRF_GRA },           //  9

	{ "71047-07.u59",	0x200000, 0xd9828b62, 0 | BRF_SND },           // 10 Ensoniq samples 0

	{ "71047-08.u60",	0x200000, 0x30ebff6d, 1 | BRF_SND },           // 11 Ensoniq samples 1
};

STD_ROM_PICK(ultraxg)
STD_ROM_FN(ultraxg)

struct BurnDriver BurnDrvUltraxg = {
	"ultraxg", "ultrax", NULL, NULL, "1995",
	"Ultra X Weapons / Ultra Keibitai (GAMEST review build)\0", NULL, "Banpresto / Tsuburaya Productions", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA_SSV, GBF_VERSHOOT, 0,
	NULL, ultraxgRomInfo, ultraxgRomName, NULL, NULL, DrvInputInfo, UltraxDIPInfo,
	UltraxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	240, 336, 3, 4
};


// Storm Blade (US)

static struct BurnRomInfo stmbladeRomDesc[] = {
	{ "sb-pd0.u26",		0x100000, 0x91c4fbf7, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "s-blade.u37",	0x080000, 0xa6a42cc7, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "s-blade.u33",	0x080000, 0x16104ca6, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "sb-a0.u41",		0x200000, 0x2a327b51, 3 | BRF_GRA },           //  3 Graphics
	{ "sb-a1.u35",		0x200000, 0x246f6f28, 3 | BRF_GRA },           //  4
	{ "sb-a2.u32",		0x080000, 0x2049acf3, 3 | BRF_GRA },           //  5
	{ "sb-b0.u25",		0x200000, 0xb3aa3e68, 3 | BRF_GRA },           //  6
	{ "sb-b1.u21",		0x200000, 0xe95b38e7, 3 | BRF_GRA },           //  7
	{ "sb-b2.u18",		0x080000, 0xd080e620, 3 | BRF_GRA },           //  8
	{ "sb-c0.u11",		0x200000, 0x825dd8f1, 3 | BRF_GRA },           //  9
	{ "sb-c1.u7",		0x200000, 0x744afcd7, 3 | BRF_GRA },           // 10
	{ "sb-c2.u4",		0x080000, 0xfd1d2a92, 3 | BRF_GRA },           // 11

	{ "sb-snd0.u22",	0x200000, 0x4efd605b, 0 | BRF_SND },           // 12 Ensoniq samples 0

	{ "st010.bin",		0x011000, 0xaa11ee2d, 0 | BRF_PRG | BRF_ESS }, // 13 ST010 Code
};

STD_ROM_PICK(stmblade)
STD_ROM_FN(stmblade)

static void StmbladeV60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvNVRAM,			0x580000, 0x5807ff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xc00000, 0xcfffff, MAP_ROM);
	v60MapMemory(DrvV60ROM + 0x100000,	0xe00000, 0xffffff, MAP_ROM);
	v60SetWriteWordHandler(common_main_write_word);
	v60SetWriteByteHandler(common_main_write_byte);
	v60SetReadWordHandler(common_main_read_word);
	v60SetReadByteHandler(common_main_read_byte);

	st010Expand(13);
}

static INT32 StmbladeInit()
{
	watchdog_disable = 1;
	vbl_invert = 1;

	return DrvCommonInit(StmbladeV60Map, NULL, 0, 0, -1, -1, -1, 0.80, 0);
}

struct BurnDriver BurnDrvStmblade = {
	"stmblade", NULL, NULL, NULL, "1996",
	"Storm Blade (US)\0", NULL, "Visco", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA_SSV, GBF_VERSHOOT, 0,
	NULL, stmbladeRomInfo, stmbladeRomName, NULL, NULL, DrvInputInfo, StmbladeDIPInfo,
	StmbladeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	240, 352, 3, 4
};


// Storm Blade (Japan)

static struct BurnRomInfo stmbladejRomDesc[] = {
	{ "sb-pd0.u26",		0x100000, 0x91c4fbf7, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "u37j.u37",		0x080000, 0xdce20df8, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "u33j.u33",		0x080000, 0x12f68940, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "sb-a0.u41",		0x200000, 0x2a327b51, 3 | BRF_GRA },           //  3 Graphics
	{ "sb-a1.u35",		0x200000, 0x246f6f28, 3 | BRF_GRA },           //  4
	{ "sb-a2.u32",		0x080000, 0x2049acf3, 3 | BRF_GRA },           //  5
	{ "sb-b0.u25",		0x200000, 0xb3aa3e68, 3 | BRF_GRA },           //  6
	{ "sb-b1.u21",		0x200000, 0xe95b38e7, 3 | BRF_GRA },           //  7
	{ "sb-b2.u18",		0x080000, 0xd080e620, 3 | BRF_GRA },           //  8
	{ "sb-c0.u11",		0x200000, 0x825dd8f1, 3 | BRF_GRA },           //  9
	{ "sb-c1.u7",		0x200000, 0x744afcd7, 3 | BRF_GRA },           // 10
	{ "sb-c2.u4",		0x080000, 0xfd1d2a92, 3 | BRF_GRA },           // 11

	{ "sb-snd0.u22",	0x200000, 0x4efd605b, 0 | BRF_SND },           // 12 Ensoniq samples 0

	{ "st010.bin",		0x011000, 0xaa11ee2d, 0 | BRF_PRG | BRF_ESS }, // 13 ST010 Code
};

STD_ROM_PICK(stmbladej)
STD_ROM_FN(stmbladej)

struct BurnDriver BurnDrvStmbladej = {
	"stmbladej", "stmblade", NULL, NULL, "1996",
	"Storm Blade (Japan)\0", NULL, "Visco", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA_SSV, GBF_VERSHOOT, 0,
	NULL, stmbladejRomInfo, stmbladejRomName, NULL, NULL, DrvInputInfo, StmbladeDIPInfo,
	StmbladeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	240, 352, 3, 4
};


// Gourmet Battle Quiz Ryohrioh CooKing (Japan)

static struct BurnRomInfo ryoriohRomDesc[] = {
	{ "ryorioh.dat",	0x200000, 0xd1335a6a, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "ryorioh.l",		0x080000, 0x9ad60e7d, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "ryorioh.h",		0x080000, 0x0655fcff, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "ryorioh.a0",		0x400000, 0xf76ee003, 3 | BRF_GRA },           //  3 Graphics
	{ "ryorioh.a1",		0x400000, 0xca44d66d, 3 | BRF_GRA },           //  4
	{ "ryorioh.b0",		0x400000, 0xdaa134f4, 3 | BRF_GRA },           //  5
	{ "ryorioh.b1",		0x400000, 0x7611697c, 3 | BRF_GRA },           //  6
	{ "ryorioh.c0",		0x400000, 0x20eb49cf, 3 | BRF_GRA },           //  7
	{ "ryorioh.c1",		0x400000, 0x1370c75e, 3 | BRF_GRA },           //  8
	{ "ryorioh.d0",		0x400000, 0xffa14ef1, 3 | BRF_GRA },           //  9
	{ "ryorioh.d1",		0x400000, 0xae6055e8, 3 | BRF_GRA },           // 10

	{ "ryorioh.snd",	0x200000, 0x7bd38b76, 0 | BRF_SND },           // 11 Ensoniq samples 0
};

STD_ROM_PICK(ryorioh)
STD_ROM_FN(ryorioh)

static INT32 RyoriohInit()
{
	return DrvCommonInit(VasaraV60Map, NULL, 0, 0, -1, -1, -1, 0.80, 0);
}

struct BurnDriver BurnDrvRyorioh = {
	"ryorioh", NULL, NULL, NULL, "1998",
	"Gourmet Battle Quiz Ryohrioh CooKing (Japan)\0", NULL, "Visco", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_QUIZ, 0,
	NULL, ryoriohRomInfo, ryoriohRomName, NULL, NULL, RyoriohInputInfo, RyoriohDIPInfo,
	RyoriohInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 240, 4, 3
};


// Monster Slider (Japan)

static struct BurnRomInfo msliderRomDesc[] = {
	{ "ms-pl.bin",		0x080000, 0x70b2a05d, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "ms-ph.bin",		0x080000, 0x34a64e9f, 2 | BRF_PRG | BRF_ESS }, //  1

	{ "ms-a0.bin",		0x200000, 0x7ed38ccc, 3 | BRF_GRA },           //  2 Graphics
	{ "ms-a1.bin",		0x080000, 0x83f5995f, 3 | BRF_GRA },           //  3
	{ "ms-b0.bin",		0x200000, 0xfaa076e1, 3 | BRF_GRA },           //  4
	{ "ms-b1.bin",		0x080000, 0xef9748db, 3 | BRF_GRA },           //  5
	{ "ms-c0.bin",		0x200000, 0xf9d3e052, 3 | BRF_GRA },           //  6
	{ "ms-c1.bin",		0x080000, 0x7f910c5a, 3 | BRF_GRA },           //  7

	{ "ms-snd0.bin",	0x200000, 0xcda6e3a5, 4 | BRF_SND },           //  8 Ensoniq samples 0
	{ "ms-snd1.bin",	0x200000, 0x8f484b35, 4 | BRF_SND },           //  9
};

STD_ROM_PICK(mslider)
STD_ROM_FN(mslider)

static void MsliderV60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvV60RAM0,		0x010000, 0x01ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xf00000, 0xffffff, MAP_ROM);	
	v60SetWriteWordHandler(common_main_write_word);
	v60SetWriteByteHandler(common_main_write_byte);
	v60SetReadWordHandler(common_main_read_word);
	v60SetReadByteHandler(common_main_read_byte);
}

static INT32 MsliderInit()
{
	watchdog_disable = 1;

	return DrvCommonInit(MsliderV60Map, NULL, 0, 0, -1, -1, -1, 0.80, 1);
}

struct BurnDriver BurnDrvMslider = {
	"mslider", NULL, NULL, NULL, "1997",
	"Monster Slider (Japan)\0", NULL, "Visco / Datt Japan", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_PUZZLE, 0,
	NULL, msliderRomInfo, msliderRomName, NULL, NULL, DrvInputInfo, MsliderDIPInfo,
	MsliderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	352, 240, 4, 3
};


// Mobil Suit Gundam Final Shooting (Japan)

static struct BurnRomInfo gdfsRomDesc[] = {
	{ "vg004-14.u3",	0x100000, 0xd88254df, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "ssv2set0.u1",	0x080000, 0xc23b9e2c, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "ssv2set1.u2",	0x080000, 0xd7d52570, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "vg004-09.u43",	0x200000, 0xb7382cfa, 3 | BRF_GRA },           //  3 Graphics
	{ "vg004-10.u45",	0x200000, 0xb3c6b1cb, 3 | BRF_GRA },           //  4
	{ "vg004-11.u48",	0x200000, 0x1491def1, 3 | BRF_GRA },           //  5

	{ "vg004-01.u33",	0x200000, 0xaa9a81c2, 0 | BRF_GRA },           //  6 ST0020 sprites
	{ "vg004-02.u34",	0x200000, 0xfa40ecb4, 0 | BRF_GRA },           //  7
	{ "vg004-03.u35",	0x200000, 0x90004023, 0 | BRF_GRA },           //  8
	{ "vg004-04.u36",	0x200000, 0xfdafd289, 0 | BRF_GRA },           //  9
	{ "vg004-05.u37",	0x200000, 0x9ae488b0, 0 | BRF_GRA },           // 10
	{ "vg004-06.u38",	0x200000, 0x3402325f, 0 | BRF_GRA },           // 11
	{ "vg004-07.u39",	0x200000, 0x5e89fcf9, 0 | BRF_GRA },           // 12
	{ "vg004-08.u40",	0x200000, 0x6b1746dc, 0 | BRF_GRA },           // 13

	{ "ssvv7.u16",		0x080000, 0xf1c3ab6f, 8 | BRF_GRA },           // 14 Tilemap tiles

	{ "vg004-12.u4",	0x200000, 0xeb41a4ef, 5 | BRF_SND },           // 15 Ensoniq samples 0
	{ "vg004-13.u5",	0x200000, 0xa4ed3977, 5 | BRF_SND },           // 16
};

STD_ROM_PICK(gdfs)
STD_ROM_FN(gdfs)

static void GdfsV60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvTMAPRAM,		0x400000, 0x43ffff, MAP_RAM);
	v60MapMemory(DrvDspRAM,			0x482000, 0x482fff, MAP_RAM);
	v60MapMemory(DrvV60RAM2,		0x600000, 0x600fff, MAP_RAM);
	v60MapMemory(st0020SprRAM,		0x800000, 0x87ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xc00000, 0xffffff, MAP_ROM);
	v60SetWriteWordHandler(gdfs_write_word);
	v60SetWriteByteHandler(gdfs_write_byte);
	v60SetReadWordHandler(gdfs_read_word);
	v60SetReadByteHandler(gdfs_read_byte);

	EEPROMInit(&eeprom_interface_93C46);
}

static void GdfsRomLoadCallback()
{
	memcpy (DrvV60ROM + 0x200000, DrvV60ROM + 0x100000, 0x100000);
	memcpy (DrvV60ROM + 0x300000, DrvV60ROM + 0x100000, 0x100000);
	memcpy (DrvV60ROM + 0x100000, DrvV60ROM + 0x000000, 0x100000);

	if (BurnLoadRom(st0020GfxROM + 0x0000000,  6, 1)) return;
	if (BurnLoadRom(st0020GfxROM + 0x0200000,  7, 1)) return;
	if (BurnLoadRom(st0020GfxROM + 0x0400000,  8, 1)) return;
	if (BurnLoadRom(st0020GfxROM + 0x0600000,  9, 1)) return;
	if (BurnLoadRom(st0020GfxROM + 0x0800000, 10, 1)) return;
	if (BurnLoadRom(st0020GfxROM + 0x0a00000, 11, 1)) return;
	if (BurnLoadRom(st0020GfxROM + 0x0c00000, 12, 1)) return;
	if (BurnLoadRom(st0020GfxROM + 0x0e00000, 13, 1)) return;

	if (BurnLoadRom(DrvSndROM0 + 0x000001, 15, 2)) return;
	if (BurnLoadRom(DrvSndROM0 + 0x000000, 16, 2)) return;
}

static INT32 GdfsInit()
{
	is_gdfs = 1;
	st0020GfxROMLen = 0x1000000;
	watchdog_disable = 1;

	return DrvCommonInit(GdfsV60Map, GdfsRomLoadCallback, 0, 0, 0, 0, 0, 0.80, 1);
}

struct BurnDriver BurnDrvGdfs = {
	"gdfs", NULL, NULL, NULL, "1995",
	"Mobil Suit Gundam Final Shooting (Japan)\0", NULL, "Banpresto", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_SHOOT, 0,
	NULL, gdfsRomInfo, gdfsRomName, NULL, NULL, GdfsInputInfo, GdfsDIPInfo,
	GdfsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 240, 4, 3
};


// Lovely Pop Mahjong JangJang Shimasho (Japan)

static struct BurnRomInfo janjans1RomDesc[] = {
	{ "jj1-data.bin",	0x200000, 0x6734537e, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "jj1-prol.bin",	0x080000, 0x4231d928, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "jj1-proh.bin",	0x080000, 0x651383c6, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "jj1-a0.bin",		0x400000, 0x39bbbc46, 3 | BRF_GRA },           //  3 Graphics
	{ "jj1-a1.bin",		0x400000, 0x26020133, 3 | BRF_GRA },           //  4
	{ "jj1-a2.bin",		0x200000, 0xe993251e, 3 | BRF_GRA },           //  5
	{ "jj1-b0.bin",		0x400000, 0x8ee66b0a, 3 | BRF_GRA },           //  6
	{ "jj1-b1.bin",		0x400000, 0x048719b3, 3 | BRF_GRA },           //  7
	{ "jj1-b2.bin",		0x200000, 0x6e95af3f, 3 | BRF_GRA },           //  8
	{ "jj1-c0.bin",		0x400000, 0x9df28afc, 3 | BRF_GRA },           //  9
	{ "jj1-c1.bin",		0x400000, 0xeb470ed3, 3 | BRF_GRA },           // 10
	{ "jj1-c2.bin",		0x200000, 0xaaf72c2d, 3 | BRF_GRA },           // 11
	{ "jj1-d0.bin",		0x400000, 0x2b3bd591, 3 | BRF_GRA },           // 12
	{ "jj1-d1.bin",		0x400000, 0xf24c0d36, 3 | BRF_GRA },           // 13
	{ "jj1-d2.bin",		0x200000, 0x481b3be8, 3 | BRF_GRA },           // 14

	{ "jj1-snd0.bin",	0x200000, 0x4f7d620a, 0 | BRF_SND },           // 15 Ensoniq samples 0

	{ "jj1-snd1.bin",	0x200000, 0x9b3a7ae5, 1 | BRF_SND },           // 16 Ensoniq samples 1
};

STD_ROM_PICK(janjans1)
STD_ROM_FN(janjans1)

static void Janjans1V60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvV60RAM0,		0x010000, 0x01ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xc00000, 0xffffff, MAP_ROM);	
	v60SetWriteWordHandler(janjan1_write_word);
	v60SetWriteByteHandler(janjan1_write_byte);
	v60SetReadWordHandler(janjan1_read_word);
	v60SetReadByteHandler(janjan1_read_byte);
}

static INT32 Janjans1Init()
{
	watchdog_disable = 1;

	return DrvCommonInit(Janjans1V60Map, NULL, 0, 0, 1, 0, 1, 0.80, 0);
}

struct BurnDriver BurnDrvJanjans1 = {
	"janjans1", NULL, NULL, NULL, "1996",
	"Lovely Pop Mahjong JangJang Shimasho (Japan)\0", NULL, "Visco", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_MAHJONG, 0,
	NULL, janjans1RomInfo, janjans1RomName, NULL, NULL, MahjongInputInfo, Janjans1DIPInfo,
	Janjans1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 238, 4, 3
};


// Lovely Pop Mahjong JangJang Shimasho 2 (Japan)

static struct BurnRomInfo janjans2RomDesc[] = {
	{ "jan2-dat.u28",	0x200000, 0x0c9c62bf, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "jan2-prol.u26",	0x080000, 0x758a7249, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "jan2-proh.u27",	0x080000, 0xfcd5da62, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "jan2-a0.u13",	0x400000, 0x37869bea, 3 | BRF_GRA },           //  3 Graphics
	{ "jan2-a1.u14",	0x400000, 0x8189e74f, 3 | BRF_GRA },           //  4
	{ "jan2-b0.u16",	0x400000, 0x19877c5c, 3 | BRF_GRA },           //  5
	{ "jan2-b1.u17",	0x400000, 0x8d0f7190, 3 | BRF_GRA },           //  6
	{ "jan2-c0.u21",	0x400000, 0x8bdff3d5, 3 | BRF_GRA },           //  7
	{ "jan2-c1.u22",	0x400000, 0xf7ea5934, 3 | BRF_GRA },           //  8
	{ "jan2-d0.u34",	0x400000, 0x479fdb54, 3 | BRF_GRA },           //  9
	{ "jan2-d1.u35",	0x400000, 0xc0148895, 3 | BRF_GRA },           // 10

	{ "jan2-snd0.u29",	0x200000, 0x22cc054e, 0 | BRF_SND },           // 11 Ensoniq samples 0

	{ "jan2-snd1.u33",	0x200000, 0xcbcac4a6, 1 | BRF_SND },           // 12 Ensoniq samples 1
};

STD_ROM_PICK(janjans2)
STD_ROM_FN(janjans2)

static INT32 Janjans2Init()
{
	return DrvCommonInit(Janjans1V60Map, NULL, 0, 0, 1, 0, 1, 0.80, 0);
}

struct BurnDriver BurnDrvJanjans2 = {
	"janjans2", NULL, NULL, NULL, "2000",
	"Lovely Pop Mahjong JangJang Shimasho 2 (Japan)\0", NULL, "Visco", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_MAHJONG, 0,
	NULL, janjans2RomInfo, janjans2RomName, NULL, NULL, Srmp4InputInfo, Janjans2DIPInfo,
	Janjans2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 238, 4, 3
};


// Koi Koi Shimasho 2 - Super Real Hanafuda (Japan)

static struct BurnRomInfo koikois2RomDesc[] = {
	{ "u26.bin",		0x080000, 0x4be937a1, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "u27.bin",		0x080000, 0x25f39d93, 2 | BRF_PRG | BRF_ESS }, //  1

	{ "kk2-a0.bin",		0x400000, 0xb94b76c2, 3 | BRF_GRA },           //  2 Graphics
	{ "kk2-a1.bin",		0x200000, 0xa7c99f56, 3 | BRF_GRA },           //  3
	{ "kk2-b0.bin",		0x400000, 0x4d028972, 3 | BRF_GRA },           //  4
	{ "kk2-b1.bin",		0x200000, 0x778ec9fb, 3 | BRF_GRA },           //  5
	{ "kk2-c0.bin",		0x400000, 0x34b699d9, 3 | BRF_GRA },           //  6
	{ "kk2-c1.bin",		0x200000, 0xab451e88, 3 | BRF_GRA },           //  7
	{ "kk2-d0.bin",		0x400000, 0x0e3005a4, 3 | BRF_GRA },           //  8
	{ "kk2-d1.bin",		0x200000, 0x17a02252, 3 | BRF_GRA },           //  9

	{ "kk2_snd0.bin",	0x200000, 0xb27eaa94, 0 | BRF_SND },           // 10 Ensoniq samples 0

	{ "kk2_snd1.bin",	0x200000, 0xe5a963e1, 1 | BRF_SND },           // 11 Ensoniq samples 1
};

STD_ROM_PICK(koikois2)
STD_ROM_FN(koikois2)

static void Koikois2V60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvV60RAM0,		0x010000, 0x01ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xf00000, 0xffffff, MAP_ROM);	
	v60SetWriteWordHandler(janjan1_write_word);
	v60SetWriteByteHandler(janjan1_write_byte);
	v60SetReadWordHandler(janjan1_read_word);
	v60SetReadByteHandler(janjan1_read_byte);
}

static INT32 Koikois2Init()
{
	return DrvCommonInit(Koikois2V60Map, NULL, 0, 0, 1, 0, 1, 1.40, 0);
}

struct BurnDriver BurnDrvKoikois2 = {
	"koikois2", NULL, NULL, NULL, "1997",
	"Koi Koi Shimasho 2 - Super Real Hanafuda (Japan)\0", NULL, "Visco", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_MAHJONG, 0,
	NULL, koikois2RomInfo, koikois2RomName, NULL, NULL, MahjongInputInfo, Koikois2DIPInfo,
	Koikois2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 240, 4, 3
};


// Super Real Mahjong PIV (Japan)

static struct BurnRomInfo srmp4RomDesc[] = {
	{ "sx001-14.prl",	0x080000, 0x19aaf46e, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "sx001-15.prh",	0x080000, 0xdbd31399, 2 | BRF_PRG | BRF_ESS }, //  1

	{ "sx001-01.a0",	0x200000, 0x94ee9203, 3 | BRF_GRA },           //  2 Graphics
	{ "sx001-04.a1",	0x200000, 0x38c9c49a, 3 | BRF_GRA },           //  3
	{ "sx001-07.a2",	0x200000, 0xee66021e, 3 | BRF_GRA },           //  4
	{ "sx001-02.b0",	0x200000, 0xadffb598, 3 | BRF_GRA },           //  5
	{ "sx001-05.b1",	0x200000, 0x4c400a38, 3 | BRF_GRA },           //  6
	{ "sx001-08.b2",	0x200000, 0x36efd52c, 3 | BRF_GRA },           //  7
	{ "sx001-03.c0",	0x200000, 0x4336b037, 3 | BRF_GRA },           //  8
	{ "sx001-06.c1",	0x200000, 0x6fe7229e, 3 | BRF_GRA },           //  9
	{ "sx001-09.c2",	0x200000, 0x91dd8218, 3 | BRF_GRA },           // 10

	{ "sx001-10.sd0",	0x200000, 0x45409ef1, 4 | BRF_SND },           // 11 Ensoniq samples 0
};

STD_ROM_PICK(srmp4)
STD_ROM_FN(srmp4)

static void Srmp4V60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvV60RAM0,		0x010000, 0x01ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xf00000, 0xffffff, MAP_ROM);	
	v60SetWriteWordHandler(janjan1_write_word);
	v60SetWriteByteHandler(janjan1_write_byte);
	v60SetReadWordHandler(janjan1_read_word);
	v60SetReadByteHandler(janjan1_read_byte);
}

static void Srpm4RomLoadCallback()
{
	memcpy (DrvSndROM0 + 0x200000, DrvSndROM0, 0x200000);
}

static INT32 Srmp4Init()
{
	return DrvCommonInit(Srmp4V60Map, Srpm4RomLoadCallback, 0, 0, 1, 0, 1, 0.80, 0);
}

struct BurnDriver BurnDrvSrmp4 = {
	"srmp4", NULL, NULL, NULL, "1993",
	"Super Real Mahjong PIV (Japan)\0", NULL, "Seta", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_MAHJONG, 0,
	NULL, srmp4RomInfo, srmp4RomName, NULL, NULL, Srmp4InputInfo, Srmp4DIPInfo,
	Srmp4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 240, 4, 3
};


// Super Real Mahjong PIV (Japan, older set)

static struct BurnRomInfo srmp4oRomDesc[] = {
	{ "sx001-11.prl",	0x080000, 0xdede3e64, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "sx001-12.prh",	0x080000, 0x739c53c3, 2 | BRF_PRG | BRF_ESS }, //  1

	{ "sx001-01.a0",	0x200000, 0x94ee9203, 3 | BRF_GRA },           //  2 Graphics
	{ "sx001-04.a1",	0x200000, 0x38c9c49a, 3 | BRF_GRA },           //  3
	{ "sx001-07.a2",	0x200000, 0xee66021e, 3 | BRF_GRA },           //  4
	{ "sx001-02.b0",	0x200000, 0xadffb598, 3 | BRF_GRA },           //  5
	{ "sx001-05.b1",	0x200000, 0x4c400a38, 3 | BRF_GRA },           //  6
	{ "sx001-08.b2",	0x200000, 0x36efd52c, 3 | BRF_GRA },           //  7
	{ "sx001-03.c0",	0x200000, 0x4336b037, 3 | BRF_GRA },           //  8
	{ "sx001-06.c1",	0x200000, 0x6fe7229e, 3 | BRF_GRA },           //  9
	{ "sx001-09.c2",	0x200000, 0x91dd8218, 3 | BRF_GRA },           // 10

	{ "sx001-10.sd0",	0x200000, 0x45409ef1, 4 | BRF_SND },           // 11 Ensoniq samples 0
};

STD_ROM_PICK(srmp4o)
STD_ROM_FN(srmp4o)

struct BurnDriver BurnDrvSrmp4o = {
	"srmp4o", "srmp4", NULL, NULL, "1993",
	"Super Real Mahjong PIV (Japan, older set)\0", NULL, "Seta", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SETA_SSV, GBF_MAHJONG, 0,
	NULL, srmp4oRomInfo, srmp4oRomName, NULL, NULL, Srmp4InputInfo, Srmp4DIPInfo,
	Srmp4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 240, 4, 3
};


// Mahjong Hyper Reaction (Japan)

static struct BurnRomInfo hypreactRomDesc[] = {
	{ "s14-1-02.u2",	0x080000, 0xd90a383c, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "s14-1-01.u1",	0x080000, 0x80481401, 2 | BRF_PRG | BRF_ESS }, //  1

	{ "s14-1-07.u7",	0x200000, 0x6c429fd0, 3 | BRF_GRA },           //  2 Graphics
	{ "s14-1-05.u13",	0x200000, 0x2ff72f98, 3 | BRF_GRA },           //  3
	{ "s14-1-06.u10",	0x200000, 0xf470ec42, 3 | BRF_GRA },           //  4
	{ "s14-1-10.u6",	0x200000, 0xfdd706ba, 3 | BRF_GRA },           //  5
	{ "s14-1-08.u12",	0x200000, 0x5bb9bb0d, 3 | BRF_GRA },           //  6
	{ "s14-1-09.u9",	0x200000, 0xd1dda65f, 3 | BRF_GRA },           //  7
	{ "s14-1-13.u8",	0x200000, 0x971caf11, 3 | BRF_GRA },           //  8
	{ "s14-1-11.u14",	0x200000, 0x6d8e7bae, 3 | BRF_GRA },           //  9
	{ "s14-1-12.u11",	0x200000, 0x233a8e23, 3 | BRF_GRA },           // 10

	{ "s14-1-04.u4",	0x200000, 0xa5955336, 6 | BRF_SND },           // 11 Ensoniq samples 2
	{ "s14-1-03.u5",	0x200000, 0x283a6ec2, 6 | BRF_SND },           // 12
};

STD_ROM_PICK(hypreact)
STD_ROM_FN(hypreact)

static INT32 HypreactInit()
{
	watchdog_disable = 1;

	return DrvCommonInit(Srmp4V60Map, NULL, 0, 0, 1, 0, 1, 0.10, 0);
}

struct BurnDriver BurnDrvHypreact = {
	"hypreact", NULL, NULL, NULL, "1995",
	"Mahjong Hyper Reaction (Japan)\0", NULL, "Sammy", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_MAHJONG, 0,
	NULL, hypreactRomInfo, hypreactRomName, NULL, NULL, HypreactInputInfo, HypreactDIPInfo,
	HypreactInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	338, 240, 4, 3
};


// Mahjong Hyper Reaction 2 (Japan)

static struct BurnRomInfo hypreac2RomDesc[] = {
	{ "u2.bin",		0x080000, 0x05c93266, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "u1.bin",		0x080000, 0x80cf9e59, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "u47.bin",		0x080000, 0xa3e9bfee, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "u46.bin",		0x080000, 0x68c41235, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "s16-1-16.u6",	0x400000, 0xb308ac34, 3 | BRF_GRA },           //  4 Graphics
	{ "s16-1-15.u9",	0x400000, 0x2c8e381e, 3 | BRF_GRA },           //  5
	{ "s16-1-14.u12",	0x200000, 0xafe9d187, 3 | BRF_GRA },           //  6
	{ "s16-1-10.u7",	0x400000, 0x86a10cbd, 3 | BRF_GRA },           //  7
	{ "s16-1-09.u10",	0x400000, 0x6b8e4d92, 3 | BRF_GRA },           //  8
	{ "s16-1-08.u13",	0x200000, 0xb355f45d, 3 | BRF_GRA },           //  9
	{ "s16-1-13.u8",	0x400000, 0x89869af2, 3 | BRF_GRA },           // 10
	{ "s16-1-12.u11",	0x400000, 0x87d9c748, 3 | BRF_GRA },           // 11
	{ "s16-1-11.u14",	0x200000, 0x70b3c0a0, 3 | BRF_GRA },           // 12

	{ "s16-1-06.u41",	0x400000, 0x626e8a81, 4 | BRF_SND },           // 13 Ensoniq samples 0

	{ "s16-1-07.u42",	0x400000, 0x42bcb41b, 5 | BRF_SND },           // 14 Ensoniq samples 1

	{ "s16-1-07.u42",	0x400000, 0x42bcb41b, 6 | BRF_SND },           // 15 Ensoniq samples 2
};

STD_ROM_PICK(hypreac2)
STD_ROM_FN(hypreac2)

static void Hypreac2V60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvDspRAM,			0x482000, 0x482fff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xe00000, 0xffffff, MAP_ROM);	
	v60SetWriteWordHandler(hypreac2_write_word);
	v60SetWriteByteHandler(hypreac2_write_byte);
	v60SetReadWordHandler(hypreac2_read_word);
	v60SetReadByteHandler(hypreac2_read_byte);
}

static INT32 Hypreac2Init()
{
	watchdog_disable = 1;

	return DrvCommonInit(Hypreac2V60Map, NULL, 1, 0, 1, 2, -1, 0.10, 0);
}

struct BurnDriver BurnDrvHypreac2 = {
	"hypreac2", NULL, NULL, NULL, "1997",
	"Mahjong Hyper Reaction 2 (Japan)\0", NULL, "Sammy", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_MAHJONG, 0,
	NULL, hypreac2RomInfo, hypreac2RomName, NULL, NULL, Hypreac2InputInfo, Hypreac2DIPInfo,
	Hypreac2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	338, 240, 4, 3
};


// Super Real Mahjong P7 (Japan)

static struct BurnRomInfo srmp7RomDesc[] = {
	{ "sx015-10.dat",	0x200000, 0xfad3ac6a, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "sx015-07.pr0",	0x080000, 0x08d7f841, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "sx015-08.pr1",	0x080000, 0x90307825, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "sx015-26.a0",	0x400000, 0xa997be9d, 3 | BRF_GRA },           //  3 Graphics
	{ "sx015-25.a1",	0x400000, 0x29ac4211, 3 | BRF_GRA },           //  4
	{ "sx015-24.a2",	0x400000, 0xb8fea3da, 3 | BRF_GRA },           //  5
	{ "sx015-23.a3",	0x400000, 0x9ec0b81e, 3 | BRF_GRA },           //  6
	{ "sx015-22.b0",	0x400000, 0x62c3df07, 3 | BRF_GRA },           //  7
	{ "sx015-21.b1",	0x400000, 0x55b8a431, 3 | BRF_GRA },           //  8
	{ "sx015-20.b2",	0x400000, 0xe84a64d7, 3 | BRF_GRA },           //  9
	{ "sx015-19.b3",	0x400000, 0x994b5063, 3 | BRF_GRA },           // 10
	{ "sx015-18.c0",	0x400000, 0x72d43fd4, 3 | BRF_GRA },           // 11
	{ "sx015-17.c1",	0x400000, 0xfdfd82f1, 3 | BRF_GRA },           // 12
	{ "sx015-16.c2",	0x400000, 0x86aa314b, 3 | BRF_GRA },           // 13
	{ "sx015-15.c3",	0x400000, 0x11f50e16, 3 | BRF_GRA },           // 14
	{ "sx015-14.d0",	0x400000, 0x186f83fa, 3 | BRF_GRA },           // 15
	{ "sx015-13.d1",	0x400000, 0xea6e5329, 3 | BRF_GRA },           // 16
	{ "sx015-12.d2",	0x400000, 0x80336523, 3 | BRF_GRA },           // 17
	{ "sx015-11.d3",	0x400000, 0x134c8e28, 3 | BRF_GRA },           // 18

	{ "sx015-06.s0",	0x200000, 0x0d5a206c, 0 | BRF_SND },           // 19 Ensoniq samples 0

	{ "sx015-05.s1",	0x200000, 0xbb8cebe2, 1 | BRF_SND },           // 20 Ensoniq samples 1

	{ "sx015-04.s2",	0x200000, 0xf6e933df, 2 | BRF_SND },           // 21 Ensoniq samples 2
	{ "sx015-02.s4",	0x200000, 0x6567bc3e, 2 | BRF_SND },           // 22

	{ "sx015-03.s3",	0x200000, 0x5b51ab21, 3 | BRF_SND },           // 23 Ensoniq samples 3
	{ "sx015-01.s5",	0x200000, 0x481b00ed, 3 | BRF_SND },           // 24
};

STD_ROM_PICK(srmp7)
STD_ROM_FN(srmp7)

static void Srmp7V60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvV60RAM2,		0x010000, 0x050fff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xc00000, 0xffffff, MAP_ROM);	
	v60SetWriteWordHandler(srmp7_write_word);
	v60SetWriteByteHandler(srmp7_write_byte);
	v60SetReadWordHandler(srmp7_read_word);
	v60SetReadByteHandler(srmp7_read_byte);
}

static void Srmp7ROMCallback()
{
	return; // iq_132

	memcpy (DrvSndROM0 + 0x400000, DrvSndROM0, 0x400000);
	memcpy (DrvSndROM1 + 0x400000, DrvSndROM1, 0x400000);
}

static INT32 Srmp7Init()
{
	return DrvCommonInit(Srmp7V60Map, Srmp7ROMCallback, 0, 0, 1, 2, 3, 0.80, 0);
}

struct BurnDriver BurnDrvSrmp7 = {
	"srmp7", NULL, NULL, NULL, "1997",
	"Super Real Mahjong P7 (Japan)\0", "No sound.", "Seta", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_MAHJONG, 0,
	NULL, srmp7RomInfo, srmp7RomName, NULL, NULL, Srmp7InputInfo, Srmp7DIPInfo,
	Srmp7Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 239, 4, 3
};


// Pachinko Sexy Reaction (Japan)

static struct BurnRomInfo sxyreactRomDesc[] = {
	{ "ac414e00.u2",	0x080000, 0xd5dd7593, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "ac413e00.u1",	0x080000, 0xf46aee4a, 2 | BRF_PRG | BRF_ESS }, //  1
	{ "ac416e00.u47",	0x080000, 0xe0f7bba9, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "ac415e00.u46",	0x080000, 0x92de1b5f, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "ac1401m0.u6",	0x400000, 0x0b7b693c, 3 | BRF_GRA },           //  4 Graphics
	{ "ac1402m0.u9",	0x400000, 0x9d593303, 3 | BRF_GRA },           //  5
	{ "ac1403m0.u12",	0x200000, 0xaf433eca, 3 | BRF_GRA },           //  6
	{ "ac1404m0.u7",	0x400000, 0xcdda2ccb, 3 | BRF_GRA },           //  7
	{ "ac1405m0.u10",	0x400000, 0xe5e7a5df, 3 | BRF_GRA },           //  8
	{ "ac1406m0.u13",	0x200000, 0xc7053409, 3 | BRF_GRA },           //  9
	{ "ac1407m0.u8",	0x400000, 0x28c83d5e, 3 | BRF_GRA },           // 10
	{ "ac1408m0.u11",	0x400000, 0xc45bab47, 3 | BRF_GRA },           // 11
	{ "ac1409m0.u14",	0x200000, 0xbe1c66c2, 3 | BRF_GRA },           // 12

	{ "ac1410m0.u41",	0x400000, 0x2a880afc, 4 | BRF_SND },           // 13 Ensoniq samples 0

	{ "ac1411m0.u42",	0x400000, 0x2ba4ca43, 5 | BRF_SND },           // 14 Ensoniq samples 1
};

STD_ROM_PICK(sxyreact)
STD_ROM_FN(sxyreact)

static void SexyreactRomLoadCallback()
{
	DrvMemSwap(DrvSndROM1, DrvSndROM1 + 0x200000, 0x200000);
}

static void SxyreactV60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvNVRAM,			0x580800, 0x58ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xe00000, 0xffffff, MAP_ROM);	
	v60SetWriteWordHandler(sxyreact_write_word);
	v60SetWriteByteHandler(sxyreact_write_byte);
	v60SetReadWordHandler(sxyreact_read_word);
	v60SetReadByteHandler(sxyreact_read_byte);
}

static INT32 SxyreactInit()
{
	return DrvCommonInit(SxyreactV60Map, SexyreactRomLoadCallback, 1, 0, 1, 2, -1, 0.10, 0);
}

struct BurnDriver BurnDrvSxyreact = {
	"sxyreact", NULL, NULL, NULL, "1998",
	"Pachinko Sexy Reaction (Japan)\0", NULL, "Sammy", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_MISC, 0,
	NULL, sxyreactRomInfo, sxyreactRomName, NULL, NULL, SxyreactInputInfo, SxyreactDIPInfo,
	SxyreactInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	338, 240, 4, 3
};


// Pachinko Sexy Reaction 2 (Japan)

static struct BurnRomInfo sxyreac2RomDesc[] = {
	{ "ac1714e00.u32",	0x200000, 0x78075d70, 1 | BRF_PRG | BRF_ESS }, //  0 V60 Code

	{ "ac1701t00.u6",	0x400000, 0xe14611c2, 3 | BRF_GRA },           //  1 Graphics
	{ "ac1702t00.u9",	0x400000, 0x2c8b07f8, 3 | BRF_GRA },           //  2
	{ "ac1703t00.u7",	0x400000, 0xd6c7e861, 3 | BRF_GRA },           //  3
	{ "ac1704t00.u10",	0x400000, 0x5fa7ccf0, 3 | BRF_GRA },           //  4
	{ "ac1705t00.u8",	0x400000, 0x2dff0652, 3 | BRF_GRA },           //  5
	{ "ac1706t00.u11",	0x400000, 0xe7a168e0, 3 | BRF_GRA },           //  6

	{ "ac1707t00.u41",	0x400000, 0x28999bc4, 4 | BRF_SND },           //  7 Ensoniq samples 0

	{ "ac1708t00.u42",	0x400000, 0x7001eec0, 5 | BRF_SND },           //  8 Ensoniq samples 1
};

STD_ROM_PICK(sxyreac2)
STD_ROM_FN(sxyreac2)

static INT32 Sxyreac2Init()
{
	return DrvCommonInit(SxyreactV60Map, SexyreactRomLoadCallback, 1, 0, 1, 2, -1, 0.10, 0);
}

struct BurnDriver BurnDrvSxyreac2 = {
	"sxyreac2", NULL, NULL, NULL, "1999",
	"Pachinko Sexy Reaction 2 (Japan)\0", NULL, "Sammy", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_MISC, 0,
	NULL, sxyreac2RomInfo, sxyreac2RomName, NULL, NULL, SxyreactInputInfo, SxyreactDIPInfo,
	Sxyreac2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 240, 4, 3
};


// Joryuu Syougi Kyoushitsu (Japan)

static struct BurnRomInfo jskRomDesc[] = {
	{ "jsk-u72.bin",	0x080000, 0xdb6b2554, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "jsk-u71.bin",	0x080000, 0xf6774fba, 2 | BRF_PRG | BRF_ESS }, //  1

	{ "jsk-a0.bin",		0x400000, 0x18981a19, 3 | BRF_GRA },           //  2 Graphics
	{ "jsk-b0.bin",		0x400000, 0xf6df0ff9, 3 | BRF_GRA },           //  3
	{ "jsk-c0.bin",		0x400000, 0xb8282939, 3 | BRF_GRA },           //  4
	{ "jsk-d0.bin",		0x400000, 0xfc733e0c, 3 | BRF_GRA },           //  5

	{ "jsk-s0.u65",		0x200000, 0x8d1a9aeb, 0 | BRF_SND },           //  6 Ensoniq samples 0

	{ "jsk-u52.bin",	0x020000, 0xb11aef0c, 0 | BRF_PRG | BRF_ESS }, //  7 V810 Code
	{ "jsk-u38.bin",	0x020000, 0x8e5c0de3, 0 | BRF_PRG | BRF_ESS }, //  8
	{ "jsk-u24.bin",	0x020000, 0x1fa6e156, 0 | BRF_PRG | BRF_ESS }, //  9
	{ "jsk-u4.bin",		0x020000, 0xec22fb41, 0 | BRF_PRG | BRF_ESS }, // 10
};

STD_ROM_PICK(jsk)
STD_ROM_FN(jsk)

static INT32 JskInit()
{
	return 1;
}

struct BurnDriverD BurnDrvJsk = {
	"jsk", NULL, NULL, NULL, "1997",
	"Joryuu Syougi Kyoushitsu (Japan)\0", "Unemulated CPU", "Visco", "MSSV",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SETA_SSV, GBF_MAHJONG, 0,
	NULL, jskRomInfo, jskRomName, NULL, NULL, DrvInputInfo, JskDIPInfo,
	JskInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	336, 240, 4, 3
};


// Eagle Shot Golf

static struct BurnRomInfo eaglshotRomDesc[] = {
	{ "si003-09.u18",	0x080000, 0x219c71ce, 2 | BRF_PRG | BRF_ESS }, //  0 V60 Code
	{ "si003-10.u20",	0x080000, 0xc8872e48, 2 | BRF_PRG | BRF_ESS }, //  1

	{ "si003-01.u13",	0x200000, 0xd7df0d52, 8 | BRF_GRA },           //  2 Graphics
	{ "si003-02.u12",	0x200000, 0x92b4d50d, 8 | BRF_GRA },           //  3
	{ "si003-03.u11",	0x200000, 0x6ede4012, 8 | BRF_GRA },           //  4
	{ "si003-04.u10",	0x200000, 0x4c65d1a1, 8 | BRF_GRA },           //  5
	{ "si003-05.u30",	0x200000, 0xdaf52d56, 8 | BRF_GRA },           //  6
	{ "si003-06.u31",	0x200000, 0x449f9ae5, 8 | BRF_GRA },           //  7

	{ "si003-07.u23",	0x200000, 0x81679fd6, 4 | BRF_SND },           //  8 Ensoniq samples 0
	{ "si003-08.u24",	0x200000, 0xd0122ba2, 4 | BRF_SND },           //  9
};

STD_ROM_PICK(eaglshot)
STD_ROM_FN(eaglshot)

static void EaglshotV60Map()
{
	v60MapMemory(DrvV60RAM0,		0x000000, 0x00ffff, MAP_RAM);
	v60MapMemory(DrvSprRAM,			0x100000, 0x13ffff, MAP_RAM);
	v60MapMemory(DrvPalRAM,			0x140000, 0x15ffff, MAP_ROM); // handler
	v60MapMemory(DrvV60RAM1,		0x160000, 0x17ffff, MAP_RAM);
	v60MapMemory(DrvNVRAM,			0xc00000, 0xc007ff, MAP_RAM);
	v60MapMemory(DrvV60ROM,			0xf00000, 0xffffff, MAP_ROM);
	v60SetWriteWordHandler(eaglshot_write_word);
	v60SetWriteByteHandler(eaglshot_write_byte);
	v60SetReadWordHandler(eaglshot_read_word);
	v60SetReadByteHandler(eaglshot_read_byte);
}

static INT32 EaglshotInit()
{
	return DrvCommonInit(EaglshotV60Map, NULL, 0, 0, 0, 0, 0, 0.80, 0);
}

struct BurnDriver BurnDrvEaglshot = {
	"eaglshot", NULL, NULL, NULL, "1994",
	"Eagle Shot Golf\0", NULL, "Sammy", "SSV",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_SSV, GBF_SPORTSMISC, 0,
	NULL, eaglshotRomInfo, eaglshotRomName, NULL, NULL, EaglshotInputInfo, EaglshotDIPInfo,
	EaglshotInit, DrvExit, DrvFrame, DrvDraw, eaglshtScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};
