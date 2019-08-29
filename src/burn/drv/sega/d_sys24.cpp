// FB Alpha Sega System 24 driver module
// Based on MAME driver by Olivier Galibert

// Needs inputs hooked up...
// mahmajn
// mahmajn2

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "burn_ym2151.h"
#include "dac.h"
#include "fd1094_intf.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *Drv68KKey;
static UINT8 *DrvUserROM;
static UINT8 *DrvFloppyData;
static UINT8 *DrvCharExp;
static UINT8 *DrvShareRAM2;
static UINT8 *DrvShareRAM3;
static UINT8 *DrvTileRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvMixerRegs;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 irq_allow0;
static UINT8 irq_allow1;
static INT32 irq_yms;
static INT32 irq_timer;

static INT32 irq_tdata;
static INT32 irq_tmode;
static INT32 irq_vblank;
static INT32 irq_sprite;

static INT32 fdc_status;
static INT32 fdc_track;
static INT32 fdc_sector;
static INT32 fdc_data;
static INT32 fdc_phys_track;
static INT32 fdc_irq;
static INT32 fdc_drq;
static INT32 fdc_span;
static INT32 fdc_index_count;
static UINT32 fdc_pointer;

static INT32 frc_mode;
static INT32 frc_cnt;
static INT32 frc_timer;
static INT32 frc_irq;

static UINT8 mlatch;
static UINT8 bankdata;
static UINT8 hotrod_ctrl_cur;
static INT32 cur_input_line;

static UINT8 system24temp_sys16_io_cnt;
static UINT8 system24temp_sys16_io_dir;

static INT32 extra_cycles[2];
static INT32 prev_resetcontrol;
static INT32 resetcontrol;

static INT32 gground_hack = 0;

static INT32 track_size;

static UINT8 (*system24temp_sys16_io_io_r)(INT32 port);
static void  (*system24temp_sys16_io_io_w)(INT32 port, UINT8 data);

static INT32 gfx_set;

static const UINT8 *mlatch_table;
static UINT8  mahmajn_mlt[8] = { 5, 1, 6, 2, 3, 7, 4, 0 };
static UINT8 mahmajn2_mlt[8] = { 6, 0, 5, 3, 1, 4, 2, 7 };
static UINT8      gqh_mlt[8] = { 3, 7, 4, 0, 2, 6, 5, 1 };
static UINT8 bnzabros_mlt[8] = { 2, 4, 0, 5, 7, 3, 1, 6 };
static UINT8   qrouka_mlt[8] = { 1, 6, 4, 7, 0, 5, 3, 2 };
static UINT8 quizmeku_mlt[8] = { 0, 3, 2, 4, 6, 1, 7, 5 };
static UINT8   dcclub_mlt[8] = { 4, 7, 3, 0, 2, 6, 5, 1 };

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[10];
static UINT8 DrvReset;

static INT16 AnalogWheel[4] = { 0, 0, 0, 0 };
static INT16 AnalogPedal[4] = { 0, 0, 0, 0 };

static INT32 uses_tball = 0; // hotrod, roughrac, dcclub analog wheels

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy4 + 2,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnInputInfo GgroundInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p3 coin"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 5,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 4,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 6,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy4 + 2,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Gground)

static struct BurnInputInfo GgroundjInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy4 + 2,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ggroundj)

static struct BurnInputInfo RoughracInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 start"	},
	A("P1 Wheel",       BIT_ANALOG_REL, &AnalogWheel[0],"p1 x-axis" ),
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 start"	},
	A("P2 Wheel",       BIT_ANALOG_REL, &AnalogWheel[1],"p2 x-axis" ),
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy4 + 2,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Roughrac)

static struct BurnInputInfo SgmastInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sgmast)

static struct BurnInputInfo SgmastjInputList[] = {  // and dcclub!
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 4"	},
	A("P1 Wheel",       BIT_ANALOG_REL, &AnalogWheel[0],"p1 x-axis" ),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sgmastj)

