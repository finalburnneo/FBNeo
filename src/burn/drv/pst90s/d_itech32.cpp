// FB Neo Incredible technologies (32-bit blitter) driver module
// Based on MAME driver by Aaron Giles and Brian Troha

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6809_intf.h"
#include "es5506.h"
#include "watchdog.h"
#include "timekpr.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvSndROM2;
static UINT8 *DrvSndROM3;
static UINT8 *DrvNVRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvM6809RAM;
static UINT16 *video_regs;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 vint_state;
static INT32 xint_state;
static INT32 qint_state;
static INT32 sound_int_state;

static INT32 soundlatch;
static INT32 sound_return;
static INT32 sound_flipper;

static UINT16 color_latch[2];
static UINT8 enable_latch[2];
static double palette_intensity = 1.0;
static INT32 sound_bank;

static INT32 maincpu_clock;
static INT32 prot_address;
static INT32 flip_color_banks = 0;
static INT32 vblank;
static INT32 video_reinitialize;

static INT32 n68KROMLen;
static INT32 nGfxROMLen;
static INT32 nSndROMLen[4];

static INT32 scanline;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvSvc0[1]; // f2/service mode(diag) button
static UINT8 DrvDips[1];
static UINT8 DrvReset;
static UINT8 DrvInputs[6];

static INT32 is_shufshot = 0;
static INT32 is_pubball = 0;
static INT32 is_shoottv = 0;

static INT32 is_16bit = 0;

static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;
static INT16 DrvAnalogPort2 = 0;
static INT16 DrvAnalogPort3 = 0;

static INT32 Trackball_Type = -1;

enum {
	TB_TYPE0 = 0, // shufshot, wcbowl, wcbowl165, wcbowl161, wcbowl16, wcbowl140, wcbowldx
	TB_TYPE1,     // wcbowl15, wcbowl14, wcbowl13, wcbowl13j, wcbowl12, wcbowl11
	TB_TYPE2,     // gt3d
	TB_TYPE3,     // gt*"S"
	TB_TYPE4      // gt3dl
};

static INT32 tb_last_read[2];
static INT32 tb_last_result[2];
static INT32 tb_effx[2];
static INT32 tb_effy[2];

static struct BurnInputInfo TimekillInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 5"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 5"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvSvc0 + 0,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Timekill)

static struct BurnInputInfo BloodstmInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy6 + 0,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy6 + 2,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy6 + 4,	"p1 fire 5"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy6 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy6 + 3,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy6 + 5,	"p2 fire 5"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvSvc0 + 0,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Bloodstm)

static struct BurnInputInfo HardyardInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 7,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 6,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 5,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 3,	"p3 fire 2"	},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 1,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy4 + 7,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy4 + 6,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy4 + 5,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 2,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvSvc0 + 0,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Hardyard)

static struct BurnInputInfo SftmInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Weak Punch",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Medium Punch",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},
	{"P1 Strong Punch",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 3"	},
	{"P1 Weak Kick",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 4"	},
	{"P1 Medium Kick",	BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 5"	},
	{"P1 Strong Kick",	BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 6"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Weak Punch",	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Medium Punch",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},
	{"P2 Strong Punch",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 3"	},
	{"P2 Weak Kick",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 4"	},
	{"P2 Medium Kick",	BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 5"	},
	{"P2 Strong Kick",	BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 6"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvSvc0 + 0,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Sftm)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static struct BurnInputInfo ShoottvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	A("P1 Gun X", 		BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Gun Y", 		BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	A("P2 Gun X", 		BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	A("P2 Gun Y", 		BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvSvc0 + 0,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Shoottv)

static struct BurnInputInfo PubballInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),
	{"P1 First Base",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Second Base",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},
	{"P1 Third Base",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 3"	},
	{"P1 Home",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 4"	},
	{"P1 Power Up",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 5"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis"),
	{"P2 First Base",	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Second Base",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},
	{"P2 Third Base",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 3"	},
	{"P2 Home",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 4"	},
	{"P2 Power Up",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 5"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvSvc0 + 0,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Pubball)

static struct BurnInputInfo PairsInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvSvc0 + 0,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Pairs)

static struct BurnInputInfo WcbowlInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvSvc0 + 0,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Wcbowl)

static struct BurnInputInfo ShufshotInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvSvc0 + 0,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Shufshot)

static struct BurnInputInfo Gt3dInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},

	{"Volume Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 3"	},
	{"Volume Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy5 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvSvc0 + 0,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Gt3d)
#undef A

static struct BurnDIPInfo TimekillDIPList[]=
{
	{0x19, 0xff, 0xff, 0x07, NULL				},

	{0   , 0xfe, 0   ,    2, "Video Sync"		},
	{0x19, 0x01, 0x10, 0x00, "-"				},
	{0x19, 0x01, 0x10, 0x10, "+"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x19, 0x01, 0x20, 0x00, "Off"				},
	{0x19, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Violence"			},
	{0x19, 0x01, 0x40, 0x40, "Off"				},
	{0x19, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x19, 0x01, 0x80, 0x00, "Off"				},
	{0x19, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Timekill)

static struct BurnDIPInfo BloodstmDIPList[]=
{
	{0x19, 0xff, 0xff, 0x07, NULL				},

	{0   , 0xfe, 0   ,    2, "Video Sync"		},
	{0x19, 0x01, 0x10, 0x00, "-"				},
	{0x19, 0x01, 0x10, 0x10, "+"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x19, 0x01, 0x20, 0x00, "Off"				},
	{0x19, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Violence"			},
	{0x19, 0x01, 0x40, 0x40, "Off"				},
	{0x19, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x19, 0x01, 0x80, 0x00, "Off"				},
	{0x19, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Bloodstm)

static struct BurnDIPInfo HardyardDIPList[]=
{
	{0x21, 0xff, 0xff, 0x07, NULL				},

	{0   , 0xfe, 0   ,    2, "Video Sync"		},
	{0x21, 0x01, 0x10, 0x00, "-"				},
	{0x21, 0x01, 0x10, 0x10, "+"				},

	{0   , 0xfe, 0   ,    1, "Cabinet"			},
	{0x21, 0x01, 0x20, 0x00, "Upright"			},

	{0   , 0xfe, 0   ,    2, "Players"			},
	{0x21, 0x01, 0x40, 0x00, "4"				},
	{0x21, 0x01, 0x40, 0x40, "2"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x21, 0x01, 0x80, 0x00, "Off"				},
	{0x21, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Hardyard)

static struct BurnDIPInfo SftmDIPList[]=
{
	{0x1b, 0xff, 0xff, 0x01, NULL				},

	{0   , 0xfe, 0   ,    2, "Video Sync"		},
	{0x1b, 0x01, 0x10, 0x00, "-"				},
	{0x1b, 0x01, 0x10, 0x10, "+"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x1b, 0x01, 0x20, 0x00, "Off"				},
	{0x1b, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Freeze Screen"	},
	{0x1b, 0x01, 0x40, 0x00, "Off"				},
	{0x1b, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x1b, 0x01, 0x80, 0x00, "Off"				},
	{0x1b, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Sftm)

static struct BurnDIPInfo ShoottvDIPList[]=
{
	DIP_OFFSET(0x0d)
	{0x00, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Video Sync"			},
	{0x00, 0x01, 0x10, 0x00, "-"					},
	{0x00, 0x01, 0x10, 0x10, "+"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x20, 0x00, "Off"					},
	{0x00, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x80, 0x00, "Off"					},
	{0x00, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Shoottv)

static struct BurnDIPInfo PubballDIPList[]=
{
	DIP_OFFSET(0x1d)
	{0x00, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Video Sync"			},
	{0x00, 0x01, 0x10, 0x00, "-"					},
	{0x00, 0x01, 0x10, 0x10, "+"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x20, 0x00, "Off"					},
	{0x00, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x80, 0x00, "Off"					},
	{0x00, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Pubball)

static struct BurnDIPInfo PairsDIPList[]=
{
	{0x11, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Video Sync"			},
	{0x11, 0x01, 0x10, 0x00, "-"					},
	{0x11, 0x01, 0x10, 0x10, "+"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x11, 0x01, 0x20, 0x00, "Off"					},
	{0x11, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Modesty"				},
	{0x11, 0x01, 0x40, 0x00, "Off"					},
	{0x11, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Pairs)

static struct BurnDIPInfo WcbowlDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Video Sync"			},
	{0x0f, 0x01, 0x10, 0x00, "-"					},
	{0x0f, 0x01, 0x10, 0x10, "+"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0f, 0x01, 0x20, 0x00, "Off"					},
	{0x0f, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x0f, 0x01, 0x40, 0x40, "Off"					},
	{0x0f, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x80, 0x00, "Off"					},
	{0x0f, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Wcbowl)

static struct BurnDIPInfo WcbowljDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x07, NULL					},

    {0   , 0xfe, 0   ,    2, "Video Sync"			},
	{0x0f, 0x01, 0x10, 0x00, "-"					},
	{0x0f, 0x01, 0x10, 0x10, "+"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0f, 0x01, 0x20, 0x00, "Off"					},
	{0x0f, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Controls"				},
	{0x0f, 0x01, 0x40, 0x00, "One Trackball"		},
	{0x0f, 0x01, 0x40, 0x40, "Two Trackballs"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x80, 0x00, "Off"					},
	{0x0f, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Wcbowlj)

static struct BurnDIPInfo WcbowlnDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x0f, 0x01, 0x10, 0x00, "Off"					},
	{0x0f, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0f, 0x01, 0x20, 0x00, "Upright"				},
	{0x0f, 0x01, 0x20, 0x20, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Freeze Screen"		},
	{0x0f, 0x01, 0x40, 0x00, "Off"					},
	{0x0f, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x80, 0x00, "Off"					},
	{0x0f, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Wcbowln)

static struct BurnDIPInfo WcbowldxDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x0f, 0x01, 0x10, 0x00, "Off"					},
	{0x0f, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0f, 0x01, 0x20, 0x00, "Off"					},
	{0x0f, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x0f, 0x01, 0x40, 0x40, "Off"					},
	{0x0f, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x80, 0x00, "Off"					},
	{0x0f, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Wcbowldx)

static struct BurnDIPInfo WcbowloDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x0f, 0x01, 0x10, 0x00, "Off"					},
	{0x0f, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0f, 0x01, 0x20, 0x00, "Off"					},
	{0x0f, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Freeze Screen"		},
	{0x0f, 0x01, 0x40, 0x40, "Off"					},
	{0x0f, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x80, 0x00, "Off"					},
	{0x0f, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Wcbowlo)

static struct BurnDIPInfo ShufshotDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Video Sync"			},
	{0x0f, 0x01, 0x10, 0x00, "-"					},
	{0x0f, 0x01, 0x10, 0x10, "+"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0f, 0x01, 0x20, 0x00, "Upright"				},
	{0x0f, 0x01, 0x20, 0x20, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x0f, 0x01, 0x40, 0x40, "Off"					},
	{0x0f, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x80, 0x00, "Off"					},
	{0x0f, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Shufshot)

static struct BurnDIPInfo ShufshtoDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Video Sync"			},
	{0x0f, 0x01, 0x10, 0x00, "-"					},
	{0x0f, 0x01, 0x10, 0x10, "+"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0f, 0x01, 0x20, 0x00, "Off"					},
	{0x0f, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Freeze Screen"		},
	{0x0f, 0x01, 0x40, 0x40, "Off"					},
	{0x0f, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x80, 0x00, "Off"					},
	{0x0f, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Shufshto)

static struct BurnDIPInfo Gt3dDIPList[]=
{
	{0x11, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x10, 0x00, "Off"					},
	{0x11, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Trackball Orientation"},
	{0x11, 0x01, 0x20, 0x00, "Normal Mount"			},
	{0x11, 0x01, 0x20, 0x20, "45 Degree Angle"		},

	{0   , 0xfe, 0   ,    2, "Controls"				},
	{0x11, 0x01, 0x40, 0x00, "One Trackball"		},
	{0x11, 0x01, 0x40, 0x40, "Two Trackballs"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Gt3d)

static struct BurnDIPInfo Gt97DIPList[]=
{
	{0x11, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Freeze Screen"		},
	{0x11, 0x01, 0x10, 0x00, "Off"					},
	{0x11, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x20, 0x00, "Off"					},
	{0x11, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x11, 0x01, 0x40, 0x00, "Upright"				},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Gt97)

static struct BurnDIPInfo Gt97oDIPList[]=
{
	{0x11, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Freeze Screen"		},
	{0x11, 0x01, 0x10, 0x00, "Off"					},
	{0x11, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Trackball Orientation"},
	{0x11, 0x01, 0x20, 0x00, "Normal Mount"			},
	{0x11, 0x01, 0x20, 0x20, "45 Degree Angle"		},

	{0   , 0xfe, 0   ,    2, "Controls"				},
	{0x11, 0x01, 0x40, 0x00, "One Trackball"		},
	{0x11, 0x01, 0x40, 0x40, "Two Trackballs"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Gt97o)

static struct BurnDIPInfo Gt97sDIPList[]=
{
	{0x11, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Freeze Screen"		},
	{0x11, 0x01, 0x10, 0x00, "Off"					},
	{0x11, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Trackball Orientation"},
	{0x11, 0x01, 0x20, 0x00, "Normal Mount"			},
	{0x11, 0x01, 0x20, 0x20, "45 Degree Angle"		},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x40, 0x00, "Off"					},
	{0x11, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Gt97s)

static struct BurnDIPInfo Gt98DIPList[]=
{
	{0x11, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x10, 0x00, "Off"					},
	{0x11, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x20, 0x00, "Off"					},
	{0x11, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x11, 0x01, 0x40, 0x00, "Upright"				},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Gt98)

static struct BurnDIPInfo Gt98sDIPList[]=
{
	{0x11, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x10, 0x00, "Off"					},
	{0x11, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x20, 0x00, "Off"					},
	{0x11, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x40, 0x00, "Off"					},
	{0x11, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Gt98s)

static struct BurnDIPInfo S_verDIPList[]=
{
	{0x11, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Trackball Orientation"},
	{0x11, 0x01, 0x10, 0x00, "Normal Mount"			},
	{0x11, 0x01, 0x10, 0x10, "45 Degree Angle"		},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x20, 0x00, "Off"					},
	{0x11, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x11, 0x01, 0x40, 0x00, "Off"					},
	{0x11, 0x01, 0x40, 0x40, "On"					},

    {0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(S_ver)

static struct BurnDIPInfo AamaDIPList[]=
{
	{0x11, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    2, "Trackball Orientation"},
	{0x11, 0x01, 0x10, 0x00, "Normal Mount"			},
	{0x11, 0x01, 0x10, 0x10, "45 Degree Angle"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x11, 0x01, 0x20, 0x00, "Upright"				},
	{0x11, 0x01, 0x20, 0x20, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Controls"				},
	{0x11, 0x01, 0x40, 0x00, "One Trackball"		},
	{0x11, 0x01, 0x40, 0x40, "Two Trackballs"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Aama)

static void itech32_update_interrupts(INT32 vint, INT32 xint, INT32 qint)
{
	INT32 level = 0;

	if (is_shoottv)
		vint = -1;

	if (vint != -1) vint_state = vint;
	if (xint != -1) xint_state = xint;
	if (qint != -1) qint_state = qint;

	if (vint_state) level = 1;
	if (xint_state) level = 2;
	if (qint_state) level = 3;

	if (level)
		SekSetIRQLine(level, CPU_IRQSTATUS_ACK);
	else
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
}

#define itech32_video			video_regs

#define VIDEO_UNKNOWN00			itech32_video[0x00/2] 	/* $0087 at startup */
#define VIDEO_STATUS			itech32_video[0x00/2]
#define VIDEO_INTSTATE			itech32_video[0x02/2]
#define VIDEO_INTACK			itech32_video[0x02/2]
#define VIDEO_TRANSFER			itech32_video[0x04/2]
#define VIDEO_TRANSFER_FLAGS	itech32_video[0x06/2]	/* $5080 at startup (kept at $1512) */
#define VIDEO_COMMAND			itech32_video[0x08/2]	/* $0005 at startup */
#define VIDEO_INTENABLE			itech32_video[0x0a/2]	/* $0144 at startup (kept at $1514) */
#define VIDEO_TRANSFER_HEIGHT	itech32_video[0x0c/2]
#define VIDEO_TRANSFER_WIDTH	itech32_video[0x0e/2]
#define VIDEO_TRANSFER_ADDRLO	itech32_video[0x10/2]
#define VIDEO_TRANSFER_X		itech32_video[0x12/2]
#define VIDEO_TRANSFER_Y		itech32_video[0x14/2]
#define VIDEO_SRC_YSTEP			itech32_video[0x16/2]	/* $0011 at startup */
#define VIDEO_SRC_XSTEP			itech32_video[0x18/2]
#define VIDEO_DST_XSTEP			itech32_video[0x1a/2]
#define VIDEO_DST_YSTEP			itech32_video[0x1c/2]
#define VIDEO_YSTEP_PER_X		itech32_video[0x1e/2]
#define VIDEO_XSTEP_PER_Y		itech32_video[0x20/2]
#define VIDEO_UNKNOWN22			itech32_video[0x22/2]	/* $0033 at startup */
#define VIDEO_LEFTCLIP			itech32_video[0x24/2]
#define VIDEO_RIGHTCLIP			itech32_video[0x26/2]
#define VIDEO_TOPCLIP			itech32_video[0x28/2]
#define VIDEO_BOTTOMCLIP		itech32_video[0x2a/2]
#define VIDEO_INTSCANLINE		itech32_video[0x2c/2]	/* $00ef at startup */
#define VIDEO_TRANSFER_ADDRHI	itech32_video[0x2e/2]	/* $0000 at startup */

#define VIDEO_UNKNOWN30			itech32_video[0x30/2]	/* $0040 at startup */
#define VIDEO_VTOTAL			itech32_video[0x32/2]	/* $0106 at startup */
#define VIDEO_VSYNC				itech32_video[0x34/2]	/* $0101 at startup */
#define VIDEO_VBLANK_START		itech32_video[0x36/2]	/* $00f3 at startup */
#define VIDEO_VBLANK_END		itech32_video[0x38/2]	/* $0003 at startup */
#define VIDEO_HTOTAL			itech32_video[0x3a/2]	/* $01fc at startup */
#define VIDEO_HSYNC				itech32_video[0x3c/2]	/* $01e4 at startup */
#define VIDEO_HBLANK_START		itech32_video[0x3e/2]	/* $01b2 at startup */
#define VIDEO_HBLANK_END		itech32_video[0x40/2]	/* $0032 at startup */
#define VIDEO_UNKNOWN42			itech32_video[0x42/2]	/* $0015 at startup */
#define VIDEO_DISPLAY_YORIGIN1	itech32_video[0x44/2]	/* $0000 at startup */
#define VIDEO_DISPLAY_YORIGIN2	itech32_video[0x46/2]	/* $0000 at startup */
#define VIDEO_DISPLAY_YSCROLL2	itech32_video[0x48/2]	/* $0000 at startup */
#define VIDEO_UNKNOWN4a			itech32_video[0x4a/2]	/* $0000 at startup */
#define VIDEO_DISPLAY_XORIGIN1	itech32_video[0x4c/2]	/* $0000 at startup */
#define VIDEO_DISPLAY_XORIGIN2	itech32_video[0x4e/2]	/* $0000 at startup */
#define VIDEO_DISPLAY_XSCROLL2	itech32_video[0x50/2]	/* $0000 at startup */
#define VIDEO_UNKNOWN52			itech32_video[0x52/2]	/* $0000 at startup */
#define VIDEO_UNKNOWN54			itech32_video[0x54/2]	/* $0080 at startup */
#define VIDEO_UNKNOWN56			itech32_video[0x56/2]	/* $00c0 at startup */
#define VIDEO_UNKNOWN58			itech32_video[0x58/2]	/* $01c0 at startup */
#define VIDEO_UNKNOWN5a			itech32_video[0x5a/2]	/* $01c0 at startup */
#define VIDEO_UNKNOWN5c			itech32_video[0x5c/2]	/* $01cf at startup */
#define VIDEO_UNKNOWN5e			itech32_video[0x5e/2]	/* $01cf at startup */
#define VIDEO_UNKNOWN60			itech32_video[0x60/2]	/* $01e3 at startup */
#define VIDEO_UNKNOWN62			itech32_video[0x62/2]	/* $01cf at startup */
#define VIDEO_UNKNOWN64			itech32_video[0x64/2]	/* $01ff at startup */
#define VIDEO_UNKNOWN66			itech32_video[0x66/2]	/* $0183 at startup */
#define VIDEO_UNKNOWN68			itech32_video[0x68/2]	/* $01ff at startup */
#define VIDEO_UNKNOWN6a			itech32_video[0x6a/2]	/* $000f at startup */
#define VIDEO_UNKNOWN6c			itech32_video[0x6c/2]	/* $018f at startup */
#define VIDEO_UNKNOWN6e			itech32_video[0x6e/2]	/* $01ff at startup */
#define VIDEO_UNKNOWN70			itech32_video[0x70/2]	/* $000f at startup */
#define VIDEO_UNKNOWN72			itech32_video[0x72/2]	/* $000f at startup */
#define VIDEO_UNKNOWN74			itech32_video[0x74/2]	/* $01ff at startup */
#define VIDEO_UNKNOWN76			itech32_video[0x76/2]	/* $01ff at startup */
#define VIDEO_UNKNOWN78			itech32_video[0x78/2]	/* $01ff at startup */
#define VIDEO_UNKNOWN7a			itech32_video[0x7a/2]	/* $01ff at startup */
#define VIDEO_UNKNOWN7c			itech32_video[0x7c/2]	/* $0820 at startup */
#define VIDEO_UNKNOWN7e			itech32_video[0x7e/2]	/* $0100 at startup */

#define VIDEO_STARTSTEP			itech32_video[0x80/2]	/* drivedge only? */
#define VIDEO_LEFTSTEPLO		itech32_video[0x82/2]	/* drivedge only? */
#define VIDEO_LEFTSTEPHI		itech32_video[0x84/2]	/* drivedge only? */
#define VIDEO_RIGHTSTEPLO		itech32_video[0x86/2]	/* drivedge only? */
#define VIDEO_RIGHTSTEPHI		itech32_video[0x88/2]	/* drivedge only? */

#define VIDEOINT_SCANLINE		0x0004
#define VIDEOINT_BLITTER		0x0040

#define XFERFLAG_TRANSPARENT	0x0001
#define XFERFLAG_XFLIP			0x0002
#define XFERFLAG_YFLIP			0x0004
#define XFERFLAG_DSTXSCALE		0x0008
#define XFERFLAG_DYDXSIGN		0x0010
#define XFERFLAG_DXDYSIGN		0x0020
#define XFERFLAG_UNKNOWN8		0x0100
#define XFERFLAG_CLIP			0x0400
#define XFERFLAG_WIDTHPIX		0x8000

#define XFERFLAG_KNOWNFLAGS		(XFERFLAG_TRANSPARENT | XFERFLAG_XFLIP | XFERFLAG_YFLIP | XFERFLAG_DSTXSCALE | XFERFLAG_DYDXSIGN | XFERFLAG_DXDYSIGN | XFERFLAG_CLIP | XFERFLAG_WIDTHPIX)

#define VRAM_WIDTH				512



//static UINT32 *drivedge_zbuf_control;
static UINT8 itech32_planes;
static UINT16 itech32_vram_height;

static UINT16 xfer_xcount, xfer_ycount;
static UINT16 xfer_xcur, xfer_ycur;

static clip_struct clip_rect, scaled_clip_rect;
static clip_struct clip_save;

static INT32 scanline_timer;

static UINT8 *grom_base;
static UINT32 grom_size;
static UINT32 grom_bank; // scan
static UINT32 grom_bank_mask;

static UINT16 *videoplane[2];
static UINT16 *videoram16;
static UINT32 vram_mask;
static UINT32 vram_xmask, vram_ymask;

static void update_interrupts(INT32 )
{
	INT32 scanline_state = 0, blitter_state = 0;

	if (VIDEO_INTSTATE & VIDEO_INTENABLE & VIDEOINT_SCANLINE)
		scanline_state = 1;
	if (VIDEO_INTSTATE & VIDEO_INTENABLE & VIDEOINT_BLITTER)
		blitter_state = 1;

	itech32_update_interrupts(-1, blitter_state, scanline_state);
}

static void scanline_interrupt()
{
	scanline_timer = VIDEO_INTSCANLINE;

//	bprintf (0, _T("A timer:%d\n"), scanline_timer);

	VIDEO_INTSTATE |= VIDEOINT_SCANLINE;

	update_interrupts(0);
}


#define ADJUSTED_HEIGHT(x) ((((x) >> 1) & 0x100) | ((x) & 0xff))

#define GET_NEXT_RUN(xleft, count, innercount, src)	\
do {												\
	/* load next RLE chunk if needed */				\
	if (!count)										\
	{												\
		count = *src++;								\
		val = (count & 0x80) ? -1 : *src++;			\
		count &= 0x7f;								\
	}												\
													\
	/* determine how much to bite off */			\
	innercount = (xleft > count) ? count : xleft;	\
	count -= innercount;							\
	xleft -= innercount;							\
} while (0)


#define SKIP_RLE(skip, xleft, count, innercount, src)\
do {												\
	/* scan RLE until done */						\
	for (xleft = skip; xleft > 0; )					\
	{												\
		/* load next RLE chunk if needed */			\
		GET_NEXT_RUN(xleft, count, innercount, src);\
													\
		/* skip past the data */					\
		if (val == -1) src += innercount;			\
	}												\
} while (0)


static inline UINT32 compute_safe_address(INT32 x, INT32 y)
{
	return ((y & vram_ymask) * 512) + (x & vram_xmask);
}

static inline void disable_clipping()
{
	clip_save = clip_rect;

	clip_rect.nMinx = clip_rect.nMiny = 0;
	clip_rect.nMaxx = clip_rect.nMaxy = 0xfff;

	scaled_clip_rect.nMinx = scaled_clip_rect.nMiny = 0;
	scaled_clip_rect.nMaxx = scaled_clip_rect.nMaxy = 0xfff << 8;
}

static inline void enable_clipping()
{
	clip_rect = clip_save;

	scaled_clip_rect.nMinx = clip_rect.nMinx << 8;
	scaled_clip_rect.nMaxx = clip_rect.nMaxx << 8;
	scaled_clip_rect.nMiny = clip_rect.nMiny << 8;
	scaled_clip_rect.nMaxy = clip_rect.nMaxy << 8;
}

static void draw_raw(UINT16 *base, UINT16 color)
{
	UINT8* src = &grom_base[0];// UINT8 *src = &grom_base[(grom_bank | ((VIDEO_TRANSFER_ADDRHI & 0xff) << 16) | VIDEO_TRANSFER_ADDRLO) % grom_size];
	const UINT32 grom_start = grom_bank | ((VIDEO_TRANSFER_ADDRHI & 0xff) << 16) | VIDEO_TRANSFER_ADDRLO;
	INT32 transparent_pen = (VIDEO_TRANSFER_FLAGS & XFERFLAG_TRANSPARENT) ? 0xff : -1;
	INT32 width = VIDEO_TRANSFER_WIDTH << 8;
	INT32 height = ADJUSTED_HEIGHT(VIDEO_TRANSFER_HEIGHT) << 8;
	INT32 xsrcstep = VIDEO_SRC_XSTEP;
	INT32 ysrcstep = VIDEO_SRC_YSTEP;
	INT32 sx, sy = (VIDEO_TRANSFER_Y & 0xfff) << 8;
	INT32 startx = (VIDEO_TRANSFER_X & 0xfff) << 8;
	INT32 xdststep = 0x100;
	INT32 ydststep = VIDEO_DST_YSTEP;
	INT32 x, y;

	/* adjust for (lack of) clipping */
	if (!(VIDEO_TRANSFER_FLAGS & XFERFLAG_CLIP))
		disable_clipping();

	/* adjust for scaling */
	if (VIDEO_TRANSFER_FLAGS & XFERFLAG_DSTXSCALE)
		xdststep = VIDEO_DST_XSTEP;

	/* adjust for flipping */
	if (VIDEO_TRANSFER_FLAGS & XFERFLAG_XFLIP)
		xdststep = -xdststep;
	if (VIDEO_TRANSFER_FLAGS & XFERFLAG_YFLIP)
		ydststep = -ydststep;

	/* loop over Y in src pixels */
	for (y = 0; y < height; y += ysrcstep, sy += ydststep)
	{
		const UINT32 row_base = (y >> 8) * (width >> 8);

		/* simpler case: VIDEO_YSTEP_PER_X is zero */
		if (VIDEO_YSTEP_PER_X == 0)
		{
			/* clip in the Y direction */
			if (sy >= scaled_clip_rect.nMiny && sy < scaled_clip_rect.nMaxy)
			{
				UINT32 dstoffs;

				/* direction matters here */
				sx = startx;
				if (xdststep > 0)
				{
					/* skip left pixels */
					for (x = 0; x < width && sx < scaled_clip_rect.nMinx; x += xsrcstep, sx += xdststep) ;

					/* compute the address */
					dstoffs = compute_safe_address(sx >> 8, sy >> 8) - (sx >> 8);

					/* render middle pixels */
					for ( ; x < width && sx < scaled_clip_rect.nMaxx; x += xsrcstep, sx += xdststep)
					{
						INT32 pixel = src[(grom_start + row_base + (x >> 8)) % grom_size];
						if (pixel != transparent_pen)
							base[(dstoffs + (sx >> 8)) & vram_mask] = pixel | color;
					}
				}
				else
				{
					/* skip right pixels */
					for (x = 0; x < width && sx >= scaled_clip_rect.nMaxx; x += xsrcstep, sx += xdststep) ;

					/* compute the address */
					dstoffs = compute_safe_address(sx >> 8, sy >> 8) - (sx >> 8);

					/* render middle pixels */
					for ( ; x < width && sx >= scaled_clip_rect.nMinx; x += xsrcstep, sx += xdststep)
					{
						INT32 pixel = src[(grom_start + row_base + (x >> 8)) % grom_size];
						if (pixel != transparent_pen)
							base[(dstoffs + (sx >> 8)) & vram_mask] = pixel | color;
					}
				}
			}
		}

		/* slow case: VIDEO_YSTEP_PER_X is non-zero */
		else
		{
			INT32 ystep = (VIDEO_TRANSFER_FLAGS & XFERFLAG_DYDXSIGN) ? -VIDEO_YSTEP_PER_X : VIDEO_YSTEP_PER_X;
			INT32 ty = sy;

			/* render all pixels */
			sx = startx;
			for (x = 0; x < width && sx < scaled_clip_rect.nMaxx; x += xsrcstep, sx += xdststep, ty += ystep)
				if (ty >= scaled_clip_rect.nMiny && ty < scaled_clip_rect.nMaxy &&
					sx >= scaled_clip_rect.nMinx && sx < scaled_clip_rect.nMaxx)
				{
					INT32 pixel = src[(grom_start + row_base + (x >> 8)) % grom_size];
					if (pixel != transparent_pen)
						base[compute_safe_address(sx >> 8, ty >> 8)] = pixel | color;
				}
		}

		/* apply skew */
		if (VIDEO_TRANSFER_FLAGS & XFERFLAG_DXDYSIGN)
			startx += VIDEO_XSTEP_PER_Y;
		else
			startx -= VIDEO_XSTEP_PER_Y;
	}

	/* restore cliprects */
	if (!(VIDEO_TRANSFER_FLAGS & XFERFLAG_CLIP))
		enable_clipping();
}

static void draw_raw_widthpix(UINT16 *base, UINT16 color)
{
	UINT8 *src = &grom_base[(grom_bank | ((VIDEO_TRANSFER_ADDRHI & 0xff) << 16) | VIDEO_TRANSFER_ADDRLO) % grom_size];
	INT32 transparent_pen = (VIDEO_TRANSFER_FLAGS & XFERFLAG_TRANSPARENT) ? 0xff : -1;
	INT32 width = VIDEO_TRANSFER_WIDTH << 8;
	INT32 height = ADJUSTED_HEIGHT(VIDEO_TRANSFER_HEIGHT) << 8;
	INT32 xsrcstep = VIDEO_SRC_XSTEP;
	INT32 ysrcstep = VIDEO_SRC_YSTEP;
	INT32 sx, sy = (VIDEO_TRANSFER_Y & 0xfff) << 8;
	INT32 startx = (VIDEO_TRANSFER_X & 0xfff) << 8;
	INT32 xdststep = 0x100;
	INT32 ydststep = VIDEO_DST_YSTEP;
	INT32 x, y, px;

	/* adjust for (lack of) clipping */
	if (!(VIDEO_TRANSFER_FLAGS & XFERFLAG_CLIP))
		disable_clipping();

	/* adjust for scaling */
	if (VIDEO_TRANSFER_FLAGS & XFERFLAG_DSTXSCALE)
		xdststep = VIDEO_DST_XSTEP;

	/* adjust for flipping */
	if (VIDEO_TRANSFER_FLAGS & XFERFLAG_XFLIP)
		xdststep = -xdststep;
	if (VIDEO_TRANSFER_FLAGS & XFERFLAG_YFLIP)
		ydststep = -ydststep;

	/* loop over Y in src pixels */
	for (y = 0; y < height; y += ysrcstep, sy += ydststep)
	{
		UINT8 *rowsrc = &src[(y >> 8) * (width >> 8)];

		x = 0;
		px = 0;

		/* simpler case: VIDEO_YSTEP_PER_X is zero */
		if (VIDEO_YSTEP_PER_X == 0)
		{
			/* clip in the Y direction */
			if (sy >= scaled_clip_rect.nMiny && sy < scaled_clip_rect.nMaxy)
			{
				UINT32 dstoffs;

				/* direction matters here */
				sx = startx;
				if (xdststep > 0)
				{
					/* skip left pixels */
					for ( ; px < width && sx < scaled_clip_rect.nMinx; x += xsrcstep, px += 0x100, sx += xdststep) ;

					/* compute the address */
					dstoffs = compute_safe_address(sx >> 8, sy >> 8) - (sx >> 8);

					/* render middle pixels */
					for ( ; px < width && sx < scaled_clip_rect.nMaxx; x += xsrcstep, px += 0x100, sx += xdststep)
					{
						INT32 pixel = rowsrc[x >> 8];
						if (pixel != transparent_pen)
							base[(dstoffs + (sx >> 8)) & vram_mask] = pixel | color;
					}
				}
				else
				{
					/* skip right pixels */
					for ( ; px < width && sx >= scaled_clip_rect.nMaxx; x += xsrcstep, px += 0x100, sx += xdststep);

					/* compute the address */
					dstoffs = compute_safe_address(sx >> 8, sy >> 8) - (sx >> 8);

					/* render middle pixels */
					for ( ; px < width && sx >= scaled_clip_rect.nMinx; x += xsrcstep, px += 0x100, sx += xdststep)
					{
						INT32 pixel = rowsrc[x >> 8];
						if (pixel != transparent_pen)
							base[(dstoffs + (sx >> 8)) & vram_mask] = pixel | color;
					}
				}
			}
		}

		/* slow case: VIDEO_YSTEP_PER_X is non-zero */
		else
		{
			INT32 ystep = (VIDEO_TRANSFER_FLAGS & XFERFLAG_DYDXSIGN) ? -VIDEO_YSTEP_PER_X : VIDEO_YSTEP_PER_X;
			INT32 ty = sy;

			/* render all pixels */
			sx = startx;
			for ( ; px < width && sx < scaled_clip_rect.nMaxx; x += xsrcstep, px += 0x100, sx += xdststep, ty += ystep)
				if (ty >= scaled_clip_rect.nMiny && ty < scaled_clip_rect.nMaxy &&
					sx >= scaled_clip_rect.nMinx && sx < scaled_clip_rect.nMaxx)
				{
					INT32 pixel = rowsrc[x >> 8];
					if (pixel != transparent_pen)
						base[compute_safe_address(sx >> 8, ty >> 8)] = pixel | color;
				}
		}

		/* apply skew */
		if (VIDEO_TRANSFER_FLAGS & XFERFLAG_DXDYSIGN)
			startx += VIDEO_XSTEP_PER_Y;
		else
			startx -= VIDEO_XSTEP_PER_Y;
	}

	/* restore cliprects */
	if (!(VIDEO_TRANSFER_FLAGS & XFERFLAG_CLIP))
		enable_clipping();
}


static inline void draw_rle_fast(UINT16 *base, UINT16 color)
{
	UINT8 *src = &grom_base[(grom_bank | ((VIDEO_TRANSFER_ADDRHI & 0xff) << 16) | VIDEO_TRANSFER_ADDRLO) % grom_size];
	INT32 transparent_pen = (VIDEO_TRANSFER_FLAGS & XFERFLAG_TRANSPARENT) ? 0xff : -1;
	INT32 width = VIDEO_TRANSFER_WIDTH;
	INT32 height = ADJUSTED_HEIGHT(VIDEO_TRANSFER_HEIGHT);
	INT32 sx = VIDEO_TRANSFER_X & 0xfff;
	INT32 sy = (VIDEO_TRANSFER_Y & 0xfff) << 8;
	INT32 xleft, y, count = 0, val = 0, innercount;
	INT32 ydststep = VIDEO_DST_YSTEP;
	INT32 lclip, rclip;

	/* determine clipping */
	lclip = clip_rect.nMinx - sx;
	if (lclip < 0) lclip = 0;
	rclip = sx + width - clip_rect.nMaxx;
	if (rclip < 0) rclip = 0;
	width -= lclip + rclip;
	sx += lclip;

	/* adjust for flipping */
	if (VIDEO_TRANSFER_FLAGS & XFERFLAG_YFLIP)
		ydststep = -ydststep;

	/* loop over Y in src pixels */
	for (y = 0; y < height; y++, sy += ydststep)
	{
		UINT32 dstoffs;

		/* clip in the Y direction */
		if (sy < scaled_clip_rect.nMiny || sy >= scaled_clip_rect.nMaxy)
		{
			SKIP_RLE(width + lclip + rclip, xleft, count, innercount, src);
			continue;
		}

		/* compute the address */
		dstoffs = compute_safe_address(sx, sy >> 8);

		/* left clip */
		SKIP_RLE(lclip, xleft, count, innercount, src);

		/* loop until gone */
		for (xleft = width; xleft > 0; )
		{
			/* load next RLE chunk if needed */
			GET_NEXT_RUN(xleft, count, innercount, src);

			/* run of literals */
			if (val == -1)
				while (innercount--)
				{
					INT32 pixel = *src++;
					if (pixel != transparent_pen)
						base[dstoffs & vram_mask] = color | pixel;
					dstoffs++;
				}

			/* run of non-transparent repeats */
			else if (val != transparent_pen)
			{
				val |= color;
				while (innercount--)
					base[dstoffs++ & vram_mask] = val;
			}

			/* run of transparent repeats */
			else
				dstoffs += innercount;
		}

		/* right clip */
		SKIP_RLE(rclip, xleft, count, innercount, src);
	}
}


static inline void draw_rle_fast_xflip(UINT16 *base, UINT16 color)
{
	UINT8 *src = &grom_base[(grom_bank | ((VIDEO_TRANSFER_ADDRHI & 0xff) << 16) | VIDEO_TRANSFER_ADDRLO) % grom_size];
	INT32 transparent_pen = (VIDEO_TRANSFER_FLAGS & XFERFLAG_TRANSPARENT) ? 0xff : -1;
	INT32 width = VIDEO_TRANSFER_WIDTH;
	INT32 height = ADJUSTED_HEIGHT(VIDEO_TRANSFER_HEIGHT);
	INT32 sx = VIDEO_TRANSFER_X & 0xfff;
	INT32 sy = (VIDEO_TRANSFER_Y & 0xfff) << 8;
	INT32 xleft, y, count = 0, val = 0, innercount;
	INT32 ydststep = VIDEO_DST_YSTEP;
	INT32 lclip, rclip;

	/* determine clipping */
	lclip = sx - clip_rect.nMaxx;
	if (lclip < 0) lclip = 0;
	rclip = clip_rect.nMinx - (sx - width);
	if (rclip < 0) rclip = 0;
	width -= lclip + rclip;
	sx -= lclip;

	/* adjust for flipping */
	if (VIDEO_TRANSFER_FLAGS & XFERFLAG_YFLIP)
		ydststep = -ydststep;

	/* loop over Y in src pixels */
	for (y = 0; y < height; y++, sy += ydststep)
	{
		UINT32 dstoffs;

		/* clip in the Y direction */
		if (sy < scaled_clip_rect.nMiny || sy >= scaled_clip_rect.nMaxy)
		{
			SKIP_RLE(width + lclip + rclip, xleft, count, innercount, src);
			continue;
		}

		/* compute the address */
		dstoffs = compute_safe_address(sx, sy >> 8);

		/* left clip */
		SKIP_RLE(lclip, xleft, count, innercount, src);

		/* loop until gone */
		for (xleft = width; xleft > 0; )
		{
			/* load next RLE chunk if needed */
			GET_NEXT_RUN(xleft, count, innercount, src);

			/* run of literals */
			if (val == -1)
				while (innercount--)
				{
					INT32 pixel = *src++;
					if (pixel != transparent_pen)
						base[dstoffs & vram_mask] = color | pixel;
					dstoffs--;
				}

			/* run of non-transparent repeats */
			else if (val != transparent_pen)
			{
				val |= color;
				while (innercount--)
					base[dstoffs-- & vram_mask] = val;
			}

			/* run of transparent repeats */
			else
				dstoffs -= innercount;
		}

		/* right clip */
		SKIP_RLE(rclip, xleft, count, innercount, src);
	}
}



/*************************************
 *
 *  Slow compressed blitter functions
 *
 *************************************/

static inline void draw_rle_slow(UINT16 *base, UINT16 color)
{
	UINT8 *src = &grom_base[(grom_bank | ((VIDEO_TRANSFER_ADDRHI & 0xff) << 16) | VIDEO_TRANSFER_ADDRLO) % grom_size];
	INT32 transparent_pen = (VIDEO_TRANSFER_FLAGS & XFERFLAG_TRANSPARENT) ? 0xff : -1;
	INT32 width = VIDEO_TRANSFER_WIDTH;
	INT32 height = ADJUSTED_HEIGHT(VIDEO_TRANSFER_HEIGHT);
	INT32 sx, sy = (VIDEO_TRANSFER_Y & 0xfff) << 8;
	INT32 xleft, y, count = 0, val = 0, innercount;
	INT32 xdststep = 0x100;
	INT32 ydststep = VIDEO_DST_YSTEP;
	INT32 startx = (VIDEO_TRANSFER_X & 0xfff) << 8;

	/* adjust for scaling */
	if (VIDEO_TRANSFER_FLAGS & XFERFLAG_DSTXSCALE)
		xdststep = VIDEO_DST_XSTEP;

	/* adjust for flipping */
	if (VIDEO_TRANSFER_FLAGS & XFERFLAG_XFLIP)
		xdststep = -xdststep;
	if (VIDEO_TRANSFER_FLAGS & XFERFLAG_YFLIP)
		ydststep = -ydststep;

	/* loop over Y in src pixels */
	for (y = 0; y < height; y++, sy += ydststep)
	{
		UINT32 dstoffs;

		/* clip in the Y direction */
		if (sy < scaled_clip_rect.nMiny || sy >= scaled_clip_rect.nMaxy)
		{
			SKIP_RLE(width, xleft, count, innercount, src);
			continue;
		}

		/* compute the address */
		sx = startx;
		dstoffs = compute_safe_address(clip_rect.nMinx, sy >> 8) - clip_rect.nMinx;

		/* loop until gone */
		for (xleft = width; xleft > 0; )
		{
			/* load next RLE chunk if needed */
			GET_NEXT_RUN(xleft, count, innercount, src);

			/* run of literals */
			if (val == -1)
				for ( ; innercount--; sx += xdststep)
				{
					INT32 pixel = *src++;
					if (pixel != transparent_pen)
						if (sx >= scaled_clip_rect.nMinx && sx < scaled_clip_rect.nMaxx)
							base[(dstoffs + (sx >> 8)) & vram_mask] = color | pixel;
				}

			/* run of non-transparent repeats */
			else if (val != transparent_pen)
			{
				val |= color;
				for ( ; innercount--; sx += xdststep)
					if (sx >= scaled_clip_rect.nMinx && sx < scaled_clip_rect.nMaxx)
						base[(dstoffs + (sx >> 8)) & vram_mask] = val;
			}

			/* run of transparent repeats */
			else
				sx += xdststep * innercount;
		}

		/* apply skew */
		if (VIDEO_TRANSFER_FLAGS & XFERFLAG_DXDYSIGN)
			startx += VIDEO_XSTEP_PER_Y;
		else
			startx -= VIDEO_XSTEP_PER_Y;
	}
}



static void draw_rle(UINT16 *base, UINT16 color)
{
	/* adjust for (lack of) clipping */
	if (!(VIDEO_TRANSFER_FLAGS & XFERFLAG_CLIP))
		disable_clipping();

	/* if we have an X scale, draw it slow */
	if (((VIDEO_TRANSFER_FLAGS & XFERFLAG_DSTXSCALE) && VIDEO_DST_XSTEP != 0x100) || VIDEO_XSTEP_PER_Y)
		draw_rle_slow(base, color);

	/* else draw it fast */
	else if (VIDEO_TRANSFER_FLAGS & XFERFLAG_XFLIP)
		draw_rle_fast_xflip(base, color);
	else
		draw_rle_fast(base, color);

	/* restore cliprects */
	if (!(VIDEO_TRANSFER_FLAGS & XFERFLAG_CLIP))
		enable_clipping();
}



/*************************************
 *
 *  Shift register manipulation
 *
 *************************************/

static void shiftreg_clear(UINT16 *base)
{
	INT32 ydir = (VIDEO_TRANSFER_FLAGS & XFERFLAG_YFLIP) ? -1 : 1;
	INT32 height = ADJUSTED_HEIGHT(VIDEO_TRANSFER_HEIGHT);
	INT32 sx = VIDEO_TRANSFER_X & 0xfff;
	INT32 sy = VIDEO_TRANSFER_Y & 0xfff;
	UINT16 *src;
	INT32 y;

	/* first line is the source */
	src = &base[compute_safe_address(sx, sy)];
	sy += ydir;

	/* loop over height */
	for (y = 1; y < height; y++)
	{
		memcpy(&base[compute_safe_address(sx, sy)], src, 512*2);
		sy += ydir;
	}
}



/*************************************
 *
 *  Video commands
 *
 *************************************/

static void handle_video_command(void)
{
	/* only 6 known commands */
	switch (VIDEO_COMMAND)
	{
		/* command 1: blit raw data */
		case 1:
			if (VIDEO_TRANSFER_FLAGS & XFERFLAG_WIDTHPIX) {
				if (enable_latch[0]) draw_raw_widthpix(videoplane[0], color_latch[0]);
				if (enable_latch[1]) draw_raw_widthpix(videoplane[1], color_latch[1]);
			} else {
				if (enable_latch[0]) draw_raw(videoplane[0], color_latch[0]);
				if (enable_latch[1]) draw_raw(videoplane[1], color_latch[1]);
			}
			break;

		/* command 2: blit RLE-compressed data */
		case 2:
			if (enable_latch[0]) draw_rle(videoplane[0], color_latch[0]);
			if (enable_latch[1]) draw_rle(videoplane[1], color_latch[1]);
			break;

		/* command 3: set up raw data transfer */
		case 3:
			xfer_xcount = VIDEO_TRANSFER_WIDTH;
			xfer_ycount = ADJUSTED_HEIGHT(VIDEO_TRANSFER_HEIGHT);
			xfer_xcur = VIDEO_TRANSFER_X & 0xfff;
			xfer_ycur = VIDEO_TRANSFER_Y & 0xfff;
			break;

		/* command 4: flush? */
		case 4:
			break;

		/* command 5: reset? */
		case 5:
			break;

		/* command 6: perform shift register copy */
		case 6:
			if (enable_latch[0]) shiftreg_clear(videoplane[0]);
			if (enable_latch[1]) shiftreg_clear(videoplane[1]);
			break;
	}

	/* tell the processor we're done */
	VIDEO_INTSTATE |= VIDEOINT_BLITTER;
	update_interrupts(1);
}

static void itech32_video_write(INT32 offset, UINT16 old)
{
	//INT32 old = itech32_video[offset];
	//COMBINE_DATA(&itech32_video[offset]);
	UINT16 data = itech32_video[offset];

	switch (offset)
	{
		case 0x02/2:	/* VIDEO_INTACK */
			VIDEO_INTSTATE = old & ~data;
			update_interrupts(1);
		return;

		case 0x04/2:	/* VIDEO_TRANSFER */
			if (VIDEO_COMMAND == 3 && xfer_ycount)
			{
				UINT32 addr = compute_safe_address(xfer_xcur, xfer_ycur);
				if (enable_latch[0])
				{
					VIDEO_TRANSFER = videoplane[0][addr];
					videoplane[0][addr] = (data & 0xff) | color_latch[0];
				}
				if (enable_latch[1])
				{
					VIDEO_TRANSFER = videoplane[1][addr];
					videoplane[1][addr] = (data & 0xff) | color_latch[1];
				}
				if (--xfer_xcount)
					xfer_xcur++;
				else if (--xfer_ycount)
					xfer_xcur = VIDEO_TRANSFER_X, xfer_xcount = VIDEO_TRANSFER_WIDTH, xfer_ycur++;
			}
		return;

		case 0x08/2:	/* VIDEO_COMMAND */
			handle_video_command();
		return;

		case 0x0a/2:	/* VIDEO_INTENABLE */
			update_interrupts(1);
		return;

		case 0x24/2:	/* VIDEO_LEFTCLIP */
			clip_rect.nMinx = VIDEO_LEFTCLIP;
			scaled_clip_rect.nMinx = VIDEO_LEFTCLIP << 8;
		return;

		case 0x26/2:	/* VIDEO_RIGHTCLIP */
			clip_rect.nMaxx = VIDEO_RIGHTCLIP;
			scaled_clip_rect.nMaxx = VIDEO_RIGHTCLIP << 8;
		return;

		case 0x28/2:	/* VIDEO_TOPCLIP */
			clip_rect.nMiny = VIDEO_TOPCLIP;
			scaled_clip_rect.nMiny = VIDEO_TOPCLIP << 8;
		return;

		case 0x2a/2:	/* VIDEO_BOTTOMCLIP */
			clip_rect.nMaxy = VIDEO_BOTTOMCLIP;
			scaled_clip_rect.nMaxy = VIDEO_BOTTOMCLIP << 8;
		return;

		case 0x2c/2:	/* VIDEO_INTSCANLINE */
			scanline_timer = VIDEO_INTSCANLINE;
	//		bprintf (0, _T("B timer:%d\n"), scanline_timer);
		return;

		case 0x32/2:    /* VIDEO_VTOTAL */
		case 0x36/2:    /* VIDEO_VBLANK_START */
		case 0x38/2:    /* VIDEO_VBLANK_END */
		case 0x3a/2:    /* VIDEO_HTOTAL */
		case 0x3e/2:    /* VIDEO_HBLANK_START */
		case 0x40/2:    /* VIDEO_HBLANK_END */
#if 0
			/* do some sanity checks first */
			if ((VIDEO_HTOTAL > 0) && (VIDEO_VTOTAL > 0) &&
				(VIDEO_VBLANK_START != VIDEO_VBLANK_END) &&
				(VIDEO_HBLANK_START != VIDEO_HBLANK_END) &&
				(VIDEO_HBLANK_START < VIDEO_HTOTAL) &&
				(VIDEO_HBLANK_END < VIDEO_HTOTAL) &&
				(VIDEO_VBLANK_START < VIDEO_VTOTAL) &&
				(VIDEO_VBLANK_END < VIDEO_VTOTAL))
			{
				visarea.min_x = visarea.min_y = 0;

				if (VIDEO_HBLANK_START > VIDEO_HBLANK_END)
					visarea.max_x = VIDEO_HBLANK_START - VIDEO_HBLANK_END - 1;
				else
					visarea.max_x = VIDEO_HTOTAL - VIDEO_HBLANK_END + VIDEO_HBLANK_START - 1;

				if (VIDEO_VBLANK_START > VIDEO_VBLANK_END)
					visarea.max_y = VIDEO_VBLANK_START - VIDEO_VBLANK_END - 1;
				else
					visarea.max_y = VIDEO_VTOTAL - VIDEO_VBLANK_END + VIDEO_VBLANK_START - 1;

				m_screen->configure(VIDEO_HTOTAL, VIDEO_VTOTAL, visarea, HZ_TO_ATTOSECONDS(VIDEO_CLOCK) * VIDEO_HTOTAL * VIDEO_VTOTAL);
			}
#endif
			break;
	}

//	bprintf (0, _T("VIDEO MISS!!! %4.4x, %4.4x %4.4x\n"), offset, data, old);
}

static void itech32VideoInit(INT32 vram_height, INT32 video_planes, INT32 gfx_size)
{
	INT32 i;

	itech32_vram_height = vram_height;
	itech32_planes = video_planes;

	/* allocate memory */
	videoram16 = (UINT16*)BurnMalloc(VRAM_WIDTH * (itech32_vram_height + 16) * 2 * 2);
	memset(videoram16, 0xff, VRAM_WIDTH * (itech32_vram_height + 16) * 2 * 2);

	/* videoplane[0] is the foreground; videoplane[1] is the background */
	videoplane[0] = &videoram16[0 * VRAM_WIDTH * (itech32_vram_height + 16) + 8 * VRAM_WIDTH];
	videoplane[1] = &videoram16[1 * VRAM_WIDTH * (itech32_vram_height + 16) + 8 * VRAM_WIDTH];

	/* set the masks */
	vram_mask = VRAM_WIDTH * itech32_vram_height - 1;
	vram_xmask = VRAM_WIDTH - 1;
	vram_ymask = itech32_vram_height - 1;

	/* clear the planes initially */
	for (i = 0; i < VRAM_WIDTH * itech32_vram_height; i++)
		videoplane[0][i] = videoplane[1][i] = 0xff;

	/* fetch the GROM base */
	grom_base = DrvGfxROM;
	grom_size = gfx_size;
	grom_bank = 0;
	grom_bank_mask = grom_size >> 24;
	if (grom_bank_mask == 2)
		grom_bank_mask = 3;

	/* reset statics */
	memset(itech32_video, 0, 0x80);

	enable_latch[0] = 1;
	enable_latch[1] = (itech32_planes > 1) ? 1 : 0;
}

static void itech32copy()
{
	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		UINT16 *src1 = &videoplane[0][compute_safe_address(VIDEO_DISPLAY_XORIGIN1, VIDEO_DISPLAY_YORIGIN1 + y)];
		UINT16 *dst = pTransDraw + y * nScreenWidth;

		if (itech32_planes > 1)
		{
			UINT16 *src2 = &videoplane[1][compute_safe_address(VIDEO_DISPLAY_XORIGIN2 + VIDEO_DISPLAY_XSCROLL2, VIDEO_DISPLAY_YORIGIN2 + VIDEO_DISPLAY_YSCROLL2 + y)];

			/* blend the pixels in the scanline; color xxFF is transparent */
			for (INT32 x = 0; x < nScreenWidth; x++)
			{
				UINT16 pixel = src1[x];
				if ((pixel & 0xff) == 0xff)
					pixel = src2[x];
				dst[x] = pixel;
			}
		}
		/* otherwise, draw directly from VRAM */
		else
		{
			for (INT32 x = 0; x < nScreenWidth; x++)
			{
				dst[x] = src1[x];
			}
		}
	}
}




static inline UINT16 itech32_video_read(INT32 offset)
{
	if (offset == 0) return (video_regs[0] & ~0x08) | 0x05;
	if (offset == 3) return 0xef;

	return video_regs[offset];
}

static void __fastcall timekill_main_write_word(UINT32 address, UINT16 data)
{
//	bprintf (0, _T("MWW: %5.5x, %4.4x\n"), address, data);

	if ((address & 0xffff80) == 0x080000) {
		address = (address/2) & 0x3f;
		UINT16 old = video_regs[address];
		video_regs[address] = data;
		itech32_video_write(address, old);
		return;
	}

	switch (address)
	{
		case 0x050000:
		case 0x050001:
			palette_intensity = (double)(data & 0xff) / (double)0x60;
		return;

		case 0x058000:
		case 0x058001:
			BurnWatchdogWrite();
		return;

		case 0x060000:
		case 0x060001:
			enable_latch[0] = (~data >> 5) & 1;
			enable_latch[1] = (~data >> 7) & 1;
			color_latch[0] = (data & 0x0f) << 8;
		return;

		case 0x068000:
		case 0x068001:
			color_latch[1] = ((data & 0xf0) << 4) | 0x1000;
		return;

		case 0x070000:
		case 0x070001:
			// nop
		return;

		case 0x078000:
		case 0x078001:
			soundlatch = data & 0xff;
			sound_int_state = 1;
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x0a0000:
		case 0x0a0001:
			itech32_update_interrupts(0, -1, -1);
		return;
	}
}
	
static void __fastcall timekill_main_write_byte(UINT32 address, UINT8 data)
{
//	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);

	if ((address & 0xffff80) == 0x080000) {
		UINT8 *r = (UINT8*)video_regs;
		UINT16 old = video_regs[(address/2)&0x3f];
		r[(address & 0x7f)^1] = data;
		itech32_video_write(address, old);
		return;
	}

	switch (address)
	{
		case 0x050000:
		case 0x050001:
			palette_intensity = (double)(data & 0xff) / (double)0x60;
		return;

		case 0x058000:
		case 0x058001:
			BurnWatchdogWrite();
		return;

		case 0x060000:
		case 0x060001:
			enable_latch[0] = (~data >> 5) & 1;
			enable_latch[1] = (~data >> 7) & 1;
			color_latch[0] = (data & 0x0f) << 8;
		return;

		case 0x068000:
		case 0x068001:
			color_latch[1] = ((data & 0xf0) << 4) | 0x1000;
		return;

		case 0x070000:
		case 0x070001:
			// nop
		return;

		case 0x078000:
		case 0x078001:
			soundlatch = data & 0xff;
			sound_int_state = 1;
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x0a0000:
		case 0x0a0001:
			itech32_update_interrupts(0, -1, -1);
		return;
	}
}

static UINT16 __fastcall timekill_main_read_word(UINT32 address)
{
	if (address < 0x40000) return 0; // overflow reads?

//	bprintf (0, _T("MRW: %5.5x\n"), address);

	if ((address & 0xffff80) == 0x080000) {
		return itech32_video_read((address / 2) & 0x3f);
	}

	switch (address)
	{
		case 0x040000:
		case 0x040001:
			return DrvInputs[0];

		case 0x048000:
		case 0x048001:
			return DrvInputs[1];

		case 0x050000:
		case 0x050001:
			return DrvInputs[2];

		case 0x058000:
		case 0x058001: {
			UINT16 ret = (DrvDips[0] & ~0x0e) | 4 | (DrvInputs[3] & 0x02);
			if (vblank) ret ^= 0x04;
			if (sound_int_state) ret ^= 0x08;
			return ret;
		}
	}

	return 0;
}

static UINT8 __fastcall timekill_main_read_byte(UINT32 address)
{
//	bprintf (0, _T("MRB: %5.5x\n"), address);

	if ((address & 0xffff80) == 0x080000) {
		return itech32_video_read((address / 2) & 0x3f) >> ((~address & 1) * 8);
	}

	switch (address)
	{
		case 0x040000:
		case 0x040001:
			return DrvInputs[0];

		case 0x048000:
		case 0x048001:
			return DrvInputs[1];

		case 0x050000:
		case 0x050001:
			return DrvInputs[2];

		case 0x058000:
		case 0x058001: {
			UINT16 ret = (DrvDips[0] & ~0x0e) | 4 | (DrvInputs[3] & 0x02);
			if (vblank) ret ^= 0x04;
			if (sound_int_state) ret ^= 0x08;
			return ret;
		}
	}

	return 0;
}

static void __fastcall common16_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfe0000) == 0x580000) {
		*((UINT16*)(DrvPalRAM + (address & 0x1fffe))) = data;
		return;
	}

	if ((address & 0xffff00) == 0x500000) {
		address = (address / 4) & 0x3f;
		UINT16 old = video_regs[address];
		video_regs[address] = data;
		itech32_video_write(address, old);
		return;
	}

	switch (address)
	{
		case 0x080000:
		case 0x080001:
			itech32_update_interrupts(0, -1, -1);
		return;

		case 0x200000:
		case 0x200001:
		case 0x400000:
		case 0x400001:
			BurnWatchdogWrite();
		return;

		case 0x300000:
		case 0x300001:
			color_latch[0] = (data & 0x7f) << 8;
		return;

		case 0x380000:
		case 0x380001:
			color_latch[1] = (data & 0x7f) << 8;
		return;

		case 0x480000:
		case 0x480001:
			soundlatch = data & 0xff;
			sound_int_state = 1;
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x700000:
		case 0x700001:
			enable_latch[0] = (~data >> 1) & 1;
			enable_latch[1] = (~data >> 2) & 1;
		return;
	}
}
	
static void __fastcall common16_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfe0000) == 0x580000) {
		address &= 0x1ffff;
		if (address & 2) {
			DrvPalRAM[address & ~1] = data;
			DrvPalRAM[address | 1] = data;
		} else {
			DrvPalRAM[address ^ 1] = data;
		}
		return;
	}

//	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);

	if ((address & 0xffff00) == 0x500000) {
		address = ((address & 0xfc)/2) | (address & 1);
		UINT8 *r = (UINT8*)video_regs;
		UINT16 old = video_regs[address/2];
		r[address^1] = data;
		itech32_video_write(address/2, old);
		return;
	}

	switch (address)
	{
		case 0x080000:
		case 0x080001:
			itech32_update_interrupts(0, -1, -1);
		return;

		case 0x200000:
		case 0x200001:
		case 0x400000:
		case 0x400001:
			BurnWatchdogWrite();
		return;

		case 0x300000:
		case 0x300001:
			color_latch[0] = (data & 0x7f) << 8;
		return;

		case 0x380000:
		case 0x380001:
			color_latch[1] = (data & 0x7f) << 8;
		return;

		case 0x480000:
		case 0x480001:
			soundlatch = data & 0xff;
			sound_int_state = 1;
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x700000:
		case 0x700001:
			enable_latch[0] = (~data >> 1) & 1;
			enable_latch[1] = (~data >> 2) & 1;
		return;
	}
}

static UINT16 __fastcall common16_main_read_word(UINT32 address)
{
	if (address < 0x40000) return 0; // overflow reads?

//	bprintf (0, _T("MRW: %5.5x\n"), address);

	if ((address & 0xffff00) == 0x500000) {
		return itech32_video_read((address / 4) & 0x3f);
	}

	switch (address)
	{
		case 0x080000:
			return DrvInputs[0];

		case 0x100000:
			return DrvInputs[1];

		case 0x180000:
			return DrvInputs[2];

		case 0x200000:
			return DrvInputs[3];

		case 0x280000:{
			UINT16 ret = (DrvDips[0] & ~0x0e) | 4 | (DrvInputs[4] & 0x02);
			if (vblank) ret ^= 0x04;
			if (sound_int_state) ret ^= 0x08;
			return ret;
		}

		case 0x680080:
			return *((UINT16*)(Drv68KRAM + 0x111d)); // protection read

		case 0x780000:
			return DrvInputs[5];
	}

	//bprintf (0, _T("MRW: %5.5x\n"), address);

	return 0;
}

static UINT8 wcbowl_track_read(INT32 player); // forward :)

static UINT8 __fastcall common16_main_read_byte(UINT32 address)
{
//	bprintf (0, _T("MRB: %5.5x\n"), address);

	if ((address & 0xffff00) == 0x500000) {
		return itech32_video_read((address / 4) & 0x3f) >> ((~address & 1) * 8);
	}

	// trackball derp
	switch (address)
	{
		case 0x680001:
			if (Trackball_Type == TB_TYPE1)
				return wcbowl_track_read(0);
			else break;
		case 0x680041:
			if (Trackball_Type == TB_TYPE1)
				return wcbowl_track_read(1);
			else break;
	}

	switch (address)
	{
		case 0x080000:
		case 0x080001:
			return DrvInputs[0];

		case 0x100000:
		case 0x100001:
			return DrvInputs[1];

		case 0x180000:
		case 0x180001:
			return DrvInputs[2];

		case 0x200000:
		case 0x200001:
			return DrvInputs[3];

		case 0x280000:
		case 0x280001:{
			UINT16 ret = (DrvDips[0] & ~0x0e) | 4 | (DrvInputs[4] & 0x02);
			if (vblank) ret ^= 0x04;
			if (sound_int_state) ret ^= 0x08;
			return ret;
		}

		case 0x780000:
		case 0x780001:
			return DrvInputs[5];
	}

	//bprintf (0, _T("MRB: %5.5x\n"), address);

	return 0;
}

static void __fastcall common32_main_write_long(UINT32 address, UINT32 data)
{
	if ((address & 0xfff800) == 0x681000) { // timekeeper (in bytehandler)
        SEK_DEF_WRITE_LONG(0, address, data);
		return;
	}

	switch (address)
	{
		case 0x080000:
			itech32_update_interrupts(0, -1, -1);
		return;

		case 0x300000:
			color_latch[0^flip_color_banks] = (data & 0x7f) << 8; // color1 for standard 020 hardware!!
		return;

		case 0x380000:
			color_latch[1^flip_color_banks] = (data & 0x7f) << 8; // color0 for standard 020 hardware!!
		return;

		case 0x400000:
			BurnWatchdogWrite();
		return;

		case 0x480000:
			soundlatch = data & 0xff;
			sound_int_state = 1;
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x700000:
			enable_latch[0] = (~data >> 9) & 1;
			enable_latch[1] = (~data >> 10) & 1;
			grom_bank = ((data >> 14) & grom_bank_mask) << 24;
		return;
	}

	//bprintf (0, _T("MWL: %5.5x, %8.8x\n"), address, data);
}

static void __fastcall common32_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff00) == 0x500000) {
		address = (address & 0xfc) / 4;
		INT32 old = video_regs[address];
		video_regs[address] = data;
		itech32_video_write(address, old);
		return;
	}

	if ((address & 0xfff800) == 0x681000) { // timekeeper (in bytehandler)
        SEK_DEF_WRITE_WORD(0, address, data);
		return;
	}

	switch (address)
	{
		case 0x080000:
		case 0x080002:
			itech32_update_interrupts(0, -1, -1);
		return;

		case 0x300000:
		case 0x300002:
			color_latch[0^flip_color_banks] = (data & 0x7f) << 8; // color1 for standard 020 hardware!!
		return;

		case 0x380000:
		case 0x380002:
			color_latch[1^flip_color_banks] = (data & 0x7f) << 8; // color0 for standard 020 hardware!!
		return;

		case 0x400000:
		case 0x400002:
			BurnWatchdogWrite();
		return;

		case 0x480000:
		case 0x480002:
			soundlatch = data & 0xff;
			sound_int_state = 1;
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x700000:
		case 0x700002:
			enable_latch[0] = (~data >> 9) & 1;
			enable_latch[1] = (~data >> 10) & 1;
			grom_bank = ((data >> 14) & grom_bank_mask) << 24;
		return;
	}

	if ((address&0xffff00) != 0x61ff00)
		bprintf (0, _T("MWW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall common32_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffff00) == 0x500000) {
		bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
		return;
	}

	if ((address & 0xfff800) == 0x681000) {
        TimeKeeperWrite(address & 0x7ff, data);
		return;
	}

	switch (address)
	{
		case 0x080000:
		case 0x080001:
		case 0x080002:
		case 0x080003:
			itech32_update_interrupts(0, -1, -1);
		return;

		case 0x300000:
		case 0x300001:
		case 0x300002:
		case 0x300003:
			color_latch[0^flip_color_banks] = (data & 0x7f) << 8; // color1 for standard 020 hardware!!
		return;

		case 0x380000:
		case 0x380001:
		case 0x380002:
		case 0x380003:
			color_latch[1^flip_color_banks] = (data & 0x7f) << 8; // color0 for standard 020 hardware!!
		return;

		case 0x400000:
		case 0x400001:
		case 0x400002:
		case 0x400003:
			BurnWatchdogWrite();
		return;

		case 0x480000:
		case 0x480001:
		case 0x480002:
		case 0x480003:
			soundlatch = data & 0xff;
			sound_int_state = 1;
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x680002:
		return; // protection nop

		case 0x700000:
		case 0x700001:
		case 0x700002:
		case 0x700003:
			enable_latch[0] = (~data >> 9) & 1;
			enable_latch[1] = (~data >> 10) & 1;
			grom_bank = ((data >> 14) & grom_bank_mask) << 24;
		return;
	}

	if ((address&0xffff00) != 0x61ff00)
		bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static UINT8 wcbowl_track_read(INT32 player)
{
	BurnTrackballUpdate(player);
	return (BurnTrackballRead(player, 0) & 0xf) | ((BurnTrackballRead(player, 1) & 0xf) << 4);
}

static UINT16 track_read_8bit(INT32 player)
{
	BurnTrackballUpdate(player);
	return (BurnTrackballRead(player, 0) & 0xff) | ((BurnTrackballRead(player, 1) & 0xff) << 8);
}

static UINT32 track_read_4bit(INT32 player)
{
	if (tb_last_read[player] != scanline) {
		BurnTrackballUpdate(player);
		INT32 curx = BurnTrackballRead(player, 0);
		INT32 cury = BurnTrackballRead(player, 1);

		INT32 dx = curx - tb_effx[player];
		if (dx < -0x80) dx += 0x100;
		else if (dx > 0x80) dx -= 0x100;
		if (dx > 7) dx = 7;
		else if (dx < -7) dx = -7;
		tb_effx[player] = (tb_effx[player] + dx) & 0xff;
		INT32 lower = tb_effx[player] & 15;

		INT32 dy = cury - tb_effy[player];
		if (dy < -0x80) dy += 0x100;
		else if (dy > 0x80) dy -= 0x100;
		if (dy > 7) dy = 7;
		else if (dy < -7) dy = -7;

		tb_effy[player] = (tb_effy[player] + dy) & 0xff;
		INT32 upper = tb_effy[player] & 15;

		tb_last_result[player] = lower | (upper << 4);
	}

	tb_last_read[player] = scanline;

	return tb_last_result[player] | (tb_last_result[player] << 16);
}

static UINT32 track_read_4bit_both()
{
	return track_read_4bit(0) | (track_read_4bit(1) << 8);
}

static UINT16 DrvGunReturnX(INT32 gun)
{
	return scalerange(BurnGunReturnX(gun), 0x00, 0xff, 0x1c, 0x19b);
}

static UINT32 __fastcall common32_main_read_long(UINT32 address)
{
	if ((address & 0xffff00) == 0x500000) {
		return itech32_video_read((address / 4) & 0x3f) | (itech32_video_read((address / 4) & 0x3f) << 16);
	}

    if ((address & 0xfff800) == 0x681000) { // timekeeper (in bytehandler)
        SEK_DEF_READ_LONG(0, address);
	}

	if (is_shoottv) {
		switch (address)
		{
			case 0x183000: SekSetIRQLine(6, CPU_IRQSTATUS_NONE); return 0;
			case 0x183800: SekSetIRQLine(5, CPU_IRQSTATUS_NONE); return 0;

			case 0x190000: return (DrvGunReturnX(0) & 0x00ff) << 16;
			case 0x190800: return (DrvGunReturnX(0) & 0xff00) << 8;
			case 0x191000: return (BurnGunReturnY(0) & 0x00ff) << 16;
			case 0x191800: return 0;

			case 0x192000: return (DrvGunReturnX(1) & 0x00ff) << 16;
			case 0x192800: return (DrvGunReturnX(1) & 0xff00) << 8;
			case 0x193000: return (BurnGunReturnY(1) & 0x00ff) << 16;
			case 0x193800: return 0;
			case 0x200000: return 0xffffffff;

			case 0x680000: return 0x2000;
		}
	}

	// wcbowl trackball
	switch (address)
	{
		case 0x680000:
			if (Trackball_Type == TB_TYPE1)
				return wcbowl_track_read(0);
			else break;
		case 0x680040:
			if (Trackball_Type == TB_TYPE1)
				return wcbowl_track_read(1);
			else break;
		case 0x180800:
			if (Trackball_Type == TB_TYPE0)
				return track_read_4bit(0);
			else break;
		case 0x181000:
			if (Trackball_Type == TB_TYPE0)
				return track_read_4bit(1);
			else break;
		case 0x200000: // type2 and 4
			if (Trackball_Type == TB_TYPE2)
				return track_read_8bit(0);
			else if (Trackball_Type == TB_TYPE4)
				return track_read_4bit_both();
			else break;
		case 0x200200:
			if (Trackball_Type == TB_TYPE3)
				return track_read_4bit(0);
			else break;
	}

	switch (address)
	{
		case 0x080000:
			return DrvInputs[0];

		case 0x100000:
			return DrvInputs[1];

		case 0x180000:
			return DrvInputs[2];

		case 0x200000:
			if (is_shoottv) return 0xffffffff;
			return DrvInputs[3];

		case 0x280000: {
			UINT8 ret = (DrvDips[0] & ~0x0e) | 0xc | (DrvInputs[4] & 0x02);
			if (vblank) ret ^= 0x04;
			if (sound_int_state) sound_flipper ^= 0x08;
			ret ^= sound_flipper;
			return (ret<<16);
		}

		case 0x680000: {
			bprintf (0, _T("Prot RL\n"));
			if (is_shoottv) return 0x2000;
			UINT32 *ram = (UINT32*)Drv68KRAM;
			UINT8 ret = ram[prot_address / 4] >> ((~prot_address & 3) * 8);
			return ret << 8;
		}
	}

	//bprintf(0, _T("rl %x\n"), address);

	return 0;
}

static UINT16 __fastcall common32_main_read_word(UINT32 address)
{
	if ((address & 0xffff00) == 0x500000) {
		return itech32_video_read((address / 4) & 0x3f);
	}

    if ((address & 0xfff800) == 0x681000) { // timekeeper (in bytehandler)
        SEK_DEF_READ_WORD(0, address);
	}

	// wcbowl trackball
	switch (address)
	{
		case 0x680000:
			if (Trackball_Type == TB_TYPE1)
				return wcbowl_track_read(0);
			else break;
		case 0x680040:
			if (Trackball_Type == TB_TYPE1)
				return wcbowl_track_read(1);
			else break;
		case 0x180800:
		case 0x180802:
			if (Trackball_Type == TB_TYPE0)
				return track_read_4bit(0) & 0xffff;
			else break;
		case 0x181000:
		case 0x181002:
			if (Trackball_Type == TB_TYPE0)
				return track_read_4bit(1) & 0xffff;
			else break;
		case 0x200000:
		case 0x200002:
			if (Trackball_Type == TB_TYPE2)
				return track_read_8bit(0);
			else if (Trackball_Type == TB_TYPE4)
				return track_read_4bit_both();
			else break;
		case 0x200200:
		case 0x200202:
			if (Trackball_Type == TB_TYPE3)
				return track_read_4bit(0);
			else break;
	}

	switch (address)
	{
		case 0x080000:
		case 0x080002:
			return DrvInputs[0];

		case 0x100000:
		case 0x100002:
			return DrvInputs[1];

		case 0x180000:
		case 0x180002:
			return DrvInputs[2];

		case 0x200000:
		case 0x200002:
			return DrvInputs[3];

		case 0x280000:
		case 0x280002:{
			UINT16 ret = (DrvDips[0] & ~0x0e) | 0xc | (DrvInputs[4] & 0x02);
			if (vblank) ret ^= 0x04;
			if (sound_int_state) sound_flipper ^= 0x08;
			ret ^= sound_flipper;
			return ret;
		}

		case 0x680000:
			if (is_shoottv) return 0x0000;
		case 0x680002: {
			if (is_shoottv) return 0x2000;
			UINT32 *ram = (UINT32*)Drv68KRAM;
			UINT8 ret = ram[prot_address / 4] >> ((~prot_address & 3) * 8);
			return ret << 8;
		}
	}

	//bprintf(0, _T("rw %x\n"), address);

	return 0;
}

static UINT8 __fastcall common32_main_read_byte(UINT32 address)
{
	if ((address & 0xffff00) == 0x500000) {
		return itech32_video_read((address / 4) & 0x3f) >> ((~address & 1) * 8);
	}

	if ((address & 0xfff800) == 0x681000) {
		return TimeKeeperRead(address & 0x7ff);
	}

	// trackball derp
	switch (address)
	{
		case 0x680001:
			if (Trackball_Type == TB_TYPE1)
				return wcbowl_track_read(0);
			else break;
		case 0x680041:
			if (Trackball_Type == TB_TYPE1)
				return wcbowl_track_read(1);
			else break;
		case 0x180800:
		case 0x180801:
		case 0x180802:
		case 0x180803:
			if (Trackball_Type == TB_TYPE0)
				return track_read_4bit(0) >> (((~address)&3) * 8);
			else break;
		case 0x181000:
		case 0x181001:
		case 0x181002:
		case 0x181003:
			if (Trackball_Type == TB_TYPE0)
				return track_read_4bit(1) >> (((~address)&3) * 8);
			else break;
		case 0x200000:
		case 0x200001:
		case 0x200002:
		case 0x200003:
			if (Trackball_Type == TB_TYPE2)
				return track_read_8bit(0) >> (((~address)&3) * 8);
			else if (Trackball_Type == TB_TYPE4)
				return track_read_4bit_both() >> (((~address)&3) * 8);
			else break;
		case 0x200200:
		case 0x200201:
		case 0x200202:
		case 0x200203:
			if (Trackball_Type == TB_TYPE3)
				return track_read_4bit(0) >> (((~address)&3) * 8);
			else break;
	}

	switch (address)
	{
		case 0x080000:
		case 0x080001:
		case 0x080002:
		case 0x080003:
			return DrvInputs[0];

		case 0x100000:
		case 0x100001:
		case 0x100002:
		case 0x100003:
			return DrvInputs[1];

		case 0x180000:
		case 0x180001:
		case 0x180002:
		case 0x180003:
			return DrvInputs[2];

		case 0x200000:
		case 0x200001:
		case 0x200002:
		case 0x200003:
			return DrvInputs[3];

		case 0x280000:
		case 0x280001:
		case 0x280002:
		case 0x280003:{
			UINT16 ret = (DrvDips[0] & ~0x0e) | 0xc | (DrvInputs[4] & 0x02);
			if (vblank) ret ^= 0x04;
			if (sound_int_state) sound_flipper ^= 0x08;
			ret ^= sound_flipper;
			return ret;
		}

		case 0x680000:
			if (is_shoottv) return 0x00;
		case 0x680001:
			if (is_shoottv) return 0x00;
		case 0x680002: {
			if (is_shoottv) return 0x20;
			UINT32 *ram = (UINT32*)Drv68KRAM;
			UINT32 ret = (ram[prot_address/4] << 16) | (ram[prot_address/4] >> 16);
		//	bprintf (0, _T("Prot RB %8.8x\n"), (ret >> ((~prot_address & 3) * 8))&0xff);
			return ret >> ((~prot_address & 3) * 8);
		}
	}

	if (is_shoottv && address >= 0x183000 && address <= 0x200003) {
		return (common32_main_read_long(address & ~3) >> ((~address & 3) * 8)) & 0xff;
	}

	//bprintf (0, _T("MRB: %5.5x\n"), address);

	return 0;
}

static void bankswitch(INT32 data)
{
	INT32 bankaddress = (data & 0xf) * 0x4000;

	sound_bank = data;

	M6809MapMemory(DrvM6809ROM + bankaddress, 0x4000, 0x7fff, MAP_ROM);
}

static void itech32_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff40) == 0x0800) {
		ES5506Write(address & 0x3f, data);
		return;
	}

	if ((address & 0xfff0) == 0x1400) {
		M6809SetIRQLine(1, CPU_IRQSTATUS_NONE); // 020 type only!!!!! - iq_132
		// via_0_w
		return;
	}

	switch (address)
	{
		case 0x0000:
			sound_return = data;
		return;

		case 0x0c00:
			bankswitch(data);
		return;

		case 0x1000:
			// nop
		return;
	}
}

static UINT8 itech32_sound_read(UINT16 address)
{
	if ((address & 0xff40) == 0x0800) {
		return ES5506Read(address & 0x3f);
	}

	if ((address & 0xfff0) == 0x1400) {
		// via_0_r
		return 0;
	}

	switch (address)
	{
		case 0x0000: // type 020 only!
		case 0x0400:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			sound_int_state = 0;
			return soundlatch;

		case 0x1800:
			return 0; // sound data buffer?
	}

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(DrvPalRAM, 0, 0x20000);
		memset(DrvM6809RAM, 0, 0x2000);
		memset(video_regs, 0, 0x80);
		if (is_16bit == 0) {
			// w/16bit games: Drv68KRAM is also NVRAM :)
			memset(Drv68KRAM, 0, 0x10000);
		}
	}

	memcpy (Drv68KRAM, Drv68KROM, 0x80); // copy vectors (before 68k RESET!!)

	SekOpen(0);
	SekReset();
	SekClose();

	M6809Open(0);
	bankswitch(0);
	M6809Reset();
	M6809Close();

	ES5506Reset();

	vint_state = 0;
	xint_state = 0;
	qint_state = 0;
	sound_int_state = 0;

	soundlatch = 0;
	sound_return = 0;
	sound_flipper = 0;

	enable_latch[0] = enable_latch[1] = 0;
	color_latch[0] = color_latch[1] = 0;
	palette_intensity = 1.0;

	video_reinitialize = 0;

	memset(tb_last_read, 0, sizeof(tb_last_read));
	memset(tb_last_result, 0, sizeof(tb_last_result));
	memset(tb_effx, 0, sizeof(tb_effx));
	memset(tb_effy, 0, sizeof(tb_effy));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += n68KROMLen;
	DrvM6809ROM			= Next; Next += 0x040000;

	DrvGfxROM			= Next; Next += nGfxROMLen;

	DrvSndROM0 			= Next;

	if (nSndROMLen[0]) {
		DrvSndROM1      = Next;
		DrvSndROM2 		= Next;
		DrvSndROM3 		= Next;
		DrvSndROM0		= Next; Next += 0x400000;
	}
	if (nSndROMLen[1]) {
		DrvSndROM2 		= Next;
		DrvSndROM3 		= Next;
		DrvSndROM1		= Next; Next += 0x400000;
	}
	if (nSndROMLen[2]) {
		DrvSndROM3		= Next;
		DrvSndROM2		= Next; Next += 0x400000;
	}
	if (nSndROMLen[3]) {
		DrvSndROM3		= Next; Next += 0x400000;
	}

	DrvPalette			= (UINT32*)Next; Next += 0x8000 * sizeof(UINT32);

	DrvNVRAM			= Next; Next += 0x020000;

	AllRam				= Next;

	DrvPalRAM			= Next; Next += 0x020000;
	DrvM6809RAM			= Next; Next += 0x002000;

	video_regs			= (UINT16*)Next; Next += 0x00080;

	// keep this here!!!
	Drv68KRAM			= Next; Next += 0x010000; // +4 for cheap hack

	RamEnd				= Next;

						Next += 4; // cheap hack for some security setups (overflow 68k RAM!!)

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGetRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri, ti;
	UINT8 *p68KLoad = Drv68KROM;
	UINT8 *p6809Load = DrvM6809ROM;
	UINT8 *pGfxLoad = DrvGfxROM;
	UINT8 *pSndLoad[4] = { DrvSndROM0, DrvSndROM1, DrvSndROM2, DrvSndROM3 };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 1) {
			BurnDrvGetRomInfo(&ti, i+3);
			if ((ti.nType & BRF_PRG) && (ti.nType & 0xf) == 1) { // 32-BIT
				if (bLoad) {
					if (BurnLoadRom(p68KLoad + 1, i + 0, 4)) return 1;
					if (BurnLoadRom(p68KLoad + 0, i + 1, 4)) return 1;
					if (BurnLoadRom(p68KLoad + 3, i + 2, 4)) return 1;
					if (BurnLoadRom(p68KLoad + 2, i + 3, 4)) return 1;
				}
				i+=3;
				p68KLoad += ri.nLen * 4;
			} else { // 16-BIT
				if (bLoad) {
					if (BurnLoadRom(p68KLoad + 1, i + 0, 2)) return 1;
					if (BurnLoadRom(p68KLoad + 0, i + 1, 2)) return 1;
				}
				i++;
				p68KLoad += ri.nLen * 2;
			}
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 2) {
			if (bLoad) {
				if (BurnLoadRom(p6809Load, i, 1)) return 1;
			}
			p6809Load += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 3) {
			if (bLoad) {
				if (BurnLoadRom(pGfxLoad + 0, i+0, 4)) return 1;
				if (BurnLoadRom(pGfxLoad + 1, i+1, 4)) return 1;
				if (BurnLoadRom(pGfxLoad + 2, i+2, 4)) return 1;
				if (BurnLoadRom(pGfxLoad + 3, i+3, 4)) return 1;
			}
			i+=3;
			pGfxLoad += ri.nLen * 4;
			continue;
		}

		if ((ri.nType & BRF_SND) && (ri.nType & 0x0c) == 4) { //4-7
			INT32 bank = ri.nType & 3;
			if (bLoad) {
				if (BurnLoadRom(pSndLoad[bank] + 1, i, 2)) return 1;
			}
			if (is_pubball || is_shoottv) {
				pSndLoad[bank] += 0x200000; // non-pow2 snd rom sizes need padding
			} else {
				if (nSndROMLen[1] || is_shufshot) {
					// wcbowl,wcbowldx,wcbowl{140,165,161,16} have bank1 and 0x200000 spacing
					// shufshot has 0x200000 spacing but only bank0
					pSndLoad[bank] += 0x200000;
				} else {
					pSndLoad[bank] += ri.nLen * 2;
				}
			}
			continue;
		}
	}

	n68KROMLen = p68KLoad - Drv68KROM;
	nGfxROMLen = pGfxLoad - DrvGfxROM;
	nSndROMLen[0] = pSndLoad[0] - DrvSndROM0;
	nSndROMLen[1] = pSndLoad[1] - DrvSndROM1;
	nSndROMLen[2] = pSndLoad[2] - DrvSndROM2;
	nSndROMLen[3] = pSndLoad[3] - DrvSndROM3;

	if (bLoad) {
		if ((p6809Load - DrvM6809ROM) == 0x20000) {
			memcpy (p6809Load, DrvM6809ROM, 0x20000);
		}
	}

	return 0;
}

static void CommonSoundInit()
{
	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,				0x2000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0x38000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(itech32_sound_write);
	M6809SetReadHandler(itech32_sound_read);
	M6809Close();

	ES5506Init(16000000, DrvSndROM0, DrvSndROM1, DrvSndROM2, DrvSndROM3, NULL);
	ES5506SetRoute(0, 1.00, BURN_SND_ES5506_ROUTE_BOTH);
}

static INT32 TimekillInit()
{
	DrvGetRoms(false);

	BurnAllocMemIndex();

	if (DrvGetRoms(true)) return 1;

	maincpu_clock = 12000000;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRAM,					0x000000, 0x003fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,					0x0c0000, 0x0c7fff, MAP_RAM);
	SekMapMemory(Drv68KROM,					0x100000, 0x100000 + (n68KROMLen-1), MAP_ROM);
	SekSetWriteWordHandler(0,				timekill_main_write_word);
	SekSetWriteByteHandler(0,				timekill_main_write_byte);
	SekSetReadWordHandler(0,				timekill_main_read_word);
	SekSetReadByteHandler(0,				timekill_main_read_byte);
	SekClose();

	TimeKeeperInit(TIMEKEEPER_M48T02, NULL); // not on this hardware (32-bit only!)
	BurnWatchdogInit(DrvDoReset, 180);
	BurnTrackballInit(2);
	BurnTrackballSetVelocityCurve(1); // logarithmic curve

	CommonSoundInit();

	GenericTilesInit();

	itech32VideoInit(512, 2, nGfxROMLen);

	is_16bit = 1;

	DrvDoReset(1);

	return 0;
}

static INT32 Common16BitInit()
{
	DrvGetRoms(false);

	BurnAllocMemIndex();

	if (DrvGetRoms(true)) return 1;

	maincpu_clock = 12000000;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRAM,					0x000000, 0x00ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,					0x580000, 0x59ffff, MAP_ROM); // handler
	for (INT32 i = 0; i < 0x800000; i+=0x80000) {		// mirrors
		SekMapMemory(Drv68KROM,				0x800000 + i, 0x800000 + (n68KROMLen-1) + i, MAP_ROM);
	}
	SekSetWriteWordHandler(0,				common16_main_write_word);
	SekSetWriteByteHandler(0,				common16_main_write_byte);
	SekSetReadWordHandler(0,				common16_main_read_word);
	SekSetReadByteHandler(0,				common16_main_read_byte);
	SekClose();

	TimeKeeperInit(TIMEKEEPER_M48T02, NULL); // not on this hardware (32-bit only!)
	BurnWatchdogInit(DrvDoReset, 180);
	BurnTrackballInit(2);
	BurnTrackballSetVelocityCurve(1); // logarithmic curve

	CommonSoundInit();

	GenericTilesInit();

	itech32VideoInit(1024, 1, nGfxROMLen);

	is_16bit = 1;

	DrvDoReset(1);

	return 0;
}

static INT32 Common32BitInit(UINT32 prot_addr, INT32 plane_num, INT32 color_bank_flip)
{
	DrvGetRoms(false);

	BurnAllocMemIndex();

	if (DrvGetRoms(true)) return 1;

	maincpu_clock = 25000000;
	prot_address = prot_addr; // sftm!
	flip_color_banks = color_bank_flip;

	SekInit(0, 0x68ec020);
	SekOpen(0);
	SekMapMemory(Drv68KRAM,					0x000000, 0x007fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,					0x580000, 0x59ffff, MAP_RAM);
	SekMapMemory(DrvNVRAM,					0x600000, 0x61ffff, MAP_RAM);
	SekMapMemory(Drv68KROM,					0x800000, 0x800000 + (n68KROMLen-1), MAP_ROM);
	SekSetWriteLongHandler(0,				common32_main_write_long);
	SekSetWriteWordHandler(0,				common32_main_write_word);
	SekSetWriteByteHandler(0,				common32_main_write_byte);
	SekSetReadLongHandler(0,				common32_main_read_long);
	SekSetReadWordHandler(0,				common32_main_read_word);
	SekSetReadByteHandler(0,				common32_main_read_byte);
	SekClose();

	TimeKeeperInit(TIMEKEEPER_M48T02, NULL);
	BurnWatchdogInit(DrvDoReset, 180);

	if (is_shoottv) {
		BurnGunInit(2, true);
	} else {
		BurnTrackballInit(2);
		BurnTrackballSetVelocityCurve(1); // logarithmic curve
	}

	CommonSoundInit();

	GenericTilesInit();

	itech32VideoInit(1024, plane_num, nGfxROMLen);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	maincpu_clock = 0;
	flip_color_banks = 0;

	GenericTilesExit();

	SekExit();
	M6809Exit();
	ES5506Exit();
	TimeKeeperExit();

	if (is_shoottv) {
		BurnGunExit();
	} else {
		BurnTrackballExit();
	}

	BurnFreeMemIndex();
	BurnFree (videoram16);

	Trackball_Type = -1;

	is_shufshot = 0;
	is_pubball = 0;
	is_shoottv = 0;
	is_16bit = 0;

	return 0;
}

static void PaletteUpdate16()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries()*2; i+=2)
	{
		UINT8 r = BURN_ENDIAN_SWAP_INT16(p[i]);
		UINT8 g = BURN_ENDIAN_SWAP_INT16(p[i]) >> 8;
		UINT8 b = BURN_ENDIAN_SWAP_INT16(p[i+1]) >> 8;

		r = (UINT8)(r * palette_intensity);
		g = (UINT8)(g * palette_intensity);
		b = (UINT8)(b * palette_intensity);

		DrvPalette[i/2] = BurnHighCol(r,g,b,0);
	}
}

static void PaletteUpdate32()
{
	UINT32 *p = (UINT32*)DrvPalRAM;

	for (INT32 i = 0; i < 0x20000/4; i++)
	{
		UINT8 r,g,b;

		r = BURN_ENDIAN_SWAP_INT32(p[i]) >>  0;
		g = BURN_ENDIAN_SWAP_INT32(p[i]) >> 24;
		b = BURN_ENDIAN_SWAP_INT32(p[i]) >> 16;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		PaletteUpdate16();
		DrvRecalc = 1;		// force update
	}

	itech32copy();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDraw32()
{
	if (DrvRecalc) {
		PaletteUpdate32();
		DrvRecalc = 1;		// force update
	}

	itech32copy();

	BurnTransferCopy(DrvPalette);
	BurnGunDrawTargets();

	return 0;
}

static INT32 DrvFrame()
{
	if ((GetCurrentFrame() % 60) == 0) {
		TimeKeeperTick();
	}

	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	SekNewFrame();
	M6809NewFrame();

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

		if (Trackball_Type != -1) {
			BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
			BurnTrackballFrame(0, DrvAnalogPort0, DrvAnalogPort1, 0x01, 0x20);

			BurnTrackballConfig(1, AXIS_REVERSED, AXIS_NORMAL);
			BurnTrackballFrame(1, DrvAnalogPort2, DrvAnalogPort3, 0x01, 0x20);
        }

		if (is_shoottv) {
			BurnGunMakeInputs(0, DrvAnalogPort0, DrvAnalogPort1);
			BurnGunMakeInputs(1, DrvAnalogPort2, DrvAnalogPort3);
		}

        DrvDips[0] = (DrvDips[0] & ~1) | (~DrvSvc0[0] & 1); // F2 (svc mode)
	}

	INT32 nInterleave = (maincpu_clock == 25000000) ? 286 : 262;
	INT32 nCyclesTotal[2] = { maincpu_clock / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	M6809Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		if (i == scanline_timer) {
			scanline_interrupt();
		}

		if (is_shoottv) {
			if ((i & 0x1f) == 0x00) {
				SekSetIRQLine(5, CPU_IRQSTATUS_ACK);
			}
			if ((i & 0x1f) == 0x10) {
				SekSetIRQLine(6, CPU_IRQSTATUS_ACK);
			}
		}

		CPU_RUN(0, Sek);
		CPU_RUN(1, M6809);

		// iq_132 -- hack!!! until we have via emulation!
		if ((i % (nInterleave/4))==((nInterleave/4)-1)) M6809SetIRQLine(1, CPU_IRQSTATUS_ACK);

		if (i == (nScreenHeight - 1)) {
			vblank = 1;
			itech32_update_interrupts(1, -1, -1);

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}
	}

	M6809Close();
	SekClose();

	if (pBurnSoundOut) {
		ES5506Update(pBurnSoundOut, nBurnSoundLen);
		BurnSoundSwapLR(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin =  0x029702;
	}

	SekOpen(0);

	if (nAction & ACB_MEMORY_ROM) {	
		ba.Data		= Drv68KROM;
		ba.nLen		= 0x400000;
		ba.nAddress	= 0x100000;
		ba.szName	= "68K ROM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_MEMORY_RAM) {	
		ba.Data		= Drv68KRAM;
		ba.nLen		= 0x010000;
		ba.nAddress	= 0x000000;
		ba.szName	= "68K RAM";
		BurnAcb(&ba);

		ba.Data		= (UINT8*)videoram16;
		ba.nLen		= VRAM_WIDTH * (itech32_vram_height + 16) * 2 * 2;
		ba.nAddress	= 0x000000;
		ba.szName	= "Video RAM";
		BurnAcb(&ba);

		ba.Data		= (UINT8*)video_regs;
		ba.nLen		= 0x000080;
		ba.nAddress	= 0x080000;
		ba.szName	= "Video Regs";
		BurnAcb(&ba);

		ba.Data		= DrvPalRAM;
		ba.nLen		= 0x020000;
		ba.nAddress	= 0x0c0000;
		ba.szName	= "Palette RAM";
		BurnAcb(&ba);

		ba.Data		= DrvM6809RAM;
		ba.nLen		= 0x002000;
		ba.nAddress	= 0xf00000;
		ba.szName	= "M6809 RAM";
		BurnAcb(&ba);
	}

	SekClose();

	if (nAction & ACB_DRIVER_DATA)
	{	
		SekScan(nAction);
		M6809Scan(nAction);

		ES5506Scan(nAction, pnMin);
		BurnTrackballScan();
		if (is_shoottv) BurnGunScan();

		SCAN_VAR(vint_state);
		SCAN_VAR(xint_state);
		SCAN_VAR(qint_state);
		SCAN_VAR(sound_int_state);
		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_return);
		SCAN_VAR(enable_latch);
		SCAN_VAR(color_latch);
		SCAN_VAR(palette_intensity);
		SCAN_VAR(sound_bank);

		SCAN_VAR(grom_bank);
		SCAN_VAR(xfer_xcount);
		SCAN_VAR(xfer_ycount);
		SCAN_VAR(xfer_xcur);
		SCAN_VAR(xfer_ycur);

		SCAN_VAR(clip_rect);
		SCAN_VAR(scaled_clip_rect);
		SCAN_VAR(clip_save);
		SCAN_VAR(scanline_timer);

		SCAN_VAR(tb_last_read);
		SCAN_VAR(tb_last_result);
		SCAN_VAR(tb_effx);
		SCAN_VAR(tb_effy);
	}

	if (nAction & ACB_NVRAM) {
		if (is_16bit) {
			ScanVar(Drv68KRAM, 0x10000, "NV RAM");
		} else {
			ScanVar(DrvNVRAM,  (is_pubball || is_shoottv) ? 0x20000 : 0x04000, "NV RAM");
		}
	}

	if (nAction & ACB_WRITE)
	{
		M6809Open(0);
		bankswitch(sound_bank);
		M6809Close();
	}

	TimeKeeperScan(nAction);

 	return 0;
}



// Time Killers (v1.32)

static struct BurnRomInfo timekillRomDesc[] = {
	{ "tk00_v1.32_u54.u54",					0x040000, 0x68c74b81, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "tk01_v1.32_u53.u53",					0x040000, 0x2158d8ef, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tk_snd_v_4.1_u17.u17",				0x020000, 0xc699af7b, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "time_killers_0.rom0",				0x200000, 0x94cbf6f8, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "time_killers_1.rom1",				0x200000, 0xc07dea98, 3 | BRF_GRA },           //  4
	{ "time_killers_2.rom2",				0x200000, 0x183eed2a, 3 | BRF_GRA },           //  5
	{ "time_killers_3.rom3",				0x200000, 0xb1da1058, 3 | BRF_GRA },           //  6
	{ "timekill_grom01.grom1",				0x020000, 0xb030c3d9, 3 | BRF_GRA },           //  7
	{ "timekill_grom02.grom2",				0x020000, 0xe98492a4, 3 | BRF_GRA },           //  8
	{ "timekill_grom03.grom3",				0x020000, 0x6088fa64, 3 | BRF_GRA },           //  9
	{ "timekill_grom04.grom4",				0x020000, 0x95be2318, 3 | BRF_GRA },           // 10

	{ "tksrom00_u18.u18",					0x080000, 0x79d8b83a, 6 | BRF_SND },           // 11 Ensoniq Bank 2
	{ "tksrom01_u20.u20",					0x080000, 0xec01648c, 6 | BRF_SND },           // 12
	{ "tksrom02_u26.u26",					0x080000, 0x051ced3e, 6 | BRF_SND },           // 13
};

STD_ROM_PICK(timekill)
STD_ROM_FN(timekill)

struct BurnDriver BurnDrvTimekill = {
	"timekill", NULL, NULL, NULL, "1992",
	"Time Killers (v1.32)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, timekillRomInfo, timekillRomName, NULL, NULL, NULL, NULL, TimekillInputInfo, TimekillDIPInfo,
	TimekillInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 240, 4, 3
};


// Time Killers (v1.32I)

static struct BurnRomInfo timekill132iRomDesc[] = {
	{ "tk00_v1.32i_u54.u54",				0x040000, 0x6cef5e8c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "tk01_v1.32i_u53.u53",				0x040000, 0x3360f6a3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tk_snd_v_4.1_u17.u17",				0x020000, 0xc699af7b, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "time_killers_0.rom0",				0x200000, 0x94cbf6f8, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "time_killers_1.rom1",				0x200000, 0xc07dea98, 3 | BRF_GRA },           //  4
	{ "time_killers_2.rom2",				0x200000, 0x183eed2a, 3 | BRF_GRA },           //  5
	{ "time_killers_3.rom3",				0x200000, 0xb1da1058, 3 | BRF_GRA },           //  6
	{ "timekill_grom01.grom1",				0x020000, 0xb030c3d9, 3 | BRF_GRA },           //  7
	{ "timekill_grom02.grom2",				0x020000, 0xe98492a4, 3 | BRF_GRA },           //  8
	{ "timekill_grom03.grom3",				0x020000, 0x6088fa64, 3 | BRF_GRA },           //  9
	{ "timekill_grom04.grom4",				0x020000, 0x95be2318, 3 | BRF_GRA },           // 10

	{ "tksrom00_u18.u18",					0x080000, 0x79d8b83a, 6 | BRF_SND },           // 11 Ensoniq Bank 2
	{ "tksrom01_u20.u20",					0x080000, 0xec01648c, 6 | BRF_SND },           // 12
	{ "tksrom02_u26.u26",					0x080000, 0x051ced3e, 6 | BRF_SND },           // 13
};

STD_ROM_PICK(timekill132i)
STD_ROM_FN(timekill132i)

struct BurnDriver BurnDrvTimekill132i = {
	"timekill132i", "timekill", NULL, NULL, "1992",
	"Time Killers (v1.32I)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, timekill132iRomInfo, timekill132iRomName, NULL, NULL, NULL, NULL, TimekillInputInfo, TimekillDIPInfo,
	TimekillInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 240, 4, 3
};


// Time Killers (v1.31)

static struct BurnRomInfo timekill131RomDesc[] = {
	{ "tk00_v1.31_u54.u54",					0x040000, 0xe09ae32b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "tk01_v1.31_u53.u53",					0x040000, 0xc29137ec, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "timekillsnd_u17.u17",				0x020000, 0xab1684c3, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "time_killers_0.rom0",				0x200000, 0x94cbf6f8, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "time_killers_1.rom1",				0x200000, 0xc07dea98, 3 | BRF_GRA },           //  4
	{ "time_killers_2.rom2",				0x200000, 0x183eed2a, 3 | BRF_GRA },           //  5
	{ "time_killers_3.rom3",				0x200000, 0xb1da1058, 3 | BRF_GRA },           //  6
	{ "timekill_grom01.grom1",				0x020000, 0xb030c3d9, 3 | BRF_GRA },           //  7
	{ "timekill_grom02.grom2",				0x020000, 0xe98492a4, 3 | BRF_GRA },           //  8
	{ "timekill_grom03.grom3",				0x020000, 0x6088fa64, 3 | BRF_GRA },           //  9
	{ "timekill_grom04.grom4",				0x020000, 0x95be2318, 3 | BRF_GRA },           // 10

	{ "tksrom00_u18.u18",					0x080000, 0x79d8b83a, 6 | BRF_SND },           // 11 Ensoniq Bank 2
	{ "tksrom01_u20.u20",					0x080000, 0xec01648c, 6 | BRF_SND },           // 12
	{ "tksrom02_u26.u26",					0x080000, 0x051ced3e, 6 | BRF_SND },           // 13
};

STD_ROM_PICK(timekill131)
STD_ROM_FN(timekill131)

struct BurnDriver BurnDrvTimekill131 = {
	"timekill131", "timekill", NULL, NULL, "1992",
	"Time Killers (v1.31)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, timekill131RomInfo, timekill131RomName, NULL, NULL, NULL, NULL, TimekillInputInfo, TimekillDIPInfo,
	TimekillInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 240, 4, 3
};


// Time Killers (v1.21)

static struct BurnRomInfo timekill121RomDesc[] = {
	{ "tk00_v1.21_u54.u54",					0x040000, 0x4938a940, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "tk01_v1.21_u53.u53",					0x040000, 0x0bb75c40, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "timekillsnd_u17.u17",				0x020000, 0xab1684c3, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "time_killers_0.rom0",				0x200000, 0x94cbf6f8, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "time_killers_1.rom1",				0x200000, 0xc07dea98, 3 | BRF_GRA },           //  4
	{ "time_killers_2.rom2",				0x200000, 0x183eed2a, 3 | BRF_GRA },           //  5
	{ "time_killers_3.rom3",				0x200000, 0xb1da1058, 3 | BRF_GRA },           //  6
	{ "timekill_grom01.grom1",				0x020000, 0xb030c3d9, 3 | BRF_GRA },           //  7
	{ "timekill_grom02.grom2",				0x020000, 0xe98492a4, 3 | BRF_GRA },           //  8
	{ "timekill_grom03.grom3",				0x020000, 0x6088fa64, 3 | BRF_GRA },           //  9
	{ "timekill_grom04.grom4",				0x020000, 0x95be2318, 3 | BRF_GRA },           // 10

	{ "tksrom00_u18.u18",					0x080000, 0x79d8b83a, 6 | BRF_SND },           // 11 Ensoniq Bank 2
	{ "tksrom01_u20.u20",					0x080000, 0xec01648c, 6 | BRF_SND },           // 12
	{ "tksrom02_u26.u26",					0x080000, 0x051ced3e, 6 | BRF_SND },           // 13
};

STD_ROM_PICK(timekill121)
STD_ROM_FN(timekill121)

struct BurnDriver BurnDrvTimekill121 = {
	"timekill121", "timekill", NULL, NULL, "1992",
	"Time Killers (v1.21)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, timekill121RomInfo, timekill121RomName, NULL, NULL, NULL, NULL, TimekillInputInfo, TimekillDIPInfo,
	TimekillInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 240, 4, 3
};


// Time Killers (v1.21, alternate ROM board)

static struct BurnRomInfo timekill121aRomDesc[] = {
	{ "tk00_v1.21_u54.u54",					0x040000, 0x4938a940, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "tk01_v1.21_u53.u53",					0x040000, 0x0bb75c40, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "timekillsnd_u17.u17",				0x020000, 0xab1684c3, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "timekill_grom00.grom00",				0x080000, 0x980aab02, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "timekill_grom05.grom05",				0x080000, 0x0b28ae65, 3 | BRF_GRA },           //  4
	{ "timekill_grom10.grom10",				0x080000, 0x6092c59e, 3 | BRF_GRA },           //  5
	{ "timekill_grom15.grom15",				0x080000, 0xb08497c1, 3 | BRF_GRA },           //  6
	{ "timekill_grom01.grom01",				0x080000, 0xc37d9486, 3 | BRF_GRA },           //  7
	{ "timekill_grom06.grom06",				0x080000, 0xf698fc14, 3 | BRF_GRA },           //  8
	{ "timekill_grom11.grom11",				0x080000, 0x69735cd0, 3 | BRF_GRA },           //  9
	{ "timekill_grom16.grom16",				0x080000, 0x1fe7cd97, 3 | BRF_GRA },           // 10
	{ "timekill_grom02.grom02",				0x080000, 0xa7b9240c, 3 | BRF_GRA },           // 11
	{ "timekill_grom07.grom07",				0x080000, 0xfb9c04d2, 3 | BRF_GRA },           // 12
	{ "timekill_grom12.grom12",				0x080000, 0x383adf84, 3 | BRF_GRA },           // 13
	{ "timekill_grom17.grom17",				0x080000, 0x77dcbf80, 3 | BRF_GRA },           // 14
	{ "timekill_grom03.grom03",				0x080000, 0x7a464aa0, 3 | BRF_GRA },           // 15
	{ "timekill_grom08.grom08",				0x080000, 0x7d6f7ba9, 3 | BRF_GRA },           // 16
	{ "timekill_grom13.grom13",				0x080000, 0xecde039d, 3 | BRF_GRA },           // 17
	{ "timekill_grom18.grom18",				0x080000, 0x05cb6d82, 3 | BRF_GRA },           // 18
	{ "timekill_grom04.grom04",				0x020000, 0xb030c3d9, 3 | BRF_GRA },           // 19
	{ "timekill_grom09.grom09",				0x020000, 0xe98492a4, 3 | BRF_GRA },           // 20
	{ "timekill_grom14.grom14",				0x020000, 0x6088fa64, 3 | BRF_GRA },           // 21
	{ "timekill_grom19.grom19",				0x020000, 0x95be2318, 3 | BRF_GRA },           // 22

	{ "tksrom00_u18.u18",					0x080000, 0x79d8b83a, 6 | BRF_SND },           // 23 Ensoniq Bank 2
	{ "tksrom01_u20.u20",					0x080000, 0xec01648c, 6 | BRF_SND },           // 24
	{ "tksrom02_u26.u26",					0x080000, 0x051ced3e, 6 | BRF_SND },           // 25
};

STD_ROM_PICK(timekill121a)
STD_ROM_FN(timekill121a)

struct BurnDriver BurnDrvTimekill121a = {
	"timekill121a", "timekill", NULL, NULL, "1992",
	"Time Killers (v1.21, alternate ROM board)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, timekill121aRomInfo, timekill121aRomName, NULL, NULL, NULL, NULL, TimekillInputInfo, TimekillDIPInfo,
	TimekillInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 240, 4, 3
};


// Time Killers (v1.20)
/* Version 1.20 (3-tier board set: P/N 1050 Rev 1, P/N 1057 Rev 0 &  P/N 1052 Rev 2) */

static struct BurnRomInfo timekill120RomDesc[] = {
	{ "tk00_v1.2_u54.u54",					0x040000, 0xdf1ce59d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "tk01_v1.2_u53.u53",					0x040000, 0xd42b9849, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "timekillsnd_u17.u17",				0x020000, 0xab1684c3, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "time_killers_0.rom0",				0x200000, 0x94cbf6f8, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "time_killers_1.rom1",				0x200000, 0xc07dea98, 3 | BRF_GRA },           //  4
	{ "time_killers_2.rom2",				0x200000, 0x183eed2a, 3 | BRF_GRA },           //  5
	{ "time_killers_3.rom3",				0x200000, 0xb1da1058, 3 | BRF_GRA },           //  6
	{ "timekill_grom01.grom1",				0x020000, 0xb030c3d9, 3 | BRF_GRA },           //  7
	{ "timekill_grom02.grom2",				0x020000, 0xe98492a4, 3 | BRF_GRA },           //  8
	{ "timekill_grom03.grom3",				0x020000, 0x6088fa64, 3 | BRF_GRA },           //  9
	{ "timekill_grom04.grom4",				0x020000, 0x95be2318, 3 | BRF_GRA },           // 10

	{ "tksrom00_u18.u18",					0x080000, 0x79d8b83a, 6 | BRF_SND },           // 11 Ensoniq Bank 2
	{ "tksrom01_u20.u20",					0x080000, 0xec01648c, 6 | BRF_SND },           // 12
	{ "tksrom02_u26.u26",					0x080000, 0x051ced3e, 6 | BRF_SND },           // 13
};

STD_ROM_PICK(timekill120)
STD_ROM_FN(timekill120)

struct BurnDriver BurnDrvTimekill120 = {
	"timekill120", "timekill", NULL, NULL, "1992",
	"Time Killers (v1.20)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, timekill120RomInfo, timekill120RomName, NULL, NULL, NULL, NULL, TimekillInputInfo, TimekillDIPInfo,
	TimekillInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 240, 4, 3
};


// Time Killers (v1.00)
/* Version 1.00? - actual version not shown (3-tier board set: P/N 1050 Rev 1, P/N 1051 Rev 0 &  P/N 1052 Rev 2) */

static struct BurnRomInfo timekill100RomDesc[] = {
	{ "tk00.bim_u54.u54",					0x040000, 0x2b379f30, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "tk01.bim_u53.u53",					0x040000, 0xe43e029c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "timekillsnd_u17.u17",				0x020000, 0xab1684c3, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "timekill_grom00.grom00",				0x080000, 0x980aab02, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "timekill_grom05.grom05",				0x080000, 0x0b28ae65, 3 | BRF_GRA },           //  4
	{ "timekill_grom10.grom10",				0x080000, 0x6092c59e, 3 | BRF_GRA },           //  5
	{ "timekill_grom15.grom15",				0x080000, 0xb08497c1, 3 | BRF_GRA },           //  6
	{ "timekill_grom01.grom01",				0x080000, 0xc37d9486, 3 | BRF_GRA },           //  7
	{ "timekill_grom06.grom06",				0x080000, 0xf698fc14, 3 | BRF_GRA },           //  8
	{ "timekill_grom11.grom11",				0x080000, 0x69735cd0, 3 | BRF_GRA },           //  9
	{ "timekill_grom16.grom16",				0x080000, 0x1fe7cd97, 3 | BRF_GRA },           // 10
	{ "timekill_grom02.grom02",				0x080000, 0xa7b9240c, 3 | BRF_GRA },           // 11
	{ "timekill_grom07.grom07",				0x080000, 0xfb9c04d2, 3 | BRF_GRA },           // 12
	{ "timekill_grom12.grom12",				0x080000, 0x383adf84, 3 | BRF_GRA },           // 13
	{ "timekill_grom17.grom17",				0x080000, 0x77dcbf80, 3 | BRF_GRA },           // 14
	{ "timekill_grom03.grom03",				0x080000, 0x7a464aa0, 3 | BRF_GRA },           // 15
	{ "timekill_grom08.grom08",				0x080000, 0x7d6f7ba9, 3 | BRF_GRA },           // 16
	{ "timekill_grom13.grom13",				0x080000, 0xecde039d, 3 | BRF_GRA },           // 17
	{ "timekill_grom18.grom18",				0x080000, 0x05cb6d82, 3 | BRF_GRA },           // 18
	{ "timekill_grom04.grom04",				0x020000, 0xb030c3d9, 3 | BRF_GRA },           // 19
	{ "timekill_grom09.grom09",				0x020000, 0xe98492a4, 3 | BRF_GRA },           // 20
	{ "timekill_grom14.grom14",				0x020000, 0x6088fa64, 3 | BRF_GRA },           // 21
	{ "timekill_grom19.grom19",				0x020000, 0x95be2318, 3 | BRF_GRA },           // 22

	{ "tksrom00_u18.u18",					0x080000, 0x79d8b83a, 6 | BRF_SND },           // 23 Ensoniq Bank 2
	{ "tksrom01_u20.u20",					0x080000, 0xec01648c, 6 | BRF_SND },           // 24
	{ "tksrom02_u26.u26",					0x080000, 0x051ced3e, 6 | BRF_SND },           // 25
};

STD_ROM_PICK(timekill100)
STD_ROM_FN(timekill100)

struct BurnDriver BurnDrvTimekill100 = {
	"timekill100", "timekill", NULL, NULL, "1992",
	"Time Killers (v1.00)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, timekill100RomInfo, timekill100RomName, NULL, NULL, NULL, NULL, TimekillInputInfo, TimekillDIPInfo,
	TimekillInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 240, 4, 3
};


// Blood Storm (v2.22)

static struct BurnRomInfo bloodstmRomDesc[] = {
	{ "bld00_v222.u83",						0x040000, 0x95f36db6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "bld01_v222.u88",						0x040000, 0xfcc04b93, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bldsnd_v10.u17",						0x020000, 0xdddeedbb, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "bsgrom0.bin",						0x080000, 0x4e10b8c1, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "bsgrom5.bin",						0x080000, 0x6333b6ce, 3 | BRF_GRA },           //  4
	{ "bsgrom10.bin",						0x080000, 0xa972a65c, 3 | BRF_GRA },           //  5
	{ "bsgrom15.bin",						0x080000, 0x9a8f54aa, 3 | BRF_GRA },           //  6
	{ "bsgrom1.bin",						0x080000, 0x10abf660, 3 | BRF_GRA },           //  7
	{ "bsgrom6.bin",						0x080000, 0x06a260d5, 3 | BRF_GRA },           //  8
	{ "bsgrom11.bin",						0x080000, 0xf2cab3c7, 3 | BRF_GRA },           //  9
	{ "bsgrom16.bin",						0x080000, 0x403aef7b, 3 | BRF_GRA },           // 10
	{ "bsgrom2.bin",						0x080000, 0x488200b1, 3 | BRF_GRA },           // 11
	{ "bsgrom7.bin",						0x080000, 0x5bb19727, 3 | BRF_GRA },           // 12
	{ "bsgrom12.bin",						0x080000, 0xb10d674f, 3 | BRF_GRA },           // 13
	{ "bsgrom17.bin",						0x080000, 0x7119df7e, 3 | BRF_GRA },           // 14
	{ "bsgrom3.bin",						0x080000, 0x2378792e, 3 | BRF_GRA },           // 15
	{ "bsgrom8.bin",						0x080000, 0x3640ca2e, 3 | BRF_GRA },           // 16
	{ "bsgrom13.bin",						0x080000, 0xbd4a071d, 3 | BRF_GRA },           // 17
	{ "bsgrom18.bin",						0x080000, 0x12959bb8, 3 | BRF_GRA },           // 18

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 19 Ensoniq Bank 0

	{ "bssrom0.bin",						0x080000, 0xee4570c8, 6 | BRF_SND },           // 20 Ensoniq Bank 2
	{ "bssrom1.bin",						0x080000, 0xb0f32ec5, 6 | BRF_SND },           // 21
	{ "bssrom2.bin",						0x040000, 0x8aee1e77, 6 | BRF_SND },           // 22
};

STD_ROM_PICK(bloodstm)
STD_ROM_FN(bloodstm)

struct BurnDriver BurnDrvBloodstm = {
	"bloodstm", NULL, NULL, NULL, "1994",
	"Blood Storm (v2.22)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, bloodstmRomInfo, bloodstmRomName, NULL, NULL, NULL, NULL, BloodstmInputInfo, BloodstmDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Blood Storm (v2.21)

static struct BurnRomInfo bloodstm221RomDesc[] = {
	{ "bld00_v221.u83",						0x040000, 0x01907aec, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "bld01_v221.u88",						0x040000, 0xeeae123e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bldsnd_v10.u17",						0x020000, 0xdddeedbb, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "bsgrom0.bin",						0x080000, 0x4e10b8c1, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "bsgrom5.bin",						0x080000, 0x6333b6ce, 3 | BRF_GRA },           //  4
	{ "bsgrom10.bin",						0x080000, 0xa972a65c, 3 | BRF_GRA },           //  5
	{ "bsgrom15.bin",						0x080000, 0x9a8f54aa, 3 | BRF_GRA },           //  6
	{ "bsgrom1.bin",						0x080000, 0x10abf660, 3 | BRF_GRA },           //  7
	{ "bsgrom6.bin",						0x080000, 0x06a260d5, 3 | BRF_GRA },           //  8
	{ "bsgrom11.bin",						0x080000, 0xf2cab3c7, 3 | BRF_GRA },           //  9
	{ "bsgrom16.bin",						0x080000, 0x403aef7b, 3 | BRF_GRA },           // 10
	{ "bsgrom2.bin",						0x080000, 0x488200b1, 3 | BRF_GRA },           // 11
	{ "bsgrom7.bin",						0x080000, 0x5bb19727, 3 | BRF_GRA },           // 12
	{ "bsgrom12.bin",						0x080000, 0xb10d674f, 3 | BRF_GRA },           // 13
	{ "bsgrom17.bin",						0x080000, 0x7119df7e, 3 | BRF_GRA },           // 14
	{ "bsgrom3.bin",						0x080000, 0x2378792e, 3 | BRF_GRA },           // 15
	{ "bsgrom8.bin",						0x080000, 0x3640ca2e, 3 | BRF_GRA },           // 16
	{ "bsgrom13.bin",						0x080000, 0xbd4a071d, 3 | BRF_GRA },           // 17
	{ "bsgrom18.bin",						0x080000, 0x12959bb8, 3 | BRF_GRA },           // 18

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 19 Ensoniq Bank 0

	{ "bssrom0.bin",						0x080000, 0xee4570c8, 6 | BRF_SND },           // 20 Ensoniq Bank 2
	{ "bssrom1.bin",						0x080000, 0xb0f32ec5, 6 | BRF_SND },           // 21
	{ "bssrom2.bin",						0x040000, 0x8aee1e77, 6 | BRF_SND },           // 22
};

STD_ROM_PICK(bloodstm221)
STD_ROM_FN(bloodstm221)

struct BurnDriver BurnDrvBloodstm221 = {
	"bloodstm221", "bloodstm", NULL, NULL, "1994",
	"Blood Storm (v2.21)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, bloodstm221RomInfo, bloodstm221RomName, NULL, NULL, NULL, NULL, BloodstmInputInfo, BloodstmDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Blood Storm (v2.20)

static struct BurnRomInfo bloodstm220RomDesc[] = {
	{ "bld00_v22.u83",						0x040000, 0x904e9208, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "bld01_v22.u88",						0x040000, 0x78336a7b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bldsnd_v10.u17",						0x020000, 0xdddeedbb, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "bsgrom0.bin",						0x080000, 0x4e10b8c1, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "bsgrom5.bin",						0x080000, 0x6333b6ce, 3 | BRF_GRA },           //  4
	{ "bsgrom10.bin",						0x080000, 0xa972a65c, 3 | BRF_GRA },           //  5
	{ "bsgrom15.bin",						0x080000, 0x9a8f54aa, 3 | BRF_GRA },           //  6
	{ "bsgrom1.bin",						0x080000, 0x10abf660, 3 | BRF_GRA },           //  7
	{ "bsgrom6.bin",						0x080000, 0x06a260d5, 3 | BRF_GRA },           //  8
	{ "bsgrom11.bin",						0x080000, 0xf2cab3c7, 3 | BRF_GRA },           //  9
	{ "bsgrom16.bin",						0x080000, 0x403aef7b, 3 | BRF_GRA },           // 10
	{ "bsgrom2.bin",						0x080000, 0x488200b1, 3 | BRF_GRA },           // 11
	{ "bsgrom7.bin",						0x080000, 0x5bb19727, 3 | BRF_GRA },           // 12
	{ "bsgrom12.bin",						0x080000, 0xb10d674f, 3 | BRF_GRA },           // 13
	{ "bsgrom17.bin",						0x080000, 0x7119df7e, 3 | BRF_GRA },           // 14
	{ "bsgrom3.bin",						0x080000, 0x2378792e, 3 | BRF_GRA },           // 15
	{ "bsgrom8.bin",						0x080000, 0x3640ca2e, 3 | BRF_GRA },           // 16
	{ "bsgrom13.bin",						0x080000, 0xbd4a071d, 3 | BRF_GRA },           // 17
	{ "bsgrom18.bin",						0x080000, 0x12959bb8, 3 | BRF_GRA },           // 18

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 19 Ensoniq Bank 0

	{ "bssrom0.bin",						0x080000, 0xee4570c8, 6 | BRF_SND },           // 20 Ensoniq Bank 2
	{ "bssrom1.bin",						0x080000, 0xb0f32ec5, 6 | BRF_SND },           // 21
	{ "bssrom2.bin",						0x040000, 0x8aee1e77, 6 | BRF_SND },           // 22
};

STD_ROM_PICK(bloodstm220)
STD_ROM_FN(bloodstm220)

struct BurnDriver BurnDrvBloodstm220 = {
	"bloodstm220", "bloodstm", NULL, NULL, "1994",
	"Blood Storm (v2.20)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, bloodstm220RomInfo, bloodstm220RomName, NULL, NULL, NULL, NULL, BloodstmInputInfo, BloodstmDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Blood Storm (v2.10)

static struct BurnRomInfo bloodstm210RomDesc[] = {
	{ "bld00_v21.u83",						0x040000, 0x71215c8e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "bld01_v21.u88",						0x040000, 0xda403da6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bldsnd_v10.u17",						0x020000, 0xdddeedbb, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "bsgrom0.bin",						0x080000, 0x4e10b8c1, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "bsgrom5.bin",						0x080000, 0x6333b6ce, 3 | BRF_GRA },           //  4
	{ "bsgrom10.bin",						0x080000, 0xa972a65c, 3 | BRF_GRA },           //  5
	{ "bsgrom15.bin",						0x080000, 0x9a8f54aa, 3 | BRF_GRA },           //  6
	{ "bsgrom1.bin",						0x080000, 0x10abf660, 3 | BRF_GRA },           //  7
	{ "bsgrom6.bin",						0x080000, 0x06a260d5, 3 | BRF_GRA },           //  8
	{ "bsgrom11.bin",						0x080000, 0xf2cab3c7, 3 | BRF_GRA },           //  9
	{ "bsgrom16.bin",						0x080000, 0x403aef7b, 3 | BRF_GRA },           // 10
	{ "bsgrom2.bin",						0x080000, 0x488200b1, 3 | BRF_GRA },           // 11
	{ "bsgrom7.bin",						0x080000, 0x5bb19727, 3 | BRF_GRA },           // 12
	{ "bsgrom12.bin",						0x080000, 0xb10d674f, 3 | BRF_GRA },           // 13
	{ "bsgrom17.bin",						0x080000, 0x7119df7e, 3 | BRF_GRA },           // 14
	{ "bsgrom3.bin",						0x080000, 0x2378792e, 3 | BRF_GRA },           // 15
	{ "bsgrom8.bin",						0x080000, 0x3640ca2e, 3 | BRF_GRA },           // 16
	{ "bsgrom13.bin",						0x080000, 0xbd4a071d, 3 | BRF_GRA },           // 17
	{ "bsgrom18.bin",						0x080000, 0x12959bb8, 3 | BRF_GRA },           // 18

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 19 Ensoniq Bank 0

	{ "bssrom0.bin",						0x080000, 0xee4570c8, 6 | BRF_SND },           // 20 Ensoniq Bank 2
	{ "bssrom1.bin",						0x080000, 0xb0f32ec5, 6 | BRF_SND },           // 21
	{ "bssrom2.bin",						0x040000, 0x8aee1e77, 6 | BRF_SND },           // 22
};

STD_ROM_PICK(bloodstm210)
STD_ROM_FN(bloodstm210)

struct BurnDriver BurnDrvBloodstm210 = {
	"bloodstm210", "bloodstm", NULL, NULL, "1994",
	"Blood Storm (v2.10)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, bloodstm210RomInfo, bloodstm210RomName, NULL, NULL, NULL, NULL, BloodstmInputInfo, BloodstmDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Blood Storm (v1.10)

static struct BurnRomInfo bloodstm110RomDesc[] = {
	{ "bld00_v11.u83",						0x040000, 0x4fff8f9b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "bld01_v11.u88",						0x040000, 0x59ce23ea, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bldsnd_v10.u17",						0x020000, 0xdddeedbb, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "bsgrom0.bin",						0x080000, 0x4e10b8c1, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "bsgrom5.bin",						0x080000, 0x6333b6ce, 3 | BRF_GRA },           //  4
	{ "bsgrom10.bin",						0x080000, 0xa972a65c, 3 | BRF_GRA },           //  5
	{ "bsgrom15.bin",						0x080000, 0x9a8f54aa, 3 | BRF_GRA },           //  6
	{ "bsgrom1.bin",						0x080000, 0x10abf660, 3 | BRF_GRA },           //  7
	{ "bsgrom6.bin",						0x080000, 0x06a260d5, 3 | BRF_GRA },           //  8
	{ "bsgrom11.bin",						0x080000, 0xf2cab3c7, 3 | BRF_GRA },           //  9
	{ "bsgrom16.bin",						0x080000, 0x403aef7b, 3 | BRF_GRA },           // 10
	{ "bsgrom2.bin",						0x080000, 0x488200b1, 3 | BRF_GRA },           // 11
	{ "bsgrom7.bin",						0x080000, 0x5bb19727, 3 | BRF_GRA },           // 12
	{ "bsgrom12.bin",						0x080000, 0xb10d674f, 3 | BRF_GRA },           // 13
	{ "bsgrom17.bin",						0x080000, 0x7119df7e, 3 | BRF_GRA },           // 14
	{ "bsgrom3.bin",						0x080000, 0x2378792e, 3 | BRF_GRA },           // 15
	{ "bsgrom8.bin",						0x080000, 0x3640ca2e, 3 | BRF_GRA },           // 16
	{ "bsgrom13.bin",						0x080000, 0xbd4a071d, 3 | BRF_GRA },           // 17
	{ "bsgrom18.bin",						0x080000, 0x12959bb8, 3 | BRF_GRA },           // 18

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 19 Ensoniq Bank 0

	{ "bssrom0.bin",						0x080000, 0xee4570c8, 6 | BRF_SND },           // 20 Ensoniq Bank 2
	{ "bssrom1.bin",						0x080000, 0xb0f32ec5, 6 | BRF_SND },           // 21
	{ "bssrom2.bin",						0x040000, 0x8aee1e77, 6 | BRF_SND },           // 22
};

STD_ROM_PICK(bloodstm110)
STD_ROM_FN(bloodstm110)

struct BurnDriver BurnDrvBloodstm110 = {
	"bloodstm110", "bloodstm", NULL, NULL, "1994",
	"Blood Storm (v1.10)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, bloodstm110RomInfo, bloodstm110RomName, NULL, NULL, NULL, NULL, BloodstmInputInfo, BloodstmDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Blood Storm (v1.04)

static struct BurnRomInfo bloodstm104RomDesc[] = {
	{ "bld00_v10.u83",						0x040000, 0xa0982119, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "bld01_v10.u88",						0x040000, 0x65800339, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bldsnd_v10.u17",						0x020000, 0xdddeedbb, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code
	
	{ "bsgrom0.bin",						0x080000, 0x4e10b8c1, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "bsgrom5.bin",						0x080000, 0x6333b6ce, 3 | BRF_GRA },           //  4
	{ "bsgrom10.bin",						0x080000, 0xa972a65c, 3 | BRF_GRA },           //  5
	{ "bsgrom15.bin",						0x080000, 0x9a8f54aa, 3 | BRF_GRA },           //  6
	{ "bsgrom1.bin",						0x080000, 0x10abf660, 3 | BRF_GRA },           //  7
	{ "bsgrom6.bin",						0x080000, 0x06a260d5, 3 | BRF_GRA },           //  8
	{ "bsgrom11.bin",						0x080000, 0xf2cab3c7, 3 | BRF_GRA },           //  9
	{ "bsgrom16.bin",						0x080000, 0x403aef7b, 3 | BRF_GRA },           // 10
	{ "bsgrom2.bin",						0x080000, 0x488200b1, 3 | BRF_GRA },           // 11
	{ "bsgrom7.bin",						0x080000, 0x5bb19727, 3 | BRF_GRA },           // 12
	{ "bsgrom12.bin",						0x080000, 0xb10d674f, 3 | BRF_GRA },           // 13
	{ "bsgrom17.bin",						0x080000, 0x7119df7e, 3 | BRF_GRA },           // 14
	{ "bsgrom3.bin",						0x080000, 0x2378792e, 3 | BRF_GRA },           // 15
	{ "bsgrom8.bin",						0x080000, 0x3640ca2e, 3 | BRF_GRA },           // 16
	{ "bsgrom13.bin",						0x080000, 0xbd4a071d, 3 | BRF_GRA },           // 17
	{ "bsgrom18.bin",						0x080000, 0x12959bb8, 3 | BRF_GRA },           // 18
	
	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 19 Ensoniq Bank 0

	{ "bssrom0.bin",						0x080000, 0xee4570c8, 6 | BRF_SND },           // 20 Ensoniq Bank 2
	{ "bssrom1.bin",						0x080000, 0xb0f32ec5, 6 | BRF_SND },           // 21
	{ "bssrom2.bin",						0x040000, 0x8aee1e77, 6 | BRF_SND },           // 22
};

STD_ROM_PICK(bloodstm104)
STD_ROM_FN(bloodstm104)

struct BurnDriver BurnDrvBloodstm104 = {
	"bloodstm104", "bloodstm", NULL, NULL, "1994",
	"Blood Storm (v1.04)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, bloodstm104RomInfo, bloodstm104RomName, NULL, NULL, NULL, NULL, BloodstmInputInfo, BloodstmDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Hard Yardage (v1.20)

static struct BurnRomInfo hardyardRomDesc[] = {
	{ "fb00_v1.20_u83.u83",					0x040000, 0xc7497692, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "fb01_v1.20_u88.u88",					0x040000, 0x3320c79a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "fb_snd_v1.1_u17.u17",				0x020000, 0xd221b121, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "footbal_0.grom0.itfb0",				0x100000, 0x0b7781af, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "footbal_1.grom5.itfb1",				0x100000, 0x178d0f9b, 3 | BRF_GRA },           //  4
	{ "footbal_2.grom10.itfb2",				0x100000, 0x0a17231e, 3 | BRF_GRA },           //  5
	{ "footbal_3.grom15.itfb3",				0x100000, 0x104456af, 3 | BRF_GRA },           //  6
	{ "footbal_4.grom1.itfb4",				0x100000, 0x2cb6f454, 3 | BRF_GRA },           //  7
	{ "footbal_5.grom6.itfb5",				0x100000, 0x9b19b873, 3 | BRF_GRA },           //  8
	{ "footbal_6.grom11.itfb6",				0x100000, 0x58694394, 3 | BRF_GRA },           //  9
	{ "footbal_7.grom16.itfb7",				0x100000, 0x9b7a2d1a, 3 | BRF_GRA },           // 10
	{ "itfb-8.grom2.itfb8",					0x020000, 0xa1656bf8, 3 | BRF_GRA },           // 11
	{ "itfb-9.grom7.itfb9",					0x020000, 0x2afa9e10, 3 | BRF_GRA },           // 12
	{ "itfb-10.grom12.itfb10",				0x020000, 0xd5d15b38, 3 | BRF_GRA },           // 13
	{ "itfb-11.grom17.itfb11",				0x020000, 0xcd4f0df0, 3 | BRF_GRA },           // 14

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 15 Ensoniq Bank 0

	{ "fb_srom00.srom0",					0x080000, 0xb0a76ad2, 6 | BRF_SND },           // 16 Ensoniq Bank 2
	{ "fb_srom01.srom1",					0x080000, 0x9fbf6a02, 6 | BRF_SND },           // 17
};

STD_ROM_PICK(hardyard)
STD_ROM_FN(hardyard)

struct BurnDriver BurnDrvHardyard = {
	"hardyard", NULL, NULL, NULL, "1993",
	"Hard Yardage (v1.20)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hardyardRomInfo, hardyardRomName, NULL, NULL, NULL, NULL, HardyardInputInfo, HardyardDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Hard Yardage (v1.10)

static struct BurnRomInfo hardyard11RomDesc[] = {
	{ "fb00_v1.10_u83.u83",					0x040000, 0x603f8a03, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "fb01_v1.10_u88.u88",					0x040000, 0x7b11db88, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "fb_snd_v1.1_u17.u17",				0x020000, 0xd221b121, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "footbal_0.grom0.itfb0",				0x100000, 0x0b7781af, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "footbal_1.grom5.itfb1",				0x100000, 0x178d0f9b, 3 | BRF_GRA },           //  4
	{ "footbal_2.grom10.itfb2",				0x100000, 0x0a17231e, 3 | BRF_GRA },           //  5
	{ "footbal_3.grom15.itfb3",				0x100000, 0x104456af, 3 | BRF_GRA },           //  6
	{ "footbal_4.grom1.itfb4",				0x100000, 0x2cb6f454, 3 | BRF_GRA },           //  7
	{ "footbal_5.grom6.itfb5",				0x100000, 0x9b19b873, 3 | BRF_GRA },           //  8
	{ "footbal_6.grom11.itfb6",				0x100000, 0x58694394, 3 | BRF_GRA },           //  9
	{ "footbal_7.grom16.itfb7",				0x100000, 0x9b7a2d1a, 3 | BRF_GRA },           // 10
	{ "itfb-8.grom2.itfb8",					0x020000, 0xa1656bf8, 3 | BRF_GRA },           // 11
	{ "itfb-9.grom7.itfb9",					0x020000, 0x2afa9e10, 3 | BRF_GRA },           // 12
	{ "itfb-10.grom12.itfb10",				0x020000, 0xd5d15b38, 3 | BRF_GRA },           // 13
	{ "itfb-11.grom17.itfb11",				0x020000, 0xcd4f0df0, 3 | BRF_GRA },           // 14

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 15 Ensoniq Bank 0

	{ "fb_srom00.srom0",					0x080000, 0xb0a76ad2, 6 | BRF_SND },           // 16 Ensoniq Bank 2
	{ "fb_srom01.srom1",					0x080000, 0x9fbf6a02, 6 | BRF_SND },           // 17
};

STD_ROM_PICK(hardyard11)
STD_ROM_FN(hardyard11)

struct BurnDriver BurnDrvHardyard11 = {
	"hardyard11", "hardyard", NULL, NULL, "1993",
	"Hard Yardage (v1.10)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hardyard11RomInfo, hardyard11RomName, NULL, NULL, NULL, NULL, HardyardInputInfo, HardyardDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Hard Yardage (v1.00)

static struct BurnRomInfo hardyard10RomDesc[] = {
	{ "fb00_v1.0_u83.u83",					0x040000, 0xf839393c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "fb01_v1.0_u88.u88",					0x040000, 0xca444702, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "fb_snd_v1.0_u17.u17",				0x020000, 0x6c6db5b8, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "footbal_0.grom0.itfb0",				0x100000, 0x0b7781af, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "footbal_1.grom5.itfb1",				0x100000, 0x178d0f9b, 3 | BRF_GRA },           //  4
	{ "footbal_2.grom10.itfb2",				0x100000, 0x0a17231e, 3 | BRF_GRA },           //  5
	{ "footbal_3.grom15.itfb3",				0x100000, 0x104456af, 3 | BRF_GRA },           //  6
	{ "footbal_4.grom1.itfb4",				0x100000, 0x2cb6f454, 3 | BRF_GRA },           //  7
	{ "footbal_5.grom6.itfb5",				0x100000, 0x9b19b873, 3 | BRF_GRA },           //  8
	{ "footbal_6.grom11.itfb6",				0x100000, 0x58694394, 3 | BRF_GRA },           //  9
	{ "footbal_7.grom16.itfb7",				0x100000, 0x9b7a2d1a, 3 | BRF_GRA },           // 10
	{ "itfb-8.grom2.itfb8",					0x020000, 0xa1656bf8, 3 | BRF_GRA },           // 11
	{ "itfb-9.grom7.itfb9",					0x020000, 0x2afa9e10, 3 | BRF_GRA },           // 12
	{ "itfb-10.grom12.itfb10",				0x020000, 0xd5d15b38, 3 | BRF_GRA },           // 13
	{ "itfb-11.grom17.itfb11",				0x020000, 0xcd4f0df0, 3 | BRF_GRA },           // 14

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 15 Ensoniq Bank 0

	{ "fb_srom00.srom0",					0x080000, 0xb0a76ad2, 6 | BRF_SND },           // 16 Ensoniq Bank 2
	{ "fb_srom01.srom1",					0x080000, 0x9fbf6a02, 6 | BRF_SND },           // 17
};

STD_ROM_PICK(hardyard10)
STD_ROM_FN(hardyard10)

struct BurnDriver BurnDrvHardyard10 = {
	"hardyard10", "hardyard", NULL, NULL, "1993",
	"Hard Yardage (v1.00)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hardyard10RomInfo, hardyard10RomName, NULL, NULL, NULL, NULL, HardyardInputInfo, HardyardDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Pairs (V1.2, 09/30/94)

static struct BurnRomInfo pairsRomDesc[] = {
	{ "pair0_u83_x_v1.2.u83",				0x020000, 0xa9c761d8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pair1_u88_x_v1.2.u88",				0x020000, 0x5141eb86, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pairsnd_u17_v1.u17",					0x020000, 0x7a514cfd, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "pairx_grom0_v1.grom0",				0x080000, 0xbaf1c2dd, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "pairx_grom5_v1.grom5",				0x080000, 0x30e993f3, 3 | BRF_GRA },           //  4
	{ "pairx_grom10_v1.grom10",				0x080000, 0x3f52f50d, 3 | BRF_GRA },           //  5
	{ "pairx_grom15_v1.grom15",				0x080000, 0xfd38aa36, 3 | BRF_GRA },           //  6
	{ "pairx_grom1_v1.grom1",				0x040000, 0xb0bd7008, 3 | BRF_GRA },           //  7
	{ "pairx_grom6_v1.grom6",				0x040000, 0xf7b20a47, 3 | BRF_GRA },           //  8
	{ "pairx_grom11_v1.grom11",				0x040000, 0x1e5f2523, 3 | BRF_GRA },           //  9
	{ "pairx_grom16_v1.grom16",				0x040000, 0xb2975259, 3 | BRF_GRA },           // 10

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 11 Ensoniq Bank 0

	{ "srom0.bin",							0x080000, 0x19a857f9, 6 | BRF_SND },           // 12 Ensoniq Bank 2
};

STD_ROM_PICK(pairs)
STD_ROM_FN(pairs)

struct BurnDriver BurnDrvPairs = {
	"pairs", NULL, NULL, NULL, "1994",
	"Pairs (V1.2, 09/30/94)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, pairsRomInfo, pairsRomName, NULL, NULL, NULL, NULL, PairsInputInfo, PairsDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Pairs (V1, 09/07/94)

static struct BurnRomInfo pairsaRomDesc[] = {
	{ "pair0_u83_x_v1.u83",					0x020000, 0x774995a3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pair1_u83_x_v1.u88",					0x020000, 0x85d0b73a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pairsnd_u17_v1.u17",					0x020000, 0x7a514cfd, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "pairx_grom0_v1.grom0",				0x080000, 0xbaf1c2dd, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "pairx_grom5_v1.grom5",				0x080000, 0x30e993f3, 3 | BRF_GRA },           //  4
	{ "pairx_grom10_v1.grom10",				0x080000, 0x3f52f50d, 3 | BRF_GRA },           //  5
	{ "pairx_grom15_v1.grom15",				0x080000, 0xfd38aa36, 3 | BRF_GRA },           //  6
	{ "pairx_grom1_v1.grom1",				0x040000, 0xb0bd7008, 3 | BRF_GRA },           //  7
	{ "pairx_grom6_v1.grom6",				0x040000, 0xf7b20a47, 3 | BRF_GRA },           //  8
	{ "pairx_grom11_v1.grom11",				0x040000, 0x1e5f2523, 3 | BRF_GRA },           //  9
	{ "pairx_grom16_v1.grom16",				0x040000, 0xb2975259, 3 | BRF_GRA },           // 10

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 11 Ensoniq Bank 0

	{ "srom0_pairs_v1.srom0",				0x080000, 0x1d96c581, 6 | BRF_SND },           // 12 Ensoniq Bank 2
};

STD_ROM_PICK(pairsa)
STD_ROM_FN(pairsa)

struct BurnDriver BurnDrvPairsa = {
	"pairsa", "pairs", NULL, NULL, "1994",
	"Pairs (V1, 09/07/94)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, pairsaRomInfo, pairsaRomName, NULL, NULL, NULL, NULL, PairsInputInfo, PairsDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Pairs Redemption (V1.0, 10/25/94)
/* Version RED V1.0 (3-tier board set: P/N 1059 Rev 3, P/N 1061 Rev 1 &  P/N 1060 Rev 0) */

static struct BurnRomInfo pairsredRomDesc[] = {
	{ "pair0_u83_redv1.u83",				0x020000, 0xcf27b93c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pair1_u88_redv1.u88",				0x020000, 0x7ad48e7e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pairsnd_u17_redv1.u17",				0x020000, 0x198e1743, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "pair-red_grom0",						0x040000, 0x1bbcc6c6, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "pair-red_grom5",						0x040000, 0xeff93bde, 3 | BRF_GRA },           //  4
	{ "pair-red_grom10",					0x040000, 0x016f4d19, 3 | BRF_GRA },           //  5
	{ "pair-red_grom15",					0x040000, 0xdc95160d, 3 | BRF_GRA },           //  6

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           //  7 Ensoniq Bank 0

	{ "srom0_pairs_redv1",					0x080000, 0xa998e29f, 6 | BRF_SND },           //  8 Ensoniq Bank 2
};

STD_ROM_PICK(pairsred)
STD_ROM_FN(pairsred)

struct BurnDriver BurnDrvPairsred = {
	"pairsred", NULL, NULL, NULL, "1994",
	"Pairs Redemption (V1.0, 10/25/94)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, pairsredRomInfo, pairsredRomName, NULL, NULL, NULL, NULL, PairsInputInfo, PairsDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Hot Memory (V1.2, Germany, 12/28/94)

static struct BurnRomInfo hotmemryRomDesc[] = {
	{ "hotmem0_u83_v1.2.u83",				0x040000, 0x5b9d87a2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "hotmem1_u88_v1.2.u88",				0x040000, 0xaeea087c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "hotmemsnd_u17_v1.u17",				0x020000, 0x805941c7, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "hotmem_grom0_v1.grom0",				0x080000, 0x68f279ef, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "hotmem_grom5_v1.grom5",				0x080000, 0x295bb43d, 3 | BRF_GRA },           //  4
	{ "hotmem_grom10_v1.grom10",			0x080000, 0xf8cc939b, 3 | BRF_GRA },           //  5
	{ "hotmem_grom15_v1.grom15",			0x080000, 0xa03d9bcd, 3 | BRF_GRA },           //  6
	{ "hotmem_grom1_v1.grom1",				0x040000, 0xb446105e, 3 | BRF_GRA },           //  7
	{ "hotmem_grom6_v1.grom6",				0x040000, 0x3a7ba9eb, 3 | BRF_GRA },           //  8
	{ "hotmem_grom11_v1.grom11",			0x040000, 0x9ec4ea41, 3 | BRF_GRA },           //  9
	{ "hotmem_grom16_v1.grom16",			0x040000, 0x4507a895, 3 | BRF_GRA },           // 10

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 11 Ensoniq Bank 0

	{ "srom0_hotmem_v1.srom0",				0x080000, 0xc1103224, 6 | BRF_SND },           // 12 Ensoniq Bank 2
};

STD_ROM_PICK(hotmemry)
STD_ROM_FN(hotmemry)

struct BurnDriver BurnDrvHotmemry = {
	"hotmemry", "pairs", NULL, NULL, "1994",
	"Hot Memory (V1.2, Germany, 12/28/94)\0", NULL, "Incredible Technologies (Tuning license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hotmemryRomInfo, hotmemryRomName, NULL, NULL, NULL, NULL, PairsInputInfo, PairsDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Hot Memory (V1.1, Germany, 11/30/94)

static struct BurnRomInfo hotmemry11RomDesc[] = {
	{ "hotmem0_u83_v1.1.u83",				0x020000, 0x8d614b1b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "hotmem1_u88_v1.1.u88",				0x020000, 0x009639fb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "hotmemsnd_u17_v1.u17",				0x020000, 0x805941c7, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "hotmem_grom0_v1.grom0",				0x080000, 0x68f279ef, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "hotmem_grom5_v1.grom5",				0x080000, 0x295bb43d, 3 | BRF_GRA },           //  4
	{ "hotmem_grom10_v1.grom10",			0x080000, 0xf8cc939b, 3 | BRF_GRA },           //  5
	{ "hotmem_grom15_v1.grom15",			0x080000, 0xa03d9bcd, 3 | BRF_GRA },           //  6
	{ "hotmem_grom1_v1.grom1",				0x040000, 0xb446105e, 3 | BRF_GRA },           //  7
	{ "hotmem_grom6_v1.grom6",				0x040000, 0x3a7ba9eb, 3 | BRF_GRA },           //  8
	{ "hotmem_grom11_v1.grom11",			0x040000, 0x9ec4ea41, 3 | BRF_GRA },           //  9
	{ "hotmem_grom16_v1.grom16",			0x040000, 0x4507a895, 3 | BRF_GRA },           // 10

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 11 Ensoniq Bank 0

	{ "srom0_hotmem_v1.srom0",				0x080000, 0xc1103224, 6 | BRF_SND },           // 12 Ensoniq Bank 2
};

STD_ROM_PICK(hotmemry11)
STD_ROM_FN(hotmemry11)

struct BurnDriver BurnDrvHotmemry11 = {
	"hotmemry11", "pairs", NULL, NULL, "1994",
	"Hot Memory (V1.1, Germany, 11/30/94)\0", NULL, "Incredible Technologies (Tuning license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hotmemry11RomInfo, hotmemry11RomName, NULL, NULL, NULL, NULL, PairsInputInfo, PairsDIPInfo,
	Common16BitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// World Class Bowling Deluxe (v2.00)
/* Deluxe version 2.00 (PCB P/N 1083 Rev 2), This version is derived from the Tournament v1.40 set, but tournament features have be removed/disabled */

static struct BurnRomInfo wcbowldxRomDesc[] = {
	{ "wcbd_prom0_2.00.prom0",				0x020000, 0x280df7f0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcbd_prom1_2.00.prom1",				0x020000, 0x526eded0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wcbd_prom2_2.00.prom2",				0x020000, 0xcb263173, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wcbd_prom3_2.00.prom3",				0x020000, 0x43ecad0b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "wcbsnd_u88_4.01.u88",				0x020000, 0xe97a6d28, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "wcbd_grom0_0_s.grm0_0",				0x080000, 0x6fcb4246, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "wcbd_grom0_1_s.grm0_1",				0x080000, 0x2ae31f45, 3 | BRF_GRA },           //  6
	{ "wcbd_grom0_2_s.grm0_2",				0x080000, 0xbccc0f35, 3 | BRF_GRA },           //  7
	{ "wcbd_grom0_3_s.grm0_3",				0x080000, 0xab1da462, 3 | BRF_GRA },           //  8
	{ "wcbd_grom1_0_s.grm1_0",				0x080000, 0xbdfafd1f, 3 | BRF_GRA },           //  9
	{ "wcbd_grom1_1_s.grm1_1",				0x080000, 0x7d6baa2e, 3 | BRF_GRA },           // 10
	{ "wcbd_grom1_2_s.grm1_2",				0x080000, 0x7513d3de, 3 | BRF_GRA },           // 11
	{ "wcbd_grom1_3_s.grm1_3",				0x080000, 0xe46877e6, 3 | BRF_GRA },           // 12
	{ "wcbd_grom2_0_s.grm2_0",				0x040000, 0x3b4c5a5c, 3 | BRF_GRA },           // 13
	{ "wcbd_grom2_1_s.grm2_1",				0x040000, 0xed0b9b26, 3 | BRF_GRA },           // 14
	{ "wcbd_grom2_2_s.grm2_2",				0x040000, 0x4b9e345e, 3 | BRF_GRA },           // 15
	{ "wcbd_grom2_3_s.grm2_3",				0x040000, 0x4e13ee7a, 3 | BRF_GRA },           // 16

	{ "wcb_srom0_s.srom0",					0x080000, 0xd42dd283, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "wcb_srom1_s.srom1",					0x080000, 0x7a69ab54, 4 | BRF_SND },           // 18

	{ "wcb_srom2_s.srom2",					0x080000, 0x346530a2, 5 | BRF_SND },           // 19 Ensoniq Bank 1
	{ "wcb_srom3_s.srom3",					0x040000, 0x1dfe3a31, 5 | BRF_SND },           // 20
};

STD_ROM_PICK(wcbowldx)
STD_ROM_FN(wcbowldx)

static INT32 WcbowldxInit()
{
	Trackball_Type = TB_TYPE0;

	return Common32BitInit(0x111a, 1, 0);
}

struct BurnDriver BurnDrvWcbowldx = {
	"wcbowldx", NULL, NULL, NULL, "1999",
	"World Class Bowling Deluxe (v2.00)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowldxRomInfo, wcbowldxRomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowldxDIPInfo,
	WcbowldxInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// World Class Bowling Tournament (v1.40)
/* Version 1.40 Tournament (PCB P/N 1083 Rev 2) */

static struct BurnRomInfo wcbowl140RomDesc[] = {
	{ "wcbf_prom0_1.40.prom0",				0x020000, 0x9d31ceb1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcbf_prom1_1.40.prom1",				0x020000, 0xc6669452, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wcbf_prom2_1.40.prom2",				0x020000, 0xd2fc4d09, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wcbf_prom3_1.40.prom3",				0x020000, 0xc41258a4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "wcbsnd_u88_4.01.u88",				0x020000, 0xe97a6d28, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "wcb_grom0_0_s.grm0_0",				0x080000, 0x6fcb4246, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "wcb_grom0_1_s.grm0_1",				0x080000, 0x2ae31f45, 3 | BRF_GRA },           //  6
	{ "wcb_grom0_2_s.grm0_2",				0x080000, 0xbccc0f35, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_3_s.grm0_3",				0x080000, 0xab1da462, 3 | BRF_GRA },           //  8
	{ "wcb_grom1_0_s.grm1_0",				0x080000, 0xbdfafd1f, 3 | BRF_GRA },           //  9
	{ "wcb_grom1_1_s.grm1_1",				0x080000, 0x7d6baa2e, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_2_s.grm1_2",				0x080000, 0x7513d3de, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_3_s.grm1_3",				0x080000, 0xe46877e6, 3 | BRF_GRA },           // 12
	{ "wcbf_grom2_0.grm2_0",				0x040000, 0x3b4c5a5c, 3 | BRF_GRA },           // 13
	{ "wcbf_grom2_1.grm2_1",				0x040000, 0xed0b9b26, 3 | BRF_GRA },           // 14
	{ "wcbf_grom2_2.grm2_2",				0x040000, 0x4b9e345e, 3 | BRF_GRA },           // 15
	{ "wcbf_grom2_3.grm2_3",				0x040000, 0x4e13ee7a, 3 | BRF_GRA },           // 16

	{ "wcb_srom0_s.srom0",					0x080000, 0xd42dd283, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "wcb_srom1_s.srom1",					0x080000, 0x7a69ab54, 4 | BRF_SND },           // 18

	{ "wcb_srom2_s.srom2",					0x080000, 0x346530a2, 5 | BRF_SND },           // 19 Ensoniq Bank 1
	{ "wcb_srom3_s.srom3",					0x040000, 0x1dfe3a31, 5 | BRF_SND },           // 20
	
	{ "itbwl-3 1997 it,inc.1996",			0x001fff, 0x1461cbe0, 0 | BRF_OPT },		   // 21 pic
};

STD_ROM_PICK(wcbowl140)
STD_ROM_FN(wcbowl140)

static INT32 Wcbowl140Init()
{
	Trackball_Type = TB_TYPE0;

	return Common32BitInit(0x111a, 1, 0);
}

struct BurnDriver BurnDrvWcbowl140 = {
	"wcbowl140", "wcbowldx", NULL, NULL, "1997",
	"World Class Bowling Tournament (v1.40)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowl140RomInfo, wcbowl140RomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowldxDIPInfo,
	Wcbowl140Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// World Class Bowling Tournament (v1.30)
/* Version 1.30 Tournament (PCB P/N 1083 Rev 2) */

static struct BurnRomInfo wcbowl130RomDesc[] = {
	{ "wcb_prom0_v1.30t.prom0",				0x020000, 0xfbcde4e0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcb_prom1_v1.30t.prom1",				0x020000, 0xf4b8e7c3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wcb_prom2_v1.30t.prom2",				0x020000, 0xf441afae, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wcb_prom3_v1.30t.prom3",				0x020000, 0x47e26d4b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "wcbsnd_u88_4.01.u88",				0x020000, 0xe97a6d28, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "wcb_grom0_0_s.grm0_0",				0x080000, 0x6fcb4246, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "wcb_grom0_1_s.grm0_1",				0x080000, 0x2ae31f45, 3 | BRF_GRA },           //  6
	{ "wcb_grom0_2_s.grm0_2",				0x080000, 0xbccc0f35, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_3_s.grm0_3",				0x080000, 0xab1da462, 3 | BRF_GRA },           //  8
	{ "wcb_grom1_0_s.grm1_0",				0x080000, 0xbdfafd1f, 3 | BRF_GRA },           //  9
	{ "wcb_grom1_1_s.grm1_1",				0x080000, 0x7d6baa2e, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_2_s.grm1_2",				0x080000, 0x7513d3de, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_3_s.grm1_3",				0x080000, 0xe46877e6, 3 | BRF_GRA },           // 12
	{ "wcbf_grom2_0.grm2_0",				0x040000, 0x3b4c5a5c, 3 | BRF_GRA },           // 13
	{ "wcbf_grom2_1.grm2_1",				0x040000, 0xed0b9b26, 3 | BRF_GRA },           // 14
	{ "wcbf_grom2_2.grm2_2",				0x040000, 0x4b9e345e, 3 | BRF_GRA },           // 15
	{ "wcbf_grom2_3.grm2_3",				0x040000, 0x4e13ee7a, 3 | BRF_GRA },           // 16

	{ "wcb_srom0_s.srom0",					0x080000, 0xd42dd283, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "wcb_srom1_s.srom1",					0x080000, 0x7a69ab54, 4 | BRF_SND },           // 18

	{ "wcb_srom2_s.srom2",					0x080000, 0x346530a2, 5 | BRF_SND },           // 19 Ensoniq Bank 1
	{ "wcb_srom3_s.srom3",					0x040000, 0x1dfe3a31, 5 | BRF_SND },           // 20
	
	{ "itbwl-3 1997 it,inc.1996",			0x001fff, 0x1461cbe0, 0 | BRF_OPT },		   // 21 pic
};

STD_ROM_PICK(wcbowl130)
STD_ROM_FN(wcbowl130)

struct BurnDriver BurnDrvWcbowl130 = {
	"wcbowl130", "wcbowldx", NULL, NULL, "1997",
	"World Class Bowling Tournament (v1.30)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowl130RomInfo, wcbowl130RomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowloDIPInfo,
	Wcbowl140Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// World Class Bowling (v1.66)
/* Version 1.66 (PCB P/N 1083 Rev 2) */

static struct BurnRomInfo wcbowlRomDesc[] = {
	{ "wcb_prom0_v1.66n.prom0",				0x020000, 0xf6774112, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcb_prom1_v1.66n.prom1",				0x020000, 0x931821ae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wcb_prom2_v1.66n.prom2",				0x020000, 0xc54f5e40, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wcb_prom3_v1.66n.prom3",				0x020000, 0xdd72c796, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "wcbsnd_u88_4.01.u88",				0x020000, 0xe97a6d28, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "wcb_grom0_0_s.grm0_0",				0x080000, 0x6fcb4246, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "wcb_grom0_1_s.grm0_1",				0x080000, 0x2ae31f45, 3 | BRF_GRA },           //  6
	{ "wcb_grom0_2_s.grm0_2",				0x080000, 0xbccc0f35, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_3_s.grm0_3",				0x080000, 0xab1da462, 3 | BRF_GRA },           //  8
	{ "wcb_grom1_0_s.grm1_0",				0x080000, 0xbdfafd1f, 3 | BRF_GRA },           //  9
	{ "wcb_grom1_1_s.grm1_1",				0x080000, 0x7d6baa2e, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_2_s.grm1_2",				0x080000, 0x7513d3de, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_3_s.grm1_3",				0x080000, 0xe46877e6, 3 | BRF_GRA },           // 12

	{ "wcb_srom0_s.srom0",					0x080000, 0xd42dd283, 4 | BRF_SND },           // 13 Ensoniq Bank 0
	{ "wcb_srom1_s.srom1",					0x080000, 0x7a69ab54, 4 | BRF_SND },           // 14

	{ "wcb_srom2_s.srom2",					0x080000, 0x346530a2, 5 | BRF_SND },           // 15 Ensoniq Bank 1
	{ "wcb_srom3_s.srom3",					0x040000, 0x1dfe3a31, 5 | BRF_SND },           // 16
	
	{ "itbwl-3 1997 it,inc.1996",			0x001fff, 0x1461cbe0, 0 | BRF_OPT },		   // 17 pic
};

STD_ROM_PICK(wcbowl)
STD_ROM_FN(wcbowl)

static INT32 WcbowlInit()
{
	Trackball_Type = TB_TYPE0;

	return Common32BitInit(0x1116, 1, 0);
}

struct BurnDriver BurnDrvWcbowl = {
	"wcbowl", NULL, NULL, NULL, "1995",
	"World Class Bowling (v1.66)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowlRomInfo, wcbowlRomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowlnDIPInfo,
	WcbowlInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// World Class Bowling (v1.65)
/* Version 1.65 (PCB P/N 1083 Rev 2) */

static struct BurnRomInfo wcbowl165RomDesc[] = {
	{ "wcb_prom0_v1.65n.prom0",				0x020000, 0xcf0f6c25, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcb_prom1_v1.65n.prom1",				0x020000, 0x076ab158, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wcb_prom2_v1.65n.prom2",				0x020000, 0x47259009, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wcb_prom3_v1.65n.prom3",				0x020000, 0x4c6b4e4f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "wcbsnd_u88_4.01.u88",				0x020000, 0xe97a6d28, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "wcb_grom0_0_s.grm0_0",				0x080000, 0x6fcb4246, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "wcb_grom0_1_s.grm0_1",				0x080000, 0x2ae31f45, 3 | BRF_GRA },           //  6
	{ "wcb_grom0_2_s.grm0_2",				0x080000, 0xbccc0f35, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_3_s.grm0_3",				0x080000, 0xab1da462, 3 | BRF_GRA },           //  8
	{ "wcb_grom1_0_s.grm1_0",				0x080000, 0xbdfafd1f, 3 | BRF_GRA },           //  9
	{ "wcb_grom1_1_s.grm1_1",				0x080000, 0x7d6baa2e, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_2_s.grm1_2",				0x080000, 0x7513d3de, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_3_s.grm1_3",				0x080000, 0xe46877e6, 3 | BRF_GRA },           // 12

	{ "wcb_srom0_s.srom0",					0x080000, 0xd42dd283, 4 | BRF_SND },           // 13 Ensoniq Bank 0
	{ "wcb_srom1_s.srom1",					0x080000, 0x7a69ab54, 4 | BRF_SND },           // 14

	{ "wcb_srom2_s.srom2",					0x080000, 0x346530a2, 5 | BRF_SND },           // 15 Ensoniq Bank 1
	{ "wcb_srom3_s.srom3",					0x040000, 0x1dfe3a31, 5 | BRF_SND },           // 16
	
	{ "itbwl-3 1997 it,inc.1996",			0x001fff, 0x1461cbe0, 0 | BRF_OPT },		   // 17 pic
};

STD_ROM_PICK(wcbowl165)
STD_ROM_FN(wcbowl165)

struct BurnDriver BurnDrvWcbowl165 = {
	"wcbowl165", "wcbowl", NULL, NULL, "1995",
	"World Class Bowling (v1.65)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowl165RomInfo, wcbowl165RomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowloDIPInfo,
	WcbowlInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// World Class Bowling (v1.61)
 /* Version 1.61 (PCB P/N 1083 Rev 2) */
 
static struct BurnRomInfo wcbowl161RomDesc[] = {
	{ "wcb_prom0_v1.61n.prom0",				0x020000, 0xb879d4a7, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcb_prom1_v1.61n.prom1",				0x020000, 0x49f3ed6a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wcb_prom2_v1.61n.prom2",				0x020000, 0x47259009, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wcb_prom3_v1.61n.prom3",				0x020000, 0xe5081f85, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "wcbsnd_u88_v4.0.u88",				0x020000, 0x194a51d7, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "wcb_grom0_0_s.grm0_0",				0x080000, 0x6fcb4246, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "wcb_grom0_1_s.grm0_1",				0x080000, 0x2ae31f45, 3 | BRF_GRA },           //  6
	{ "wcb_grom0_2_s.grm0_2",				0x080000, 0xbccc0f35, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_3_s.grm0_3",				0x080000, 0xab1da462, 3 | BRF_GRA },           //  8
	{ "wcb_grom1_0_s.grm1_0",				0x080000, 0xbdfafd1f, 3 | BRF_GRA },           //  9
	{ "wcb_grom1_1_s.grm1_1",				0x080000, 0x7d6baa2e, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_2_s.grm1_2",				0x080000, 0x7513d3de, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_3_s.grm1_3",				0x080000, 0xe46877e6, 3 | BRF_GRA },           // 12

	{ "wcb_srom0.srom0",					0x080000, 0xc3821cb5, 4 | BRF_SND },           // 13 Ensoniq Bank 0
	{ "wcb_srom1.srom1",					0x080000, 0x37bfa3c7, 4 | BRF_SND },           // 14

	{ "wcb_srom2.srom2",					0x080000, 0xf82c08fd, 5 | BRF_SND },           // 15 Ensoniq Bank 1
	{ "wcb_srom3.srom3",					0x020000, 0x1c2efdee, 5 | BRF_SND },           // 16
	
	{ "itbwl-3 1997 it,inc.1996",			0x001fff, 0x1461cbe0, 0 | BRF_OPT },		   // 17 pic
};

STD_ROM_PICK(wcbowl161)
STD_ROM_FN(wcbowl161)

struct BurnDriver BurnDrvWcbowl161 = {
	"wcbowl161", "wcbowl", NULL, NULL, "1995",
	"World Class Bowling (v1.61)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowl161RomInfo, wcbowl161RomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowloDIPInfo,
	WcbowlInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// World Class Bowling (v1.6)
/* Version 1.6 (PCB P/N 1083 Rev 2), This is the first set to move to the single board platform */

static struct BurnRomInfo wcbowl16RomDesc[] = {
	{ "wcb_prom0_v1.6n.prom0",				0x020000, 0x332c558f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcb_prom1_v1.6n.prom1",				0x020000, 0xc5750857, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wcb_prom2_v1.6n.prom2",				0x020000, 0x28f4ee8a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wcb_prom3_v1.6n.prom3",				0x020000, 0xf0a58979, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "wcbsnd_u88_v3.0n.u88",				0x020000, 0x45c4f659, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "wcb_grom0_0_s.grm0_0",				0x080000, 0x6fcb4246, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "wcb_grom0_1_s.grm0_1",				0x080000, 0x2ae31f45, 3 | BRF_GRA },           //  6
	{ "wcb_grom0_2_s.grm0_2",				0x080000, 0xbccc0f35, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_3_s.grm0_3",				0x080000, 0xab1da462, 3 | BRF_GRA },           //  8
	{ "wcb_grom1_0_s.grm1_0",				0x080000, 0xbdfafd1f, 3 | BRF_GRA },           //  9
	{ "wcb_grom1_1_s.grm1_1",				0x080000, 0x7d6baa2e, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_2_s.grm1_2",				0x080000, 0x7513d3de, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_3_s.grm1_3",				0x080000, 0xe46877e6, 3 | BRF_GRA },           // 12

	{ "wcb_srom0.srom0",					0x080000, 0xc3821cb5, 4 | BRF_SND },           // 13 Ensoniq Bank 0
	{ "wcb_srom1.srom1",					0x080000, 0x37bfa3c7, 4 | BRF_SND },           // 14

	{ "wcb_srom2.srom2",					0x080000, 0xf82c08fd, 5 | BRF_SND },           // 15 Ensoniq Bank 1
	{ "wcb_srom3.srom3",					0x020000, 0x1c2efdee, 5 | BRF_SND },           // 16
	
	{ "itbwl-3 1997 it,inc.1996",			0x001fff, 0x1461cbe0, 0 | BRF_OPT },		   // 17 pic
};

STD_ROM_PICK(wcbowl16)
STD_ROM_FN(wcbowl16)

struct BurnDriver BurnDrvWcbowl16 = {
	"wcbowl16", "wcbowl", NULL, NULL, "1995",
	"World Class Bowling (v1.6)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowl16RomInfo, wcbowl16RomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowloDIPInfo,
	WcbowlInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};

static INT32 Wcbowl_16B_Init()
{
	Trackball_Type = TB_TYPE1;

	return Common16BitInit();
}


// World Class Bowling (v1.5)
/* Version 1.5 (3-tier board set: P/N 1059 Rev 3, P/N 1079 Rev 1 & P/N 1060 Rev 0) */

static struct BurnRomInfo wcbowl15RomDesc[] = {
	{ "wcb_v1.5_u83.u83",					0x020000, 0x3ca9ab85, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcb_v1.5_u88.u88",					0x020000, 0xd43e6fad, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "wcb_snd_v2.0.u17",					0x020000, 0xc14907ba, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

/*	{ "wcb_grom0_0_+.grm0_0",				0x100000, 0x40837737, 3 | BRF_OPT },           //  3 Graphics (Blitter data)
	{ "wcb_grom0_1_+.grm0_1",				0x100000, 0x1615aee8, 3 | BRF_OPT },           //  4
	{ "wcb_grom0_2_+.grm0_2",				0x100000, 0xd8e0b06e, 3 | BRF_OPT },           //  5
	{ "wcb_grom0_3_+.grm0_3",				0x100000, 0x0348a7f0, 3 | BRF_OPT },           //  6*/
	{ "wcb_grom0_0.grm0_0",					0x080000, 0x5d79aaae, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_1.grm0_1",					0x080000, 0xe26dcedb, 3 | BRF_GRA },           //  8
	{ "wcb_grom0_2.grm0_2",					0x080000, 0x32735875, 3 | BRF_GRA },           //  9
	{ "wcb_grom0_3.grm0_3",					0x080000, 0x019d0ab8, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_0.grm1_0",					0x080000, 0x8bd31762, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_1.grm1_1",					0x080000, 0xb3f761fc, 3 | BRF_GRA },           // 12
	{ "wcb_grom1_2.grm1_2",					0x080000, 0xc22f44ad, 3 | BRF_GRA },           // 13
	{ "wcb_grom1_3.grm1_3",					0x080000, 0x036084c4, 3 | BRF_GRA },           // 14

	{ "ensoniq_2mx16u.rom0",				0x200000, 0x0814ab80, 4 | BRF_SND },           // 15 Ensoniq Bank 0

	{ "wcb__srom0.srom0",					0x080000, 0x115bcd1f, 6 | BRF_SND },           // 16 Ensoniq Bank 2
	{ "wcb__srom1.srom1",					0x080000, 0x87a4a4d8, 6 | BRF_SND },           // 17
};

STD_ROM_PICK(wcbowl15)
STD_ROM_FN(wcbowl15)

struct BurnDriver BurnDrvWcbowl15 = {
	"wcbowl15", "wcbowl", NULL, NULL, "1995",
	"World Class Bowling (v1.5)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowl15RomInfo, wcbowl15RomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowlDIPInfo,
	Wcbowl_16B_Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// World Class Bowling (v1.4)
/* Version 1.4 (3-tier board set: P/N 1059 Rev 3, P/N 1079 Rev 1 & P/N 1060 Rev 0) */

static struct BurnRomInfo wcbowl14RomDesc[] = {
	{ "wcb_v1.4_u83.u83",					0x020000, 0x7086131f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcb_v1.4_u88.u88",					0x020000, 0x0225aac1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "wcb_snd_v2.0.u17",					0x020000, 0xc14907ba, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

/*	{ "wcb_grom0_0_+.grm0_0",				0x100000, 0x40837737, 3 | BRF_OPT },           //  3 Graphics (Blitter data)
	{ "wcb_grom0_1_+.grm0_1",				0x100000, 0x1615aee8, 3 | BRF_OPT },           //  4
	{ "wcb_grom0_2_+.grm0_2",				0x100000, 0xd8e0b06e, 3 | BRF_OPT },           //  5
	{ "wcb_grom0_3_+.grm0_3",				0x100000, 0x0348a7f0, 3 | BRF_OPT },           //  6*/
	{ "wcb_grom0_0.grm0_0",					0x080000, 0x5d79aaae, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_1.grm0_1",					0x080000, 0xe26dcedb, 3 | BRF_GRA },           //  8
	{ "wcb_grom0_2.grm0_2",					0x080000, 0x32735875, 3 | BRF_GRA },           //  9
	{ "wcb_grom0_3.grm0_3",					0x080000, 0x019d0ab8, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_0.grm1_0",					0x080000, 0x8bd31762, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_1.grm1_1",					0x080000, 0xb3f761fc, 3 | BRF_GRA },           // 12
	{ "wcb_grom1_2.grm1_2",					0x080000, 0xc22f44ad, 3 | BRF_GRA },           // 13
	{ "wcb_grom1_3.grm1_3",					0x080000, 0x036084c4, 3 | BRF_GRA },           // 14

	{ "ensoniq_2mx16u.rom0",				0x200000, 0x0814ab80, 4 | BRF_SND },           // 15 Ensoniq Bank 0

	{ "wcb__srom0.srom0",					0x080000, 0x115bcd1f, 6 | BRF_SND },           // 16 Ensoniq Bank 2
	{ "wcb__srom1.srom1",					0x080000, 0x87a4a4d8, 6 | BRF_SND },           // 17
};

STD_ROM_PICK(wcbowl14)
STD_ROM_FN(wcbowl14)

struct BurnDriver BurnDrvWcbowl14 = {
	"wcbowl14", "wcbowl", NULL, NULL, "1995",
	"World Class Bowling (v1.4)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowl14RomInfo, wcbowl14RomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowlDIPInfo,
	Wcbowl_16B_Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// World Class Bowling (v1.3)
/* Version 1.3 (3-tier board set: P/N 1059 Rev 3, P/N 1079 Rev 1 & P/N 1060 Rev 0) */

static struct BurnRomInfo wcbowl13RomDesc[] = {
	{ "wcb_v1.3_u83.u83",					0x020000, 0x2b6d284e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcb_v1.3_u88.u88",					0x020000, 0x039af877, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "wcb_snd_v2.0.u17",					0x020000, 0xc14907ba, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

/*	{ "wcb_grom0_0_+.grm0_0",				0x100000, 0x40837737, 3 | BRF_OPT },           //  3 Graphics (Blitter data)
	{ "wcb_grom0_1_+.grm0_1",				0x100000, 0x1615aee8, 3 | BRF_OPT },           //  4
	{ "wcb_grom0_2_+.grm0_2",				0x100000, 0xd8e0b06e, 3 | BRF_OPT },           //  5
	{ "wcb_grom0_3_+.grm0_3",				0x100000, 0x0348a7f0, 3 | BRF_OPT },           //  6*/
	{ "wcb_grom0_0.grm0_0",					0x080000, 0x5d79aaae, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_1.grm0_1",					0x080000, 0xe26dcedb, 3 | BRF_GRA },           //  8
	{ "wcb_grom0_2.grm0_2",					0x080000, 0x32735875, 3 | BRF_GRA },           //  9
	{ "wcb_grom0_3.grm0_3",					0x080000, 0x019d0ab8, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_0.grm1_0",					0x080000, 0x8bd31762, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_1.grm1_1",					0x080000, 0xb3f761fc, 3 | BRF_GRA },           // 12
	{ "wcb_grom1_2.grm1_2",					0x080000, 0xc22f44ad, 3 | BRF_GRA },           // 13
	{ "wcb_grom1_3.grm1_3",					0x080000, 0x036084c4, 3 | BRF_GRA },           // 14

	{ "ensoniq_2mx16u.rom0",				0x200000, 0x0814ab80, 4 | BRF_SND },           // 15 Ensoniq Bank 0

	{ "wcb__srom0.srom0",					0x080000, 0x115bcd1f, 6 | BRF_SND },           // 16 Ensoniq Bank 2
	{ "wcb__srom1.srom1",					0x080000, 0x87a4a4d8, 6 | BRF_SND },           // 17
};

STD_ROM_PICK(wcbowl13)
STD_ROM_FN(wcbowl13)

struct BurnDriver BurnDrvWcbowl13 = {
	"wcbowl13", "wcbowl", NULL, NULL, "1995",
	"World Class Bowling (v1.3)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowl13RomInfo, wcbowl13RomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowlDIPInfo,
	Wcbowl_16B_Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// World Class Bowling (v1.3J, Japan)
/* Version 1.3 (3-tier board set: P/N 1059 Rev 3, P/N 1079 Rev 1 & P/N 1060 Rev 0) */

static struct BurnRomInfo wcbowl13jRomDesc[] = {
	{ "wcb_v1.3j_u83.u83",					0x020000, 0x5805fd92, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcb_v1.3j_u88.u88",					0x020000, 0xb846660e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "wcb_snd_v2.0.u17",					0x020000, 0xc14907ba, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

/*	{ "wcb_grom0_0_+.grm0_0",				0x100000, 0x40837737, 3 | BRF_OPT },           //  3 Graphics (Blitter data)
	{ "wcb_grom0_1_+.grm0_1",				0x100000, 0x1615aee8, 3 | BRF_OPT },           //  4
	{ "wcb_grom0_2_+.grm0_2",				0x100000, 0xd8e0b06e, 3 | BRF_OPT },           //  5
	{ "wcb_grom0_3_+.grm0_3",				0x100000, 0x0348a7f0, 3 | BRF_OPT },           //  6*/
	{ "wcb_grom0_0.grm0_0",					0x080000, 0x5d79aaae, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_1.grm0_1",					0x080000, 0xe26dcedb, 3 | BRF_GRA },           //  8
	{ "wcb_grom0_2.grm0_2",					0x080000, 0x32735875, 3 | BRF_GRA },           //  9
	{ "wcb_grom0_3.grm0_3",					0x080000, 0x019d0ab8, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_0.grm1_0",					0x080000, 0x8bd31762, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_1.grm1_1",					0x080000, 0xb3f761fc, 3 | BRF_GRA },           // 12
	{ "wcb_grom1_2.grm1_2",					0x080000, 0xc22f44ad, 3 | BRF_GRA },           // 13
	{ "wcb_grom1_3.grm1_3",					0x080000, 0x036084c4, 3 | BRF_GRA },           // 14

	{ "ensoniq_2mx16u.rom0",				0x200000, 0x0814ab80, 4 | BRF_SND },           // 15 Ensoniq Bank 0

	{ "wcb__srom0.srom0",					0x080000, 0x115bcd1f, 6 | BRF_SND },           // 16 Ensoniq Bank 2
	{ "wcb__srom1.srom1",					0x080000, 0x87a4a4d8, 6 | BRF_SND },           // 17
};

STD_ROM_PICK(wcbowl13j)
STD_ROM_FN(wcbowl13j)

struct BurnDriver BurnDrvWcbowl13j = {
	"wcbowl13j", "wcbowl", NULL, NULL, "1995",
	"World Class Bowling (v1.3J, Japan)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowl13jRomInfo, wcbowl13jRomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowljDIPInfo,
	Wcbowl_16B_Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// World Class Bowling (v1.2)
/* Version 1.2 (3-tier board set: P/N 1059 Rev 3, P/N 1079 Rev 1 & P/N 1060 Rev 0) */

static struct BurnRomInfo wcbowl12RomDesc[] = {
	{ "wcb_v1.2_u83.u83",					0x020000, 0x0602c5ce, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcb_v1.2_u88.u88",					0x020000, 0x49573493, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "wcb_snd_v2.0.u17",					0x020000, 0xc14907ba, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

/*	{ "wcb_grom0_0_+.grm0_0",				0x100000, 0x40837737, 3 | BRF_OPT },           //  3 Graphics (Blitter data)
	{ "wcb_grom0_1_+.grm0_1",				0x100000, 0x1615aee8, 3 | BRF_OPT },           //  4
	{ "wcb_grom0_2_+.grm0_2",				0x100000, 0xd8e0b06e, 3 | BRF_OPT },           //  5
	{ "wcb_grom0_3_+.grm0_3",				0x100000, 0x0348a7f0, 3 | BRF_OPT },           //  6*/
	{ "wcb_grom0_0.grm0_0",					0x080000, 0x5d79aaae, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_1.grm0_1",					0x080000, 0xe26dcedb, 3 | BRF_GRA },           //  8
	{ "wcb_grom0_2.grm0_2",					0x080000, 0x32735875, 3 | BRF_GRA },           //  9
	{ "wcb_grom0_3.grm0_3",					0x080000, 0x019d0ab8, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_0.grm1_0",					0x080000, 0x8bd31762, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_1.grm1_1",					0x080000, 0xb3f761fc, 3 | BRF_GRA },           // 12
	{ "wcb_grom1_2.grm1_2",					0x080000, 0xc22f44ad, 3 | BRF_GRA },           // 13
	{ "wcb_grom1_3.grm1_3",					0x080000, 0x036084c4, 3 | BRF_GRA },           // 14

	{ "ensoniq_2mx16u.rom0",				0x200000, 0x0814ab80, 4 | BRF_SND },           // 15 Ensoniq Bank 0

	{ "wcb__srom0.srom0",					0x080000, 0x115bcd1f, 6 | BRF_SND },           // 16 Ensoniq Bank 2
	{ "wcb__srom1.srom1",					0x080000, 0x87a4a4d8, 6 | BRF_SND },           // 17
};

STD_ROM_PICK(wcbowl12)
STD_ROM_FN(wcbowl12)

struct BurnDriver BurnDrvWcbowl12 = {
	"wcbowl12", "wcbowl", NULL, NULL, "1995",
	"World Class Bowling (v1.2)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowl12RomInfo, wcbowl12RomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowlDIPInfo,
	Wcbowl_16B_Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// World Class Bowling (v1.1)
/* Version 1.1 (3-tier board set: P/N 1059 Rev 3, P/N 1079 Rev 1 & P/N 1060 Rev 0) */

static struct BurnRomInfo wcbowl11RomDesc[] = {
	{ "wcb_v1.1_u83.u83",					0x020000, 0xd4902392, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcb_v1.1_u88.u88",					0x020000, 0xea81a95c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "wcb_snd_v1.0.u17",					0x020000, 0x28f14071, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

/*	{ "wcb_grom0_0_+.grm0_0",				0x100000, 0x40837737, 3 | BRF_OPT },           //  3 Graphics (Blitter data)
	{ "wcb_grom0_1_+.grm0_1",				0x100000, 0x1615aee8, 3 | BRF_OPT },           //  4
	{ "wcb_grom0_2_+.grm0_2",				0x100000, 0xd8e0b06e, 3 | BRF_OPT },           //  5
	{ "wcb_grom0_3_+.grm0_3",				0x100000, 0x0348a7f0, 3 | BRF_OPT },           //  6*/
	{ "wcb_grom0_0.grm0_0",					0x080000, 0x5d79aaae, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_1.grm0_1",					0x080000, 0xe26dcedb, 3 | BRF_GRA },           //  8
	{ "wcb_grom0_2.grm0_2",					0x080000, 0x32735875, 3 | BRF_GRA },           //  9
	{ "wcb_grom0_3.grm0_3",					0x080000, 0x019d0ab8, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_0.grm1_0",					0x080000, 0x8bd31762, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_1.grm1_1",					0x080000, 0xb3f761fc, 3 | BRF_GRA },           // 12
	{ "wcb_grom1_2.grm1_2",					0x080000, 0xc22f44ad, 3 | BRF_GRA },           // 13
	{ "wcb_grom1_3.grm1_3",					0x080000, 0x036084c4, 3 | BRF_GRA },           // 14

	{ "ensoniq_2m.rom0",					0x200000, 0x9fdc4825, 4 | BRF_SND },           // 15 Ensoniq Bank 0

	{ "wcb__srom0.srom0",					0x080000, 0x115bcd1f, 6 | BRF_SND },           // 16 Ensoniq Bank 2
	{ "wcb__srom1.srom1",					0x080000, 0x87a4a4d8, 6 | BRF_SND },           // 17
};

STD_ROM_PICK(wcbowl11)
STD_ROM_FN(wcbowl11)

struct BurnDriver BurnDrvWcbowl11 = {
	"wcbowl11", "wcbowl", NULL, NULL, "1995",
	"World Class Bowling (v1.1)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowl11RomInfo, wcbowl11RomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowlDIPInfo,
	Wcbowl_16B_Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// World Class Bowling (v1.0)
/* Version 1.0 (3-tier board set: P/N 1059 Rev 3, P/N 1079 Rev 1 & P/N 1060 Rev 0) */

static struct BurnRomInfo wcbowl10RomDesc[] = {
	{ "wcb_v1.0_u83.u83",					0x020000, 0x675ad0b1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "wcb_v1.0_u88.u88",					0x020000, 0x3afbec1c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "wcb_snd_v1.0.u17",					0x020000, 0x28f14071, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

/*	{ "wcb_grom0_0_+.grm0_0",				0x100000, 0x40837737, 3 | BRF_OPT },           //  3 Graphics (Blitter data)
	{ "wcb_grom0_1_+.grm0_1",				0x100000, 0x1615aee8, 3 | BRF_OPT },           //  4
	{ "wcb_grom0_2_+.grm0_2",				0x100000, 0xd8e0b06e, 3 | BRF_OPT },           //  5
	{ "wcb_grom0_3_+.grm0_3",				0x100000, 0x0348a7f0, 3 | BRF_OPT },           //  6*/
	{ "wcb_grom0_0.grm0_0",					0x080000, 0x5d79aaae, 3 | BRF_GRA },           //  7
	{ "wcb_grom0_1.grm0_1",					0x080000, 0xe26dcedb, 3 | BRF_GRA },           //  8
	{ "wcb_grom0_2.grm0_2",					0x080000, 0x32735875, 3 | BRF_GRA },           //  9
	{ "wcb_grom0_3.grm0_3",					0x080000, 0x019d0ab8, 3 | BRF_GRA },           // 10
	{ "wcb_grom1_0.grm1_0",					0x080000, 0x8bd31762, 3 | BRF_GRA },           // 11
	{ "wcb_grom1_1.grm1_1",					0x080000, 0xb3f761fc, 3 | BRF_GRA },           // 12
	{ "wcb_grom1_2.grm1_2",					0x080000, 0xc22f44ad, 3 | BRF_GRA },           // 13
	{ "wcb_grom1_3.grm1_3",					0x080000, 0x036084c4, 3 | BRF_GRA },           // 14

	{ "ensoniq_2m.rom0",					0x200000, 0x9fdc4825, 4 | BRF_SND },           // 15 Ensoniq Bank 0

	{ "wcb__srom0.srom0",					0x080000, 0x115bcd1f, 6 | BRF_SND },           // 16 Ensoniq Bank 2
	{ "wcb__srom1.srom1",					0x080000, 0x87a4a4d8, 6 | BRF_SND },           // 17
};

STD_ROM_PICK(wcbowl10)
STD_ROM_FN(wcbowl10)

struct BurnDriver BurnDrvWcbowl10 = {
	"wcbowl10", "wcbowl", NULL, NULL, "1995",
	"World Class Bowling (v1.0)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wcbowl10RomInfo, wcbowl10RomName, NULL, NULL, NULL, NULL, WcbowlInputInfo, WcbowlDIPInfo,
	Wcbowl_16B_Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Driver's Edge (v1.6)

static struct BurnRomInfo drivedgeRomDesc[] = {
	{ "de_v1.6-u130.bin",					0x008000, 0x873579b0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
		
	{ "desndu17.bin",						0x020000, 0x6e8ca8bc, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "de-grom0.bin",						0x080000, 0x7fe5b01b, 3 | BRF_GRA },           //  3 Graphics (Blitter data)
	{ "de-grom5.bin",						0x080000, 0x5ea6dbc2, 3 | BRF_GRA },           //  4
	{ "de-grm10.bin",						0x080000, 0x76be06cd, 3 | BRF_GRA },           //  5
	{ "de-grm15.bin",						0x080000, 0x119bf46b, 3 | BRF_GRA },           //  6
	{ "de-grom1.bin",						0x080000, 0x5b88e8b7, 3 | BRF_GRA },           //  7
	{ "de-grom6.bin",						0x080000, 0x2cb76b9a, 3 | BRF_GRA },           //  8
	{ "de-grm11.bin",						0x080000, 0x5d29018c, 3 | BRF_GRA },           //  9
	{ "de-grm16.bin",						0x080000, 0x476940fb, 3 | BRF_GRA },           // 10
	{ "de-grom2.bin",						0x080000, 0x5ccc4c62, 3 | BRF_GRA },           // 11
	{ "de-grom7.bin",						0x080000, 0x45367070, 3 | BRF_GRA },           // 12
	{ "de-grm12.bin",						0x080000, 0xb978ef5a, 3 | BRF_GRA },           // 13
	{ "de-grm17.bin",						0x080000, 0xeff8abac, 3 | BRF_GRA },           // 14
	{ "de-grom3.bin",						0x020000, 0x9cd252c9, 3 | BRF_GRA },           // 15
	{ "de-grom8.bin",						0x020000, 0x43f10ca4, 3 | BRF_GRA },           // 16
	{ "de-grm13.bin",						0x020000, 0x431d131e, 3 | BRF_GRA },           // 17
	{ "de-grm18.bin",						0x020000, 0xb09e0d9c, 3 | BRF_GRA },           // 18
	{ "de-grom4.bin",						0x020000, 0xc499cdfa, 3 | BRF_GRA },           // 19
	{ "de-grom9.bin",						0x020000, 0xe5f21566, 3 | BRF_GRA },           // 20
	{ "de-grm14.bin",						0x020000, 0x09dbe382, 3 | BRF_GRA },           // 21
	{ "de-grm19.bin",						0x020000, 0x4ced78e1, 3 | BRF_GRA },           // 22

	{ "ensoniq.2m",							0x200000, 0x9fdc4825, 4 | BRF_SND },           // 23 Ensoniq Bank 0

	{ "de-srom0.bin",						0x080000, 0x649c685f, 6 | BRF_SND },           // 24 Ensoniq Bank 2
	{ "de-srom1.bin",						0x080000, 0xdf4fff97, 6 | BRF_SND },           // 25

	{ "de-u7net.bin",						0x008000, 0xdea8b9de, 8 | BRF_PRG | BRF_ESS }, //  2 cpu4
};

STD_ROM_PICK(drivedge)
STD_ROM_FN(drivedge)

static INT32 DrivedgeInit()
{
	bprintf (0, _T("Called without proper init!!\n"));
	return 0;
}

static INT32 DrivedgeExit()
{
	return 0;
}

struct BurnDriverX BurnDrvDrivedge = {
	"drivedge", NULL, NULL, NULL, "1994",
	"Driver's Edge (v1.6)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, drivedgeRomInfo, drivedgeRomName, NULL, NULL, NULL, NULL, NULL, NULL, //DrivedgeInputInfo, DrivedgeDIPInfo,
	DrivedgeInit, DrivedgeExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Street Fighter: The Movie (v1.12)

static struct BurnRomInfo sftmRomDesc[] = {
	{ "sfm_0_v1.12.prom0",					0x040000, 0x9d09355c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sfm_1_v1.12.prom1",					0x040000, 0xa58ac6a9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sfm_2_v1.12.prom2",					0x040000, 0x2f21a4f6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sfm_3_v1.12.prom3",					0x040000, 0xd26648d9, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sfm_snd_v1.u23",						0x040000, 0x10d85366, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "rm0-0.grm0_0",						0x400000, 0x09ef29cb, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "rm0-1.grm0_1",						0x400000, 0x6f5910fa, 3 | BRF_GRA },           //  6
	{ "rm0-2.grm0_2",						0x400000, 0xb8a2add5, 3 | BRF_GRA },           //  7
	{ "rm0-3.grm0_3",						0x400000, 0x6b6ff867, 3 | BRF_GRA },           //  8
	{ "rm1-0.grm1_0",						0x400000, 0xd5d65f77, 3 | BRF_GRA },           //  9
	{ "rm1-1.grm1_1",						0x400000, 0x90467e27, 3 | BRF_GRA },           // 10
	{ "rm1-2.grm1_2",						0x400000, 0x903e56c2, 3 | BRF_GRA },           // 11
	{ "rm1-3.grm1_3",						0x400000, 0xfac35686, 3 | BRF_GRA },           // 12
	{ "sfm_grm3_0.grm3_0",					0x020000, 0x3e1f76f7, 3 | BRF_GRA },           // 13
	{ "sfm_grm3_1.grm3_1",					0x020000, 0x578054b6, 3 | BRF_GRA },           // 14
	{ "sfm_grm3_2.grm3_2",					0x020000, 0x9af2f698, 3 | BRF_GRA },           // 15
	{ "sfm_grm3_3.grm3_3",					0x020000, 0xcd38d1d6, 3 | BRF_GRA },           // 16

	{ "sfm_srom0.srom0",					0x200000, 0x6ca1d3fc, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "sfm_srom3.srom3",					0x080000, 0x4f181534, 7 | BRF_SND },           // 18 Ensoniq Bank 3
};

STD_ROM_PICK(sftm)
STD_ROM_FN(sftm)

static INT32 SftmInit()
{
	return Common32BitInit(0x7a6a, 1, 0);
}

struct BurnDriver BurnDrvSftm = {
	"sftm", NULL, NULL, NULL, "1995",
	"Street Fighter: The Movie (v1.12)\0", NULL, "Capcom / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, sftmRomInfo, sftmRomName, NULL, NULL, NULL, NULL, SftmInputInfo, SftmDIPInfo,
	SftmInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Street Fighter: The Movie (v1.11)

static struct BurnRomInfo sftm111RomDesc[] = {
	{ "sfm_0_v1.11.prom0",					0x040000, 0x28187ddc, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sfm_1_v1.11.prom1",					0x040000, 0xec2ce6fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sfm_2_v1.11.prom2",					0x040000, 0xbe20510e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sfm_3_v1.11.prom3",					0x040000, 0xeead342f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sfm_snd_v1.u23",						0x040000, 0x10d85366, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "rm0-0.grm0_0",						0x400000, 0x09ef29cb, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "rm0-1.grm0_1",						0x400000, 0x6f5910fa, 3 | BRF_GRA },           //  6
	{ "rm0-2.grm0_2",						0x400000, 0xb8a2add5, 3 | BRF_GRA },           //  7
	{ "rm0-3.grm0_3",						0x400000, 0x6b6ff867, 3 | BRF_GRA },           //  8
	{ "rm1-0.grm1_0",						0x400000, 0xd5d65f77, 3 | BRF_GRA },           //  9
	{ "rm1-1.grm1_1",						0x400000, 0x90467e27, 3 | BRF_GRA },           // 10
	{ "rm1-2.grm1_2",						0x400000, 0x903e56c2, 3 | BRF_GRA },           // 11
	{ "rm1-3.grm1_3",						0x400000, 0xfac35686, 3 | BRF_GRA },           // 12
	{ "sfm_grm3_0.grm3_0",					0x020000, 0x3e1f76f7, 3 | BRF_GRA },           // 13
	{ "sfm_grm3_1.grm3_1",					0x020000, 0x578054b6, 3 | BRF_GRA },           // 14
	{ "sfm_grm3_2.grm3_2",					0x020000, 0x9af2f698, 3 | BRF_GRA },           // 15
	{ "sfm_grm3_3.grm3_3",					0x020000, 0xcd38d1d6, 3 | BRF_GRA },           // 16

	{ "sfm_srom0.srom0",					0x200000, 0x6ca1d3fc, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "sfm_srom3.srom3",					0x080000, 0x4f181534, 7 | BRF_SND },           // 18 Ensoniq Bank 3
};

STD_ROM_PICK(sftm111)
STD_ROM_FN(sftm111)

static INT32 Sftm110Init()
{
	return Common32BitInit(0x7a66, 1, 0);
}

struct BurnDriver BurnDrvSftm111 = {
	"sftm111", "sftm", NULL, NULL, "1995",
	"Street Fighter: The Movie (v1.11)\0", NULL, "Capcom / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, sftm111RomInfo, sftm111RomName, NULL, NULL, NULL, NULL, SftmInputInfo, SftmDIPInfo,
	Sftm110Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Street Fighter: The Movie (v1.10)

static struct BurnRomInfo sftm110RomDesc[] = {
	{ "sfm_0_v1.1.prom0",					0x040000, 0x00c0c63c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sfm_1_v1.1.prom1",					0x040000, 0xd4d2a67e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sfm_2_v1.1.prom2",					0x040000, 0xd7b36c92, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sfm_3_v1.1.prom3",					0x040000, 0xbe3efdbd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sfm_snd_v1.u23",						0x040000, 0x10d85366, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "rm0-0.grm0_0",						0x400000, 0x09ef29cb, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "rm0-1.grm0_1",						0x400000, 0x6f5910fa, 3 | BRF_GRA },           //  6
	{ "rm0-2.grm0_2",						0x400000, 0xb8a2add5, 3 | BRF_GRA },           //  7
	{ "rm0-3.grm0_3",						0x400000, 0x6b6ff867, 3 | BRF_GRA },           //  8
	{ "rm1-0.grm1_0",						0x400000, 0xd5d65f77, 3 | BRF_GRA },           //  9
	{ "rm1-1.grm1_1",						0x400000, 0x90467e27, 3 | BRF_GRA },           // 10
	{ "rm1-2.grm1_2",						0x400000, 0x903e56c2, 3 | BRF_GRA },           // 11
	{ "rm1-3.grm1_3",						0x400000, 0xfac35686, 3 | BRF_GRA },           // 12
	{ "sfm_grm3_0.grm3_0",					0x020000, 0x3e1f76f7, 3 | BRF_GRA },           // 13
	{ "sfm_grm3_1.grm3_1",					0x020000, 0x578054b6, 3 | BRF_GRA },           // 14
	{ "sfm_grm3_2.grm3_2",					0x020000, 0x9af2f698, 3 | BRF_GRA },           // 15
	{ "sfm_grm3_3.grm3_3",					0x020000, 0xcd38d1d6, 3 | BRF_GRA },           // 16

	{ "sfm_srom0.srom0",					0x200000, 0x6ca1d3fc, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "sfm_srom3.srom3",					0x080000, 0x4f181534, 7 | BRF_SND },           // 18 Ensoniq Bank 3
};

STD_ROM_PICK(sftm110)
STD_ROM_FN(sftm110)

struct BurnDriver BurnDrvSftm110 = {
	"sftm110", "sftm", NULL, NULL, "1995",
	"Street Fighter: The Movie (v1.10)\0", NULL, "Capcom / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, sftm110RomInfo, sftm110RomName, NULL, NULL, NULL, NULL, SftmInputInfo, SftmDIPInfo,
	Sftm110Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Street Fighter: The Movie (v1.14N, Japan)

static struct BurnRomInfo sftmj114RomDesc[] = {
	{ "sfmn_0_v1.14.prom0",					0x040000, 0x2a0c0bb7, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sfmn_1_v1.14.prom1",					0x040000, 0x088aa12c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sfmn_2_v1.14.prom2",					0x040000, 0x7120836e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sfmn_3_v1.14.prom3",					0x040000, 0x84eb200d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sfm_snd_v1.11.u23",					0x040000, 0x004854ed, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "rm0-0.grm0_0",						0x400000, 0x09ef29cb, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "rm0-1.grm0_1",						0x400000, 0x6f5910fa, 3 | BRF_GRA },           //  6
	{ "rm0-2.grm0_2",						0x400000, 0xb8a2add5, 3 | BRF_GRA },           //  7
	{ "rm0-3.grm0_3",						0x400000, 0x6b6ff867, 3 | BRF_GRA },           //  8
	{ "rm1-0.grm1_0",						0x400000, 0xd5d65f77, 3 | BRF_GRA },           //  9
	{ "rm1-1.grm1_1",						0x400000, 0x90467e27, 3 | BRF_GRA },           // 10
	{ "rm1-2.grm1_2",						0x400000, 0x903e56c2, 3 | BRF_GRA },           // 11
	{ "rm1-3.grm1_3",						0x400000, 0xfac35686, 3 | BRF_GRA },           // 12
	{ "sfm_grm3_0.grm3_0",					0x020000, 0x3e1f76f7, 3 | BRF_GRA },           // 13
	{ "sfm_grm3_1.grm3_1",					0x020000, 0x578054b6, 3 | BRF_GRA },           // 14
	{ "sfm_grm3_2.grm3_2",					0x020000, 0x9af2f698, 3 | BRF_GRA },           // 15
	{ "sfm_grm3_3.grm3_3",					0x020000, 0xcd38d1d6, 3 | BRF_GRA },           // 16

	{ "sfm_srom0.srom0",					0x200000, 0x6ca1d3fc, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "sfm_srom3.srom3",					0x080000, 0x4f181534, 7 | BRF_SND },           // 18 Ensoniq Bank 3
};

STD_ROM_PICK(sftmj114)
STD_ROM_FN(sftmj114)

struct BurnDriver BurnDrvSftmj114 = {
	"sftmj114", "sftm", NULL, NULL, "1995",
	"Street Fighter: The Movie (v1.14N, Japan)\0", NULL, "Capcom / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, sftmj114RomInfo, sftmj114RomName, NULL, NULL, NULL, NULL, SftmInputInfo, SftmDIPInfo,
	SftmInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Street Fighter: The Movie (v1.12N, Japan)

static struct BurnRomInfo sftmj112RomDesc[] = {
	{ "sfmn_0_v1.12.prom0",					0x040000, 0x640a04a8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sfmn_1_v1.12.prom1",					0x040000, 0x2a27b690, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sfmn_2_v1.12.prom2",					0x040000, 0xcec1dd7b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sfmn_3_v1.12.prom3",					0x040000, 0x48fa60f4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sfm_snd_v1.11.u23",					0x040000, 0x004854ed, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "rm0-0.grm0_0",						0x400000, 0x09ef29cb, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "rm0-1.grm0_1",						0x400000, 0x6f5910fa, 3 | BRF_GRA },           //  6
	{ "rm0-2.grm0_2",						0x400000, 0xb8a2add5, 3 | BRF_GRA },           //  7
	{ "rm0-3.grm0_3",						0x400000, 0x6b6ff867, 3 | BRF_GRA },           //  8
	{ "rm1-0.grm1_0",						0x400000, 0xd5d65f77, 3 | BRF_GRA },           //  9
	{ "rm1-1.grm1_1",						0x400000, 0x90467e27, 3 | BRF_GRA },           // 10
	{ "rm1-2.grm1_2",						0x400000, 0x903e56c2, 3 | BRF_GRA },           // 11
	{ "rm1-3.grm1_3",						0x400000, 0xfac35686, 3 | BRF_GRA },           // 12
	{ "sfm_grm3_0.grm3_0",					0x020000, 0x3e1f76f7, 3 | BRF_GRA },           // 13
	{ "sfm_grm3_1.grm3_1",					0x020000, 0x578054b6, 3 | BRF_GRA },           // 14
	{ "sfm_grm3_2.grm3_2",					0x020000, 0x9af2f698, 3 | BRF_GRA },           // 15
	{ "sfm_grm3_3.grm3_3",					0x020000, 0xcd38d1d6, 3 | BRF_GRA },           // 16

	{ "sfm_srom0.srom0",					0x200000, 0x6ca1d3fc, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "sfm_srom3.srom3",					0x080000, 0x4f181534, 7 | BRF_SND },           // 18 Ensoniq Bank 3
};

STD_ROM_PICK(sftmj112)
STD_ROM_FN(sftmj112)

struct BurnDriver BurnDrvSftmj112 = {
	"sftmj112", "sftm", NULL, NULL, "1995",
	"Street Fighter: The Movie (v1.12N, Japan)\0", NULL, "Capcom / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, sftmj112RomInfo, sftmj112RomName, NULL, NULL, NULL, NULL, SftmInputInfo, SftmDIPInfo,
	SftmInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Must Shoot TV (prototype)

static struct BurnRomInfo shoottvRomDesc[] = {
	{ "gun_0.bin",		0x00c5f9, 0x1086b219, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gun_1.bin",		0x00c5f9, 0xa0f0e5ea, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gun_2.bin",		0x00c5f9, 0x1b84cf05, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gun_3.bin",		0x00c5f9, 0x43ed58aa, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gun.bim",		0x020000, 0x7439569a, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "grom00_0.bin",	0x080000, 0x9a06d497, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "grom00_1.bin",	0x080000, 0x018ff629, 3 | BRF_GRA },           //  6
	{ "grom00_2.bin",	0x080000, 0xf47ea010, 3 | BRF_GRA },           //  7
	{ "grom00_3.bin",	0x080000, 0x3c12be47, 3 | BRF_GRA },           //  8
	{ "grom01_0.bin",	0x04fdd5, 0xebf70a20, 3 | BRF_GRA },           //  9
	{ "grom01_1.bin",	0x04fdd5, 0xa78fedd1, 3 | BRF_GRA },           // 10
	{ "grom01_2.bin",	0x04fdd5, 0x3578d74d, 3 | BRF_GRA },           // 11
	{ "grom01_3.bin",	0x04fdd5, 0x394be494, 3 | BRF_GRA },           // 12

	{ "guns0.bin",		0x07fb51, 0x35e9ba70, 4 | BRF_SND },           // 13 Ensoniq Bank 0
	{ "guns1.bin",		0x03dccd, 0xec1c3ab3, 4 | BRF_SND },           // 14
};

STD_ROM_PICK(shoottv)
STD_ROM_FN(shoottv)

static INT32 ShoottvInit()
{
	is_shoottv = 1;

	return Common32BitInit(0x0000, 2, 0);
}

struct BurnDriver BurnDrvShoottv = {
	"shoottv", NULL, NULL, NULL, "199?",
	"Must Shoot TV (prototype)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, shoottvRomInfo, shoottvRomName, NULL, NULL, NULL, NULL, ShoottvInputInfo, ShoottvDIPInfo,
	ShoottvInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Power Up Baseball (prototype)

static struct BurnRomInfo pubballRomDesc[] = {
	{ "bb0.bin",		0x025a91, 0xf9350590, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "bb1.bin",		0x025a91, 0x5711f503, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bb2.bin",		0x025a91, 0x3e202dc6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bb3.bin",		0x025a91, 0xe6d1ba8d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bball.bim",		0x020000, 0x915a9116, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "grom00_0.bin",	0x100000, 0x46822b0f, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "grom00_1.bin",	0x100000, 0xc11236ce, 3 | BRF_GRA },           //  6
	{ "grom00_2.bin",	0x100000, 0xe6be30f3, 3 | BRF_GRA },           //  7
	{ "grom00_3.bin",	0x100000, 0xe0d454fb, 3 | BRF_GRA },           //  8
	{ "grom01_0.bin",	0x100000, 0x115a66f2, 3 | BRF_GRA },           //  9
	{ "grom01_1.bin",	0x100000, 0x1dfc8dbd, 3 | BRF_GRA },           // 10
	{ "grom01_2.bin",	0x100000, 0x23386483, 3 | BRF_GRA },           // 11
	{ "grom01_3.bin",	0x100000, 0xac0123ce, 3 | BRF_GRA },           // 12
	{ "grom02_0.bin",	0x100000, 0x06cd3cca, 3 | BRF_GRA },           // 13
	{ "grom02_1.bin",	0x100000, 0x3df38e91, 3 | BRF_GRA },           // 14
	{ "grom02_2.bin",	0x100000, 0x7c47dde9, 3 | BRF_GRA },           // 15
	{ "grom02_3.bin",	0x100000, 0xe6eb01bf, 3 | BRF_GRA },           // 16
	{ "grom3_0.bin",	0x080000, 0x376beb10, 3 | BRF_GRA },           // 17
	{ "grom3_1.bin",	0x080000, 0x3b3cb8ba, 3 | BRF_GRA },           // 18
	{ "grom3_2.bin",	0x080000, 0x3bdfac73, 3 | BRF_GRA },           // 19
	{ "grom3_3.bin",	0x080000, 0x1de04025, 3 | BRF_GRA },           // 20

	{ "bbsrom0.bin",	0x0f8b8e, 0x2a69f77e, 4 | BRF_SND },           // 21 Ensoniq Bank 0
	{ "bbsrom1.bin",	0x0fe442, 0x4af8c871, 4 | BRF_SND },           // 22

	{ "bbsrom2.bin",	0x0fe62d, 0xe4f9ad52, 5 | BRF_SND },           // 23 Ensoniq Bank 1
	{ "bbsrom3.bin",	0x0f90b3, 0xb37e2906, 5 | BRF_SND },           // 24
};

STD_ROM_PICK(pubball)
STD_ROM_FN(pubball)

static INT32 PubballInit()
{
	Trackball_Type = TB_TYPE0;
	is_pubball = 1;

	return Common32BitInit(0x10000, 2, 0);
}

struct BurnDriver BurnDrvPubball = {
	"pubball", NULL, NULL, NULL, "1996",
	"Power Up Baseball (prototype)\0", NULL, "Midway / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pubballRomInfo, pubballRomName, NULL, NULL, NULL, NULL, PubballInputInfo, PubballDIPInfo,
	PubballInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Shuffleshot (v1.40)
/* Version 1.40 (PCB P/N 1083 Rev 2) */

static struct BurnRomInfo shufshotRomDesc[] = {
	{ "shot_prom0_v1.40.prom0",				0x020000, 0x33c0c98b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "shot_prom1_v1.40.prom1",				0x020000, 0xd30a8831, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "shot_prom2_v1.40.prom2",				0x020000, 0xea10ada8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "shot_prom3_v1.40.prom3",				0x020000, 0x4b28f28b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "shotsnd_u88_v1.1.u88",				0x020000, 0xe37d599d, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "shf_grom0_0.grm0_0",					0x080000, 0x832a3d6a, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "shf_grom0_1.grm0_1",					0x080000, 0x155e48a2, 3 | BRF_GRA },           //  6
	{ "shf_grom0_2.grm0_2",					0x080000, 0x9f2b470d, 3 | BRF_GRA },           //  7
	{ "shf_grom0_3.grm0_3",					0x080000, 0x3855a16a, 3 | BRF_GRA },           //  8
	{ "shf_grom1_0.grm1_0",					0x080000, 0xed140389, 3 | BRF_GRA },           //  9
	{ "shf_grom1_1.grm1_1",					0x080000, 0xbd2ffbca, 3 | BRF_GRA },           // 10
	{ "shf_grom1_2.grm1_2",					0x080000, 0xc6de4187, 3 | BRF_GRA },           // 11
	{ "shf_grom1_3.grm1_3",					0x080000, 0x0c707aa2, 3 | BRF_GRA },           // 12
	{ "shf_grom2_0.grm2_0",					0x080000, 0x529b4259, 3 | BRF_GRA },           // 13
	{ "shf_grom2_1.grm2_1",					0x080000, 0x4b52ab1a, 3 | BRF_GRA },           // 14
	{ "shf_grom2_2.grm2_2",					0x080000, 0xf45fad03, 3 | BRF_GRA },           // 15
	{ "shf_grom2_3.grm2_3",					0x080000, 0x1bcb26c8, 3 | BRF_GRA },           // 16
	{ "shfa_grom3_0.grm3_0",				0x080000, 0xc5afc9d1, 3 | BRF_GRA },           // 17
	{ "shfa_grom3_1.grm3_1",				0x080000, 0x70dd7b68, 3 | BRF_GRA },           // 18
	{ "shfa_grom3_2.grm3_2",				0x080000, 0xda56512d, 3 | BRF_GRA },           // 19
	{ "shfa_grom3_3.grm3_3",				0x080000, 0x21727c50, 3 | BRF_GRA },           // 20

	{ "shf_srom0.srom0",					0x080000, 0x9a3cb6c9, 4 | BRF_SND },           // 21 Ensoniq Bank 0
	{ "shf_srom1.srom1",					0x080000, 0x8c89948a, 4 | BRF_SND },           // 22
};

STD_ROM_PICK(shufshot)
STD_ROM_FN(shufshot)

static INT32 ShufshotInit()
{
	Trackball_Type = TB_TYPE0;
	is_shufshot = 1;

	return Common32BitInit(0x111a, 1, 0);
}

struct BurnDriver BurnDrvShufshot = {
	"shufshot", NULL, NULL, NULL, "1997",
	"Shuffleshot (v1.40)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, shufshotRomInfo, shufshotRomName, NULL, NULL, NULL, NULL, ShufshotInputInfo, ShufshotDIPInfo,
	ShufshotInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Shuffleshot (v1.39)
/* Version 1.39 (PCB P/N 1083 Rev 2) */

static struct BurnRomInfo shufshot139RomDesc[] = {
	{ "shot_prom0_v1.39.prom0",				0x020000, 0xe811fc4a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "shot_prom1_v1.39.prom1",				0x020000, 0xf9d120c5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "shot_prom2_v1.39.prom2",				0x020000, 0x9f12414d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "shot_prom3_v1.39.prom3",				0x020000, 0x108a69be, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "shotsnd_u88_v1.1.u88",				0x020000, 0xe37d599d, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "shf_grom0_0.grm0_0",					0x080000, 0x832a3d6a, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "shf_grom0_1.grm0_1",					0x080000, 0x155e48a2, 3 | BRF_GRA },           //  6
	{ "shf_grom0_2.grm0_2",					0x080000, 0x9f2b470d, 3 | BRF_GRA },           //  7
	{ "shf_grom0_3.grm0_3",					0x080000, 0x3855a16a, 3 | BRF_GRA },           //  8
	{ "shf_grom1_0.grm1_0",					0x080000, 0xed140389, 3 | BRF_GRA },           //  9
	{ "shf_grom1_1.grm1_1",					0x080000, 0xbd2ffbca, 3 | BRF_GRA },           // 10
	{ "shf_grom1_2.grm1_2",					0x080000, 0xc6de4187, 3 | BRF_GRA },           // 11
	{ "shf_grom1_3.grm1_3",					0x080000, 0x0c707aa2, 3 | BRF_GRA },           // 12
	{ "shf_grom2_0.grm2_0",					0x080000, 0x529b4259, 3 | BRF_GRA },           // 13
	{ "shf_grom2_1.grm2_1",					0x080000, 0x4b52ab1a, 3 | BRF_GRA },           // 14
	{ "shf_grom2_2.grm2_2",					0x080000, 0xf45fad03, 3 | BRF_GRA },           // 15
	{ "shf_grom2_3.grm2_3",					0x080000, 0x1bcb26c8, 3 | BRF_GRA },           // 16
	{ "shfa_grom3_0.grm3_0",				0x080000, 0xc5afc9d1, 3 | BRF_GRA },           // 17
	{ "shfa_grom3_1.grm3_1",				0x080000, 0x70dd7b68, 3 | BRF_GRA },           // 18
	{ "shfa_grom3_2.grm3_2",				0x080000, 0xda56512d, 3 | BRF_GRA },           // 19
	{ "shfa_grom3_3.grm3_3",				0x080000, 0x21727c50, 3 | BRF_GRA },           // 20

	{ "shf_srom0.srom0",					0x080000, 0x9a3cb6c9, 4 | BRF_SND },           // 21 Ensoniq Bank 0
	{ "shf_srom1.srom1",					0x080000, 0x8c89948a, 4 | BRF_SND },           // 22
};

STD_ROM_PICK(shufshot139)
STD_ROM_FN(shufshot139)

struct BurnDriver BurnDrvShufshot139 = {
	"shufshot139", "shufshot", NULL, NULL, "1997",
	"Shuffleshot (v1.39)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, shufshot139RomInfo, shufshot139RomName, NULL, NULL, NULL, NULL, ShufshotInputInfo, ShufshotDIPInfo,
	ShufshotInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Shuffleshot (v1.38)
 /* Version 1.38 (PCB P/N 1083 Rev 2) - Not offically released? - found on dev CD */
 
static struct BurnRomInfo shufshot138RomDesc[] = {
	{ "shot_prom0_v1.38.prom0",				0x020000, 0x68f823ff, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "shot_prom1_v1.38.prom1",				0x020000, 0xbdd9a8e9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "shot_prom2_v1.38.prom2",				0x020000, 0x92008e13, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "shot_prom3_v1.38.prom3",				0x020000, 0x723cb9a5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "shotsnd_u88_v1.1.u88",				0x020000, 0xe37d599d, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "shf_grom0_0.grm0_0",					0x080000, 0x832a3d6a, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "shf_grom0_1.grm0_1",					0x080000, 0x155e48a2, 3 | BRF_GRA },           //  6
	{ "shf_grom0_2.grm0_2",					0x080000, 0x9f2b470d, 3 | BRF_GRA },           //  7
	{ "shf_grom0_3.grm0_3",					0x080000, 0x3855a16a, 3 | BRF_GRA },           //  8
	{ "shf_grom1_0.grm1_0",					0x080000, 0xed140389, 3 | BRF_GRA },           //  9
	{ "shf_grom1_1.grm1_1",					0x080000, 0xbd2ffbca, 3 | BRF_GRA },           // 10
	{ "shf_grom1_2.grm1_2",					0x080000, 0xc6de4187, 3 | BRF_GRA },           // 11
	{ "shf_grom1_3.grm1_3",					0x080000, 0x0c707aa2, 3 | BRF_GRA },           // 12
	{ "shf_grom2_0.grm2_0",					0x080000, 0x529b4259, 3 | BRF_GRA },           // 13
	{ "shf_grom2_1.grm2_1",					0x080000, 0x4b52ab1a, 3 | BRF_GRA },           // 14
	{ "shf_grom2_2.grm2_2",					0x080000, 0xf45fad03, 3 | BRF_GRA },           // 15
	{ "shf_grom2_3.grm2_3",					0x080000, 0x1bcb26c8, 3 | BRF_GRA },           // 16
	{ "shfa_grom3_0.grm3_0",				0x080000, 0xc5afc9d1, 3 | BRF_GRA },           // 17
	{ "shfa_grom3_1.grm3_1",				0x080000, 0x70dd7b68, 3 | BRF_GRA },           // 18
	{ "shfa_grom3_2.grm3_2",				0x080000, 0xda56512d, 3 | BRF_GRA },           // 19
	{ "shfa_grom3_3.grm3_3",				0x080000, 0x21727c50, 3 | BRF_GRA },           // 20

	{ "shf_srom0.srom0",					0x080000, 0x9a3cb6c9, 4 | BRF_SND },           // 21 Ensoniq Bank 0
	{ "shf_srom1.srom1",					0x080000, 0x8c89948a, 4 | BRF_SND },           // 22
};

STD_ROM_PICK(shufshot138)
STD_ROM_FN(shufshot138)

struct BurnDriver BurnDrvShufshot138 = {
	"shufshot138", "shufshot", NULL, NULL, "1997",
	"Shuffleshot (v1.38)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, shufshot138RomInfo, shufshot138RomName, NULL, NULL, NULL, NULL, ShufshotInputInfo, ShufshotDIPInfo,
	ShufshotInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Shuffleshot (v1.37)
 /* Version 1.37 (PCB P/N 1083 Rev 2) */
 
static struct BurnRomInfo shufshot137RomDesc[] = {
	{ "shot_prom0_v1.37.prom0",				0x020000, 0x6499c76f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "shot_prom1_v1.37.prom1",				0x020000, 0x64fb47a4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "shot_prom2_v1.37.prom2",				0x020000, 0xe0df3025, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "shot_prom3_v1.37.prom3",				0x020000, 0xefa66ad8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "shotsnd_u88_v1.1.u88",				0x020000, 0xe37d599d, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "shf_grom0_0.grm0_0",					0x080000, 0x832a3d6a, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "shf_grom0_1.grm0_1",					0x080000, 0x155e48a2, 3 | BRF_GRA },           //  6
	{ "shf_grom0_2.grm0_2",					0x080000, 0x9f2b470d, 3 | BRF_GRA },           //  7
	{ "shf_grom0_3.grm0_3",					0x080000, 0x3855a16a, 3 | BRF_GRA },           //  8
	{ "shf_grom1_0.grm1_0",					0x080000, 0xed140389, 3 | BRF_GRA },           //  9
	{ "shf_grom1_1.grm1_1",					0x080000, 0xbd2ffbca, 3 | BRF_GRA },           // 10
	{ "shf_grom1_2.grm1_2",					0x080000, 0xc6de4187, 3 | BRF_GRA },           // 11
	{ "shf_grom1_3.grm1_3",					0x080000, 0x0c707aa2, 3 | BRF_GRA },           // 12
	{ "shf_grom2_0.grm2_0",					0x080000, 0x529b4259, 3 | BRF_GRA },           // 13
	{ "shf_grom2_1.grm2_1",					0x080000, 0x4b52ab1a, 3 | BRF_GRA },           // 14
	{ "shf_grom2_2.grm2_2",					0x080000, 0xf45fad03, 3 | BRF_GRA },           // 15
	{ "shf_grom2_3.grm2_3",					0x080000, 0x1bcb26c8, 3 | BRF_GRA },           // 16
	{ "shf_grom3_0.grm3_0",					0x080000, 0xa29763db, 3 | BRF_GRA },           // 17
	{ "shf_grom3_1.grm3_1",					0x080000, 0xc757084c, 3 | BRF_GRA },           // 18
	{ "shf_grom3_2.grm3_2",					0x080000, 0x2971cb25, 3 | BRF_GRA },           // 19
	{ "shf_grom3_3.grm3_3",					0x080000, 0x4fcbee51, 3 | BRF_GRA },           // 20	

	{ "shf_srom0.srom0",					0x080000, 0x9a3cb6c9, 4 | BRF_SND },           // 21 Ensoniq Bank 0
	{ "shf_srom1.srom1",					0x080000, 0x8c89948a, 4 | BRF_SND },           // 22
};

STD_ROM_PICK(shufshot137)
STD_ROM_FN(shufshot137)

struct BurnDriver BurnDrvShufshot137 = {
	"shufshot137", "shufshot", NULL, NULL, "1997",
	"Shuffleshot (v1.37)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, shufshot137RomInfo, shufshot137RomName, NULL, NULL, NULL, NULL, ShufshotInputInfo, ShufshtoDIPInfo,
	ShufshotInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};


// Shuffleshot (v1.35)
/* Version 1.35 (PCB P/N 1083 Rev 2) - Not offically released? - found on dev CD  */

static struct BurnRomInfo shufshot135RomDesc[] = {
	{ "shot_prom0_v1.35.prom0",				0x020000, 0x1a1d510c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "shot_prom1_v1.35.prom1",				0x020000, 0x5d7d5017, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "shot_prom2_v1.35.prom2",				0x020000, 0x6f27b111, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "shot_prom3_v1.35.prom3",				0x020000, 0xbf6fabbb, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "shotsnd_u88_v1.1.u88",				0x020000, 0xe37d599d, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "shf_grom0_0.grm0_0",					0x080000, 0x832a3d6a, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "shf_grom0_1.grm0_1",					0x080000, 0x155e48a2, 3 | BRF_GRA },           //  6
	{ "shf_grom0_2.grm0_2",					0x080000, 0x9f2b470d, 3 | BRF_GRA },           //  7
	{ "shf_grom0_3.grm0_3",					0x080000, 0x3855a16a, 3 | BRF_GRA },           //  8
	{ "shf_grom1_0.grm1_0",					0x080000, 0xed140389, 3 | BRF_GRA },           //  9
	{ "shf_grom1_1.grm1_1",					0x080000, 0xbd2ffbca, 3 | BRF_GRA },           // 10
	{ "shf_grom1_2.grm1_2",					0x080000, 0xc6de4187, 3 | BRF_GRA },           // 11
	{ "shf_grom1_3.grm1_3",					0x080000, 0x0c707aa2, 3 | BRF_GRA },           // 12
	{ "shf_grom2_0.grm2_0",					0x080000, 0x529b4259, 3 | BRF_GRA },           // 13
	{ "shf_grom2_1.grm2_1",					0x080000, 0x4b52ab1a, 3 | BRF_GRA },           // 14
	{ "shf_grom2_2.grm2_2",					0x080000, 0xf45fad03, 3 | BRF_GRA },           // 15
	{ "shf_grom2_3.grm2_3",					0x080000, 0x1bcb26c8, 3 | BRF_GRA },           // 16
	{ "shf_grom3_0.grm3_0",					0x080000, 0xa29763db, 3 | BRF_GRA },           // 17
	{ "shf_grom3_1.grm3_1",					0x080000, 0xc757084c, 3 | BRF_GRA },           // 18
	{ "shf_grom3_2.grm3_2",					0x080000, 0x2971cb25, 3 | BRF_GRA },           // 19
	{ "shf_grom3_3.grm3_3",					0x080000, 0x4fcbee51, 3 | BRF_GRA },           // 20	

	{ "shf_srom0.srom0",					0x080000, 0x9a3cb6c9, 4 | BRF_SND },           // 21 Ensoniq Bank 0
	{ "shf_srom1.srom1",					0x080000, 0x8c89948a, 4 | BRF_SND },           // 22
};

STD_ROM_PICK(shufshot135)
STD_ROM_FN(shufshot135)

struct BurnDriver BurnDrvShufshot135 = {
	"shufshot135", "shufshot", NULL, NULL, "1997",
	"Shuffleshot (v1.35)\0", NULL, "Strata/Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, shufshot135RomInfo, shufshot135RomName, NULL, NULL, NULL, NULL, ShufshotInputInfo, ShufshtoDIPInfo,
	ShufshotInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 256, 4, 3
};

// Golden Tee 3D Golf (v1.93N)

static struct BurnRomInfo gt3dRomDesc[] = {
	{ "gtg3_prom0_v1.93n.prom0",			0x080000, 0xcacacb44, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg3_prom1_v1.93n.prom1",			0x080000, 0x4c172d7f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg3_prom2_v1.93n.prom2",			0x080000, 0xb53fe6f0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg3_prom3_v1.93n.prom3",			0x080000, 0x78468761, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gtg3_grom0_0.grm0_0",				0x080000, 0x1b10379d, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gtg3_grom0_1.grm0_1",				0x080000, 0x3b852e1a, 3 | BRF_GRA },           //  6
	{ "gtg3_grom0_2.grm0_2",				0x080000, 0xd43ffb35, 3 | BRF_GRA },           //  7
	{ "gtg3_grom0_3.grm0_3",				0x080000, 0x2d24e93e, 3 | BRF_GRA },           //  8
	{ "gtg3_grom1_0.grm1_0",				0x080000, 0x4476b239, 3 | BRF_GRA },           //  9
	{ "gtg3_grom1_1.grm1_1",				0x080000, 0x0aadfad2, 3 | BRF_GRA },           // 10
	{ "gtg3_grom1_2.grm1_2",				0x080000, 0x27871980, 3 | BRF_GRA },           // 11
	{ "gtg3_grom1_3.grm1_3",				0x080000, 0x7dbc242b, 3 | BRF_GRA },           // 12
	{ "gtg3_grom2_0_t.grm2_0",				0x080000, 0x80ae7148, 3 | BRF_GRA },           // 13
	{ "gtg3_grom2_1_t.grm2_1",				0x080000, 0x0f85a618, 3 | BRF_GRA },           // 14
	{ "gtg3_grom2_2_t.grm2_2",				0x080000, 0x09ca5fbf, 3 | BRF_GRA },           // 15
	{ "gtg3_grom2_3_t.grm2_3",				0x080000, 0xd136853a, 3 | BRF_GRA },           // 16

	{ "gtg3_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gtg3_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt3d)
STD_ROM_FN(gt3d)

static INT32 Gt3dInit()
{
	Trackball_Type = TB_TYPE0;

	return Common32BitInit(0x112f, 2, 1);
}

struct BurnDriver BurnDrvGt3d = {
	"gt3d", NULL, NULL, NULL, "1995",
	"Golden Tee 3D Golf (v1.93N)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt3dRomInfo, gt3dRomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt3dDIPInfo,
	Gt3dInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};

static INT32 Gt3d_2_Init()
{
	Trackball_Type = TB_TYPE2;

	return Common32BitInit(0x112f, 2, 1);
}

// Golden Tee 3D Golf (v1.92S)

static struct BurnRomInfo gt3ds192RomDesc[] = {
	{ "gtg3_prom0_v1.92s.prom0",			0x080000, 0xeee38005, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg3_prom1_v1.92s.prom1",			0x080000, 0x818ba70e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg3_prom2_v1.92s.prom2",			0x080000, 0x7ab661a1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg3_prom3_v1.92s.prom3",			0x080000, 0xf9f96c01, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3-snd_u23_v1.2.u23",				0x020000, 0xcbbe41f9, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gtg3_grom0_0.grm0_0",				0x080000, 0x1b10379d, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gtg3_grom0_1.grm0_1",				0x080000, 0x3b852e1a, 3 | BRF_GRA },           //  6
	{ "gtg3_grom0_2.grm0_2",				0x080000, 0xd43ffb35, 3 | BRF_GRA },           //  7
	{ "gtg3_grom0_3.grm0_3",				0x080000, 0x2d24e93e, 3 | BRF_GRA },           //  8
	{ "gtg3_grom1_0.grm1_0",				0x080000, 0x4476b239, 3 | BRF_GRA },           //  9
	{ "gtg3_grom1_1.grm1_1",				0x080000, 0x0aadfad2, 3 | BRF_GRA },           // 10
	{ "gtg3_grom1_2.grm1_2",				0x080000, 0x27871980, 3 | BRF_GRA },           // 11
	{ "gtg3_grom1_3.grm1_3",				0x080000, 0x7dbc242b, 3 | BRF_GRA },           // 12
	{ "gtg3_grom2_0.grm2_0",				0x080000, 0x15cb22bc, 3 | BRF_GRA },           // 13
	{ "gtg3_grom2_1.grm2_1",				0x080000, 0x52f3ad44, 3 | BRF_GRA },           // 14
	{ "gtg3_grom2_2.grm2_2",				0x080000, 0xc7712125, 3 | BRF_GRA },           // 15
	{ "gtg3_grom2_3.grm2_3",				0x080000, 0x77869bcf, 3 | BRF_GRA },           // 16

	{ "ensoniq_2m.rom0",					0x200000, 0x9fdc4825, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "gtg3_srom1.srom1",					0x080000, 0xac669434, 6 | BRF_SND },           // 18 Ensoniq Bank 2
	{ "gtg3_srom2.srom2",					0x080000, 0x6853578e, 6 | BRF_SND },           // 19

	{ "gtg3_srom3.srom3",					0x020000, 0xd2462965, 7 | BRF_SND },           // 20 Ensoniq Bank 3
};

STD_ROM_PICK(gt3ds192)
STD_ROM_FN(gt3ds192)

struct BurnDriver BurnDrvGt3ds192 = {
	"gt3ds192", "gt3d", NULL, NULL, "1995",
	"Golden Tee 3D Golf (v1.92S)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt3ds192RomInfo, gt3ds192RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt3dDIPInfo,
	Gt3d_2_Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 3D Golf (v1.92L)

static struct BurnRomInfo gt3dl192RomDesc[] = {
	{ "gtg3_prom0_v1.92l.prom0",			0x080000, 0xb449b939, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg3_prom1_v1.92l.prom1",			0x080000, 0xff986e67, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg3_prom2_v1.92l.prom2",			0x080000, 0xeb959447, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg3_prom3_v1.92l.prom3",			0x080000, 0x0265b798, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3-snd_u23_v2.2.u23",				0x020000, 0x26fe2e92, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gtg3_grom0_0.grm0_0",				0x080000, 0x1b10379d, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gtg3_grom0_1.grm0_1",				0x080000, 0x3b852e1a, 3 | BRF_GRA },           //  6
	{ "gtg3_grom0_2.grm0_2",				0x080000, 0xd43ffb35, 3 | BRF_GRA },           //  7
	{ "gtg3_grom0_3.grm0_3",				0x080000, 0x2d24e93e, 3 | BRF_GRA },           //  8
	{ "gtg3_grom1_0.grm1_0",				0x080000, 0x4476b239, 3 | BRF_GRA },           //  9
	{ "gtg3_grom1_1.grm1_1",				0x080000, 0x0aadfad2, 3 | BRF_GRA },           // 10
	{ "gtg3_grom1_2.grm1_2",				0x080000, 0x27871980, 3 | BRF_GRA },           // 11
	{ "gtg3_grom1_3.grm1_3",				0x080000, 0x7dbc242b, 3 | BRF_GRA },           // 12
	{ "gtg3_grom2_0.grm2_0",				0x080000, 0x15cb22bc, 3 | BRF_GRA },           // 13
	{ "gtg3_grom2_1.grm2_1",				0x080000, 0x52f3ad44, 3 | BRF_GRA },           // 14
	{ "gtg3_grom2_2.grm2_2",				0x080000, 0xc7712125, 3 | BRF_GRA },           // 15
	{ "gtg3_grom2_3.grm2_3",				0x080000, 0x77869bcf, 3 | BRF_GRA },           // 16

	{ "ensoniq_2mx16u.rom0",				0x200000, 0x0814ab80, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "gtg3_srom1.srom1",					0x080000, 0xac669434, 6 | BRF_SND },           // 18 Ensoniq Bank 2
	{ "gtg3_srom2.srom2",					0x080000, 0x6853578e, 6 | BRF_SND },           // 19

	{ "gtg3_srom3.srom3",					0x020000, 0xd2462965, 7 | BRF_SND },           // 20 Ensoniq Bank 3
};

STD_ROM_PICK(gt3dl192)
STD_ROM_FN(gt3dl192)

static INT32 Gt3d1192Init()
{
	Trackball_Type = TB_TYPE4;

	INT32 nRet = Common32BitInit(0x112f, 2, 1);

	return nRet;
}

struct BurnDriver BurnDrvGt3dl192 = {
	"gt3dl192", "gt3d", NULL, NULL, "1995",
	"Golden Tee 3D Golf (v1.92L)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt3dl192RomInfo, gt3dl192RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt3dDIPInfo,
	Gt3d1192Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 3D Golf (v1.91L)

static struct BurnRomInfo gt3dl191RomDesc[] = {
	{ "gtg3_prom0_v1.91l.prom0",			0x080000, 0xa3ea30d8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg3_prom1_v1.91l.prom1",			0x080000, 0x3aa87e56, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg3_prom2_v1.91l.prom2",			0x080000, 0x41720e87, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg3_prom3_v1.91l.prom3",			0x080000, 0x30946139, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3-snd_u23_v2.1.u23",				0x020000, 0x6ae2646d, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gtg3_grom0_0.grm0_0",				0x080000, 0x1b10379d, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gtg3_grom0_1.grm0_1",				0x080000, 0x3b852e1a, 3 | BRF_GRA },           //  6
	{ "gtg3_grom0_2.grm0_2",				0x080000, 0xd43ffb35, 3 | BRF_GRA },           //  7
	{ "gtg3_grom0_3.grm0_3",				0x080000, 0x2d24e93e, 3 | BRF_GRA },           //  8
	{ "gtg3_grom1_0.grm1_0",				0x080000, 0x4476b239, 3 | BRF_GRA },           //  9
	{ "gtg3_grom1_1.grm1_1",				0x080000, 0x0aadfad2, 3 | BRF_GRA },           // 10
	{ "gtg3_grom1_2.grm1_2",				0x080000, 0x27871980, 3 | BRF_GRA },           // 11
	{ "gtg3_grom1_3.grm1_3",				0x080000, 0x7dbc242b, 3 | BRF_GRA },           // 12
	{ "gtg3_grom2_0.grm2_0",				0x080000, 0x15cb22bc, 3 | BRF_GRA },           // 13
	{ "gtg3_grom2_1.grm2_1",				0x080000, 0x52f3ad44, 3 | BRF_GRA },           // 14
	{ "gtg3_grom2_2.grm2_2",				0x080000, 0xc7712125, 3 | BRF_GRA },           // 15
	{ "gtg3_grom2_3.grm2_3",				0x080000, 0x77869bcf, 3 | BRF_GRA },           // 16

	{ "ensoniq_2mx16u.rom0",				0x200000, 0x0814ab80, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "gtg3_srom1.srom1",					0x080000, 0xac669434, 6 | BRF_SND },           // 18 Ensoniq Bank 2
	{ "gtg3_srom2.srom2",					0x080000, 0x6853578e, 6 | BRF_SND },           // 19

	{ "gtg3_srom3.srom3",					0x020000, 0xd2462965, 7 | BRF_SND },           // 20 Ensoniq Bank 3
};

STD_ROM_PICK(gt3dl191)
STD_ROM_FN(gt3dl191)

struct BurnDriver BurnDrvGt3dl191 = {
	"gt3dl191", "gt3d", NULL, NULL, "1995",
	"Golden Tee 3D Golf (v1.91L)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt3dl191RomInfo, gt3dl191RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt3dDIPInfo,
	Gt3d1192Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 3D Golf (v1.9L)

static struct BurnRomInfo gt3dl19RomDesc[] = {
	{ "gtg3_prom0_v1.9l.prom0",				0x080000, 0xb6293cf6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg3_prom1_v1.9l.prom1",				0x080000, 0x270b7936, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg3_prom2_v1.9l.prom2",				0x080000, 0x3f892e81, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg3_prom3_v1.9l.prom3",				0x080000, 0xb63ef2c0, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3-snd_u23_v2.0.u23",				0x020000, 0x3f69a9ea, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gtg3_grom0_0.grm0_0",				0x080000, 0x1b10379d, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gtg3_grom0_1.grm0_1",				0x080000, 0x3b852e1a, 3 | BRF_GRA },           //  6
	{ "gtg3_grom0_2.grm0_2",				0x080000, 0xd43ffb35, 3 | BRF_GRA },           //  7
	{ "gtg3_grom0_3.grm0_3",				0x080000, 0x2d24e93e, 3 | BRF_GRA },           //  8
	{ "gtg3_grom1_0.grm1_0",				0x080000, 0x4476b239, 3 | BRF_GRA },           //  9
	{ "gtg3_grom1_1.grm1_1",				0x080000, 0x0aadfad2, 3 | BRF_GRA },           // 10
	{ "gtg3_grom1_2.grm1_2",				0x080000, 0x27871980, 3 | BRF_GRA },           // 11
	{ "gtg3_grom1_3.grm1_3",				0x080000, 0x7dbc242b, 3 | BRF_GRA },           // 12
	{ "gtg3_grom2_0.grm2_0",				0x080000, 0x15cb22bc, 3 | BRF_GRA },           // 13
	{ "gtg3_grom2_1.grm2_1",				0x080000, 0x52f3ad44, 3 | BRF_GRA },           // 14
	{ "gtg3_grom2_2.grm2_2",				0x080000, 0xc7712125, 3 | BRF_GRA },           // 15
	{ "gtg3_grom2_3.grm2_3",				0x080000, 0x77869bcf, 3 | BRF_GRA },           // 16

	{ "ensoniq_2mx16u.rom0",				0x200000, 0x0814ab80, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "gtg3_srom1.srom1",					0x080000, 0xac669434, 6 | BRF_SND },           // 18 Ensoniq Bank 2
	{ "gtg3_srom2.srom2",					0x080000, 0x6853578e, 6 | BRF_SND },           // 19

	{ "gtg3_srom3.srom3",					0x020000, 0xd2462965, 7 | BRF_SND },           // 20 Ensoniq Bank 3
};

STD_ROM_PICK(gt3dl19)
STD_ROM_FN(gt3dl19)

struct BurnDriver BurnDrvGt3dl19 = {
	"gt3dl19", "gt3d", NULL, NULL, "1995",
	"Golden Tee 3D Golf (v1.9L)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt3dl19RomInfo, gt3dl19RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt3dDIPInfo,
	Gt3d1192Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 3D Golf (v1.8)

static struct BurnRomInfo gt3dv18RomDesc[] = {
	{ "gtg3_prom0_v1.8.prom0",				0x080000, 0x0fa53c40, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg3_prom1_v1.8.prom1",				0x080000, 0xbef2cbe3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg3_prom2_v1.8.prom2",				0x080000, 0x1d5fb128, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg3_prom3_v1.8.prom3",				0x080000, 0x5542c335, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3-snd_u23_v1.1.u23",				0x020000, 0x2f4cde9f, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gtg3_grom0_0.grm0_0",				0x080000, 0x1b10379d, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gtg3_grom0_1.grm0_1",				0x080000, 0x3b852e1a, 3 | BRF_GRA },           //  6
	{ "gtg3_grom0_2.grm0_2",				0x080000, 0xd43ffb35, 3 | BRF_GRA },           //  7
	{ "gtg3_grom0_3.grm0_3",				0x080000, 0x2d24e93e, 3 | BRF_GRA },           //  8
	{ "gtg3_grom1_0.grm1_0",				0x080000, 0x4476b239, 3 | BRF_GRA },           //  9
	{ "gtg3_grom1_1.grm1_1",				0x080000, 0x0aadfad2, 3 | BRF_GRA },           // 10
	{ "gtg3_grom1_2.grm1_2",				0x080000, 0x27871980, 3 | BRF_GRA },           // 11
	{ "gtg3_grom1_3.grm1_3",				0x080000, 0x7dbc242b, 3 | BRF_GRA },           // 12
	{ "gtg3_grom2_0.grm2_0",				0x080000, 0x15cb22bc, 3 | BRF_GRA },           // 13
	{ "gtg3_grom2_1.grm2_1",				0x080000, 0x52f3ad44, 3 | BRF_GRA },           // 14
	{ "gtg3_grom2_2.grm2_2",				0x080000, 0xc7712125, 3 | BRF_GRA },           // 15
	{ "gtg3_grom2_3.grm2_3",				0x080000, 0x77869bcf, 3 | BRF_GRA },           // 16

	{ "ensoniq_2m.rom0",					0x200000, 0x9fdc4825, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "gtg3_srom1.srom1",					0x080000, 0xac669434, 6 | BRF_SND },           // 18 Ensoniq Bank 2
	{ "gtg3_srom2.srom2",					0x080000, 0x6853578e, 6 | BRF_SND },           // 19

	{ "gtg3_srom3.srom3",					0x020000, 0xd2462965, 7 | BRF_SND },           // 20 Ensoniq Bank 3
};

STD_ROM_PICK(gt3dv18)
STD_ROM_FN(gt3dv18)

struct BurnDriver BurnDrvGt3dv18 = {
	"gt3dv18", "gt3d", NULL, NULL, "1995",
	"Golden Tee 3D Golf (v1.8)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt3dv18RomInfo, gt3dv18RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt3dDIPInfo,
	Gt3d_2_Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 3D Golf (v1.7)

static struct BurnRomInfo gt3dv17RomDesc[] = {
	{ "gtg3_prom0_v1.7.prom0",				0x080000, 0x9a6fc839, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg3_prom1_v1.7.prom1",				0x080000, 0x26606578, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg3_prom2_v1.7.prom2",				0x080000, 0x9c4d348b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg3_prom3_v1.7.prom3",				0x080000, 0x53b1d6e7, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3-snd_u23_v1.1.u23",				0x020000, 0x2f4cde9f, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gtg3_grom0_0.grm0_0",				0x080000, 0x1b10379d, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gtg3_grom0_1.grm0_1",				0x080000, 0x3b852e1a, 3 | BRF_GRA },           //  6
	{ "gtg3_grom0_2.grm0_2",				0x080000, 0xd43ffb35, 3 | BRF_GRA },           //  7
	{ "gtg3_grom0_3.grm0_3",				0x080000, 0x2d24e93e, 3 | BRF_GRA },           //  8
	{ "gtg3_grom1_0.grm1_0",				0x080000, 0x4476b239, 3 | BRF_GRA },           //  9
	{ "gtg3_grom1_1.grm1_1",				0x080000, 0x0aadfad2, 3 | BRF_GRA },           // 10
	{ "gtg3_grom1_2.grm1_2",				0x080000, 0x27871980, 3 | BRF_GRA },           // 11
	{ "gtg3_grom1_3.grm1_3",				0x080000, 0x7dbc242b, 3 | BRF_GRA },           // 12
	{ "gtg3_grom2_0.grm2_0",				0x080000, 0x15cb22bc, 3 | BRF_GRA },           // 13
	{ "gtg3_grom2_1.grm2_1",				0x080000, 0x52f3ad44, 3 | BRF_GRA },           // 14
	{ "gtg3_grom2_2.grm2_2",				0x080000, 0xc7712125, 3 | BRF_GRA },           // 15
	{ "gtg3_grom2_3.grm2_3",				0x080000, 0x77869bcf, 3 | BRF_GRA },           // 16

	{ "ensoniq_2m.rom0",					0x200000, 0x9fdc4825, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "gtg3_srom1.srom1",					0x080000, 0xac669434, 6 | BRF_SND },           // 18 Ensoniq Bank 2
	{ "gtg3_srom2.srom2",					0x080000, 0x6853578e, 6 | BRF_SND },           // 19

	{ "gtg3_srom3.srom3",					0x020000, 0xd2462965, 7 | BRF_GRA },           // 20 Ensoniq Bank 3
};

STD_ROM_PICK(gt3dv17)
STD_ROM_FN(gt3dv17)

struct BurnDriver BurnDrvGt3dv17 = {
	"gt3dv17", "gt3d", NULL, NULL, "1995",
	"Golden Tee 3D Golf (v1.7)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt3dv17RomInfo, gt3dv17RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt3dDIPInfo,
	Gt3d_2_Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 3D Golf (v1.6)

static struct BurnRomInfo gt3dv16RomDesc[] = {
	{ "gtg3_prom0_v1.6.prom0",				0x080000, 0x99d9a7e7, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg3_prom1_v1.6.prom1",				0x080000, 0x0ec4b307, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg3_prom2_v1.6.prom2",				0x080000, 0x02ce6085, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg3_prom3_v1.6.prom3",				0x080000, 0xe77fa8a2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3-snd_u23_v1.1.u23",				0x020000, 0x2f4cde9f, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gtg3_grom0_0.grm0_0",				0x080000, 0x1b10379d, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gtg3_grom0_1.grm0_1",				0x080000, 0x3b852e1a, 3 | BRF_GRA },           //  6
	{ "gtg3_grom0_2.grm0_2",				0x080000, 0xd43ffb35, 3 | BRF_GRA },           //  7
	{ "gtg3_grom0_3.grm0_3",				0x080000, 0x2d24e93e, 3 | BRF_GRA },           //  8
	{ "gtg3_grom1_0.grm1_0",				0x080000, 0x4476b239, 3 | BRF_GRA },           //  9
	{ "gtg3_grom1_1.grm1_1",				0x080000, 0x0aadfad2, 3 | BRF_GRA },           // 10
	{ "gtg3_grom1_2.grm1_2",				0x080000, 0x27871980, 3 | BRF_GRA },           // 11
	{ "gtg3_grom1_3.grm1_3",				0x080000, 0x7dbc242b, 3 | BRF_GRA },           // 12
	{ "gtg3_grom2_0.grm2_0",				0x080000, 0x15cb22bc, 3 | BRF_GRA },           // 13
	{ "gtg3_grom2_1.grm2_1",				0x080000, 0x52f3ad44, 3 | BRF_GRA },           // 14
	{ "gtg3_grom2_2.grm2_2",				0x080000, 0xc7712125, 3 | BRF_GRA },           // 15
	{ "gtg3_grom2_3.grm2_3",				0x080000, 0x77869bcf, 3 | BRF_GRA },           // 16

	{ "ensoniq_2m.rom0",					0x200000, 0x9fdc4825, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "gtg3_srom1.srom1",					0x080000, 0xac669434, 6 | BRF_SND },           // 18 Ensoniq Bank 2
	{ "gtg3_srom2.srom2",					0x080000, 0x6853578e, 6 | BRF_SND },           // 19

	{ "gtg3_srom3.srom3",					0x020000, 0xd2462965, 7 | BRF_SND },           // 20 Ensoniq Bank 3
};

STD_ROM_PICK(gt3dv16)
STD_ROM_FN(gt3dv16)

struct BurnDriver BurnDrvGt3dv16 = {
	"gt3dv16", "gt3d", NULL, NULL, "1995",
	"Golden Tee 3D Golf (v1.6)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt3dv16RomInfo, gt3dv16RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt3dDIPInfo,
	Gt3d_2_Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 3D Golf (v1.5)

static struct BurnRomInfo gt3dv15RomDesc[] = {
	{ "gtg3_prom0_v1.5.prom0",				0x080000, 0x51a5e811, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg3_prom1_v1.5.prom1",				0x080000, 0x1e8744ad, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg3_prom2_v1.5.prom2",				0x080000, 0xe465c813, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg3_prom3_v1.5.prom3",				0x080000, 0x3b25e198, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3-snd_u23_v1.1.u23",				0x020000, 0x2f4cde9f, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gtg3_grom0_0.grm0_0",				0x080000, 0x1b10379d, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gtg3_grom0_1.grm0_1",				0x080000, 0x3b852e1a, 3 | BRF_GRA },           //  6
	{ "gtg3_grom0_2.grm0_2",				0x080000, 0xd43ffb35, 3 | BRF_GRA },           //  7
	{ "gtg3_grom0_3.grm0_3",				0x080000, 0x2d24e93e, 3 | BRF_GRA },           //  8
	{ "gtg3_grom1_0.grm1_0",				0x080000, 0x4476b239, 3 | BRF_GRA },           //  9
	{ "gtg3_grom1_1.grm1_1",				0x080000, 0x0aadfad2, 3 | BRF_GRA },           // 10
	{ "gtg3_grom1_2.grm1_2",				0x080000, 0x27871980, 3 | BRF_GRA },           // 11
	{ "gtg3_grom1_3.grm1_3",				0x080000, 0x7dbc242b, 3 | BRF_GRA },           // 12
	{ "gtg3_grom2_0.grm2_0",				0x080000, 0x15cb22bc, 3 | BRF_GRA },           // 13
	{ "gtg3_grom2_1.grm2_1",				0x080000, 0x52f3ad44, 3 | BRF_GRA },           // 14
	{ "gtg3_grom2_2.grm2_2",				0x080000, 0xc7712125, 3 | BRF_GRA },           // 15
	{ "gtg3_grom2_3.grm2_3",				0x080000, 0x77869bcf, 3 | BRF_GRA },           // 16

	{ "ensoniq_2m.rom0",					0x200000, 0x9fdc4825, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "gtg3_srom1.srom1",					0x080000, 0xac669434, 6 | BRF_SND },           // 18 Ensoniq Bank 2
	{ "gtg3_srom2.srom2",					0x080000, 0x6853578e, 6 | BRF_SND },           // 19

	{ "gtg3_srom3.srom3",					0x020000, 0xd2462965, 7 | BRF_SND },           // 20 Ensoniq Bank 3
};

STD_ROM_PICK(gt3dv15)
STD_ROM_FN(gt3dv15)

struct BurnDriver BurnDrvGt3dv15 = {
	"gt3dv15", "gt3d", NULL, NULL, "1995",
	"Golden Tee 3D Golf (v1.5)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt3dv15RomInfo, gt3dv15RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt3dDIPInfo,
	Gt3d_2_Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 3D Golf (v1.4)

static struct BurnRomInfo gt3dv14RomDesc[] = {
	{ "gtg3_prom0_v1.4.prom0",				0x080000, 0x396934a7, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg3_prom1_v1.4.prom1",				0x080000, 0x5ba19b8d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg3_prom2_v1.4.prom2",				0x080000, 0x23991fcf, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg3_prom3_v1.4.prom3",				0x080000, 0x2f7b5a26, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3-snd_u23_v1.0.u23",				0x020000, 0x4f106cd1, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gtg3_grom0_0.grm0_0",				0x080000, 0x1b10379d, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gtg3_grom0_1.grm0_1",				0x080000, 0x3b852e1a, 3 | BRF_GRA },           //  6
	{ "gtg3_grom0_2.grm0_2",				0x080000, 0xd43ffb35, 3 | BRF_GRA },           //  7
	{ "gtg3_grom0_3.grm0_3",				0x080000, 0x2d24e93e, 3 | BRF_GRA },           //  8
	{ "gtg3_grom1_0.grm1_0",				0x080000, 0x4476b239, 3 | BRF_GRA },           //  9
	{ "gtg3_grom1_1.grm1_1",				0x080000, 0x0aadfad2, 3 | BRF_GRA },           // 10
	{ "gtg3_grom1_2.grm1_2",				0x080000, 0x27871980, 3 | BRF_GRA },           // 11
	{ "gtg3_grom1_3.grm1_3",				0x080000, 0x7dbc242b, 3 | BRF_GRA },           // 12
	{ "gtg3_grom2_0.grm2_0",				0x080000, 0x15cb22bc, 3 | BRF_GRA },           // 13
	{ "gtg3_grom2_1.grm2_1",				0x080000, 0x52f3ad44, 3 | BRF_GRA },           // 14
	{ "gtg3_grom2_2.grm2_2",				0x080000, 0xc7712125, 3 | BRF_GRA },           // 15
	{ "gtg3_grom2_3.grm2_3",				0x080000, 0x77869bcf, 3 | BRF_GRA },           // 16

	{ "ensoniq_2m.rom0",					0x200000, 0x9fdc4825, 4 | BRF_SND },           // 17 Ensoniq Bank 0

	{ "gtg3_srom1.srom1",					0x080000, 0xac669434, 6 | BRF_SND },           // 18 Ensoniq Bank 2
	{ "gtg3_srom2.srom2",					0x080000, 0x6853578e, 6 | BRF_SND },           // 19

	{ "gtg3_srom3.srom3",					0x020000, 0xd2462965, 7 | BRF_SND },           // 20 Ensoniq Bank 3
};

STD_ROM_PICK(gt3dv14)
STD_ROM_FN(gt3dv14)

struct BurnDriver BurnDrvGt3dv14 = {
	"gt3dv14", "gt3d", NULL, NULL, "1995",
	"Golden Tee 3D Golf (v1.4)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt3dv14RomInfo, gt3dv14RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt3dDIPInfo,
	Gt3d_2_Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 3D Golf Tournament (v2.31)

static struct BurnRomInfo gt3dt231RomDesc[] = {
	{ "gtg3_prom0_v2.31t.prom0",			0x100000, 0x92a5c3e9, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg3_prom1_v2.31t.prom1",			0x100000, 0xa3b60226, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg3_prom2_v2.31t.prom2",			0x100000, 0xd1659616, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg3_prom3_v2.31t.prom3",			0x100000, 0x1d231ea2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gtg3_grom0_0.grm0_0",				0x080000, 0x1b10379d, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gtg3_grom0_1.grm0_1",				0x080000, 0x3b852e1a, 3 | BRF_GRA },           //  6
	{ "gtg3_grom0_2.grm0_2",				0x080000, 0xd43ffb35, 3 | BRF_GRA },           //  7
	{ "gtg3_grom0_3.grm0_3",				0x080000, 0x2d24e93e, 3 | BRF_GRA },           //  8
	{ "gtg3_grom1_0.grm1_0",				0x080000, 0x4476b239, 3 | BRF_GRA },           //  9
	{ "gtg3_grom1_1.grm1_1",				0x080000, 0x0aadfad2, 3 | BRF_GRA },           // 10
	{ "gtg3_grom1_2.grm1_2",				0x080000, 0x27871980, 3 | BRF_GRA },           // 11
	{ "gtg3_grom1_3.grm1_3",				0x080000, 0x7dbc242b, 3 | BRF_GRA },           // 12
	{ "gtg3_grom2_0_t.grm2_0",				0x080000, 0x80ae7148, 3 | BRF_GRA },           // 13
	{ "gtg3_grom2_1_t.grm2_1",				0x080000, 0x0f85a618, 3 | BRF_GRA },           // 14
	{ "gtg3_grom2_2_t.grm2_2",				0x080000, 0x09ca5fbf, 3 | BRF_GRA },           // 15
	{ "gtg3_grom2_3_t.grm2_3",				0x080000, 0xd136853a, 3 | BRF_GRA },           // 16

	{ "gtg3_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gtg3_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt3dt231)
STD_ROM_FN(gt3dt231)

static INT32 Gt3dAmaaInit()
{
//	needs special trackball inputs hooked up!!!
//	needs timekeeper!!
	Trackball_Type = TB_TYPE0;

	return Common32BitInit(0x112f, 2, 1);
}

struct BurnDriver BurnDrvGt3dt231 = {
	"gt3dt231", "gt3d", NULL, NULL, "1995",
	"Golden Tee 3D Golf Tournament (v2.31)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt3dt231RomInfo, gt3dt231RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt3dDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 3D Golf Tournament (v2.11)

static struct BurnRomInfo gt3dt211RomDesc[] = {
	{ "gtg3_prom0_v2.11t.prom0",			0x100000, 0x54360fdf, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg3_prom1_v2.11t.prom1",			0x100000, 0x9142ebb7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg3_prom2_v2.11t.prom2",			0x100000, 0x058b906a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg3_prom3_v2.11t.prom3",			0x100000, 0x8dbeee1b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gtg3_grom0_0++.grm0_0",				0x100000, 0x22c481b7, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gtg3_grom0_1++.grm0_1",				0x100000, 0x40e4032b, 3 | BRF_GRA },           //  6
	{ "gtg3_grom0_2++.grm0_2",				0x100000, 0x67a02ef9, 3 | BRF_GRA },           //  7
	{ "gtg3_grom0_3++.grm0_3",				0x100000, 0x1173a710, 3 | BRF_GRA },           //  8
	{ "gtg3_grom1_0+.grm1_0",				0x080000, 0x80ae7148, 3 | BRF_GRA },           //  9
	{ "gtg3_grom1_1+.grm1_1",				0x080000, 0x0f85a618, 3 | BRF_GRA },           // 10
	{ "gtg3_grom1_2+.grm1_2",				0x080000, 0x09ca5fbf, 3 | BRF_GRA },           // 11
	{ "gtg3_grom1_3+.grm1_3",				0x080000, 0xd136853a, 3 | BRF_GRA },           // 12

	{ "gtg3_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 13 Ensoniq Bank 0
	{ "gtg3_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 14
};

STD_ROM_PICK(gt3dt211)
STD_ROM_FN(gt3dt211)

struct BurnDriver BurnDrvGt3dt211 = {
	"gt3dt211", "gt3d", NULL, NULL, "1995",
	"Golden Tee 3D Golf Tournament (v2.11)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt3dt211RomInfo, gt3dt211RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt3dDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '97 (v1.30)

static struct BurnRomInfo gt97RomDesc[] = {
	{ "gt97_prom0_v1.30.prom0",				0x080000, 0x7490ba4e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt97_prom1_v1.30.prom1",				0x080000, 0x71f9c5f3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt97_prom2_v1.30.prom2",				0x080000, 0x8292b51a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt97_prom3_v1.30.prom3",				0x080000, 0x64539f72, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt97nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt97_grom0_0.grm0_0",				0x080000, 0x81784aaf, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt97_grom0_1.grm0_1",				0x080000, 0x345bda44, 3 | BRF_GRA },           //  6
	{ "gt97_grom0_2.grm0_2",				0x080000, 0xb2beb40d, 3 | BRF_GRA },           //  7
	{ "gt97_grom0_3.grm0_3",				0x080000, 0x7cef32ff, 3 | BRF_GRA },           //  8
	{ "gt97_grom1_0.grm1_0",				0x080000, 0x1cc4c309, 3 | BRF_GRA },           //  9
	{ "gt97_grom1_1.grm1_1",				0x080000, 0x512cea45, 3 | BRF_GRA },           // 10
	{ "gt97_grom1_2.grm1_2",				0x080000, 0x0599b505, 3 | BRF_GRA },           // 11
	{ "gt97_grom1_3.grm1_3",				0x080000, 0x257eacc9, 3 | BRF_GRA },           // 12
	{ "gt97_grom2_0.grm2_0",				0x080000, 0x95b6e7ee, 3 | BRF_GRA },           // 13
	{ "gt97_grom2_1.grm2_1",				0x080000, 0xafd558b4, 3 | BRF_GRA },           // 14
	{ "gt97_grom2_2.grm2_2",				0x080000, 0x5b7a8733, 3 | BRF_GRA },           // 15
	{ "gt97_grom2_3.grm2_3",				0x080000, 0x72e8ae60, 3 | BRF_GRA },           // 16

	{ "gt97_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt97_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt97)
STD_ROM_FN(gt97)

struct BurnDriver BurnDrvGt97 = {
	"gt97", NULL, NULL, NULL, "1997",
	"Golden Tee '97 (v1.30)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt97RomInfo, gt97RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt97DIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '97 (v1.22)

static struct BurnRomInfo gt97v122RomDesc[] = {
	{ "gt97_prom0_v1.22.prom0",				0x080000, 0x4a543c99, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt97_prom1_v1.22.prom1",				0x080000, 0x27668628, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt97_prom2_v1.22.prom2",				0x080000, 0xd73a769f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt97_prom3_v1.22.prom3",				0x080000, 0x03962957, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt97nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt97_grom0_0.grm0_0",				0x080000, 0x81784aaf, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt97_grom0_1.grm0_1",				0x080000, 0x345bda44, 3 | BRF_GRA },           //  6
	{ "gt97_grom0_2.grm0_2",				0x080000, 0xb2beb40d, 3 | BRF_GRA },           //  7
	{ "gt97_grom0_3.grm0_3",				0x080000, 0x7cef32ff, 3 | BRF_GRA },           //  8
	{ "gt97_grom1_0.grm1_0",				0x080000, 0x1cc4c309, 3 | BRF_GRA },           //  9
	{ "gt97_grom1_1.grm1_1",				0x080000, 0x512cea45, 3 | BRF_GRA },           // 10
	{ "gt97_grom1_2.grm1_2",				0x080000, 0x0599b505, 3 | BRF_GRA },           // 11
	{ "gt97_grom1_3.grm1_3",				0x080000, 0x257eacc9, 3 | BRF_GRA },           // 12
	{ "gt97_grom2_0.grm2_0",				0x080000, 0x95b6e7ee, 3 | BRF_GRA },           // 13
	{ "gt97_grom2_1.grm2_1",				0x080000, 0xafd558b4, 3 | BRF_GRA },           // 14
	{ "gt97_grom2_2.grm2_2",				0x080000, 0x5b7a8733, 3 | BRF_GRA },           // 15
	{ "gt97_grom2_3.grm2_3",				0x080000, 0x72e8ae60, 3 | BRF_GRA },           // 16

	{ "gt97_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt97_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt97v122)
STD_ROM_FN(gt97v122)

struct BurnDriver BurnDrvGt97v122 = {
	"gt97v122", "gt97", NULL, NULL, "1997",
	"Golden Tee '97 (v1.22)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt97v122RomInfo, gt97v122RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt97oDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '97 (v1.21)

static struct BurnRomInfo gt97v121RomDesc[] = {
	{ "gt97_prom0_v1.21.prom0",				0x080000, 0xa210a2c6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt97_prom1_v1.21.prom1",				0x080000, 0xa60806f8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt97_prom2_v1.21.prom2",				0x080000, 0xa97ce668, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt97_prom3_v1.21.prom3",				0x080000, 0x7a6b1ad8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt97nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt97_grom0_0.grm0_0",				0x080000, 0x81784aaf, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt97_grom0_1.grm0_1",				0x080000, 0x345bda44, 3 | BRF_GRA },           //  6
	{ "gt97_grom0_2.grm0_2",				0x080000, 0xb2beb40d, 3 | BRF_GRA },           //  7
	{ "gt97_grom0_3.grm0_3",				0x080000, 0x7cef32ff, 3 | BRF_GRA },           //  8
	{ "gt97_grom1_0.grm1_0",				0x080000, 0x1cc4c309, 3 | BRF_GRA },           //  9
	{ "gt97_grom1_1.grm1_1",				0x080000, 0x512cea45, 3 | BRF_GRA },           // 10
	{ "gt97_grom1_2.grm1_2",				0x080000, 0x0599b505, 3 | BRF_GRA },           // 11
	{ "gt97_grom1_3.grm1_3",				0x080000, 0x257eacc9, 3 | BRF_GRA },           // 12
	{ "gt97_grom2_0.grm2_0",				0x080000, 0x95b6e7ee, 3 | BRF_GRA },           // 13
	{ "gt97_grom2_1.grm2_1",				0x080000, 0xafd558b4, 3 | BRF_GRA },           // 14
	{ "gt97_grom2_2.grm2_2",				0x080000, 0x5b7a8733, 3 | BRF_GRA },           // 15
	{ "gt97_grom2_3.grm2_3",				0x080000, 0x72e8ae60, 3 | BRF_GRA },           // 16

	{ "gt97_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt97_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt97v121)
STD_ROM_FN(gt97v121)

struct BurnDriver BurnDrvGt97v121 = {
	"gt97v121", "gt97", NULL, NULL, "1997",
	"Golden Tee '97 (v1.21)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt97v121RomInfo, gt97v121RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt97oDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '97 (v1.21S)

static struct BurnRomInfo gt97s121RomDesc[] = {
	{ "gt97_prom0_v1.21s.prom0",			0x080000, 0x1143f45b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt97_prom1_v1.21s.prom1",			0x080000, 0xe7cfb1ea, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt97_prom2_v1.21s.prom2",			0x080000, 0x0cc24291, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt97_prom3_v1.21s.prom3",			0x080000, 0x922727c2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3_nr_u23_v2.2.u23",				0x020000, 0x04effd73, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt97_grom0_0.grm0_0",				0x080000, 0x81784aaf, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt97_grom0_1.grm0_1",				0x080000, 0x345bda44, 3 | BRF_GRA },           //  6
	{ "gt97_grom0_2.grm0_2",				0x080000, 0xb2beb40d, 3 | BRF_GRA },           //  7
	{ "gt97_grom0_3.grm0_3",				0x080000, 0x7cef32ff, 3 | BRF_GRA },           //  8
	{ "gt97_grom1_0.grm1_0",				0x080000, 0x1cc4c309, 3 | BRF_GRA },           //  9
	{ "gt97_grom1_1.grm1_1",				0x080000, 0x512cea45, 3 | BRF_GRA },           // 10
	{ "gt97_grom1_2.grm1_2",				0x080000, 0x0599b505, 3 | BRF_GRA },           // 11
	{ "gt97_grom1_3.grm1_3",				0x080000, 0x257eacc9, 3 | BRF_GRA },           // 12
	{ "gt97_grom2_0.grm2_0",				0x080000, 0x95b6e7ee, 3 | BRF_GRA },           // 13
	{ "gt97_grom2_1.grm2_1",				0x080000, 0xafd558b4, 3 | BRF_GRA },           // 14
	{ "gt97_grom2_2.grm2_2",				0x080000, 0x5b7a8733, 3 | BRF_GRA },           // 15
	{ "gt97_grom2_3.grm2_3",				0x080000, 0x72e8ae60, 3 | BRF_GRA },           // 16

	{ "gtg3_srom1_nr++.srom1",				0x100000, 0x44983bd7, 6 | BRF_SND },           // 17 Ensoniq Bank 2
	{ "gtg3_srom2_nr+.srom2",				0x080000, 0x1b3f18b6, 6 | BRF_SND },           // 18
	
	{ "itgfm-3 1997 it, inc",				0x001fff, 0x2527dffc, 0 | BRF_OPT },		   // 19 pic
};

STD_ROM_PICK(gt97s121)
STD_ROM_FN(gt97s121)

static INT32 Gt3dSverInit()
{
	Trackball_Type = TB_TYPE3;

	return Common32BitInit(0x112f, 2, 1);
}

struct BurnDriver BurnDrvGt97s121 = {
	"gt97s121", "gt97", NULL, NULL, "1997",
	"Golden Tee '97 (v1.21S)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt97s121RomInfo, gt97s121RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt97sDIPInfo,
	Gt3dSverInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '97 (v1.20)

static struct BurnRomInfo gt97v120RomDesc[] = {
	{ "gt97_prom0_v1.20.prom0",				0x080000, 0xcdc4226f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt97_prom1_v1.20.prom1",				0x080000, 0xb36fc43f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt97_prom2_v1.20.prom2",				0x080000, 0x30b0d97e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt97_prom3_v1.20.prom3",				0x080000, 0x77281d3a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt97nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt97_grom0_0.grm0_0",				0x080000, 0x81784aaf, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt97_grom0_1.grm0_1",				0x080000, 0x345bda44, 3 | BRF_GRA },           //  6
	{ "gt97_grom0_2.grm0_2",				0x080000, 0xb2beb40d, 3 | BRF_GRA },           //  7
	{ "gt97_grom0_3.grm0_3",				0x080000, 0x7cef32ff, 3 | BRF_GRA },           //  8
	{ "gt97_grom1_0.grm1_0",				0x080000, 0x1cc4c309, 3 | BRF_GRA },           //  9
	{ "gt97_grom1_1.grm1_1",				0x080000, 0x512cea45, 3 | BRF_GRA },           // 10
	{ "gt97_grom1_2.grm1_2",				0x080000, 0x0599b505, 3 | BRF_GRA },           // 11
	{ "gt97_grom1_3.grm1_3",				0x080000, 0x257eacc9, 3 | BRF_GRA },           // 12
	{ "gt97_grom2_0.grm2_0",				0x080000, 0x95b6e7ee, 3 | BRF_GRA },           // 13
	{ "gt97_grom2_1.grm2_1",				0x080000, 0xafd558b4, 3 | BRF_GRA },           // 14
	{ "gt97_grom2_2.grm2_2",				0x080000, 0x5b7a8733, 3 | BRF_GRA },           // 15
	{ "gt97_grom2_3.grm2_3",				0x080000, 0x72e8ae60, 3 | BRF_GRA },           // 16

	{ "gt97_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt97_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt97v120)
STD_ROM_FN(gt97v120)

struct BurnDriver BurnDrvGt97v120 = {
	"gt97v120", "gt97", NULL, NULL, "1997",
	"Golden Tee '97 (v1.20)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt97v120RomInfo, gt97v120RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt97oDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '97 Tournament (v2.43)

static struct BurnRomInfo gt97t243RomDesc[] = {
	{ "gt97_prom0_v2.43t.prom0",			0x100000, 0xb8de60f1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt97_prom1_v2.43t.prom1",			0x100000, 0x8152e5d3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt97_prom2_v2.43t.prom2",			0x100000, 0xb80061be, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt97_prom3_v2.43t.prom3",			0x100000, 0xd184968d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt97nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt97_grom0_0.grm0_0",				0x080000, 0x81784aaf, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt97_grom0_1.grm0_1",				0x080000, 0x345bda44, 3 | BRF_GRA },           //  6
	{ "gt97_grom0_2.grm0_2",				0x080000, 0xb2beb40d, 3 | BRF_GRA },           //  7
	{ "gt97_grom0_3.grm0_3",				0x080000, 0x7cef32ff, 3 | BRF_GRA },           //  8
	{ "gt97_grom1_0.grm1_0",				0x080000, 0x1cc4c309, 3 | BRF_GRA },           //  9
	{ "gt97_grom1_1.grm1_1",				0x080000, 0x512cea45, 3 | BRF_GRA },           // 10
	{ "gt97_grom1_2.grm1_2",				0x080000, 0x0599b505, 3 | BRF_GRA },           // 11
	{ "gt97_grom1_3.grm1_3",				0x080000, 0x257eacc9, 3 | BRF_GRA },           // 12
	{ "gt97_grom2_0.grm2_0",				0x080000, 0x95b6e7ee, 3 | BRF_GRA },           // 13
	{ "gt97_grom2_1.grm2_1",				0x080000, 0xafd558b4, 3 | BRF_GRA },           // 14
	{ "gt97_grom2_2.grm2_2",				0x080000, 0x5b7a8733, 3 | BRF_GRA },           // 15
	{ "gt97_grom2_3.grm2_3",				0x080000, 0x72e8ae60, 3 | BRF_GRA },           // 16

	{ "gt97_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt97_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt97t243)
STD_ROM_FN(gt97t243)

struct BurnDriver BurnDrvGt97t243 = {
	"gt97t243", "gt97", NULL, NULL, "1997",
	"Golden Tee '97 Tournament (v2.43)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt97t243RomInfo, gt97t243RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt97oDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '97 Tournament (v2.40)

static struct BurnRomInfo gt97t240RomDesc[] = {
	{ "gt97_prom0_v2.40t.prom0",			0x100000, 0x88a386d0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt97_prom1_v2.40t.prom1",			0x100000, 0xb0d751aa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt97_prom2_v2.40t.prom2",			0x100000, 0x451be534, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt97_prom3_v2.40t.prom3",			0x100000, 0x70da8ca5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt97nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt97_grom0_0.grm0_0",				0x080000, 0x81784aaf, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt97_grom0_1.grm0_1",				0x080000, 0x345bda44, 3 | BRF_GRA },           //  6
	{ "gt97_grom0_2.grm0_2",				0x080000, 0xb2beb40d, 3 | BRF_GRA },           //  7
	{ "gt97_grom0_3.grm0_3",				0x080000, 0x7cef32ff, 3 | BRF_GRA },           //  8
	{ "gt97_grom1_0.grm1_0",				0x080000, 0x1cc4c309, 3 | BRF_GRA },           //  9
	{ "gt97_grom1_1.grm1_1",				0x080000, 0x512cea45, 3 | BRF_GRA },           // 10
	{ "gt97_grom1_2.grm1_2",				0x080000, 0x0599b505, 3 | BRF_GRA },           // 11
	{ "gt97_grom1_3.grm1_3",				0x080000, 0x257eacc9, 3 | BRF_GRA },           // 12
	{ "gt97_grom2_0.grm2_0",				0x080000, 0x95b6e7ee, 3 | BRF_GRA },           // 13
	{ "gt97_grom2_1.grm2_1",				0x080000, 0xafd558b4, 3 | BRF_GRA },           // 14
	{ "gt97_grom2_2.grm2_2",				0x080000, 0x5b7a8733, 3 | BRF_GRA },           // 15
	{ "gt97_grom2_3.grm2_3",				0x080000, 0x72e8ae60, 3 | BRF_GRA },           // 16

	{ "gt97_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt97_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt97t240)
STD_ROM_FN(gt97t240)

struct BurnDriver BurnDrvGt97t240 = {
	"gt97t240", "gt97", NULL, NULL, "1997",
	"Golden Tee '97 Tournament (v2.40)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt97t240RomInfo, gt97t240RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt97oDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '98 (v1.10)

static struct BurnRomInfo gt98RomDesc[] = {
	{ "gt98_prom0_v1.10.prom0",				0x080000, 0xdd93ab2a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt98_prom1_v1.10.prom1",				0x080000, 0x6ea92960, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt98_prom2_v1.10.prom2",				0x080000, 0x27a8a15f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt98_prom3_v1.10.prom3",				0x080000, 0xd61f2bb2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt98nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt98_grom0_0.grm0_0",				0x080000, 0x2d79492b, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt98_grom0_1.grm0_1",				0x080000, 0x79afda1a, 3 | BRF_GRA },           //  6
	{ "gt98_grom0_2.grm0_2",				0x080000, 0x8c381f56, 3 | BRF_GRA },           //  7
	{ "gt98_grom0_3.grm0_3",				0x080000, 0x46c35ba6, 3 | BRF_GRA },           //  8
	{ "gt98_grom1_0.grm1_0",				0x080000, 0xb07bc634, 3 | BRF_GRA },           //  9
	{ "gt98_grom1_1.grm1_1",				0x080000, 0xb23d59a7, 3 | BRF_GRA },           // 10
	{ "gt98_grom1_2.grm1_2",				0x080000, 0x9c113abc, 3 | BRF_GRA },           // 11
	{ "gt98_grom1_3.grm1_3",				0x080000, 0x231bbe58, 3 | BRF_GRA },           // 12
	{ "gt98_grom2_0.grm2_0",				0x080000, 0xdb5cec87, 3 | BRF_GRA },           // 13
	{ "gt98_grom2_1.grm2_1",				0x080000, 0xc74fc7d3, 3 | BRF_GRA },           // 14
	{ "gt98_grom2_2.grm2_2",				0x080000, 0x1227609d, 3 | BRF_GRA },           // 15
	{ "gt98_grom2_3.grm2_3",				0x080000, 0x78745131, 3 | BRF_GRA },           // 16

	{ "gt98_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt98_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt98)
STD_ROM_FN(gt98)

struct BurnDriver BurnDrvGt98 = {
	"gt98", NULL, NULL, NULL, "1998",
	"Golden Tee '98 (v1.10)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt98RomInfo, gt98RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, AamaDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '98 (v1.00)

static struct BurnRomInfo gt98v100RomDesc[] = {
	{ "gt98_prom0_v1.00.prom0",				0x080000, 0xf2dc0a6c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt98_prom1_v1.00.prom1",				0x080000, 0xb0ca22f3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt98_prom2_v1.00.prom2",				0x080000, 0xf940acdc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt98_prom3_v1.00.prom3",				0x080000, 0x22d7e8dc, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt98nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt98_grom0_0.grm0_0",				0x080000, 0x2d79492b, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt98_grom0_1.grm0_1",				0x080000, 0x79afda1a, 3 | BRF_GRA },           //  6
	{ "gt98_grom0_2.grm0_2",				0x080000, 0x8c381f56, 3 | BRF_GRA },           //  7
	{ "gt98_grom0_3.grm0_3",				0x080000, 0x46c35ba6, 3 | BRF_GRA },           //  8
	{ "gt98_grom1_0.grm1_0",				0x080000, 0xb07bc634, 3 | BRF_GRA },           //  9
	{ "gt98_grom1_1.grm1_1",				0x080000, 0xb23d59a7, 3 | BRF_GRA },           // 10
	{ "gt98_grom1_2.grm1_2",				0x080000, 0x9c113abc, 3 | BRF_GRA },           // 11
	{ "gt98_grom1_3.grm1_3",				0x080000, 0x231bbe58, 3 | BRF_GRA },           // 12
	{ "gt98_grom2_0.grm2_0",				0x080000, 0xdb5cec87, 3 | BRF_GRA },           // 13
	{ "gt98_grom2_1.grm2_1",				0x080000, 0xc74fc7d3, 3 | BRF_GRA },           // 14
	{ "gt98_grom2_2.grm2_2",				0x080000, 0x1227609d, 3 | BRF_GRA },           // 15
	{ "gt98_grom2_3.grm2_3",				0x080000, 0x78745131, 3 | BRF_GRA },           // 16

	{ "gt98_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt98_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt98v100)
STD_ROM_FN(gt98v100)

struct BurnDriver BurnDrvGt98v100 = {
	"gt98v100", "gt98", NULL, NULL, "1998",
	"Golden Tee '98 (v1.00)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt98v100RomInfo, gt98v100RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt98DIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '98 (v1.00S)

static struct BurnRomInfo gt98s100RomDesc[] = {
	{ "gt98_prom0_v1.00s.prom0",			0x080000, 0x962ff444, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt98_prom1_v1.00s.prom1",			0x080000, 0xbe0ac375, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt98_prom2_v1.00s.prom2",			0x080000, 0x304e881c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt98_prom3_v1.00s.prom3",			0x080000, 0xac04ea81, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3_nr_u23_v2.2.u23",				0x020000, 0x04effd73, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt98_grom0_0.grm0_0",				0x080000, 0x2d79492b, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt98_grom0_1.grm0_1",				0x080000, 0x79afda1a, 3 | BRF_GRA },           //  6
	{ "gt98_grom0_2.grm0_2",				0x080000, 0x8c381f56, 3 | BRF_GRA },           //  7
	{ "gt98_grom0_3.grm0_3",				0x080000, 0x46c35ba6, 3 | BRF_GRA },           //  8
	{ "gt98_grom1_0.grm1_0",				0x080000, 0xb07bc634, 3 | BRF_GRA },           //  9
	{ "gt98_grom1_1.grm1_1",				0x080000, 0xb23d59a7, 3 | BRF_GRA },           // 10
	{ "gt98_grom1_2.grm1_2",				0x080000, 0x9c113abc, 3 | BRF_GRA },           // 11
	{ "gt98_grom1_3.grm1_3",				0x080000, 0x231bbe58, 3 | BRF_GRA },           // 12
	{ "gt98_grom2_0.grm2_0",				0x080000, 0xdb5cec87, 3 | BRF_GRA },           // 13
	{ "gt98_grom2_1.grm2_1",				0x080000, 0xc74fc7d3, 3 | BRF_GRA },           // 14
	{ "gt98_grom2_2.grm2_2",				0x080000, 0x1227609d, 3 | BRF_GRA },           // 15
	{ "gt98_grom2_3.grm2_3",				0x080000, 0x78745131, 3 | BRF_GRA },           // 16

	{ "gtg3_srom1_nr++.srom1",				0x100000, 0x44983bd7, 6 | BRF_SND },           // 17 Ensoniq Bank 2
	{ "gtg3_srom2_nr+.srom2",				0x080000, 0x1b3f18b6, 6 | BRF_SND },           // 18
};

STD_ROM_PICK(gt98s100)
STD_ROM_FN(gt98s100)

struct BurnDriver BurnDrvGt98s100 = {
	"gt98s100", "gt98", NULL, NULL, "1998",
	"Golden Tee '98 (v1.00S)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt98s100RomInfo, gt98s100RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt98sDIPInfo,
	Gt3dSverInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '98 Tournament (v3.03)

static struct BurnRomInfo gt98t303RomDesc[] = {
	{ "gt98_prom0_v3.03t.prom0",			0x100000, 0xe3879c30, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt98_prom1_v3.03t.prom1",			0x100000, 0x6a42ab1e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt98_prom2_v3.03t.prom2",			0x100000, 0xa695c1bc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt98_prom3_v3.03t.prom3",			0x100000, 0xbd7f5c7a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt98nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt98_grom0_0.grm0_0",				0x080000, 0x2d79492b, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt98_grom0_1.grm0_1",				0x080000, 0x79afda1a, 3 | BRF_GRA },           //  6
	{ "gt98_grom0_2.grm0_2",				0x080000, 0x8c381f56, 3 | BRF_GRA },           //  7
	{ "gt98_grom0_3.grm0_3",				0x080000, 0x46c35ba6, 3 | BRF_GRA },           //  8
	{ "gt98_grom1_0.grm1_0",				0x080000, 0xb07bc634, 3 | BRF_GRA },           //  9
	{ "gt98_grom1_1.grm1_1",				0x080000, 0xb23d59a7, 3 | BRF_GRA },           // 10
	{ "gt98_grom1_2.grm1_2",				0x080000, 0x9c113abc, 3 | BRF_GRA },           // 11
	{ "gt98_grom1_3.grm1_3",				0x080000, 0x231bbe58, 3 | BRF_GRA },           // 12
	{ "gt98_grom2_0.grm2_0",				0x080000, 0xdb5cec87, 3 | BRF_GRA },           // 13
	{ "gt98_grom2_1.grm2_1",				0x080000, 0xc74fc7d3, 3 | BRF_GRA },           // 14
	{ "gt98_grom2_2.grm2_2",				0x080000, 0x1227609d, 3 | BRF_GRA },           // 15
	{ "gt98_grom2_3.grm2_3",				0x080000, 0x78745131, 3 | BRF_GRA },           // 16

	{ "gt98_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt98_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt98t303)
STD_ROM_FN(gt98t303)

struct BurnDriver BurnDrvGt98t303 = {
	"gt98t303", "gt98", NULL, NULL, "1998",
	"Golden Tee '98 Tournament (v3.03)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt98t303RomInfo, gt98t303RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt98sDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '98 Tournament (v3.02)

static struct BurnRomInfo gt98t302RomDesc[] = {
	{ "gt98_prom0_v3.02t.prom0",			0x100000, 0x744e0d9b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt98_prom1_v3.02t.prom1",			0x100000, 0xb25508a1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt98_prom2_v3.02t.prom2",			0x100000, 0x98a3466e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt98_prom3_v3.02t.prom3",			0x100000, 0x17c3152a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt98nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt98_grom0_0.grm0_0",				0x080000, 0x2d79492b, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt98_grom0_1.grm0_1",				0x080000, 0x79afda1a, 3 | BRF_GRA },           //  6
	{ "gt98_grom0_2.grm0_2",				0x080000, 0x8c381f56, 3 | BRF_GRA },           //  7
	{ "gt98_grom0_3.grm0_3",				0x080000, 0x46c35ba6, 3 | BRF_GRA },           //  8
	{ "gt98_grom1_0.grm1_0",				0x080000, 0xb07bc634, 3 | BRF_GRA },           //  9
	{ "gt98_grom1_1.grm1_1",				0x080000, 0xb23d59a7, 3 | BRF_GRA },           // 10
	{ "gt98_grom1_2.grm1_2",				0x080000, 0x9c113abc, 3 | BRF_GRA },           // 11
	{ "gt98_grom1_3.grm1_3",				0x080000, 0x231bbe58, 3 | BRF_GRA },           // 12
	{ "gt98_grom2_0.grm2_0",				0x080000, 0xdb5cec87, 3 | BRF_GRA },           // 13
	{ "gt98_grom2_1.grm2_1",				0x080000, 0xc74fc7d3, 3 | BRF_GRA },           // 14
	{ "gt98_grom2_2.grm2_2",				0x080000, 0x1227609d, 3 | BRF_GRA },           // 15
	{ "gt98_grom2_3.grm2_3",				0x080000, 0x78745131, 3 | BRF_GRA },           // 16

	{ "gt98_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt98_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt98t302)
STD_ROM_FN(gt98t302)

struct BurnDriver BurnDrvGt98t302 = {
	"gt98t302", "gt98", NULL, NULL, "1998",
	"Golden Tee '98 Tournament (v3.02)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt98t302RomInfo, gt98t302RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt98sDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee Diamond Edition Tournament (v3.05T ELC)

static struct BurnRomInfo gtdiamondRomDesc[] = {
	{ "gt98_golf_elc_prom0_v3.05tl.prom0",	0x100000, 0xb6b0e3b8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt98_golf_elc_prom1_v3.05tl.prom1",	0x100000, 0xba15f3a3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt98_golf_elc_prom2_v3.05tl.prom2",	0x100000, 0x015e1c94, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt98_golf_elc_prom3_v3.05tl.prom3",	0x100000, 0xd990528b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt98nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt98_grom0_0.grm0_0",				0x080000, 0x2d79492b, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt98_grom0_1.grm0_1",				0x080000, 0x79afda1a, 3 | BRF_GRA },           //  6
	{ "gt98_grom0_2.grm0_2",				0x080000, 0x8c381f56, 3 | BRF_GRA },           //  7
	{ "gt98_grom0_3.grm0_3",				0x080000, 0x46c35ba6, 3 | BRF_GRA },           //  8
	{ "gt98_grom1_0.grm1_0",				0x080000, 0xb07bc634, 3 | BRF_GRA },           //  9
	{ "gt98_grom1_1.grm1_1",				0x080000, 0xb23d59a7, 3 | BRF_GRA },           // 10
	{ "gt98_grom1_2.grm1_2",				0x080000, 0x9c113abc, 3 | BRF_GRA },           // 11
	{ "gt98_grom1_3.grm1_3",				0x080000, 0x231bbe58, 3 | BRF_GRA },           // 12
	{ "gt98_grome2_0.grm2_0",				0x080000, 0x0c898920, 3 | BRF_GRA },           // 13
	{ "gt98_grome2_1.grm2_1",				0x080000, 0xcbe5b2b2, 3 | BRF_GRA },           // 14
	{ "gt98_grome2_2.grm2_2",				0x080000, 0x71bd4441, 3 | BRF_GRA },           // 15
	{ "gt98_grome2_3.grm2_3",				0x080000, 0x86149804, 3 | BRF_GRA },           // 16

	{ "gt98_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt98_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gtdiamond)
STD_ROM_FN(gtdiamond)

struct BurnDriver BurnDrvGtdiamond = {
	"gtdiamond", "gt98", NULL, NULL, "1998",
	"Golden Tee Diamond Edition Tournament (v3.05T ELC)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gtdiamondRomInfo, gtdiamondRomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt98sDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '99 (v1.00)

static struct BurnRomInfo gt99RomDesc[] = {
	{ "gt99_prom0_v1.00.prom0",				0x080000, 0x1ca05267, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt99_prom1_v1.00.prom1",				0x080000, 0x4fb757fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt99_prom2_v1.00.prom2",				0x080000, 0x3eb2b13a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt99_prom3_v1.00.prom3",				0x080000, 0x03454e7d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt99nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt99_grom0_0.grm0_0",				0x080000, 0xc22b50f9, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt99_grom0_1.grm0_1",				0x080000, 0xd6d6be57, 3 | BRF_GRA },           //  6
	{ "gt99_grom0_2.grm0_2",				0x080000, 0x005d4791, 3 | BRF_GRA },           //  7
	{ "gt99_grom0_3.grm0_3",				0x080000, 0x0c998eb7, 3 | BRF_GRA },           //  8
	{ "gt99_grom1_0.grm1_0",				0x080000, 0x8b79d6e2, 3 | BRF_GRA },           //  9
	{ "gt99_grom1_1.grm1_1",				0x080000, 0x84ef1803, 3 | BRF_GRA },           // 10
	{ "gt99_grom1_2.grm1_2",				0x080000, 0xd73d8afc, 3 | BRF_GRA },           // 11
	{ "gt99_grom1_3.grm1_3",				0x080000, 0x59f48688, 3 | BRF_GRA },           // 12
	{ "gt99_grom2_0.grm2_0",				0x080000, 0x693d9d68, 3 | BRF_GRA },           // 13
	{ "gt99_grom2_1.grm2_1",				0x080000, 0x2c0b8b8c, 3 | BRF_GRA },           // 14
	{ "gt99_grom2_2.grm2_2",				0x080000, 0xba1b5961, 3 | BRF_GRA },           // 15
	{ "gt99_grom2_3.grm2_3",				0x080000, 0xcfccd5c2, 3 | BRF_GRA },           // 16

	{ "gt99_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt99_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt99)
STD_ROM_FN(gt99)

struct BurnDriver BurnDrvGt99 = {
	"gt99", NULL, NULL, NULL, "1999",
	"Golden Tee '99 (v1.00)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt99RomInfo, gt99RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, AamaDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '99 (v1.00S)

static struct BurnRomInfo gt99s100RomDesc[] = {
	{ "gt99_prom0_v1.00s.prom0",			0x080000, 0x58e7c4e1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt99_prom1_v1.00s.prom1",			0x080000, 0x09f8bdf4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt99_prom2_v1.00s.prom2",			0x080000, 0xfd084b68, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt99_prom3_v1.00s.prom3",			0x080000, 0x3ff88ff7, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3_nr_u23_v2.2.u23",				0x020000, 0x04effd73, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt99_grom0_0.grm0_0",				0x080000, 0xc22b50f9, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt99_grom0_1.grm0_1",				0x080000, 0xd6d6be57, 3 | BRF_GRA },           //  6
	{ "gt99_grom0_2.grm0_2",				0x080000, 0x005d4791, 3 | BRF_GRA },           //  7
	{ "gt99_grom0_3.grm0_3",				0x080000, 0x0c998eb7, 3 | BRF_GRA },           //  8
	{ "gt99_grom1_0.grm1_0",				0x080000, 0x8b79d6e2, 3 | BRF_GRA },           //  9
	{ "gt99_grom1_1.grm1_1",				0x080000, 0x84ef1803, 3 | BRF_GRA },           // 10
	{ "gt99_grom1_2.grm1_2",				0x080000, 0xd73d8afc, 3 | BRF_GRA },           // 11
	{ "gt99_grom1_3.grm1_3",				0x080000, 0x59f48688, 3 | BRF_GRA },           // 12
	{ "gt99_grom2_0.grm2_0",				0x080000, 0x693d9d68, 3 | BRF_GRA },           // 13
	{ "gt99_grom2_1.grm2_1",				0x080000, 0x2c0b8b8c, 3 | BRF_GRA },           // 14
	{ "gt99_grom2_2.grm2_2",				0x080000, 0xba1b5961, 3 | BRF_GRA },           // 15
	{ "gt99_grom2_3.grm2_3",				0x080000, 0xcfccd5c2, 3 | BRF_GRA },           // 16

	{ "gtg3_srom1_nr++.srom1",				0x100000, 0x44983bd7, 6 | BRF_SND },           // 17 Ensoniq Bank 2
	{ "gtg3_srom2_nr+.srom2",				0x080000, 0x1b3f18b6, 6 | BRF_SND },           // 18
};

STD_ROM_PICK(gt99s100)
STD_ROM_FN(gt99s100)

struct BurnDriver BurnDrvGt99s100 = {
	"gt99s100", "gt99", NULL, NULL, "1999",
	"Golden Tee '99 (v1.00S)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt99s100RomInfo, gt99s100RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, S_verDIPInfo,
	Gt3dSverInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee '99 Tournament (v4.00)

static struct BurnRomInfo gt99t400RomDesc[] = {
	{ "gt99_prom0_v4.00t.prom0",			0x100000, 0xbc58e0a2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt99_prom1_v4.00t.prom1",			0x100000, 0x89d8cc6b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt99_prom2_v4.00t.prom2",			0x100000, 0x891e26c1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt99_prom3_v4.00t.prom3",			0x100000, 0x127f7aa7, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt99nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt99_grom0_0.grm0_0",				0x080000, 0xc22b50f9, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt99_grom0_1.grm0_1",				0x080000, 0xd6d6be57, 3 | BRF_GRA },           //  6
	{ "gt99_grom0_2.grm0_2",				0x080000, 0x005d4791, 3 | BRF_GRA },           //  7
	{ "gt99_grom0_3.grm0_3",				0x080000, 0x0c998eb7, 3 | BRF_GRA },           //  8
	{ "gt99_grom1_0.grm1_0",				0x080000, 0x8b79d6e2, 3 | BRF_GRA },           //  9
	{ "gt99_grom1_1.grm1_1",				0x080000, 0x84ef1803, 3 | BRF_GRA },           // 10
	{ "gt99_grom1_2.grm1_2",				0x080000, 0xd73d8afc, 3 | BRF_GRA },           // 11
	{ "gt99_grom1_3.grm1_3",				0x080000, 0x59f48688, 3 | BRF_GRA },           // 12
	{ "gt99_grom2_0.grm2_0",				0x080000, 0x693d9d68, 3 | BRF_GRA },           // 13
	{ "gt99_grom2_1.grm2_1",				0x080000, 0x2c0b8b8c, 3 | BRF_GRA },           // 14
	{ "gt99_grom2_2.grm2_2",				0x080000, 0xba1b5961, 3 | BRF_GRA },           // 15
	{ "gt99_grom2_3.grm2_3",				0x080000, 0xcfccd5c2, 3 | BRF_GRA },           // 16

	{ "gt99_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt99_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt99t400)
STD_ROM_FN(gt99t400)

struct BurnDriver BurnDrvGt99t400 = {
	"gt99t400", "gt99", NULL, NULL, "1999",
	"Golden Tee '99 Tournament (v4.00)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt99t400RomInfo, gt99t400RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt98sDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee Royal Edition Tournament (v4.02T EDM)

static struct BurnRomInfo gtroyalRomDesc[] = {
	{ "gtr_prom0_v4.02t_edm.prom0",			0x100000, 0xae499ea3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtr_prom1_v4.02t_edm.prom1",			0x100000, 0x87ee04b5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtr_prom2_v4.02t_edm.prom2",			0x100000, 0xa925d392, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtr_prom3_v4.02t_edm.prom3",			0x100000, 0x1c442664, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt99nr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt99_grom0_0.grm0_0",				0x080000, 0xc22b50f9, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt99_grom0_1.grm0_1",				0x080000, 0xd6d6be57, 3 | BRF_GRA },           //  6
	{ "gt99_grom0_2.grm0_2",				0x080000, 0x005d4791, 3 | BRF_GRA },           //  7
	{ "gt99_grom0_3.grm0_3",				0x080000, 0x0c998eb7, 3 | BRF_GRA },           //  8
	{ "gt99_grom1_0.grm1_0",				0x080000, 0x8b79d6e2, 3 | BRF_GRA },           //  9
	{ "gt99_grom1_1.grm1_1",				0x080000, 0x84ef1803, 3 | BRF_GRA },           // 10
	{ "gt99_grom1_2.grm1_2",				0x080000, 0xd73d8afc, 3 | BRF_GRA },           // 11
	{ "gt99_grom1_3.grm1_3",				0x080000, 0x59f48688, 3 | BRF_GRA },           // 12
	{ "gt99_grom2_0.grm2_0",				0x080000, 0x693d9d68, 3 | BRF_GRA },           // 13
	{ "gt99_grom2_1.grm2_1",				0x080000, 0x2c0b8b8c, 3 | BRF_GRA },           // 14
	{ "gt99_grom2_2.grm2_2",				0x080000, 0xba1b5961, 3 | BRF_GRA },           // 15
	{ "gt99_grom2_3.grm2_3",				0x080000, 0xcfccd5c2, 3 | BRF_GRA },           // 16

	{ "gt99_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt99_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gtroyal)
STD_ROM_FN(gtroyal)

struct BurnDriver BurnDrvGtroyal = {
	"gtroyal", "gt99", NULL, NULL, "1999",
	"Golden Tee Royal Edition Tournament (v4.02T EDM)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gtroyalRomInfo, gtroyalRomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt98sDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 2K (v1.00)

static struct BurnRomInfo gt2kRomDesc[] = {
	{ "gt2k_prom0_v1.00.prom0",				0x080000, 0xb83d7b67, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt2k_prom1_v1.00.prom1",				0x080000, 0x89bd952d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt2k_prom2_v1.00.prom2",				0x080000, 0xb603d283, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt2k_prom3_v1.00.prom3",				0x080000, 0x85ba9e2d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt2knr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt2k_grom0_0.grm0_0",				0x080000, 0xc22b50f9, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt2k_grom0_1.grm0_1",				0x080000, 0xd6d6be57, 3 | BRF_GRA },           //  6
	{ "gt2k_grom0_2.grm0_2",				0x080000, 0x005d4791, 3 | BRF_GRA },           //  7
	{ "gt2k_grom0_3.grm0_3",				0x080000, 0x0c998eb7, 3 | BRF_GRA },           //  8
	{ "gt2k_grom1_0.grm1_0",				0x080000, 0x8b79d6e2, 3 | BRF_GRA },           //  9
	{ "gt2k_grom1_1.grm1_1",				0x080000, 0x84ef1803, 3 | BRF_GRA },           // 10
	{ "gt2k_grom1_2.grm1_2",				0x080000, 0xd73d8afc, 3 | BRF_GRA },           // 11
	{ "gt2k_grom1_3.grm1_3",				0x080000, 0x59f48688, 3 | BRF_GRA },           // 12
	{ "gt2k_grom2_0.grm2_0",				0x080000, 0xcc11b93f, 3 | BRF_GRA },           // 13
	{ "gt2k_grom2_1.grm2_1",				0x080000, 0x1c3a0126, 3 | BRF_GRA },           // 14
	{ "gt2k_grom2_2.grm2_2",				0x080000, 0x97814df5, 3 | BRF_GRA },           // 15
	{ "gt2k_grom2_3.grm2_3",				0x080000, 0xf0f7373f, 3 | BRF_GRA },           // 16

	{ "gt2k_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt2k_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt2k)
STD_ROM_FN(gt2k)

struct BurnDriver BurnDrvGt2k = {
	"gt2k", NULL, NULL, NULL, "2000",
	"Golden Tee 2K (v1.00)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt2kRomInfo, gt2kRomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, AamaDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 2K (v1.00) (alt protection)

static struct BurnRomInfo gt2kp100RomDesc[] = {
	{ "gt2kprm0.10p",						0x080000, 0x16e8502d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt2kprm1.10p",						0x080000, 0xbf47cd95, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt2kprm2.10p",						0x080000, 0x204ddf15, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt2kprm3.10p",						0x080000, 0x45b9dd56, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt2knr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt2k_grom0_0.grm0_0",				0x080000, 0xc22b50f9, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt2k_grom0_1.grm0_1",				0x080000, 0xd6d6be57, 3 | BRF_GRA },           //  6
	{ "gt2k_grom0_2.grm0_2",				0x080000, 0x005d4791, 3 | BRF_GRA },           //  7
	{ "gt2k_grom0_3.grm0_3",				0x080000, 0x0c998eb7, 3 | BRF_GRA },           //  8
	{ "gt2k_grom1_0.grm1_0",				0x080000, 0x8b79d6e2, 3 | BRF_GRA },           //  9
	{ "gt2k_grom1_1.grm1_1",				0x080000, 0x84ef1803, 3 | BRF_GRA },           // 10
	{ "gt2k_grom1_2.grm1_2",				0x080000, 0xd73d8afc, 3 | BRF_GRA },           // 11
	{ "gt2k_grom1_3.grm1_3",				0x080000, 0x59f48688, 3 | BRF_GRA },           // 12
	{ "gt2k_grom2_0.grm2_0",				0x080000, 0xcc11b93f, 3 | BRF_GRA },           // 13
	{ "gt2k_grom2_1.grm2_1",				0x080000, 0x1c3a0126, 3 | BRF_GRA },           // 14
	{ "gt2k_grom2_2.grm2_2",				0x080000, 0x97814df5, 3 | BRF_GRA },           // 15
	{ "gt2k_grom2_3.grm2_3",				0x080000, 0xf0f7373f, 3 | BRF_GRA },           // 16

	{ "gt2k_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt2k_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt2kp100)
STD_ROM_FN(gt2kp100)

static INT32 Gt2kp100Init()
{
	Trackball_Type = TB_TYPE0;

	INT32 nRet = Common32BitInit(0x10000, 2, 1);

	memset (Drv68KRAM, 1, 4);

	return nRet;
}

struct BurnDriver BurnDrvGt2kp100 = {
	"gt2kp100", "gt2k", NULL, NULL, "2000",
	"Golden Tee 2K (v1.00) (alt protection)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt2kp100RomInfo, gt2kp100RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, AamaDIPInfo,
	Gt2kp100Init, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 2K (v1.00S)

static struct BurnRomInfo gt2ks100RomDesc[] = {
	{ "gt2k_kit_prom0_v1.00m.prom0",		0x080000, 0x3aab67c8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt2k_kit_prom1_v1.00m.prom1",		0x080000, 0x47d4a74d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt2k_kit_prom2_v1.00m.prom2",		0x080000, 0x77a222cc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt2k_kit_prom3_v1.00m.prom3",		0x080000, 0xc3e77ad5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3_nr_u23_v2.2.u23",				0x020000, 0x04effd73, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt2k_grom0_0.grm0_0",				0x080000, 0xc22b50f9, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt2k_grom0_1.grm0_1",				0x080000, 0xd6d6be57, 3 | BRF_GRA },           //  6
	{ "gt2k_grom0_2.grm0_2",				0x080000, 0x005d4791, 3 | BRF_GRA },           //  7
	{ "gt2k_grom0_3.grm0_3",				0x080000, 0x0c998eb7, 3 | BRF_GRA },           //  8
	{ "gt2k_grom1_0.grm1_0",				0x080000, 0x8b79d6e2, 3 | BRF_GRA },           //  9
	{ "gt2k_grom1_1.grm1_1",				0x080000, 0x84ef1803, 3 | BRF_GRA },           // 10
	{ "gt2k_grom1_2.grm1_2",				0x080000, 0xd73d8afc, 3 | BRF_GRA },           // 11
	{ "gt2k_grom1_3.grm1_3",				0x080000, 0x59f48688, 3 | BRF_GRA },           // 12
	{ "gt2k_grom2_0.grm2_0",				0x080000, 0xcc11b93f, 3 | BRF_GRA },           // 13
	{ "gt2k_grom2_1.grm2_1",				0x080000, 0x1c3a0126, 3 | BRF_GRA },           // 14
	{ "gt2k_grom2_2.grm2_2",				0x080000, 0x97814df5, 3 | BRF_GRA },           // 15
	{ "gt2k_grom2_3.grm2_3",				0x080000, 0xf0f7373f, 3 | BRF_GRA },           // 16

	{ "gtg3_srom1_nr++.srom1",				0x100000, 0x44983bd7, 6 | BRF_SND },           // 17 Ensoniq Bank 2
	{ "gtg3_srom2_nr+.srom2",				0x080000, 0x1b3f18b6, 6 | BRF_SND },           // 18
};

STD_ROM_PICK(gt2ks100)
STD_ROM_FN(gt2ks100)

struct BurnDriver BurnDrvGt2ks100 = {
	"gt2ks100", "gt2k", NULL, NULL, "2000",
	"Golden Tee 2K (v1.00S)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt2ks100RomInfo, gt2ks100RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, S_verDIPInfo,
	Gt3dSverInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee 2K Tournament (v5.00)

static struct BurnRomInfo gt2kt500RomDesc[] = {
	{ "gt2k_prom0_v5.00t.prom0",			0x100000, 0x8f20f9eb, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt2k_prom1_v5.00t.prom1",			0x100000, 0xbdecc1f5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt2k_prom2_v5.00t.prom2",			0x100000, 0x46666c15, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt2k_prom3_v5.00t.prom3",			0x100000, 0x89544fbc, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt2knr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt2k_grom0_0.grm0_0",				0x080000, 0xc22b50f9, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt2k_grom0_1.grm0_1",				0x080000, 0xd6d6be57, 3 | BRF_GRA },           //  6
	{ "gt2k_grom0_2.grm0_2",				0x080000, 0x005d4791, 3 | BRF_GRA },           //  7
	{ "gt2k_grom0_3.grm0_3",				0x080000, 0x0c998eb7, 3 | BRF_GRA },           //  8
	{ "gt2k_grom1_0.grm1_0",				0x080000, 0x8b79d6e2, 3 | BRF_GRA },           //  9
	{ "gt2k_grom1_1.grm1_1",				0x080000, 0x84ef1803, 3 | BRF_GRA },           // 10
	{ "gt2k_grom1_2.grm1_2",				0x080000, 0xd73d8afc, 3 | BRF_GRA },           // 11
	{ "gt2k_grom1_3.grm1_3",				0x080000, 0x59f48688, 3 | BRF_GRA },           // 12
	{ "gt2k_grom2_0.grm2_0",				0x080000, 0xcc11b93f, 3 | BRF_GRA },           // 13
	{ "gt2k_grom2_1.grm2_1",				0x080000, 0x1c3a0126, 3 | BRF_GRA },           // 14
	{ "gt2k_grom2_2.grm2_2",				0x080000, 0x97814df5, 3 | BRF_GRA },           // 15
	{ "gt2k_grom2_3.grm2_3",				0x080000, 0xf0f7373f, 3 | BRF_GRA },           // 16

	{ "gt2k_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt2k_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gt2kt500)
STD_ROM_FN(gt2kt500)

struct BurnDriver BurnDrvGt2kt500 = {
	"gt2kt500", "gt2k", NULL, NULL, "2000",
	"Golden Tee 2K Tournament (v5.00)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gt2kt500RomInfo, gt2kt500RomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt98sDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee Supreme Edition Tournament (v5.10T ELC S)

static struct BurnRomInfo gtsupremeRomDesc[] = {
	{ "gtg_sup_elc_prom0_5.10t.prom0",		0x100000, 0xa14f7e2b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtg_sup_elc_prom1_5.10t.prom1",		0x100000, 0x772f4dc9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtg_sup_elc_prom2_5.10t.prom2",		0x100000, 0xfbaae916, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtg_sup_elc_prom3_5.10t.prom3",		0x100000, 0x69b13204, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt2knr_u88_v1.0.u88",				0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt2k_grom0_0.grm0_0",				0x080000, 0xc22b50f9, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt2k_grom0_1.grm0_1",				0x080000, 0xd6d6be57, 3 | BRF_GRA },           //  6
	{ "gt2k_grom0_2.grm0_2",				0x080000, 0x005d4791, 3 | BRF_GRA },           //  7
	{ "gt2k_grom0_3.grm0_3",				0x080000, 0x0c998eb7, 3 | BRF_GRA },           //  8
	{ "gt2k_grom1_0.grm1_0",				0x080000, 0x8b79d6e2, 3 | BRF_GRA },           //  9
	{ "gt2k_grom1_1.grm1_1",				0x080000, 0x84ef1803, 3 | BRF_GRA },           // 10
	{ "gt2k_grom1_2.grm1_2",				0x080000, 0xd73d8afc, 3 | BRF_GRA },           // 11
	{ "gt2k_grom1_3.grm1_3",				0x080000, 0x59f48688, 3 | BRF_GRA },           // 12
	{ "gt_supreme_grom2_0.grm2_0",			0x080000, 0x33998a3e, 3 | BRF_GRA },           // 13
	{ "gt_supreme_grom2_1.grm2_1",			0x080000, 0xafa937ef, 3 | BRF_GRA },           // 14
	{ "gt_supreme_grom2_2.grm2_2",			0x080000, 0x8f39c061, 3 | BRF_GRA },           // 15
	{ "gt_supreme_grom2_3.grm2_3",			0x080000, 0xc3c2337a, 3 | BRF_GRA },           // 16

	{ "gt2k_srom0_nr.srom0",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt2k_srom1_nr.srom1",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gtsupreme)
STD_ROM_FN(gtsupreme)

struct BurnDriver BurnDrvGtsupreme = {
	"gtsupreme", "gt2k", NULL, NULL, "2002",
	"Golden Tee Supreme Edition Tournament (v5.10T ELC S)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gtsupremeRomInfo, gtsupremeRomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, Gt98sDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee Classic (v1.00)

static struct BurnRomInfo gtclasscRomDesc[] = {
	{ "gt_classic_prom0_v1.00.prom0",		0x080000, 0xa57e6ef0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt_classic_prom1_v1.00.prom1",		0x080000, 0x15f8a831, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt_classic_prom2_v1.00.prom2",		0x080000, 0x2f260a93, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt_classic_prom3_v1.00.prom3",		0x080000, 0x03a1fcdd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt_classicnr_u88_v1.0.u88",			0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt_classic_grom0_0.grm0_0",			0x080000, 0xc22b50f9, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt_classic_grom0_1.grm0_1",			0x080000, 0xd6d6be57, 3 | BRF_GRA },           //  6
	{ "gt_classic_grom0_2.grm0_2",			0x080000, 0x005d4791, 3 | BRF_GRA },           //  7
	{ "gt_classic_grom0_3.grm0_3",			0x080000, 0x0c998eb7, 3 | BRF_GRA },           //  8
	{ "gt_classic_grom1_0.grm1_0",			0x080000, 0x8b79d6e2, 3 | BRF_GRA },           //  9
	{ "gt_classic_grom1_1.grm1_1",			0x080000, 0x84ef1803, 3 | BRF_GRA },           // 10
	{ "gt_classic_grom1_2.grm1_2",			0x080000, 0xd73d8afc, 3 | BRF_GRA },           // 11
	{ "gt_classic_grom1_3.grm1_3",			0x080000, 0x59f48688, 3 | BRF_GRA },           // 12
	{ "gt_classic_grom2_0.grm2_0",			0x080000, 0xc4f54398, 3 | BRF_GRA },           // 13
	{ "gt_classic_grom2_1.grm2_1",			0x080000, 0x2c1f83cf, 3 | BRF_GRA },           // 14
	{ "gt_classic_grom2_2.grm2_2",			0x080000, 0x607657a6, 3 | BRF_GRA },           // 15
	{ "gt_classic_grom2_3.grm2_3",			0x080000, 0x7ad615c1, 3 | BRF_GRA },           // 16

	{ "gt_classic_srom0.nr",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt_classic_srom1.nr",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gtclassc)
STD_ROM_FN(gtclassc)

struct BurnDriver BurnDrvGtclassc = {
	"gtclassc", NULL, NULL, NULL, "2001",
	"Golden Tee Classic (v1.00)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gtclasscRomInfo, gtclasscRomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, AamaDIPInfo,
	Gt3dAmaaInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee Classic (v1.00) (alt protection)

static struct BurnRomInfo gtclasscpRomDesc[] = {
	{ "gtcpprm0.100",						0x080000, 0x21f0e0ea, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gtcpprm1.100",						0x080000, 0xd2a69fbc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gtcpprm2.100",						0x080000, 0xa8dea029, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gtcpprm3.100",						0x080000, 0x6016299e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gt_classicnr_u88_v1.0.u88",			0x020000, 0x2cee9e98, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt_classic_grom0_0.grm0_0",			0x080000, 0xc22b50f9, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt_classic_grom0_1.grm0_1",			0x080000, 0xd6d6be57, 3 | BRF_GRA },           //  6
	{ "gt_classic_grom0_2.grm0_2",			0x080000, 0x005d4791, 3 | BRF_GRA },           //  7
	{ "gt_classic_grom0_3.grm0_3",			0x080000, 0x0c998eb7, 3 | BRF_GRA },           //  8
	{ "gt_classic_grom1_0.grm1_0",			0x080000, 0x8b79d6e2, 3 | BRF_GRA },           //  9
	{ "gt_classic_grom1_1.grm1_1",			0x080000, 0x84ef1803, 3 | BRF_GRA },           // 10
	{ "gt_classic_grom1_2.grm1_2",			0x080000, 0xd73d8afc, 3 | BRF_GRA },           // 11
	{ "gt_classic_grom1_3.grm1_3",			0x080000, 0x59f48688, 3 | BRF_GRA },           // 12
	{ "gt_classic_grom2_0.grm2_0",			0x080000, 0xc4f54398, 3 | BRF_GRA },           // 13
	{ "gt_classic_grom2_1.grm2_1",			0x080000, 0x2c1f83cf, 3 | BRF_GRA },           // 14
	{ "gt_classic_grom2_2.grm2_2",			0x080000, 0x607657a6, 3 | BRF_GRA },           // 15
	{ "gt_classic_grom2_3.grm2_3",			0x080000, 0x7ad615c1, 3 | BRF_GRA },           // 16

	{ "gt_classic_srom0.nr",				0x100000, 0x44983bd7, 4 | BRF_SND },           // 17 Ensoniq Bank 0
	{ "gt_classic_srom1.nr",				0x080000, 0x1b3f18b6, 4 | BRF_SND },           // 18
};

STD_ROM_PICK(gtclasscp)
STD_ROM_FN(gtclasscp)

static INT32 GtclasscpInit()
{
	Trackball_Type = TB_TYPE0;

	INT32 nRet = Common32BitInit(0x10000, 2, 1);

	memset (Drv68KRAM + 0x10000, 0x80, 4); // security hack

	return nRet;
}

struct BurnDriver BurnDrvGtclasscp = {
	"gtclasscp", "gtclassc", NULL, NULL, "2001",
	"Golden Tee Classic (v1.00) (alt protection)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gtclasscpRomInfo, gtclasscpRomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, AamaDIPInfo,
	GtclasscpInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};


// Golden Tee Classic (v1.00S)

static struct BurnRomInfo gtclasscsRomDesc[] = {
	{ "gt_classic_prom0_v1.00m.prom0",		0x080000, 0x1e41884f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gt_classic_prom1_v1.00m.prom1",		0x080000, 0x31c18b2c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gt_classic_prom2_v1.00m.prom2",		0x080000, 0x8896efcb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gt_classic_prom3_v1.00m.prom3",		0x080000, 0x567a9490, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gtg3_nr_u23_v2.2.u23",				0x020000, 0x04effd73, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 Code

	{ "gt_classic_grom0_0.grm0_0",			0x080000, 0xc22b50f9, 3 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "gt_classic_grom0_1.grm0_1",			0x080000, 0xd6d6be57, 3 | BRF_GRA },           //  6
	{ "gt_classic_grom0_2.grm0_2",			0x080000, 0x005d4791, 3 | BRF_GRA },           //  7
	{ "gt_classic_grom0_3.grm0_3",			0x080000, 0x0c998eb7, 3 | BRF_GRA },           //  8
	{ "gt_classic_grom1_0.grm1_0",			0x080000, 0x8b79d6e2, 3 | BRF_GRA },           //  9
	{ "gt_classic_grom1_1.grm1_1",			0x080000, 0x84ef1803, 3 | BRF_GRA },           // 10
	{ "gt_classic_grom1_2.grm1_2",			0x080000, 0xd73d8afc, 3 | BRF_GRA },           // 11
	{ "gt_classic_grom1_3.grm1_3",			0x080000, 0x59f48688, 3 | BRF_GRA },           // 12
	{ "gt_classic_grom2_0.grm2_0",			0x080000, 0xc4f54398, 3 | BRF_GRA },           // 13
	{ "gt_classic_grom2_1.grm2_1",			0x080000, 0x2c1f83cf, 3 | BRF_GRA },           // 14
	{ "gt_classic_grom2_2.grm2_2",			0x080000, 0x607657a6, 3 | BRF_GRA },           // 15
	{ "gt_classic_grom2_3.grm2_3",			0x080000, 0x7ad615c1, 3 | BRF_GRA },           // 16

	{ "gtg3_srom1_nr++.srom1",				0x100000, 0x44983bd7, 6 | BRF_SND },           // 17 Ensoniq Bank 2
	{ "gtg3_srom2_nr+.srom2",				0x080000, 0x1b3f18b6, 6 | BRF_SND },           // 18
};

STD_ROM_PICK(gtclasscs)
STD_ROM_FN(gtclasscs)

struct BurnDriver BurnDrvGtclasscs = {
	"gtclasscs", "gtclassc", NULL, NULL, "2001",
	"Golden Tee Classic (v1.00S)\0", NULL, "Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gtclasscsRomInfo, gtclasscsRomName, NULL, NULL, NULL, NULL, Gt3dInputInfo, S_verDIPInfo,
	Gt3dSverInit, DrvExit, DrvFrame, DrvDraw32, DrvScan, &DrvRecalc, 0x8000,
	384, 240, 4, 3
};