static struct BurnInputInfo HotrodInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	A("P1 Wheel",       BIT_ANALOG_REL, &AnalogWheel[0], "p1 x-axis"),
	A("P1 Accelerator", BIT_ANALOG_REL, &AnalogPedal[0], "p1 fire 1"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	A("P2 Wheel",       BIT_ANALOG_REL, &AnalogWheel[1], "p2 x-axis"),
	A("P2 Accelerator", BIT_ANALOG_REL, &AnalogPedal[1], "p2 fire 1"),

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p3 coin"	},
	A("P3 Wheel",       BIT_ANALOG_REL, &AnalogWheel[2], "p3 x-axis"),
	A("P3 Accelerator", BIT_ANALOG_REL, &AnalogPedal[2], "p3 fire 1"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy4 + 2,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Hotrod)

static struct BurnInputInfo HotrodjInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	A("P1 Wheel",       BIT_ANALOG_REL, &AnalogWheel[0], "p1 x-axis"),
	A("P1 Accelerator", BIT_ANALOG_REL, &AnalogPedal[0], "p1 fire 1"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	A("P2 Wheel",       BIT_ANALOG_REL, &AnalogWheel[1], "p2 x-axis"),
	A("P2 Accelerator", BIT_ANALOG_REL, &AnalogPedal[1], "p2 fire 1"),

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p3 coin"	},
	A("P3 Wheel",       BIT_ANALOG_REL, &AnalogWheel[2], "p3 x-axis"),
	A("P3 Accelerator", BIT_ANALOG_REL, &AnalogPedal[2], "p3 fire 1"),

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p4 coin"	},
	A("P4 Wheel",       BIT_ANALOG_REL, &AnalogWheel[3], "p4 x-axis"),
	A("P4 Accelerator", BIT_ANALOG_REL, &AnalogPedal[3], "p4 fire 1"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"service"	},
	{"Service 4",		BIT_DIGITAL,	DrvJoy2 + 6,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy4 + 2,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Hotrodj)

static struct BurnInputInfo QroukaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 5,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 4,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 6,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p3 fire 1"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy3 + 1,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy3 + 0,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy3 + 3,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy3 + 2,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p4 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 2,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Qrouka)

static struct BurnInputInfo QuizmekuInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy4 + 6,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 5,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 4,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 6,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p3 fire 1"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy4 + 7,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy3 + 1,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy3 + 0,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy3 + 3,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy3 + 2,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p4 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 2,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Quizmeku)

#define DEFAULT_COIN_DIPS(_dip)										\
	{_dip, 0xff, 0xff, 0xff, NULL								},	\
	\
	{0   , 0xfe, 0   ,   16, "Coin A"							},	\
	{_dip, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"				},	\
	{_dip, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"				},	\
	{_dip, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"				},	\
	{_dip, 0x01, 0x0f, 0x05, "2 Coins/1 Credit, 5/3, 6/4"		},	\
	{_dip, 0x01, 0x0f, 0x04, "2 Coins/1 Credit, 4/3"			},	\
	{_dip, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"				},	\
	{_dip, 0x01, 0x0f, 0x03, "1 Coin/1 Credit, 5/6"				},	\
	{_dip, 0x01, 0x0f, 0x02, "1 Coin/1 Credit, 4/5"				},	\
	{_dip, 0x01, 0x0f, 0x01, "1 Coin/1 Credit, 2/3"				},	\
	{_dip, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"				},	\
	{_dip, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"				},	\
	{_dip, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"				},	\
	{_dip, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"				},	\
	{_dip, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"				},	\
	{_dip, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"				},	\
	{_dip, 0x01, 0x0f, 0x00, "Free Play (if Coin B too) or 1/1"	},	\
	\
	{0   , 0xfe, 0   ,    16, "Coin B"							},	\
	{_dip, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"				},	\
	{_dip, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"				},	\
	{_dip, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"				},	\
	{_dip, 0x01, 0xf0, 0x50, "2 Coins/1 Credit, 5/3, 6/4"		},	\
	{_dip, 0x01, 0xf0, 0x40, "2 Coins/1 Credit, 4/3"			},	\
	{_dip, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"				},	\
	{_dip, 0x01, 0xf0, 0x30, "1 Coin/1 Credit, 5/6"				},	\
	{_dip, 0x01, 0xf0, 0x20, "1 Coin/1 Credit, 4/5"				},	\
	{_dip, 0x01, 0xf0, 0x10, "1 Coin/1 Credit, 2/3"				},	\
	{_dip, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"				},	\
	{_dip, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"				},	\
	{_dip, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"				},	\
	{_dip, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"				},	\
	{_dip, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"				},	\
	{_dip, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"				},	\
	{_dip, 0x01, 0xf0, 0x00, "Free Play (if Coin A too) or 1/1"	},


static struct BurnDIPInfo GgroundDIPList[]=
{
	{0x1a, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xfd, NULL					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x01, 0x01, 0x1c, 0x00, "Easiest"				},
	{0x01, 0x01, 0x1c, 0x10, "Easier"				},
	{0x01, 0x01, 0x1c, 0x08, "Easy"					},
	{0x01, 0x01, 0x1c, 0x18, "Little Easy"			},
	{0x01, 0x01, 0x1c, 0x1c, "Normal"				},
	{0x01, 0x01, 0x1c, 0x0c, "Little Hard"			},
	{0x01, 0x01, 0x1c, 0x14, "Hard"					},
	{0x01, 0x01, 0x1c, 0x04, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Time Limit Per Stage"	},
	{0x01, 0x01, 0x60, 0x20, "Easiest"				},
	{0x01, 0x01, 0x60, 0x40, "Easy"					},
	{0x01, 0x01, 0x60, 0x60, "Normal"				},
	{0x01, 0x01, 0x60, 0x00, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Clock Of Time Limit"	},
	{0x01, 0x01, 0x80, 0x80, "1.00 sec"				},
	{0x01, 0x01, 0x80, 0x00, "0.80 sec"				},
};

STDDIPINFO(Gground)

static struct BurnDIPInfo GgroundjDIPList[]=
{
	{0x15, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xfd, NULL					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x01, 0x01, 0x1c, 0x00, "Easiest"				},
	{0x01, 0x01, 0x1c, 0x10, "Easier"				},
	{0x01, 0x01, 0x1c, 0x08, "Easy"					},
	{0x01, 0x01, 0x1c, 0x18, "Little Easy"			},
	{0x01, 0x01, 0x1c, 0x1c, "Normal"				},
	{0x01, 0x01, 0x1c, 0x0c, "Little Hard"			},
	{0x01, 0x01, 0x1c, 0x14, "Hard"					},
	{0x01, 0x01, 0x1c, 0x04, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Time Limit Per Stage"	},
	{0x01, 0x01, 0x60, 0x20, "Easiest"				},
	{0x01, 0x01, 0x60, 0x40, "Easy"					},
	{0x01, 0x01, 0x60, 0x60, "Normal"				},
	{0x01, 0x01, 0x60, 0x00, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Clock Of Time Limit"	},
	{0x01, 0x01, 0x80, 0x80, "1.00 sec"				},
	{0x01, 0x01, 0x80, 0x00, "0.80 sec"				},
};

STDDIPINFO(Ggroundj)

static struct BurnDIPInfo RoughracDIPList[]=
{
	{0x19, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xfd, NULL					},

	{0   , 0xfe, 0   ,    2, "Start Credit"			},
	{0x01, 0x01, 0x01, 0x01, "1"					},
	{0x01, 0x01, 0x01, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x04, 0x04, "Off"					},
	{0x01, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Game Instruction"		},
	{0x01, 0x01, 0x08, 0x00, "Off"					},
	{0x01, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x30, 0x20, "Easy"					},
	{0x01, 0x01, 0x30, 0x30, "Normal"				},
	{0x01, 0x01, 0x30, 0x10, "Hard"					},
	{0x01, 0x01, 0x30, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Start Money"			},
	{0x01, 0x01, 0x40, 0x40, "15000"				},
	{0x01, 0x01, 0x40, 0x00, "20000"				},

	{0   , 0xfe, 0   ,    2, "Start Intro"			},
	{0x01, 0x01, 0x80, 0x80, "10"					},
	{0x01, 0x01, 0x80, 0x00, "15"					},
};

STDDIPINFO(Roughrac)

static struct BurnDIPInfo HotrodDIPList[]=
{
	{0x0e, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Start Credit"			},
	{0x01, 0x01, 0x20, 0x20, "1"					},
	{0x01, 0x01, 0x20, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Coin Chute"			},
	{0x01, 0x01, 0x40, 0x40, "Separate"				},
	{0x01, 0x01, 0x40, 0x00, "Common"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Hotrod)

static struct BurnDIPInfo HotrodjDIPList[]=
{
	{0x12, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Start Credit"			},
	{0x01, 0x01, 0x04, 0x04, "1"					},
	{0x01, 0x01, 0x04, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Play Mode"			},
	{0x01, 0x01, 0x08, 0x08, "4 Player"				},
	{0x01, 0x01, 0x08, 0x00, "2 Player"				},

	{0   , 0xfe, 0   ,    2, "Game Style"			},
	{0x01, 0x01, 0x10, 0x10, "4 Sides"				},
	{0x01, 0x01, 0x10, 0x00, "2 Sides"				},

	{0   , 0xfe, 0   ,    2, "Language"				},
	{0x01, 0x01, 0x20, 0x20, "Japanese"				},
	{0x01, 0x01, 0x20, 0x00, "English"				},
};

STDDIPINFO(Hotrodj)

static struct BurnDIPInfo QghDIPList[]=
{
	{0x17, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xfd, NULL					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x0c, 0x08, "Easy"					},
	{0x01, 0x01, 0x0c, 0x0c, "Normal"				},
	{0x01, 0x01, 0x0c, 0x04, "Hard"					},
	{0x01, 0x01, 0x0c, 0x00, "Hard"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x30, 0x10, "3"					},
	{0x01, 0x01, 0x30, 0x00, "3"					},
	{0x01, 0x01, 0x30, 0x20, "4"					},
	{0x01, 0x01, 0x30, 0x30, "5"					},
};

STDDIPINFO(Qgh)

static struct BurnDIPInfo SspiritsDIPList[]=
{
	{0x17, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xfd, NULL					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x0c, 0x00, "2"					},
	{0x01, 0x01, 0x0c, 0x0c, "3"					},
	{0x01, 0x01, 0x0c, 0x08, "4"					},
	{0x01, 0x01, 0x0c, 0x04, "5"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x30, 0x20, "500K"					},
	{0x01, 0x01, 0x30, 0x30, "600K"					},
	{0x01, 0x01, 0x30, 0x10, "800K"					},
	{0x01, 0x01, 0x30, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0xc0, 0x80, "Easy"					},
	{0x01, 0x01, 0xc0, 0xc0, "Normal"				},
	{0x01, 0x01, 0xc0, 0x40, "Hard"					},
	{0x01, 0x01, 0xc0, 0x00, "Hardest"				},
};

STDDIPINFO(Sspirits)

static struct BurnDIPInfo BnzabrosDIPList[]=
{
	{0x17, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Start Credit"			},
	{0x01, 0x01, 0x02, 0x02, "1"					},
	{0x01, 0x01, 0x02, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Coin Chute"			},
	{0x01, 0x01, 0x04, 0x04, "Common"				},
	{0x01, 0x01, 0x04, 0x00, "Individual"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x08, 0x08, "Off"					},
	{0x01, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x30, 0x20, "Easy"					},
	{0x01, 0x01, 0x30, 0x30, "Normal"				},
	{0x01, 0x01, 0x30, 0x10, "Hard"					},
	{0x01, 0x01, 0x30, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0xc0, 0x00, "1"					},
	{0x01, 0x01, 0xc0, 0x40, "2"					},
	{0x01, 0x01, 0xc0, 0xc0, "3"					},
	{0x01, 0x01, 0xc0, 0x80, "4"					},
};

STDDIPINFO(Bnzabros)

static struct BurnDIPInfo CrkdownDIPList[]=
{
	{0x17, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xfF, NULL					},

	{0   , 0xfe, 0   ,    2, "Coin Chute"			},
	{0x01, 0x01, 0x40, 0x40, "Common"				},
	{0x01, 0x01, 0x40, 0x00, "Individual"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Crkdown)

static struct BurnDIPInfo QswwDIPList[]=
{
	{0x17, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xfd, NULL					},

	{0   , 0xfe, 0   ,    2, "Start Credit"			},
	{0x01, 0x01, 0x01, 0x01, "1"					},
	{0x01, 0x01, 0x01, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x04, 0x04, "Off"					},
	{0x01, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x30, 0x10, "2"					},
	{0x01, 0x01, 0x30, 0x00, "3"					},
	{0x01, 0x01, 0x30, 0x30, "4"					},
	{0x01, 0x01, 0x30, 0x20, "5"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0xc0, 0x80, "Easy"					},
	{0x01, 0x01, 0xc0, 0xc0, "Normal"				},
	{0x01, 0x01, 0xc0, 0x40, "Hard"					},
	{0x01, 0x01, 0xc0, 0x00, "Hardest"				},
};

STDDIPINFO(Qsww)

static struct BurnDIPInfo SgmastDIPList[]=
{
	{0x16, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xed, NULL					},

	{0   , 0xfe, 0   ,    2, "Start Credit"			},
	{0x01, 0x01, 0x01, 0x01, "1"					},
	{0x01, 0x01, 0x01, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x04, 0x04, "Off"					},
	{0x01, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Gameplay"				},
	{0x01, 0x01, 0x08, 0x08, "1 Hole 1 Credit"		},
	{0x01, 0x01, 0x08, 0x00, "Lose Ball Game Over"	},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x01, 0x01, 0xe0, 0x80, "Easiest"				},
	{0x01, 0x01, 0xe0, 0xa0, "Easier"				},
	{0x01, 0x01, 0xe0, 0xc0, "Easy"					},
	{0x01, 0x01, 0xe0, 0xe0, "Normal"				},
	{0x01, 0x01, 0xe0, 0x60, "Little Hard"			},
	{0x01, 0x01, 0xe0, 0x40, "Hard"					},
	{0x01, 0x01, 0xe0, 0x20, "Harder"				},
	{0x01, 0x01, 0xe0, 0x00, "Hardest"				},
};

STDDIPINFO(Sgmast)

static struct BurnDIPInfo SgmastjDIPList[]=
{
	{0x17, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xed, NULL					},

	{0   , 0xfe, 0   ,    2, "Start Credit"			},
	{0x01, 0x01, 0x01, 0x01, "1"					},
	{0x01, 0x01, 0x01, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x04, 0x04, "Off"					},
	{0x01, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x01, 0x01, 0xe0, 0x80, "Easiest"				},
	{0x01, 0x01, 0xe0, 0xa0, "Easier"				},
	{0x01, 0x01, 0xe0, 0xc0, "Easy"					},
	{0x01, 0x01, 0xe0, 0xe0, "Normal"				},
	{0x01, 0x01, 0xe0, 0x60, "Little Hard"			},
	{0x01, 0x01, 0xe0, 0x40, "Hard"					},
	{0x01, 0x01, 0xe0, 0x20, "Harder"				},
	{0x01, 0x01, 0xe0, 0x00, "Hardest"				},
};

STDDIPINFO(Sgmastj)

static struct BurnDIPInfo DcclubDIPList[]=
{
	{0x17, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xfb, NULL					},

	{0   , 0xfe, 0   ,    2, "Start Credit"			},
	{0x01, 0x01, 0x01, 0x01, "1"					},
	{0x01, 0x01, 0x01, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x04, 0x04, "Off"					},
	{0x01, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Timing Meter"			},
	{0x01, 0x01, 0x08, 0x08, "Normal"				},
	{0x01, 0x01, 0x08, 0x00, "Easy"					},

	{0   , 0xfe, 0   ,    2, "Initial Balls"		},
	{0x01, 0x01, 0x10, 0x00, "1"					},
	{0x01, 0x01, 0x10, 0x10, "2"					},

	{0   , 0xfe, 0   ,    2, "Balls Limit"			},
	{0x01, 0x01, 0x20, 0x00, "3"					},
	{0x01, 0x01, 0x20, 0x20, "4"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0xc0, 0x80, "Easy"					},
	{0x01, 0x01, 0xc0, 0xc0, "Normal"				},
	{0x01, 0x01, 0xc0, 0x40, "Hard"					},
	{0x01, 0x01, 0xc0, 0x00, "Hardest"				},
};

STDDIPINFO(Dcclub)

static struct BurnDIPInfo QuizmekuDIPList[]=
{
	{0x1f, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xf0, NULL					},

	{0   , 0xfe, 0   ,    2, "Play Mode"			},
	{0x01, 0x01, 0x01, 0x01, "2 Player"				},
	{0x01, 0x01, 0x01, 0x00, "4 Player"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x0c, 0x08, "2"					},
	{0x01, 0x01, 0x0c, 0x0c, "3"					},
	{0x01, 0x01, 0x0c, 0x04, "4"					},
	{0x01, 0x01, 0x0c, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "Answer Counts"		},
	{0x01, 0x01, 0x10, 0x10, "Every"				},
	{0x01, 0x01, 0x10, 0x00, "3"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x01, 0x01, 0x20, 0x20, "Off"					},
	{0x01, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0xc0, 0x80, "Easy"					},
	{0x01, 0x01, 0xc0, 0xc0, "Normal"				},
	{0x01, 0x01, 0xc0, 0x40, "Hard"					},
	{0x01, 0x01, 0xc0, 0x00, "Hardest"				},
};

STDDIPINFO(Quizmeku)

static struct BurnDIPInfo QroukaDIPList[]=
{
	{0x1f, 0xf0, 0xff, 0xff, NULL /* dip offset */  },

	DEFAULT_COIN_DIPS(0x00)

	{0x01, 0xff, 0xff, 0xf0, NULL					},

	{0   , 0xfe, 0   ,    2, "Play Mode"			},
	{0x01, 0x01, 0x01, 0x01, "2 Player"				},
	{0x01, 0x01, 0x01, 0x00, "4 Player"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x30, 0x10, "2"					},
	{0x01, 0x01, 0x30, 0x00, "3"					},
	{0x01, 0x01, 0x30, 0x30, "4"					},
	{0x01, 0x01, 0x30, 0x20, "5"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0xc0, 0x80, "Easy"					},
	{0x01, 0x01, 0xc0, 0xc0, "Normal"				},
	{0x01, 0x01, 0xc0, 0x40, "Hard"					},
	{0x01, 0x01, 0xc0, 0x00, "Hardest"				},
};

STDDIPINFO(Qrouka)

enum {
	IRQ_YM2151 = 1,
	IRQ_TIMER  = 2,
	IRQ_VBLANK = 3,
	IRQ_SPRITE = 4,
	IRQ_FRC    = 5
};

static void set_irq(INT32 cpu, INT32 line, INT32 state)
{
	SekSetVIRQLine(cpu, line, state);
	//bprintf(0, _T("frame: %05X  cpu %X:  %X   %X   (allow %02X %02X)   @cyc %d\n"), nCurrentFrame, cpu, line, state, irq_allow0, irq_allow1, SekTotalCycles());
}

static void irq_timer_start(INT32 old_tmode);

static void irq_timer_cb()
{
	irq_timer_start(irq_tmode);

	if (irq_allow0 & (1 << IRQ_TIMER)) set_irq(0, IRQ_TIMER+1, CPU_IRQSTATUS_ACK);
	if (irq_allow1 & (1 << IRQ_TIMER)) set_irq(1, IRQ_TIMER+1, CPU_IRQSTATUS_ACK);
}

static void irq_timer_start(INT32 old_tmode)
{
	switch (irq_tmode)
	{
		case 0:
			if (old_tmode && irq_timer != -1)
			{
				irq_timer--;

				if (irq_timer <= 0)	{
					irq_timer_cb();
				}
				irq_timer = -1;
			}
			break;

		case 1:
			{
				irq_timer = 0x1000 - irq_tdata; //~410 cycles/scanline
			}
			break;

		case 3:
			{
				INT32 count = 0x1000 - irq_tdata;
				INT32 val = ((count * 125) / 100) * 406; // 1.25 cycles per count, we're doing timer scanline based.
				irq_timer = val;	// 8000000
			}
			break;
	}
}

static void irq_write(UINT32 offset, UINT16 data)
{
	switch ((offset / 2) & 3)
	{
		case 0:
			irq_tdata = data;
			irq_tdata &= 0xfff;
			irq_timer_start(irq_tmode);
			break;

		case 1:
			{
				UINT8 old_tmode = irq_tmode;
				irq_tmode = data & 3;
				irq_timer_start(old_tmode);
			}
			break;

		case 2:
			irq_allow0 = data & 0x3f;
			set_irq(0, IRQ_TIMER+1, CLEAR_LINE);
			set_irq(0, IRQ_YM2151+1, irq_yms && (irq_allow0 & (1 << IRQ_YM2151)) ? ASSERT_LINE : CLEAR_LINE);
			set_irq(0, IRQ_VBLANK+1, irq_vblank && (irq_allow0 & (1 << IRQ_VBLANK)) ? ASSERT_LINE : CLEAR_LINE);
			set_irq(0, IRQ_SPRITE+1, irq_sprite && (irq_allow0 & (1 << IRQ_SPRITE)) ? ASSERT_LINE : CLEAR_LINE);
			break;

		case 3:
			irq_allow1 = data & 0x3f;
			set_irq(1, IRQ_TIMER+1, CLEAR_LINE);
			set_irq(1, IRQ_YM2151+1, irq_yms && (irq_allow1 & (1 << IRQ_YM2151)) ? ASSERT_LINE : CLEAR_LINE);
			set_irq(1, IRQ_VBLANK+1, irq_vblank && (irq_allow1 & (1 << IRQ_VBLANK)) ? ASSERT_LINE : CLEAR_LINE);
			set_irq(1, IRQ_SPRITE+1, irq_sprite && (irq_allow1 & (1 << IRQ_SPRITE)) ? ASSERT_LINE : CLEAR_LINE);
			break;
	}
}

static UINT16 irq_read(UINT32 offset)
{
	offset = (offset / 2) & 3;

	switch (offset)
	{
		case 2:
			set_irq(0, IRQ_TIMER+1, CLEAR_LINE);
			break;

		case 3:
			set_irq(1, IRQ_TIMER+1, CLEAR_LINE);
			break;
	}
	gground_hack++;
	return (irq_tdata + gground_hack) & 0xfff; // iq_132 - gground hack!!     git 'r dun! -dink
}

static void irq_vbl_clear()
{
	set_irq(0, IRQ_VBLANK+1, CLEAR_LINE);
	set_irq(0, IRQ_SPRITE+1, CLEAR_LINE);
	set_irq(1, IRQ_VBLANK+1, CLEAR_LINE);
	set_irq(1, IRQ_SPRITE+1, CLEAR_LINE);
	irq_sprite = irq_vblank = 0;
}

static void irq_vbl(INT32 scanline)
{
	INT32 irq = 0;

	if (scanline == 0) { irq = IRQ_SPRITE; irq_sprite = 1; }
	else if (scanline == 384) { irq = IRQ_VBLANK; irq_vblank = 1; }
	else return;

	INT32 mask = 1 << irq;

	if (irq_allow0 & mask) set_irq(0, irq+1, ASSERT_LINE);
	if (irq_allow1 & mask) set_irq(1, irq+1, ASSERT_LINE);
}

static void irq_ym(INT32 state)
{
	irq_yms = state;

	set_irq(0, IRQ_YM2151+1, irq_yms && (irq_allow0 & (1 << IRQ_YM2151)) ? ASSERT_LINE : CLEAR_LINE);
	set_irq(1, IRQ_YM2151+1, irq_yms && (irq_allow1 & (1 << IRQ_YM2151)) ? ASSERT_LINE : CLEAR_LINE);
}

static void irq_frc_cb()
{
	frc_cnt++;
	frc_irq = 0;

	if (irq_allow0 & (1 << IRQ_FRC) && frc_mode == 1) {
		set_irq(0, IRQ_FRC+1, CPU_IRQSTATUS_ACK);
		frc_irq = 1;
	}

	if (irq_allow1 & (1 << IRQ_FRC) && frc_mode == 1) {
		set_irq(1, IRQ_FRC+1, CPU_IRQSTATUS_ACK);
		frc_irq = 1;
	}
}

static void mlatch_write(UINT16 data)
{
	UINT8 mxor = 0;

	if (!mlatch_table) {
		return;
	}

	data &= 0xff;

	if (data != 0xff)
	{
		for (INT32 i = 0; i < 8; i++)
			if (mlatch & (1 << i))
				mxor |= 1 << mlatch_table[i];
		mlatch = data ^ mxor;
	} else {
		mlatch = 0x00;
	}
}

static UINT16 fdc_read(UINT32 offset)
{
	if (!track_size)
		return 0xffff;

	switch (offset & 6)
	{
		case 0:
			fdc_irq = 0;
			return fdc_status;
		case 2:
			return fdc_track;
		case 4:
			return fdc_sector;
		case 6:
			{
				INT32 res = fdc_data;
				if (fdc_drq) {
					fdc_span--;
					if (fdc_span) {
						fdc_pointer++;
						fdc_data = DrvFloppyData[fdc_pointer];
					} else {
						fdc_drq = 0;
						fdc_status = 0;
						fdc_irq = 1;
					}
				}
				return res;
			}
	}

	return 0;
}

static void fdc_write(UINT32 offset, UINT16 data)
{
	if (!track_size)
		return;

	data &= 0xff;

	switch (offset & 6)
	{
		case 0:
			fdc_irq = 0;
			switch (data >> 4)
			{
				case 0x0:
					fdc_phys_track = fdc_track = 0;
					fdc_irq = 1;
					fdc_status = 4;
					break;
				case 0x1:
					fdc_phys_track = fdc_track = fdc_data;
					fdc_irq = 1;
					fdc_status = fdc_track ? 0 : 4;
					break;
				case 0x9:
					fdc_pointer = track_size*(2*fdc_phys_track+(data & 8 ? 1 : 0));
					fdc_span = track_size;
					fdc_status = 3;
					fdc_drq = 1;
					fdc_data = DrvFloppyData[fdc_pointer];
					break;
				case 0xb:
					fdc_pointer = track_size*(2*fdc_phys_track+(data & 8 ? 1 : 0));
					fdc_span = track_size;
					fdc_status = 3;
					fdc_drq = 1;
					break;
				case 0xd:
					fdc_span = 0;
					fdc_drq = 0;
					fdc_irq = data & 1;
					fdc_status = 0;
					break;
			}
			break;

		case 2:
			fdc_track = data;
			break;

		case 4:
			fdc_sector = data;
			break;

		case 6:
			if (fdc_drq)
			{
				DrvFloppyData[fdc_pointer] = data;
				fdc_pointer++;
				fdc_span--;
				if (!fdc_span) {
					fdc_drq = 0;
					fdc_status = 0;
					fdc_irq = 1;
				}
			}
			fdc_data = data;
			break;
	}
}

static UINT16 fdc_status_read(UINT32 /*offset*/)
{
	if (!track_size)
		return 0xffff;

	return 0x90 | (fdc_irq ? 2 : 0) | (fdc_drq ? 1 : 0) | (fdc_phys_track ? 0x40 : 0) | ((fdc_index_count%20) ? 0x20 : 0);
}

static void reset_reset()
{
	INT32 changed = resetcontrol ^ prev_resetcontrol;
	if (changed & 2) {
		INT32 active = SekGetActive();
		if (resetcontrol & 2) {
			if (active != -1) {
				SekClose();
			}

			SekSetHALT(1, 0);
			s24_fd1094_machine_init();
			SekReset(1);

			if (active != -1) {
				SekOpen(active);
			}
		}
		else
		{
			SekSetHALT(1, 1);
		}
	}

	if (changed & 4) {
		BurnYM2151Reset();
	}
	prev_resetcontrol = resetcontrol;
}

static void resetcontrol_write(UINT8 data)
{
	resetcontrol = data;
	reset_reset();
}

static UINT8 dcclub_io_read(INT32 port)
{
	switch(port) {
		case 0: {
			static UINT8 pos[16] = { 0, 1, 3, 2, 6, 4, 12, 8, 9, 0, 0, 0, 0, 0, 0, 0 };
			return (DrvInputs[0] & 0xf) | ((~pos[BurnTrackballRead(0, 0) >> 4] << 4) & 0xf0);
		}
		case 1:
			return DrvInputs[1];
		case 2: {
			static UINT8 pos[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 2, 0, 0, 0, 0 };
			return ((~pos[BurnTrackballRead(0, 0)>>4] & 0x3) | 0xfc);
		}
		case 3:
			return 0xff;
		case 4:
			return DrvInputs[3];
		case 5: // Dip switches
			return DrvDips[0];
		case 6:
			return DrvDips[1];
		case 7: // DAC
			return 0xff;
	}
	return 0x00;
}

static UINT8 mahmajn_io_read(INT32 port)
{
	switch(port) {
		case 0:
			return ~(1 << cur_input_line);
		case 1:
			return 0xff;
		case 2:
			return DrvInputs[cur_input_line];
		case 3:
			return 0xff;
		case 4:
			return DrvInputs[8];
		case 5: // Dip switches
			return DrvDips[0];
		case 6:
			return DrvDips[1];
		case 7: // DAC
			return 0xff;
	}
	return 0x00;
}

static UINT8 gground_io_read(INT32 port)
{
	switch(port) {
		case 0:
			return DrvInputs[0];
		case 1:
			return DrvInputs[1];
		case 2: // P3 inputs
			return DrvInputs[2];
		case 3:
			return 0xff;
		case 4:
			return DrvInputs[3];
		case 5: // Dip switches
			return DrvDips[0];
		case 6:
			return DrvDips[1];
		case 7: // DAC
			return 0xff;
	}
	return 0x00;
}

static UINT8 hotrod_io_read(INT32 port)
{
	switch(port) {
		case 0:
			return DrvInputs[0];
		case 1:
			return DrvInputs[1];
		case 2:
			return 0xff;
		case 3:
			return 0xff;
		case 4:
			return DrvInputs[3];
		case 5: // Dip switches
			return DrvDips[0];
		case 6:
			return DrvDips[1];
		case 7: // DAC
			return 0xff;
	}
	return 0x00;
}

static void hotrod_io_write(INT32 port, UINT8 data)
{
	switch (port) {
		case 3: // Lamps
			break;

		case 7: // DAC
			DACSignedWrite(0, data);
			break;
	}
}

static void mahmajn_io_write(INT32 port, UINT8 data)
{
	switch (port)
	{
		case 3:
			if (data & 4)
				cur_input_line = (cur_input_line + 1) & 7;
			break;
		case 7: // DAC
			DACSignedWrite(0, data);
			break;
	}
}

static void hotrod3_ctrl_write(UINT32,UINT16 data)
{
	hotrod_ctrl_cur = ProcessAnalog(AnalogPedal[data&3], 0, INPUT_DEADZONE | INPUT_MIGHTBEDIGITAL | INPUT_LINEAR, 0x01, 0xff);
}

static UINT16 hotrod3_ctrl_read(UINT32 offset)
{
	INT32 idx = (offset / 2) & 0xf;

	switch (idx)
	{
		case 0:
		case 2:
		case 4:
		case 6:
			return BurnTrackballReadWord(idx>>2, (idx>>1) & 1) & 0xff;

		case 1:
		case 3:
		case 5:
		case 7:
			return (BurnTrackballReadWord(idx>>2, (idx>>1) & 1) & 0xf00) >> 8;

		case 8: { // Serial ADCs for the accel
			INT32 v = hotrod_ctrl_cur & 0x80;
			hotrod_ctrl_cur <<= 1;
			return v ? 0xff : 0;
		}
	}
	return 0;
}

static void system24temp_sys16_io_set_callbacks(UINT8 (*io_r)(INT32 port), void  (*io_w)(INT32 port, UINT8 data))
{
	system24temp_sys16_io_io_r = io_r;
	system24temp_sys16_io_io_w = io_w;
	system24temp_sys16_io_cnt = 0x00;
	system24temp_sys16_io_dir = 0x00;
}

static UINT16 system24temp_sys16_io_read(UINT32 offset)
{
	offset = (offset & 0x7e) / 2;

	if (offset < 8)	{
		return system24temp_sys16_io_io_r ? system24temp_sys16_io_io_r(offset) : 0xff;
	}
	else if (offset < 0x20) {
		switch (offset) {
			case 0x8:
				return 'S';
			case 0x9:
				return 'E';
			case 0xa:
				return 'G';
			case 0xb:
				return 'A';
			case 0xe:
				return system24temp_sys16_io_cnt;
			case 0xf:
				return system24temp_sys16_io_dir;
			default:
				return 0xff;
		}
	} else
		return 0xffff;
}

static void system24temp_sys16_io_write(UINT32 offset, UINT16 data)
{
	offset = (offset & 0x7e) / 2;

	if (offset < 8) {
		if (!(system24temp_sys16_io_dir & (1 << offset))) {
			return;
		}
		if (system24temp_sys16_io_io_w)
			system24temp_sys16_io_io_w(offset, data);
	} else if (offset < 0x20) {
		switch (offset) {
			case 0xe:
				system24temp_sys16_io_cnt = data;
				resetcontrol_write(data & 7);
				break;
			case 0xf:
				system24temp_sys16_io_dir = data;
				break;
			default: break;
		}
	}
}


static void bankswitch() // change banks on both cpus
{
	INT32 active = SekGetActive();
	INT32 bank = bankdata & 0x0f;

	SekMapMemory(DrvUserROM + (bank * 0x40000),		0xb80000, 0xbbffff, MAP_ROM);
	SekMapMemory(DrvUserROM + (bank * 0x40000),		0xc80000, 0xcbffff, MAP_ROM);

	SekClose();
	SekOpen(active^1);

	SekMapMemory(DrvUserROM + (bank * 0x40000),		0xb80000, 0xbbffff, MAP_ROM);
	SekMapMemory(DrvUserROM + (bank * 0x40000),		0xc80000, 0xcbffff, MAP_ROM);

	SekClose();
	SekOpen(active);
}

static void __fastcall sys24_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff80) == 0x800000) {
		system24temp_sys16_io_write(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0xa00000) {
		irq_write(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0xb00000) {
		fdc_write(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0xb00008) {
		// fdc control writes (not used)
		return;
	}

	if (address >= 0xc00000 && address <= 0xc00011) {
		hotrod3_ctrl_write(address,data);
		return;
	}

	switch (address)
	{
		case 0x220000:
			return; // ?

		case 0x240000: // h-sync regs
		case 0x260000: // v-sync regs
		case 0x270000: // video sync switch
			return;

		case 0x800100:
		case 0x800102:
			BurnYM2151Write((address / 2) & 1, data);
			return;

		case 0xbc0000:
		case 0xcc0000:
			bankdata = data & 0xff;
			bankswitch();
			return;

		case 0xbc0002:
		case 0xcc0002:
			frc_cnt = 0;
			frc_mode = data & 1;
			frc_timer = 4;
			return;

		case 0xbc0004:
		case 0xcc0004:
			frc_cnt = data;
			set_irq(0, IRQ_FRC+1, CLEAR_LINE);
			set_irq(1, IRQ_FRC+1, CLEAR_LINE);
			return;

		case 0xbc0006:
		case 0xcc0006:
			mlatch_write(data);
			return;
	}

	bprintf (0, _T("MISS! WW: %5.5x, %4.4x (%d)\n"), address, data, SekGetActive());
}

static void __fastcall sys24_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffff80) == 0x800000) {
		if (address & 1) system24temp_sys16_io_write(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0xa00000) {
		if (address & 1) irq_write(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0xb00000) {
		fdc_write(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0xb00008) {
		// fdc control writes (not used)
		return;
	}

	if (address >= 0xc00000 && address <= 0xc00011) {
		hotrod3_ctrl_write(address,data);
		return;
	}

	switch (address)
	{
		case 0x220001:
			return; // ?

		case 0x240001: // h-sync regs
		case 0x260001: // v-sync regs
		case 0x270001: // video sync switch
			return;

		case 0x800101:
		case 0x800103:
			BurnYM2151Write((address / 2) & 1, data);
			return;

		case 0xbc0001:
		case 0xcc0001:
			bankdata = data & 0xff;
			bankswitch();
			return;

		case 0xbc0003:
		case 0xcc0003:
			frc_cnt = 0;
			frc_mode = data & 1;
			frc_timer = 4;
			return;

		case 0xbc0005:
		case 0xcc0005:
			frc_cnt = data;
			set_irq(0, IRQ_FRC+1, CLEAR_LINE);
			set_irq(1, IRQ_FRC+1, CLEAR_LINE);
			return;

		case 0xbc0007:
		case 0xcc0007:
			mlatch_write(data);
			return;

		case 0xd00035:
		case 0xd00300:
		case 0xd00301:
		case 0xd00307:
			return; // nop
	}

	bprintf (0, _T("MISS! WB: %5.5x, %2.2x (%d)\n"), address, data, SekGetActive());
}

static UINT16 __fastcall sys24_read_word(UINT32 address)
{
	if ((address & 0xffff80) == 0x800000) {
		UINT16 ret = system24temp_sys16_io_read(address);

		return ret | (ret << 8);
	}

	if ((address & 0xfffff8) == 0xa00000) {
		return irq_read(address);
	}

	if ((address & 0xfffff8) == 0xb00000) {
		return fdc_read(address);
	}

	if ((address & 0xfffff8) == 0xb00008) {
		return fdc_status_read(address);
	}

	if (address >= 0xc00000 && address <= 0xc00011) {
		return hotrod3_ctrl_read(address);
	}

	switch (address)
	{
		case 0x800102:
			return BurnYM2151Read();

		case 0xbc0000:
		case 0xcc0000:
			return bankdata;

		case 0xbc0002:
		case 0xcc0002:
			return frc_mode & 1;

		case 0xbc0004:
		case 0xcc0004:
			return frc_cnt % ((frc_mode) ? 0x100 : 0x67);

		case 0xbc0006:
		case 0xcc0006:
			return mlatch;

		case 0xd00300:
		case 0xd00301:
			return 0; // nop
	}

	bprintf (0, _T("MISS! RW: %5.5x (%d)\n"), address, SekGetActive());

	return 0;
}

static UINT8 __fastcall sys24_read_byte(UINT32 address)
{
	if ((address & 0xffff80) == 0x800000) {
		UINT8 ret = system24temp_sys16_io_read(address);
		return ret;
	}

	if ((address & 0xfffff8) == 0xa00000) {
		return irq_read(address);
	}

	if ((address & 0xfffff8) == 0xb00000) {
		return fdc_read(address);
	}

	if ((address & 0xfffff8) == 0xb00008) {
		return fdc_status_read(address);
	}

	if (address >= 0xc00000 && address <= 0xc00011) {
		return hotrod3_ctrl_read(address);
	}

	switch (address)
	{
		case 0x800103:
			return BurnYM2151Read();

		case 0xbc0001:
		case 0xcc0001:
			return bankdata;

		case 0xbc0003:
		case 0xcc0003:
			return frc_mode & 1;

		case 0xbc0005:
		case 0xcc0005:
			return frc_cnt % ((frc_mode) ? 0x100 : 0x67);

		case 0xbc0007:
		case 0xcc0007:
			return mlatch;

		case 0xd00300:
		case 0xd00301:
			return 0; // nop
	}

	bprintf (0, _T("MISS! RB: %5.5x (%d)\n"), address, SekGetActive());

	return 0;
}

static inline void character_update(UINT32 i)
{
	DrvCharExp[i*2+0] = DrvCharRAM[i+1] >> 4;
	DrvCharExp[i*2+1] = DrvCharRAM[i+1] & 0xf;
	DrvCharExp[i*2+2] = DrvCharRAM[i+0] >> 4;
	DrvCharExp[i*2+3] = DrvCharRAM[i+0] & 0xf;
}

static void __fastcall character_write_word(UINT32 address, UINT16 data)
{
	address &= 0x1fffe;
	INT32 old = *((UINT16*)(DrvCharRAM + address));

	if (old != data) {
		*((UINT16*)(DrvCharRAM + address)) = data;
		character_update(address);
	}
}

static void __fastcall character_write_byte(UINT32 address, UINT8 data)
{
	address &= 0x1ffff;
	INT32 old = DrvCharRAM[address^1];

	if (old != data) {
		DrvCharRAM[address^1] = data;
		character_update(address & 0x1fffe);
	}
}

static inline void palette_update_entry(INT32 i)
{
	UINT16 data = *((UINT16*)(DrvPalRAM + i * 2));

	UINT8 r = (data & 0x00f) << 4;
	UINT8 g = (data & 0x0f0);
	UINT8 b = (data & 0xf00) >> 4;

	if (data & 0x1000) r |= 8;
	if (data & 0x2000) g |= 8;
	if (data & 0x4000) b |= 8;

	r |= r >> 5;
	g |= g >> 5;
	b |= b >> 5;

	DrvPalette[i] = BurnHighCol(r,g,b,0);

	if (data & 0x8000) { // highlight
		r = 256 - (((r ^ 255) * 6) / 10);
		g = 256 - (((g ^ 255) * 6) / 10);
		b = 256 - (((b ^ 255) * 6) / 10);
	} else {
		r = (r * 6) / 10;
		g = (r * 6) / 10;
		b = (r * 6) / 10;
	}

	DrvPalette[i + 0x2000] = BurnHighCol(r,g,b,0);
}

static void __fastcall palette_write_word(UINT32 address, UINT16 data)
{
	address &= 0x3ffe;
	INT32 old = *((UINT16*)(DrvPalRAM + address));

	if (old != data)
	{
		*((UINT16*)(DrvPalRAM + address)) = data;
		palette_update_entry(address/2);
	}
}

static void __fastcall palette_write_byte(UINT32 address, UINT8 data)
{
	address &= 0x3fff;
	INT32 old = DrvPalRAM[address^1];

	if (old != data)
	{
		DrvPalRAM[address^1] = data;
		palette_update_entry(address/2);
	}
}

static void fd1094_map_memory(UINT8 *mem)
{
	SekMapMemory(mem+0x00000,		0x000000, 0x03ffff, MAP_FETCH);
	SekMapMemory(mem+0x00000,		0x040000, 0x07ffff, MAP_FETCH); // mirror
}

static tilemap_callback( _0s )
{
	UINT16 attr = *((UINT16*)(DrvTileRAM + 0x0000 + offs * 2));
	INT32 color = attr >> 7;

	TILE_SET_INFO(gfx_set, attr, color, TILE_GROUP(attr >> 15));
}

static tilemap_callback( _0w )
{
	UINT16 attr = *((UINT16*)(DrvTileRAM + 0x2000 + offs * 2));
	INT32 color = attr >> 7;

	TILE_SET_INFO(gfx_set, attr, color, TILE_GROUP(attr >> 15));
}

static tilemap_callback( _1s )
{
	UINT16 attr = *((UINT16*)(DrvTileRAM + 0x4000 + offs * 2));
	INT32 color = attr >> 7;

	TILE_SET_INFO(gfx_set, attr, color, TILE_GROUP(attr >> 15));
}

static tilemap_callback( _1w )
{
	UINT16 attr = *((UINT16*)(DrvTileRAM + 0x6000 + offs * 2));
	INT32 color = attr >> 7;

	TILE_SET_INFO(gfx_set, attr, color, TILE_GROUP(attr >> 15));
}

static INT32 DrvDoReset()
{
	SekOpen(0);
	SekReset();
	bankdata = 0;
	bankswitch();
	SekClose();

	SekOpen(1);
	SekReset();
	SekSetHALT(1); // subcpu starts halted! (it's waiting for prog. to get uploaded)
	DACReset();
	BurnYM2151Reset();
	SekClose();

	irq_allow0 = 0;
	irq_allow1 = 0;
	irq_timer = -1;

	irq_tdata = 0;
	irq_tmode = 0;
	irq_vblank = 0;
	irq_sprite = 0;

	frc_mode = 0;
	frc_cnt = 0;
	frc_timer = -1;
	frc_irq = 0;

	fdc_status = 0;
	fdc_track = 0;
	fdc_sector = 0;
	fdc_data = 0;
	fdc_phys_track = 0;
	fdc_irq = 0;
	fdc_drq = 0;
	fdc_index_count = 0;

	system24temp_sys16_io_cnt = 0;
	system24temp_sys16_io_dir = 0;

	prev_resetcontrol = 0x06;
	resetcontrol = 0x06;
	mlatch = 0;
	hotrod_ctrl_cur = 0;
	cur_input_line = 0;

	extra_cycles[0] = extra_cycles[1] = 0;

	gground_hack = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x040000;

	Drv68KKey		= Next; Next += 0x002000;

	DrvFloppyData	= Next; Next += 0x200000;

	DrvUserROM		= Next; Next += 0x040000 * 0x10; // good enough

	DrvCharExp		= Next; Next += 0x040000;

	AllRam			= Next;

	DrvShareRAM3	= Next; Next += 0x040000;
	DrvShareRAM2	= Next; Next += 0x040000;

	DrvTileRAM		= Next; Next += 0x010000;
	DrvCharRAM		= Next; Next += 0x020000;
	DrvPalRAM		= Next; Next += 0x004000;
	DrvMixerRegs	= Next; Next += 0x000400; // 20
	DrvSprRAM		= Next; Next += 0x040000;

	RamEnd			= Next;

	DrvPalette		= (UINT32*)Next; Next += 0x4000 * 2 * sizeof(UINT32) + 4;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[5] = { NULL, Drv68KROM, DrvUserROM, DrvFloppyData, Drv68KKey };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		INT32 type = ri.nType & 7;

		if (type == 1 || type == 2) {
			if (BurnLoadRom(pLoad[type] + 1, i + 0, 2)) return 1;
			if (BurnLoadRom(pLoad[type] + 0, i + 1, 2)) return 1;
			pLoad[type] += ri.nLen * 2; i++;
			continue;
		}

		if (type == 3 || type == 4) {
			if (BurnLoadRom(pLoad[type], i, 1)) return 1;
			pLoad[type] += ri.nLen;
			continue;
		}

		if (type == 5) { // qrouka fix
			if (BurnLoadRom(pLoad[2] + 1, i + 0, 2)) return 1;
			pLoad[2] += ri.nLen * 2;
			continue;
		}
	}

	if ((pLoad[2] - DrvUserROM) == 0x180000) { // 256k data roms mirrored to 512k (bonanza bros)
		memcpy (pLoad[2], pLoad[2] - 0x80000, 0x080000);
	}

	track_size = (pLoad[3] - DrvFloppyData) / 0xa0;
	Drv68KKey = (pLoad[4] - Drv68KKey) ? Drv68KKey : NULL;

	//	bprintf (0, _T("Tracksize: %x, key: %s\n"), track_size, (Drv68KKey == NULL) ? _T("false") : _T("true"));

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(58.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	DrvLoadRoms();

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM); // share(1)
	SekMapMemory(Drv68KROM,			0x040000, 0x07ffff, MAP_ROM); // mirror
	SekMapMemory(DrvShareRAM2,		0x080000, 0x0bffff, MAP_RAM); // share(2)
	SekMapMemory(DrvShareRAM2,		0x0c0000, 0x0fffff, MAP_RAM); // share(2)
	SekMapMemory(Drv68KROM,			0x100000, 0x13ffff, MAP_ROM);
	SekMapMemory(Drv68KROM,			0x140000, 0x17ffff, MAP_ROM);
	SekMapMemory(Drv68KROM,			0x180000, 0x1bffff, MAP_ROM);
	SekMapMemory(Drv68KROM,			0x1c0000, 0x1fffff, MAP_ROM);
	SekMapMemory(DrvTileRAM,		0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvCharRAM,		0x280000, 0x29ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x400000, 0x403fff, MAP_RAM);
	SekMapMemory(DrvMixerRegs,		0x404000, 0x4043ff, MAP_RAM); // regs 0-1f
	SekMapMemory(DrvSprRAM,			0x600000, 0x63ffff, MAP_RAM);
	//	SekMapMemory(DrvUserROM,		0xb80000, 0xbbffff, MAP_ROM); // banked!
	//	SekMapMemory(DrvUserROM,		0xc80000, 0xcbffff, MAP_ROM); // banked!
	SekMapMemory(DrvShareRAM3,		0xf00000, 0xf3ffff, MAP_RAM);
	SekMapMemory(DrvShareRAM3,		0xf40000, 0xf7ffff, MAP_RAM); // mirror
	SekMapMemory(DrvShareRAM2,		0xf80000, 0xfbffff, MAP_RAM);
	SekMapMemory(DrvShareRAM2,		0xfc0000, 0xffffff, MAP_RAM); // mirror

	SekSetWriteWordHandler(0,		sys24_write_word);
	SekSetWriteByteHandler(0,		sys24_write_byte);
	SekSetReadWordHandler(0,		sys24_read_word);
	SekSetReadByteHandler(0,		sys24_read_byte);

	SekMapHandler(1,				0x280000, 0x29ffff, MAP_WRITE);
	SekSetWriteWordHandler(1,		character_write_word);
	SekSetWriteByteHandler(1,		character_write_byte);

	SekMapHandler(2,				0x400000, 0x403fff, MAP_WRITE);
	SekSetWriteWordHandler(2,		palette_write_word);
	SekSetWriteByteHandler(2,		palette_write_byte);
	SekClose();

	SekInit(1, 0x68000);
	SekOpen(1);
	SekMapMemory(DrvShareRAM3,		0x000000, 0x03ffff, MAP_RAM);
	SekMapMemory(DrvShareRAM3,		0x040000, 0x07ffff, MAP_RAM); // mirror

	// everything below is the same as the master cpu
	SekMapMemory(DrvShareRAM2,		0x080000, 0x0bffff, MAP_RAM); // share(2)
	SekMapMemory(DrvShareRAM2,		0x0c0000, 0x0fffff, MAP_RAM); // share(2)A
	SekMapMemory(Drv68KROM,			0x100000, 0x13ffff, MAP_ROM);
	SekMapMemory(Drv68KROM,			0x140000, 0x17ffff, MAP_ROM);
	SekMapMemory(Drv68KROM,			0x180000, 0x1bffff, MAP_ROM);
	SekMapMemory(Drv68KROM,			0x1c0000, 0x1fffff, MAP_ROM);
	SekMapMemory(DrvTileRAM,		0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvCharRAM,		0x280000, 0x29ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x400000, 0x403fff, MAP_RAM);
	SekMapMemory(DrvMixerRegs,		0x404000, 0x4043ff, MAP_RAM); // regs 0-1f
	SekMapMemory(DrvSprRAM,			0x600000, 0x63ffff, MAP_RAM);
	//	SekMapMemory(DrvUserROM,		0xb80000, 0xbbffff, MAP_ROM); // banked!
	//	SekMapMemory(DrvUserROM,		0xc80000, 0xcbffff, MAP_ROM); // banked!
	SekMapMemory(DrvShareRAM3,		0xf00000, 0xf3ffff, MAP_RAM);
	SekMapMemory(DrvShareRAM3,		0xf40000, 0xf7ffff, MAP_RAM); // mirror
	SekMapMemory(DrvShareRAM2,		0xf80000, 0xfbffff, MAP_RAM);
	SekMapMemory(DrvShareRAM2,		0xfc0000, 0xffffff, MAP_RAM); // mirror
	SekSetWriteWordHandler(0,		sys24_write_word);
	SekSetWriteByteHandler(0,		sys24_write_byte);
	SekSetReadWordHandler(0,		sys24_read_word);
	SekSetReadByteHandler(0,		sys24_read_byte);

	SekMapHandler(1,				0x280000, 0x29ffff, MAP_WRITE);
	SekSetWriteWordHandler(1,		character_write_word);
	SekSetWriteByteHandler(1,		character_write_byte);

	SekMapHandler(2,				0x400000, 0x403fff, MAP_WRITE);
	SekSetWriteWordHandler(2,		palette_write_word);
	SekSetWriteByteHandler(2,		palette_write_byte);
	SekClose();

	s24_fd1094_driver_init(1, 8, Drv68KKey, DrvShareRAM3, 0x40000, fd1094_map_memory);

	BurnYM2151Init(4000000);
	BurnYM2151SetIrqHandler(&irq_ym);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	DACInit(0, 0, 1, SekTotalCycles, 10000000);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, _0s_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, _0w_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, _1s_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, _1w_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvCharExp, 4, 8, 8, 0x40000, 0, 0x0ff);
	GenericTilemapSetGfx(1, DrvCharExp, 4, 8, 8, 0x40000, 0, 0x1ff);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapSetTransparent(3, 0);

	BurnBitmapAllocate(1, 512, 512, true);
	BurnBitmapAllocate(2, 512, 512, true);
	BurnBitmapAllocate(3, 512, 512, true);
	BurnBitmapAllocate(4, 512, 512, true);

	if (uses_tball)	BurnTrackballInit(2);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();
	BurnYM2151Exit();
	DACExit();
	s24_fd1094_exit();

	BurnFree(AllMem);

	mlatch_table = NULL;

	if (uses_tball)	BurnTrackballExit();
	uses_tball = 0;

	return 0;
}

static inline UINT16 sys24_mixer_get_reg(INT32 reg)
{
	return *((UINT16*)(DrvMixerRegs + ((reg & 0xf) * 2)));
}

static INT32 layer_cmp(const void *pl1, const void *pl2)
{
	static const INT32 default_pri[12] = { 0, 1, 2, 3, 4, 5, 6, 7, -4, -3, -2, -1 };
	INT32 l1 = *(INT32 *)pl1;
	INT32 l2 = *(INT32 *)pl2;
	INT32 p1 = sys24_mixer_get_reg(l1) & 7;
	INT32 p2 = sys24_mixer_get_reg(l2) & 7;
	if (p1 != p2)
		return p1 - p2;

	return default_pri[l2] - default_pri[l1];
}

static void draw_sprites(const INT32 *spri)
{
	UINT16 curspr = 0;
	INT32 countspr = 0;
	UINT8 pmt[4];
	UINT16 *sprd[0x2000], *clip[0x2000];
	UINT16 *cclip = NULL;
	UINT16 *sprite_ram = (UINT16*)DrvSprRAM;

	for (INT32 i = 0; i < 4; i++)
		pmt[i] = 0xff << (1+spri[3-i]);

	for (INT32 seen = 0; seen < 0x2000; seen++) {
		UINT16 *source;
		UINT16 type;

		source = sprite_ram + (curspr << 3);

		if (curspr == 0 && source[0] == 0)
			break;

		curspr = source[0];
		type = curspr & 0xc000;
		curspr &= 0x1fff;

		if (type == 0xc000)
			break;

		if (type == 0x8000)
			continue;

		if (type == 0x4000) {
			cclip = source;
			continue;
		}

		sprd[countspr] = source;
		clip[countspr] = cclip;

		countspr++;
		if (!curspr)
			break;
	}

	for (countspr--; countspr >= 0; countspr--) {
		UINT16 *source, *pix;
		INT32 x, y, sx, sy;
		INT32 px, py;
		UINT16 colors[16];
		INT32 flipx, flipy;
		INT32 zoomx, zoomy;
		UINT8 pm[16];
		INT32 xmod, ymod;
		INT32 min_x, min_y, max_x, max_y;

		UINT32 addoffset;
		UINT32 newoffset;
		UINT32 offset;

		source = sprd[countspr];
		cclip = clip[countspr];

		if (cclip) {
			min_y = (cclip[2] & 511);
			min_x = (cclip[3] & 511) - 8;
			max_y = (cclip[4] & 511);
			max_x = (cclip[5] & 511) - 8;
		} else {
			min_x = 0;
			max_x = 495;
			min_y = 0;
			max_y = 383;
		}

		if (min_x < 0)
			min_x = 0;
		if (min_y < 0)
			min_y = 0;
		if (max_x >= nScreenWidth)
			max_x = nScreenWidth-1;
		if (max_y >= nScreenHeight)
			max_y = nScreenHeight-1;

		if (!(source[0] & 0x2000))
			zoomx = zoomy = source[1] & 0xff;
		else {
			zoomx = source[1] >> 8;
			zoomy = source[1] & 0xff;
		}
		if (!zoomx)
			zoomx = 0x3f;
		if (!zoomy)
			zoomy = 0x3f;

		zoomx++;
		zoomy++;

		x = source[5] & 0xfff;
		flipx = source[5] & 0x8000;
		if (x & 0x800)
			x -= 0x1000;
		sx = 1 << ((source[5] & 0x7000) >> 12);

		x -= 8;

		y = source[4] & 0xfff;
		if (y & 0x800)
			y -= 0x1000;
		flipy = source[4] & 0x8000;
		sy = 1 << ((source[4] & 0x7000) >> 12);

		pix = &sprite_ram[(source[3] & 0x3fff)* 0x8];
		for (px = 0; px < 8; px++) {
			INT32 c;
			c              = pix[px] >> 8;
			pm[px*2]       = pmt[c>>6];
			if (c>1)
				c |= 0x1000;
			colors[px*2]   = c;

			c              = pix[px] & 0xff;
			pm[px*2+1]     = pmt[c>>6];
			if (c>1)
				c |= 0x1000;
			colors[px*2+1] = c;
		}

		offset = (source[2] & 0x7fff) * 0x10;

		xmod = 0x20;
		ymod = 0x20;
		for (py = 0; py < sy; py++) {
			INT32 xmod1 = xmod;
			INT32 xpos1 = x;
			INT32 ypos1 = y, ymod1 = ymod;
			for (px = 0; px < sx; px++) {
				INT32 xmod2 = xmod1, xpos2 = xpos1;
				INT32 zy;
				addoffset = 0x10*(flipx ? sx-px-1 : px) + 0x10*sx*(flipy ? sy-py-1 : py) + (flipy ? 7*2 : 0);
				newoffset = offset + addoffset;

				ymod1 = ymod;
				ypos1 = y;
				for (zy = 0; zy < 8; zy++) {
					ymod1 += zoomy;
					while (ymod1 >= 0x40) {
						if (ypos1 >= min_y && ypos1 <= max_y) {
							xmod2 = xmod1;
							xpos2 = xpos1;

							for(INT32 zx = 0; zx < 8; zx++) {
								xmod2 += zoomx;
								while (xmod2 >= 0x40) {
									if (xpos2 >= min_x && xpos2 <= max_x) {
										INT32 zx1 = flipx ? 7-zx : zx;
										UINT32 neweroffset = (newoffset+(zx1>>2))&0x1ffff; // crackdown sometimes attempts to use data past the end of spriteram
										INT32 c = (sprite_ram[neweroffset] >> (((~zx1) & 3) << 2)) & 0xf;
										UINT8 *pri = pPrioDraw + (ypos1 * nScreenWidth) + xpos2; //&priority_bitmap.pix8(ypos1, xpos2);
										if (!(*pri & pm[c])) {
											c = colors[c];
											if (c) {
												uint16_t *dst = pTransDraw + (ypos1 * nScreenWidth) + xpos2; //&bitmap.pix16(ypos1, xpos2);
												if (c==1)
													*dst = (*dst) | 0x2000;
												else
													*dst = c;
												*pri = 0xff;
											}
										}
									}
									xmod2 -= 0x40;
									xpos2++;
								}
							}
						}
						ymod1 -= 0x40;
						ypos1++;
					}
					if (flipy)
						newoffset -= 2;
					else
						newoffset += 2;
				}

				xpos1 = xpos2;
				xmod1 = xmod2;
			}
			y    = ypos1;
			ymod = ymod1;
		}
	}
}

static void draw_rect(UINT16 *source, const UINT16 *mask, UINT16 tpri, UINT8 lpri, INT32 win, INT32 sx, INT32 sy, INT32 xx1, INT32 yy1, INT32 xx2, INT32 yy2)
{
	source += (sy * 512) + sx;
	UINT8	*prib = pPrioDraw;
	UINT16	*dest = pTransDraw;

	tpri <<= 12;

	dest += yy1*nScreenWidth + xx1;
	prib += yy1*nScreenWidth + xx1;
	mask += yy1*4;
	yy2 -= yy1;

	while (xx1 >= 128) {
		xx1 -= 128;
		xx2 -= 128;
		mask++;
	}

	for (INT32 y = 0; y < yy2; y++) {
		const UINT16 *src   = source;
		UINT16 *dst         = dest;
		UINT8 *pr           = prib;
		const UINT16 *mask1 = mask;
		INT32 llx = xx2;
		INT32 cur_x = xx1;

		while (llx > 0) {
			UINT16 m = *mask1++;

			if (win)
				m = ~m;

			if (!cur_x && llx >= 128) {
				// Fast paths for the 128-pixels without side clipping case

				if (!m) {
					// 1- 128 pixels from this layer
					for (INT32 x = 0; x < 128; x++) {
						if ((*src & 0x1000) == tpri && (*src & 0xf)) {
							*dst = *src & 0xfff;
							*pr |= lpri;
						}
						src++;
						dst++;
						pr++;
					}

				} else if (m == 0xffff) {
					// 2- 128 pixels from the other layer
					src += 128;
					dst += 128;
					pr += 128;

				} else {
					// 3- 128 pixels from both layers
					for (INT32 x = 0; x < 128; x+=8) {
						if (!(m & 0x8000)) {
							for (INT32 xx = 0; xx < 8; xx++)
								if ((src[xx] & 0x1000) == tpri && (src[xx] & 0xf)) {
									dst[xx] = src[xx] & 0xfff;
									pr[xx] |= lpri;
								}
						}
						src += 8;
						dst += 8;
						pr += 8;
						m <<= 1;
					}
				}
			} else {
				// Clipped path
				INT32 llx1 = llx >= 128 ? 128 : llx;

				if (!m) {
					// 1- 128 pixels from this layer
					for (INT32 x = cur_x; x < llx1; x++) {
						if ((*src & 0x1000) == tpri && (*src & 0xf)) {
							*dst = *src & 0xfff;
							*pr |= lpri;
						}
						src++;
						dst++;
						pr++;
					}

				} else if (m == 0xffff) {
					// 2- 128 pixels from the other layer
					src += 128 - cur_x;
					dst += 128 - cur_x;
					pr += 128 - cur_x;

				} else {
					// 3- 128 pixels from both layers
					for (INT32 x = cur_x; x < llx1; x++) {
						if ((*src & 0x1000) == tpri && (*src & 0xf) && !(m & (0x8000 >> (x >> 3)))) {
							*dst = *src & 0xfff;
							*pr |= lpri;
						}
						src++;
						dst++;
						pr++;
					}
				}
			}
			llx -= 128;
			cur_x = 0;
		}
		source += 512;
		dest   += nScreenWidth;
		prib   += nScreenWidth;
		mask   += 4;
	}
}

static void draw_common(INT32 layer, INT32 lpri, INT32 )
{
	UINT16 *tile_ram = (UINT16*)DrvTileRAM;
	UINT16 hscr = tile_ram[0x5000+(layer >> 1)];
	UINT16 vscr = tile_ram[0x5004+(layer >> 1)];
	UINT16 ctrl = tile_ram[0x5004+((layer >> 1) & 2)];
	UINT16 *mask = tile_ram + (layer & 4 ? 0x6800 : 0x6000);
	UINT16 tpri = layer & 1;

	if ((nSpriteEnable & 2) == 0) {
		if (tpri == 0) return;
	}
	if ((nSpriteEnable & 4) == 0) {
		if (tpri == 1) return;
	}

	lpri = 1 << lpri;
	layer >>= 1;

	// Layer disable
	if (vscr & 0x8000) return;

	if (ctrl & 0x6000)	// Special window/scroll modes
	{
		if (layer & 1) return;

		gfx_set = 0;

		GenericTilemapSetScrollY(layer^0, vscr & 0x1ff);
		GenericTilemapSetScrollY(layer^1, vscr & 0x1ff);

		if (hscr & 0x8000)
		{
			UINT16 *hscrtb = tile_ram + 0x4000 + 0x200*layer;

			switch ((ctrl & 0x6000) >> 13)
			{
				case 1:
					{
						UINT16 v = (-vscr) & 0x1ff;

						if (!((-vscr) & 0x200))
							layer ^= 1;

						for (INT32 y = 0; y < nScreenHeight; y++)
						{
							INT32 l1 = layer;
							if (y >= v) l1 ^= 1;

							GenericTilesSetScanline(y);
							GenericTilemapSetScrollX(l1, -(hscrtb[y] & 0x1ff));
							GenericTilemapDraw(l1, pTransDraw, lpri|TMAP_SET_GROUP(tpri), 0xff);
						}
						GenericTilesClearClip();
					}
					break;

				case 2:
				case 3:
					{
						for (INT32 y = 0; y < nScreenHeight; y++)
						{
							INT32 l1 = layer;
							UINT16 h = hscrtb[y] & 0x1ff;

							GenericTilemapSetScrollX(layer|0, -h);
							GenericTilemapSetScrollX(layer|1, -h);

							INT32 maxx = nScreenWidth;
							INT32 minx = 0;
							if (maxx > h) maxx = h; // c1
							if (minx < h) minx = h; // c2

							if (!(hscr & 0x200)) l1 ^= 1;

							GenericTilesSetClip(-1, maxx, y, y+1);
							GenericTilemapDraw(l1|0, pTransDraw, lpri|TMAP_SET_GROUP(tpri), 0xff);
							GenericTilesClearClip();

							GenericTilesSetClip(minx, -1, y, y+1);
							GenericTilemapDraw(l1^1, pTransDraw, lpri|TMAP_SET_GROUP(tpri), 0xff);
							GenericTilesClearClip();
						}
						break;
					}
			}

		} else {
			GenericTilemapSetScrollX(layer|0, -(hscr & 0x1ff));
			GenericTilemapSetScrollX(layer|1, -(hscr & 0x1ff));

			switch ((ctrl & 0x6000) >> 13)
			{
				case 1:
					{
						UINT16 v = (-vscr) & 0x1ff;

						INT32 miny = 0;
						INT32 maxy = nScreenHeight;
						if (maxy > v) maxy = v; // c1
						if (miny < v) miny = v; // c2

						if (!((-vscr) & 0x200)) layer ^= 1;

						GenericTilesSetClip(-1, -1, -1, maxy);
						GenericTilemapDraw(layer^0, pTransDraw, lpri|TMAP_SET_GROUP(tpri), 0xff);
						GenericTilesClearClip();

						GenericTilesSetClip(-1, -1, miny, -1);
						GenericTilemapDraw(layer^1, pTransDraw, lpri|TMAP_SET_GROUP(tpri), 0xff);
						GenericTilesClearClip();
					}
					break;

				case 2:
				case 3:
					{
						UINT16 h = (+hscr) & 0x1ff;

						INT32 maxx = nScreenWidth;
						INT32 minx = 0;
						if (maxx > h) maxx = h; // c1
						if (minx < h) minx = h; // c2

						if (!((+hscr) & 0x200)) layer ^= 1;

						GenericTilesSetClip(-1, maxx, -1, -1);
						GenericTilemapDraw(layer^0, pTransDraw, lpri|TMAP_SET_GROUP(tpri), 0xff);
						GenericTilesClearClip();

						GenericTilesSetClip(minx, -1, -1, -1);
						GenericTilemapDraw(layer^1, pTransDraw, lpri|TMAP_SET_GROUP(tpri), 0xff);
						GenericTilesClearClip();
						break;
					}
			}
		}
	} else {
		INT32 win = layer & 1;
		UINT16 *source = BurnBitmapGetBitmap(1+layer);

		if (hscr & 0x8000)
		{
			UINT16 *hscrtb = tile_ram + 0x4000 + 0x200*layer;
			vscr &= 0x1ff;

			for(INT32 y = 0; y < 384; y++) {
				hscr = (-hscrtb[y]) & 0x1ff;
				if (hscr + 496 <= 512) {		// Horizontal split unnecessary
					draw_rect(source, mask, tpri, lpri, win, hscr, vscr,        0,        y,      496,      y+1);
				} else {			// Horizontal split necessary
					draw_rect(source, mask, tpri, lpri, win, hscr, vscr,        0,        y, 512-hscr,      y+1);
					draw_rect(source, mask, tpri, lpri, win,    0, vscr, 512-hscr,        y,      496,      y+1);
				}
				vscr = (vscr + 1) & 0x1ff;
			}
		} else {
			hscr = (-hscr) & 0x1ff;
			vscr = (+vscr) & 0x1ff;

			if (hscr + 496 <= 512) {	// Horizontal split unnecessary
				if (vscr + 384 <= 512) {	// Vertical split unnecessary
					draw_rect(source, mask, tpri, lpri, win, hscr, vscr,        0,        0,      496,      384);
				} else {		// Vertical split necessary
					draw_rect(source, mask, tpri, lpri, win, hscr, vscr,        0,        0,      496, 512-vscr);
					draw_rect(source, mask, tpri, lpri, win, hscr,    0,        0, 512-vscr,      496,      384);

				}
			} else {	// Horizontal split necessary
				if (vscr + 384 <= 512) {
					// Vertical split unnecessary
					draw_rect(source, mask, tpri, lpri, win, hscr, vscr,        0,        0, 512-hscr,      384);
					draw_rect(source, mask, tpri, lpri, win,    0, vscr, 512-hscr,        0,      496,      384);
				} else {
					// Vertical split necessary
					draw_rect(source, mask, tpri, lpri, win, hscr, vscr,        0,        0, 512-hscr, 512-vscr);
					draw_rect(source, mask, tpri, lpri, win,    0, vscr, 512-hscr,        0,      496, 512-vscr);
					draw_rect(source, mask, tpri, lpri, win, hscr,    0,        0, 512-vscr, 512-hscr,      384);
					draw_rect(source, mask, tpri, lpri, win,    0,    0, 512-hscr, 512-vscr,      496,      384);
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x4000/2; i++) {
			palette_update_entry(i);
		}
		DrvRecalc = 0;
	}

	DrvPalette[0x4000] = 0; //BurnHighCol(0xff,0,0xff,0); // debug, magenta

	if (sys24_mixer_get_reg(13) & 1) {
		BurnTransferClear(0x4000);
		BurnTransferCopy(DrvPalette);
		return 0;
	}

	INT32 order[12], spri[4], level = 0;

	for (INT32 i = 0; i < 12; i++) {
		order[i] = i;
	}

	qsort(order, 12, sizeof(INT32), layer_cmp);

	BurnTransferClear(0x4000);

	{
		UINT16 *tile_ram = (UINT16*)DrvTileRAM;
		for (INT32 i = 0; i < 4; i++)
		{
			if ((tile_ram[0x5004+(i & 2)] & 0x6000) == 0)
			{
				gfx_set = 1; // set1 allows for passing group in color variable
				GenericTilemapSetScrollX(i, 0); // no scroll! added below!
				GenericTilemapSetScrollY(i, 0);
				GenericTilemapDraw(i, 1 + i, TMAP_FORCEOPAQUE); // no priority, draw opaque!
			}
		}
	}

	for (INT32 i = 0; i < 12; i++) {
		if (order[i] < 8) {
			draw_common(order[i], level, 0);
		} else {
			spri[order[i]-8] = level;
			level++;
		}
	}

	draw_sprites(spri);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (uses_tball) {
			BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
			BurnTrackballFrame(0, AnalogWheel[0], AnalogWheel[1], 0x02, 0x07);
			BurnTrackballUpdate(0);

			BurnTrackballConfig(1, AXIS_NORMAL, AXIS_NORMAL);
			BurnTrackballFrame(1, AnalogWheel[2], AnalogWheel[3], 0x02, 0x07);
			BurnTrackballUpdate(1);
		}
	}

	const INT32 MULT  = 4;
	INT32 nInterleave = 424 * MULT;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 10000000 / 58, 10000000 / 58 };
	INT32 nCyclesDone[2] = { extra_cycles[0], extra_cycles[1] };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		CPU_RUN(0, Sek);
		SekClose();

		SekOpen(1);
		CPU_RUN(1, Sek);
		SekClose();

		if (i == 0*MULT || i == 384*MULT) {
			irq_vbl(i/4);
		}

		if (i == 1*MULT || i == 385*MULT) {
			irq_vbl_clear();
		}

		if ((i%MULT)==0 && irq_timer >= 0) {
			if (irq_timer == 0) {
				irq_timer_cb();
			}
			irq_timer--;
		}

		if ((i%MULT)==0 && frc_timer >= 0) { // frc irq clocks the dac (bnzabros)
			frc_timer -= 100;
			if (frc_timer < 0) {
				irq_frc_cb();
				frc_timer = 375;
			}
		}

		if (pBurnSoundOut && (i&0xf)==0xf) {
			INT32 nSegment = nBurnSoundLen / (nInterleave / 0x10);
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}

		SekOpen(1);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		SekClose();
	}

	extra_cycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	extra_cycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	fdc_index_count++;

	return 0;
}

static void DrvCharExpand()
{
	for (INT32 i = 0; i < 0x20000; i++)
	{
		DrvCharExp[i*2+0] = DrvCharRAM[i^1] >> 4;
		DrvCharExp[i*2+1] = DrvCharRAM[i^1] & 0xf;
	}
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_ROM) {
		for (INT32 i = 0; i < 0x80000; i += 0x040000) {
			ba.Data		= Drv68KROM;
			ba.nLen		= 0x040000;
			ba.nAddress	= 0;
			ba.szName	= "68K ROM";
			BurnAcb(&ba);
		}

		for (INT32 i = 0; i < 0x100000; i += 0x040000) {
			ba.Data		= Drv68KROM;
			ba.nLen		= 0x040000;
			ba.nAddress	= 0x100000 + i;
			ba.szName	= "68K ROM (Mirror)";
			BurnAcb(&ba);
		}

		for (INT32 i = 0; i < 0x200000; i += 0x100000) {
			ba.Data		= DrvUserROM + (bankdata & 0xf) * 0x40000;
			ba.nLen		= 0x0040000;
			ba.nAddress	= 0xb80000 + i;
			ba.szName	= "User ROM (bank)";
			BurnAcb(&ba);
		}
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvShareRAM2;
		ba.nLen	  = 0x40000;
		ba.nAddress	= 0x080000;
		ba.szName = "Shared RAM (2)";
		BurnAcb(&ba);

		for (INT32 i = 0; i < 0x80000; i += 0x040000) {
			memset(&ba, 0, sizeof(ba));
			ba.Data	  = DrvShareRAM2;
			ba.nLen	  = 0x40000;
			ba.nAddress	= 0x080000 + i;
			ba.szName = "Shared RAM (2)";
			BurnAcb(&ba);
		}

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvTileRAM;
		ba.nLen	  = 0x10000;
		ba.nAddress	= 0x200000;
		ba.szName = "Tile (Video) RAM";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvCharRAM;
		ba.nLen	  = 0x20000;
		ba.nAddress	= 0x280000;
		ba.szName = "Character (Tile Data) RAM";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvPalRAM;
		ba.nLen	  = 0x04000;
		ba.nAddress	= 0x400000;
		ba.szName = "Palette RAM";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvMixerRegs;
		ba.nLen	  = 0x00020;
		ba.nAddress	= 0x404000;
		ba.szName = "Mixer Regs";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvPalRAM;
		ba.nLen	  = 0x40000;
		ba.nAddress	= 0x600000;
		ba.szName = "Sprite RAM";
		BurnAcb(&ba);

		for (INT32 i = 0; i < 0x80000; i += 0x040000) {
			memset(&ba, 0, sizeof(ba));
			ba.Data	  = DrvShareRAM3;
			ba.nLen	  = 0x40000;
			ba.nAddress	= 0xf00000 + i;
			ba.szName = "Shared RAM (3)";
			BurnAcb(&ba);
		}

		for (INT32 i = 0; i < 0x80000; i += 0x040000) {
			memset(&ba, 0, sizeof(ba));
			ba.Data	  = DrvShareRAM2;
			ba.nLen	  = 0x40000;
			ba.nAddress	= 0xf80000 + i;
			ba.szName = "Shared RAM (2)";
			BurnAcb(&ba);
		}

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		s24_fd1094_scan(nAction);

		if (uses_tball)	BurnTrackballScan();

		SCAN_VAR(irq_allow0);
		SCAN_VAR(irq_allow1);
		SCAN_VAR(irq_yms);
		SCAN_VAR(irq_timer);
		SCAN_VAR(irq_tdata);
		SCAN_VAR(irq_tmode);
		SCAN_VAR(irq_vblank);
		SCAN_VAR(irq_sprite);

		SCAN_VAR(frc_mode);
		SCAN_VAR(frc_cnt);
		SCAN_VAR(frc_timer);
		SCAN_VAR(frc_irq);

		SCAN_VAR(fdc_status);
		SCAN_VAR(fdc_track);
		SCAN_VAR(fdc_sector);
		SCAN_VAR(fdc_data);
		SCAN_VAR(fdc_phys_track);
		SCAN_VAR(fdc_irq);
		SCAN_VAR(fdc_drq);
		SCAN_VAR(fdc_span);
		SCAN_VAR(fdc_index_count);
		SCAN_VAR(fdc_pointer);

		SCAN_VAR(mlatch);
		SCAN_VAR(bankdata);
		SCAN_VAR(hotrod_ctrl_cur);
		SCAN_VAR(cur_input_line);
		SCAN_VAR(system24temp_sys16_io_cnt);
		SCAN_VAR(system24temp_sys16_io_dir);
		SCAN_VAR(extra_cycles);
		SCAN_VAR(prev_resetcontrol);
		SCAN_VAR(resetcontrol);

		SCAN_VAR(gground_hack);
	}

	if (nAction & ACB_WRITE) {
		DrvCharExpand();

		SekOpen(0);
		bankswitch();
		SekClose();
	}

	return 0;
}


// Hot Rod (World, 3 Players, Turbo set 1, Floppy Based)

static struct BurnRomInfo hotrodRomDesc[] = {
	{ "epr-11339.ic2",				0x020000, 0x75130e73, 1 | BRF_PRG | BRF_ESS }, //  0 68K Bios
	{ "epr-11338.ic1",				0x020000, 0x7d4a7ff3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ds3-5000-01d_3p_turbo.img",	0x1d6000, 0x842006fd, 3 | BRF_PRG | BRF_ESS }, //  2 Floppy Disk
};

STD_ROM_PICK(hotrod)
STD_ROM_FN(hotrod)

static INT32 HotrodInit()
{
	mlatch_table = NULL;
	system24temp_sys16_io_set_callbacks(hotrod_io_read, hotrod_io_write);

	uses_tball = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvHotrod = {
	"hotrod", NULL, NULL, NULL, "1988",
	"Hot Rod (World, 3 Players, Turbo set 1, Floppy Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, hotrodRomInfo, hotrodRomName, NULL, NULL, NULL, NULL, HotrodInputInfo, HotrodDIPInfo,
	HotrodInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Hot Rod (World, 3 Players, Turbo set 2, Floppy Based)

static struct BurnRomInfo hotrodaRomDesc[] = {
	{ "epr-11339.ic2",				0x020000, 0x75130e73, 1 | BRF_PRG | BRF_ESS }, //  0 68K Bios
	{ "epr-11338.ic1",				0x020000, 0x7d4a7ff3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ds3-5000-01d.img",			0x1d6000, 0xe25c6b63, 3 | BRF_PRG | BRF_ESS }, //  2 Floppy Disk
};

STD_ROM_PICK(hotroda)
STD_ROM_FN(hotroda)

struct BurnDriver BurnDrvHotroda = {
	"hotroda", "hotrod", NULL, NULL, "1988",
	"Hot Rod (World, 3 Players, Turbo set 2, Floppy Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, hotrodaRomInfo, hotrodaRomName, NULL, NULL, NULL, NULL, HotrodInputInfo, HotrodDIPInfo,
	HotrodInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Hot Rod (Japan, 4 Players, Floppy Based, Rev B)

static struct BurnRomInfo hotrodjaRomDesc[] = {
	{ "epr-11339.ic2",				0x020000, 0x75130e73, 1 | BRF_PRG | BRF_ESS }, //  0 68K Bios
	{ "epr-11338.ic1",				0x020000, 0x7d4a7ff3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ds3-5000-01a-rev-b.img",		0x1d6000, 0xc18f6dca, 3 | BRF_PRG | BRF_ESS }, //  2 Floppy Disk
};

STD_ROM_PICK(hotrodja)
STD_ROM_FN(hotrodja)

struct BurnDriver BurnDrvHotrodja = {
	"hotrodja", "hotrod", NULL, NULL, "1988",
	"Hot Rod (Japan, 4 Players, Floppy Based, Rev B)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, hotrodjaRomInfo, hotrodjaRomName, NULL, NULL, NULL, NULL, HotrodjInputInfo, HotrodjDIPInfo,
	HotrodInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Hot Rod (Japan, 4 Players, Floppy Based, Rev C)

static struct BurnRomInfo hotrodjRomDesc[] = {
	{ "epr-11339.ic2",				0x020000, 0x75130e73, 1 | BRF_PRG | BRF_ESS }, //  0 68K Bios
	{ "epr-11338.ic1",				0x020000, 0x7d4a7ff3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ds3-5000-01a-rev-c.img",		0x1d6000, 0x852f9b5f, 3 | BRF_PRG | BRF_ESS }, //  2 Floppy Disk
};

STD_ROM_PICK(hotrodj)
STD_ROM_FN(hotrodj)

struct BurnDriver BurnDrvHotrodj = {
	"hotrodj", "hotrod", NULL, NULL, "1988",
	"Hot Rod (Japan, 4 Players, Floppy Based, Rev C)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, hotrodjRomInfo, hotrodjRomName, NULL, NULL, NULL, NULL, HotrodjInputInfo, HotrodjDIPInfo,
	HotrodInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Quiz Ghost Hunter (Japan, ROM Based)

static struct BurnRomInfo qghRomDesc[] = {
	{ "16900b",						0x20000, 0x20d7b7d1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Bios
	{ "16899b",						0x20000, 0x397b3ba9, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "16902a",						0x80000, 0xd35b7706, 2 | BRF_PRG | BRF_ESS }, //  2 68K Data
	{ "16901a",						0x80000, 0xab4bcb33, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "16904",						0x80000, 0x10987c88, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "16903",						0x80000, 0xc19f9e46, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "16906",						0x80000, 0x99c6773e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "16905",						0x80000, 0x3922bbe3, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "16908",						0x80000, 0x407ec20f, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "16907",						0x80000, 0x734b0a82, 2 | BRF_PRG | BRF_ESS }, //  9
};

STD_ROM_PICK(qgh)
STD_ROM_FN(qgh)

static INT32 QghInit()
{
	mlatch_table = gqh_mlt;
	system24temp_sys16_io_set_callbacks(hotrod_io_read, hotrod_io_write);

	return DrvInit();
}

struct BurnDriver BurnDrvQgh = {
	"qgh", NULL, NULL, NULL, "1994",
	"Quiz Ghost Hunter (Japan, ROM Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_QUIZ, 0,
	NULL, qghRomInfo, qghRomName, NULL, NULL, NULL, NULL, DrvInputInfo, QghDIPInfo,
	QghInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Scramble Spirits (World, Floppy Based)

static struct BurnRomInfo sspiritsRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Bios
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ds3-5000-02-.img",			0x1c2000, 0x179b98e9, 3 | BRF_PRG | BRF_ESS }, //  2 Floppy Disk Data
};

STD_ROM_PICK(sspirits)
STD_ROM_FN(sspirits)

static INT32 SspiritsInit()
{
	mlatch_table = NULL;
	system24temp_sys16_io_set_callbacks(hotrod_io_read, hotrod_io_write);

	return DrvInit();
}

struct BurnDriver BurnDrvSspirits = {
	"sspirits", NULL, NULL, NULL, "1988",
	"Scramble Spirits (World, Floppy Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, sspiritsRomInfo, sspiritsRomName, NULL, NULL, NULL, NULL, DrvInputInfo, SspiritsDIPInfo,
	SspiritsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	384, 496, 3, 4
};


// Scramble Spirits (Japan, Floppy DS3-5000-02-REV-A Based)

static struct BurnRomInfo sspiritjRomDesc[] = {
	{ "epr-11339.ic2",				0x020000, 0x75130e73, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-11338.ic1",				0x020000, 0x7d4a7ff3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ds3-5000-02-rev-a.img",		0x1d6000, 0x0385470f, 3 | BRF_GRA },           //  2 Floppy Disk Data
};

STD_ROM_PICK(sspiritj)
STD_ROM_FN(sspiritj)

struct BurnDriver BurnDrvSspiritj = {
	"sspiritj", "sspirits", NULL, NULL, "1988",
	"Scramble Spirits (Japan, Floppy DS3-5000-02-REV-A Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, sspiritjRomInfo, sspiritjRomName, NULL, NULL, NULL, NULL, DrvInputInfo, SspiritsDIPInfo,
	SspiritsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	384, 496, 3, 4
};


// Scramble Spirits (World, Floppy Based, FD1094 317-0058-02c)

static struct BurnRomInfo sspirtfcRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "317-0058-02c.key",			0x002000, 0xebae170e, 4 | BRF_PRG | BRF_ESS }, //  2 FD1094 Key

	{ "ds3-5000-02c.img",			0x1c2000, 0x00000000, 3 | BRF_PRG | BRF_ESS | BRF_NODUMP },           //  3 Floppy Disk Data
};

STD_ROM_PICK(sspirtfc)
STD_ROM_FN(sspirtfc)

struct BurnDriver BurnDrvSspirtfc = {
	"sspirtfc", "sspirits", NULL, NULL, "1988",
	"Scramble Spirits (World, Floppy Based, FD1094 317-0058-02c)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, sspirtfcRomInfo, sspirtfcRomName, NULL, NULL, NULL, NULL, DrvInputInfo, SspiritsDIPInfo,
	SspiritsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	384, 496, 3, 4
};


// Bonanza Bros (US, Floppy DS3-5000-07d? Based)

static struct BurnRomInfo bnzabrosRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mpr-13188-h.2",				0x080000, 0xd3802294, 2 | BRF_PRG | BRF_ESS }, //  2 68K ROM Data
	{ "mpr-13187-h.1",				0x080000, 0xe3d8c5f7, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "mpr-13190.4",				0x040000, 0x0b4df388, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-13189.3",				0x040000, 0x5ea5a2f3, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "ds3-5000-07d.img",			0x1c2000, 0x2e70251f, 3 | BRF_PRG | BRF_ESS }, //  6 Floppy Disk Data
};

STD_ROM_PICK(bnzabros)
STD_ROM_FN(bnzabros)

static INT32 BnzabrosInit()
{
	mlatch_table = bnzabros_mlt;
	system24temp_sys16_io_set_callbacks(hotrod_io_read, hotrod_io_write);

	return DrvInit();
}

struct BurnDriver BurnDrvBnzabros = {
	"bnzabros", NULL, NULL, NULL, "1990",
	"Bonanza Bros (US, Floppy DS3-5000-07d? Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, bnzabrosRomInfo, bnzabrosRomName, NULL, NULL, NULL, NULL, DrvInputInfo, BnzabrosDIPInfo,
	BnzabrosInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Bonanza Bros (Japan, Floppy DS3-5000-07b Based)

static struct BurnRomInfo bnzabrosjRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mpr-13188-h.2",				0x080000, 0xd3802294, 2 | BRF_PRG | BRF_ESS }, //  2 68K ROM Data
	{ "mpr-13187-h.1",				0x080000, 0xe3d8c5f7, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "mpr-13190.4",				0x040000, 0x0b4df388, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-13189.3",				0x040000, 0x5ea5a2f3, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "ds3-5000-07b.img",			0x1c2000, 0xefa7f2a7, 3 | BRF_PRG | BRF_ESS }, //  6 Floppy Disk Data
};

STD_ROM_PICK(bnzabrosj)
STD_ROM_FN(bnzabrosj)

struct BurnDriver BurnDrvBnzabrosj = {
	"bnzabrosj", "bnzabros", NULL, NULL, "1990",
	"Bonanza Bros (Japan, Floppy DS3-5000-07b Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, bnzabrosjRomInfo, bnzabrosjRomName, NULL, NULL, NULL, NULL, DrvInputInfo, BnzabrosDIPInfo,
	BnzabrosInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Quiz Rouka Ni Tattenasai (Japan, ROM Based)

static struct BurnRomInfo qroukaRomDesc[] = {
	{ "14485",						0x20000, 0xfc0085f9, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "14484",						0x20000, 0xf51c641c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "14482",						0x80000, 0x7a13dd97, 5 | BRF_PRG | BRF_ESS }, //  2 68K ROM Data
	{ "14483",						0x80000, 0xf3eb51a0, 5 | BRF_PRG | BRF_ESS }, //  3
};

STD_ROM_PICK(qrouka)
STD_ROM_FN(qrouka)

static INT32 QroukaInit()
{
	mlatch_table = qrouka_mlt;
	system24temp_sys16_io_set_callbacks(hotrod_io_read, hotrod_io_write);

	return DrvInit();
}

struct BurnDriver BurnDrvQrouka = {
	"qrouka", NULL, NULL, NULL, "1991",
	"Quiz Rouka Ni Tattenasai (Japan, ROM Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_QUIZ, 0,
	NULL, qroukaRomInfo, qroukaRomName, NULL, NULL, NULL, NULL, QroukaInputInfo, QroukaDIPInfo,
	QroukaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Quiz Mekurumeku Story (Japan, ROM Based)

static struct BurnRomInfo quizmekuRomDesc[] = {
	{ "epr15343.ic2",				0x20000, 0xc72399a7, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr15342.ic1",				0x20000, 0x0968ac84, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr15345.ic5",				0x80000, 0x88030b5d, 2 | BRF_PRG | BRF_ESS }, //  2 68K ROM Data
	{ "epr15344.ic4",				0x80000, 0xdd11b382, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "mpr15347.ic7",				0x80000, 0x0472677b, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr15346.ic6",				0x80000, 0x746d4d0e, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr15349.ic9",				0x80000, 0x770eecf1, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr15348.ic8",				0x80000, 0x7666e960, 2 | BRF_PRG | BRF_ESS }, //  7
};

STD_ROM_PICK(quizmeku)
STD_ROM_FN(quizmeku)

static INT32 QuizmekuInit()
{
	mlatch_table = quizmeku_mlt;
	system24temp_sys16_io_set_callbacks(hotrod_io_read, hotrod_io_write);

	return DrvInit();
}

struct BurnDriver BurnDrvQuizmeku = {
	"quizmeku", NULL, NULL, NULL, "1992",
	"Quiz Mekurumeku Story (Japan, ROM Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_QUIZ, 0,
	NULL, quizmekuRomInfo, quizmekuRomName, NULL, NULL, NULL, NULL, QuizmekuInputInfo, QuizmekuDIPInfo,
	QuizmekuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Dynamic Country Club (World, ROM Based)

static struct BurnRomInfo dcclubRomDesc[] = {
	{ "epr13948.bin",				0x20000, 0xd6a031c8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr13947.bin",				0x20000, 0x7e3cff5e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-15345.2",				0x80000, 0xd9e120c2, 2 | BRF_PRG | BRF_ESS }, //  2 68K ROM Data
	{ "epr-15344.1",				0x80000, 0x8f8b9f74, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "mpr-14097-t.4",				0x80000, 0x4bd74cae, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-14096-t.3",				0x80000, 0x38d96502, 2 | BRF_PRG | BRF_ESS }, //  5
};

STD_ROM_PICK(dcclub)
STD_ROM_FN(dcclub)

static INT32 DcclubInit()
{
	mlatch_table = dcclub_mlt;
	system24temp_sys16_io_set_callbacks(dcclub_io_read, hotrod_io_write);

	uses_tball = 1;

	INT32 rc = DrvInit();

	if (!rc) {
		BurnTrackballConfigStartStopPoints(0, 0x00, 0xbf, 0x00, 0x00);
		bprintf(0, _T("dcclub-dial mode\n"));
	}

	return rc;
}

struct BurnDriver BurnDrvDcclub = {
	"dcclub", NULL, NULL, NULL, "1991",
	"Dynamic Country Club (World, ROM Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_SPORTSMISC, 0,
	NULL, dcclubRomInfo, dcclubRomName, NULL, NULL, NULL, NULL, SgmastjInputInfo, DcclubDIPInfo,
	DcclubInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Dynamic Country Club (US, Floppy Based, FD1094 317-0058-09d)

static struct BurnRomInfo dcclubfdRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "317-0058-09d.key",			0x002000, 0xa91ebffb, 4 | BRF_PRG | BRF_ESS }, //  2 FD1094 Key

	{ "ds3-5000-09d.img",			0x1c2000, 0x69870887, 3 | BRF_PRG | BRF_ESS }, //  3 Floppy Disk Data
};

STD_ROM_PICK(dcclubfd)
STD_ROM_FN(dcclubfd)

struct BurnDriver BurnDrvDcclubfd = {
	"dcclubfd", "dcclub", NULL, NULL, "1991",
	"Dynamic Country Club (US, Floppy Based, FD1094 317-0058-09d)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_SPORTSMISC, 0,
	NULL, dcclubfdRomInfo, dcclubfdRomName, NULL, NULL, NULL, NULL, SgmastjInputInfo, DcclubDIPInfo,
	DcclubInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Dynamic Country Club (Japan, ROM Based)

static struct BurnRomInfo dcclubjRomDesc[] = {
	{ "epr13948.bin",				0x20000, 0xd6a031c8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr13947.bin",				0x20000, 0x7e3cff5e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-14095a.2",				0x80000, 0x88d184e9, 2 | BRF_PRG | BRF_ESS }, //  2 68K ROM Data
	{ "epr-14094a.1",				0x80000, 0x7dd2b7d4, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "mpr-14097-t.4",				0x80000, 0x4bd74cae, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr-14096-t.3",				0x80000, 0x38d96502, 2 | BRF_PRG | BRF_ESS }, //  5
};

STD_ROM_PICK(dcclubj)
STD_ROM_FN(dcclubj)

struct BurnDriver BurnDrvDcclubj = {
	"dcclubj", "dcclub", NULL, NULL, "1991",
	"Dynamic Country Club (Japan, ROM Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_SPORTSMISC, 0,
	NULL, dcclubjRomInfo, dcclubjRomName, NULL, NULL, NULL, NULL, SgmastjInputInfo, DcclubDIPInfo,
	DcclubInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Super Masters Golf (World?, Floppy Based, FD1094 317-0058-05d?)

static struct BurnRomInfo sgmastRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "317-0058-05d.key",			0x002000, 0xc779738d, 4 | BRF_PRG | BRF_ESS }, //  2 FD1094 Key

	{ "ds3-5000-05d.img",			0x1c2000, 0xe9a69f93, 3 | BRF_PRG | BRF_ESS }, //  3 Floppy Disk Data
};

STD_ROM_PICK(sgmast)
STD_ROM_FN(sgmast)

static INT32 SgmastInit()
{
	mlatch_table = NULL;
	system24temp_sys16_io_set_callbacks(hotrod_io_read, hotrod_io_write);

	return DrvInit();
}

struct BurnDriver BurnDrvSgmast = {
	"sgmast", NULL, NULL, NULL, "1989",
	"Super Masters Golf (World?, Floppy Based, FD1094 317-0058-05d?)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_SPORTSMISC, 0,
	NULL, sgmastRomInfo, sgmastRomName, NULL, NULL, NULL, NULL, SgmastInputInfo, SgmastDIPInfo,
	SgmastInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Jumbo Ozaki Super Masters Golf (World, Floppy Based, FD1094 317-0058-05c)

static struct BurnRomInfo sgmastcRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "317-0058-05c.key",			0x002000, 0xae0eabe5, 4 | BRF_PRG | BRF_ESS }, //  2 FD1094 Key

	{ "ds3-5000-05c.img",			0x1c2000, 0x63a6ef3a, 3 | BRF_PRG | BRF_ESS }, //  3 Floppy Disk Data
};

STD_ROM_PICK(sgmastc)
STD_ROM_FN(sgmastc)

struct BurnDriver BurnDrvSgmastc = {
	"sgmastc", "sgmast", NULL, NULL, "1989",
	"Jumbo Ozaki Super Masters Golf (World, Floppy Based, FD1094 317-0058-05c)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_SPORTSMISC, 0,
	NULL, sgmastcRomInfo, sgmastcRomName, NULL, NULL, NULL, NULL, SgmastInputInfo, SgmastDIPInfo,
	SgmastInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Jumbo Ozaki Super Masters Golf (Japan, Floppy Based, FD1094 317-0058-05b)

static struct BurnRomInfo sgmastjRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "317-0058-05b.key",			0x002000, 0xadc0c83b, 4 | BRF_PRG | BRF_ESS }, //  2 FD1094 Key

	{ "ds3-5000-05b.img",			0x1c2000, 0xa136668c, 3 | BRF_PRG | BRF_ESS }, //  3 Floppy Disk Data
};

STD_ROM_PICK(sgmastj)
STD_ROM_FN(sgmastj)

static INT32 SgmastjInit()
{
	uses_tball = 1;
	return SgmastInit();
}

struct BurnDriver BurnDrvSgmastj = {
	"sgmastj", "sgmast", NULL, NULL, "1989",
	"Jumbo Ozaki Super Masters Golf (Japan, Floppy Based, FD1094 317-0058-05b)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_SPORTSMISC, 0,
	NULL, sgmastjRomInfo, sgmastjRomName, NULL, NULL, NULL, NULL, SgmastjInputInfo, SgmastjDIPInfo,
	SgmastjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Tokoro San no MahMahjan (Japan, ROM Based)

static struct BurnRomInfo mahmajnRomDesc[] = {
	{ "epr14813.bin",				0x20000, 0xea38cf4b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr14812.bin",				0x20000, 0x5a3cb4a7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mpr14820.bin",				0x80000, 0x8d2a03d3, 2 | BRF_PRG | BRF_ESS }, //  2 68K ROM Data
	{ "mpr14819.bin",				0x80000, 0xe84c4827, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "mpr14822.bin",				0x80000, 0x7c3dcc51, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr14821.bin",				0x80000, 0xbd8dc543, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr14824.bin",				0x80000, 0x38311933, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr14823.bin",				0x80000, 0x4c8d4550, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "mpr14826.bin",				0x80000, 0xc31b8805, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "mpr14825.bin",				0x80000, 0x191080a1, 2 | BRF_PRG | BRF_ESS }, //  9
};

STD_ROM_PICK(mahmajn)
STD_ROM_FN(mahmajn)

static INT32 MahmajnInit()
{
	mlatch_table = mahmajn_mlt;
	system24temp_sys16_io_set_callbacks(mahmajn_io_read, mahmajn_io_write);

	return DrvInit();
}

struct BurnDriverD BurnDrvMahmajn = {
	"mahmajn", NULL, NULL, NULL, "1992",
	"Tokoro San no MahMahjan (Japan, ROM Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MAHJONG, 0,
	NULL, mahmajnRomInfo, mahmajnRomName, NULL, NULL, NULL, NULL, DrvInputInfo, QghDIPInfo, //MahmajnInputInfo, MahmajnDIPInfo,
	MahmajnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Tokoro San no MahMahjan 2 (Japan, ROM Based)

static struct BurnRomInfo mahmajn2RomDesc[] = {
	{ "epr16799.bin",				0x20000, 0x3a34cf75, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr16798.bin",				0x20000, 0x662923fa, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mpr16801.bin",				0x80000, 0x74855a17, 2 | BRF_PRG | BRF_ESS }, //  2 68K ROM Data
	{ "mpr16800.bin",				0x80000, 0x6dbc1e02, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "mpr16803.bin",				0x80000, 0x9b658dd6, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mpr16802.bin",				0x80000, 0xb4723225, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "mpr16805.bin",				0x80000, 0xd15528df, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "mpr16804.bin",				0x80000, 0xa0de08e2, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "mpr16807.bin",				0x80000, 0x816188bb, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "mpr16806.bin",				0x80000, 0x54b353d3, 2 | BRF_PRG | BRF_ESS }, //  9
};

STD_ROM_PICK(mahmajn2)
STD_ROM_FN(mahmajn2)

static INT32 Mahmajn2Init()
{
	mlatch_table = mahmajn2_mlt;
	system24temp_sys16_io_set_callbacks(mahmajn_io_read, mahmajn_io_write);

	return DrvInit();
}

struct BurnDriverD BurnDrvMahmajn2 = {
	"mahmajn2", NULL, NULL, NULL, "1994",
	"Tokoro San no MahMahjan 2 (Japan, ROM Based)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MAHJONG, 0,
	NULL, mahmajn2RomInfo, mahmajn2RomName, NULL, NULL, NULL, NULL, DrvInputInfo, QghDIPInfo, //MahmajnInputInfo, MahmajnDIPInfo,
	Mahmajn2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Quiz Syukudai wo Wasuremashita (Japan, Floppy Based, FD1094 317-0058-08b)

static struct BurnRomInfo qswwRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "317-0058-08b.key",			0x002000, 0xfe0a336a, 4 | BRF_PRG | BRF_ESS }, //  2 FD1094 Key

	{ "ds3-5000-08b.img",			0x1c2000, 0x5a886d38, 3 | BRF_PRG | BRF_ESS }, //  3 Floppy Disk Data
};

STD_ROM_PICK(qsww)
STD_ROM_FN(qsww)

static INT32 QswwInit()
{
	mlatch_table = NULL;
	system24temp_sys16_io_set_callbacks(hotrod_io_read, hotrod_io_write);

	return DrvInit();
}

struct BurnDriver BurnDrvQsww = {
	"qsww", NULL, NULL, NULL, "1991",
	"Quiz Syukudai wo Wasuremashita (Japan, Floppy Based, FD1094 317-0058-08b)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_QUIZ, 0,
	NULL, qswwRomInfo, qswwRomName, NULL, NULL, NULL, NULL, DrvInputInfo, QswwDIPInfo,
	QswwInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Gain Ground (World, 3 Players, Floppy Based, FD1094 317-0058-03d Rev A)

static struct BurnRomInfo ggroundRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "317-0058-03d.key",			0x002000, 0xe1785bbd, 4 | BRF_PRG | BRF_ESS }, //  2 FD1094 Key

	{ "ds3-5000-03d-rev-a.img",		0x1c2000, 0x5c5910f2, 3 | BRF_PRG | BRF_ESS }, //  3 Floppy Disk Data
};

STD_ROM_PICK(gground)
STD_ROM_FN(gground)

static INT32 GgroundInit()
{
	mlatch_table = NULL;
	system24temp_sys16_io_set_callbacks(gground_io_read, hotrod_io_write);

	return DrvInit();
}

struct BurnDriver BurnDrvGground = {
	"gground", NULL, NULL, NULL, "1988",
	"Gain Ground (World, 3 Players, Floppy Based, FD1094 317-0058-03d Rev A)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_ACTION | GBF_SHOOT, 0,
	NULL, ggroundRomInfo, ggroundRomName, NULL, NULL, NULL, NULL, GgroundInputInfo, GgroundDIPInfo,
	GgroundInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	384,496, 3, 4
};


// Gain Ground (Japan, 2 Players, Floppy Based, FD1094 317-0058-03b)

static struct BurnRomInfo ggroundjRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "317-0058-03b.key",			0x002000, 0x84aecdba, 4 | BRF_PRG | BRF_ESS }, //  2 FD1094 Key

	{ "ds3-5000-03b.img",			0x1c2000, 0x7200dac9, 3 | BRF_PRG | BRF_ESS }, //  3 Floppy Disk Data
};

STD_ROM_PICK(ggroundj)
STD_ROM_FN(ggroundj)

struct BurnDriver BurnDrvGgroundj = {
	"ggroundj", "gground", NULL, NULL, "1988",
	"Gain Ground (Japan, 2 Players, Floppy Based, FD1094 317-0058-03b)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_ACTION | GBF_SHOOT, 0,
	NULL, ggroundjRomInfo, ggroundjRomName, NULL, NULL, NULL, NULL, GgroundjInputInfo, GgroundjDIPInfo,
	GgroundInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	384,496, 3, 4
};


// Crack Down (World, Floppy Based, FD1094 317-0058-04c)

static struct BurnRomInfo crkdownRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "317-0058-04c.key",			0x002000, 0x16e978cc, 4 | BRF_PRG | BRF_ESS }, //  2 FD1094 Key

	{ "ds3-5000-04c.img",			0x1c2000, 0x7d97ba5e, 3 | BRF_PRG | BRF_ESS }, //  3 Floppy Disk Data
};

STD_ROM_PICK(crkdown)
STD_ROM_FN(crkdown)

static INT32 CrkdownInit()
{
	mlatch_table = NULL;
	system24temp_sys16_io_set_callbacks(hotrod_io_read, hotrod_io_write);

	return DrvInit();
}

struct BurnDriver BurnDrvCrkdown = {
	"crkdown", NULL, NULL, NULL, "1989",
	"Crack Down (World, Floppy Based, FD1094 317-0058-04c)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_ACTION | GBF_SHOOT, 0,
	NULL, crkdownRomInfo, crkdownRomName, NULL, NULL, NULL, NULL, DrvInputInfo, CrkdownDIPInfo,
	CrkdownInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Crack Down (US, Floppy Based, FD1094 317-0058-04d)

static struct BurnRomInfo crkdownuRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "317-0058-04d.key",			0x002000, 0x934ac358, 4 | BRF_PRG | BRF_ESS }, //  2 FD1094 Key

	{ "ds3-5000-04d.img",			0x1c2000, 0x8679032c, 3 | BRF_PRG | BRF_ESS }, //  3 Floppy Disk Data
};

STD_ROM_PICK(crkdownu)
STD_ROM_FN(crkdownu)

struct BurnDriver BurnDrvCrkdownu = {
	"crkdownu", "crkdown", NULL, NULL, "1989",
	"Crack Down (US, Floppy Based, FD1094 317-0058-04d)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_ACTION | GBF_SHOOT, 0,
	NULL, crkdownuRomInfo, crkdownuRomName, NULL, NULL, NULL, NULL, DrvInputInfo, CrkdownDIPInfo,
	CrkdownInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Crack Down (Japan, Floppy Based, FD1094 317-0058-04b Rev A)

static struct BurnRomInfo crkdownjRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "317-0058-04b.key",			0x002000, 0x4a99a202, 4 | BRF_PRG | BRF_ESS }, //  2 FD1094 Key

	{ "ds3-5000-04b-rev-a.img",		0x1c2000, 0x5daa1a9a, 3 | BRF_PRG | BRF_ESS }, //  3 Floppy Disk Data
};

STD_ROM_PICK(crkdownj)
STD_ROM_FN(crkdownj)

struct BurnDriver BurnDrvCrkdownj = {
	"crkdownj", "crkdown", NULL, NULL, "1989",
	"Crack Down (Japan, Floppy Based, FD1094 317-0058-04b Rev A)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_ACTION | GBF_SHOOT, 0,
	NULL, crkdownjRomInfo, crkdownjRomName, NULL, NULL, NULL, NULL, DrvInputInfo, CrkdownDIPInfo,
	CrkdownInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};


// Rough Racer (Japan, Floppy Based, FD1094 317-0058-06b)

static struct BurnRomInfo roughracRomDesc[] = {
	{ "epr-12187.ic2",				0x020000, 0xe83783f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "epr-12186.ic1",				0x020000, 0xce76319d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "317-0058-06b.key",			0x002000, 0x6a5bf536, 4 | BRF_PRG | BRF_ESS }, //  2 FD1094 Key

	{ "ds3-5000-06b.img",			0x1c2000, 0xa7fb2149, 3 | BRF_PRG | BRF_ESS }, //  3 Floppy Disk Dave
};

STD_ROM_PICK(roughrac)
STD_ROM_FN(roughrac)

static INT32 RoughracInit()
{
	mlatch_table = NULL;
	system24temp_sys16_io_set_callbacks(hotrod_io_read, hotrod_io_write);

	uses_tball = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvRoughrac = {
	"roughrac", NULL, NULL, NULL, "1990",
	"Rough Racer (Japan, Floppy Based, FD1094 317-0058-06b)\0", NULL, "Sega", "System 24",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, roughracRomInfo, roughracRomName, NULL, NULL, NULL, NULL, RoughracInputInfo, RoughracDIPInfo,
	RoughracInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	496, 384, 4, 3
};
