// FB Alpha Seta driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "m6502_intf.h"
#include "burn_ym2612.h"
#include "burn_ym3812.h"
#include "burn_ym2203.h"
#include "burn_gun.h"
#include "msm6295.h"
#include "x1010.h"

/*
	To do:
		flipscreen support
		jockeyc needs work...
*/

#define NOIRQ2				0x80
#define SET_IRQLINES(vblank, irq2)	(((vblank) << 8) | (irq2))
#define SPRITE_BUFFER			1
#define NO_SPRITE_BUFFER		0
#define SET_GFX_DECODE(n0, n1, n2)	n0, n1, n2

static UINT8 *AllMem		= NULL;
static UINT8 *MemEnd		= NULL;
static UINT8 *AllRam		= NULL;
static UINT8 *RamEnd		= NULL;
static UINT8 *Drv68KROM		= NULL;
static UINT8 *DrvSubROM		= NULL;
static UINT8 *DrvGfxROM0	= NULL;
static UINT8 *DrvGfxROM1	= NULL;
static UINT8 *DrvGfxROM2	= NULL;
static UINT8 *DrvColPROM	= NULL;
static UINT8 *DrvSndROM		= NULL;
static UINT8 *Drv68KRAM		= NULL;
static UINT8 *Drv68KRAM2	= NULL;
static UINT8 *Drv68KRAM3	= NULL;
static UINT8 *DrvSubRAM		= NULL;
static UINT8 *DrvShareRAM	= NULL;
static UINT8 *DrvNVRAM		= NULL;
static UINT8 *DrvPalRAM		= NULL;
static UINT8 *DrvSprRAM0	= NULL;
static UINT8 *DrvSprRAM1	= NULL;
static UINT8 *DrvVidRAM0	= NULL;
static UINT8 *DrvVidRAM1	= NULL;
static UINT8 *DrvVIDCTRLRAM0	= NULL;
static UINT8 *DrvVIDCTRLRAM1	= NULL;
static UINT8 *DrvVideoRegs	= NULL;

static UINT32 *Palette		= NULL;
static UINT32 *DrvPalette	= NULL;
static UINT8 DrvRecalc;

static UINT8 soundlatch     = 0;
static UINT8 soundlatch2    = 0;

static UINT8 *tilebank		= NULL;
static UINT32 *tile_offset	= NULL;

// allow us to override generic rom loading
static INT32 (*pRomLoadCallback)(INT32 bLoad) = NULL;

static INT32 cpuspeed = 0;
static INT32 irqtype = 0;
static INT32 buffer_sprites = 0;
static INT32 DrvROMLen[5] = { 0, 0, 0, 0, 0 };
static INT32 DrvGfxMask[3] = { 0, 0, 0 };
static UINT8 *DrvGfxTransMask[3] = { NULL, NULL, NULL };
static INT32 VideoOffsets[3][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 } };
static INT32 ColorOffsets[3] = { 0, 0, 0 };
static INT32 ColorDepths[3];
static INT32 twineagle = 0;
static INT32 daiohc = 0; // lazy fix - disable writes to alternate scroll write offsets
static INT32 usclssic = 0;
static INT32 oisipuzl_hack = 0; // 32px sprite offset
static INT32 refresh_rate = 6000;

static INT32 seta_samples_bank = 0;
static INT32 usclssic_port_select = 0;
static INT32 gun_input_bit = 0;
static INT32 gun_input_src = 0;
static INT32 flipflop = 0;

static INT32 watchdog_enable = 0; // not dynamic (config @ game init)
static INT32 watchdog = 0;
static INT32 flipscreen;
static INT32 m65c02_mode = 0; // not dynamic (config @ game init)
static INT32 m65c02_bank = 0;
static INT32 sub_ctrl_data = 0;
static INT32 has_2203 = 0;
static INT32 has_z80 = 0;

static INT32 DrvAxis[4];
static UINT16 DrvAnalogInput[4];
static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvJoy5[16];
static UINT8 DrvJoy6[16];
static UINT8 DrvJoy7[16];
static UINT8 DrvDips[7];
static UINT16 DrvInputs[7];
static UINT8 DrvReset;

static INT32 has_raster = 0; // for raster effect
static INT32 raster_needs_update = 0;
static INT32 lastline;
static INT32 scanline;
static void rasterUpdateDraw(); // forward

// trackball stuff for Krazy Bowl & usclssic
static INT32 trackball_mode = 0;
static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;
static INT16 DrvAnalogPort2 = 0;
static INT16 DrvAnalogPort3 = 0;
static UINT32 track_x = 0;
static UINT32 track_y = 0;
static INT32 track_x_last = 0;
static INT32 track_y_last = 0;
static UINT32 track_x2 = 0;
static UINT32 track_y2 = 0;
static INT32 track_x2_last = 0;
static INT32 track_y2_last = 0;

// Rotation stuff! -dink
static UINT8  DrvFakeInput[14]      = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // 0-5 legacy; 6-9 P1, 10-13 P2
static UINT8  nRotateHoldInput[2]   = {0, 0};
static INT32  nRotate[2]            = {0, 0};
static INT32  nRotateTarget[2]      = {0, 0};
static INT32  nRotateTry[2]         = {0, 0};
static UINT32 nRotateTime[2]        = {0, 0};
static UINT8  game_rotates = 0;
static UINT8  clear_opposites = 0;
static UINT8  nAutoFireCounter[2] 	= {0, 0};

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }

static struct BurnInputInfo QzkklogyInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 5"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Qzkklogy)

static struct BurnInputInfo DrgnunitInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Drgnunit)

static struct BurnInputInfo StgInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Stg)

static struct BurnInputInfo Qzkklgy2InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 4"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Qzkklgy2)

static struct BurnInputInfo DaiohInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 6"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 fire 6"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Daioh)

static struct BurnInputInfo RezonInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Rezon)

static struct BurnInputInfo EightfrcInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Eightfrc)

static struct BurnInputInfo WrofaeroInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Wrofaero)

static struct BurnInputInfo ZingzipInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Zingzip)

static struct BurnInputInfo MsgundamInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Msgundam)

static struct BurnInputInfo KamenridInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Kamenrid)

static struct BurnInputInfo MadsharkInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Madshark)

static struct BurnInputInfo WitsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p3 start"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy4 + 2,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy4 + 3,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy4 + 0,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p3 fire 2"	},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy5 + 7,	"p4 start"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy5 + 2,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy5 + 3,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy5 + 0,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy5 + 1,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy5 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy5 + 5,	"p4 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Wits)

static struct BurnInputInfo ThunderlInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Thunderl)

static struct BurnInputInfo AtehateInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 4"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Atehate)

static struct BurnInputInfo BlockcarInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Blockcar)

static struct BurnInputInfo GundharaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Gundhara)

static struct BurnInputInfo BlandiaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Blandia)

static struct BurnInputInfo OisipuzlInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Oisipuzl)

static struct BurnInputInfo PairloveInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Pairlove)

static struct BurnInputInfo OrbsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Orbs)

static struct BurnInputInfo KeroppiInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Keroppi)

static struct BurnInputInfo JjsquawkInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Jjsquawk)

static struct BurnInputInfo ExtdwnhlInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Extdwnhl)

static struct BurnInputInfo KrzybowlInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	A("P2 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis"),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Krzybowl)

static struct BurnInputInfo UtoukondInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Utoukond)

static struct BurnInputInfo TndrcadeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 5,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tndrcade)

static struct BurnInputInfo DowntownInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3 (rotate)",		BIT_DIGITAL,	DrvFakeInput + 4,	"p1 fire 3"	},
	{"P1 Shoot Up"       	, BIT_DIGITAL  , DrvFakeInput + 6,  "p1 up 2" }, // 6
	{"P1 Shoot Down"      	, BIT_DIGITAL  , DrvFakeInput + 7,  "p1 down 2" }, // 7
	{"P1 Shoot Left"       	, BIT_DIGITAL  , DrvFakeInput + 8,  "p1 left 2" }, // 8
	{"P1 Shoot Right"      	, BIT_DIGITAL  , DrvFakeInput + 9,  "p1 right 2" }, // 9

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3 (rotate)",		BIT_DIGITAL,	DrvFakeInput + 5,	"p2 fire 3"	},
	{"P2 Shoot Up"       	, BIT_DIGITAL  , DrvFakeInput + 10, "p2 up 2" },
	{"P2 Shoot Down"      	, BIT_DIGITAL  , DrvFakeInput + 11, "p2 down 2" },
	{"P2 Shoot Left"       	, BIT_DIGITAL  , DrvFakeInput + 12, "p2 left 2" },
	{"P2 Shoot Right"      	, BIT_DIGITAL  , DrvFakeInput + 13, "p2 right 2" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 5,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 4,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	// Auto-fire on right-stick
	{"Dip D", 		BIT_DIPSWITCH, 	DrvDips + 3, 	"dip"       },

};

STDINPUTINFO(Downtown)

static struct BurnInputInfo MetafoxInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 5,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 4,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Metafox)

static struct BurnInputInfo ZombraidInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"mouse button 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"mouse button 2"	},
	A("P1 Right / left",	BIT_ANALOG_REL, DrvAxis + 0,	"mouse x-axis"),
	A("P1 Up / Down",	BIT_ANALOG_REL, DrvAxis + 1,	"mouse y-axis"),

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	A("P2 Right / left",	BIT_ANALOG_REL, DrvAxis + 2,	"p2 x-axis"),
	A("P2 Up / Down",	BIT_ANALOG_REL, DrvAxis + 3,	"p2 y-axis"),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Zombraid)

static struct BurnInputInfo KiwameInputList[] = {
	{"Coin 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"Coin 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"Start",		BIT_DIGITAL,	DrvJoy4 + 5,	"p1 start"	},
	{"P1 A",		BIT_DIGITAL,	DrvJoy4 + 0,	"mah a"		},
	{"P1 B",		BIT_DIGITAL,	DrvJoy3 + 0,	"mah b"		},
	{"P1 C",		BIT_DIGITAL,	DrvJoy5 + 0,	"mah c"		},
	{"P1 D",		BIT_DIGITAL,	DrvJoy6 + 0,	"mah d"		},
	{"P1 E",		BIT_DIGITAL,	DrvJoy4 + 1,	"mah e"		},
	{"P1 F",		BIT_DIGITAL,	DrvJoy2 + 3,	"mah f"		},
	{"P1 G",		BIT_DIGITAL,	DrvJoy5 + 1,	"mah g"		},
	{"P1 H",		BIT_DIGITAL,	DrvJoy6 + 1,	"mah h"		},
	{"P1 I",		BIT_DIGITAL,	DrvJoy4 + 2,	"mah i"		},
	{"P1 J",		BIT_DIGITAL,	DrvJoy3 + 2,	"mah j"		},
	{"P1 K",		BIT_DIGITAL,	DrvJoy4 + 4,	"mah k"		},
	{"P1 L",		BIT_DIGITAL,	DrvJoy2 + 0,	"mah l"		},
	{"P1 M",		BIT_DIGITAL,	DrvJoy4 + 3,	"mah m"		},
	{"P1 N",		BIT_DIGITAL,	DrvJoy3 + 3,	"mah n"		},
	{"P1 Pon",		BIT_DIGITAL,	DrvJoy6 + 3,	"mah pon"	},
	{"P1 Chi",		BIT_DIGITAL,	DrvJoy5 + 3,	"mah chi"	},
	{"P1 Kan",		BIT_DIGITAL,	DrvJoy4 + 4,	"mah kan"	},
	{"P1 Ron",		BIT_DIGITAL,	DrvJoy5 + 4,	"mah ron"	},
	{"P1 Reach",	BIT_DIGITAL,	DrvJoy3 + 4,	"mah reach"	},
	{"P1 Flip Flip",BIT_DIGITAL,	DrvJoy2 + 3,	"mah ff"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Kiwame)

static struct BurnInputInfo SokonukeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Sokonuke)

static struct BurnInputInfo NeobattlInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Neobattl)

static struct BurnInputInfo UmanclubInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 3,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Umanclub)

static struct BurnInputInfo TwineaglInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 5,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Twineagl)

static struct BurnInputInfo CrazyfgtInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 top-left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"	},
	{"P1 top-center",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},
	{"P1 top-right",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 3"	},
	{"P1 bottom-left",	BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 4"	},
	{"P1 bottom-center",BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 5"	},
	{"P1 bottom-right",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 6"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Crazyfgt)

static struct BurnInputInfo Calibr50InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3 (rotate)", BIT_DIGITAL,	DrvFakeInput + 4,	"p1 fire 3"	},
	A("P1 Aim X", 		BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Aim Y", 		BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3 (rotate)", BIT_DIGITAL,	DrvFakeInput + 5,	"p2 fire 3"	},
	A("P2 Aim X", 		BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	A("P2 Aim Y", 		BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 5,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	// Auto-fire on right-stick
	{"Dip D", 			BIT_DIPSWITCH, 	DrvDips + 3, 	"dip"       },
};

STDINPUTINFO(Calibr50)

static struct BurnInputInfo UsclssicInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 14,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 13,	"p1 fire 1"	},

	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 14,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy5 + 13,	"p2 fire 1"	},

	A("P2 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis"),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 7,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Usclssic)

static struct BurnInputInfo JockeycInputList[] = {
	{"P1 A Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"P1 B Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p3 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 start"	},
	{"P1 Bet 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 bet 1"	},
	{"P1 Bet 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 bet 2"	},
	{"P1 Bet 3",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 bet 3"	},
	{"P1 Bet 4",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 bet 4"	},
	{"P1 Bet 5",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 bet 5"	},
	{"P1 Bet 6",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 bet 6"	},
	{"P1 Bet 1-2",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 bet 1-2"	},
	{"P1 Bet 1-3",		BIT_DIGITAL,	DrvJoy5 + 1,	"p1 bet 1-3"	},
	{"P1 Bet 1-4",		BIT_DIGITAL,	DrvJoy5 + 2,	"p1 bet 1-4"	},
	{"P1 Bet 1-5",		BIT_DIGITAL,	DrvJoy5 + 3,	"p1 bet 1-5"	},
	{"P1 Bet 1-6",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 bet 1-6"	},
	{"P1 Bet 2-3",		BIT_DIGITAL,	DrvJoy6 + 0,	"p1 bet 2-3"	},
	{"P1 Bet 2-4",		BIT_DIGITAL,	DrvJoy6 + 1,	"p1 bet 2-4"	},
	{"P1 Bet 2-5",		BIT_DIGITAL,	DrvJoy6 + 2,	"p1 bet 2-5"	},
	{"P1 Bet 2-6",		BIT_DIGITAL,	DrvJoy6 + 3,	"p1 bet 2-6"	},
	{"P1 Bet 3-4",		BIT_DIGITAL,	DrvJoy6 + 4,	"p1 bet 3-4"	},
	{"P1 Bet 3-5",		BIT_DIGITAL,	DrvJoy7 + 0,	"p1 bet 3-5"	},
	{"P1 Bet 3-6",		BIT_DIGITAL,	DrvJoy7 + 1,	"p1 bet 3-6"	},
	{"P1 Bet 4-5",		BIT_DIGITAL,	DrvJoy7 + 2,	"p1 bet 4-5"	},
	{"P1 Bet 4-6",		BIT_DIGITAL,	DrvJoy7 + 3,	"p1 bet 4-6"	},
	{"P1 Bet 5-6",		BIT_DIGITAL,	DrvJoy7 + 4,	"p1 bet 5-6"	},
	{"P1 Collect",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 collect"	},
	{"P1 Credit",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 credit"	},
	{"P1 Cancel",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 cancel"	},

	{"P2 A Coin",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 coin"	},
	{"P2 B Coin",		BIT_DIGITAL,	DrvJoy1 + 12,	"p4 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 11,	"p2 start"	},
	{"P2 Bet 1",		BIT_DIGITAL,	DrvJoy3 + 8,	"p2 bet 1"	},
	{"P2 Bet 2",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 bet 2"	},
	{"P2 Bet 3",		BIT_DIGITAL,	DrvJoy3 + 10,	"p2 bet 3"	},
	{"P2 Bet 4",		BIT_DIGITAL,	DrvJoy3 + 11,	"p2 bet 4"	},
	{"P2 Bet 5",		BIT_DIGITAL,	DrvJoy3 + 12,	"p2 bet 5"	},
	{"P2 Bet 6",		BIT_DIGITAL,	DrvJoy4 + 8,	"p2 bet 6"	},
	{"P2 Bet 1-2",		BIT_DIGITAL,	DrvJoy5 + 8,	"p2 bet 1-2"	},
	{"P2 Bet 1-3",		BIT_DIGITAL,	DrvJoy5 + 9,	"p2 bet 1-3"	},
	{"P2 Bet 1-4",		BIT_DIGITAL,	DrvJoy5 + 10,	"p2 bet 1-4"	},
	{"P2 Bet 1-5",		BIT_DIGITAL,	DrvJoy5 + 11,	"p2 bet 1-5"	},
	{"P2 Bet 1-6",		BIT_DIGITAL,	DrvJoy5 + 12,	"p2 bet 1-6"	},
	{"P2 Bet 2-3",		BIT_DIGITAL,	DrvJoy6 + 8,	"p2 bet 2-3"	},
	{"P2 Bet 2-4",		BIT_DIGITAL,	DrvJoy6 + 9,	"p2 bet 2-4"	},
	{"P2 Bet 2-5",		BIT_DIGITAL,	DrvJoy6 + 10,	"p2 bet 2-5"	},
	{"P2 Bet 2-6",		BIT_DIGITAL,	DrvJoy6 + 11,	"p2 bet 2-6"	},
	{"P2 Bet 3-4",		BIT_DIGITAL,	DrvJoy6 + 12,	"p2 bet 3-4"	},
	{"P2 Bet 3-5",		BIT_DIGITAL,	DrvJoy7 + 8,	"p2 bet 3-5"	},
	{"P2 Bet 3-6",		BIT_DIGITAL,	DrvJoy7 + 9,	"p2 bet 3-6"	},
	{"P2 Bet 4-5",		BIT_DIGITAL,	DrvJoy7 + 10,	"p2 bet 4-5"	},
	{"P2 Bet 4-6",		BIT_DIGITAL,	DrvJoy7 + 11,	"p2 bet 4-6"	},
	{"P2 Bet 5-6",		BIT_DIGITAL,	DrvJoy7 + 12,	"p2 bet 5-6"	},
	{"P2 Collect",		BIT_DIGITAL,	DrvJoy4 + 9,	"p2 collect"	},
	{"P2 Credit",		BIT_DIGITAL,	DrvJoy4 + 10,	"p2 credit"	},
	{"P2 Cancel",		BIT_DIGITAL,	DrvJoy4 + 12,	"p2 cancel"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 14,	"service"	},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip E",		BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
	{"Dip F",		BIT_DIPSWITCH,	DrvDips + 5,	"dip"		},
	{"Dip 10",		BIT_DIPSWITCH,	DrvDips + 6,	"dip"		},
};

STDINPUTINFO(Jockeyc)

#undef A

static struct BurnDIPInfo CrazyfgtDIPList[]=
{
	{0x09, 0xff, 0xff, 0x3f, NULL			},
	{0x0a, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x09, 0x01, 0x07, 0x00, "4 Coins 1 Credits"	},
	{0x09, 0x01, 0x07, 0x01, "3 Coins 1 Credits"	},
	{0x09, 0x01, 0x07, 0x02, "2 Coins 1 Credits"	},
	{0x09, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x09, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x09, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x09, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x09, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x09, 0x01, 0x38, 0x00, "4 Coins 1 Credits"	},
	{0x09, 0x01, 0x38, 0x08, "3 Coins 1 Credits"	},
	{0x09, 0x01, 0x38, 0x10, "2 Coins 1 Credits"	},
	{0x09, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x09, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x09, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x09, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x09, 0x01, 0x38, 0x18, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Unknown"		},
	{0x09, 0x01, 0xc0, 0xc0, "5"			},
	{0x09, 0x01, 0xc0, 0x80, "10"			},
	{0x09, 0x01, 0xc0, 0x40, "15"			},
	{0x09, 0x01, 0xc0, 0x00, "20"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0a, 0x01, 0x01, 0x01, "Off"			},
	{0x0a, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty?"		},
	{0x0a, 0x01, 0x0c, 0x0c, "0"			},
	{0x0a, 0x01, 0x0c, 0x08, "1"			},
	{0x0a, 0x01, 0x0c, 0x04, "2"			},
	{0x0a, 0x01, 0x0c, 0x00, "3"			},

	{0   , 0xfe, 0   ,    4, "Energy"		},
	{0x0a, 0x01, 0x30, 0x10, "24"			},
	{0x0a, 0x01, 0x30, 0x20, "32"			},
	{0x0a, 0x01, 0x30, 0x30, "48"			},
	{0x0a, 0x01, 0x30, 0x00, "100"			},

	{0   , 0xfe, 0   ,    4, "Bonus?"		},
	{0x0a, 0x01, 0xc0, 0xc0, "0"			},
	{0x0a, 0x01, 0xc0, 0x80, "1"			},
	{0x0a, 0x01, 0xc0, 0x40, "2"			},
	{0x0a, 0x01, 0xc0, 0x00, "3"			},
};

STDDIPINFO(Crazyfgt)


static struct BurnDIPInfo UsclssicDIPList[]=
{
	{0x0d, 0xff, 0xff, 0xff, NULL			},
	{0x0e, 0xff, 0xff, 0xfe, NULL			},

	{0   , 0xfe, 0   ,    2, "Credits For 9-Hole"	},
	{0x0d, 0x01, 0x01, 0x01, "2"			},
	{0x0d, 0x01, 0x01, 0x00, "3"			},

	{0   , 0xfe, 0   ,    2, "Game Type"		},
	{0x0d, 0x01, 0x02, 0x02, "Domestic"		},
	{0x0d, 0x01, 0x02, 0x00, "Foreign"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0d, 0x01, 0x0c, 0x04, "1"			},
	{0x0d, 0x01, 0x0c, 0x08, "2"			},
	{0x0d, 0x01, 0x0c, 0x0c, "3"			},
	{0x0d, 0x01, 0x0c, 0x00, "4"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0d, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x0d, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0d, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x0d, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x01, 0x00, "Upright"		},
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0e, 0x01, 0x02, 0x02, "Off"			},
	{0x0e, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0e, 0x01, 0x04, 0x04, "Off"			},
	{0x0e, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Flight Distance"	},
	{0x0e, 0x01, 0x38, 0x38, "Normal"		},
	{0x0e, 0x01, 0x38, 0x30, "-30 Yards"		},
	{0x0e, 0x01, 0x38, 0x28, "+10 Yards"		},
	{0x0e, 0x01, 0x38, 0x20, "+20 Yards"		},
	{0x0e, 0x01, 0x38, 0x18, "+30 Yards"		},
	{0x0e, 0x01, 0x38, 0x10, "+40 Yards"		},
	{0x0e, 0x01, 0x38, 0x08, "+50 Yards"		},
	{0x0e, 0x01, 0x38, 0x00, "+60 Yards"		},

	{0   , 0xfe, 0   ,    4, "Licensed To"		},
	{0x0e, 0x01, 0xc0, 0xc0, "Romstar"		},
	{0x0e, 0x01, 0xc0, 0x80, "None (Japan)"		},
	{0x0e, 0x01, 0xc0, 0x40, "Taito"		},
	{0x0e, 0x01, 0xc0, 0x00, "Taito America"	},
};

STDDIPINFO(Usclssic)


static struct BurnDIPInfo Calibr50DIPList[]=
{
	DIP_OFFSET(0x19)

	{0x00, 0xff, 0xff, 0xfe, NULL			},
	{0x01, 0xff, 0xff, 0xfd, NULL			},
	{0x02, 0xff, 0xff, 0xff, NULL			},
	{0x03, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Licensed To"		},
	{0x00, 0x01, 0x01, 0x01, "Romstar"		},
	{0x00, 0x01, 0x01, 0x00, "None (Japan)"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x00, 0x01, 0x02, 0x02, "Off"			},
	{0x00, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x04, 0x04, "Off"			},
	{0x00, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x00, 0x01, 0x08, 0x00, "Off"			},
	{0x00, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x00, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x00, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x01, 0x01, 0x03, 0x03, "Easiest"		},
	{0x01, 0x01, 0x03, 0x02, "Easy"			},
	{0x01, 0x01, 0x03, 0x01, "Normal"		},
	{0x01, 0x01, 0x03, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Score Digits"		},
	{0x01, 0x01, 0x04, 0x04, "7"			},
	{0x01, 0x01, 0x04, 0x00, "3"			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x01, 0x01, 0x08, 0x08, "3"			},
	{0x01, 0x01, 0x08, 0x00, "4"			},

	{0   , 0xfe, 0   ,    2, "Display Score"	},
	{0x01, 0x01, 0x10, 0x00, "Off"			},
	{0x01, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Erase Backup Ram"	},
	{0x01, 0x01, 0x20, 0x00, "Off"			},
	{0x01, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Licensed To"		},
	{0x01, 0x01, 0x40, 0x40, "Taito America"	},
	{0x01, 0x01, 0x40, 0x00, "Taito"		},

	// Dip D
	{0   , 0xfe, 0   , 2   , "Second Stick"           },
	{0x03, 0x01, 0x01, 0x00, "Moves & Shoots"         },
	{0x03, 0x01, 0x01, 0x01, "Moves"                  },
};

STDDIPINFO(Calibr50)

static struct BurnDIPInfo TwineaglDIPList[]=
{
	{0x13, 0xff, 0xff, 0xf7, NULL					},
	{0x14, 0xff, 0xff, 0xf3, NULL					},
	{0x15, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Copyright"				},
	{0x13, 0x01, 0x01, 0x01, "Taito"				},
	{0x13, 0x01, 0x01, 0x00, "Taito America (Romstar license)"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x13, 0x01, 0x02, 0x02, "Off"					},
	{0x13, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x13, 0x01, 0x04, 0x04, "Off"					},
	{0x13, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x13, 0x01, 0x08, 0x00, "Upright"				},
	{0x13, 0x01, 0x08, 0x08, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"			},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"			},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"			},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"			},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"			},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"			},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x14, 0x01, 0x03, 0x03, "Normal"				},
	{0x14, 0x01, 0x03, 0x02, "Easy"					},
	{0x14, 0x01, 0x03, 0x01, "Hard"					},
	{0x14, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x14, 0x01, 0x0c, 0x0c, "Never"				},
	{0x14, 0x01, 0x0c, 0x08, "500K Only"				},
	{0x14, 0x01, 0x0c, 0x04, "1000K Only"				},
	{0x14, 0x01, 0x0c, 0x00, "500K, Every 1500K"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x14, 0x01, 0x30, 0x10, "1"					},
	{0x14, 0x01, 0x30, 0x00, "2"					},
	{0x14, 0x01, 0x30, 0x30, "3"					},
	{0x14, 0x01, 0x30, 0x20, "5"					},

	{0   , 0xfe, 0   ,    2, "Copyright"				},
	{0x14, 0x01, 0x40, 0x40, "Seta (Taito license)"			},
	{0x14, 0x01, 0x40, 0x40, "Taito America"			},

	{0   , 0xfe, 0   ,    2, "Coinage Type"				},
	{0x14, 0x01, 0x80, 0x80, "1"					},
	{0x14, 0x01, 0x80, 0x00, "2"					},
};

STDDIPINFO(Twineagl)

static struct BurnDIPInfo KiwameDIPList[]=
{
	{0x19, 0xff, 0xff, 0xff, NULL			},
	{0x1a, 0xff, 0xff, 0xff, NULL			},
	{0x1b, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x19, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x19, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x19, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x19, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x19, 0x01, 0x07, 0x03, "2 Coins 3 Credits"	},
	{0x19, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x19, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x19, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x19, 0x01, 0x38, 0x08, "4 Coins 1 Credits"	},
	{0x19, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x19, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x19, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x19, 0x01, 0x38, 0x18, "2 Coins 3 Credits"	},
	{0x19, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x19, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x19, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x19, 0x01, 0x40, 0x00, "Off"			},
	{0x19, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Player's TSUMO"	},
	{0x19, 0x01, 0x80, 0x80, "Manual"		},
	{0x19, 0x01, 0x80, 0x00, "Auto"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x1a, 0x01, 0x01, 0x01, "Off"			},
	{0x1a, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x1a, 0x01, 0x02, 0x02, "Off"			},
	{0x1a, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x1a, 0x01, 0x1c, 0x1c, "None"			},
	{0x1a, 0x01, 0x1c, 0x18, "Prelim  1"		},
	{0x1a, 0x01, 0x1c, 0x14, "Prelim  2"		},
	{0x1a, 0x01, 0x1c, 0x10, "Final   1"		},
	{0x1a, 0x01, 0x1c, 0x0c, "Final   2"		},
	{0x1a, 0x01, 0x1c, 0x08, "Final   3"		},
	{0x1a, 0x01, 0x1c, 0x04, "Qrt Final"		},
	{0x1a, 0x01, 0x1c, 0x00, "SemiFinal"		},

	{0   , 0xfe, 0   ,    8, "Points Gap"		},
	{0x1a, 0x01, 0xe0, 0xe0, "None"		},
	{0x1a, 0x01, 0xe0, 0xc0, "+6000"		},
	{0x1a, 0x01, 0xe0, 0xa0, "+4000"		},
	{0x1a, 0x01, 0xe0, 0x80, "+2000"		},
	{0x1a, 0x01, 0xe0, 0x60, "-2000"		},
	{0x1a, 0x01, 0xe0, 0x40, "-4000"		},
	{0x1a, 0x01, 0xe0, 0x20, "-6000"		},
	{0x1a, 0x01, 0xe0, 0x00, "-8000"		},
};

STDDIPINFO(Kiwame)

static struct BurnDIPInfo MetafoxDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xb1, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Licensed To"		},
	{0x13, 0x01, 0x01, 0x01, "Jordan"		},
	{0x13, 0x01, 0x01, 0x00, "Taito America"	},

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
	{0x14, 0x01, 0x03, 0x03, "Normal"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x0c, "None"			},
	{0x14, 0x01, 0x0c, 0x08, "60K Only"		},
	{0x14, 0x01, 0x0c, 0x00, "60k & 90k"		},
	{0x14, 0x01, 0x0c, 0x04, "90K Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x10, "1"			},
	{0x14, 0x01, 0x30, 0x00, "2"			},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "Licensed To"		},
	{0x14, 0x01, 0x40, 0x40, "Romstar"		},
	{0x14, 0x01, 0x40, 0x00, "Taito"		},

	{0   , 0xfe, 0   ,    2, "Coinage Type"		},
	{0x14, 0x01, 0x80, 0x80, "1"			},
	{0x14, 0x01, 0x80, 0x00, "2"			},
};

STDDIPINFO(Metafox)

static struct BurnDIPInfo ArbalestDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xbf, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Licensed To"		},
	{0x13, 0x01, 0x01, 0x00, "Taito"		},
	{0x13, 0x01, 0x01, 0x01, "Jordan"		},

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
	{0x14, 0x01, 0x03, 0x03, "Easy"			},
	{0x14, 0x01, 0x03, 0x02, "Hard"			},
	{0x14, 0x01, 0x03, 0x01, "Harder"		},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x0c, "Never"		},
	{0x14, 0x01, 0x0c, 0x08, "300k Only"		},
	{0x14, 0x01, 0x0c, 0x04, "600k Only"		},
	{0x14, 0x01, 0x0c, 0x00, "300k & 600k"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x10, "1"			},
	{0x14, 0x01, 0x30, 0x00, "2"			},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    1, "Licensed To"		},
	{0x14, 0x01, 0x40, 0x40, "Romstar"		},

	{0   , 0xfe, 0   ,    2, "Coinage Type"		},
	{0x14, 0x01, 0x80, 0x80, "1"			},
	{0x14, 0x01, 0x80, 0x00, "2"			},
};

STDDIPINFO(Arbalest)

static struct BurnDIPInfo DowntownDIPList[]=
{
	DIP_OFFSET(0x1d)

	{0x00, 0xff, 0xff, 0xf6, NULL			},
	{0x01, 0xff, 0xff, 0xbd, NULL			},
	{0x02, 0xff, 0xff, 0xff, NULL			},
	{0x03, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Sales"		},
	{0x00, 0x01, 0x01, 0x01, "Japan Only"		},
	{0x00, 0x01, 0x01, 0x00, "World"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x00, 0x01, 0x02, 0x02, "Off"			},
	{0x00, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x04, 0x04, "Off"			},
	{0x00, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x00, 0x01, 0x08, 0x08, "Off"			},
	{0x00, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x00, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x00, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x01, 0x01, 0x03, 0x02, "Easy"			},
	{0x01, 0x01, 0x03, 0x03, "Normal"		},
	{0x01, 0x01, 0x03, 0x01, "Hard"			},
	{0x01, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x01, 0x01, 0x0c, 0x0c, "Never"		},
	{0x01, 0x01, 0x0c, 0x08, "50K Only"		},
	{0x01, 0x01, 0x0c, 0x04, "100K Only"		},
	{0x01, 0x01, 0x0c, 0x00, "50K, Every 150K"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x01, 0x01, 0x30, 0x10, "2"			},
	{0x01, 0x01, 0x30, 0x30, "3"			},
	{0x01, 0x01, 0x30, 0x00, "4"			},
	{0x01, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "World License"	},
	{0x01, 0x01, 0x40, 0x40, "Romstar"		},
	{0x01, 0x01, 0x40, 0x00, "Taito"		},

	{0   , 0xfe, 0   ,    2, "Coinage Type"		},
	{0x01, 0x01, 0x80, 0x80, "1"			},
	{0x01, 0x01, 0x80, 0x00, "2"			},

	// Dip D
	{0   , 0xfe, 0   , 2   , "Second Stick"           },
	{0x03, 0x01, 0x01, 0x00, "Moves & Shoots"         },
	{0x03, 0x01, 0x01, 0x01, "Moves"                  },
};

STDDIPINFO(Downtown)

static struct BurnDIPInfo TndrcadeDIPList[]=
{
	{0x13, 0xff, 0xff, 0x7f, NULL			},
	{0x14, 0xff, 0xff, 0xf7, NULL			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x03, 0x02, "Easy"			},
	{0x13, 0x01, 0x03, 0x03, "Normal"		},
	{0x13, 0x01, 0x03, 0x01, "Hard"			},
	{0x13, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x0c, 0x0c, "50K  Only"		},
	{0x13, 0x01, 0x0c, 0x04, "50K, Every 150K"	},
	{0x13, 0x01, 0x0c, 0x00, "70K, Every 200K"	},
	{0x13, 0x01, 0x0c, 0x08, "100K Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x30, 0x10, "1"			},
	{0x13, 0x01, 0x30, 0x00, "2"			},
	{0x13, 0x01, 0x30, 0x30, "3"			},
	{0x13, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Licensed To"		},
	{0x13, 0x01, 0x80, 0x80, "Taito America Corp."	},
	{0x13, 0x01, 0x80, 0x00, "Taito Corp. Japan"	},

	{0   , 0xfe, 0   ,    2, "Title"		},
	{0x14, 0x01, 0x01, 0x01, "Thundercade"		},
	{0x14, 0x01, 0x01, 0x00, "Twin Formation"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x02, 0x02, "Off"			},
	{0x14, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x04, 0x04, "Off"			},
	{0x14, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x08, 0x08, "Off"			},
	{0x14, 0x01, 0x08, 0x00, "On"			},

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
};

STDDIPINFO(Tndrcade)

static struct BurnDIPInfo TndrcadjDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xf7, NULL			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x03, 0x02, "Easy"			},
	{0x13, 0x01, 0x03, 0x03, "Normal"		},
	{0x13, 0x01, 0x03, 0x01, "Hard"			},
	{0x13, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x0c, 0x0c, "50K  Only"		},
	{0x13, 0x01, 0x0c, 0x04, "50K, Every 150K"	},
	{0x13, 0x01, 0x0c, 0x00, "70K, Every 200K"	},
	{0x13, 0x01, 0x0c, 0x08, "100K Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x30, 0x10, "1"			},
	{0x13, 0x01, 0x30, 0x00, "2"			},
	{0x13, 0x01, 0x30, 0x30, "3"			},
	{0x13, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x02, 0x02, "Off"			},
	{0x14, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x04, 0x04, "Off"			},
	{0x14, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x08, 0x08, "Off"			},
	{0x14, 0x01, 0x08, 0x00, "On"			},

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
};

STDDIPINFO(Tndrcadj)

static struct BurnDIPInfo UtoukondDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,   16, "Coin A"		},
	{0x14, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x14, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x14, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,   16, "Coin B"		},
	{0x14, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x00, "5 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x14, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x03, 0x00, "1"			},
	{0x15, 0x01, 0x03, 0x02, "2"			},
	{0x15, 0x01, 0x03, 0x03, "3"			},
	{0x15, 0x01, 0x03, 0x01, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x0c, 0x08, "Easy"			},
	{0x15, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x15, 0x01, 0x0c, 0x04, "Hard"			},
	{0x15, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x10, 0x10, "Off"			},
	{0x15, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x20, 0x00, "Off"			},
	{0x15, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x15, 0x01, 0x40, 0x40, "100k"			},
	{0x15, 0x01, 0x40, 0x00, "150k"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Utoukond)

static struct BurnDIPInfo KrzybowlDIPList[]=
{
	{0x18, 0xff, 0xff, 0xff, NULL			},
	{0x19, 0xff, 0xff, 0xff, NULL			},
	{0x1a, 0xff, 0xff, 0xff, NULL			},

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
	{0x18, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Frames"		},
	{0x18, 0x01, 0x10, 0x10, "10"			},
	{0x18, 0x01, 0x10, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x18, 0x01, 0x20, 0x20, "Upright"		},
	{0x18, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Controls"		},
	{0x18, 0x01, 0x40, 0x40, "Trackball"		},
	{0x18, 0x01, 0x40, 0x00, "Joystick"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x18, 0x01, 0x80, 0x80, "Off"			},
	{0x18, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x19, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x19, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x19, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x19, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x19, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x19, 0x01, 0x07, 0x02, "1 Coin  4 Credits"	},
	{0x19, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},
	{0x19, 0x01, 0x07, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x19, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x19, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x19, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x19, 0x01, 0x38, 0x20, "1 Coin  2 Credits"	},
	{0x19, 0x01, 0x38, 0x18, "1 Coin  3 Credits"	},
	{0x19, 0x01, 0x38, 0x10, "1 Coin  4 Credits"	},
	{0x19, 0x01, 0x38, 0x08, "1 Coin  5 Credits"	},
	{0x19, 0x01, 0x38, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Force Coinage"	},
	{0x19, 0x01, 0x40, 0x40, "No"			},
	{0x19, 0x01, 0x40, 0x00, "2 Coins 1 Credits"	},
};

STDDIPINFO(Krzybowl)

static struct BurnDIPInfo ExtdwnhlDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xff, NULL						},
	{0x0d, 0xff, 0xff, 0xff, NULL						},
	{0x0e, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x0c, 0x01, 0x01, 0x01, "Off"						},
	{0x0c, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x0c, 0x01, 0x02, 0x00, "Off"						},
	{0x0c, 0x01, 0x02, 0x02, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x0c, 0x01, 0x0c, 0x08, "Easy"						},
	{0x0c, 0x01, 0x0c, 0x0c, "Normal"					},
	{0x0c, 0x01, 0x0c, 0x04, "Hard"						},
	{0x0c, 0x01, 0x0c, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x0c, 0x01, 0x10, 0x10, "Off"						},
	{0x0c, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x0c, 0x01, 0x20, 0x20, "Upright"					},
	{0x0c, 0x01, 0x20, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Controls"					},
	{0x0c, 0x01, 0x40, 0x40, "2"						},
	{0x0c, 0x01, 0x40, 0x00, "1"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x0c, 0x01, 0x80, 0x80, "Off"						},
	{0x0c, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x0d, 0x01, 0x07, 0x05, "3 Coins 1 Credits"				},
	{0x0d, 0x01, 0x07, 0x06, "2 Coins 1 Credits"				},
	{0x0d, 0x01, 0x07, 0x07, "1 Coin  1 Credits"				},
	{0x0d, 0x01, 0x07, 0x04, "1 Coin  2 Credits"				},
	{0x0d, 0x01, 0x07, 0x03, "1 Coin  3 Credits"				},
	{0x0d, 0x01, 0x07, 0x02, "1 Coin  4 Credits"				},
	{0x0d, 0x01, 0x07, 0x01, "1 Coin  5 Credits"				},
	{0x0d, 0x01, 0x07, 0x00, "1 Coin  6 Credits"				},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x0d, 0x01, 0x38, 0x28, "3 Coins 1 Credits"				},
	{0x0d, 0x01, 0x38, 0x30, "2 Coins 1 Credits"				},
	{0x0d, 0x01, 0x38, 0x38, "1 Coin  1 Credits"				},
	{0x0d, 0x01, 0x38, 0x20, "1 Coin  2 Credits"				},
	{0x0d, 0x01, 0x38, 0x18, "1 Coin  3 Credits"				},
	{0x0d, 0x01, 0x38, 0x10, "1 Coin  4 Credits"				},
	{0x0d, 0x01, 0x38, 0x08, "1 Coin  5 Credits"				},
	{0x0d, 0x01, 0x38, 0x00, "1 Coin  6 Credits"				},

	{0   , 0xfe, 0   ,    2, "Continue Coin"				},
	{0x0d, 0x01, 0x40, 0x40, "Normal: Start 1C / Continue 1C"		},
	{0x0d, 0x01, 0x40, 0x00, "Half Continue: Start 2C / Continue 1C"	},

	{0   , 0xfe, 0   ,    2, "Game Mode"					},
	{0x0d, 0x01, 0x80, 0x80, "Finals Only"					},
	{0x0d, 0x01, 0x80, 0x00, "Semi-Finals & Finals"				},

	{0   , 0xfe, 0   ,    2, "Service Mode (No Toggle)"			},
	{0x0e, 0x01, 0x08, 0x08, "Off"						},
	{0x0e, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    3, "Country"					},
	{0x0e, 0x01, 0x30, 0x30, "World"					},
	{0x0e, 0x01, 0x30, 0x10, "USA"						},
	{0x0e, 0x01, 0x30, 0x00, "Japan"					},
};

STDDIPINFO(Extdwnhl)

static struct BurnDIPInfo SokonukeDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL			},
	{0x11, 0xff, 0xff, 0xff, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x10, 0x01, 0x01, 0x01, "Off"			},
	{0x10, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x10, 0x01, 0x02, 0x00, "Off"			},
	{0x10, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x10, 0x01, 0x0c, 0x08, "Easy"			},
	{0x10, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x10, 0x01, 0x0c, 0x04, "Hard"			},
	{0x10, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x10, 0x01, 0x10, 0x10, "Off"			},
	{0x10, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x80, 0x80, "Off"			},
	{0x10, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x11, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x07, 0x02, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},
	{0x11, 0x01, 0x07, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x11, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x38, 0x20, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x38, 0x18, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x38, 0x10, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x38, 0x08, "1 Coin  5 Credits"	},
	{0x11, 0x01, 0x38, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cheap Continue"	},
	{0x11, 0x01, 0x40, 0x40, "No"			},
	{0x11, 0x01, 0x40, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},
};

STDDIPINFO(Sokonuke)

static struct BurnDIPInfo JjsquawkDIPList[]=
{
	{0x12, 0xff, 0xff, 0xbf, NULL			},
	{0x13, 0xff, 0xff, 0xef, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x08, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x40, 0x40, "Off"			},
	{0x12, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x01, 0x01, "Off"			},
	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x08, "Easy"			},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x13, 0x01, 0x0c, 0x04, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Energy"		},
	{0x13, 0x01, 0x30, 0x20, "2"			},
	{0x13, 0x01, 0x30, 0x30, "3"			},
	{0x13, 0x01, 0x30, 0x10, "4"			},
	{0x13, 0x01, 0x30, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0xc0, 0x80, "20K, Every 100K"	},
	{0x13, 0x01, 0xc0, 0xc0, "50K, Every 200K"	},
	{0x13, 0x01, 0xc0, 0x40, "70K, 200K Only"	},
	{0x13, 0x01, 0xc0, 0x00, "100K Only"		},
};

STDDIPINFO(Jjsquawk)

static struct BurnDIPInfo KeroppiDIPList[]=
{
	{0x04, 0xff, 0xff, 0xff, NULL			},
	{0x05, 0xff, 0xff, 0xbf, NULL			},
	{0x06, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x04, 0x01, 0x07, 0x03, "1"			},
	{0x04, 0x01, 0x07, 0x05, "2"			},
	{0x04, 0x01, 0x07, 0x01, "3"			},
	{0x04, 0x01, 0x07, 0x07, "4"			},
	{0x04, 0x01, 0x07, 0x06, "5"			},
	{0x04, 0x01, 0x07, 0x02, "6"			},
	{0x04, 0x01, 0x07, 0x04, "7"			},
	{0x04, 0x01, 0x07, 0x00, "8"			},

	{0   , 0xfe, 0   ,    8, "Game Select"		},
	{0x04, 0x01, 0x38, 0x38, "No. 1,2,3"		},
	{0x04, 0x01, 0x38, 0x30, "No. 1"		},
	{0x04, 0x01, 0x38, 0x28, "No. 2,3"		},
	{0x04, 0x01, 0x38, 0x20, "No. 3"		},
	{0x04, 0x01, 0x38, 0x18, "No. 1,2"		},
	{0x04, 0x01, 0x38, 0x10, "No. 2"		},
	{0x04, 0x01, 0x38, 0x08, "No. 1,3"		},
	{0x04, 0x01, 0x38, 0x00, "No. 1,2,3"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x05, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x05, 0x01, 0x07, 0x04, "4 Coins 1 Credits"	},
	{0x05, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x05, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x05, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x05, 0x01, 0x07, 0x03, "1 Coin  2 Credits"	},
	{0x05, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x05, 0x01, 0x07, 0x01, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x05, 0x01, 0x20, 0x20, "Off"			},
	{0x05, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x05, 0x01, 0x40, 0x40, "Off"			},
	{0x05, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x05, 0x01, 0x80, 0x80, "Off"			},
	{0x05, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Keroppi)

static struct BurnDIPInfo OrbsDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL			},
	{0x11, 0xff, 0xff, 0xff, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x10, 0x01, 0x01, 0x01, "Off"			},
	{0x10, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x10, 0x01, 0x02, 0x00, "Off"			},
	{0x10, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x10, 0x01, 0x0c, 0x08, "Easy"			},
	{0x10, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x10, 0x01, 0x0c, 0x04, "Hard"			},
	{0x10, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Stock"		},
	{0x10, 0x01, 0x10, 0x10, "1"			},
	{0x10, 0x01, 0x10, 0x00, "2"			},

	{0   , 0xfe, 0   ,    2, "Level Select"		},
	{0x10, 0x01, 0x20, 0x20, "Off"			},
	{0x10, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Timer speed"		},
	{0x10, 0x01, 0x40, 0x40, "Normal"		},
	{0x10, 0x01, 0x40, 0x00, "Fast"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x80, 0x80, "Off"			},
	{0x10, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x11, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x07, 0x02, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},
	{0x11, 0x01, 0x07, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x11, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x38, 0x20, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x38, 0x18, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x38, 0x10, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x38, 0x08, "1 Coin  5 Credits"	},
	{0x11, 0x01, 0x38, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Force Coinage (Half)"	},
	{0x11, 0x01, 0x40, 0x40, "No"			},
	{0x11, 0x01, 0x40, 0x00, "2 Coins 1 Credits"	},
};

STDDIPINFO(Orbs)

static struct BurnDIPInfo PairloveDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfd, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x01, 0x01, "Off"			},
	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x0c, 0x08, "Easy"			},
	{0x14, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x14, 0x01, 0x0c, 0x04, "Hard"			},
	{0x14, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,   16, "Coinage"		},
	{0x14, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x10, "8 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x20, "5 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0xf0, 0x00, "Free Play"		},
};

STDDIPINFO(Pairlove)

static struct BurnDIPInfo OisipuzlDIPList[]=
{
	{0x12, 0xff, 0xff, 0xfb, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x03, 0x02, "Easy"			},
	{0x12, 0x01, 0x03, 0x03, "Normal"		},
	{0x12, 0x01, 0x03, 0x01, "Hard"			},
	{0x12, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x04, 0x04, "Off"			},
	{0x12, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x38, 0x00, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x08, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x18, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
};

STDDIPINFO(Oisipuzl)

static struct BurnDIPInfo BlandiaDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},
	{0x17, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x01, 0x00, "Off"			},
	{0x15, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Coinage Type"		},
	{0x15, 0x01, 0x02, 0x02, "1"			},
	{0x15, 0x01, 0x02, 0x00, "2"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x15, 0x01, 0x1c, 0x10, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x1c, 0x0c, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x1c, 0x04, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x1c, 0x18, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x1c, 0x08, "2 Coins 5 Credits"	},
	{0x15, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x1c, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x15, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0xe0, 0x60, "2 Coins 4 Credits"	},
	{0x15, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0xe0, 0x80, "3 Coins/7 Credits"	},
	{0x15, 0x01, 0xe0, 0x20, "2 Coins 5 Credits"	},
	{0x15, 0x01, 0xe0, 0x40, "2 Coins 6 Credits"	},
	{0x15, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0xe0, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x16, 0x01, 0x03, 0x02, "1"			},
	{0x16, 0x01, 0x03, 0x03, "2"			},
	{0x16, 0x01, 0x03, 0x01, "3"			},
	{0x16, 0x01, 0x03, 0x00, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x16, 0x01, 0x0c, 0x08, "Easy"			},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x16, 0x01, 0x0c, 0x04, "Hard"			},
	{0x16, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "2 Player Game"	},
	{0x16, 0x01, 0x10, 0x10, "2 Credits"		},
	{0x16, 0x01, 0x10, 0x00, "1 Credit"		},

	{0   , 0xfe, 0   ,    2, "Continue"		},
	{0x16, 0x01, 0x20, 0x20, "1 Credit"		},
	{0x16, 0x01, 0x20, 0x00, "1 Coin"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x40, 0x40, "Off"			},
	{0x16, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x80, 0x80, "Off"			},
	{0x16, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Blandia)

static struct BurnDIPInfo GundharaDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x14, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x14, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x14, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0x30, 0x00, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x14, 0x01, 0xc0, 0xc0, "Japanese"		},
	{0x14, 0x01, 0xc0, 0x00, "English"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x02, "Easy"			},
	{0x15, 0x01, 0x03, 0x03, "Normal"		},
	{0x15, 0x01, 0x03, 0x01, "Hard"			},
	{0x15, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x0c, 0x08, "1"			},
	{0x15, 0x01, 0x0c, 0x0c, "2"			},
	{0x15, 0x01, 0x0c, 0x04, "3"			},
	{0x15, 0x01, 0x0c, 0x00, "4"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x15, 0x01, 0x30, 0x30, "200K"			},
	{0x15, 0x01, 0x30, 0x20, "200K, Every 200K"	},
	{0x15, 0x01, 0x30, 0x10, "400K"			},
	{0x15, 0x01, 0x30, 0x00, "None"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x08, 0x08, "Off"			},
	{0x16, 0x01, 0x08, 0x00, "On"			},
};

STDDIPINFO(Gundhara)

static struct BurnDIPInfo AtehateDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL			},
	{0x0f, 0xff, 0xff, 0xff, NULL			},
	{0x10, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x0e, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x0e, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x07, 0x03, "2 Coins 3 Credits"	},
	{0x0e, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x0e, 0x01, 0x38, 0x08, "4 Coins 1 Credits"	},
	{0x0e, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x38, 0x18, "2 Coins 3 Credits"	},
	{0x0e, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0e, 0x01, 0x40, 0x00, "Off"			},
	{0x0e, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0f, 0x01, 0x01, 0x01, "Off"			},
	{0x0f, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0f, 0x01, 0x02, 0x02, "Off"			},
	{0x0f, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0f, 0x01, 0x0c, 0x08, "Easy"			},
	{0x0f, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x0f, 0x01, 0x0c, 0x04, "Hard"			},
	{0x0f, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x30, 0x00, "2"			},
	{0x0f, 0x01, 0x30, 0x30, "3"			},
	{0x0f, 0x01, 0x30, 0x10, "4"			},
	{0x0f, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0f, 0x01, 0xc0, 0xc0, "None"			},
	{0x0f, 0x01, 0xc0, 0x00, "20K Only"		},
	{0x0f, 0x01, 0xc0, 0x80, "20K, Every 30K"	},
	{0x0f, 0x01, 0xc0, 0x40, "30K, Every 40K"	},
};

STDDIPINFO(Atehate)

static struct BurnDIPInfo ThunderlDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xe9, NULL			},
	{0x15, 0xff, 0xff, 0xef, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x13, 0x01, 0x0f, 0x0c, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x0d, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x08, "4 Coins 2 Credits"	},
	{0x13, 0x01, 0x0f, 0x0e, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x09, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0x0f, 0x04, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x00, "4 Coins 4 Credits"	},
	{0x13, 0x01, 0x0f, 0x05, "3 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0a, "2 Coins 2 Credits"	},
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x0f, 0x01, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x02, "2 Coins 4 Credits"	},
	{0x13, 0x01, 0x0f, 0x0b, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0f, 0x07, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x13, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x80, "4 Coins 2 Credits"	},
	{0x13, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x90, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xf0, 0x40, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0x00, "4 Coins 4 Credits"	},
	{0x13, 0x01, 0xf0, 0x50, "3 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0xa0, "2 Coins 2 Credits"	},
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xf0, 0x10, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0x20, "2 Coins 4 Credits"	},
	{0x13, 0x01, 0xf0, 0xb0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xf0, 0x70, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x02, 0x00, "Off"			},
	{0x14, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x04, 0x00, "Upright"		},
	{0x14, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Controls"		},
	{0x14, 0x01, 0x08, 0x08, "2"			},
	{0x14, 0x01, 0x08, 0x00, "1"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x10, 0x10, "Off"			},
	{0x14, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x14, 0x01, 0x20, 0x20, "3"			},
	{0x14, 0x01, 0x20, 0x00, "2"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0xc0, 0x80, "Easy"			},
	{0x14, 0x01, 0xc0, 0xc0, "Normal"		},
	{0x14, 0x01, 0xc0, 0x40, "Hard"			},
	{0x14, 0x01, 0xc0, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Force 1 Life"		},
	{0x15, 0x01, 0x10, 0x00, "Off"			},
	{0x15, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    5, "Copyright"		},
	{0x15, 0x01, 0xe0, 0x80, "Romstar"		},
	{0x15, 0x01, 0xe0, 0xc0, "Seta (Romstar License)"},
	{0x15, 0x01, 0xe0, 0xe0, "Seta (Visco License)"	},
	{0x15, 0x01, 0xe0, 0xa0, "Visco"		},
	{0x15, 0x01, 0xe0, 0x60, "None"			},
};

STDDIPINFO(Thunderl)

static struct BurnDIPInfo WitsDIPList[]=
{
	{0x21, 0xff, 0xff, 0xff, NULL					},
	{0x22, 0xff, 0xff, 0xff, NULL					},
	{0x23, 0xff, 0xff, 0x7f, NULL					},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x21, 0x01, 0x03, 0x02, "Easy"					},
	{0x21, 0x01, 0x03, 0x03, "Normal"				},
	{0x21, 0x01, 0x03, 0x01, "Hard"					},
	{0x21, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x21, 0x01, 0x0c, 0x08, "150k, 350k"				},
	{0x21, 0x01, 0x0c, 0x0c, "200k, 500k"				},
	{0x21, 0x01, 0x0c, 0x04, "300k, 600k"				},
	{0x21, 0x01, 0x0c, 0x00, "400k"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x21, 0x01, 0x30, 0x00, "1"					},
	{0x21, 0x01, 0x30, 0x10, "2"					},
	{0x21, 0x01, 0x30, 0x30, "3"					},
	{0x21, 0x01, 0x30, 0x20, "5"					},

	{0   , 0xfe, 0   ,    2, "Play Mode"				},
	{0x21, 0x01, 0x40, 0x40, "2 Players"				},
	{0x21, 0x01, 0x40, 0x00, "4 Players"				},

	{0   , 0xfe, 0   ,    2, "CPU Player During Multi-Player Game"	},
	{0x21, 0x01, 0x80, 0x00, "No"					},
	{0x21, 0x01, 0x80, 0x80, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x22, 0x01, 0x01, 0x01, "Upright"				},
	{0x22, 0x01, 0x01, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x22, 0x01, 0x02, 0x02, "Off"					},
	{0x22, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x22, 0x01, 0x04, 0x04, "Every 3rd Loop"			},
	{0x22, 0x01, 0x04, 0x00, "Every 7th Loop"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x22, 0x01, 0x08, 0x08, "Off"					},
	{0x22, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x22, 0x01, 0x30, 0x00, "4 Coins 1 Credits"			},
	{0x22, 0x01, 0x30, 0x10, "3 Coins 1 Credits"			},
	{0x22, 0x01, 0x30, 0x20, "2 Coins 1 Credits"			},
	{0x22, 0x01, 0x30, 0x30, "1 Coin  1 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x22, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"			},
	{0x22, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"			},
	{0x22, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"			},
	{0x22, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"			},

	{0   , 0xfe, 0   ,    4, "License"				},
	{0x23, 0x01, 0xc0, 0xc0, "Romstar"				},
	{0x23, 0x01, 0xc0, 0x80, "Seta U.S.A"				},
	{0x23, 0x01, 0xc0, 0x40, "Visco (Japan Only)"			},
	{0x23, 0x01, 0xc0, 0x00, "Athena (Japan Only)"			},
};

STDDIPINFO(Wits)

static struct BurnDIPInfo MadsharkDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0x7f, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x01, 0x01, "Off"			},
	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x18, 0x18, "1000k"		},
	{0x13, 0x01, 0x18, 0x08, "1000k 2000k"		},
	{0x13, 0x01, 0x18, 0x10, "1500k 3000k"		},
	{0x13, 0x01, 0x18, 0x00, "No"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x60, 0x40, "Easy"			},
	{0x13, 0x01, 0x60, 0x60, "Normal"		},
	{0x13, 0x01, 0x60, 0x20, "Hard"			},
	{0x13, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x02, "2"			},
	{0x14, 0x01, 0x03, 0x03, "3"			},
	{0x14, 0x01, 0x03, 0x01, "4"			},
	{0x14, 0x01, 0x03, 0x00, "5"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x14, 0x01, 0x1c, 0x04, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x1c, 0x10, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x1c, 0x0c, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x1c, 0x18, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x1c, 0x00, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x14, 0x01, 0xe0, 0x20, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0xe0, 0x80, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xe0, 0x60, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xe0, 0x00, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Country"		},
	{0x15, 0x01, 0x80, 0x80, "Japan"		},
	{0x15, 0x01, 0x80, 0x00, "World"		},
};

STDDIPINFO(Madshark)

static struct BurnDIPInfo MsgundamDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x07, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x01, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x02, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x38, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x01, 0x00, "Off"			},
	{0x14, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x06, 0x04, "Easy"			},
	{0x14, 0x01, 0x06, 0x06, "Normal"		},
	{0x14, 0x01, 0x06, 0x02, "Hard"			},
	{0x14, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x10, 0x10, "Off"			},
	{0x14, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Memory Check"		},
	{0x14, 0x01, 0x20, 0x20, "Off"			},
	{0x14, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x15, 0x01, 0x80, 0x80, "English"		},
	{0x15, 0x01, 0x80, 0x00, "Japanese"		},
};

STDDIPINFO(Msgundam)

static struct BurnDIPInfo Msgunda1DIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0x7f, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x07, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x01, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x02, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x38, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x01, 0x00, "Off"			},
	{0x14, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x06, 0x04, "Easy"			},
	{0x14, 0x01, 0x06, 0x06, "Normal"		},
	{0x14, 0x01, 0x06, 0x02, "Hard"			},
	{0x14, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x10, 0x10, "Off"			},
	{0x14, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Memory Check"		},
	{0x14, 0x01, 0x20, 0x20, "Off"			},
	{0x14, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    1, "Language"		},
	{0x15, 0x01, 0x80, 0x00, "Japanese"		},
};

STDDIPINFO(Msgunda1)

static struct BurnDIPInfo ZingzipDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x02, 0x00, "Off"			},
	{0x12, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "4"			},
	{0x13, 0x01, 0x03, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x08, "Easy"			},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x13, 0x01, 0x0c, 0x04, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,   16, "Coinage"		},
	{0x13, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x10, "8 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x20, "5 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0xf0, 0x00, "Free Play"		},
};

STDDIPINFO(Zingzip)

static struct BurnDIPInfo WrofaeroDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},
	{0x17, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x01, 0x01, "Off"			},
	{0x15, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x02, 0x00, "Off"			},
	{0x15, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Stage & Weapon Select"},
	{0x15, 0x01, 0x08, 0x08, "Off"			},
	{0x15, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x16, 0x01, 0x03, 0x02, "2"			},
	{0x16, 0x01, 0x03, 0x03, "3"			},
	{0x16, 0x01, 0x03, 0x01, "4"			},
	{0x16, 0x01, 0x03, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x16, 0x01, 0x0c, 0x08, "Easy"			},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x16, 0x01, 0x0c, 0x04, "Hard"			},
	{0x16, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,   16, "Coinage"		},
	{0x16, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0x10, "8 Coins 3 Credits"	},
	{0x16, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0x20, "5 Coins 3 Credits"	},
	{0x16, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"	},
	{0x16, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"	},
	{0x16, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"	},
	{0x16, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"	},
	{0x16, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x16, 0x01, 0xf0, 0x00, "Free Play"		},
};

STDDIPINFO(Wrofaero)

static struct BurnDIPInfo EightfrcDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0x7b, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x07, 0x04, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x07, 0x01, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x38, 0x20, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x38, 0x08, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Shared Credits"	},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Credits To Start"	},
	{0x13, 0x01, 0x80, 0x80, "1"			},
	{0x13, 0x01, 0x80, 0x00, "2"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x02, 0x02, "Off"			},
	{0x14, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x04, 0x04, "Off"			},
	{0x14, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x18, 0x10, "Easy"			},
	{0x14, 0x01, 0x18, 0x18, "Normal"		},
	{0x14, 0x01, 0x18, 0x08, "Hard"			},
	{0x14, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x60, 0x40, "2"			},
	{0x14, 0x01, 0x60, 0x60, "3"			},
	{0x14, 0x01, 0x60, 0x20, "4"			},
	{0x14, 0x01, 0x60, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x14, 0x01, 0x80, 0x00, "English"		},
	{0x14, 0x01, 0x80, 0x80, "Japanese"		},
};

STDDIPINFO(Eightfrc)

static struct BurnDIPInfo RezonDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x02, 0x00, "Off"			},
	{0x14, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    3, "Cabinet"		},
	{0x14, 0x01, 0x18, 0x00, "Upright 1 Controller"	},
	{0x14, 0x01, 0x18, 0x18, "Upright 2 Controllers"},
	{0x14, 0x01, 0x18, 0x08, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x03, 0x02, "2"			},
	{0x15, 0x01, 0x03, 0x03, "3"			},
	{0x15, 0x01, 0x03, 0x01, "4"			},
	{0x15, 0x01, 0x03, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x0c, 0x08, "Easy"			},
	{0x15, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x15, 0x01, 0x0c, 0x04, "Hard"			},
	{0x15, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,   16, "Coinage"		},
	{0x15, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0x10, "8 Coins 3 Credits"	},
	{0x15, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0xf0, 0x20, "5 Coins 3 Credits"	},
	{0x15, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"	},
	{0x15, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x15, 0x01, 0xf0, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x08, 0x08, "Off"			},
	{0x16, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Taito Logo"		},
	{0x16, 0x01, 0x10, 0x00, "Off"			},
	{0x16, 0x01, 0x10, 0x10, "On"			},
};

STDDIPINFO(Rezon)

static struct BurnDIPInfo Qzkklgy2DIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL				},
	{0x0f, 0xff, 0xff, 0xff, NULL				},
	{0x10, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Highlight Right Answer"	},
	{0x0e, 0x01, 0x04, 0x04, "Off"				},
	{0x0e, 0x01, 0x04, 0x00, "On"				},
	
	{0   , 0xfe, 0   ,    2, "Skip Real DAT Rom Check?"	},
	{0x0e, 0x01, 0x08, 0x08, "Off"				},
	{0x0e, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x20, 0x20, "Off"				},
	{0x0f, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0e, 0x01, 0x40, 0x00, "Off"				},
	{0x0e, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0e, 0x01, 0x80, 0x80, "Off"				},
	{0x0e, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x0f, 0x01, 0x07, 0x04, "4 Coins 1 Credits"		},
	{0x0f, 0x01, 0x07, 0x05, "3 Coins 1 Credits"		},
	{0x0f, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x07, 0x00, "2 Coins 3 Credits"		},
	{0x0f, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x07, 0x02, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0x07, 0x01, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x0f, 0x01, 0x08, 0x08, "Off"				},
	{0x0f, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0f, 0x01, 0x30, 0x30, "Easy"				},
	{0x0f, 0x01, 0x30, 0x20, "Normal"			},
	{0x0f, 0x01, 0x30, 0x10, "Hard"				},
	{0x0f, 0x01, 0x30, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0f, 0x01, 0xc0, 0x80, "2"				},
	{0x0f, 0x01, 0xc0, 0xc0, "3"				},
	{0x0f, 0x01, 0xc0, 0x40, "4"				},
	{0x0f, 0x01, 0xc0, 0x00, "5"				},
};

STDDIPINFO(Qzkklgy2)

static struct BurnDIPInfo QzkklogyDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xff, NULL				},
	{0x10, 0xff, 0xff, 0xff, NULL				},
	{0x11, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Highlight Right Answer"	},
	{0x0f, 0x01, 0x04, 0x04, "Off"				},
	{0x0f, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x20, 0x20, "Off"				},
	{0x0f, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0f, 0x01, 0x40, 0x00, "Off"				},
	{0x0f, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0f, 0x01, 0x80, 0x80, "Off"				},
	{0x0f, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x10, 0x01, 0x07, 0x04, "4 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x05, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x07, 0x01, "2 Coins 3 Credits"		},
	{0x10, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x07, 0x02, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x10, 0x01, 0x30, 0x20, "Easy"				},
	{0x10, 0x01, 0x30, 0x30, "Normal"			},
	{0x10, 0x01, 0x30, 0x10, "Hard"				},
	{0x10, 0x01, 0x30, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x10, 0x01, 0xc0, 0x80, "2"				},
	{0x10, 0x01, 0xc0, 0xc0, "3"				},
	{0x10, 0x01, 0xc0, 0x40, "4"				},
	{0x10, 0x01, 0xc0, 0x00, "5"				},
};

STDDIPINFO(Qzkklogy)

static struct BurnDIPInfo StgDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x03, 0x02, "Easy"			},
	{0x13, 0x01, 0x03, 0x03, "Normal"		},
	{0x13, 0x01, 0x03, 0x01, "Hard"			},
	{0x13, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x30, 0x10, "1"			},
	{0x13, 0x01, 0x30, 0x00, "2"			},
	{0x13, 0x01, 0x30, 0x30, "3"			},
	{0x13, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    4, "Flip Screen"		},
	{0x14, 0x01, 0x02, 0x02, "Off"			},
	{0x14, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x04, 0x04, "Off"			},
	{0x14, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Stg)

static struct BurnDIPInfo DrgnunitDIPList[]=
{
	{0x15, 0xff, 0xff, 0xfe, NULL				},
	{0x16, 0xff, 0xff, 0xff, NULL				},
	{0x17, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x03, 0x03, "Easy"				},
	{0x15, 0x01, 0x03, 0x02, "Normal"			},
	{0x15, 0x01, 0x03, 0x01, "Hard"				},
	{0x15, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x15, 0x01, 0x0c, 0x08, "150K, Every 300K"		},
	{0x15, 0x01, 0x0c, 0x0c, "200K, Every 400K"		},
	{0x15, 0x01, 0x0c, 0x04, "300K, Every 500K"		},
	{0x15, 0x01, 0x0c, 0x00, "400K Only"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x15, 0x01, 0x30, 0x00, "1"				},
	{0x15, 0x01, 0x30, 0x10, "2"				},
	{0x15, 0x01, 0x30, 0x30, "3"				},
	{0x15, 0x01, 0x30, 0x20, "5"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x16, 0x01, 0x02, 0x02, "Off"				},
	{0x16, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x04, 0x04, "1 of 4 Scenes"		},
	{0x16, 0x01, 0x04, 0x00, "1 of 8 Scenes"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x16, 0x01, 0x08, 0x08, "Off"				},
	{0x16, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x16, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x16, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0x30, 0x00, "2 Coins 3 Credits"		},
	{0x16, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x16, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"		},
	{0x16, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"		},
	{0x16, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Coinage Type"			},
	{0x17, 0x01, 0x10, 0x10, "1"				},
	{0x17, 0x01, 0x10, 0x00, "2"				},

	{0   , 0xfe, 0   ,    2, "Title"			},
	{0x17, 0x01, 0x20, 0x20, "Dragon Unit"			},
	{0x17, 0x01, 0x20, 0x00, "Castle of Dragon"		},

	{0   , 0xfe, 0   ,    4, "(C) / License"		},
	{0x17, 0x01, 0xc0, 0xc0, "Athena (Japan)"		},
	{0x17, 0x01, 0xc0, 0x80, "Athena / Taito (Japan)"	},
	{0x17, 0x01, 0xc0, 0x40, "Seta USA / Taito America"	},
	{0x17, 0x01, 0xc0, 0x00, "Seta USA / Romstar"		},
};

STDDIPINFO(Drgnunit)

static struct BurnDIPInfo DaiohDIPList[]=
{
	{0x1a, 0xff, 0xff, 0x7f, NULL			},
	{0x1b, 0xff, 0xff, 0xff, NULL			},
	{0x1c, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x1a, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x1a, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x1a, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x1a, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x1a, 0x01, 0x07, 0x03, "2 Coins 3 Credits"	},
	{0x1a, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x1a, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x1a, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x1a, 0x01, 0x38, 0x08, "4 Coins 1 Credits"	},
	{0x1a, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x1a, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x1a, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x1a, 0x01, 0x38, 0x18, "2 Coins 3 Credits"	},
	{0x1a, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x1a, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x1a, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x1a, 0x01, 0x40, 0x00, "Off"			},
	{0x1a, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Auto Shot"		},
	{0x1a, 0x01, 0x80, 0x80, "Off"			},
	{0x1a, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x1b, 0x01, 0x01, 0x01, "Off"			},
	{0x1b, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x1b, 0x01, 0x02, 0x02, "Off"			},
	{0x1b, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x1b, 0x01, 0x0c, 0x08, "Easy"			},
	{0x1b, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x1b, 0x01, 0x0c, 0x04, "Hard"			},
	{0x1b, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x1b, 0x01, 0x30, 0x00, "1"			},
	{0x1b, 0x01, 0x30, 0x10, "2"			},
	{0x1b, 0x01, 0x30, 0x30, "3"			},
	{0x1b, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x1b, 0x01, 0xc0, 0x80, "300k and every 800k"	},
	{0x1b, 0x01, 0xc0, 0xc0, "500k and every 1000k"	},
	{0x1b, 0x01, 0xc0, 0x40, "800k and 2000k only"	},
	{0x1b, 0x01, 0xc0, 0x00, "1000k Only"		},

	{0   , 0xfe, 0   ,    2, "Country"		},
	{0x1c, 0x01, 0x80, 0x80, "USA (6 buttons)"	},
	{0x1c, 0x01, 0x80, 0x00, "Japan (2 buttons)"	},
};

STDDIPINFO(Daioh)

static struct BurnDIPInfo DaiohpDIPList[]=
{
	{0x1a, 0xff, 0xff, 0x7f, NULL			},
	{0x1b, 0xff, 0xff, 0xff, NULL			},
	{0x1c, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x1a, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x1a, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x1a, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x1a, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x1a, 0x01, 0x07, 0x03, "2 Coins 3 Credits"	},
	{0x1a, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x1a, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x1a, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x1a, 0x01, 0x38, 0x08, "4 Coins 1 Credits"	},
	{0x1a, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x1a, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x1a, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x1a, 0x01, 0x38, 0x18, "2 Coins 3 Credits"	},
	{0x1a, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x1a, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x1a, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x1a, 0x01, 0x40, 0x00, "Off"			},
	{0x1a, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Auto Shot"		},
	{0x1a, 0x01, 0x80, 0x80, "Off"			},
	{0x1a, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x1b, 0x01, 0x01, 0x01, "Off"			},
	{0x1b, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x1b, 0x01, 0x02, 0x02, "Off"			},
	{0x1b, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x1b, 0x01, 0x0c, 0x08, "Easy"			},
	{0x1b, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x1b, 0x01, 0x0c, 0x04, "Hard"			},
	{0x1b, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x1b, 0x01, 0x30, 0x00, "1"			},
	{0x1b, 0x01, 0x30, 0x10, "2"			},
	{0x1b, 0x01, 0x30, 0x30, "3"			},
	{0x1b, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x1b, 0x01, 0xc0, 0x80, "100k and every 600k"	},
	{0x1b, 0x01, 0xc0, 0xc0, "200k and every 800k"	},
	{0x1b, 0x01, 0xc0, 0x40, "300k and 1000k only"	},
	{0x1b, 0x01, 0xc0, 0x00, "500k Only"		},

	{0   , 0xfe, 0   ,    2, "Country"		},
	{0x1c, 0x01, 0x80, 0x80, "USA (6 buttons)"	},
	{0x1c, 0x01, 0x80, 0x00, "Japan (2 buttons)"	},
};

STDDIPINFO(Daiohp)

static struct BurnDIPInfo NeobattlDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x01, 0x01, "Off"			},
	{0x11, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x02, 0x00, "Off"			},
	{0x11, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Stage Select"		},
	{0x11, 0x01, 0x08, 0x08, "Off"			},
	{0x11, 0x01, 0x08, 0x00, "On"			},
	
	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x80, 0x80, "Off"			},
	{0x11, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x03, 0x02, "1"			},
	{0x12, 0x01, 0x03, 0x03, "2"			},
	{0x12, 0x01, 0x03, 0x01, "3"			},
	{0x12, 0x01, 0x03, 0x00, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x0c, 0x08, "Easy"			},
	{0x12, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x12, 0x01, 0x0c, 0x04, "Hard"			},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,   16, "Coinage"		},
	{0x12, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "8 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x20, "5 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "Free Play"		},
};

STDDIPINFO(Neobattl)

static struct BurnDIPInfo UmanclubDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x01, 0x01, "Off"			},
	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Stage Select"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x02, "1"			},
	{0x14, 0x01, 0x03, 0x03, "2"			},
	{0x14, 0x01, 0x03, 0x01, "3"			},
	{0x14, 0x01, 0x03, 0x00, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x0c, 0x08, "Easy"			},
	{0x14, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x14, 0x01, 0x0c, 0x04, "Hard"			},
	{0x14, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,   16, "Coinage"		},
	{0x14, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x10, "8 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0xf0, 0x20, "5 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0xf0, 0x00, "Free Play"		},
};

STDDIPINFO(Umanclub)

static struct BurnDIPInfo KamenridDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x01, 0x01, "Off"			},
	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Intro Music"		},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,   16, "Coinage"		},
	{0x14, 0x01, 0x0f, 0x05, "6 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x0d, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x03, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x0b, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x08, "8 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x07, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x04, "5 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0c, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0f, 0x02, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0f, 0x01, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x0f, 0x06, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x30, 0x10, "Easy"			},
	{0x14, 0x01, 0x30, 0x30, "Normal"		},
	{0x14, 0x01, 0x30, 0x20, "Hard"			},
	{0x14, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Country"		},
	{0x15, 0x01, 0x80, 0x80, "USA"			},
	{0x15, 0x01, 0x80, 0x00, "Japan"		},
};

STDDIPINFO(Kamenrid)

static struct BurnDIPInfo BlockcarDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL				},
	{0x14, 0xff, 0xff, 0xff, NULL				},
	{0x15, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x03, 0x02, "Easy"				},
	{0x13, 0x01, 0x03, 0x03, "Normal"			},
	{0x13, 0x01, 0x03, 0x01, "Hard"				},
	{0x13, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x13, 0x01, 0x0c, 0x0c, "20K, Every 50K"		},
	{0x13, 0x01, 0x0c, 0x04, "20K, Every 70K"		},
	{0x13, 0x01, 0x0c, 0x08, "30K, Every 60K"		},
	{0x13, 0x01, 0x0c, 0x00, "30K, Every 90K"		},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x13, 0x01, 0x30, 0x00, "1"				},
	{0x13, 0x01, 0x30, 0x30, "2"				},
	{0x13, 0x01, 0x30, 0x20, "3"				},
	{0x13, 0x01, 0x30, 0x10, "4"				},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x02, 0x02, "Off"				},
	{0x14, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x04, 0x00, "Off"				},
	{0x14, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x14, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x30, 0x00, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x14, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Title"			},
	{0x15, 0x01, 0x10, 0x10, "Thunder & Lightning 2"	},
	{0x15, 0x01, 0x10, 0x00, "Block Carnival"		},
};

STDDIPINFO(Blockcar)

static struct BurnDIPInfo ZombraidDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xfd, NULL					},
	{0x0f, 0xff, 0xff, 0xff, NULL					},
	{0x10, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Vertical Screen Flip"			},
	{0x0e, 0x01, 0x01, 0x01, "Off"					},
	{0x0e, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Horizontal Screen Flip"		},
	{0x0e, 0x01, 0x02, 0x00, "Off"					},
	{0x0e, 0x01, 0x02, 0x02, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0e, 0x01, 0x04, 0x00, "Off"					},
	{0x0e, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0e, 0x01, 0x18, 0x10, "Easy"					},
	{0x0e, 0x01, 0x18, 0x18, "Normal"				},
	{0x0e, 0x01, 0x18, 0x08, "Hard"					},
	{0x0e, 0x01, 0x18, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x0e, 0x01, 0x20, 0x00, "Off"					},
	{0x0e, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x0e, 0x01, 0x80, 0x80, "Off"					},
	{0x0e, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x0f, 0x01, 0x07, 0x05, "3 Coins 1 Credits"			},
	{0x0f, 0x01, 0x07, 0x06, "2 Coins 1 Credits"			},
	{0x0f, 0x01, 0x07, 0x07, "1 Coin  1 Credits"			},
	{0x0f, 0x01, 0x07, 0x04, "1 Coin  2 Credits"			},
	{0x0f, 0x01, 0x07, 0x03, "1 Coin  3 Credits"			},
	{0x0f, 0x01, 0x07, 0x02, "1 Coin  4 Credits"			},
	{0x0f, 0x01, 0x07, 0x01, "1 Coin  5 Credits"			},
	{0x0f, 0x01, 0x07, 0x00, "1 Coin  6 Credits"			},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x0f, 0x01, 0x38, 0x28, "3 Coins 1 Credits"			},
	{0x0f, 0x01, 0x38, 0x30, "2 Coins 1 Credits"			},
	{0x0f, 0x01, 0x38, 0x38, "1 Coin  1 Credits"			},
	{0x0f, 0x01, 0x38, 0x20, "1 Coin  2 Credits"			},
	{0x0f, 0x01, 0x38, 0x18, "1 Coin  3 Credits"			},
	{0x0f, 0x01, 0x38, 0x10, "1 Coin  4 Credits"			},
	{0x0f, 0x01, 0x38, 0x08, "1 Coin  5 Credits"			},
	{0x0f, 0x01, 0x38, 0x00, "1 Coin  6 Credits"			},

	{0   , 0xfe, 0   ,    2, "2 Coins to Start, 1 to Continue"	},
	{0x0f, 0x01, 0x40, 0x40, "No"					},
	{0x0f, 0x01, 0x40, 0x00, "Yes"					},
};

STDDIPINFO(Zombraid)

static struct BurnDIPInfo JockeycDIPList[]=
{
	{0x39, 0xff, 0xff, 0xff, NULL				},
	{0x3a, 0xff, 0xff, 0xff, NULL				},
	{0x3b, 0xff, 0xff, 0xff, NULL				},

	{0x3c, 0xff, 0xff, 0xff, NULL				},
	{0x3d, 0xff, 0xff, 0xff, NULL				},
	{0x3e, 0xff, 0xff, 0xff, NULL				},
	{0x3f, 0xff, 0xff, 0xff, NULL				},

// dip1
	{0   , 0xfe, 0   ,    3, "Max Bet"			},
	{0x39, 0x01, 0x03, 0x03, "10"				},
	{0x39, 0x01, 0x03, 0x02, "20"				},
	{0x39, 0x01, 0x03, 0x01, "99"				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x39, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},
	{0x39, 0x01, 0x1c, 0x18, "1 Coin  2 Credits"		},
	{0x39, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"		},
	{0x39, 0x01, 0x1c, 0x10, "1 Coin  4 Credits"		},
	{0x39, 0x01, 0x1c, 0x0c, "1 Coin  5 Credits"		},
	{0x39, 0x01, 0x1c, 0x08, "1 Coin/10 Credits"		},
	{0x39, 0x01, 0x1c, 0x04, "1 Coin/20 Credits"		},
	{0x39, 0x01, 0x1c, 0x00, "1 Coin/50 Credits"		},

// dip2-3
	{0   , 0xfe, 0   ,    4, "Betting Clock Speed"		},
	{0x3a, 0x01, 0x18, 0x18, "Slowest"			},
	{0x3a, 0x01, 0x18, 0x10, "Slower"			},
	{0x3a, 0x01, 0x18, 0x08, "Faster"			},
	{0x3a, 0x01, 0x18, 0x00, "Fastest"			},

	{0   , 0xfe, 0   ,    16, "Payout Rate"			},
	{0x3b, 0x01, 0x01, 0x01, "80%"				},
	{0x3b, 0x01, 0x01, 0x01, "81%"				},
	{0x3b, 0x01, 0x01, 0x01, "82%"				},
	{0x3b, 0x01, 0x01, 0x01, "83%"				},
	{0x3b, 0x01, 0x01, 0x01, "84%"				},
	{0x3b, 0x01, 0x01, 0x01, "85%"				},
	{0x3b, 0x01, 0x01, 0x01, "86%"				},
	{0x3b, 0x01, 0x01, 0x01, "87%"				},
	{0x3b, 0x01, 0x01, 0xe0, "88%"				},
	{0x3b, 0x01, 0x01, 0xc0, "89%"				},
	{0x3b, 0x01, 0x01, 0xa0, "90%"				},
	{0x3b, 0x01, 0x01, 0x80, "91%"				},
	{0x3b, 0x01, 0x01, 0x60, "92%"				},
	{0x3b, 0x01, 0x01, 0x40, "93%"				},
	{0x3b, 0x01, 0x01, 0x20, "94%"				},
	{0x3b, 0x01, 0x01, 0x00, "95%"				},

	{0   , 0xfe, 0   ,    2, "Payout"			},
	{0x3b, 0x01, 0x04, 0x00, "Off"				},
	{0x3b, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    2, "Horses"			},
	{0x3b, 0x01, 0x08, 0x08, "Random"			},
	{0x3b, 0x01, 0x08, 0x00, "Cyclic"			},

	{0   , 0xfe, 0   ,    2, "Higher Odds"			},
	{0x3b, 0x01, 0x10, 0x10, "Off"				},
	{0x3b, 0x01, 0x10, 0x00, "On"				},

// overlay on p1/p2
	{0   , 0xfe, 0   ,    2, "Coin Drop - 1P"		},
	{0x3c, 0x01, 0x01, 0x01, "Off"				},
	{0x3c, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Hopper Overflow - 1P"		},
	{0x3c, 0x01, 0x02, 0x02, "Off"				},
	{0x3c, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Coin Out"			},
	{0x3c, 0x01, 0x04, 0x00, "Off"				},
	{0x3c, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    2, "Att Pay - 1P"			},
	{0x3c, 0x01, 0x08, 0x08, "Off"				},
	{0x3c, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Coin Sense 2 - 1P"		},
	{0x3c, 0x01, 0x40, 0x40, "Off"				},
	{0x3c, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Coin Sense 1 - 1P"		},
	{0x3c, 0x01, 0x80, 0x80, "Off"				},
	{0x3c, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Coin Drop - 2P"		},
	{0x3d, 0x01, 0x01, 0x01, "Off"				},
	{0x3d, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Sel Sense"			},
	{0x3d, 0x01, 0x02, 0x02, "Off"				},
	{0x3d, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Att Pay - 2P"			},
	{0x3d, 0x01, 0x08, 0x08, "Off"				},
	{0x3d, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Coin Sense 2 - 2P"		},
	{0x3d, 0x01, 0x40, 0x40, "Off"				},
	{0x3d, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Coin Sense 1 - 2P"		},
	{0x3d, 0x01, 0x80, 0x80, "Off"				},
	{0x3d, 0x01, 0x80, 0x00, "On"				},

// p2
	{0   , 0xfe, 0   ,    2, "SYSTEM"			},
	{0x3e, 0x01, 0x02, 0x02, "Off"				},
	{0x3e, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Tilt"				},
	{0x3e, 0x01, 0x08, 0x08, "Off"				},
	{0x3e, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Special Test Mode Item?"	},
	{0x3f, 0x01, 0x04, 0x04, "Off"				},
	{0x3f, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Call SW"			},
	{0x3f, 0x01, 0x08, 0x08, "Off"				},
	{0x3f, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x3f, 0x01, 0x10, 0x10, "Off"				},
	{0x3f, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Analyzer"			},
	{0x3f, 0x01, 0x20, 0x20, "Off"				},
	{0x3f, 0x01, 0x20, 0x00, "On"				},
};

STDDIPINFO(Jockeyc)

void __fastcall setaSoundRegWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	//bprintf(PRINT_NORMAL, _T("x1-010 to write word value %x to location %x\n"), wordValue, sekAddress);
	UINT32 offset = (sekAddress & 0x00003fff) >> 1;
	INT32 channel, reg;

	x1_010_chip->HI_WORD_BUF[ offset ] = wordValue >> 8;

	offset ^= x1_010_chip->address;

	channel	= offset / sizeof(X1_010_CHANNEL);
	reg		= offset % sizeof(X1_010_CHANNEL);

	if( channel < SETA_NUM_CHANNELS && reg == 0 && (x1_010_chip->reg[offset]&1) == 0 && (wordValue&1) != 0 ) {
	 	x1_010_chip->smp_offset[channel] = 0;
	 	x1_010_chip->env_offset[channel] = 0;
	}
	x1_010_chip->reg[offset] = wordValue & 0xff;
}

UINT16 __fastcall setaSoundRegReadWord(UINT32 sekAddress)
{
	return x1010_sound_read_word((sekAddress & 0x3ffff) >> 1);
}

// these should probably be moved to x1010.cpp
static UINT8 __fastcall setaSoundRegReadByte(UINT32 sekAddress)
{
	if (~sekAddress & 1) {
		return x1_010_chip->HI_WORD_BUF[(sekAddress & 0x3fff) >> 1];
	} else {
		return x1010_sound_read_word((sekAddress & 0x3fff) >> 1);
	}
}

static void __fastcall setaSoundRegWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	UINT32 offset = (sekAddress & 0x00003fff) >> 1;
	INT32 channel, reg;

	if (~sekAddress & 1) {
		x1_010_chip->HI_WORD_BUF[ offset ] = byteValue;
	} else {
		offset  ^= x1_010_chip->address;
		channel  = offset / sizeof(X1_010_CHANNEL);
		reg      = offset % sizeof(X1_010_CHANNEL);

		if (channel < SETA_NUM_CHANNELS && reg == 0 && (x1_010_chip->reg[offset] & 1) == 0 && (byteValue&1) != 0) {
	 		x1_010_chip->smp_offset[channel] = 0;
	 		x1_010_chip->env_offset[channel] = 0;
		}
		x1_010_chip->reg[offset] = byteValue;
	}
}

static void __fastcall setaSoundRegWriteByte8bit(UINT32 sekAddress, UINT8 byteValue)
{
	UINT32 offset = (sekAddress & 0x00003fff);
	INT32 channel, reg;
	//bprintf(0, _T("8bit addy %X offset %X byte %X. "), x1_010_chip->address, offset, byteValue);
	offset  ^= x1_010_chip->address;
	channel  = offset / sizeof(X1_010_CHANNEL);
	reg      = offset % sizeof(X1_010_CHANNEL);

	if (channel < SETA_NUM_CHANNELS && reg == 0 && (x1_010_chip->reg[offset] & 1) == 0 && (byteValue&1) != 0) {
		x1_010_chip->smp_offset[channel] = 0;
		x1_010_chip->env_offset[channel] = 0;
	}
	x1_010_chip->reg[offset] = byteValue;
}

static void set_pcm_bank(INT32 data)
{
	INT32 new_bank = (data >> 3) & 0x07;

	if (new_bank != seta_samples_bank)
	{
		INT32 samples_len = DrvROMLen[3];
		//bprintf(0, _T("seta_samples_bank[%X] new_bank[%X] samples_len[%x]\n"), seta_samples_bank, new_bank, samples_len);
		seta_samples_bank = data;

		if (samples_len == 0x240000 || samples_len == 0x1c0000 || samples_len == 0x80000) // eightfrc, blandia
		{
			INT32 addr = 0x40000 * new_bank;
			if (new_bank >= 3) addr += 0x40000;

			if ((samples_len > 0x100000) && ((addr + 0x40000) <= samples_len)) {
				memcpy(DrvSndROM + 0xc0000, DrvSndROM + addr, 0x40000);
			}
		}
		else if (samples_len == 0x400000) // zombraid
		{
			if (new_bank == 0) new_bank = 1;
			INT32 addr = 0x80000 * new_bank;
			if (new_bank > 0) addr += 0x80000;

			memcpy (DrvSndROM + 0x80000, DrvSndROM + addr, 0x80000);
		}
	}
}

//-----------------------------------------------------------------------------------------------------------------------------------
// macros

#define SetaVidRAMCtrlWriteWord(num, base)						\
	if ((address >= (base + 0)) && address <= (base + 5)) {				\
		*((UINT16*)(DrvVIDCTRLRAM##num + (address & 0x06))) = BURN_ENDIAN_SWAP_INT16(data);	\
		raster_needs_update = 1;				\
		return;									\
	}

#define SetaVidRAMCtrlWriteByte(num, base)				\
	if ((address >= (base + 0)) && (address <= (base + 5))) {	\
		DrvVIDCTRLRAM##num[(address & 0x07)^1] = data;		\
		raster_needs_update = 1;		\
		return;							\
	}

#define SetVidRAMRegsWriteWord(base)	\
	if ((address >= (base + 0)) && (address <= (base + 5))) {		\
		*((UINT16*)(DrvVideoRegs + (address & 0x06))) = BURN_ENDIAN_SWAP_INT16(data);	\
		if ((address - base) == 0) x1010Enable(data & 0x20);		\
		if ((address - base) == 2) set_pcm_bank(data);			\
		return;								\
	}

#define SetVidRAMRegsWriteByte(base)	\
	if ((address >= (base + 0)) && (address <= (base + 5))) {		\
		DrvVideoRegs[(address & 0x07)^1] = data;			\
		return;								\
	}

#define SetaReadDips(base)	\
	if (address >= (base + 0) && address <= (base + 3)) {	\
		return DrvDips[((address - base)/2)^1];		\
	}

//-----------------------------------------------------------------------------------------------------------------------------------
// drgnunit

static UINT16 __fastcall drgnunit_read_word(UINT32 address)
{
	SetaReadDips(0x600000)

	switch (address)
	{
		case 0xb00000:
			return DrvInputs[0];

		case 0xb00002:
			return DrvInputs[1];

		case 0xb00004:
			return DrvInputs[2]^0xff^DrvDips[2];
	}

	return 0;
}

static UINT8 __fastcall drgnunit_read_byte(UINT32 address)
{
	SetaReadDips(0x600000)

	switch (address)
	{
		case 0xb00000:
		case 0xb00001:
			return DrvInputs[0];

		case 0xb00002:
		case 0xb00003:
			return DrvInputs[1];

		case 0xb00004:
		case 0xb00005:
			return DrvInputs[2]^0xff^DrvDips[2];
	}

	return 0;
}

static void __fastcall drgnunit_write_word(UINT32 address, UINT16 data)
{
	SetaVidRAMCtrlWriteWord(0, 0x800000)

	SetVidRAMRegsWriteWord(0x500000)
}

static void __fastcall drgnunit_write_byte(UINT32 address, UINT8 data)
{
	SetaVidRAMCtrlWriteByte(0, 0x800000)

	SetVidRAMRegsWriteByte(0x500000)
}

//-----------------------------------------------------------------------------------------------------------------------------------
// thunderl, wits

static UINT16 __fastcall thunderl_read_word(UINT32 address)
{
	SetaReadDips(0x600000)

	switch (address)
	{
		case 0xb00000:
		case 0xb00001:
			return DrvInputs[0];

		case 0xb00002:
		case 0xb00003:
			return DrvInputs[1];

		case 0xb00004:
		case 0xb00005:
			return DrvInputs[2]^0xff^DrvDips[2];

		case 0xb00008:
		case 0xb00009:
			return DrvInputs[3];

		case 0xb0000a:
		case 0xb0000b:
			return DrvInputs[4];

		case 0xb0000c:
		case 0xb0000d:
			return 0x00dd;// thunderl_prot
	}

	return 0;
}

static UINT8 __fastcall thunderl_read_byte(UINT32 address)
{
	SetaReadDips(0x600000)

	switch (address)
	{
		case 0xb00000:
		case 0xb00001:
			return DrvInputs[0];

		case 0xb00002:
		case 0xb00003:
			return DrvInputs[1];

		case 0xb00004:
		case 0xb00005:
			return DrvInputs[2]^0xff^DrvDips[2];

		case 0xb00008:
		case 0xb00009:
			return DrvInputs[3];

		case 0xb0000a:
		case 0xb0000b:
			return DrvInputs[4];

		case 0xb0000c:
		case 0xb0000d:
			return 0xdd;// thunderl_prot
	}

	return 0;
}

static void __fastcall thunderl_write_word(UINT32 address, UINT16 data)
{
	SetVidRAMRegsWriteWord(0x500000)
}

static void __fastcall thunderl_write_byte(UINT32 address, UINT8 data)
{
	SetVidRAMRegsWriteByte(0x500000)
}

//-----------------------------------------------------------------------------------------------------------------------------------
// daioh

static UINT16 __fastcall daioh_read_word(UINT32 address)
{
	SetaReadDips(0x300000)
	SetaReadDips(0x400008)
	SetaReadDips(0x600000)

	switch (address)
	{
		case 0x400000:
			return DrvInputs[0];

		case 0x400002:
			return DrvInputs[1];

		case 0x400004:
			return DrvInputs[2]^0xff^DrvDips[2];

		case 0x40000c:
			watchdog = 0;
			return 0xff;

		case 0x500006:
			return DrvInputs[3];
	}

	return 0;
}

static UINT8 __fastcall daioh_read_byte(UINT32 address)
{
	SetaReadDips(0x300000)
	SetaReadDips(0x400008)
	SetaReadDips(0x600000)

	switch (address)
	{
		case 0x400000:
		case 0x400001:
			return DrvInputs[0];

		case 0x400002:
		case 0x400003:
			return DrvInputs[1];

		case 0x400004:
		case 0x400005:
			return DrvInputs[2]^0xff^DrvDips[2];

		case 0x40000c:
		case 0x40000d:
			watchdog = 0;
			return 0xff;

		case 0x500006:
		case 0x500007:
			return DrvInputs[3];
	}

	return 0;
}

static void __fastcall daioh_write_word(UINT32 address, UINT16 data)
{
	SetVidRAMRegsWriteWord(0x500000)

	SetaVidRAMCtrlWriteWord(0, 0x900000) // blandiap
	if (!daiohc) {
		SetaVidRAMCtrlWriteWord(0, 0x908000) // jjsquawkb
		SetaVidRAMCtrlWriteWord(0, 0xa00000) // blandia
	}

	SetaVidRAMCtrlWriteWord(1, 0x980000) // blandiap
	if (!daiohc) {
		SetaVidRAMCtrlWriteWord(1, 0x909000) // jjsquawkb
		SetaVidRAMCtrlWriteWord(1, 0xa80000) // blandia
	}

	switch (address)
	{
		case 0x400000:
		case 0x40000c:
			watchdog = 0;
		return;
	}
}

static void __fastcall daioh_write_byte(UINT32 address, UINT8 data)
{
	SetVidRAMRegsWriteByte(0x500000)

	SetaVidRAMCtrlWriteByte(0, 0x900000) // blandiap
	if (!daiohc) {
		SetaVidRAMCtrlWriteByte(0, 0x908000) // jjsquawkb
		SetaVidRAMCtrlWriteByte(0, 0xa00000) // blandia
	}

	SetaVidRAMCtrlWriteByte(1, 0x980000) // blandiap
	if (!daiohc) {
		SetaVidRAMCtrlWriteByte(1, 0x909000) // jjsquawkb
		SetaVidRAMCtrlWriteByte(1, 0xa80000) // blandia
	}

	switch (address)
	{
		case 0x400000:
		case 0x400001:
		case 0x40000c:
		case 0x40000d:
			watchdog = 0;
		return;
	}
}

//-----------------------------------------------------------------------------------------------------------------------------------
// msgundam

static void __fastcall msgundam_write_word(UINT32 address, UINT16 data)
{
	SetaVidRAMCtrlWriteWord(0, 0xb00000)
	SetaVidRAMCtrlWriteWord(1, 0xb80000)

	switch (address)
	{
		case 0x500000:
		case 0x500001:
			*((UINT16*)(DrvVideoRegs + 0)) = data;
		return;
		case 0x500002:
		case 0x500003:
			*((UINT16*)(DrvVideoRegs + 4)) = data;
		return;

		case 0x500004:
		case 0x500005:
			*((UINT16*)(DrvVideoRegs + 2)) = data;
		return;
	}
}

static void __fastcall msgundam_write_byte(UINT32 address, UINT8 data)
{
	SetaVidRAMCtrlWriteByte(0, 0xb00000)
	SetaVidRAMCtrlWriteByte(1, 0xb80000)

	switch (address)
	{
		case 0x500000:
		case 0x500001:
			DrvVideoRegs[(~address & 0x01) | 0] = data;
		return;

		case 0x500002:
		case 0x500003:
			DrvVideoRegs[(~address & 0x01) | 4] = data;
		return;

		case 0x500004:
		case 0x500005:
			DrvVideoRegs[(~address & 0x01) | 2] = data;
			// seta_vregs_w
		return;
	}
}

//-----------------------------------------------------------------------------------------------------------------------------------
// kamenrid

static UINT16 __fastcall kamenrid_read_word(UINT32 address)
{
	SetaReadDips(0x500004)

	switch (address)
	{
		case 0x500000:
			return DrvInputs[0];

		case 0x500002:
			return DrvInputs[1];

		case 0x500008:
			return DrvInputs[2]^0xff^DrvDips[2];

		case 0x50000c:
			return 0xffff; // watchdog
	}

	return 0;
}

static UINT8 __fastcall kamenrid_read_byte(UINT32 address)
{
	SetaReadDips(0x500004)

	switch (address)
	{
		case 0x500000:
		case 0x500001:
			return DrvInputs[0];

		case 0x500002:
		case 0x500003:
			return DrvInputs[1];

		case 0x500008:
		case 0x500009:
			return DrvInputs[2]^0xff^DrvDips[2];

		case 0x50000c:
		case 0x50000d:
			return 0xff; // watchdog
	}

	return 0;
}

//-----------------------------------------------------------------------------------------------------------------------------------
// krzybowl, madshark

static void trackball_input_tick() // krzybowl, usclssic
{
	INT32 padx = ProcessAnalog(DrvAnalogPort0, 1, 1, 0x01, 0xff) - 0x7f;
	if (padx > 0 && padx < 8) padx = 0;

	if (usclssic) {
		padx /= 16;
		track_x -= padx;
		if (padx) track_x_last = padx;
		if (!padx) {
			track_x = 0; track_x_last = 0; // PORT_RESET
		}
	} else { // krzybowl
		track_x += padx;
	}

	INT32 pady = ProcessAnalog(DrvAnalogPort1, 1, 1, 0x01, 0xff) - 0x7f;
	if (pady > 0 && pady < 8) pady = 0;
	if (usclssic) {
		pady /= 16;
		track_y -= pady;
		if (pady) track_y_last = pady;
		if (!pady) {
			track_y = 0; track_y_last = 0; // PORT_RESET
		}
	} else { // krzybowl
		track_y += pady;
	}

	padx = ProcessAnalog(DrvAnalogPort2, 1, 1, 0x01, 0xff) - 0x7f;
	if (padx > 0 && padx < 8) padx = 0;

	if (usclssic) {
		padx /= 16;
		track_x2 -= padx;
		if (padx) track_x2_last = padx;
		if (!padx) {
			track_x2 = 0; track_x2_last = 0; // PORT_RESET
		}
	} else { // krzybowl
		track_x2 += padx;
	}

	pady = ProcessAnalog(DrvAnalogPort3, 1, 1, 0x01, 0xff) - 0x7f;
	if (pady > 0 && pady < 8) pady = 0;
	if (usclssic) {
		pady /= 16;
		track_y2 -= pady;
		if (pady) track_y2_last = pady;
		if (!pady) {
			track_y2 = 0; track_y2_last = 0; // PORT_RESET
		}
	} else { // krzybowl
		track_y2 += pady;
	}
}

static UINT16 krzybowl_input_read(INT32 offset)
{
	INT32 dir1x = track_x & 0xfff;
	INT32 dir1y = track_y & 0xfff;
	INT32 dir2x = track_x2 & 0xfff;
	INT32 dir2y = track_y2 & 0xfff;

	switch (offset / 2)
	{
		case 0x0/2:	return dir1x & 0xff;
		case 0x2/2:	return dir1x >> 8;
		case 0x4/2:	return dir1y & 0xff;
		case 0x6/2:	return dir1y >> 8;
		case 0x8/2:	return dir2x & 0xff;
		case 0xa/2:	return dir2x >> 8;
		case 0xc/2:	return dir2y & 0xff;
		case 0xe/2:	return dir2y >> 8;
	}

	return 0;
}

static UINT16 __fastcall madshark_read_word(UINT32 address)
{
	SetaReadDips(0x300000)
	SetaReadDips(0x500008)

	switch (address)
	{
		case 0x500000:
			return DrvInputs[0];

		case 0x500002:
			return DrvInputs[1];

		case 0x500004:
			return DrvInputs[2]^0xff^DrvDips[2];

		case 0x50000c:
			watchdog = 0;
			return 0xffff;
	}

	if ((address & ~0x00000f) == 0x600000) {
		return krzybowl_input_read(address&0xf);
	}

	return 0;
}

static UINT8 __fastcall madshark_read_byte(UINT32 address)
{
	SetaReadDips(0x300000)
	SetaReadDips(0x500008)

	switch (address)
	{
		case 0x500000:
			return DrvInputs[0] >> 8;
		case 0x500001:
			return DrvInputs[0] & 0xff;

		case 0x500002:
			return DrvInputs[1] >> 8;
		case 0x500003:
			return DrvInputs[1] & 0xff;

		case 0x500004:
		case 0x500005:
			return DrvInputs[2]^0xff^DrvDips[2];

		case 0x50000c:
		case 0x50000d:
			watchdog = 0;
			return 0xff;
	}

	if ((address & ~0x00000f) == 0x600000) {
		return krzybowl_input_read(address&0xf);
	}

	return 0;
}

static void __fastcall madshark_write_word(UINT32 address, UINT16 data)
{
	SetVidRAMRegsWriteWord(0x600000)
	SetaVidRAMCtrlWriteWord(0, 0x900000)
	SetaVidRAMCtrlWriteWord(1, 0x980000)

	switch (address)
	{
		case 0x50000c:
			watchdog = 0;
		return;
	}
}

static void __fastcall madshark_write_byte(UINT32 address, UINT8 data)
{
	SetVidRAMRegsWriteByte(0x600000)
	SetaVidRAMCtrlWriteByte(0, 0x900000)
	SetaVidRAMCtrlWriteByte(1, 0x980000)

	switch (address)
	{
		case 0x50000c:
		case 0x50000d:
			watchdog = 0;
		return;
	}
}

//-----------------------------------------------------------------------------------------------------------------------------------
// keroppi, pairlove

static INT32 keroppi_prize_hop = 0;
static INT32 keroppi_protection_count = 0;
static INT32 keroppi_timer_frame = -1;
static const UINT16 keroppi_protection_word[] = {
	0x0000,
	0x0000, 0x0000, 0x0000,
	0x2000, 0x2000, 0x2000,
	0x2000, 0x2000, 0x2000,
	0x0400, 0x0400, 0x0400,
	0x0000, 0x0000, 0x0000
};

static UINT16 pairslove_protram[0x100];
static UINT16 pairslove_protram_old[0x100];

static void keroppi_pairslove_scan()
{
	SCAN_VAR(keroppi_prize_hop);
	SCAN_VAR(keroppi_protection_count);
	SCAN_VAR(keroppi_timer_frame);
	SCAN_VAR(pairslove_protram);
	SCAN_VAR(pairslove_protram_old);
}

static UINT16 __fastcall pairlove_read_word(UINT32 address)
{
	SetaReadDips(0x300000)

	switch (address)
	{
		case 0x100000: { // keroppi
			INT32 res = keroppi_protection_word[keroppi_protection_count];
			keroppi_protection_count++;
			if (keroppi_protection_count > 15) keroppi_protection_count = 15;
			return res;
		}

		case 0x200000: // keroppi
			keroppi_protection_count = 0;
			return 0x00;

		case 0x500000:
			return DrvInputs[0];

		case 0x500002:
			return DrvInputs[1];

		case 0x500004: {
			INT32 res = DrvInputs[2]^0xff^DrvDips[2];

			if (keroppi_prize_hop == 1 && keroppi_timer_frame != -1) {
				if ((GetCurrentFrame() - keroppi_timer_frame) >= 3) {
					keroppi_prize_hop = 2;
					keroppi_timer_frame = - 1;
				}
			}

			if (keroppi_prize_hop == 2) {
				res &= ~0x0002;
				keroppi_prize_hop = 0;
			}
			return res;
		}
	}

	if ((address & 0xfffffe00) == 0x900000) {
		INT32 offset = (address & 0x1ff) / 2;

		INT32 retdata = pairslove_protram[offset];
		pairslove_protram[offset]=pairslove_protram_old[offset];
		return retdata;
	}

	return 0;
}

static UINT8 __fastcall pairlove_read_byte(UINT32 address)
{
	SetaReadDips(0x300000)

	switch (address)
	{
		case 0x100000: // keroppi
		case 0x100001: {
			INT32 res = keroppi_protection_word[keroppi_protection_count];
			keroppi_protection_count++;
			if (keroppi_protection_count > 15) keroppi_protection_count = 15;
			return res;
		}

		case 0x200000:
		case 0x200001: // keroppi
			keroppi_protection_count = 0;
			return 0x00;

		case 0x500000:
		case 0x500001:
			return DrvInputs[0];

		case 0x500002:
		case 0x500003:
			return DrvInputs[1];

		case 0x500004:
		case 0x500005: {
			INT32 res = DrvInputs[2]^0xff^DrvDips[2];

			if (keroppi_prize_hop == 1 && keroppi_timer_frame != -1) {
				if ((GetCurrentFrame() - keroppi_timer_frame) >= 3) {
					keroppi_prize_hop = 2;
					keroppi_timer_frame = -1;
				}
			}

			if (keroppi_prize_hop == 2) {
				res &= ~0x0002;
				keroppi_prize_hop = 0;
			}
			return res;
		}
	}

	if ((address & 0xfffffe00) == 0x900000) {
		INT32 offset = (address & 0x1ff) / 2;
		INT32 retdata = pairslove_protram[offset];
		pairslove_protram[offset]=pairslove_protram_old[offset];
		return retdata;
	}

	return 0;
}

static void __fastcall pairlove_write_word(UINT32 address, UINT16 data)
{
	SetVidRAMRegsWriteWord(0x400000)

	switch (address)
	{
		case 0x900002: // keroppi
			if ((data & 0x0010) && !keroppi_prize_hop) {
				keroppi_prize_hop = 1;
				keroppi_timer_frame = GetCurrentFrame();
			}
		break; // for pairslove prot
	}

	if ((address & 0xfffffe00) == 0x900000) {
		INT32 offset = (address & 0x1ff) / 2;
		pairslove_protram_old[offset]=pairslove_protram[offset];
		pairslove_protram[offset]=data;
		return;
	}
}

static void __fastcall pairlove_write_byte(UINT32 address, UINT8 data)
{
	SetVidRAMRegsWriteByte(0x400000)

	switch (address)
	{
		case 0x900002:
		case 0x900003: // keroppi
			if ((data & 0x0010) && !keroppi_prize_hop) {
				keroppi_prize_hop = 1;
				keroppi_timer_frame = GetCurrentFrame();
			}
		break; // for pairslove prot
	}

	if ((address & 0xfffffe00) == 0x900000) {
		INT32 offset = (address & 0x1ff) / 2;
		pairslove_protram_old[offset]=pairslove_protram[offset];
		pairslove_protram[offset]=data;
		return;
	}
}

//-----------------------------------------------------------------------------------------------------------------------------------
// downtown, metafox, twineagl, arbelester 

static void __fastcall downtown_write_word(UINT32 address, UINT16 data)
{
	SetaVidRAMCtrlWriteWord(0, 0x800000)

	switch (address)
	{
		case 0x400000:
		case 0x400002:
		case 0x400004:
		case 0x400006:
	//		tilebank[(address & 6) / 2] = data;
		return;

		case 0xa00000:
		case 0xa00002:
		case 0xa00004:
		case 0xa00006: bprintf(0, _T("sub ctrlW unimpl. %X\n"), address);
			// sub_ctrl_w
		return;
	}
}

static void sub_ctrl_w(INT32 offset, UINT8 data)
{
	switch(offset)
	{
		case 0:   // bit 0: reset sub cpu?
			{
				if ( !(sub_ctrl_data & 1) && (data & 1) )
				{
					M6502Open(0);
					M6502Reset();
					M6502Close();
				}
				sub_ctrl_data = data;
			}
			break;

		case 2:   // ?
			break;

		case 4:   // not sure
			soundlatch = data;
			break;

		case 6:   // not sure
			soundlatch2 = data;
			break;
	}
}

static void __fastcall downtown_write_byte(UINT32 address, UINT8 data)
{
	SetaVidRAMCtrlWriteByte(0, 0x800000)

	switch (address)
	{
		case 0x400000:
		case 0x400001:
		case 0x400002:
		case 0x400003:
		case 0x400004:
		case 0x400005:
		case 0x400006:
		case 0x400007:
			tilebank[(address & 6) / 2] = data;
		return;

		case 0xa00000:
		case 0xa00001:
		case 0xa00002:
		case 0xa00003:
		case 0xa00004:
		case 0xa00005:
		case 0xa00006:
		case 0xa00007:
			sub_ctrl_w((address&6), data);
		return;
	}
}

//-----------------------------------------------------------------------------------------------------------------------------------
// kiwame 

static INT32 kiwame_inputs_read(INT32 offset)
{
	INT32 i;
	INT32 row_select = DrvNVRAM[0x10b];

	for (i = 0; i < 5; i++)
		if (row_select & (1 << i)) break;

	switch (offset)
	{
		case 0:	return DrvInputs[i+1];
		case 4:	return DrvInputs[0]^0xff^DrvDips[2];
		case 2:
		case 8:	return 0xffff;
	}

	return 0;
}

static UINT16 __fastcall kiwame_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xe00000:
			return DrvDips[1];

		case 0xe00002:
			return DrvDips[0];
	}

	if ((address & 0xfffff0) == 0xd00000) {
		return kiwame_inputs_read(address & 0x0e);
	}
	if ((address & 0xfffc00) == 0xfffc00) {
		return DrvNVRAM[(address & 0x3fe)];
	}

	return 0;
}

static UINT8 __fastcall kiwame_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xe00000:
		case 0xe00001:
			return DrvDips[1];

		case 0xe00002:
		case 0xe00003:
			return DrvDips[0];
	}

	if ((address & 0xfffff0) == 0xd00000) {
		return kiwame_inputs_read(address & 0x0e);
	}

	if ((address & 0xfffc01) == 0xfffc01) {
		return DrvNVRAM[(address & 0x3fe)];
	}

	return 0;
}

//-----------------------------------------------------------------------------------------------------------------------------------
// zombraid gun handler

static INT32 zombraid_gun_read()
{
	return (DrvAnalogInput[gun_input_src] >> gun_input_bit) & 1;
}

static void zombraid_gun_write(INT32 data)
{
	static INT32 bit_count = 0, old_clock = 0;

	if(data&4) { bit_count = 0; return; } // Reset

	if((data&1) == old_clock) return; // No change

	if (old_clock == 0) // Rising edge
	{
		switch (bit_count)
		{
			case 0:
			case 1: // Starting sequence 2,3,2,3. Other inputs?
				break;

			case 2: // First bit of source
				gun_input_src = (gun_input_src&2) | (data>>1);
				break;

			case 3: // Second bit of source
				gun_input_src = (gun_input_src&1) | (data&2);
				break;

			default:
			//	"Player1_Gun_Recoil", (data & 0x10)>>4
			//	"Player2_Gun_Recoil", (data & 0x08)>>3
				gun_input_bit = bit_count - 4;
				gun_input_bit = 8 - gun_input_bit; // Reverse order
				break;
		}
		bit_count++;
	}

	old_clock = data & 1;
}

static void __fastcall zombraid_gun_write_word(UINT32 address, UINT16 data)
{
	if ((address & ~1) == 0xf00000) zombraid_gun_write(data);
}

static void __fastcall zombraid_gun_write_byte(UINT32 address, UINT8 data)
{
	if ((address & ~1) == 0xf00000) zombraid_gun_write(data);
}

static UINT16 __fastcall zombraid_gun_read_word(UINT32 )
{
	return zombraid_gun_read();
}

static UINT8 __fastcall zombraid_gun_read_byte(UINT32 )
{
	return zombraid_gun_read();
}

//-----------------------------------------------------------------------------------------------------------------------------------
// utoukond sound handler

static void __fastcall utoukond_sound_write(UINT16 address, UINT8 data)
{
	if (address >= 0xf000) { // x1_010
		setaSoundRegWriteByte8bit(address & 0xfff, data);
		return;
	}
}

static UINT8 __fastcall  utoukond_sound_read(UINT16 address)
{
	if (address >= 0xf000) {
		return x1010_sound_read(address & 0xfff);
	}
	return 0;
}

static void __fastcall utoukond_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			BurnYM3438Write(0, port & 3, data);
		return;
	}
}

static UINT8 __fastcall utoukond_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			return BurnYM3438Read(0, port & 3);// right?

		case 0xc0:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

//-----------------------------------------------------------------------------------------------------------------------------------
// wiggie / superbar sound handler

static void __fastcall wiggie_sound_write_word(UINT32 a, UINT16 d)
{
	bprintf(0, _T("sww %X:%X."),a,d);

}

static void __fastcall wiggie_sound_write_byte(UINT32 address, UINT8 data)
{
	soundlatch = data;
	ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
}

static void __fastcall wiggie_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9800:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 __fastcall wiggie_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x9800:
			return MSM6295Read(0);

		case 0xa000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

//-----------------------------------------------------------------------------------------------------------------------------------
// usclssic 

static void __fastcall usclssic_write_word(UINT32 address, UINT16 data)
{
	SetaVidRAMCtrlWriteWord(0, 0xa00000)

	switch (address)
	{
		case 0xb40000:
			usclssic_port_select = (data & 0x40) >> 6;
			tile_offset[0]	     = (data & 0x10) << 10;
			// coin lockout too...
		return;

		case 0xb40010:
			soundlatch = data;
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
			SekRunEnd();
		return;

		case 0xb40018:
			watchdog = 0;
		return;
	}
}

static void __fastcall usclssic_write_byte(UINT32 address, UINT8 data)
{
	SetaVidRAMCtrlWriteByte(0, 0xa00000)

	switch (address)
	{
		case 0xb40000:
		case 0xb40001:
			usclssic_port_select = (data & 0x40) >> 6;
			tile_offset[0]	     = (data & 0x10) << 10;
			// coin lockout too...
		return;

		//case 0xb40010:
		case 0xb40011:
			soundlatch = data;
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
			SekRunEnd();
		return;

		case 0xb40018:
		case 0xb40019:
			watchdog = 0;
		return;
	}
}

static UINT8 uclssic_trackball_read(INT32 offset)
{
	UINT16 start_vals[2] = { 0xf000, 0x9000 };
	if (!usclssic_port_select) {
		start_vals[0] |= track_x&0xfff;
		start_vals[1] |= track_y&0xfff;
	} else {
		start_vals[0] |= track_x2&0xfff;
		start_vals[1] |= track_y2&0xfff;
	}

	UINT16 ret = DrvInputs[1 + ((offset & 4)/4) + (usclssic_port_select * 2)] ^ start_vals[(offset / 4) & 1];

	if (offset & 2) ret >>= 8;

	return ret ^ 0xff;
}

static UINT16 __fastcall usclssic_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xb40000:
		case 0xb40002:
		case 0xb40004:
		case 0xb40006:
			return uclssic_trackball_read(address);

		case 0xb40010:
			return (DrvInputs[0] ^ 0xf0) | 0x0f;

		case 0xb40018:
			return DrvDips[1] & 0x0f;

		case 0xb4001a:
			return DrvDips[1] >> 4;

		case 0xb4001c:
			return DrvDips[0] & 0x0f;

		case 0xb4001e:
			return DrvDips[0] >> 4;
	}

	return 0;
}

static UINT8 __fastcall usclssic_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xb40000:
		case 0xb40001:
		case 0xb40002:
		case 0xb40003:
		case 0xb40004:
		case 0xb40005:
		case 0xb40006:
		case 0xb40007:
			return uclssic_trackball_read(address);

		//case 0xb40010:
		case 0xb40011:
			return (DrvInputs[0] ^ 0xf0) | 0x0f;

		case 0xb40018:
		case 0xb40019:
			return DrvDips[1] & 0x0f;

		case 0xb4001a:
		case 0xb4001b:
			return DrvDips[1] >> 4;

		case 0xb4001c:
		case 0xb4001d:
			return DrvDips[0] & 0x0f;

		case 0xb4001e:
		case 0xb4001f:
			return DrvDips[0] >> 4;
	}

	return 0;
}

//-----------------------------------------------------------------------------------------------------------------------------------
// calibr50

// Rotation-handler code
// Notes:
// rotate_gunpos - value in game's ram which depicts the rotational position
// 		of the hero
// nRotate		 - value returned to the game's inputs
// nRotateTarget - calculated position where rotate_gunpos needs to be
// Theory:
// Direction from joy is translated and a target is set, each frame (or every
// other, depending on game's requirements) we clock towards that target.

static INT32 nRotateTargetVSmemDistance;

static void RotateReset() {
	for (INT32 playernum = 0; playernum < 2; playernum++) {
		nRotate[playernum] = 0; // start out pointing straight up (0=up)
		nRotateTarget[playernum] = -1;
		nRotateTime[playernum] = 0;
		nRotateHoldInput[0] = nRotateHoldInput[1] = 0;
	}
}

static UINT32 RotationTimer(void) {
    return nCurrentFrame;
}

static void RotateRight(INT32 *v) {
	if (game_rotates == 2) { // downtown mode
		(*v)-=1;
		if (*v < 0) *v = 0xb;
	} else { // calibr50 mode
		(*v)-=(nRotateTargetVSmemDistance > 1) ? 4 : 1;
		if (*v < 0) *v = 0x3c;
	}
}

static void RotateLeft(INT32 *v) {
	if (game_rotates == 2) { // downtown mode
		(*v)+=1;
		if (*v > 0xb) *v = 0;
	} else { // calibr50 mode
		(*v)+=(nRotateTargetVSmemDistance > 1) ? 4 : 1;
		if (*v > 0x3c) *v = 0;
	}
}

static UINT8 Joy2Rotate(UINT8 *joy) { // ugly code, but the effect is awesome. -dink
	if (joy[0] && joy[2]) return 7;    // up left
	if (joy[0] && joy[3]) return 1;    // up right

	if (joy[1] && joy[2]) return 5;    // down left
	if (joy[1] && joy[3]) return 3;    // down right

	if (joy[0]) return 0;    // up
	if (joy[1]) return 4;    // down
	if (joy[2]) return 6;    // left
	if (joy[3]) return 2;    // right

	return 0xff;
}

static int dialRotation(INT32 playernum) {
    // p1 = 0, p2 = 1
	UINT8 player[2] = { 0, 0 };
	static UINT8 lastplayer[2][2] = { { 0, 0 }, { 0, 0 } };

    if ((playernum != 0) && (playernum != 1)) {
        bprintf(PRINT_NORMAL, _T("Strange Rotation address => %06X\n"), playernum);
        return 0;
    }
    if (playernum == 0) {
        player[0] = DrvFakeInput[0]; player[1] = DrvFakeInput[1];
    }
    if (playernum == 1) {
        player[0] = DrvFakeInput[2]; player[1] = DrvFakeInput[3];
    }

    if (player[0] && (player[0] != lastplayer[playernum][0] || (RotationTimer() > nRotateTime[playernum]+0xf))) {
		RotateLeft(&nRotate[playernum]);
		bprintf(PRINT_NORMAL, _T("Player %d Rotate Left => %06X\n"), playernum+1, nRotate[playernum]);
		nRotateTime[playernum] = RotationTimer();
		nRotateTarget[playernum] = -1;
    }

	if (player[1] && (player[1] != lastplayer[playernum][1] || (RotationTimer() > nRotateTime[playernum]+0xf))) {
        RotateRight(&nRotate[playernum]);
        bprintf(PRINT_NORMAL, _T("Player %d Rotate Right => %06X\n"), playernum+1, nRotate[playernum]);
        nRotateTime[playernum] = RotationTimer();
		nRotateTarget[playernum] = -1;
	}

	lastplayer[playernum][0] = player[0];
	lastplayer[playernum][1] = player[1];

	return (nRotate[playernum]);
}

static UINT8 *rotate_gunpos[2] = {NULL, NULL};
static UINT8 rotate_gunpos_multiplier = 1;

// Gun-rotation memory locations - do not remove this tag. - dink :)
// game     p1           p2           clockwise value in memory         multiplier
// downtown 0xffef90+1   0xfefd0+1    0 1 2 3 4 5 6 7                   1
//
// calibr50 0xff2500+3   0xff2520+7   0 1 2 3 4 5 6 7 8 9 a b c d e f   2
// ff4ede = 0xff = onplane
// p1 ff0e69 p2 ff0e89? rotate reg.

static void RotateSetGunPosRAM(UINT8 *p1, UINT8 *p2, UINT8 multiplier) {
	rotate_gunpos[0] = p1;
	rotate_gunpos[1] = p2;
	rotate_gunpos_multiplier = multiplier;
}

static INT32 get_distance(INT32 from, INT32 to) {
// this function finds the shortest way to get from "from" to "to", wrapping at 0 and 7
	INT32 countA = 0;
	INT32 countB = 0;
	INT32 fromtmp = from;// / rotate_gunpos_multiplier;
	INT32 totmp = to;// / rotate_gunpos_multiplier;

	while (1) {
		fromtmp++;
		countA++;
		if(fromtmp>((game_rotates == 2) ? 0x7 : 0xf)) fromtmp = 0;
		if(fromtmp == totmp || countA > 32) break;
	}

	fromtmp = from;// / rotate_gunpos_multiplier;
	totmp = to;// / rotate_gunpos_multiplier;

	while (1) {
		fromtmp--;
		countB++;
		if(fromtmp<0) fromtmp = ((game_rotates == 2) ? 0x7 : 0xf);
		if(fromtmp == totmp || countB > 32) break;
	}

	if (countA > countB) {
		nRotateTargetVSmemDistance = countB;
		return 1; // go negative
	} else {
		nRotateTargetVSmemDistance = countA;
		return 0; // go positive
	}
}

static UINT8 adjusted_rotate_gunpos(INT32 i)
{
	// calibr50: apply compensation while in the airplane
	if (game_rotates == 1 && Drv68KRAM[0x4ede] == 0xff) {
		// ff381c: plane's rotation (same orientation as player aka *rotate_gunpos[i])
		return (Drv68KRAM[0x381c] + *rotate_gunpos[i]) & 0x0f;
	}

	return *rotate_gunpos[i];
}

static void RotateDoTick() {
	if (nCurrentFrame&1) return; // limit rotation to every other frame to avoid confusing the game (comment v3.0)

	if (game_rotates == 1) { // calibr50 switcheroo
		if (Drv68KRAM[0x4ede] == 0xff) {
			// P1/P2 in the airplane
			RotateSetGunPosRAM(Drv68KRAM + (0x0e69-1), Drv68KRAM + (0x0e89-1), 1);
		} else {
			// P1/P2 normal.
			RotateSetGunPosRAM(Drv68KRAM + (0x2503-1), Drv68KRAM + (0x2527-1), 1);
		}
	}

	for (INT32 i = 0; i < 2; i++) {
		if (rotate_gunpos[i] && (nRotateTarget[i] != -1) && (nRotateTarget[i] != (adjusted_rotate_gunpos(i) & 0x0f))) {
			if (get_distance(nRotateTarget[i], adjusted_rotate_gunpos(i) & 0x0f)) {
				RotateLeft(&nRotate[i]);  // ++
			} else {
				RotateRight(&nRotate[i]); // --
			}
 // temp disabled!!!!!
 //   		bprintf(0, _T("p%X target %X mempos %X nRotate %X try %X.\n"), i, nRotateTarget[i], adjusted_rotate_gunpos(i) & 0x0f, nRotate[i], nRotateTry[i]);
			nRotateTry[i]++;
			if (nRotateTry[i] > 0xf) nRotateTarget[i] = -1; // don't get stuck in a loop if something goes horribly wrong here.
		} else {
			nRotateTarget[i] = -1;
		}
	}
}

static void ProcessAnalogInputs() {
	// converts analog inputs to something that the existing rotate logic can work with
	INT16 AnalogPorts[4] = { DrvAnalogPort1, DrvAnalogPort0, DrvAnalogPort3, DrvAnalogPort2 };

	if (game_rotates != 1) return;

	// clear fake inputs
	// Note: DrvFakeInput 6/10 - up, 7/11 - down, 8/12 - left, 9/13 - right
	for (int i = 6; i < 14; i++)
		DrvFakeInput[i] = 0;

	for (int i = 0; i < 2; i++) { // 1 (x,y) for each player
		// note: most thumbsticks return -1024 0 +1023
		// some analog joysticks & inputs return -0x8000 0 +0x7fff
		// atan2() needs -1 0 +1

		float y_axis = (ProcessAnalog(AnalogPorts[i*2 + 0], 0, INPUT_DEADZONE, 0x00, 0xff) - 128.0)/128.0;
		float x_axis = (ProcessAnalog(AnalogPorts[i*2 + 1], 0, INPUT_DEADZONE, 0x00, 0xff) - 128.0)/128.0;

		int deg = (atan2(-x_axis, -y_axis) * 180 / M_PI) - 360/16/2; // technically, on a scale from 0-f, "0" should be -11.25 to 11.25, and not 0 to 22.5.
		if (deg < 0) deg += 360;

		int g_val = deg * 16 / 360; // scale from 0-360 to 0-f
		g_val = 0xf - g_val; // invert so up-left is 0xf, instead of up-right
		if (!(x_axis == 0.0 && y_axis == 0.0)) { // we're not in deadzone -- changed below
			DrvFakeInput[6 + i*4] = g_val; // for autofire
			DrvFakeInput[7 + i*4] = 1; // if g_val is 0, we need to still register movement!
		}
	}
}

static void SuperJoy2Rotate() {
	UINT8 FakeDrvInputPort0[4] = { 0, 0, 0, 0 };
	UINT8 FakeDrvInputPort1[4] = { 0, 0, 0, 0 };
	UINT8 NeedsSecondStick[2] = { 0, 0 };

	// prepare for right-stick rotation
	// this is not especially readable though

	ProcessAnalogInputs();

	// check if udlr (or analog direction) is pressed for p1/p2
	for (INT32 i = 0; i < 2; i++) {
		for (INT32 n = 0; n < 4; n++) {
			UINT8* RotationInput = (!i) ? &FakeDrvInputPort0[0] : &FakeDrvInputPort1[0];
			RotationInput[n] = DrvFakeInput[6 + i*4 + n];
			NeedsSecondStick[i] |= RotationInput[n];
		}
	}

	for (INT32 i = 0; i < 2; i++) { // p1 = 0, p2 = 1
		if (!NeedsSecondStick[i])
			nAutoFireCounter[i] = 0;
		if (NeedsSecondStick[i]) { // we've got input from the second stick
			UINT8 rot;
			if (game_rotates == 1) {
				// calibr50 uses 16 directions
				rot = DrvFakeInput[6 + i*4]; // ProcessAnalogInputs() stores it here
			} else {
				rot = Joy2Rotate(((!i) ? &FakeDrvInputPort0[0] : &FakeDrvInputPort1[0]));
			}
			if (rot != 0xff) {
				nRotateTarget[i] = rot * rotate_gunpos_multiplier;
			}
			
			nRotateTry[i] = 0;

			if (~DrvDips[3] & 1) {
				// fire (calibr50) / auto-fire (downtown)
				if ((nAutoFireCounter[i]++ & 0x2) || (game_rotates == 1)) {
					DrvInputs[i] &= ~0x10;
				}
			}
		}
		else if (DrvFakeInput[4 + i]) { //  rotate-button had been pressed
			UINT8 rot = Joy2Rotate(((!i) ? &DrvJoy1[0] : &DrvJoy2[0]));
			if (rot != 0xff) {
				if (game_rotates == 1) {
					nRotateTarget[i] = rot * 2; // convert 8-way to 16-way (multiplier is 1 for the analog inputs)
				} else {
					nRotateTarget[i] = rot * rotate_gunpos_multiplier;
				}
			}
			//DrvInput[i] &= ~0xf; // cancel out directionals since they are used to rotate here.
			DrvInputs[i] = (DrvInputs[i] & ~0xf) | (nRotateHoldInput[i] & 0xf); // for midnight resistance! be able to duck + change direction of gun.
			nRotateTry[i] = 0;
		} else { // cache joystick UDLR if the rotate button isn't pressed.
			// This feature is for Midnight Resistance, if you are crawling on the
			// ground and need to rotate your gun WITHOUT getting up.
			nRotateHoldInput[i] = DrvInputs[i];
		}
	}

	RotateDoTick();
}

// end Rotation-handler

static UINT16 calibr50_input_read(INT32 offset)
{
	INT32 dir1 = dialRotation(0); 					// analog port
	INT32 dir2 = dialRotation(1);					// analog port

	switch (offset & 0x1e)
	{
		case 0x00:	return DrvInputs[0];		// p1
		case 0x02:	return DrvInputs[1];		// p2
		case 0x08:	return DrvInputs[2]^0xff^DrvDips[2];		// Coins
		case 0x10:	return (dir1 & 0xff);		// lower 8 bits of p1 rotation
		case 0x12:	return (dir1 >> 8);		// upper 4 bits of p1 rotation
		case 0x14:	return (dir2 & 0xff);		// lower 8 bits of p2 rotation
		case 0x16:	return (dir2 >> 8);		// upper 4 bits of p2 rotation
		case 0x18:	return 0xffff;			// ? (value's read but not used)
	}

	return 0;
}

static UINT16 __fastcall calibr50_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
		case 0x400001:
			watchdog = 0;
			return 0xffff;
	}

	if ((address & 0xfffffe0) == 0xa00000) {
		return calibr50_input_read(address);
	}

	SetaReadDips(0x600000)

	return 0;
}

static UINT8 __fastcall calibr50_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
		case 0x400001:
			watchdog = 0;
			return 0xff;

		case 0xb00000:
		case 0xb00001:
			return soundlatch2;
	}

	if ((address & 0xfffffe0) == 0xa00000) {
		return calibr50_input_read(address) >> ((~address & 1) << 3);
	}

	SetaReadDips(0x600000)

	return 0;
}

static void __fastcall calibr50_write_word(UINT32 address, UINT16 data)
{
	SetaVidRAMCtrlWriteWord(0, 0x800000)

	if ((address & ~1) == 0xb00000) {
		soundlatch = data;

		M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		SekRunEnd();

		return;
	}
	//bprintf(0, _T("ww: %X."), address);
}

static void __fastcall calibr50_write_byte(UINT32 address, UINT8 data)
{
	SetaVidRAMCtrlWriteByte(0, 0x800000)

	if ((address & ~1) == 0xb00000) {
		soundlatch = data;

		M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		SekRunEnd();

		return;
	}
	//bprintf(0, _T("wb: %X."), address);
}

//-----------------------------------------------------------------------------------------------------------------------------------
// downtown protection handler, m65c02 simulation (metafox, arbelester)

static UINT8 *downtown_protram;

static UINT8 __fastcall downtown_prot_read(UINT32 address)
{
	if (downtown_protram[0xf8] == 0xa3) {
		if (address >= 0x200100 && address <= 0x20010b) {
			char *waltz = "WALTZ0";
			return waltz[(address & 0x0f) / 2];
		}
	}

	return downtown_protram[(address & 0x1ff)^1];
}

// twineagle, downtown, calibr50, tndrcade, arbalester, metafox etc. uses these below

static UINT8 __fastcall twineagl_sharedram_read_byte(UINT32 address)
{
	return DrvShareRAM[(address&0xfff)>>1];
}

static UINT16 __fastcall twineagl_sharedram_read_word(UINT32 address)
{
	return DrvShareRAM[(address&0xfff)>>1]&0xff;
}

static void __fastcall twineagl_sharedram_write_word(UINT32 address, UINT16 data)
{
	DrvShareRAM[(address&0xfff)>>1] = data&0xff;
}

static void __fastcall twineagl_sharedram_write_byte(UINT32 address, UINT8 data)
{
	DrvShareRAM[(address&0xfff)>>1] = data;
}


//-----------------------------------------------------------------------------------------------------------------------------------
// crazy fight

static UINT16 __fastcall crazyfgt_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x610000:
		case 0x610001:
			return DrvInputs[0];

		case 0x610002:
		case 0x610003:
			return 0xffff;

		case 0x610004:
		case 0x610005:
			return DrvInputs[1];
	}

	SetaReadDips(0x630000)

	return 0;
}

static UINT8 __fastcall crazyfgt_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x610000:
		case 0x610001:
			return DrvInputs[0];

		case 0x610002:
		case 0x610003:
			return 0xff;

		case 0x610004:
		case 0x610005:
			return DrvInputs[1];
	}

	SetaReadDips(0x630000)

	return 0;
}

static void __fastcall crazyfgt_write_byte(UINT32 address, UINT8 data)
{
	SetaVidRAMCtrlWriteByte(1, 0x900000)
	SetaVidRAMCtrlWriteByte(0, 0x980000)

	switch (address)
	{
		case 0x650001:
		case 0x650003:
			BurnYM3812Write(0, (address & 2) >> 1, data);
		return;

		case 0x658000:
		case 0x658001:
			MSM6295Write(0, data);
		return;
	}
}

static void __fastcall crazyfgt_write_word(UINT32 address, UINT16 data)
{
	SetaVidRAMCtrlWriteWord(1, 0x900000)
	SetaVidRAMCtrlWriteWord(0, 0x980000)

	switch (address)
	{
		case 0x650000:
		case 0x650002:
			BurnYM3812Write(0, (address & 2) >> 1, data);
		return;

		case 0x658000:
		case 0x658001:
			MSM6295Write(0, data);
		return;

		case 0x620002: // nop
		return;
	}
}


//-----------------------------------------------------------------------------------------------------------------------------------
// 68k initializers

static void drgnunit68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x0bffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x700000, 0x7003ff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x900000, 0x903fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xd00000, 0xd00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xe00000, 0xe03fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xf00000, 0xf0ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM + 0x0010000,	0xffc000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,		drgnunit_write_word);
	SekSetWriteByteHandler(0,		drgnunit_write_byte);
	SekSetReadWordHandler(0,		drgnunit_read_word);
	SekSetReadByteHandler(0,		drgnunit_read_byte);

	SekMapHandler(1,			0x100000, 0x103fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void daioh68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x100000, 0x10ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM + 0x0010000,	0x700000, 0x7003ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x700400, 0x700fff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,		0x701000, 0x70ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x800000, 0x80ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x880000, 0x88ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM + 0x0020000,	0xa80000, 0xa803ff, MAP_WRITE); // nop out
	SekMapMemory(DrvSprRAM1,		0xb00000, 0xb13fff, MAP_RAM);
	SekSetWriteWordHandler(0,		daioh_write_word);
	SekSetWriteByteHandler(0,		daioh_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0xc00000, 0xc03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void daiohp68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM + 0x0010000,	0x700000, 0x7003ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x700400, 0x700fff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,		0x701000, 0x70ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x800000, 0x80ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x880000, 0x88ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM + 0x0020000,	0xa80000, 0xa803ff, MAP_WRITE); // nop out
	SekMapMemory(DrvSprRAM1,		0xb00000, 0xb13fff, MAP_RAM);
	SekSetWriteWordHandler(0,		daioh_write_word);
	SekSetWriteByteHandler(0,		daioh_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0xc00000, 0xc03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void wrofaero68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x21ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,		0x300000, 0x30ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x0010000,	0x700000, 0x7003ff, MAP_RAM); 
	SekMapMemory(DrvPalRAM,			0x700400, 0x700fff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x0011000,	0x701000, 0x70ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x800000, 0x80ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x880000, 0x88ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xb00000, 0xb13fff, MAP_RAM);
	SekSetWriteWordHandler(0,		daioh_write_word);
	SekSetWriteByteHandler(0,		daioh_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0xc00000, 0xc03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void msgundam68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x1fffff, MAP_ROM);

	for (INT32 i = 0; i < 0x80000; i+=0x10000) // mirrored
		SekMapMemory(Drv68KRAM,		0x200000+i, 0x20ffff+i, MAP_RAM);

	SekMapMemory(DrvPalRAM,			0x700400, 0x700fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0x800000, 0x800607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0x900000, 0x903fff, MAP_RAM);

	SekMapMemory(DrvVidRAM0,		0xa00000, 0xa0ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0xa80000, 0xa8ffff, MAP_RAM);
	SekSetWriteWordHandler(0,		msgundam_write_word);
	SekSetWriteByteHandler(0,		msgundam_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0xc00000, 0xc03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void umanclub68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x300000, 0x300fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xb00000, 0xb03fff, MAP_RAM);
	SekSetWriteWordHandler(0,		daioh_write_word);
	SekSetWriteByteHandler(0,		daioh_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0xc00000, 0xc03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void kamenrid68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x0010000,	0x700000, 0x7003ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x700400, 0x700fff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x0011000,	0x701000, 0x703fff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x800000, 0x807fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x880000, 0x887fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xb00000, 0xb07fff, MAP_RAM);
	SekSetWriteWordHandler(0,		madshark_write_word);
	SekSetWriteByteHandler(0,		madshark_write_byte);
	SekSetReadWordHandler(0,		kamenrid_read_word);
	SekSetReadByteHandler(0,		kamenrid_read_byte);

	SekMapHandler(1,			0xd00000, 0xd03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();

	{
		DrvGfxROM2 = DrvGfxROM1 + 0x80000;
		DrvROMLen[2] = DrvROMLen[1] = DrvROMLen[1] / 2;
		memcpy (DrvGfxROM2, DrvGfxROM1 + 0x40000, 0x40000);
	}
}

static void madshark68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x0010000,	0x700000, 0x7003ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x700400, 0x700fff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x0011000,	0x701000, 0x703fff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x800000, 0x807fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x880000, 0x887fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xb00000, 0xb07fff, MAP_RAM);
	SekSetWriteWordHandler(0,		madshark_write_word);
	SekSetWriteByteHandler(0,		madshark_write_byte);
	SekSetReadWordHandler(0,		madshark_read_word);
	SekSetReadByteHandler(0,		madshark_read_byte);

	SekMapHandler(1,			0xd00000, 0xd03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();

	{
		DrvGfxROM2 = (UINT8*)BurnMalloc(0x200000);
		DrvROMLen[1] = DrvROMLen[2] = 0x200000;

		memcpy (DrvGfxROM0 + 0x200000, DrvGfxROM0 + 0x000000, 0x100000);
		memmove (DrvGfxROM0 + 0x000000, DrvGfxROM0 + 0x100000, 0x200000); // dink??

		memcpy (DrvGfxROM2 + 0x000000, DrvGfxROM1 + 0x100000, 0x100000);
		memcpy (DrvGfxROM2 + 0x100000, DrvGfxROM1 + 0x300000, 0x100000);
		memcpy (DrvGfxROM1 + 0x100000, DrvGfxROM1 + 0x200000, 0x100000);
	}
}

static void thunderl68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x00ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x700000, 0x700fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xd00000, 0xd00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xe00000, 0xe07fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xffc000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,		thunderl_write_word);
	SekSetWriteByteHandler(0,		thunderl_write_byte);
	SekSetReadWordHandler(0,		thunderl_read_word);
	SekSetReadByteHandler(0,		thunderl_read_byte);

	SekMapHandler(1,			0x100000, 0x103fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void atehate68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x700000, 0x700fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0x900000, 0x9fffff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xe00000, 0xe03fff, MAP_RAM);
	SekSetWriteWordHandler(0,		thunderl_write_word);
	SekSetWriteByteHandler(0,		thunderl_write_byte);
	SekSetReadWordHandler(0,		thunderl_read_word);
	SekSetReadByteHandler(0,		thunderl_read_byte);

	SekMapHandler(1,			0x100000, 0x103fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void blockcar68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0xb00000, 0xb00fff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xc00000, 0xc03fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xe00000, 0xe00607 | 0x7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xf00000, 0xf05fff, MAP_RAM);
	SekSetWriteWordHandler(0,		thunderl_write_word);
	SekSetWriteByteHandler(0,		thunderl_write_byte);
	SekSetReadWordHandler(0,		madshark_read_word);
	SekSetReadByteHandler(0,		madshark_read_byte);

	SekMapHandler(1,			0xa00000, 0xa03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void zombraid68kInit()
{
	wrofaero68kInit();

	SekOpen(0);
	SekMapHandler(2,			0xf00000, 0xf00003, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (2,		zombraid_gun_read_word);
	SekSetReadByteHandler (2,		zombraid_gun_read_byte);
	SekSetWriteWordHandler(2,		zombraid_gun_write_word);
	SekSetWriteByteHandler(2,		zombraid_gun_write_byte);
	SekClose();

	memmove (DrvSndROM + 0x100000, DrvSndROM + 0x080000, 0x380000);
}

static void BlandiaGfxRearrange()
{
	INT32 rom_size = DrvROMLen[1];
	UINT8 *buf = (UINT8*)BurnMalloc(rom_size);

	UINT8 *rom = DrvGfxROM1 + 0x40000;

	if (rom_size == 0x100000) memmove (rom, rom + 0x40000, 0x80000); // blandia dink??

	for (INT32 rpos = 0; rpos < 0x80000/2; rpos++) {
		buf[rpos+0x40000] = rom[rpos*2];
		buf[rpos] = rom[rpos*2+1];
	}

	memcpy (rom, buf, 0x80000);

	rom = DrvGfxROM2 + 0x40000;

	if (rom_size == 0x100000) memmove (rom, rom + 0x40000, 0x80000); // blandia dink??

	for (INT32 rpos = 0; rpos < 0x80000/2; rpos++) {
		buf[rpos+0x40000] = rom[rpos*2];
		buf[rpos] = rom[rpos*2+1];
	}

	memcpy (rom, buf, 0x80000);

	DrvROMLen[1] = DrvROMLen[2] = 0xc0000;

	BurnFree (buf);
}

static void blandia68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x21ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,		0x300000, 0x30ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x0010000,	0x700000, 0x7003ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x700400, 0x700fff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x0011000,	0x701000, 0x70ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0x800000, 0x800607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0x900000, 0x903fff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0xb00000, 0xb0ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0xb80000, 0xb8ffff, MAP_RAM);
	SekSetWriteWordHandler(0,		daioh_write_word);
	SekSetWriteByteHandler(0,		daioh_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0xc00000, 0xc03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();

	memmove (Drv68KROM + 0x100000, Drv68KROM + 0x080000, 0x100000);
	memmove (DrvSndROM + 0x100000, DrvSndROM + 0x0c0000, 0x0c0000);

	BlandiaGfxRearrange();
}

static void blandiap68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x21ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,		0x300000, 0x30ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x0010000,	0x700000, 0x7003ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x700400, 0x700fff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x0011000,	0x701000, 0x70ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x800000, 0x80ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x880000, 0x88ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM  + 0x0020000, 	0xa80000, 0xa803ff, MAP_WRITE); //nop
	SekMapMemory(DrvSprRAM1,		0xb00000, 0xb03fff, MAP_RAM);
	SekSetWriteWordHandler(0,		daioh_write_word);
	SekSetWriteByteHandler(0,		daioh_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0xc00000, 0xc03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();

	memmove (Drv68KROM + 0x100000, Drv68KROM + 0x080000, 0x100000);

	// set up sound banks...
	{
		UINT8 *tmp = (UINT8*)BurnMalloc(0x240000);

		INT32 offsets[16] = {
			0x000000, 0x140000, 0x020000, 0x160000, 
			0x040000, 0x180000, 0x060000, 0x1a0000,
			0x080000, 0x1c0000, 0x0a0000, 0x1e0000,
			0x100000, 0x200000, 0x120000, 0x220000
		};

		for (INT32 i = 0; i < 16; i++) {
			memcpy (tmp + offsets[i], DrvSndROM + (i * 0x020000), 0x020000);
		}

		memcpy (DrvSndROM, tmp, 0x240000);

		BurnFree (tmp);
	}
}

static void oisipuzl68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x800000, 0x803fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x880000, 0x883fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xb00000, 0xb03fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xc00400, 0xc00fff, MAP_RAM);
	SekSetWriteWordHandler(0,		daioh_write_word);
	SekSetWriteByteHandler(0,		daioh_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0x700000, 0x703fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void pairlove68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvNVRAM,			0x800000, 0x8001ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xb00000, 0xb00fff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xc00000, 0xc03fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xe00000, 0xe00607 | 0x7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xf00000, 0xf0ffff, MAP_RAM);
	SekSetWriteWordHandler(0,		pairlove_write_word);
	SekSetWriteByteHandler(0,		pairlove_write_byte);
	SekSetReadWordHandler(0,		pairlove_read_word);
	SekSetReadByteHandler(0,		pairlove_read_byte);

	SekMapHandler(1,			0xa00000, 0xa03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void jjsquawkb68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x21ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,		0x300000, 0x30ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x0010000,	0x700000, 0x70b3ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x70b400, 0x70bfff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x001c000,	0x70c000, 0x70ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x800000, 0x803fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x804000, 0x807fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1 + 0x4000,	0x884000, 0x88ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa0a000, 0xa0a607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xb0c000, 0xb0ffff, MAP_RAM);
	SekSetWriteWordHandler(0,		daioh_write_word);
	SekSetWriteByteHandler(0,		daioh_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0xc00000, 0xc03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();

	BurnLoadRom(Drv68KROM + 0x0000000, 0, 1);
	BurnLoadRom(Drv68KROM + 0x0100000, 1, 1);
	
	DrvGfxROM2 = DrvGfxROM1 + 0x400000;
	DrvROMLen[1] = DrvROMLen[2] = 0x200000;

	memcpy (DrvGfxROM2 + 0x000000, DrvGfxROM1 + 0x100000, 0x100000);
	memcpy (DrvGfxROM2 + 0x100000, DrvGfxROM1 + 0x300000, 0x100000);
	memcpy (DrvGfxROM1 + 0x100000, DrvGfxROM1 + 0x200000, 0x100000);
}

static void simpsonjr68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x21ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,		0x300000, 0x30ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x0010000,	0x700000, 0x70b3ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x70b400, 0x70bfff, MAP_RAM);
	SekMapMemory(Drv68KRAM2 + 0x001c000,	0x70c000, 0x70ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x800000, 0x803fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x804000, 0x807fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1 + 0x4000,	0x884000, 0x88ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa0a000, 0xa0a607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xb0c000, 0xb0ffff, MAP_RAM);
	SekSetWriteWordHandler(0,		daioh_write_word);
	SekSetWriteByteHandler(0,		daioh_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0xc00000, 0xc03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void extdwnhl68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x23ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x600400, 0x600fff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,		0x601000, 0x610bff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x800000, 0x80ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x880000, 0x88ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xb00000, 0xb13fff, MAP_RAM);
	SekSetWriteWordHandler(0,		daioh_write_word);
	SekSetWriteByteHandler(0,		daioh_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0xe00000, 0xe03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();

	memcpy (DrvSndROM + 0x100000, DrvSndROM + 0x000000, 0x080000); // temp storage

	// swap halves of sound rom (extdwnhl & sokonuke)
	memcpy (DrvSndROM + 0x000000, DrvSndROM + 0x080000, 0x080000);
	memcpy (DrvSndROM + 0x080000, DrvSndROM + 0x100000, 0x080000);
}

static void krzybowl68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvNVRAM,			0x800000, 0x8001ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xb00000, 0xb003ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xc00000, 0xc03fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xe00000, 0xe00607 | 0x7ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xf00000, 0xf0ffff, MAP_RAM);
	SekSetWriteWordHandler(0,		thunderl_write_word);
	SekSetWriteByteHandler(0,		thunderl_write_byte);
	SekSetReadWordHandler(0,		madshark_read_word);
	SekSetReadByteHandler(0,		madshark_read_byte);

	SekMapHandler(1,			0xa00000, 0xa03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();
}

static void __fastcall triplfun_sound_write_byte(UINT32 address, UINT8 data)
{
	if ((address & ~1) == 0x500006) {
		MSM6295Write(0, data);
	}
}

static UINT8 __fastcall triplfun_sound_read_byte(UINT32)
{
	return MSM6295Read(0);
}

static void triplfun68kInit()
{
	oisipuzl68kInit();

	SekOpen(0);
	SekMapHandler(2,			0x500006, 0x500007, MAP_READ | MAP_WRITE);
//	SekSetReadWordHandler (2,		triplfun_sound_read_word);
	SekSetReadByteHandler (2,		triplfun_sound_read_byte);
//	SekSetWriteWordHandler(2,		triplfun_sound_write_word);
	SekSetWriteByteHandler(2,		triplfun_sound_write_byte);
	SekClose();

	MSM6295Exit(0);
	MSM6295Init(0, 792000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	memcpy (Drv68KROM + 0x100000, Drv68KROM + 0x080000, 0x080000);
	memset (Drv68KROM + 0x080000, 0, 0x080000);
}

static void utoukond68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x700400, 0x700fff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x800000, 0x803fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x880000, 0x883fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xb00000, 0xb03fff, MAP_RAM);
	SekSetWriteWordHandler(0,		daioh_write_word);
	SekSetWriteByteHandler(0,		daioh_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0xc00000, 0xc00001, MAP_WRITE);
	SekSetWriteWordHandler(1,		wiggie_sound_write_word);
	SekSetWriteByteHandler(1,		wiggie_sound_write_byte);
	SekClose();

	has_z80 = 1;
	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xdfff, 0, DrvSubROM);
	ZetMapArea(0x0000, 0xdfff, 2, DrvSubROM);
	ZetMapArea(0xe000, 0xefff, 0, DrvSubRAM);
	ZetMapArea(0xe000, 0xefff, 1, DrvSubRAM);
	ZetMapArea(0xe000, 0xefff, 2, DrvSubRAM);
	ZetSetWriteHandler(utoukond_sound_write);
	ZetSetReadHandler(utoukond_sound_read);
	ZetSetOutHandler(utoukond_sound_write_port);
	ZetSetInHandler(utoukond_sound_read_port);
	ZetClose();

	for (INT32 i = 0; i < 0x400000; i++) DrvGfxROM0[i] ^= 0xff;
}

static void Wiggie68kDecode()
{
	UINT8 *tmp = Drv68KRAM;

	for (INT32 i = 0; i < 0x20000; i+=16) {
		for (INT32 j = 0; j < 16; j++) {
			tmp[j] = Drv68KROM[i+((j & 1) | ((j & 2) << 2) | ((j & 0x0c) >> 1))];
		}
		memcpy (Drv68KROM + i, tmp, 0x10);
	}
}

static void wiggie68kInit()
{
	thunderl68kInit();

	SekOpen(0);
	SekMapMemory(Drv68KRAM + 0x80000, 0x100000, 0x103fff, MAP_READ); // nop

	SekMapHandler(2,			0xb00008, 0xb00009, MAP_WRITE);
	SekSetWriteWordHandler(2,		wiggie_sound_write_word);
	SekSetWriteByteHandler(2,		wiggie_sound_write_byte);
	SekClose();

	Wiggie68kDecode();

	has_z80 = 1;
	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvSubROM);
	ZetMapArea(0x0000, 0x7fff, 2, DrvSubROM);
	ZetMapArea(0x8000, 0x87ff, 0, DrvSubRAM);
	ZetMapArea(0x8000, 0x87ff, 1, DrvSubRAM);
	ZetMapArea(0x8000, 0x87ff, 2, DrvSubRAM);
	ZetSetWriteHandler(wiggie_sound_write);
	ZetSetReadHandler(wiggie_sound_read);
	ZetClose();
}

static UINT16 downtown_input_read(INT32 offset)
{
	INT32 dir1 = dialRotation(0); 				// analog port
	INT32 dir2 = dialRotation(1);				// analog port

	dir1 = (~ (0x800 >> dir1)) & 0xfff;
	dir2 = (~ (0x800 >> dir2)) & 0xfff;

	switch (offset)
	{
		case 0: return (DrvInputs[2] & 0xf0) + (dir1 >> 8);  // upper 4 bits of p1 rotation + coins
		case 1: return (dir1 & 0xff);                   // lower 8 bits of p1 rotation
		case 2: return DrvInputs[0];    // p1
		case 3: return 0xff;                            // ?
		case 4: return (dir2 >> 8);                     // upper 4 bits of p2 rotation + ?
		case 5: return (dir2 & 0xff);                   // lower 8 bits of p2 rotation
		case 6: return DrvInputs[1];    // p2
		case 7: return 0xff;                            // ?
	}

	return 0;
}

static void m65c02_sub_bankswitch(UINT8 d)
{
	m65c02_bank = d;

	M6502MapMemory(DrvSubROM + 0xc000 + ((m65c02_bank >> 4) * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void downtown_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x1000:
			m65c02_sub_bankswitch(data);
			return;
	}
}

static UINT8 downtown_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x0800: return soundlatch;
		case 0x0801: return soundlatch2;
		case 0x1000:
		case 0x1001:
		case 0x1002:
		case 0x1003:
		case 0x1004:
		case 0x1005:
		case 0x1006:
		case 0x1007: return downtown_input_read(address&7);
	}

	return 0;
}

static void downtown68kInit()
{
	downtown_protram = DrvNVRAM;

	memset(DrvNVRAM, 0xff, 0x400);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x09ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x700000, 0x7003ff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x900000, 0x903fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xd00000, 0xd00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xe00000, 0xe03fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xf00000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,		downtown_write_word);
	SekSetWriteByteHandler(0,		downtown_write_byte);
	SekSetReadWordHandler(0,		daioh_read_word);
	SekSetReadByteHandler(0,		daioh_read_byte);

	SekMapHandler(1,			0x100000, 0x103fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);

	SekMapMemory(downtown_protram,		0x200000, 0x2003ff, MAP_WRITE);
	SekMapHandler(2,			0x200000, 0x2003ff, MAP_READ);
	SekSetReadByteHandler (2,		downtown_prot_read);

	SekMapHandler(3,			0xb00000, 0xb00fff, MAP_READ | MAP_WRITE);
	SekSetReadByteHandler (3,		twineagl_sharedram_read_byte);
	SekSetReadWordHandler (3,		twineagl_sharedram_read_word);
	SekSetWriteWordHandler(3,		twineagl_sharedram_write_word);
	SekSetWriteByteHandler(3,		twineagl_sharedram_write_byte);
	SekClose();

	if (strstr(BurnDrvGetTextA(DRV_NAME), "downtown")) {
		BurnLoadRom(DrvSubROM + 0x0004000, 4, 1);
		BurnLoadRom(DrvSubROM + 0x000c000, 4, 1);

		M6502Init(0, TYPE_M65C02);
		M6502Open(0);
		M6502MapMemory(DrvSubRAM,	        0x0000, 0x01ff, MAP_RAM);
		M6502MapMemory(DrvShareRAM,	        0x5000, 0x57ff, MAP_RAM);
		M6502MapMemory(DrvSubROM + 0x7000,	0x7000, 0x7fff, MAP_ROM);
		M6502MapMemory(DrvSubROM + 0xc000,	0x8000, 0xbfff, MAP_ROM); // bank default
		M6502MapMemory(DrvSubROM + 0xc000,	0xc000, 0xffff, MAP_ROM);
		M6502SetWriteHandler(downtown_sub_write);
		M6502SetReadHandler(downtown_sub_read);
		M6502Close();
		m65c02_mode = 1;

		game_rotates = 2; // 2 == downtown mode.
		RotateSetGunPosRAM(Drv68KRAM + (0xfef90+1), Drv68KRAM + (0xfefd0+1), 1);
	}
}

static UINT8 metafox_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x0800: return soundlatch;
		case 0x0801: return soundlatch2;
		case 0x1000: return DrvInputs[2];
		case 0x1002: return DrvInputs[0];
		case 0x1006: return DrvInputs[1];
	}

	return 0;
}

static void metafox68kInit()
{
	downtown68kInit();

	SekOpen(0);
	SekMapHandler(3,			0xb00000, 0xb00fff, MAP_READ | MAP_WRITE);
	SekSetReadByteHandler (3,		twineagl_sharedram_read_byte);
	SekSetReadWordHandler (3,		twineagl_sharedram_read_word);
	SekSetWriteWordHandler(3,		twineagl_sharedram_write_word);
	SekSetWriteByteHandler(3,		twineagl_sharedram_write_byte);
	SekClose();

	if (X1010_Arbalester_Mode) {
		BurnLoadRom(DrvSubROM + 0x0006000, 2, 1);
		memcpy(DrvSubROM + 0x000a000, DrvSubROM + 0x0006000, 0x4000);
		memcpy(DrvSubROM + 0x000e000, DrvSubROM + 0x0006000, 0x2000);
	} else {
		BurnLoadRom(DrvSubROM + 0x0006000, 4, 1);
		memcpy(DrvSubROM + 0x0008000, DrvSubROM + 0x0006000, 0x2000);
		memcpy(DrvSubROM + 0x000a000, DrvSubROM + 0x0006000, 0x2000);
		memcpy(DrvSubROM + 0x000c000, DrvSubROM + 0x0006000, 0x2000);
		memcpy(DrvSubROM + 0x000e000, DrvSubROM + 0x0006000, 0x2000);
	}

	M6502Init(0, TYPE_M65C02);
	M6502Open(0);
	M6502MapMemory(DrvSubRAM,	        0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvShareRAM,	        0x5000, 0x57ff, MAP_RAM);
	M6502MapMemory(DrvSubROM + 0x7000,	0x7000, 0x7fff, MAP_ROM);
	M6502MapMemory(DrvSubROM + 0xc000,	0x8000, 0xbfff, MAP_ROM); // bank default
	M6502MapMemory(DrvSubROM + 0xc000,	0xc000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(downtown_sub_write);
	M6502SetReadHandler(metafox_sub_read);
	M6502Close();
	m65c02_mode = 1;

}

static void tndrcade_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000: BurnYM2203Write(0, 0, data); return;
		case 0x2001: BurnYM2203Write(0, 1, data); return;
		case 0x3000: BurnYM3812Write(0, 0, data); return;
		case 0x3001: BurnYM3812Write(0, 1, data); return;
		case 0x1000: m65c02_sub_bankswitch(data); return;
	}
}

static UINT8 tndrcade_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x0800: return 0xff;
		case 0x1000: return DrvInputs[0];
		case 0x1001: return DrvInputs[1];
		case 0x1002: return DrvInputs[2];
		case 0x2000: return BurnYM2203Read(0, 0);
		case 0x2001: return BurnYM2203Read(0, 1);
	}

	return 0;
}

static void tndrcade68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x380000, 0x3803ff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0x600000, 0x600607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvShareRAM,		0xa00000, 0xa00fff, MAP_WRITE); // m65c02 not emulated, simulate instead
	SekMapMemory(DrvSprRAM1,		0xc00000, 0xc03fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xe00000, 0xe03fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xffc000, 0xffffff, MAP_RAM);

	SekMapHandler(3,			0xa00000, 0xa00fff, MAP_READ | MAP_WRITE);
	SekSetReadByteHandler (3,		twineagl_sharedram_read_byte);
	SekSetReadWordHandler (3,		twineagl_sharedram_read_word);
	SekSetWriteWordHandler(3,		twineagl_sharedram_write_word);
	SekSetWriteByteHandler(3,		twineagl_sharedram_write_byte);
	SekClose();

	BurnLoadRom(DrvSubROM + 0x0004000, 4, 1);
	BurnLoadRom(DrvSubROM + 0x000c000, 4, 1);

	M6502Init(0, TYPE_M65C02);
	M6502Open(0);
	M6502MapMemory(DrvSubRAM,	        0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvShareRAM,	        0x5000, 0x57ff, MAP_RAM);
	M6502MapMemory(DrvSubROM + 0x6000,	0x6000, 0x7fff, MAP_ROM);
	//M6502MapMemory(DrvSubROM + 0xc000,	0x8000, 0xbfff, MAP_ROM); // bank default
	M6502MapMemory(DrvSubROM + 0xc000,	0xc000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(tndrcade_sub_write);
	M6502SetReadHandler(tndrcade_sub_read);
	M6502Close();
	m65c02_mode = 1;

}

static void kiwame68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0x800000, 0x803fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xb00000, 0xb003ff, MAP_RAM);
	SekMapMemory(DrvNVRAM,			0xfffc00, 0xffffff, MAP_WRITE);

	SekSetReadWordHandler(0,		kiwame_read_word);
	SekSetReadByteHandler(0,		kiwame_read_byte);

	SekMapHandler(1,			0xc00000, 0xc03fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();

	{
		// give the game proper vectors
		*((UINT16 *)(Drv68KROM + 0x00064)) = 0x0000;
		*((UINT16 *)(Drv68KROM + 0x00066)) = 0x0dca;

		// get past backup ram error 
		// the game never actually tests it before failing??
		*((UINT16 *)(Drv68KROM + 0x136d2)) = 0x6052;
	}
}

static UINT8 __fastcall twineagle_extram_read_byte(UINT32 address)
{
	return DrvNVRAM[address & 0x3fe];
}

static UINT8 twineagl_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x0800: return soundlatch;
		case 0x0801: return soundlatch2;
		case 0x1000: return DrvInputs[0];
		case 0x1001: return DrvInputs[1];
		case 0x1002: return DrvInputs[2];
	}

	return 0;
}

static void twineagle68kInit()
{
	downtown68kInit();

	SekOpen(0);
	SekMapMemory(DrvNVRAM,			0x200000, 0x2003ff, MAP_WRITE);
	SekMapHandler(2,			0x200000, 0x2003ff, MAP_READ);
	SekSetReadByteHandler (2,		twineagle_extram_read_byte);

	SekMapHandler(3,			0xb00000, 0xb00fff, MAP_READ | MAP_WRITE);
	SekSetReadByteHandler (3,		twineagl_sharedram_read_byte);
	SekSetReadWordHandler (3,		twineagl_sharedram_read_word);
	SekSetWriteWordHandler(3,		twineagl_sharedram_write_word);
	SekSetWriteByteHandler(3,		twineagl_sharedram_write_byte);
	SekClose();

	BurnByteswap(Drv68KROM, 0x80000);

	BurnLoadRom(DrvSubROM + 0x0006000, 1, 1);
	memcpy(DrvSubROM + 0x0008000, DrvSubROM + 0x0006000, 0x2000);
	memcpy(DrvSubROM + 0x000a000, DrvSubROM + 0x0006000, 0x2000);
	memcpy(DrvSubROM + 0x000c000, DrvSubROM + 0x0006000, 0x2000);
	memcpy(DrvSubROM + 0x000e000, DrvSubROM + 0x0006000, 0x2000);

	M6502Init(0, TYPE_M65C02);
	M6502Open(0);
	M6502MapMemory(DrvSubRAM,	        0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvShareRAM,	        0x5000, 0x57ff, MAP_RAM);
	M6502MapMemory(DrvSubROM + 0x7000,	0x7000, 0x7fff, MAP_ROM);
	M6502MapMemory(DrvSubROM + 0xc000,	0x8000, 0xbfff, MAP_ROM); // bank default
	M6502MapMemory(DrvSubROM + 0xc000,	0xc000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(downtown_sub_write);
	M6502SetReadHandler(twineagl_sub_read);
	M6502Close();
	m65c02_mode = 1;
}

static void crazyfgt68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x400000, 0x40ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x640400, 0x640fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x800000, 0x803fff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x880000, 0x883fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xa00000, 0xa00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xb00000, 0xb03fff, MAP_RAM);
	SekSetWriteWordHandler(0,		crazyfgt_write_word);
	SekSetWriteByteHandler(0,		crazyfgt_write_byte);
	SekSetReadWordHandler(0,		crazyfgt_read_word);
	SekSetReadByteHandler(0,		crazyfgt_read_byte);
	SekClose();

	MSM6295Exit(0);
	MSM6295Init(0, 4433619 / 4 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	MSM6295SetBank(0, DrvSndROM, 0, 0x3ffff);

	// Patch protection
	*((UINT16*)(Drv68KROM + 0x1078)) = 0x4e71;

	BlandiaGfxRearrange(); // fix bg tiles
}

static void calibr50_sub_write(UINT16 address, UINT8 data)
{
	if (address <= 0x1fff) { // x1_010
		setaSoundRegWriteByte8bit(address, data);
		return;
	}

	switch (address)
	{
		case 0x4000:
			m65c02_sub_bankswitch(data);
			return;

		case 0xc000:
			{
				soundlatch2 = data;
				M6502ReleaseSlice();
				return;
			}
	}
}

static UINT8 calibr50_sub_read(UINT16 address)
{
	if (address <= 0x1fff) { // x1_010
		return x1010_sound_read(address);
	}

	switch (address)
	{
		case 0x4000: {
			return soundlatch;
		}
	}

	return 0;
}

static void calibr5068kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x09ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM2,		0x200000, 0x200fff, MAP_RAM);
	SekMapMemory(Drv68KRAM3,		0xc00000, 0xc000ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x700000, 0x7003ff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x900000, 0x904fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xd00000, 0xd00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xe00000, 0xe03fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xff0000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,		calibr50_write_word);
	SekSetWriteByteHandler(0,		calibr50_write_byte);
	SekSetReadWordHandler(0,		calibr50_read_word);
	SekSetReadByteHandler(0,		calibr50_read_byte);
	SekClose();

	// m65c02 sound...
	M6502Init(0, TYPE_M65C02);
	M6502Open(0);
	M6502MapMemory(DrvSubROM,	        0xC000, 0xffff, MAP_ROM);
	M6502MapMemory(DrvSubROM + 0x4000,	0x8000, 0xbfff, MAP_ROM);
	BurnLoadRom(DrvSubROM + 0x4000, 4, 1);
	BurnLoadRom(DrvSubROM + 0xc000, 4, 1);
	M6502SetWriteHandler(calibr50_sub_write);
	M6502SetReadHandler(calibr50_sub_read);
	M6502Close();
	m65c02_mode = 1;

	RotateSetGunPosRAM(Drv68KRAM + (0x2503-1), Drv68KRAM + (0x2527-1), 1);
	game_rotates = 1;
}

static void usclssic68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvSprRAM0,		0x800000, 0x800607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xb00000, 0xb003ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xc00000, 0xc03fff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0xd00000, 0xd04fff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,		0xe00000, 0xe00fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xff0000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,		usclssic_write_word);
	SekSetWriteByteHandler(0,		usclssic_write_byte);
	SekSetReadWordHandler(0,		usclssic_read_word);
	SekSetReadByteHandler(0,		usclssic_read_byte);
	SekClose();

	// m65c02 sound...
	M6502Init(0, TYPE_M65C02);
	M6502Open(0);
	M6502MapMemory(DrvSubROM,	        0xC000, 0xffff, MAP_ROM);
	M6502MapMemory(DrvSubROM + 0x4000,	0x8000, 0xbfff, MAP_ROM);
	BurnLoadRom(DrvSubROM + 0x4000, 4, 1);
	BurnLoadRom(DrvSubROM + 0xc000, 4, 1);
	M6502SetWriteHandler(calibr50_sub_write);
	M6502SetReadHandler(calibr50_sub_read);
	M6502Close();
	m65c02_mode = 1;
}

//-----------------------------------------------------------------------------------------------------------------------------------

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0x20, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static UINT8 DrvYM2203ReadPortA(UINT32)
{
	return DrvDips[1];
}

static UINT8 DrvYM2203ReadPortB(UINT32)
{
	return DrvDips[0];
}

static INT32 DrvGfxDecode(INT32 type, UINT8 *gfx, INT32 num)
{
	DrvGfxTransMask[num] = NULL;

	INT32 len = DrvROMLen[num];
	if (DrvROMLen[num] == 0) DrvGfxMask[num] = 1; // no divide by 0
	if (len == 0 || type == -1) return 0;

	INT32 Plane0[4]  = { ((len * 8) / 2) + 8, ((len * 8) / 2) + 0, 8, 0 };
	INT32 XOffs0[16] = { 0,1,2,3,4,5,6,7, 128,129,130,131,132,133,134,135 };
	INT32 YOffs0[16] = { 0*16,1*16,2*16,3*16,4*16,5*16,6*16,7*16, 16*16,17*16,18*16,19*16,20*16,21*16,22*16,23*16 };

	INT32 Plane1[4]  = { 0, 4, ((len * 8) / 2) + 0, ((len * 8) / 2) + 4 };
	INT32 Plane1a[6] = { (((len * 8) / 3) * 0) + 0, (((len * 8) / 3) * 0) + 4, (((len * 8) / 3) * 1) + 0, (((len * 8) / 3) * 1) + 4, (((len * 8) / 3) * 2) + 0, (((len * 8) / 3) * 2) + 4 };
	INT32 XOffs1[16] = { 128+64,128+65,128+66,128+67, 128+0,128+1,128+2,128+3, 8*8+0,8*8+1,8*8+2,8*8+3, 0,1,2,3 };
	INT32 YOffs1[16] = { 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8, 32*8,33*8,34*8,35*8,36*8,37*8,38*8,39*8 };

	INT32 Plane2[6]  = { ((len * 8) / 2)+0*4, ((len * 8) / 2)+1*4, 2*4,3*4,0*4,1*4 };
	INT32 XOffs2[16] = { 256+128,256+129,256+130,256+131, 256+0,256+1,256+2,256+3,	 128,129,130,131, 0,1,2,3 };
	INT32 YOffs2[16] = { 0*16,1*16,2*16,3*16,4*16,5*16,6*16,7*16, 32*16,33*16,34*16,35*16,36*16,37*16,38*16,39*16 };

	INT32 Plane3[4]  = { ((len * 8) / 4) * 0, ((len * 8) / 4) * 1, ((len * 8) / 4) * 2, ((len * 8) / 4) * 3 };
	INT32 XOffs3[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 64, 65, 66, 67, 68, 69, 70, 71 };
	INT32 YOffs3[16] = { 0, 8, 16, 24, 32, 40, 48, 56, 128+0, 128+8, 128+16, 128+24, 128+32, 128+40, 128+48, 128+56 };
	INT32 YOffs3a[16]= { 0*8, 16*8, 4*8, 20*8,  2*8, 18*8, 6*8, 22*8, 1*8, 17*8, 5*8, 21*8,  3*8, 19*8, 7*8, 23*8 }; // wiggie
	INT32 YOffs3b[16]= { 0*8, 2*8,  16*8, 18*8,  1*8, 3*8, 17*8, 19*8,  4*8, 6*8, 20*8, 22*8, 5*8, 7*8,21*8, 23*8 }; // superbar

	UINT8 *tmp = (UINT8*)BurnMalloc(len);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, gfx, len);

	switch (type)
	{
		case 0: // layout_planes_2roms
			GfxDecode((len * 2) / (16 * 16), 4, 16, 16, Plane0, XOffs0, YOffs0, 0x200, tmp, gfx);
			DrvGfxMask[num] = (len * 2) / (16 * 16);
			ColorDepths[num] = 4;
		break;

		case 1: // layout_planes_2roms_split
			GfxDecode((len * 2) / (16 * 16), 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, gfx);
			DrvGfxMask[num] = (len * 2) / (16 * 16);
			ColorDepths[num] = 4;
		break;

		case 2: // layout_packed
			GfxDecode((len * 2) / (16 * 16), 4, 16, 16, Plane2 + 2, XOffs2, YOffs2, 0x400, tmp, gfx);
			DrvGfxMask[num] = (len * 2) / (16 * 16);
			ColorDepths[num] = 4;
		break;

		case 3: // layout_packed_6bits_2roms
			GfxDecode((len * 1) / (16 * 16), 6, 16, 16, Plane2 + 0, XOffs2, YOffs2, 0x400, tmp, gfx);
			DrvGfxMask[num] = (len * 1) / (16 * 16);
			ColorDepths[num] = 6; //handled like 4
		break;

		case 4: // layout_packed_6bits_3roms
			GfxDecode(((len * 8)/6) / (16 * 16), 6, 16, 16, Plane1a, XOffs1, YOffs1, 0x200, tmp, gfx);
			DrvGfxMask[num] = ((len * 8)/6) / (16 * 16);
			ColorDepths[num] = 6; //handled like 4
		break;

		case 5: // layout_planes
			GfxDecode((len * 2) / (16 * 16), 4, 16, 16, Plane3, XOffs3, YOffs3 , 0x100, tmp, gfx);
			DrvGfxMask[num] = (len * 2) / (16 * 16);
			ColorDepths[num] = 4;
		break;

		case 6: // wiggie_layout
			GfxDecode((len * 2) / (16 * 16), 4, 16, 16, Plane3, XOffs3, YOffs3a, 0x100, tmp, gfx);
			DrvGfxMask[num] = (len * 2) / (16 * 16);
			ColorDepths[num] = 4;
		break;

		case 7: // superbar_layout
			GfxDecode((len * 2) / (16 * 16), 4, 16, 16, Plane3, XOffs3, YOffs3b, 0x100, tmp, gfx);
			DrvGfxMask[num] = (len * 2) / (16 * 16);
			ColorDepths[num] = 4;
		break;
	}
	
	BurnFree (tmp);

	{
		INT32 size = DrvGfxMask[num];

		DrvGfxTransMask[num] = (UINT8*)BurnMalloc(size);

		for (INT32 i = 0; i < size << 8; i += (1 << 8)) {
			DrvGfxTransMask[num][i >> 8] = 1; // transparent
			for (INT32 j = 0; j < (1 << 8); j++) {
				if (gfx[i + j]) {
					DrvGfxTransMask[num][i >> 8] = 0;
					break;
				}
			}
		}
	}

	return 0;
}

static INT32 DrvLoadRoms(INT32 bload)
{
	char* pRomName;
	struct BurnRomInfo ri, rh;

	UINT8 *LoadPrg[2] = { Drv68KROM, DrvSubROM };
	UINT8 *LoadGfx[5] = { DrvGfxROM0, DrvGfxROM1, DrvGfxROM2, DrvSndROM, DrvColPROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i + 0);
		BurnDrvGetRomInfo(&rh, i + 1);

		if ((ri.nType & 7) < 3 && (ri.nType & 7)) {
			INT32 type = (ri.nType - 1) & 1;

			if ((rh.nType & 7) == 1 && (rh.nLen == ri.nLen)) {
				if (bload) if (BurnLoadRom(LoadPrg[type] + 0, i + 1, 2)) return 1;
				if (bload) if (BurnLoadRom(LoadPrg[type] + 1, i + 0, 2)) return 1;
				LoadPrg[type] += ri.nLen * 2;
				i++; // loading two roms
			} else {
				if (bload) if (BurnLoadRom(LoadPrg[type], i, 1)) return 1;
				LoadPrg[type] += ri.nLen;
			}
			continue;
		}

		if ((ri.nType & 7) == 3 || (ri.nType & 7) == 4 || (ri.nType & 7) == 5 || (ri.nType & 7) == 6 || (ri.nType & 7) == 7) { // gfx, snd, colprom
			INT32 type = (ri.nType & 7) - 3;

			if (ri.nType & 8) { // interleaved...
				if (bload) if (BurnLoadRom(LoadGfx[type] + 0, i + 0, 2)) return 1;

				if ((ri.nType & 16) == 0) { // zingzap
					if (bload) if (BurnLoadRom(LoadGfx[type] + 1, i + 1, 2)) return 1;
					i++;
				}

				LoadGfx[type] += ri.nLen * 2;
			} else {
				if (bload) if (BurnLoadRom(LoadGfx[type], i, 1)) return 1;
				LoadGfx[type] += ri.nLen;
			}

			continue;
		}	
	}

	if (bload == 0) {
		DrvROMLen[0] = LoadGfx[0] - DrvGfxROM0;
		DrvROMLen[1] = LoadGfx[1] - DrvGfxROM1;
		DrvROMLen[2] = LoadGfx[2] - DrvGfxROM2;
		DrvROMLen[3] = LoadGfx[3] - DrvSndROM;
		DrvROMLen[4] = LoadGfx[4] - DrvColPROM;
	}

	return 0;
}

static INT32 DrvDoReset(INT32 ram)
{
	if (ram) memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	if (has_z80) {
		ZetOpen(0); // wiggie, utoukond, superbar
		ZetReset();
		ZetClose();
	}

	if (m65c02_mode) {
		M6502Open(0);
		M6502Reset();
		m65c02_sub_bankswitch(0);
		M6502Close();
		soundlatch = 0;
		soundlatch2 = 0;
		sub_ctrl_data = 0;
	}

	x1010Reset();
	MSM6295Reset(0);
	BurnYM3438Reset();
	BurnYM3812Reset();
	if (has_2203) {
		BurnYM2203Reset();
	}

	if (game_rotates)
		RotateReset();

	HiscoreReset();

	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x200000;
	DrvSubROM		= Next; Next += 0x050000;

	DrvGfxROM2		= Next; Next += DrvROMLen[2] * 2;
	DrvGfxROM1		= Next; Next += DrvROMLen[1] * 2;
	DrvGfxROM0		= Next; Next += DrvROMLen[0] * 2;

	DrvColPROM		= Next; Next += 0x000800;

	MSM6295ROM		= Next;
	X1010SNDROM		= Next;
	DrvSndROM		= Next; Next += DrvROMLen[3] + 0x200000; // for banking

	Palette			= (UINT32*)Next; Next += BurnDrvGetPaletteEntries() * sizeof(UINT32);
	DrvPalette		= (UINT32*)Next; Next += BurnDrvGetPaletteEntries() * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000400;

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x100000;
	Drv68KRAM2		= Next; Next += 0x020000;
	Drv68KRAM3		= Next; Next += 0x001000;
	DrvSubRAM		= Next; Next += 0x004000;
	DrvPalRAM		= Next; Next += 0x001000;
	DrvSprRAM0		= Next; Next += 0x000800;
	DrvSprRAM1		= Next; Next += 0x014000;

	DrvVidRAM0		= Next; Next += 0x010000;
	DrvVIDCTRLRAM0	= Next; Next += 0x000008;

	DrvVidRAM1		= Next; Next += 0x010000;
	DrvVIDCTRLRAM1	= Next; Next += 0x000008;

	DrvVideoRegs	= Next; Next += 0x000008;

	tilebank		= Next; Next += 0x000004;
	tile_offset		= (UINT32*)Next; Next += 0x000001 * sizeof(UINT32);

	DrvShareRAM		= Next; Next += 0x001000;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvSetVideoOffsets(INT32 spritex, INT32 spritex_flipped, INT32 tilesx, INT32 tilesx_flipped)
{
	VideoOffsets[0][0] = spritex;
	VideoOffsets[0][1] = spritex_flipped;
	VideoOffsets[1][0] = tilesx;
	VideoOffsets[1][1] = tilesx_flipped;
}

static void DrvSetColorOffsets(INT32 gfx0, INT32 gfx1, INT32 gfx2)
{
	ColorOffsets[0] = gfx0;
	ColorOffsets[1] = gfx1;
	ColorOffsets[2] = gfx2;
}

static void DrvSetDefaultColorTable()
{
	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++) {
		Palette[i] = i;
	}
}

static void zingzapSetColorTable()
{
	for (INT32 color = 0; color < 0x20; color++) {
		for (INT32 pen = 0; pen < 0x40; pen++) {
			Palette[0x400 + ((color << 6) | pen)] = 0x400 + (((color << 4) + pen) & 0x1ff);
		}
	}
}

static void jjsquawkSetColorTable()
{
	for (INT32 color = 0; color < 0x20; color++) {
		for (INT32 pen = 0; pen < 0x40; pen++) {
			Palette[0x200 + ((color << 6) | pen)] = 0x400 + (((color << 4) + pen) & 0x1ff);
			Palette[0xa00 + ((color << 6) | pen)] = 0x200 + (((color << 4) + pen) & 0x1ff);
		}
	}
}

static void gundharaSetColorTable()
{
	for (INT32 color = 0; color < 0x20; color++) {
		for (INT32 pen = 0; pen < 0x40; pen++) {
			Palette[0x200 + ((color << 6) | pen)] = 0x400 + ((((color & ~3) << 4) + pen) & 0x1ff);
			Palette[0xa00 + ((color << 6) | pen)] = 0x200 + ((((color & ~3) << 4) + pen) & 0x1ff);
		}
	}
}

static void blandiaSetColorTable()
{
	for (INT32 color = 0; color < 0x20; color++) {
		for (INT32 pen = 0; pen < 0x40; pen++) {
			Palette[0x200 + ((color << 6) | pen)] = 0x200 + ((color << 4) | (pen & 0x0f));
			Palette[0xa00 + ((color << 6) | pen)] = 0x400 + pen;
		}
	}
}

static void usclssicSetColorTable()
{
	memcpy (DrvColPROM + 0x600, DrvColPROM + 0x000, 0x200);
//	memcpy (DrvColPROM + 0x200, DrvColPROM + 0x200, 0x200);

	for (INT32 color = 0; color < BurnDrvGetPaletteEntries(); color++) {
		Palette[color] = color;
	}

	for (INT32 color = 0; color < 0x20; color++) {
		for (INT32 pen = 0; pen < 0x40; pen++) {
			Palette[0x200 + ((color << 6) | pen)] = 0x200 + ((((color & ~3) << 4) + pen) & 0x1ff);
			Palette[0xA00 + ((color << 6) | pen)] = 0x200 + ((((color & ~3) << 4) + pen) & 0x1ff);
		}
	}
}

static INT32 DrvInit(void (*p68kInit)(), INT32 cpu_speed, INT32 irq_type, INT32 spr_buffer, INT32 gfxtype0, INT32 gfxtype1, INT32 gfxtype2)
{
	BurnSetRefreshRate((1.00 * refresh_rate)/100);

	if (pRomLoadCallback) {
		pRomLoadCallback(0);
	} else {
		DrvLoadRoms(0);
	}

	BurnAllocMemIndex();

	if (pRomLoadCallback) {
		if (pRomLoadCallback(1)) return 1;
	} else {
		if (DrvLoadRoms(1)) return 1;
	}

	// make sure these are initialized so that we can use common routines
	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	if (p68kInit) {
		p68kInit();
	}

	DrvGfxDecode(gfxtype0, DrvGfxROM0, 0);
	DrvGfxDecode(gfxtype1, DrvGfxROM1, 1);
	DrvGfxDecode(gfxtype2, DrvGfxROM2, 2);

	cpuspeed = cpu_speed;
	irqtype = irq_type;
	buffer_sprites = spr_buffer;

	if ((strstr(BurnDrvGetTextA(DRV_NAME), "calibr50")) || (strstr(BurnDrvGetTextA(DRV_NAME), "usclssic"))) {
		x1010_sound_init(16000000, 0x1000);
	} else {
		x1010_sound_init(16000000, 0x0000);
	}
	x1010_set_route(BURN_SND_X1010_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	x1010_set_route(BURN_SND_X1010_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);

	if (strstr(BurnDrvGetTextA(DRV_NAME), "madshark") || strstr(BurnDrvGetTextA(DRV_NAME), "gundhara"))
		x1010_set_route(BURN_SND_X1010_ROUTE_1, 1.00, BURN_SND_ROUTE_BOTH);
	
	if (strstr(BurnDrvGetTextA(DRV_NAME), "kamenrid") || strstr(BurnDrvGetTextA(DRV_NAME), "wrofaero") || strstr(BurnDrvGetTextA(DRV_NAME), "sokonuke"))
		x1010_set_route(BURN_SND_X1010_ROUTE_2, 1.00, BURN_SND_ROUTE_BOTH);

	if (strstr(BurnDrvGetTextA(DRV_NAME), "tndrcade"))
		has_2203 = 1;

	BurnYM3812Init(1, 4000000, NULL, 0);
	BurnTimerAttach(&SekConfig, 16000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, (has_2203) ? 2.00 : 1.00, BURN_SND_ROUTE_BOTH); // tndrcade (has_2203) needs louder fm3812

	BurnYM3438Init(1, 16000000/4, &DrvFMIRQHandler, 1);
	if (has_z80) {
		BurnTimerAttach(&ZetConfig, 4000000);
	}
	BurnYM3438SetRoute(0, BURN_SND_YM3438_YM3438_ROUTE_1, 0.30, BURN_SND_ROUTE_LEFT);
	BurnYM3438SetRoute(0, BURN_SND_YM3438_YM3438_ROUTE_2, 0.30, BURN_SND_ROUTE_RIGHT);

	if (has_2203) {
		BurnYM2203Init(1,  4000000, NULL, 1);
		BurnYM2203SetPorts(0, &DrvYM2203ReadPortA, &DrvYM2203ReadPortB, NULL, NULL);
		BurnYM2203SetAllRoutes(0, 2.00, BURN_SND_ROUTE_BOTH);
		BurnTimerAttach(&M6502Config, 2000000);
	}

	GenericTilesInit();

	DrvSetDefaultColorTable();

	flipflop = 0;

	VideoOffsets[2][0] = ((256 - nScreenHeight) / 2); // adjust for screen height
	VideoOffsets[2][1] = VideoOffsets[2][0];

	BurnGunInit(2, true);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	pRomLoadCallback = NULL;

	GenericTilesExit();

	DrvGfxROM0 = NULL;
	DrvGfxROM1 = NULL;
	DrvGfxROM2 = NULL;

	DrvSetColorOffsets(0, 0, 0);
	DrvSetVideoOffsets(0, 0, 0, 0);

	SekExit();

	if (has_z80) {
		ZetExit();
	}

	if (m65c02_mode) {
		M6502Exit();
		m65c02_mode = 0;
	}

	BurnGunExit();

	x1010_exit();
	BurnYM3438Exit();
	BurnYM3812Exit();
	if (has_2203) {
		BurnYM2203Exit();
	}

	MSM6295Exit(0);
	MSM6295ROM = NULL;

	BurnFreeMemIndex();

	oisipuzl_hack = 0;
	twineagle = 0;
	daiohc = 0;
	watchdog_enable = 0;
	refresh_rate = 6000;
	game_rotates = 0;
	clear_opposites = 0;
	has_2203 = 0;
	has_z80 = 0;
	trackball_mode = 0;
	usclssic = 0;

	BurnFree (DrvGfxTransMask[0]);
	BurnFree (DrvGfxTransMask[2]);
	BurnFree (DrvGfxTransMask[1]);

	has_raster = 0;

	return 0;
}

static inline void DrvColors(INT32 i, INT32 pal)
{
	INT32 r = (pal >> 10) & 0x1f;
	INT32 g = (pal >>  5) & 0x1f;
	INT32 b = (pal >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[i] = BurnHighCol(r, g, b, 0);
}

static void DrvPaletteRecalc()
{
	UINT16 *p  = (UINT16*)DrvPalRAM;

	if (DrvROMLen[4] && DrvROMLen[4] > 1) { // usclassic color prom
		memcpy (DrvColPROM + 0x400, DrvPalRAM, 0x200);
		memcpy (DrvColPROM + 0x000, DrvPalRAM + 0x200, 0x200);
		p = (UINT16*)DrvColPROM;
	}

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++) {
		DrvColors(i, BURN_ENDIAN_SWAP_INT16(p[Palette[i]]));
	}
}

static void draw_sprites_map()
{
	UINT16 *spriteram16 = (UINT16*)DrvSprRAM0;

	INT32 ctrl	=	BURN_ENDIAN_SWAP_INT16(spriteram16[0x600/2]);
	INT32 ctrl2	=	BURN_ENDIAN_SWAP_INT16(spriteram16[0x602/2]);

	INT32 flip	=	ctrl  & 0x40;
	INT32 numcol	=	ctrl2 & 0x0f;

	UINT16 *src = ((UINT16*)DrvSprRAM1) + ( ((ctrl2 ^ (~ctrl2<<1)) & 0x40) ? 0x2000/2 : 0 );

	INT32 upper	= ( BURN_ENDIAN_SWAP_INT16(spriteram16[ 0x604/2 ]) & 0xFF ) +( BURN_ENDIAN_SWAP_INT16(spriteram16[ 0x606/2 ]) & 0xFF ) * 256;

	INT32 col0 = 0;
	switch (ctrl & 0x0f)
	{
		case 0x01: col0	= 0x4; break; // krzybowl
		case 0x06: col0	= 0x8; break; // kiwame
	}

	INT32 xoffs = 0;
	INT32 yoffs = flip ? 1 : -1;

	if (numcol == 1) numcol = 16;

	for (INT32 col = 0 ; col < numcol; col++)
	{
		INT32 x = BURN_ENDIAN_SWAP_INT16(spriteram16[(col * 0x20 + 0x408)/2]) & 0xff;
		INT32 y = BURN_ENDIAN_SWAP_INT16(spriteram16[(col * 0x20 + 0x400)/2]) & 0xff;

		for (INT32 offs = 0; offs < 0x20; offs++)
		{
			INT32 code	= BURN_ENDIAN_SWAP_INT16(src[((col+col0)&0xf) * 0x40/2 + offs + 0x800/2]);
			INT32 color	= BURN_ENDIAN_SWAP_INT16(src[((col+col0)&0xf) * 0x40/2 + offs + 0xc00/2]);

			INT32 flipx	= code & 0x8000;
			INT32 flipy	= code & 0x4000;

			INT32 bank	= (color & 0x0600) >> 9;

			INT32 sx		=   x + xoffs  + (offs & 1) * 16;
			INT32 sy		= -(y + yoffs) + (offs / 2) * 16;

			if (upper & (1 << col))	sx += 256;

			if (flip) {
				sy = 0xf0 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			color	= (color >> 11) & 0x1f;
			code	= ((code & 0x3fff) + (bank * 0x4000)) % DrvGfxMask[0];

			if (DrvGfxTransMask[0][code]) continue;

			sx = ((sx + 0x10) & 0x1ff) - 0x10;
			sy = ((sy + 8) & 0x0ff) - 8;
			sy = ((sy+16-VideoOffsets[2][0])&0xff)-16;

			Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, ColorDepths[0], 0, 0, DrvGfxROM0);
		}
	}
}

static void draw_sprites()
{
	if (~nSpriteEnable & 1) return;

	UINT16 *spriteram16 = (UINT16*)DrvSprRAM0;

	INT32 ctrl	= BURN_ENDIAN_SWAP_INT16(spriteram16[ 0x600/2 ]);
	INT32 ctrl2	= BURN_ENDIAN_SWAP_INT16(spriteram16[ 0x602/2 ]);

	INT32 flip	= ctrl & 0x40;

	UINT16 *src = ((UINT16*)DrvSprRAM1) + ( ((ctrl2 ^ (~ctrl2<<1)) & 0x40) ? 0x2000/2 : 0 );

	draw_sprites_map();

	INT32 yoffs = oisipuzl_hack ? 32 : 0;
	INT32 xoffs = VideoOffsets[0][flip ? 1 : 0];

	for (INT32 offs = (0x400-2)/2 ; offs >= 0; offs -= 1)
	{
		int	code	= BURN_ENDIAN_SWAP_INT16(src[offs + 0x000/2]);
		int	sx	= BURN_ENDIAN_SWAP_INT16(src[offs + 0x400/2]);

		int	sy	= BURN_ENDIAN_SWAP_INT16(spriteram16[offs + 0x000/2]) & 0xff;

		int	flipx	= code & 0x8000;
		int	flipy	= code & 0x4000;

		INT32 bank	= (sx & 0x0600) >> 9;
		INT32 color	= (sx >> 11) & 0x1f;

		if (flip)
		{
			sy = (0x100 - nScreenHeight) + 0xf0 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		code = ((code & 0x3fff) + (bank * 0x4000)) % DrvGfxMask[0];

		if (DrvGfxTransMask[0][code]) continue;

		sx = ((sx + xoffs + 0x10) & 0x1ff) - 0x10;
		sy = ((((0xf0 - sy) - (-2) + 8)) & 0x0ff) - 8;
		sy = ((yoffs+sy+16-VideoOffsets[2][0])&0xff)-16;

		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, ColorDepths[0], 0, ColorOffsets[0], DrvGfxROM0);
	}
}

static void draw_layer(UINT8 *ram, UINT8 *gfx, INT32 num, INT32 opaque, INT32 scrollx, INT32 scrolly)
{
	INT32 mask  = DrvGfxMask[num];
	INT32 depth = ColorDepths[num];
	INT32 color_offset = ColorOffsets[num];

	scrollx = scrollx & 0x3ff; // offsets added in seta_update()
	scrolly = (scrolly + VideoOffsets[2][0]) & 0x1ff;

	UINT16 *vram = (UINT16*)ram;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) << 4;
		INT32 sy = (offs >> 6) << 4;

		sx -= scrollx;
		if (sx < -15) sx += 0x400;
		sy -= scrolly;
		if (sy < -15) sy += 0x200;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = BURN_ENDIAN_SWAP_INT16(vram[offs + 0x000]);
		INT32 color = BURN_ENDIAN_SWAP_INT16(vram[offs + 0x800]) & 0x001f;

		INT32 code  = (attr & 0x3fff) + tile_offset[0];

		if (twineagle) {
			if ((code & 0x3e00) == 0x3e00) {
				code = (code & 0x007f) | ((tilebank[(code & 0x0180) >> 7] >> 1) << 7);
			}
		}

		code %= mask;

		if (!opaque) {
			if (DrvGfxTransMask[num][code]) continue;
		}

		INT32 flipx = attr & 0x8000;
		INT32 flipy = attr & 0x4000;

		if (flipscreen) {
			sx = (nScreenWidth - 16) - sx;
			sy = (nScreenHeight - 16) - sy;
			flipx ^= 0x8000;
			flipy ^= 0x4000;
		}

		if (opaque) {
			Draw16x16Tile(pTransDraw, code, sx, sy, flipx, flipy, color, depth, color_offset, gfx);
		} else {
			Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, depth, 0, color_offset, gfx);
		}
	}
}

static void seta_update(INT32 enable_tilemap2, INT32 tmap_flip)
{
	INT32 layer_enable = ~0; // start all layers enabled...

	INT32 order = 0;
	flipscreen = 0; //(DrvSprRAM0[0x601] & 0x40) >> 6; // disabled for now...
	flipscreen ^= tmap_flip;

	INT32 visible = (nScreenHeight - 1);

	UINT16 *vctrl0 = (UINT16*)DrvVIDCTRLRAM0;
	UINT16 *vctrl1 = (UINT16*)DrvVIDCTRLRAM1;

	INT32 x_0 = BURN_ENDIAN_SWAP_INT16(vctrl0[0]);
	INT32 y_0 = BURN_ENDIAN_SWAP_INT16(vctrl0[1]);
	INT32 en0 = BURN_ENDIAN_SWAP_INT16(vctrl0[2]);

	INT32 x_1 = BURN_ENDIAN_SWAP_INT16(vctrl1[0]);
	INT32 y_1 = BURN_ENDIAN_SWAP_INT16(vctrl1[1]);
	INT32 en1 = BURN_ENDIAN_SWAP_INT16(vctrl1[2]);

	x_0 += 0x10 - VideoOffsets[1][flipscreen ? 1 : 0];
	y_0 -= (256 - visible)/2;
	if (flipscreen)
	{
		x_0 = -x_0 - 512;
		y_0 = y_0 - visible;
	}

	if (enable_tilemap2)
	{
		x_1 += 0x10 - VideoOffsets[1][flipscreen ? 1 : 0];
		y_1 -= (256 - visible)/2;
		if (flipscreen)
		{
			x_1 = -x_1 - 512;
			y_1 = y_1 - visible;
		}

		order = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVideoRegs + 2)));
	}

	if ( en0 & 0x08) layer_enable &= ~0x01;
	if (~en0 & 0x08) layer_enable &= ~0x02;
	if ( en1 & 0x08) layer_enable &= ~0x04;
	if (~en1 & 0x08) layer_enable &= ~0x08;
	if (enable_tilemap2 == 0) layer_enable &= ~0x0c;

	layer_enable &= nBurnLayer;

	if (has_raster == 0) BurnTransferClear();

	if (order & 1)
	{
		if (layer_enable & 0x04) draw_layer(DrvVidRAM1 + 0x0000, DrvGfxROM2, 2, 1, x_1, y_1);
		if (layer_enable & 0x08) draw_layer(DrvVidRAM1 + 0x2000, DrvGfxROM2, 2, 1, x_1, y_1);

		if ((order & 2) == 2) draw_sprites();

		if (layer_enable & 0x01) draw_layer(DrvVidRAM0 + 0x0000, DrvGfxROM1, 1, 0, x_0, y_0);
		if (layer_enable & 0x02) draw_layer(DrvVidRAM0 + 0x2000, DrvGfxROM1, 1, 0, x_0, y_0);

		if ((order & 2) == 0) draw_sprites();
	}
	else
	{
		if (layer_enable & 0x01) draw_layer(DrvVidRAM0 + 0x0000, DrvGfxROM1, 1, 1, x_0, y_0);
		if (layer_enable & 0x02) draw_layer(DrvVidRAM0 + 0x2000, DrvGfxROM1, 1, 1, x_0, y_0);

		if ((order & 2) == 2) draw_sprites();

		if (layer_enable & 0x04) draw_layer(DrvVidRAM1 + 0x0000, DrvGfxROM2, 2, 0, x_1, y_1);
		if (layer_enable & 0x08) draw_layer(DrvVidRAM1 + 0x2000, DrvGfxROM2, 2, 0, x_1, y_1);

		if ((order & 2) == 0) draw_sprites();
	}
}

static INT32 setaNoLayersDraw()
{
	DrvPaletteRecalc();

	BurnTransferClear();

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void rasterBeginDraw()
{
	if (!pBurnDraw) return;

	BurnTransferClear();

	raster_needs_update = 0;
	lastline = 0;
}

static void rasterUpdateDraw()
{
	if (!pBurnDraw || !has_raster) return;

	if (scanline > nScreenHeight) scanline = nScreenHeight; // endofframe

	if (scanline < 0 || scanline > nScreenHeight || scanline == lastline || lastline > scanline) return;
	//bprintf(0, _T("%07d: partial %d - %d.\n"), nCurrentFrame, lastline, scanline);

	GenericTilesSetClip(0, nScreenWidth, lastline, scanline);

	seta_update(0, 0);

	GenericTilesClearClip();

	lastline = scanline;
}

static INT32 seta1layerDraw()
{
	if (has_raster == 0) DrvPaletteRecalc();

	if (has_raster == 0) seta_update(0, 0);

	if (has_raster) {
		// finish drawing the frame (in raster chain)
		// -or- draw the frame
		DrvPaletteRecalc();

		rasterUpdateDraw();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 seta2layerDraw()
{
	DrvPaletteRecalc();

	seta_update(1, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 zombraidDraw()
{
	seta2layerDraw();

	BurnGunDrawTargets();

	return 0;
}

static INT32 seta2layerFlippedDraw()
{
	DrvPaletteRecalc();

	seta_update(1, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void irq_generator(INT32 loop)
{
	if (loop != 4 && loop != 9) return;

	loop = (loop / 5) & 1;

	INT32 line = (irqtype >> (loop * 8)) & 0xff;
	if (line & 0x80) return;

	SekSetIRQLine(line, CPU_IRQSTATUS_AUTO);
}

static void sprite_buffer()
{
	if ((DrvSprRAM0[0x602] & 0x20) == 0)
	{
		if (DrvSprRAM0[0x602] & 0x40) {
			memcpy (DrvSprRAM1 + 0x0000, DrvSprRAM1 + 0x2000, 0x2000);
		} else {
			memcpy (DrvSprRAM1 + 0x2000, DrvSprRAM1 + 0x0000, 0x2000);
		}
	}
}

inline void ClearOppositesActiveLow(UINT8* nJoystickInputs)
{
	if ((*nJoystickInputs & 0x03) == 0x00) {
		*nJoystickInputs |= 0x03;
	}
	if ((*nJoystickInputs & 0x0c) == 0x00) {
		*nJoystickInputs |= 0x0c;
	}
}

static INT32 DrvCommonFrame(void (*pFrameCallback)())
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	if (watchdog_enable) {
		watchdog++;
		if (watchdog >= 180) {
			DrvDoReset(0);
		}
	}

	{
		memset (DrvInputs, 0xff, 7 * sizeof(UINT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
			DrvInputs[6] ^= (DrvJoy7[i] & 1) << i;
		}

		if (clear_opposites) {
			ClearOppositesActiveLow((UINT8*)&DrvInputs[0]);
			ClearOppositesActiveLow((UINT8*)&DrvInputs[1]);
		}

		if (game_rotates) {
			SuperJoy2Rotate();
		}

		if (trackball_mode) trackball_input_tick();

		BurnGunMakeInputs(0, (INT16)DrvAxis[0], (INT16)DrvAxis[1]);	// zombraid
		BurnGunMakeInputs(1, (INT16)DrvAxis[2], (INT16)DrvAxis[3]);
		
		float xRatio = (float)128 / 384;
		float yRatio = (float)96 / 224;
		
		for (INT32 i = 0; i < 2; i++) {
			INT32 x = BurnGunX[i] >> 8;
			INT32 y = BurnGunY[i] >> 8;
			
			x = (INT32)(x * xRatio);
			y = (INT32)(y * yRatio);
		
			x -= 0xbe;
			y += 0x48;
		
			DrvAnalogInput[0 + (i * 2)] = (UINT8)~x;
			DrvAnalogInput[1 + (i * 2)] = (UINT8)y;
		}
	}

	pFrameCallback();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	if (buffer_sprites) {
		sprite_buffer();
	}

	return 0;
}

static void Drv68kmsgundam()
{
	INT32 nInterleave = 10;
	INT32 nCyclesTotal[1] = { (cpuspeed * 100) / refresh_rate };
	INT32 nCyclesDone[1]  = { 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (i == 4 && nCurrentFrame & 2)
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		if (i == (nInterleave - 1))
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
	}

	SekClose();

	if (pBurnSoundOut) {
		x1010_sound_update();
	}
}

static void Drv68kNoSubFrameCallback()
{
	INT32 nInterleave = 10;
	INT32 nCyclesTotal[1] = { (cpuspeed * 100) / refresh_rate };
	INT32 nCyclesDone[1]  = { 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		irq_generator(i);
	}

	SekClose();

	if (pBurnSoundOut) {
		x1010_sound_update();
	}
}

static INT32 DrvFrame()
{
	return DrvCommonFrame(Drv68kNoSubFrameCallback);
}

static INT32 DrvFrameMsgundam()
{
	return DrvCommonFrame(Drv68kmsgundam);
}

static void Drv68k_Calibr50_FrameCallback()
{
	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { (8000000 * 100) / refresh_rate, (2000000 * 100) / refresh_rate}; //(cpuspeed * 100) / refresh_rate, ((cpuspeed/4) * 100) / refresh_rate};
	INT32 nCyclesDone[2]  = { 0, 0 };

	SekOpen(0);
	M6502Open(0);

	if (has_raster) rasterBeginDraw();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		if (i == (nInterleave - 1)) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		if ((i%64) == 63) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		CPU_RUN(0, Sek);

		if (raster_needs_update && scanline > -1) {
			rasterUpdateDraw();
			raster_needs_update = 0;
		}

		CPU_RUN(1, M6502);
		if (usclssic) {
			if (i == (nInterleave - 1)) M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
		} else {// calibr50
			if ((i%64) == 63) M6502SetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}
	}

	SekClose();
	M6502Close();

	if (pBurnSoundOut) {
		x1010_sound_update();
	}
}

static void Drv68k_KM_FrameCallback() // kamenrid & madshark
{
	INT32 nInterleave = 10;
	INT32 nCyclesTotal[1] = { (cpuspeed * 100) / refresh_rate };
	INT32 nCyclesDone[1]  = { 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (i & 1 && i == 1) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
	}

	if (flipflop==0) { // IRQ4 is fired every other frame
		SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		flipflop=1;
	} else {
		flipflop=0;
	}

	SekClose();

	if (pBurnSoundOut) {
		x1010_sound_update();
	}
}

static void Drv68k_Twineagl_FrameCallback()
{
	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { (cpuspeed * 100) / refresh_rate, (2000000 * 100) / refresh_rate };
	INT32 nCyclesDone[2]  = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		CPU_RUN(0, Sek);
		irq_generator(i);
		SekClose();

		M6502Open(0);
		CPU_RUN(1, M6502);
		if (i == 4) M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		if (i == 9) M6502SetIRQLine(0, CPU_IRQSTATUS_AUTO);
		M6502Close();
	}

	if (pBurnSoundOut) {
		x1010_sound_update();
	}
}

static void Drv68k_Downtown_FrameCallback()
{
	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { (cpuspeed * 100) / refresh_rate, (2000000 * 100) / refresh_rate };
	INT32 nCyclesDone[2]  = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		CPU_RUN(0, Sek);
		irq_generator(i);
		SekClose();

		M6502Open(0);
		CPU_RUN(1, M6502);
		if (i == 4) M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		if (i == 9) M6502SetIRQLine(0, CPU_IRQSTATUS_AUTO);
		M6502Close();
	}

	if (pBurnSoundOut) {
		x1010_sound_update();
	}
}

static void Drv68k_Tndrcade_FrameCallback()
{
	INT32 nInterleave = 16;
	INT32 nCyclesTotal[2] = { (cpuspeed * 100) / refresh_rate, (2000000 * 100) / refresh_rate };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekNewFrame();
	M6502NewFrame();

	SekOpen(0);
	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == (nInterleave-1)) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);

		BurnTimerUpdate((i+1) * (nCyclesTotal[1] / nInterleave));
		if (i == 4) M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		M6502SetIRQLine(0, CPU_IRQSTATUS_AUTO);
	}

	BurnTimerEndFrame(nCyclesTotal[1]);
	SekClose();
	M6502Close();

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}
}

static INT32 DrvKMFrame()
{
	return DrvCommonFrame(Drv68k_KM_FrameCallback);
}

static INT32 DrvCalibr50Frame()
{
	return DrvCommonFrame(Drv68k_Calibr50_FrameCallback);
}

static INT32 DrvTwineaglFrame()
{
	return DrvCommonFrame(Drv68k_Twineagl_FrameCallback);
}

static INT32 DrvDowntownFrame()
{
	return DrvCommonFrame(Drv68k_Downtown_FrameCallback);
}

static INT32 DrvTndrcadeFrame()
{
	return DrvCommonFrame(Drv68k_Tndrcade_FrameCallback);
}

static void Drv68kNoSubM6295FrameCallback()
{
	INT32 nInterleave = 10;
	INT32 nCyclesTotal[1] = { (cpuspeed * 100) / refresh_rate };
	INT32 nCyclesDone[1]  = { 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		irq_generator(i);
	}

	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}
}

static INT32 DrvM6295Frame()
{
	return DrvCommonFrame(Drv68kNoSubM6295FrameCallback);
}

static void Drv68kZ80M6295FrameCallback()
{
	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { (cpuspeed * 100) / refresh_rate, (4000000 * 100) / refresh_rate };
	INT32 nCyclesDone[2]  = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		CPU_RUN(1, Zet);

		irq_generator(i);
	}

	ZetClose();
	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}
}

static INT32 DrvZ80M6295Frame()
{
 	return DrvCommonFrame(Drv68kZ80M6295FrameCallback);
}


static void Drv68kZ80YM3438FrameCallback()
{
	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { (cpuspeed * 100) / refresh_rate, (4000000 * 100) / refresh_rate };
	INT32 nCyclesDone[2]  = { 0, 0 };

	ZetNewFrame();
	
	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		CPU_RUN_TIMER(1);

		irq_generator(i);
	}

	if (pBurnSoundOut) {
		x1010_sound_update();
		BurnYM3438Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

}

static INT32 Drv68kZ80YM3438Frame()
{
	return DrvCommonFrame(Drv68kZ80YM3438FrameCallback);
}

static void CrazyfghtFrameCallback()
{
	SekNewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { (cpuspeed * 100) / refresh_rate };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		BurnTimerUpdate((i+1) * (nCyclesTotal[0] / nInterleave));

		if ((i % 48) == 0) {
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		}

		if (i == 240) {
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		}
	}

	BurnTimerEndFrame(nCyclesTotal[0]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();
}

static INT32 CrazyfgtFrame()
{
	return DrvCommonFrame(CrazyfghtFrameCallback);
}


static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvNVRAM;
		ba.nLen	  = 0x400;
		ba.szName = "NV Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		if (has_z80) {
			ZetScan(nAction);
		}

		if (m65c02_mode) {
			M6502Scan(nAction);
		}

		x1010_scan(nAction, pnMin);
		BurnYM3812Scan(nAction, pnMin);
		BurnYM3438Scan(nAction, pnMin);
		if (has_2203) {
			BurnYM2203Scan(nAction, pnMin);
		}
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(soundlatch2);

		SCAN_VAR(watchdog);
		SCAN_VAR(flipscreen);
		SCAN_VAR(seta_samples_bank);
		SCAN_VAR(usclssic_port_select);
		SCAN_VAR(gun_input_bit);
		SCAN_VAR(gun_input_src);
		SCAN_VAR(m65c02_bank);
		SCAN_VAR(sub_ctrl_data);
		SCAN_VAR(flipflop);

		if (trackball_mode) {
			SCAN_VAR(track_x);
			SCAN_VAR(track_y);
			SCAN_VAR(track_x_last);
			SCAN_VAR(track_y_last);
			SCAN_VAR(track_x2);
			SCAN_VAR(track_y2);
			SCAN_VAR(track_x2_last);
			SCAN_VAR(track_y2_last);
		}
		if (game_rotates) {
			SCAN_VAR(nRotateHoldInput);
			SCAN_VAR(nRotate);
			SCAN_VAR(nRotateTarget);
			SCAN_VAR(nRotateTry);
			SCAN_VAR(nRotateTime);
			SCAN_VAR(nAutoFireCounter);
		}
		keroppi_pairslove_scan();
	}

	if (nAction & ACB_WRITE) {
		INT32 tmpbank = seta_samples_bank;
		seta_samples_bank = -1;
		set_pcm_bank(tmpbank);

		if (m65c02_mode) {
			M6502Open(0);
			m65c02_sub_bankswitch(m65c02_bank);
			M6502Close();
		}
		if (game_rotates) {
			nRotateTime[0] = nRotateTime[1] = 0;
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------------------------------------------
// Nonworking games...

//  The Roulette (Visco)

static struct BurnRomInfo setaroulRomDesc[] = {
	{ "uf1-002.u14",	0x10000, 0xb3a622b0, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "uf1-003.u16",	0x10000, 0xa6afd769, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "uf0-005.u3",		0x08000, 0x383c2d57, 0x03 | BRF_GRA },           //  2 Sprites
	{ "uf0-006.u4",		0x08000, 0x90c9dae6, 0x03 | BRF_GRA },           //  3
	{ "uf0-007.u5",		0x08000, 0xe72c3dba, 0x03 | BRF_GRA },           //  4
	{ "uf0-008.u6",		0x08000, 0xe198e602, 0x03 | BRF_GRA },           //  5

	{ "uf0-010.u15",	0x80000, 0x0af13a56, 0x04 | BRF_GRA },           //  6 Layer 1 tiles
	{ "uf0-009.u13",	0x80000, 0x20f2d7f5, 0x04 | BRF_GRA },           //  7
	{ "uf0-012.u29",	0x80000, 0xcba2a6b7, 0x04 | BRF_GRA },           //  8
	{ "uf0-011.u22",	0x80000, 0xaf60adf9, 0x04 | BRF_GRA },           //  9
	{ "uf0-014.u38",	0x80000, 0xda2bd4e4, 0x04 | BRF_GRA },           // 10
	{ "uf0-013.u37",	0x80000, 0x645ec3c3, 0x04 | BRF_GRA },           // 11
	{ "uf0-015.u40",	0x80000, 0x11dc19fa, 0x04 | BRF_GRA },           // 12
	{ "uf0-016.u48",	0x80000, 0x10f99fa8, 0x04 | BRF_GRA },           // 13

	{ "uf1-004.u52",	0x20000, 0x6638054d, 0x06 | BRF_SND },           // 14 x1-010 Samples

	{ "uf0-017.u50",	0x00200, 0xbf50c303, 0x00 | BRF_GRA },           // 15 Color Proms
	{ "uf0-018.u51",	0x00200, 0x1c584d5f, 0x00 | BRF_GRA },           // 16
};

STD_ROM_PICK(setaroul)
STD_ROM_FN(setaroul)

static INT32 NotWorkingInit()
{
	return 1;
}

struct BurnDriverD BurnDrvSetaroul = {
	"setaroul", NULL, NULL, NULL, "1989?",
	"The Roulette (Visco)\0", NULL, "Visco", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA1, GBF_CASINO, 0,
	NULL, setaroulRomInfo, setaroulRomName, NULL, NULL, NULL, NULL, DrgnunitInputInfo, NULL,
	NotWorkingInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0,
	240, 384, 3, 4
};


// International Toote (Germany, P523.V01)

static struct BurnRomInfo inttooteRomDesc[] = {
	{ "p523.v01_horse_prog_2.002",	0x10000, 0x6ce6f1ad, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "p523.v01_horse_prog_1.003",	0x10000, 0x921fcff5, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ya_002_001.u18",				0x80000, 0xdd108016, 0x01 | BRF_PRG | BRF_ESS }, //  2

	{ "ya_011_004.u10",				0x80000, 0xeb74d2e0, 0x03 | BRF_GRA },           //  3 Sprites
	{ "ya_011_005.u17",				0x80000, 0x4a6c804b, 0x03 | BRF_GRA },           //  4
	{ "ya_011_006.u22",				0x80000, 0xbfae01a5, 0x03 | BRF_GRA },           //  5
	{ "ya_011_007.u27",				0x80000, 0x2dc7a294, 0x03 | BRF_GRA },           //  6
	{ "p523.v01_horse_fore_1.u135",	0x10000, 0x3a75df30, 0x00 | BRF_GRA },           //  7
	{ "p523.v01_horse_fore_2.u134",	0x10000, 0x26fb0339, 0x00 | BRF_GRA },           //  8
	{ "p523.v01_horse_fore_3.u133",	0x10000, 0xc38596af, 0x00 | BRF_GRA },           //  9
	{ "p523.v01_horse_fore_4.u132",	0x10000, 0x64ef345e, 0x00 | BRF_GRA },           // 10

	{ "ya_011_008.u35",				0x40000, 0x4b890f83, 0x04 | BRF_GRA },           // 11 Layer 1 tiles
	{ "p523.v01_horse_back_1.u137",	0x20000, 0x39b221ea, 0x04 | BRF_GRA },   		 // 12
	{ "ya_011_009.u41",				0x40000, 0xcaa5e3c1, 0x04 | BRF_GRA },           // 13
	{ "p523.v01_horse_back_2.u136",	0x20000, 0x9c5e32a0, 0x04 | BRF_GRA },           // 14

	{ "ya_011_013.u71",				0x80000, 0x2bccaf47, 0x06 | BRF_SND },           // 17 x1-010 Samples
	{ "ya_011_012.u64",				0x80000, 0xa8015ce6, 0x06 | BRF_SND },           // 18

	{ "ya-010.prom",				0x00200, 0x778094b3, 0x00 | BRF_GRA },           // 15 Color Proms
	{ "ya-011.prom",				0x00200, 0xbd4fe2f6, 0x00 | BRF_GRA },           // 16
};

STD_ROM_PICK(inttoote)
STD_ROM_FN(inttoote)

#if 0
static void inttoote68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(DrvVidRAM0,		0xb00000, 0xb07fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xd00000, 0xd00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xe00000, 0xe03fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xffc000, 0xffffff, MAP_RAM);


	SekMapHandler(1,			0x900000, 0x903fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();


	BurnLoadRom(DrvGfxROM0 + 0x070000,  7, 1);
	BurnLoadRom(DrvGfxROM0 + 0x0f0000,  8, 1);
	BurnLoadRom(DrvGfxROM0 + 0x170000,  9, 1);
	BurnLoadRom(DrvGfxROM0 + 0x1f0000, 10, 1);
}

static INT32 inttooteInit()
{
	DrvSetColorOffsets(0, 0, 0);
	DrvSetVideoOffsets(1, 1, -1, -1);

	return DrvInit(inttoote68kInit, 16000000, SET_IRQLINES(0x80, 0x80) /* Custom */, NO_SPRITE_BUFFER, SET_GFX_DECODE(5, 1, -1));
}
#endif

struct BurnDriverD BurnDrvInttoote = {
	"inttoote", "jockeyc", NULL, NULL, "1998",
	"International Toote (Germany, P523.V01)\0", NULL, "bootleg (Coinmaster)", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_SETA1, GBF_CASINO, 0,
	NULL, inttooteRomInfo, inttooteRomName, NULL, NULL, NULL, NULL, DrgnunitInputInfo, NULL,
	NotWorkingInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0,
	384, 240, 4, 3
};


// International Toote II (World?)

static struct BurnRomInfo inttooteaRomDesc[] = {
	{ "p387.v01_horse_prog_2.002",	0x10000, 0x1ced885e, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "p387.v01_horse_prog_1.003",	0x10000, 0xe24592af, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ya_002_001.u18",				0x80000, 0xdd108016, 0x01 | BRF_PRG | BRF_ESS }, //  2

	{ "ya_011_004.u10",				0x80000, 0xeb74d2e0, 0x03 | BRF_GRA },           //  3 Sprites
	{ "ya_011_005.u17",				0x80000, 0x4a6c804b, 0x03 | BRF_GRA },           //  4
	{ "ya_011_006.u22",				0x80000, 0xbfae01a5, 0x03 | BRF_GRA },           //  5
	{ "ya_011_007.u27",				0x80000, 0x2dc7a294, 0x03 | BRF_GRA },           //  6
	{ "p523.v01_horse_fore_1.u135",	0x10000, 0x3a75df30, 0x00 | BRF_GRA },           //  7
	{ "p523.v01_horse_fore_2.u134",	0x10000, 0x26fb0339, 0x00 | BRF_GRA },           //  8
	{ "p523.v01_horse_fore_3.u133",	0x10000, 0xc38596af, 0x00 | BRF_GRA },           //  9
	{ "p523.v01_horse_fore_4.u132",	0x10000, 0x64ef345e, 0x00 | BRF_GRA },           // 10

	{ "ya_011_008.u35",				0x40000, 0x4b890f83, 0x04 | BRF_GRA },           // 11 Layer 1 tiles
	{ "p523.v01_horse_back_1.u137",	0x20000, 0x39b221ea, 0x04 | BRF_GRA },           // 12
	{ "ya_011_009.u41",				0x40000, 0xcaa5e3c1, 0x04 | BRF_GRA },           // 13
	{ "p523.v01_horse_back_2.u136",	0x20000, 0x9c5e32a0, 0x04 | BRF_GRA },           // 14

	{ "ya_011_013.u71",				0x80000, 0x2bccaf47, 0x06 | BRF_SND },           // 17 x1-010 Samples
	{ "ya_011_012.u64",				0x80000, 0xa8015ce6, 0x06 | BRF_SND },           // 18

	{ "ya-010.prom",				0x00200, 0x778094b3, 0x00 | BRF_GRA },           // 15 Color Proms
	{ "ya-011.prom",				0x00200, 0xbd4fe2f6, 0x00 | BRF_GRA },           // 16
};

STD_ROM_PICK(inttootea)
STD_ROM_FN(inttootea)

struct BurnDriverD BurnDrvInttootea = {
	"inttootea", "jockeyc", NULL, NULL, "1993",
	"International Toote II (World?)\0", NULL, "Coinmaster", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_SETA1, GBF_CASINO, 0,
	NULL, inttooteaRomInfo, inttooteaRomName, NULL, NULL, NULL, NULL, DrgnunitInputInfo, NULL,
	NotWorkingInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0,
	384, 240, 4, 3
};


//-----------------------------------------------------------------------------------------------------------------
// Working games


// Dragon Unit / Castle of Dragon

static struct BurnRomInfo drgnunitRomDesc[] = {
	{ "prg-e.bin",		0x20000, 0x728447df, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "prg-o.bin",		0x20000, 0xb2f58ecf, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "obj-2.bin",		0x20000, 0xd7f6ab5a, 0x0b | BRF_GRA },           //  2 Sprites
	{ "obj-1.bin",		0x20000, 0x53a95b13, 0x0b | BRF_GRA },           //  3
	{ "obj-6.bin",		0x20000, 0x80b801f7, 0x0b | BRF_GRA },           //  4
	{ "obj-5.bin",		0x20000, 0x6b87bc20, 0x0b | BRF_GRA },           //  5
	{ "obj-4.bin",		0x20000, 0x60d17771, 0x0b | BRF_GRA },           //  6
	{ "obj-3.bin",		0x20000, 0x0bccd4d5, 0x0b | BRF_GRA },           //  7
	{ "obj-8.bin",		0x20000, 0x826c1543, 0x0b | BRF_GRA },           //  8
	{ "obj-7.bin",		0x20000, 0xcbaa7f6a, 0x0b | BRF_GRA },           //  9

	{ "scr-1o.bin",		0x20000, 0x671525db, 0x04 | BRF_GRA },           // 10 Layer 1 tiles
	{ "scr-2o.bin",		0x20000, 0x2a3f2ed8, 0x04 | BRF_GRA },           // 11
	{ "scr-3o.bin",		0x20000, 0x4d33a92d, 0x04 | BRF_GRA },           // 12
	{ "scr-4o.bin",		0x20000, 0x79a0aa61, 0x04 | BRF_GRA },           // 13
	{ "scr-1e.bin",		0x20000, 0xdc9cd8c9, 0x04 | BRF_GRA },           // 14
	{ "scr-2e.bin",		0x20000, 0xb6126b41, 0x04 | BRF_GRA },           // 15
	{ "scr-3e.bin",		0x20000, 0x1592b8c2, 0x04 | BRF_GRA },           // 16
	{ "scr-4e.bin",		0x20000, 0x8201681c, 0x04 | BRF_GRA },           // 17

	{ "snd-1.bin",		0x20000, 0x8f47bd0d, 0x06 | BRF_SND },           // 18 x1-010 Samples
	{ "snd-2.bin",		0x20000, 0x65c40ef5, 0x06 | BRF_SND },           // 19
	{ "snd-3.bin",		0x20000, 0x71fbd54e, 0x06 | BRF_SND },           // 20
	{ "snd-4.bin",		0x20000, 0xac50133f, 0x06 | BRF_SND },           // 21
	{ "snd-5.bin",		0x20000, 0x70652f2c, 0x06 | BRF_SND },           // 22
	{ "snd-6.bin",		0x20000, 0x10a1039d, 0x06 | BRF_SND },           // 23
	{ "snd-7.bin",		0x20000, 0xdecbc8b0, 0x06 | BRF_SND },           // 24
	{ "snd-8.bin",		0x20000, 0x3ac51bee, 0x06 | BRF_SND },           // 25
};

STD_ROM_PICK(drgnunit)
STD_ROM_FN(drgnunit)

static INT32 drgnunitInit()
{
	DrvSetColorOffsets(0, 0, 0);
	DrvSetVideoOffsets(2, 2, -2, -2);

	return DrvInit(drgnunit68kInit, 8000000, SET_IRQLINES(1, 2), SPRITE_BUFFER, SET_GFX_DECODE(0, 1, -1));
}

struct BurnDriver BurnDrvDrgnunit = {
	"drgnunit", NULL, NULL, NULL, "1989",
	"Dragon Unit / Castle of Dragon\0", NULL, "Athena / Seta", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_SCRFIGHT, 0,
	NULL, drgnunitRomInfo, drgnunitRomName, NULL, NULL, NULL, NULL, DrgnunitInputInfo, DrgnunitDIPInfo,
	drgnunitInit, DrvExit, DrvFrame, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	384, 240, 4, 3
};


// Quiz Kokology

static struct BurnRomInfo qzkklogyRomDesc[] = {
	{ "3.u27",			0x20000, 0xb8c27cde, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "1.u9",			0x20000, 0xce01cd54, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "4.u33",			0x20000, 0x4f5c554c, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "2.u17",			0x20000, 0x65fa1b8d, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "t2709u32.u32",	0x80000, 0x900f196c, 0x03 | BRF_GRA },           //  4 Sprites
	{ "t2709u26.u26",	0x80000, 0x416ac849, 0x03 | BRF_GRA },           //  5

	{ "t2709u42.u39",	0x80000, 0x194d5704, 0x04 | BRF_GRA },           //  6 Layer 1 tiles
	{ "t2709u39.u42",	0x80000, 0x6f95a76d, 0x04 | BRF_GRA },           //  7

	{ "t2709u47.u47",	0x80000, 0x0ebdad40, 0x06 | BRF_SND },           //  8 x1-010 Samples
	{ "t2709u55.u55",	0x80000, 0x43960c68, 0x06 | BRF_SND },           //  9
};

STD_ROM_PICK(qzkklogy)
STD_ROM_FN(qzkklogy)

static INT32 qzkklogyInit()
{
	DrvSetColorOffsets(0, 0, 0);
	DrvSetVideoOffsets(1, 1, -1, -1);

	return DrvInit(drgnunit68kInit, 8000000, SET_IRQLINES(1, 2), SPRITE_BUFFER, SET_GFX_DECODE(0, 1, -1));
}

struct BurnDriver BurnDrvQzkklogy = {
	"qzkklogy", NULL, NULL, NULL, "1992",
	"Quiz Kokology\0", NULL, "Tecmo", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA1, GBF_QUIZ, 0,
	NULL, qzkklogyRomInfo, qzkklogyRomName, NULL, NULL, NULL, NULL, QzkklogyInputInfo, QzkklogyDIPInfo,
	qzkklogyInit, DrvExit, DrvFrame, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	384, 240, 4, 3
};


// Strike Gunner S.T.G

static struct BurnRomInfo stgRomDesc[] = {
	{ "att01003.u27",	0x20000, 0x7a640a93, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "att01001.u9",	0x20000, 0x4fa88ad3, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "att01004.u33",	0x20000, 0xbbd45ca1, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "att01002.u17",	0x20000, 0x2f8fd80c, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "att01006.u32",	0x80000, 0x6ad78ea2, 0x03 | BRF_GRA },           //  4 Sprites
	{ "att01005.u26",	0x80000, 0xa347ff00, 0x03 | BRF_GRA },           //  5

	{ "att01008.u39",	0x80000, 0x20c47457, 0x04 | BRF_GRA },           //  6 Layer 1 tiles
	{ "att01007.u42",	0x80000, 0xac975544, 0x04 | BRF_GRA },           //  7

	{ "att01009.u47",	0x80000, 0x4276b58d, 0x06 | BRF_SND },           //  8 x1-010 Samples
	{ "att01010.u55",	0x80000, 0xfffb2f53, 0x06 | BRF_SND },           //  9
};

STD_ROM_PICK(stg)
STD_ROM_FN(stg)

static INT32 stgInit()
{
	DrvSetColorOffsets(0, 0, 0);
	DrvSetVideoOffsets(0, 0, -2, -2);

	return DrvInit(drgnunit68kInit, 8000000, SET_IRQLINES(2, 1), SPRITE_BUFFER, SET_GFX_DECODE(0, 1, -1));
}

struct BurnDriver BurnDrvStg = {
	"stg", NULL, NULL, NULL, "1991",
	"Strike Gunner S.T.G\0", NULL, "Athena / Tecmo", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, stgRomInfo, stgRomName, NULL, NULL, NULL, NULL, StgInputInfo, StgDIPInfo,
	stgInit, DrvExit, DrvFrame, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	240, 384, 3, 4
};


// Quiz Kokology 2

static struct BurnRomInfo qzkklgy2RomDesc[] = {
	{ "fn001001.106",	0x080000, 0x7bf8eb17, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "fn001003.107",	0x040000, 0xee6ef111, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fn001004.100",	0x100000, 0x5ba139a2, 0x03 | BRF_GRA },           //  2 Sprites

	{ "fn001005.104",	0x200000, 0x95726a63, 0x04 | BRF_GRA },           //  3 Layer 1 tiles

	{ "fn001006.105",	0x100000, 0x83f201e6, 0x06 | BRF_SND },           //  4 x1-010 Samples
};

STD_ROM_PICK(qzkklgy2)
STD_ROM_FN(qzkklgy2)

static INT32 qzkklgy2Init()
{
	DrvSetColorOffsets(0, 0, 0);
	DrvSetVideoOffsets(0, 0, -1, -3);

	return DrvInit(drgnunit68kInit, 16000000, SET_IRQLINES(1, 2), SPRITE_BUFFER, SET_GFX_DECODE(0, 2, -1));
}

struct BurnDriver BurnDrvQzkklgy2 = {
	"qzkklgy2", NULL, NULL, NULL, "1993",
	"Quiz Kokology 2\0", NULL, "Tecmo", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA1, GBF_QUIZ, 0,
	NULL, qzkklgy2RomInfo, qzkklgy2RomName, NULL, NULL, NULL, NULL, Qzkklgy2InputInfo, Qzkklgy2DIPInfo,
	qzkklgy2Init, DrvExit, DrvFrame, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	384, 240, 4, 3
};


/*
The changes between the set daioh and daioha are very minimal, the main game effects are:

 - Fixes the crashing bug in the US version caused by pressing Shot1 and Shot2 in weird orders and timings.
 - 1UP, and 2UPs no longer spawn "randomly". (Only the fixed extend items exist, and the 1UPs from score)
 - After picking up a max powerup, a 1UP or a 2UP, daoiha sets the "item timer" to a random value.
 daioh always sets it to 0x7F.
 - The powerups spawned from picking up an additional max powerup are no longer random, but feeds from the
 original "spawn item" function (thus, it advances the "item timer")

So it's a bug fix version which also makes the game a little harder by limiting the spawning of 1ups
*/

// Daioh 

static struct BurnRomInfo daiohRomDesc[] = {
	{ "fg001001.u3",	0x080000, 0xe1ef3007, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "fg001002.u4",	0x080000, 0x5e3481f9, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "fg-001-004",		0x100000, 0x9ab0533e, 0x03 | BRF_GRA },           		//  2 Sprites
	{ "fg-001-003",		0x100000, 0x1c9d51e2, 0x03 | BRF_GRA },           		//  3

	{ "fg-001-005",		0x200000, 0xc25159b9, 0x04 | BRF_GRA },           		//  4 Layer 1 tiles

	{ "fg-001-006",		0x200000, 0x2052c39a, 0x05 | BRF_GRA },           		//  5 Layer 2 tiles

	{ "fg-001-007",		0x100000, 0x4a2fe9e0, 0x06 | BRF_SND },           		//  6 x1-010 Samples
	
	{ "fg-008.u206",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  7 plds
	{ "fg-009.u14",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  8
	{ "fg-010.u35",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  9
	{ "fg-011.u36",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 10
	{ "fg-012.u76",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11
};

STD_ROM_PICK(daioh)
STD_ROM_FN(daioh)

static INT32 daiohInit()
{
	DrvSetVideoOffsets(0, 0, -1, -1);
	DrvSetColorOffsets(0, 0x400, 0x200);

	return DrvInit(daioh68kInit, 16000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 2, 2));
}

struct BurnDriver BurnDrvDaioh = {
	"daioh", NULL, NULL, NULL, "1993",
	"Daioh\0", NULL, "Athena", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, daiohRomInfo, daiohRomName, NULL, NULL, NULL, NULL, DaiohInputInfo, DaiohDIPInfo,
	daiohInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	240, 384, 3, 4
};


// Daioh (earlier)

static struct BurnRomInfo daiohaRomDesc[] = {
	{ "fg-001-001.u3",	0x080000, 0x104ae74a, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "fg-001-002.u4",	0x080000, 0xe39a4e67, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "fg-001-004",		0x100000, 0x9ab0533e, 0x03 | BRF_GRA },           		//  2 Sprites
	{ "fg-001-003",		0x100000, 0x1c9d51e2, 0x03 | BRF_GRA },           		//  3

	{ "fg-001-005",		0x200000, 0xc25159b9, 0x04 | BRF_GRA },           		//  4 Layer 1 tiles

	{ "fg-001-006",		0x200000, 0x2052c39a, 0x05 | BRF_GRA },           		//  5 Layer 2 tiles

	{ "fg-001-007",		0x100000, 0x4a2fe9e0, 0x06 | BRF_SND },           		//  6 x1-010 Samples
	
	{ "fg-008.u206",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  7 plds
	{ "fg-009.u14",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  8
	{ "fg-010.u35",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  9
	{ "fg-011.u36",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 10
	{ "fg-012.u76",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11
};

STD_ROM_PICK(daioha)
STD_ROM_FN(daioha)

struct BurnDriver BurnDrvDaioha = {
	"daioha", "daioh", NULL, NULL, "1993",
	"Daioh (earlier)\0", NULL, "Athena", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, daiohaRomInfo, daiohaRomName, NULL, NULL, NULL, NULL, DaiohInputInfo, DaiohDIPInfo,
	daiohInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	240, 384, 3, 4
};


// Daioh (prototype)

static struct BurnRomInfo daiohpRomDesc[] = {
	{ "prg_even.u3",	0x040000, 0x3c97b976, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "prg_odd.u4",		0x040000, 0xaed2b87e, 0x01 | BRF_PRG | BRF_ESS }, 		//  1
	{ "data_even.u103",	0x040000, 0xe07776ef, 0x01 | BRF_PRG | BRF_ESS }, 		//  2
	{ "data_odd.u102",	0x040000, 0xb75b9a5c, 0x01 | BRF_PRG | BRF_ESS }, 		//  3

	{ "obj_2.u146",		0x040000, 0x77560a03, 0x03 | BRF_GRA },           		//  4 Sprites
	{ "obj_6.u147",		0x040000, 0x081f5fb1, 0x03 | BRF_GRA },           		//  5
	{ "obj_3.u144",		0x040000, 0xd33ca640, 0x03 | BRF_GRA },           		//  6
	{ "obj_7.u145",		0x040000, 0xe878ac92, 0x03 | BRF_GRA },           		//  7
	{ "obj_0.u142",		0x040000, 0x78f45582, 0x03 | BRF_GRA },           		//  8
	{ "obj_4.u143",		0x040000, 0xd387de72, 0x03 | BRF_GRA },           		//  9
	{ "obj_1.u140",		0x040000, 0x8ff6c5a9, 0x03 | BRF_GRA },           		// 10
	{ "obj_5.u141",		0x040000, 0x6a671757, 0x03 | BRF_GRA },           		// 11

	{ "bg1_1.u150",		0x080000, 0xd5793a2f, 0x04 | BRF_GRA },           		// 12 Layer 1 tiles
	{ "bg1_3.u151",		0x080000, 0x6456fae1, 0x04 | BRF_GRA },           		// 13
	{ "bg1_0.u148",		0x080000, 0xbec48d7a, 0x04 | BRF_GRA },           		// 14
	{ "bg1_2.u149",		0x080000, 0x5e674c30, 0x04 | BRF_GRA },           		// 15

	{ "bg2_1.u166",		0x080000, 0x9274123b, 0x05 | BRF_GRA },           		// 16 Layer 2 tiles
	{ "bg2_3.u167",		0x080000, 0xd3d68aa1, 0x05 | BRF_GRA },           		// 17
	{ "bg2_0.u164",		0x080000, 0x7e46a10e, 0x05 | BRF_GRA },           		// 18
	{ "bg2_2.u165",		0x080000, 0x3119189b, 0x05 | BRF_GRA },           		// 19

	{ "snd0.u156",		0x020000, 0x4d253547, 0x06 | BRF_SND },           		// 20 x1-010 Samples
	{ "snd1.u157",		0x020000, 0x79b56e22, 0x06 | BRF_SND },           		// 21
	{ "snd2.u158",		0x020000, 0xbc8de02a, 0x06 | BRF_SND },           		// 22
	{ "snd3.u159",		0x020000, 0x939777fd, 0x06 | BRF_SND },           		// 23
	{ "snd4.u160",		0x020000, 0x7b97716d, 0x06 | BRF_SND },           		// 24
	{ "snd5.u161",		0x020000, 0x294e1cc9, 0x06 | BRF_SND },           		// 25
	{ "snd6.u162",		0x020000, 0xecab073b, 0x06 | BRF_SND },           		// 26
	{ "snd7.u163",		0x020000, 0x1b7ea768, 0x06 | BRF_SND },           		// 27

	{ "con1x.u35",		0x000104, 0xce8b57d9, 0x00 | BRF_OPT },           		// 28 Pals
	{ "con2x.u36",		0x000104, 0x0b18db9e, 0x00 | BRF_OPT },           		// 29
	{ "dec1x.u14",		0x000104, 0xd197abfe, 0x00 | BRF_OPT },           		// 30
	{ "dec2x.u206",		0x000104, 0x35afbba8, 0x00 | BRF_OPT },           		// 31
	{ "pcon2.u110",		0x000104, 0x082882c2, 0x00 | BRF_OPT },           		// 32
	{ "sc.u116",		0x000104, 0xe57bfde9, 0x00 | BRF_OPT },           		// 33
	
	{ "fa-011.u50",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 34 plds
	{ "fa-012.u51",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 35
	{ "fa-013.u52",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 36
	{ "fa-014.u53",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 37
	{ "fa-015.u54",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 38
};

STD_ROM_PICK(daiohp)
STD_ROM_FN(daiohp)

static INT32 daiohpInit()
{
	DrvSetVideoOffsets(1, 1, -1, -1);
	DrvSetColorOffsets(0, 0x400, 0x200);

	INT32 nRet = DrvInit(daiohp68kInit, 16000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(5, 1, 1));

	if (nRet == 0) {
		memcpy (Drv68KROM + 0x100000, Drv68KROM + 0x080000, 0x080000);
		memcpy (Drv68KROM + 0x180000, Drv68KROM + 0x080000, 0x080000);
		memcpy (Drv68KROM + 0x080000, Drv68KROM + 0x000000, 0x080000);
	}

	return nRet;
}

struct BurnDriver BurnDrvDaiohp = {
	"daiohp", "daioh", NULL, NULL, "1993",
	"Daioh (prototype)\0", NULL, "Athena", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, daiohpRomInfo, daiohpRomName, NULL, NULL, NULL, NULL, DaiohInputInfo, DaiohpDIPInfo,
	daiohpInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	240, 384, 3, 4
};


// Daioh (prototype, earlier)

static struct BurnRomInfo daiohp2RomDesc[] = {
	{ "prg_even.u3",	0x020000, 0x0079c08f, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "prg_odd.u4",		0x020000, 0xd2a843ad, 0x01 | BRF_PRG | BRF_ESS }, 		//  1
	{ "data_even.u103",	0x040000, 0xa76139bb, 0x01 | BRF_PRG | BRF_ESS }, 		//  2
	{ "data_odd.u102",	0x040000, 0x075c4b30, 0x01 | BRF_PRG | BRF_ESS }, 		//  3

	{ "obj_1.u140",		0x080000, 0x01f12e59, 0x0b | BRF_GRA },           		//  4 Sprites
	{ "obj_0.u142",		0x080000, 0x361d47ae, 0x0b | BRF_GRA },           		//  5
	{ "obj_3.u144",		0x080000, 0x68b5be19, 0x0b | BRF_GRA },           		//  6
	{ "obj_2.u146",		0x080000, 0x85f5a720, 0x0b | BRF_GRA },           		//  7

	{ "bg1_1.u150",		0x080000, 0xd5793a2f, 0x04 | BRF_GRA },           		// 12 Layer 1 tiles
	{ "bg1_3.u151",		0x080000, 0xf6912766, 0x04 | BRF_GRA },           		// 13
	{ "bg1_0.u148",		0x080000, 0xbec48d7a, 0x04 | BRF_GRA },           		// 14
	{ "bg1_2.u149",		0x080000, 0x85761988, 0x04 | BRF_GRA },           		// 15

	{ "bg2_1.u166",		0x080000, 0x9274123b, 0x05 | BRF_GRA },           		// 16 Layer 2 tiles
	{ "bg2_3.u167",		0x080000, 0x533ba782, 0x05 | BRF_GRA },           		// 17
	{ "bg2_0.u164",		0x080000, 0x7e46a10e, 0x05 | BRF_GRA },           		// 18
	{ "bg2_2.u165",		0x080000, 0xdc8ecfb7, 0x05 | BRF_GRA },           		// 19

	{ "se_0.u69",		0x080000, 0x21e4f093, 0x06 | BRF_SND },           		// 20 x1-010 Samples
	{ "se_1.u70",		0x080000, 0x593c3c58, 0x06 | BRF_SND },           		// 21

	{ "fa-023.u35",		0x000117, 0xf187ea2d, 0x00 | BRF_OPT },           		// 28 Pals
	{ "fa-024.u36",		0x000117, 0x02c87697, 0x00 | BRF_OPT },           		// 29
	{ "fa-022.u14",		0x000117, 0xf780fd0e, 0x00 | BRF_OPT },           		// 30
	{ "fa-020.u206",	0x000117, 0xcd2cd02c, 0x00 | BRF_OPT },           		// 31
	{ "fa-025.u76",		0x000117, 0x875c0c81, 0x00 | BRF_OPT },           		// 32
	{ "fa-021.u116",	0x000117, 0xe335cf2e, 0x00 | BRF_OPT },           		// 33
	
	{ "fa-011.u50",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 34 plds
	{ "fa-012.u51",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 35
	{ "fa-013.u52",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 36
	{ "fa-014.u53",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 37
	{ "fa-015.u54",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 38
};

STD_ROM_PICK(daiohp2)
STD_ROM_FN(daiohp2)

static INT32 daiohp2Init()
{
	DrvSetVideoOffsets(1, 1, -1, -1);
	DrvSetColorOffsets(0, 0x400, 0x200);

	INT32 nRet = DrvInit(daiohp68kInit, 16000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 1, 1));

	if (nRet == 0) {
		memcpy (Drv68KROM + 0x100000, Drv68KROM + 0x040000, 0x080000);
		memcpy (Drv68KROM + 0x180000, Drv68KROM + 0x040000, 0x080000);
		memcpy (Drv68KROM + 0x040000, Drv68KROM + 0x000000, 0x040000);
	}

	return nRet;
}

struct BurnDriver BurnDrvDaiohp2 = {
	"daiohp2", "daioh", NULL, NULL, "1993",
	"Daioh (prototype, earlier)\0", NULL, "Athena", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, daiohp2RomInfo, daiohp2RomName, NULL, NULL, NULL, NULL, DaiohInputInfo, DaiohDIPInfo,
	daiohp2Init, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	240, 384, 3, 4
};


// Daioh (93111A PCB conversion)
/* Found on a 93111A PCB - same PCB as War of Areo & J. J. Squawkers */

static struct BurnRomInfo daiohcRomDesc[] = {
	{ "15.u3",		0x080000, 0x14616abb, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "14.u4",		0x080000, 0xa029f991, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "9.u9",		0x080000, 0x4444cbd4, 0x03 | BRF_GRA },           //  2 Sprites
	{ "10.u10",		0x080000, 0x1d88d20b, 0x03 | BRF_GRA },           //  3
	{ "11.u11",		0x080000, 0x3e41de61, 0x03 | BRF_GRA },           //  4
	{ "12.u12",		0x080000, 0xf35e3341, 0x03 | BRF_GRA },           //  5

	{ "5.u5",		0x080000, 0xaaa5e41e, 0x04 | BRF_GRA },           //  6 Layer 1 tiles
	{ "6.u6",		0x080000, 0x9ad8b4b4, 0x04 | BRF_GRA },           //  7 
	{ "7.u7",		0x080000, 0xbabf194a, 0x04 | BRF_GRA },           //  8 
	{ "8.u8",		0x080000, 0x2db65290, 0x04 | BRF_GRA },           //  9 

	{ "1.u1",		0x080000, 0x30f81f99, 0x05 | BRF_GRA },           // 10 Layer 2 tiles
	{ "2.u2",		0x080000, 0x3b3e0f4e, 0x05 | BRF_GRA },           // 11 
	{ "3.u3",		0x080000, 0xc5eef1c1, 0x05 | BRF_GRA },           // 12 
	{ "4.u4",		0x080000, 0x851115b6, 0x05 | BRF_GRA },           // 13 

	{ "data.u69",		0x080000, 0x21e4f093, 0x06 | BRF_SND },           // 14 x1-010 Samples
	{ "data.u70",		0x080000, 0x593c3c58, 0x06 | BRF_SND },           // 15 
	
	{ "gal.u14",		0x000117, 0xb972b479, 0x00 | BRF_OPT },           // 16 Gals
};

STD_ROM_PICK(daiohc)
STD_ROM_FN(daiohc)

static INT32 daiohcInit()
{
	daiohc = 1;
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0x400, 0x200);

	INT32 nRet = DrvInit(wrofaero68kInit, 16000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 2, 2));

	if (nRet == 0) {
		memcpy (Drv68KROM + 0x100000, Drv68KROM + 0x080000, 0x080000);
		memset (Drv68KROM + 0x080000, 0, 0x080000);
	}

	return nRet;
}

struct BurnDriver BurnDrvDaiohc = {
	"daiohc", "daioh", NULL, NULL, "1993",
	"Daioh (93111A PCB conversion)\0", NULL, "Athena", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, daiohcRomInfo, daiohcRomName, NULL, NULL, NULL, NULL, DaiohInputInfo, DaiohDIPInfo,
	daiohcInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	240, 384, 3, 4
};


// Rezon
// note the ONLY byte that changes is the year, 1992 instead of 1991.

static struct BurnRomInfo rezonRomDesc[] = {
	{ "us001001.u3",	0x020000, 0xab923052, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "rezon_1_p.u4",	0x020000, 0x9ed32f8c, 0x01 | BRF_PRG | BRF_ESS }, 		//  1
	{ "us001004.103",	0x020000, 0x54871c7c, 0x01 | BRF_PRG | BRF_ESS }, 		//  2
	{ "us001003.102",	0x020000, 0x1ac3d272, 0x01 | BRF_PRG | BRF_ESS }, 		//  3

	{ "us001006.u64",	0x080000, 0xa4916e96, 0x03 | BRF_GRA },           		//  4 Sprites
	{ "us001005.u63",	0x080000, 0xe6251ebc, 0x03 | BRF_GRA },           		//  5

	{ "us001007.u66",	0x080000, 0x3760b935, 0x04 | BRF_GRA },           		//  6 Layer 1 tiles

	{ "us001008.u68",	0x080000, 0x0ab73910, 0x05 | BRF_GRA },           		//  7 Layer 2 tiles

	{ "us001009.u70",	0x100000, 0x0d7d2e2b, 0x06 | BRF_SND },           		//  8 x1-010 Samples
	
	{ "us-010.u14",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  9 plds
	{ "us-011.u35",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 10
	{ "us-012.u36",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11
	{ "us-013.u76",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 12
};

STD_ROM_PICK(rezon)
STD_ROM_FN(rezon)

static INT32 rezonInit()
{
	DrvSetVideoOffsets(0, 0, -2, -2);
	DrvSetColorOffsets(0, 0x400, 0x200);

	INT32 nRet = DrvInit(wrofaero68kInit, 16000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 2, 2));

	if (nRet == 0) {
		memcpy (Drv68KROM + 0x100000, Drv68KROM + 0x040000, 0x040000);
		memset (Drv68KROM + 0x040000, 0, 0x40000);
		BurnByteswap(DrvSndROM, 0x100000);
	}

	return nRet;
}

struct BurnDriver BurnDrvRezon = {
	"rezon", NULL, NULL, NULL, "1992",
	"Rezon\0", NULL, "Allumer", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_HORSHOOT, 0,
	NULL, rezonRomInfo, rezonRomName, NULL, NULL, NULL, NULL, RezonInputInfo, RezonDIPInfo,
	rezonInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	384, 240, 4, 3
};
   
// Rezon (earlier)

static struct BurnRomInfo rezonoRomDesc[] = {
	{ "us001001.u3",	0x020000, 0xab923052, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "us001002.u4",	0x020000, 0x3dafa0d5, 0x01 | BRF_PRG | BRF_ESS }, 		//  1
	{ "us001004.103",	0x020000, 0x54871c7c, 0x01 | BRF_PRG | BRF_ESS }, 		//  2
	{ "us001003.102",	0x020000, 0x1ac3d272, 0x01 | BRF_PRG | BRF_ESS }, 		//  3

	{ "us001006.u64",	0x080000, 0xa4916e96, 0x03 | BRF_GRA },           		//  4 Sprites
	{ "us001005.u63",	0x080000, 0xe6251ebc, 0x03 | BRF_GRA },           		//  5

	{ "us001007.u66",	0x080000, 0x3760b935, 0x04 | BRF_GRA },           		//  6 Layer 1 tiles

	{ "us001008.u68",	0x080000, 0x0ab73910, 0x05 | BRF_GRA },           		//  7 Layer 2 tiles

	{ "us001009.u70",	0x100000, 0x0d7d2e2b, 0x06 | BRF_SND },           		//  8 x1-010 Samples
	
	{ "us-010.u14",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  9 plds
	{ "us-011.u35",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 10
	{ "us-012.u36",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11
	{ "us-013.u76",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 12
};

STD_ROM_PICK(rezono)
STD_ROM_FN(rezono)

struct BurnDriver BurnDrvRezono = {
	"rezono", "rezon", NULL, NULL, "1991",
	"Rezon (earlier)\0", NULL, "Allumer", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_HORSHOOT, 0,
	NULL, rezonoRomInfo, rezonoRomName, NULL, NULL, NULL, NULL, RezonInputInfo, RezonDIPInfo,
	rezonInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	384, 240, 4, 3
};


// Eight Forces

static struct BurnRomInfo eightfrcRomDesc[] = {
	{ "uy2-u4.u3",		0x040000, 0xf1f249c5, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "uy2-u3.u4",		0x040000, 0x6f2d8618, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "u64.bin",		0x080000, 0xf561ff2e, 0x03 | BRF_GRA },           		//  2 Sprites
	{ "u63.bin",		0x080000, 0x4c3f8366, 0x03 | BRF_GRA },           		//  3

	{ "u66.bin",		0x100000, 0x6fad2b7f, 0x04 | BRF_GRA },           		//  4 Layer 1 tiles

	{ "u68.bin",		0x100000, 0xc17aad22, 0x05 | BRF_GRA },           		//  5 Layer 2 tiles

	{ "u70.bin",		0x100000, 0xdfdb67a3, 0x06 | BRF_SND },           		//  6 x1-010 Samples
	{ "u69.bin",		0x100000, 0x82ec08f1, 0x06 | BRF_SND },           		//  7
	
	{ "uy-012.u206",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  8 plds
	{ "uy-013.u14",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  9
	{ "uy-014.u35",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 10
	{ "uy-015.u36",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11
	{ "uy-016.u76",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 12
	{ "uy-017.u116",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 13
};

STD_ROM_PICK(eightfrc)
STD_ROM_FN(eightfrc)

static INT32 eightfrcInit()
{
	DrvSetVideoOffsets(3, 4, 0, 0);
	DrvSetColorOffsets(0, 0x400, 0x200);

	INT32 nRet = DrvInit(wrofaero68kInit, 16000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 2, 2));

	if (nRet == 0) {
		// Update sample length to include the banked section that was skipped (0xc0000 - 0xfffff)
		DrvROMLen[3] = 0x240000;
		memmove(DrvSndROM + 0x100000, DrvSndROM + 0x0c0000, 0x140000); // sound banks (memcpy fails because of overlap!)
		x1010_set_route(BURN_SND_X1010_ROUTE_2, 2.00, BURN_SND_ROUTE_RIGHT);
		x1010_set_route(BURN_SND_X1010_ROUTE_1, 2.00, BURN_SND_ROUTE_LEFT);
	}

	return nRet;
}

struct BurnDriver BurnDrvEightfrc = {
	"eightfrc", NULL, NULL, NULL, "1994",
	"Eight Forces\0", NULL, "Tecmo", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, eightfrcRomInfo, eightfrcRomName, NULL, NULL, NULL, NULL, EightfrcInputInfo, EightfrcDIPInfo,
	eightfrcInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	224, 384, 3, 4
};


// War of Aero - Project MEIOU

static struct BurnRomInfo wrofaeroRomDesc[] = {
	{ "u3.bin",		0x40000, 0x9b896a97, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u4.bin",		0x40000, 0xdda84846, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "u64.bin",	0x80000, 0xf06ccd78, 0x03 | BRF_GRA },           //  2 Sprites
	{ "u63.bin",	0x80000, 0x2a602a1b, 0x03 | BRF_GRA },           //  3

	{ "u66.bin",	0x80000, 0xc9fc6a0c, 0x04 | BRF_GRA },           //  4 Layer 1 tiles

	{ "u68.bin",	0x80000, 0x25c0c483, 0x05 | BRF_GRA },           //  5 Layer 2 tiles

	{ "u69.bin",	0x80000, 0x957ecd41, 0x06 | BRF_SND },           //  6 x1-010 Samples
	{ "u70.bin",	0x80000, 0x8d756fdf, 0x06 | BRF_SND },           //  7
};

STD_ROM_PICK(wrofaero)
STD_ROM_FN(wrofaero)

static INT32 wrofaeroInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0x400, 0x200);

	return DrvInit(wrofaero68kInit, 16000000, SET_IRQLINES(2, 4), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 2, 2));
}

struct BurnDriver BurnDrvWrofaero = {
	"wrofaero", NULL, NULL, NULL, "1993",
	"War of Aero - Project MEIOU\0", NULL, "Yang Cheng", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, wrofaeroRomInfo, wrofaeroRomName, NULL, NULL, NULL, NULL, WrofaeroInputInfo, WrofaeroDIPInfo,
	wrofaeroInit, DrvExit, DrvKMFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	240, 384, 3, 4
};


// Zing Zing Zip (World) / Zhen Zhen Ji Pao (China?)

static struct BurnRomInfo zingzipRomDesc[] = {
	{ "uy001001.3",		0x040000, 0x1a1687ec, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "uy001002.4",		0x040000, 0x62e3b0c4, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "uy001006.64",	0x080000, 0x46e4a7d8, 0x03 | BRF_GRA },           		//  2 Sprites
	{ "uy001005.63",	0x080000, 0x4aac128e, 0x03 | BRF_GRA },           		//  3

	{ "uy001008.66",	0x100000, 0x1dff7c4b, 0x04 | BRF_GRA },           		//  4 Layer 1 tiles
	{ "uy001007.65",	0x080000, 0xec5b3ab9, 0x1c | BRF_GRA },           		//  5
		
	{ "uy001010.68",	0x100000, 0xbdbcdf03, 0x05 | BRF_GRA },           		//  6 Layer 2 tiles

	{ "uy001011.70",	0x100000, 0xbd845f55, 0x06 | BRF_SND },           		//  7 x1-010 Samples
	
	{ "uy-012.u206",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  8 plds
	{ "uy-013.u14",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  9
	{ "uy-014.u35",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 10
	{ "uy-015.u36",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11
	{ "uy-016.u76",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 12
};

STD_ROM_PICK(zingzip)
STD_ROM_FN(zingzip)

static INT32 zingzipInit()
{
	DrvSetVideoOffsets(0, 0, -1, -2);
	DrvSetColorOffsets(0, 0x400, 0x200);

	INT32 nRet = DrvInit(wrofaero68kInit, 16000000, SET_IRQLINES(3, NOIRQ2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 3, 2));

	if (nRet == 0) {
		zingzapSetColorTable();
	}

	return nRet;
}

struct BurnDriver BurnDrvZingzip = {
	"zingzip", NULL, NULL, NULL, "1992",
	"Zing Zing Zip (World) / Zhen Zhen Ji Pao (China?)\0", NULL, "Allumer / Tecmo", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, zingzipRomInfo, zingzipRomName, NULL, NULL, NULL, NULL, ZingzipInputInfo, ZingzipDIPInfo,
	zingzipInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0xc00,
	240, 384, 3, 4
};


// Mobile Suit Gundam

static struct BurnRomInfo msgundamRomDesc[] = {
	{ "fa003002.u25",	0x080000, 0x1cc72d4c, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "fa001001.u20",	0x100000, 0xfca139d0, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fa001008.u21",	0x200000, 0xe7accf48, 0x03 | BRF_GRA },           //  2 Sprites
	{ "fa001007.u22",	0x200000, 0x793198a6, 0x03 | BRF_GRA },           //  3

	{ "fa001006.u23",	0x100000, 0x3b60365c, 0x04 | BRF_GRA },           //  4 Layer 1 tiles

	{ "fa001005.u24",	0x080000, 0x8cd7ff86, 0x05 | BRF_GRA },           //  5 Layer 2 tiles

	{ "fa001004.u26",	0x100000, 0xb965f07c, 0x06 | BRF_SND },           //  6 x1-010 Samples
};

STD_ROM_PICK(msgundam)
STD_ROM_FN(msgundam)

static INT32 msgundamInit()
{
	refresh_rate = 5666; // 56.66 hz
	DrvSetVideoOffsets(0, 0, -2, -2);
	DrvSetColorOffsets(0, 0x400, 0x200);

	INT32 nRet = DrvInit(msgundam68kInit, 16000000, SET_IRQLINES(4, 2), SPRITE_BUFFER, SET_GFX_DECODE(0, 2, 2));

	if (nRet == 0) {
		memmove (Drv68KROM + 0x100000, Drv68KROM + 0x080000, 0x100000);
		memset(Drv68KROM + 0x080000, 0, 0x080000);
	}

	return nRet;
}

struct BurnDriver BurnDrvMsgundam = {
	"msgundam", NULL, NULL, NULL, "1993",
	"Mobile Suit Gundam\0", NULL, "Banpresto / Allumer", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VSFIGHT, 0,
	NULL, msgundamRomInfo, msgundamRomName, NULL, NULL, NULL, NULL, MsgundamInputInfo, MsgundamDIPInfo,
	msgundamInit, DrvExit, DrvFrameMsgundam, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	384, 240, 4, 3
};


// Mobile Suit Gundam (Japan)

static struct BurnRomInfo msgundam1RomDesc[] = {
	{ "fa002002.u25",	0x080000, 0xdee3b083, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "fa001001.u20",	0x100000, 0xfca139d0, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "fa001008.u21",	0x200000, 0xe7accf48, 0x03 | BRF_GRA },           		//  2 Sprites
	{ "fa001007.u22",	0x200000, 0x793198a6, 0x03 | BRF_GRA },           		//  3

	{ "fa001006.u23",	0x100000, 0x3b60365c, 0x04 | BRF_GRA },           		//  4 Layer 1 tiles

	{ "fa001005.u24",	0x080000, 0x8cd7ff86, 0x05 | BRF_GRA },           		//  5 Layer 2 tiles

	{ "fa001004.u26",	0x100000, 0xb965f07c, 0x06 | BRF_SND },           		//  6 x1-010 Samples
	
	{ "fa-011.u50",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 	7 plds
	{ "fa-012.u51",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 	8
	{ "fa-013.u52",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 	9
	{ "fa-014.u53",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 10
	{ "fa-015.u54",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11
};

STD_ROM_PICK(msgundam1)
STD_ROM_FN(msgundam1)

struct BurnDriver BurnDrvMsgundam1 = {
	"msgundam1", "msgundam", NULL, NULL, "1993",
	"Mobile Suit Gundam (Japan)\0", NULL, "Banpresto / Allumer", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VSFIGHT, 0,
	NULL, msgundam1RomInfo, msgundam1RomName, NULL, NULL, NULL, NULL, MsgundamInputInfo, Msgunda1DIPInfo,
	msgundamInit, DrvExit, DrvFrameMsgundam, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	384, 240, 4, 3
};


// SD Gundam Neo Battling (Japan)

static struct BurnRomInfo neobattlRomDesc[] = {
	{ "bp923001.u45",	0x020000, 0x0d0aeb73, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "bp923002.u46",	0x020000, 0x9731fbbc, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "bp923-003.u15",	0x080000, 0x91ca98a1, 0x03 | BRF_GRA },           		//  2 Sprites
	{ "bp923-004.u9",	0x080000, 0x15c678e3, 0x03 | BRF_GRA },           		//  3

	{ "bp923-005.u4",	0x100000, 0x7c0e37be, 0x06 | BRF_SND },           		//  4 x1-010 Samples
	
	{ "bp923-007.u37",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 	5 plds
	{ "bp923-008.u38",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 	6
};

STD_ROM_PICK(neobattl)
STD_ROM_FN(neobattl)

static INT32 umanclubInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0, 0);

	return DrvInit(umanclub68kInit, 16000000, SET_IRQLINES(3, NOIRQ2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, -1, -1));
}

struct BurnDriver BurnDrvNeobattl = {
	"neobattl", NULL, NULL, NULL, "1992",
	"SD Gundam Neo Battling (Japan)\0", NULL, "Banpresto / Sotsu Agency. Sunrise", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, neobattlRomInfo, neobattlRomName, NULL, NULL, NULL, NULL, NeobattlInputInfo, NeobattlDIPInfo,
	umanclubInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	240, 384, 3, 4
};


// Ultraman Club - Tatakae! Ultraman Kyoudai!!

static struct BurnRomInfo umanclubRomDesc[] = {
	{ "uw001006.u48",	0x020000, 0x3dae1e9d, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "uw001007.u49",	0x020000, 0x5c21e702, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "bp-u-002.u2",	0x080000, 0x936cbaaa, 0x03 | BRF_GRA },           //  2 Sprites
	{ "bp-u-001.u1",	0x080000, 0x87813c48, 0x03 | BRF_GRA },           //  3

	{ "uw003.u13",		0x100000, 0xe2f718eb, 0x06 | BRF_SND },           //  4 x1-010 Samples
};

STD_ROM_PICK(umanclub)
STD_ROM_FN(umanclub)

struct BurnDriver BurnDrvUmanclub = {
	"umanclub", NULL, NULL, NULL, "1992",
	"Ultraman Club - Tatakae! Ultraman Kyoudai!!\0", NULL, "Banpresto / Tsuburaya Productions", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_SHOOT, 0,
	NULL, umanclubRomInfo, umanclubRomName, NULL, NULL, NULL, NULL, UmanclubInputInfo, UmanclubDIPInfo,
	umanclubInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	384, 240, 4, 3
};


// Masked Riders Club Battle Race

static struct BurnRomInfo kamenridRomDesc[] = {
	{ "fj001003.25",	0x080000, 0x9b65d1b9, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code

	{ "fj001005.21",	0x100000, 0x5d031333, 0x03 | BRF_GRA },           //  1 Sprites
	{ "fj001006.22",	0x100000, 0xcf28eb78, 0x03 | BRF_GRA },           //  2

	{ "fj001007.152",	0x080000, 0xd9ffe80b, 0x04 | BRF_GRA },           //  3 user1

	{ "fj001008.26",	0x100000, 0x45e2b329, 0x06 | BRF_SND },           //  4 x1-010 Samples
};

STD_ROM_PICK(kamenrid)
STD_ROM_FN(kamenrid)

static INT32 kamenridInit()
{
	DrvSetVideoOffsets(0, 0, -2, -2);
	DrvSetColorOffsets(0, 0x400, 0x200);

	return DrvInit(kamenrid68kInit, 16000000, SET_IRQLINES(2, 4/*2, NOIRQ2*/), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 2, 2));
}

struct BurnDriver BurnDrvKamenrid = {
	"kamenrid", NULL, NULL, NULL, "1993",
	"Masked Riders Club Battle Race / Kamen Rider Club Battle Racer\0", NULL, "Banpresto / Toei", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_RACING, 0,
	NULL, kamenridRomInfo, kamenridRomName, NULL, NULL, NULL, NULL, KamenridInputInfo, KamenridDIPInfo,
	kamenridInit, DrvExit, DrvKMFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	384, 240, 4, 3
};


// Mad Shark

static struct BurnRomInfo madsharkRomDesc[] = {
	{ "fq001002.201",	0x080000, 0x4286a811, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "fq001001.200",	0x080000, 0x38bfa0ad, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "fq001004.202",	0x200000, 0xe56a1b5e, 0x03 | BRF_GRA },           		//  2 Sprites

	{ "fq001006.152",	0x200000, 0x3bc5e8e4, 0x04 | BRF_GRA },           		//  3 user1
	{ "fq001005.205",	0x100000, 0x5f6c6d4a, 0x1c | BRF_GRA },           		//  4

	{ "fq001007.26",	0x100000, 0xe4b33c13, 0x06 | BRF_SND },           		//  5 x1-010 Samples
	
	{ "fq-008.u50",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 	7 plds
	{ "fq-009.u51",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 	8
	{ "fq-010.u52",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 	9
	{ "fq-011.u53",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 10
	{ "fq-012.u54",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11
};

STD_ROM_PICK(madshark)
STD_ROM_FN(madshark)

static INT32 madsharkInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0x200, 0xa00);

	INT32 nRet = DrvInit(madshark68kInit, 16000000, SET_IRQLINES(4, 2/*2, NOIRQ2*/), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 3, 3));

	if (nRet == 0) {
		jjsquawkSetColorTable();
	}

	return nRet;
}

static INT32 madsharkExit()
{
	BurnFree (DrvGfxROM2);

	return DrvExit();
}

struct BurnDriver BurnDrvMadshark = {
	"madshark", NULL, NULL, NULL, "1993",
	"Mad Shark\0", NULL, "Allumer", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, madsharkRomInfo, madsharkRomName, NULL, NULL, NULL, NULL, MadsharkInputInfo, MadsharkDIPInfo,
	madsharkInit, madsharkExit, DrvKMFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	224, 384, 3, 4
};


// Wit's (Japan)

static struct BurnRomInfo witsRomDesc[] = {
	{ "un001001.u1",	0x08000, 0x416c567e, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "un001002.u4",	0x08000, 0x497a3fa6, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "un001008.7l",	0x20000, 0x1d5d0b2b, 0x0b | BRF_GRA },           //  2 Sprites
	{ "un001007.5l",	0x20000, 0x9e1e6d51, 0x0b | BRF_GRA },           //  3
	{ "un001006.4l",	0x20000, 0x98a980d4, 0x0b | BRF_GRA },           //  4
	{ "un001005.2l",	0x20000, 0x6f2ce3c0, 0x0b | BRF_GRA },           //  5

	{ "un001004.12a",	0x20000, 0xa15ff938, 0x06 | BRF_SND },           //  6 x1-010 Samples
	{ "un001003.10a",	0x20000, 0x3f4b9e55, 0x06 | BRF_SND },           //  7
};

STD_ROM_PICK(wits)
STD_ROM_FN(wits)

static INT32 witsInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0, 0);

	return DrvInit(thunderl68kInit, 8000000, SET_IRQLINES(2, NOIRQ2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, -1, -1));
}

struct BurnDriver BurnDrvWits = {
	"wits", NULL, NULL, NULL, "1989",
	"Wit's (Japan)\0", NULL, "Athena (Visco license)", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_SETA1, GBF_MAZE, 0,
	NULL, witsRomInfo, witsRomName, NULL, NULL, NULL, NULL, WitsInputInfo, WitsDIPInfo,
	witsInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	384, 240, 4, 3
};


// Thunder & Lightning (set 1)

static struct BurnRomInfo thunderlRomDesc[] = {
	{ "m4",			0x08000, 0x1e6b9462, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "m5",			0x08000, 0x7e82793e, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "t17",		0x20000, 0x599a632a, 0x0b | BRF_GRA },           //  2 Sprites
	{ "t16",		0x20000, 0x3aeef91c, 0x0b | BRF_GRA },           //  3
	{ "t15",		0x20000, 0xb97a7b56, 0x0b | BRF_GRA },           //  4
	{ "t14",		0x20000, 0x79c707be, 0x0b | BRF_GRA },           //  5

	{ "r28",		0x80000, 0xa043615d, 0x06 | BRF_SND },           //  6 x1-010 Samples
	{ "r27",		0x80000, 0xcb8425a3, 0x06 | BRF_SND },           //  7
	
	{ "tl-9",		0x00117, 0x3b62882d, 0x00 | BRF_OPT },			 //  8 plds
};

STD_ROM_PICK(thunderl)
STD_ROM_FN(thunderl)

struct BurnDriver BurnDrvThunderl = {
	"thunderl", NULL, NULL, NULL, "1990",
	"Thunder & Lightning (set 1)\0", NULL, "Seta", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_BREAKOUT, 0,
	NULL, thunderlRomInfo, thunderlRomName, NULL, NULL, NULL, NULL, ThunderlInputInfo, ThunderlDIPInfo,
	witsInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	240, 384, 3, 4
};


// Thunder & Lightning (set 2)

static struct BurnRomInfo thunderlaRomDesc[] = {
	{ "tl-1-1.u1",	0x08000, 0x3d4b1888, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tl-1-2.u4",	0x08000, 0x974dddda, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "t17",		0x20000, 0x599a632a, 0x0b | BRF_GRA },           //  2 Sprites
	{ "t16",		0x20000, 0x3aeef91c, 0x0b | BRF_GRA },           //  3
	{ "t15",		0x20000, 0xb97a7b56, 0x0b | BRF_GRA },           //  4
	{ "t14",		0x20000, 0x79c707be, 0x0b | BRF_GRA },           //  5

	{ "r28",		0x80000, 0xa043615d, 0x06 | BRF_SND },           //  6 x1-010 Samples
	{ "r27",		0x80000, 0xcb8425a3, 0x06 | BRF_SND },           //  7
	
	{ "tl-9",		0x00117, 0x3b62882d, 0x00 | BRF_OPT },			 //  8 plds
};

STD_ROM_PICK(thunderla)
STD_ROM_FN(thunderla)

struct BurnDriver BurnDrvThunderla = {
	"thunderla", "thunderl", NULL, NULL, "1990",
	"Thunder & Lightning (set 2)\0", NULL, "Seta", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_BREAKOUT, 0,
	NULL, thunderlaRomInfo, thunderlaRomName, NULL, NULL, NULL, NULL, ThunderlInputInfo, ThunderlDIPInfo,
	witsInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	240, 384, 3, 4
};


// Athena no Hatena ?

static struct BurnRomInfo atehateRomDesc[] = {
	{ "fs001001.evn",	0x080000, 0x4af1f273, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "fs001002.odd",	0x080000, 0xc7ca7a85, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fs001003.gfx",	0x200000, 0x8b17e431, 0x03 | BRF_GRA },           //  2 Sprites

	{ "fs001004.pcm",	0x100000, 0xf9344ce5, 0x06 | BRF_SND },           //  3 x1-010 Samples
};

STD_ROM_PICK(atehate)
STD_ROM_FN(atehate)

static INT32 atehateInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0, 0);

	return DrvInit(atehate68kInit, 16000000, SET_IRQLINES(2, 1), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, -1, -1));
}

struct BurnDriver BurnDrvAtehate = {
	"atehate", NULL, NULL, NULL, "1993",
	"Athena no Hatena?\0", NULL, "Athena", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA1, GBF_QUIZ, 0,
	NULL, atehateRomInfo, atehateRomName, NULL, NULL, NULL, NULL, AtehateInputInfo, AtehateDIPInfo,
	atehateInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	384, 240, 4, 3
};


// Block Carnival / Thunder & Lightning 2

static struct BurnRomInfo blockcarRomDesc[] = {
	{ "u1.a1",				0x020000, 0x4313fb00, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u4.a3",				0x020000, 0x2237196d, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "bl-chr-0.u6.j3",		0x080000, 0xa33300ca, 0x03 | BRF_GRA },           //  2 Sprites
	{ "bl-chr-1.u9.l3",		0x080000, 0x563de808, 0x03 | BRF_GRA },           //  3

	{ "bl-snd-0.u39.a13",	0x100000, 0x9c2130a2, 0x06 | BRF_SND },           //  4 x1-010 Samples
};

STD_ROM_PICK(blockcar)
STD_ROM_FN(blockcar)

static INT32 blockcarInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0, 0);

	return DrvInit(blockcar68kInit, 8000000, SET_IRQLINES(3, NOIRQ2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, -1, -1));
}

struct BurnDriver BurnDrvBlockcar = {
	"blockcar", NULL, NULL, NULL, "1992",
	"Block Carnival / Thunder & Lightning 2\0", NULL, "Visco", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_BREAKOUT, 0,
	NULL, blockcarRomInfo, blockcarRomName, NULL, NULL, NULL, NULL, BlockcarInputInfo, BlockcarDIPInfo,
	blockcarInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	240, 384, 3, 4
};


// Zombie Raid (9/28/95, US)

static struct BurnRomInfo zombraidRomDesc[] = {
	{ "fy001003.3",		0x080000, 0x0b34b8f7, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "fy001004.4",		0x080000, 0x71bfeb1a, 0x01 | BRF_PRG | BRF_ESS }, 		//  1
	{ "fy001002.103",	0x080000, 0x313fd68f, 0x01 | BRF_PRG | BRF_ESS }, 		//  2
	{ "fy001001.102",	0x080000, 0xa0f61f13, 0x01 | BRF_PRG | BRF_ESS }, 		//  3

	{ "fy001006.200",	0x200000, 0xe9ae99f7, 0x03 | BRF_GRA },           		//  4 Sprites

	{ "fy001008.66",	0x200000, 0x73d7b0e1, 0x04 | BRF_GRA },           		//  5 Layer 1 tiles
	{ "fy001007.65",	0x100000, 0xb2fc2c81, 0x1c | BRF_GRA },           		//  6

	{ "fy001010.68",	0x200000, 0x8b40ed7a, 0x05 | BRF_GRA },           		//  7 Layer 2 tiles
	{ "fy001009.67",	0x100000, 0x6bcca641, 0x1d | BRF_GRA },		  	  		//  8

	{ "fy001012.b",		0x200000, 0xfd30e102, 0x06 | BRF_SND },           		//  9 x1-010 Samples
	{ "fy001011.a",		0x200000, 0xe3c431de, 0x06 | BRF_SND },           		// 10
	
	{ "nvram.bin",		0x010000, 0x1a4b2ee8, 0x00 | BRF_OPT },
	
	{ "fy-001.u206",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11 plds
	{ "fy-002.u116",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 12
	{ "fy-003.u14",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 13
	{ "fy-004.u35",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 14
	{ "fy-005.u36",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 15
	{ "fy-006.u76",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 16
};

STD_ROM_PICK(zombraid)
STD_ROM_FN(zombraid)

static INT32 zombraidInit()
{
	DrvSetVideoOffsets(0, 0, -2, -2);
	DrvSetColorOffsets(0, 0x200, 0xa00);

	INT32 nRet = DrvInit(zombraid68kInit, 16000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 3, 3));

	if (nRet == 0) {
		gundharaSetColorTable();
	}

	return nRet;
}

struct BurnDriver BurnDrvZombraid = {
	"zombraid", NULL, NULL, NULL, "1995",
	"Zombie Raid (9/28/95, US)\0", NULL, "American Sammy", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_SHOOT, 0,
	NULL, zombraidRomInfo, zombraidRomName, NULL, NULL, NULL, NULL, ZombraidInputInfo, ZombraidDIPInfo,
	zombraidInit, DrvExit, DrvFrame, zombraidDraw, DrvScan, &DrvRecalc, 0x1200,
	384, 240, 4, 3
};


// Zombie Raid (9/28/95, US, prototype PCB)
/* Prototype or test board version.  Data matches released MASK rom version */

static struct BurnRomInfo zombraidpRomDesc[] = {
	{ "u3_master_usa_prg_e_l_dd28.u3",		0x080000, 0x0b34b8f7, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u4_master_usa_prg_o_l_5e2b.u4",		0x080000, 0x71bfeb1a, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "u103_master_usa_prg_e_h_789e.u103",	0x080000, 0x313fd68f, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "u102_master_usa_prg_o_h_1f25.u102",	0x080000, 0xa0f61f13, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "u142_master_obj_00_1bb3.u142",	0x040000, 0xed6c8541, 0x0b | BRF_GRA },           //  4 Sprites
	{ "obj_01",							0x040000, 0xa423620e, 0x0b | BRF_GRA },           //  5 
	{ "u143_master_obj_04_b5aa.u143",	0x040000, 0x1242670d, 0x0b | BRF_GRA },           //  6 
	{ "obj_05",							0x040000, 0x57fe3e97, 0x0b | BRF_GRA },           //  7 
	{ "u146_master_obj_02_6cc6.u146",	0x040000, 0x7562ee1b, 0x0b | BRF_GRA },           //  8 
	{ "u144_master_obj_03_1cb5.u144",	0x040000, 0xa83040f1, 0x0b | BRF_GRA },           //  9 
	{ "u147_master_obj_06_c3d8.u147",	0x040000, 0xa32c3da8, 0x0b | BRF_GRA },           // 10 
	{ "u145_master_obj_07_8ad4.u145",	0x040000, 0x8071f0b6, 0x0b | BRF_GRA },           // 11 

	{ "u148_master_scn_1-0_3ef8.u148",	0x080000, 0x7d722f2a, 0x1c | BRF_GRA },           // 12 Layer 1 tiles
	{ "u150_master_scn_1-1_89a6.u150",	0x080000, 0x3c62a8af, 0x1c | BRF_GRA },           // 13 
	{ "u149_master_scn_1-3_71bb.u149",	0x080000, 0x70d6af7f, 0x1c | BRF_GRA },           // 14 
	{ "u151_master_scn_1-4_872e.u151",	0x080000, 0x83ef4d5f, 0x1c | BRF_GRA },           // 15 
	{ "u154_master_scn_1-2_0f4b.u154",	0x080000, 0x0a1d647c, 0x1c | BRF_GRA },           // 16 
	{ "u155_master_scn_1-5_daef.u155",	0x080000, 0x2508f67f, 0x1c | BRF_GRA },           // 17 
	
	{ "u164_master_scn_2-0_e79c.u164",	0x080000, 0xf8c89062, 0x1d | BRF_GRA },           // 18 Layer 2 tiles
	{ "u166_master_scn_2-1_0b75.u166",	0x080000, 0x4d7a72d5, 0x1d | BRF_GRA },           // 19
	{ "u165_master_scn_2-3_be68.u165",	0x080000, 0x8aaaef08, 0x1d | BRF_GRA },           // 20 
	{ "u167_master_scn_2-4_c515.u167",	0x080000, 0xd22ff5c1, 0x1d | BRF_GRA },           // 21
	{ "u152_master_scn_2-2_c00e.u152",	0x080000, 0x0870ad58, 0x1d | BRF_GRA },           // 22 
	{ "u153_master_scn_2-5_e1da.u153",	0x080000, 0x814ac66a, 0x1d | BRF_GRA },           // 23 	
	
	{ "u156_master_snd_0_f630.u156",	0x080000, 0xbfc467bd, 0x06 | BRF_SND },           // 24 x1-010 Samples
	{ "u157_master_snd_1_c20a.u157",	0x080000, 0xb449a8ba, 0x06 | BRF_SND },           // 25
	{ "u158_master_snd_2_5c69.u158",	0x080000, 0xed6de791, 0x06 | BRF_SND },           // 26
	{ "u159_master_snd_3_0727.u159",	0x080000, 0x794cec21, 0x06 | BRF_SND },           // 27
	{ "u160_master_snd_4_5a70.u160",	0x080000, 0xe81ace66, 0x06 | BRF_SND },           // 28
	{ "u161_master_snd_5_599c.u161",	0x080000, 0x1793dd13, 0x06 | BRF_SND },           // 29
	{ "u162_master_snd_6_6d2e.u162",	0x080000, 0x2ece241f, 0x06 | BRF_SND },           // 30
	{ "u163_master_snd_7_c733.u163",	0x080000, 0xd90f78b2, 0x06 | BRF_SND },           // 31
	
	{ "nvram.bin",		0x010000, 0x1a4b2ee8, 0x00 | BRF_OPT },
};

STD_ROM_PICK(zombraidp)
STD_ROM_FN(zombraidp)

static INT32 zombraidpRomCallback(INT32 bLoad)
{
	if (!bLoad)
	{
		DrvROMLen[0] = 0x200000; // gfx0
		DrvROMLen[1] = 0x400000; // gfx1
		DrvROMLen[2] = 0x400000; // gfx2
		DrvROMLen[3] = 0x480000; // sound rom
	}
	else
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100000,  3, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080001,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100001,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x180001, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x180000, 11, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001, 13, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000, 14, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100001, 15, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x200000, 16, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x300000, 17, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 18, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, 19, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100000, 20, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100001, 21, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x200000, 22, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x300000, 23, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 24, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x080000, 25, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x100000, 26, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x180000, 27, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x200000, 28, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x280000, 29, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x300000, 30, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x380000, 31, 1)) return 1;
	}

	return 0;
}

static INT32 zombraidpInit()
{
	DrvSetVideoOffsets(0, 0, -2, -2);
	DrvSetColorOffsets(0, 0x200, 0xa00);

	pRomLoadCallback = zombraidpRomCallback;

	INT32 nRet = DrvInit(zombraid68kInit, 16000000, SET_IRQLINES(2, 4), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 3, 3));

	if (nRet == 0) {
		gundharaSetColorTable();
	}

	return nRet;
}

struct BurnDriver BurnDrvZombraidp = {
	"zombraidp", "zombraid", NULL, NULL, "1995",
	"Zombie Raid (9/28/95, US, prototype PCB)\0", NULL, "American Sammy", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_SHOOT, 0,
	NULL, zombraidpRomInfo, zombraidpRomName, NULL, NULL, NULL, NULL, ZombraidInputInfo, ZombraidDIPInfo,
	zombraidpInit, DrvExit, DrvFrame, zombraidDraw, DrvScan, &DrvRecalc, 0x1200,
	384, 240, 4, 3
};


// Zombie Raid (9/28/95, Japan, prototype PCB)
/* Prototype or test board version.  Data matches released MASK rom version */

static struct BurnRomInfo zombraidpjRomDesc[] = {
	{ "u3_master_usa_prg_e_l_dd28.u3",		0x080000, 0x0b34b8f7, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u4_master_jpn_prg_o_l_5e2c.u4",		0x080000, 0x3cb6bdf0, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "u103_master_usa_prg_e_h_789e.u103",	0x080000, 0x313fd68f, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "u102_master_usa_prg_o_h_1f25.u102",	0x080000, 0xa0f61f13, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "u142_master_obj_00_1bb3.u142",	0x040000, 0xed6c8541, 0x0b | BRF_GRA },           //  4 Sprites
	{ "obj_01",							0x040000, 0xa423620e, 0x0b | BRF_GRA },           //  5 
	{ "u143_master_obj_04_b5aa.u143",	0x040000, 0x1242670d, 0x0b | BRF_GRA },           //  6 
	{ "obj_05",							0x040000, 0x57fe3e97, 0x0b | BRF_GRA },           //  7 
	{ "u146_master_obj_02_6cc6.u146",	0x040000, 0x7562ee1b, 0x0b | BRF_GRA },           //  8 
	{ "u144_master_obj_03_1cb5.u144",	0x040000, 0xa83040f1, 0x0b | BRF_GRA },           //  9 
	{ "u147_master_obj_06_c3d8.u147",	0x040000, 0xa32c3da8, 0x0b | BRF_GRA },           // 10 
	{ "u145_master_obj_07_8ad4.u145",	0x040000, 0x8071f0b6, 0x0b | BRF_GRA },           // 11 
	
	{ "u148_master_scn_1-0_3ef8.u148",	0x080000, 0x7d722f2a, 0x1c | BRF_GRA },           // 12 Layer 1 tiles
	{ "u150_master_scn_1-1_89a6.u150",	0x080000, 0x3c62a8af, 0x1c | BRF_GRA },           // 13 
	{ "u149_master_scn_1-3_71bb.u149",	0x080000, 0x70d6af7f, 0x1c | BRF_GRA },           // 14 
	{ "u151_master_scn_1-4_872e.u151",	0x080000, 0x83ef4d5f, 0x1c | BRF_GRA },           // 15 
	{ "u154_master_scn_1-2_0f4b.u154",	0x080000, 0x0a1d647c, 0x1c | BRF_GRA },           // 16 
	{ "u155_master_scn_1-5_daef.u155",	0x080000, 0x2508f67f, 0x1c | BRF_GRA },           // 17 
	
	{ "u164_master_scn_2-0_e79c.u164",	0x080000, 0xf8c89062, 0x1d | BRF_GRA },           // 18 Layer 2 tiles
	{ "u166_master_scn_2-1_0b75.u166",	0x080000, 0x4d7a72d5, 0x1d | BRF_GRA },           // 19
	{ "u165_master_scn_2-3_be68.u165",	0x080000, 0x8aaaef08, 0x1d | BRF_GRA },           // 20 
	{ "u167_master_scn_2-4_c515.u167",	0x080000, 0xd22ff5c1, 0x1d | BRF_GRA },           // 21
	{ "u152_master_scn_2-2_c00e.u152",	0x080000, 0x0870ad58, 0x1d | BRF_GRA },           // 22 
	{ "u153_master_scn_2-5_e1da.u153",	0x080000, 0x814ac66a, 0x1d | BRF_GRA },           // 23 	
	
	{ "u156_master_snd_0_f630.u156",	0x080000, 0xbfc467bd, 0x06 | BRF_SND },           // 24 x1-010 Samples
	{ "u157_master_snd_1_c20a.u157",	0x080000, 0xb449a8ba, 0x06 | BRF_SND },           // 25
	{ "u158_master_snd_2_5c69.u158",	0x080000, 0xed6de791, 0x06 | BRF_SND },           // 26
	{ "u159_master_snd_3_0727.u159",	0x080000, 0x794cec21, 0x06 | BRF_SND },           // 27
	{ "u160_master_snd_4_5a70.u160",	0x080000, 0xe81ace66, 0x06 | BRF_SND },           // 28
	{ "u161_master_snd_5_599c.u161",	0x080000, 0x1793dd13, 0x06 | BRF_SND },           // 29
	{ "u162_master_snd_6_6d2e.u162",	0x080000, 0x2ece241f, 0x06 | BRF_SND },           // 30
	{ "u163_master_snd_7_c733.u163",	0x080000, 0xd90f78b2, 0x06 | BRF_SND },           // 31
	
	{ "nvram.bin",		0x010000, 0x1a4b2ee8, 0x00 | BRF_OPT },
};

STD_ROM_PICK(zombraidpj)
STD_ROM_FN(zombraidpj)

struct BurnDriver BurnDrvZombraidpj = {
	"zombraidpj", "zombraid", NULL, NULL, "1995",
	"Zombie Raid (9/28/95, Japan, prototype PCB)\0", NULL, "Sammy Industries Co.,Ltd.", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_SHOOT, 0,
	NULL, zombraidpjRomInfo, zombraidpjRomName, NULL, NULL, NULL, NULL, ZombraidInputInfo, ZombraidDIPInfo,
	zombraidpInit, DrvExit, DrvFrame, zombraidDraw, DrvScan, &DrvRecalc, 0x1200,
	384, 240, 4, 3
};


// Gundhara

static struct BurnRomInfo gundharaRomDesc[] = {
	{ "bpgh-003.u3",	0x080000, 0x14e9970a, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "bpgh-004.u4",	0x080000, 0x96dfc658, 0x01 | BRF_PRG | BRF_ESS }, 		//  1
	{ "bpgh-002.103",	0x080000, 0x312f58e2, 0x01 | BRF_PRG | BRF_ESS }, 		//  2
	{ "bpgh-001.102",	0x080000, 0x8d23a23c, 0x01 | BRF_PRG | BRF_ESS }, 		//  3

	{ "bpgh-008.u64",	0x200000, 0x7ed9d272, 0x03 | BRF_GRA },           		//  4 Sprites
	{ "bpgh-006.201",	0x200000, 0x5a81411d, 0x03 | BRF_GRA },           		//  5
	{ "bpgh-007.u63",	0x200000, 0xaa49ce7b, 0x03 | BRF_GRA },           		//  6
	{ "bpgh-005.200",	0x200000, 0x74138266, 0x03 | BRF_GRA },           		//  7

	{ "bpgh-010.u66",	0x100000, 0xb742f0b8, 0x04 | BRF_GRA },           		//  8 Layer 1 tiles
	{ "bpgh-009.u65",	0x080000, 0xb768e666, 0x1c | BRF_GRA },           		//  9

	{ "bpgh-012.u68",	0x200000, 0xedfda595, 0x05 | BRF_GRA },           		// 10 Layer 2 tiles
	{ "bpgh-011.u67",	0x100000, 0x49aff270, 0x1d | BRF_GRA },		  	  		// 11

	{ "bpgh-013.u70",	0x100000, 0x0fa5d503, 0x06 | BRF_SND },           		// 12 x1-010 Samples
	
	{ "fx-001.u206",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 13 plds
	{ "fx-002.u116",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 14
	{ "fx-003.u14",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 15
	{ "fx-004.u35",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 16
	{ "fx-005.u36",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 10
	{ "fx-006.u76",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11
};

STD_ROM_PICK(gundhara)
STD_ROM_FN(gundhara)

static void gundhara68kInit()
{
	wrofaero68kInit();

	// swap halves of sound rom
	memcpy (DrvSndROM + 0x100000, DrvSndROM + 0x000000, 0x080000); // temp storage

	memcpy (DrvSndROM + 0x000000, DrvSndROM + 0x080000, 0x080000);
	memcpy (DrvSndROM + 0x080000, DrvSndROM + 0x100000, 0x080000);
}

static INT32 gundharaInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0x200, 0xa00);

	INT32 nRet = DrvInit(gundhara68kInit, 16000000, SET_IRQLINES(2, 4), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 3, 3));

	if (nRet == 0) {
		gundharaSetColorTable();
	}

	return nRet;
}

struct BurnDriver BurnDrvGundhara = {
	"gundhara", NULL, NULL, NULL, "1995",
	"Gundhara\0", NULL, "Banpresto", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_RUNGUN, 0,
	NULL, gundharaRomInfo, gundharaRomName, NULL, NULL, NULL, NULL, GundharaInputInfo, GundharaDIPInfo,
	gundharaInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	240, 384, 3, 4
};


// Gundhara (Chinese, bootleg?)

static struct BurnRomInfo gundharacRomDesc[] = {
	{ "4.u3",			0x080000, 0x14e9970a, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "2.u4",			0x080000, 0x96dfc658, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "3.u103",			0x080000, 0x312f58e2, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "1.u102",			0x080000, 0x8d23a23c, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "19.u140",		0x080000, 0x32d92c28, 0x0b | BRF_GRA },           //  4 Sprites
	{ "23.u142",		0x080000, 0xff44db9b, 0x0b | BRF_GRA },           //  5
	{ "21.u141",		0x080000, 0x1901dc08, 0x0b | BRF_GRA },           //  6
	{ "25.u143",		0x080000, 0x877289a2, 0x0b | BRF_GRA },           //  7
	{ "18.u140-b",		0x080000, 0x4f023fb0, 0x0b | BRF_GRA },           //  8
	{ "22.u142-b",		0x080000, 0x6f3fe7e7, 0x0b | BRF_GRA },           //  9
	{ "20.u141-b",		0x080000, 0x7f1932e0, 0x0b | BRF_GRA },           // 10
	{ "24.u143-b",		0x080000, 0x066a2e2b, 0x0b | BRF_GRA },           // 11
	{ "9.u144",			0x080000, 0x6b4a531f, 0x0b | BRF_GRA },           // 12
	{ "13.u146",		0x080000, 0x45be3df4, 0x0b | BRF_GRA },           // 13
	{ "11.u145",		0x080000, 0xf5210aa5, 0x0b | BRF_GRA },           // 14
	{ "15.u147",		0x080000, 0x17003119, 0x0b | BRF_GRA },           // 15
	{ "8.u144-b",		0x080000, 0xad9d9338, 0x0b | BRF_GRA },           // 16
	{ "12.u146-b",		0x080000, 0x0fd4c062, 0x0b | BRF_GRA },           // 17
	{ "10.u145-b",		0x080000, 0x7c5d12b9, 0x0b | BRF_GRA },           // 18
	{ "14.u147-b",		0x080000, 0x5a8af50f, 0x0b | BRF_GRA },           // 19

	{ "5.u148",			0x080000, 0x0c740f9b, 0x1c | BRF_GRA },           // 20 Layer 1 tiles
	{ "6.u150",			0x080000, 0xba60eb98, 0x1c | BRF_GRA },           // 21
	{ "7.u154",			0x080000, 0xb768e666, 0x1c | BRF_GRA },           // 22

	{ "26.u164",		0x080000, 0xbe3ccaba, 0x1d | BRF_GRA },           // 23 Layer 2 tiles
	{ "28.u166",		0x080000, 0x8a650a4e, 0x1d | BRF_GRA },		  	  // 24
	{ "27.u165",		0x080000, 0x47994ff0, 0x1d | BRF_GRA },		  	  // 25
	{ "29.u167",		0x080000, 0x453c3d3f, 0x1d | BRF_GRA },		  	  // 26
	{ "16.u152",		0x080000, 0x5ccc500b, 0x1d | BRF_GRA },		  	  // 27
	{ "17.u153",		0x080000, 0x5586d086, 0x1d | BRF_GRA },		  	  // 28

	{ "30.u69",			0x080000, 0x3111a98a, 0x06 | BRF_SND },           // 29 x1-010 Samples
	{ "31.u70",			0x080000, 0x30cb2524, 0x06 | BRF_SND },           // 30 
};

STD_ROM_PICK(gundharac)
STD_ROM_FN(gundharac)

static INT32 gundharacRomCallback(INT32 bLoad)
{
	if (!bLoad)
	{
		DrvROMLen[0] = 0x800000; // gfx0
		DrvROMLen[1] = 0x200000; // gfx1
		DrvROMLen[2] = 0x400000; // gfx2
		DrvROMLen[3] = 0x100000; // sound rom
	}
	else
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100000,  3, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x200000,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x200001,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x300000, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x300001, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x400000, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x400001, 13, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x500000, 14, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x500001, 15, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x600000, 16, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x600001, 17, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x700000, 18, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x700001, 19, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, 20, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001, 21, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000, 22, 2)) return 1;
		
		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 23, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, 24, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100000, 25, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100001, 26, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x200000, 27, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x300000, 28, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x080000, 29, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x000000, 30, 1)) return 1;
	}

	return 0;
}

static INT32 gundharacInit()
{
	DrvSetVideoOffsets(0, 0, -2, -2);
	DrvSetColorOffsets(0, 0x200, 0xa00);
	
	pRomLoadCallback = gundharacRomCallback;

	INT32 nRet = DrvInit(gundhara68kInit, 16000000, SET_IRQLINES(2, 4), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 3, 3));

	if (nRet == 0) {
		gundharaSetColorTable();
	}

	return nRet;
}

struct BurnDriver BurnDrvGundharac = {
	"gundharac", "gundhara", NULL, NULL, "1995",
	"Gundhara (Chinese, bootleg?)\0", NULL, "Banpresto", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_RUNGUN, 0,
	NULL, gundharacRomInfo, gundharacRomName, NULL, NULL, NULL, NULL, GundharaInputInfo, GundharaDIPInfo,
	gundharacInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	240, 384, 3, 4
};


// Blandia

static struct BurnRomInfo blandiaRomDesc[] = {
	{ "ux001001.u3",	0x040000, 0x2376a1f3, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ux001002.u4",	0x040000, 0xb915e172, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ux001003.u202",	0x100000, 0x98052c63, 0x01 | BRF_PRG | BRF_ESS }, //  2

	{ "ux001008.u64",	0x100000, 0x413647b6, 0x03 | BRF_GRA },           //  6
	{ "ux001007.u201",	0x100000, 0x4440fdd1, 0x03 | BRF_GRA },           //  4
	{ "ux001006.u63",	0x100000, 0xabc01cf7, 0x03 | BRF_GRA },           //  5
	{ "ux001005.u200",	0x100000, 0xbea0c4a5, 0x03 | BRF_GRA },           //  3 Sprites

	{ "ux001009.u65",	0x080000, 0xbc6f6aea, 0x04 | BRF_GRA },           //  7 Layer 1 tiles
	{ "ux001010.u66",	0x080000, 0xbd7f7614, 0x04 | BRF_GRA },           //  8

	{ "ux001011.u67",	0x080000, 0x5efe0397, 0x05 | BRF_GRA },           //  9 Layer 2 tiles
	{ "ux001012.u068",	0x080000, 0xf29959f6, 0x05 | BRF_GRA },           // 10

	{ "ux001013.u69",	0x100000, 0x5cd273cd, 0x06 | BRF_SND },           // 11 x1-010 Samples
	{ "ux001014.u70",	0x080000, 0x86b49b4e, 0x06 | BRF_SND },           // 12
	
	{ "ux-015.u206",	0x000117, 0x08cddbdd, 0x00 | BRF_OPT },			  // 13 plds
	{ "ux-016.u116",	0x000117, 0x9734f1af, 0x00 | BRF_OPT },			  // 14
	{ "ux-017.u14",		0x000117, 0x9e95d8d5, 0x00 | BRF_OPT },			  // 15
	{ "ux-018.u35",		0x000117, 0xc9579473, 0x00 | BRF_OPT },			  // 16
	{ "ux-019.u36",		0x000117, 0xd85c359d, 0x00 | BRF_OPT },			  // 17
	{ "ux-020.u76",		0x000117, 0x116278bf, 0x00 | BRF_OPT },			  // 15
};

STD_ROM_PICK(blandia)
STD_ROM_FN(blandia)

static INT32 blandiaInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0xa00, 0x200);

	INT32 nRet = DrvInit(blandia68kInit, 16000000, SET_IRQLINES(2, 4), SPRITE_BUFFER, SET_GFX_DECODE(0, 4, 4));

	if (nRet == 0) {
		// Update sample length to include the banked section that was skipped (0xc0000 - 0xfffff)
		DrvROMLen[3] = 0x1c0000;
		blandiaSetColorTable();
	}

	return nRet;
}

struct BurnDriver BurnDrvBlandia = {
	"blandia", NULL, NULL, NULL, "1992",
	"Blandia\0", NULL, "Allumer", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VSFIGHT, 0,
	NULL, blandiaRomInfo, blandiaRomName, NULL, NULL, NULL, NULL, BlandiaInputInfo, BlandiaDIPInfo,
	blandiaInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	384, 240, 4, 3
};


// Blandia (prototype)

static struct BurnRomInfo blandiapRomDesc[] = {
	{ "prg-even.bin",	0x40000, 0x7ecd30e8, 0x01 | BRF_PRG | BRF_ESS }, 	//  0 68k Code
	{ "prg-odd.bin",	0x40000, 0x42b86c15, 0x01 | BRF_PRG | BRF_ESS }, 	//  1
	{ "tbl0.bin",		0x80000, 0x69b79eb8, 0x01 | BRF_PRG | BRF_ESS }, 	//  2
	{ "tbl1.bin",		0x80000, 0xcf2fd350, 0x01 | BRF_PRG | BRF_ESS }, 	//  3

	{ "o-1.bin",		0x80000, 0x4c67b7f0, 0x0b | BRF_GRA },           	//  4 Sprites
	{ "o-0.bin",		0x80000, 0x5e7b8555, 0x0b | BRF_GRA },           	//  5
	{ "o-5.bin",		0x80000, 0x40bee78b, 0x0b | BRF_GRA },           	//  6
	{ "o-4.bin",		0x80000, 0x7c634784, 0x0b | BRF_GRA },           	//  7
	{ "o-3.bin",		0x80000, 0x387fc7c4, 0x0b | BRF_GRA },           	//  8
	{ "o-2.bin",		0x80000, 0xc669bb49, 0x0b | BRF_GRA },           	//  9
	{ "o-7.bin",		0x80000, 0xfc77b04a, 0x0b | BRF_GRA },           	// 10
	{ "o-6.bin",		0x80000, 0x92882943, 0x0b | BRF_GRA },           	// 11

	{ "v1-2.bin",		0x20000, 0xd524735e, 0x04 | BRF_GRA },           	// 12 Layer 1 tiles
	{ "v1-5.bin",		0x20000, 0xeb440cdb, 0x04 | BRF_GRA },           	// 13
	{ "v1-1.bin",		0x20000, 0x09bdf75f, 0x04 | BRF_GRA },           	// 14
	{ "v1-4.bin",		0x20000, 0x803911e5, 0x04 | BRF_GRA },           	// 15
	{ "v1-0.bin",		0x20000, 0x73617548, 0x04 | BRF_GRA },           	// 16
	{ "v1-3.bin",		0x20000, 0x7f18e4fb, 0x04 | BRF_GRA },           	// 17

	{ "v2-2.bin",		0x20000, 0xc4f15638, 0x05 | BRF_GRA },           	// 18 Layer 2 tiles
	{ "v2-5.bin",		0x20000, 0xc2e57622, 0x05 | BRF_GRA },           	// 19
	{ "v2-1.bin",		0x20000, 0xc4f15638, 0x05 | BRF_GRA },           	// 20
	{ "v2-4.bin",		0x20000, 0x16ec2130, 0x05 | BRF_GRA },           	// 21
	{ "v2-0.bin",		0x20000, 0x5b05eba9, 0x05 | BRF_GRA },           	// 22
	{ "v2-3.bin",		0x20000, 0x80ad0c3b, 0x05 | BRF_GRA },           	// 23

	{ "s-0.bin",		0x40000, 0xa5fde408, 0x06 | BRF_SND },           	// 24 x1-010 Samples
	{ "s-1.bin",		0x40000, 0x3083f9c4, 0x06 | BRF_SND },           	// 25
	{ "s-2.bin",		0x40000, 0xa591c9ef, 0x06 | BRF_SND },           	// 26
	{ "s-3.bin",		0x40000, 0x68826c9d, 0x06 | BRF_SND },           	// 27
	{ "s-4.bin",		0x40000, 0x1c7dc8c2, 0x06 | BRF_SND },           	// 28
	{ "s-5.bin",		0x40000, 0x4bb0146a, 0x06 | BRF_SND },           	// 29
	{ "s-6.bin",		0x40000, 0x9f8f34ee, 0x06 | BRF_SND },           	// 30
	{ "s-7.bin",		0x40000, 0xe077dd39, 0x06 | BRF_SND },           	// 31
	
	{ "ux-015.u206",	0x00104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 32 plds
	{ "ux-016.u116",	0x00104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 33
};

STD_ROM_PICK(blandiap)
STD_ROM_FN(blandiap)

static INT32 blandiapInit()
{
	DrvSetVideoOffsets(0, 8, -2, 6);
	DrvSetColorOffsets(0, 0xa00, 0x200);

	INT32 nRet = DrvInit(blandiap68kInit, 16000000, SET_IRQLINES(2, 4), SPRITE_BUFFER, SET_GFX_DECODE(0, 4, 4));

	if (nRet == 0) {
		blandiaSetColorTable();
	}

	return nRet;
}

struct BurnDriver BurnDrvBlandiap = {
	"blandiap", "blandia", NULL, NULL, "1992",
	"Blandia (prototype)\0", NULL, "Allumer", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VSFIGHT, 0,
	NULL, blandiapRomInfo, blandiapRomName, NULL, NULL, NULL, NULL, BlandiaInputInfo, BlandiaDIPInfo,
	blandiapInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	384, 240, 4, 3
};


// Oishii Puzzle Ha Irimasenka

static struct BurnRomInfo oisipuzlRomDesc[] = {
	{ "ss1u200.v10",	0x80000, 0xf5e53baf, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ss1u201.v10",	0x80000, 0x7a7ff5ae, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "ss1u306.v10",	0x80000, 0xce43a754, 0x03 | BRF_GRA },           //  2 Sprites
	{ "ss1u307.v10",	0x80000, 0x2170b7ec, 0x03 | BRF_GRA },           //  3
	{ "ss1u304.v10",	0x80000, 0x546ab541, 0x03 | BRF_GRA },           //  4
	{ "ss1u305.v10",	0x80000, 0x2a33e08b, 0x03 | BRF_GRA },           //  5

	{ "ss1u23.v10",		0x80000, 0x9fa60901, 0x04 | BRF_GRA },           //  6 Layer 1 tiles
	{ "ss1u24.v10",		0x80000, 0xc10eb4b3, 0x04 | BRF_GRA },           //  7

	{ "ss1u25.v10",		0x80000, 0x56840728, 0x05 | BRF_GRA },           //  8 Layer 2 tiles

	{ "ss1u26.v10",		0x80000, 0xd452336b, 0x06 | BRF_SND },           //  9 x1-010 Samples
	{ "ss1u27.v10",		0x80000, 0x17fe921d, 0x06 | BRF_SND },           // 10
};

STD_ROM_PICK(oisipuzl)
STD_ROM_FN(oisipuzl)

static INT32 oisipuzlInit()
{
	oisipuzl_hack = 1; // 32 pixel offset for sprites???
	watchdog_enable = 1; // needs a reset before it will boot

	DrvSetVideoOffsets(1, 1, -1, -1);
	DrvSetColorOffsets(0, 0x400, 0x200);

	INT32 nRet = DrvInit(oisipuzl68kInit, 16000000, (2 << 8) | (1 << 0), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 2, 2));

	if (nRet == 0)
	{
		memset (Drv68KROM, 0, 0x200000);

		if (BurnLoadRom(Drv68KROM + 0x000000,  0, 1)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x100000,  1, 1)) return 1;

		for (INT32 i = 0; i < 0x400000; i++) DrvGfxROM0[i] ^= 0x0f; // invert
	}

	return nRet;
}

struct BurnDriver BurnDrvOisipuzl = {
	"oisipuzl", NULL, NULL, NULL, "1993",
	"Oishii Puzzle Ha Irimasenka\0", NULL, "Sunsoft / Atlus", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_MINIGAMES, 0,
	NULL, oisipuzlRomInfo, oisipuzlRomName, NULL, NULL, NULL, NULL, OisipuzlInputInfo, OisipuzlDIPInfo,
	oisipuzlInit, DrvExit, DrvFrame, seta2layerFlippedDraw, DrvScan, &DrvRecalc, 0x600,
	320, 224, 4, 3
};


// Triple Fun

static struct BurnRomInfo triplfunRomDesc[] = {
	{ "05.bin",		0x80000, 0x06eb3821, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "04.bin",		0x80000, 0x37a5c46e, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "09.bin",		0x80000, 0x98cc8ca5, 0x0b | BRF_GRA },           //  2 Sprites
	{ "08.bin",		0x80000, 0x63a8f10f, 0x0b | BRF_GRA },           //  3
	{ "11.bin",		0x80000, 0x276ef724, 0x0b | BRF_GRA },           //  4
	{ "10.bin",		0x80000, 0x20b0f282, 0x0b | BRF_GRA },           //  5

	{ "02.bin",		0x80000, 0x4c0d1068, 0x0c | BRF_GRA },           //  6 Layer 1 tiles
	{ "03.bin",		0x80000, 0xdba94e18, 0x0c | BRF_GRA },           //  7

	{ "06.bin",		0x40000, 0x8944bb72, 0x0d | BRF_GRA },           //  8 Layer 2 tiles
	{ "07.bin",		0x40000, 0x934a5d91, 0x0d | BRF_GRA },           //  9

	{ "01.bin",		0x40000, 0xc186a930, 0x06 | BRF_SND },           // 10 OKI M6295 Samples
};

STD_ROM_PICK(triplfun)
STD_ROM_FN(triplfun)

static INT32 triplfunInit()
{
	oisipuzl_hack = 1;
	DrvSetVideoOffsets(0, 0, -1, -1);
	DrvSetColorOffsets(0, 0x400, 0x200);

	return DrvInit(triplfun68kInit, 16000000, SET_IRQLINES(3, NOIRQ2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 2, 2));
}

struct BurnDriver BurnDrvTriplfun = {
	"triplfun", "oisipuzl", NULL, NULL, "1993",
	"Triple Fun\0", NULL, "bootleg", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_MINIGAMES, 0,
	NULL, triplfunRomInfo, triplfunRomName, NULL, NULL, NULL, NULL, OisipuzlInputInfo, OisipuzlDIPInfo,
	triplfunInit, DrvExit, DrvM6295Frame, seta2layerFlippedDraw, DrvScan, &DrvRecalc, 0x600,
	320, 224, 4, 3
};


// Sum-eoitneun Deongdalireul Chat-ara!

static struct BurnRomInfo triplfunkRomDesc[] = {
	{ "05.bin",		0x80000, 0x06eb3821, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "04.bin",		0x80000, 0x37a5c46e, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "09k.bin",		0x80000, 0x06730143, 0x0b | BRF_GRA },           //  2 Sprites
	{ "08k.bin",		0x80000, 0xe9a4b535, 0x0b | BRF_GRA },           //  3
	{ "11k.bin",		0x80000, 0x8166e961, 0x0b | BRF_GRA },           //  4
	{ "10k.bin",		0x80000, 0x2cea4898, 0x0b | BRF_GRA },           //  5

	{ "02k.bin",		0x80000, 0x3188b102, 0x0c | BRF_GRA },           //  6 Layer 1 tiles
	{ "03k.bin",		0x80000, 0x4a9520a4, 0x0c | BRF_GRA },           //  7

	{ "06k.bin",		0x40000, 0xf65f72d5, 0x0d | BRF_GRA },           //  8 Layer 2 tiles
	{ "07k.bin",		0x40000, 0x4522829e, 0x0d | BRF_GRA },           //  9

	{ "01.bin",		0x40000, 0xc186a930, 0x06 | BRF_SND },           // 10 OKI M6295 Samples
};

STD_ROM_PICK(triplfunk)
STD_ROM_FN(triplfunk)

struct BurnDriver BurnDrvTriplfunk = {
	"triplfunk", "oisipuzl", NULL, NULL, "1993",
	"Sum-eoitneun Deongdalireul Chat-ara!\0", NULL, "bootleg (Jin Young)", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_MINIGAMES, 0,
	NULL, triplfunkRomInfo, triplfunkRomName, NULL, NULL, NULL, NULL, OisipuzlInputInfo, OisipuzlDIPInfo,
	triplfunInit, DrvExit, DrvM6295Frame, seta2layerFlippedDraw, DrvScan, &DrvRecalc, 0x600,
	320, 224, 4, 3
};


// Pairs Love

static struct BurnRomInfo pairloveRomDesc[] = {
	{ "ut2-001-001.1a",		0x10000, 0x083338b7, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ut2-001-002.3a",		0x10000, 0x39d88aae, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "ut2-001-004.5j",		0x80000, 0xfdc47b26, 0x03 | BRF_GRA },           //  2 Sprites
	{ "ut2-001-005.5l",		0x80000, 0x076f94a2, 0x03 | BRF_GRA },           //  3

	{ "ut2-001-003.12a",	0x80000, 0x900219a9, 0x06 | BRF_SND },           //  4 x1-010 Samples
};

STD_ROM_PICK(pairlove)
STD_ROM_FN(pairlove)

static INT32 pairloveInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0, 0);

	INT32 nRet = DrvInit(pairlove68kInit, 8000000, SET_IRQLINES(2, 1), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, -1, -1));

	if (nRet == 0) {
		memcpy (DrvSndROM + 0x80000, DrvSndROM, 0x80000);
	}

	return nRet;
}

struct BurnDriver BurnDrvPairlove = {
	"pairlove", NULL, NULL, NULL, "1991",
	"Pairs Love\0", NULL, "Athena", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_PUZZLE, 0,
	NULL, pairloveRomInfo, pairloveRomName, NULL, NULL, NULL, NULL, PairloveInputInfo, PairloveDIPInfo,
	pairloveInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	240, 384, 3, 4
};


// Orbs (10/7/94 prototype?)

static struct BurnRomInfo orbsRomDesc[] = {
	{ "orbs.u10",		0x80000, 0x10f079c8, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "orbs.u9",		0x80000, 0xf269d16f, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "orbs.u14",		0x80000, 0x1cc76541, 0x03 | BRF_GRA },           //  2 Sprites
	{ "orbs.u13",		0x80000, 0x784bdc1a, 0x03 | BRF_GRA },           //  3
	{ "orbs.u12",		0x80000, 0xb8c352c2, 0x03 | BRF_GRA },           //  4
	{ "orbs.u11",		0x80000, 0x58cb38ba, 0x03 | BRF_GRA },           //  5

	{ "orbs.u15",		0x80000, 0xbc0e9fe3, 0x06 | BRF_SND },           //  6 x1-010 Samples
	{ "orbs.u16",		0x80000, 0xaecd8373, 0x06 | BRF_SND },           //  7
};

STD_ROM_PICK(orbs)
STD_ROM_FN(orbs)

static INT32 orbsInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0, 0);

	return DrvInit(pairlove68kInit, 7159090, SET_IRQLINES(2, 1), NO_SPRITE_BUFFER, SET_GFX_DECODE(5, -1, -1));
}

struct BurnDriver BurnDrvOrbs = {
	"orbs", NULL, NULL, NULL, "1994",
	"Orbs (10/7/94 prototype?)\0", NULL, "American Sammy", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_PUZZLE, 0,
	NULL, orbsRomInfo, orbsRomName, NULL, NULL, NULL, NULL, OrbsInputInfo, OrbsDIPInfo,
	orbsInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	320, 240, 4, 3
};


// Kero Kero Keroppi's Let's Play Together (USA, Version 2.0)

static struct BurnRomInfo keroppiRomDesc[] = {
	{ "keroppi jr. code =u10= v1.0.u10", 0x40000, 0x1fc2e895, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "keroppi jr. code =u9= v1.0.u9",   0x40000, 0xe0599e7b, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "keroppi jr. chr=u11= v1.0.u11", 	 0x80000, 0x74148c23, 0x03 | BRF_GRA },            //  2
	{ "keroppi jr. chr=u12= v1.0.u12", 	 0x80000, 0x6f4dae98, 0x03 | BRF_GRA },            //  3

	{ "keroppi jr. snd =u15= v1.0.u15",	 0x80000, 0xc98dacf0, 0x06 | BRF_SND },            //  4 x1-010 Samples
	{ "keroppi jr. snd =u16= v1.0.u16",	 0x80000, 0xd61e5a32, 0x06 | BRF_SND },            //  5
};

STD_ROM_PICK(keroppi)
STD_ROM_FN(keroppi)

static INT32 keroppiInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0, 0);

	return DrvInit(pairlove68kInit, 7159090, SET_IRQLINES(2, 1), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, -1, -1));
}

struct BurnDriver BurnDrvKeroppi = {
	"keroppi", NULL, NULL, NULL, "1995",
	"Kero Kero Keroppi's Let's Play Together (USA, Version 2.0)\0", NULL, "American Sammy", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SETA1, GBF_MINIGAMES, 0,
	NULL, keroppiRomInfo, keroppiRomName, NULL, NULL, NULL, NULL, KeroppiInputInfo, KeroppiDIPInfo,
	keroppiInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	320, 240, 4, 3
};

// Kero Kero Keroppi no Isshoni Asobou (Japan)

static struct BurnRomInfo keroppijRomDesc[] = {
	{ "ft-001-001.u10",	0x80000, 0x37861e7d, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ft-001-002.u9",	0x80000, 0xf531d4ef, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "ft-001-003.u14",	0x80000, 0x62fb22fb, 0x03 | BRF_GRA },           //  2 Sprites
	{ "ft-001-004.u13",	0x80000, 0x69908c98, 0x03 | BRF_GRA },           //  3
	{ "ft-001-005.u12",	0x80000, 0xde6432a8, 0x03 | BRF_GRA },           //  4
	{ "ft-001-006.u11",	0x80000, 0x9c500eae, 0x03 | BRF_GRA },           //  5

	{ "ft-001-007.u15",	0x80000, 0xc98dacf0, 0x06 | BRF_SND },           //  6 x1-010 Samples
	{ "ft-001-008.u16",	0x80000, 0xb9c4b637, 0x06 | BRF_SND },           //  7
};

STD_ROM_PICK(keroppij)
STD_ROM_FN(keroppij)

struct BurnDriver BurnDrvKeroppij = {
	"keroppij", "keroppi", NULL, NULL, "1993",
	"Kero Kero Keroppi no Isshoni Asobou (Japan)\0", NULL, "Sammy Industries", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SETA1, GBF_MINIGAMES, 0,
	NULL, keroppijRomInfo, keroppijRomName, NULL, NULL, NULL, NULL, KeroppiInputInfo, KeroppiDIPInfo,
	orbsInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	320, 240, 4, 3
};


// J. J. Squawkers

static struct BurnRomInfo jjsquawkRomDesc[] = {
	{ "fe2002001.u3",	0x80000, 0x7b9af960, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "fe2002002.u4",	0x80000, 0x47dd71a3, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "fe2001009",		0x80000, 0x27441cd3, 0x03 | BRF_GRA },           		//  2 Sprites
	{ "fe2001010",		0x80000, 0xca2b42c4, 0x03 | BRF_GRA },           		//  3
	{ "fe2001007",		0x80000, 0x62c45658, 0x03 | BRF_GRA },           		//  4
	{ "fe2001008",		0x80000, 0x2690c57b, 0x03 | BRF_GRA },           		//  5

	{ "fe2001011",		0x80000, 0x98b9f4b4, 0x04 | BRF_GRA },           		//  6 Layer 1 tiles
	{ "fe2001012",		0x80000, 0xd4aa916c, 0x04 | BRF_GRA },           		//  7
	{ "fe2001003",		0x80000, 0xa5a35caf, 0x1c | BRF_GRA },           		//  8

	{ "fe2001014",		0x80000, 0x274bbb48, 0x05 | BRF_GRA },           		//  9 Layer 2 tiles
	{ "fe2001013",		0x80000, 0x51e29871, 0x05 | BRF_GRA },           		// 10
	{ "fe2001004",		0x80000, 0xa235488e, 0x1d | BRF_GRA },		 	 		// 11

	{ "fe2001005.u69",	0x80000, 0xd99f2879, 0x06 | BRF_SND },           		// 12 x1-010 Samples
	{ "fe2001006.u70",	0x80000, 0x9df1e478, 0x06 | BRF_SND },           		// 13
	
	{ "m-009.u206",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 14 plds
	{ "m-010.u116",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 15
	{ "m2-011.u14",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 16
	{ "m-012.u35",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 17
	{ "m-013.u36",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 18
	{ "m-014.u76",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 19
};

STD_ROM_PICK(jjsquawk)
STD_ROM_FN(jjsquawk)

static INT32 jjsquawkInit()
{
	DrvSetVideoOffsets(1, 1, -1, -1);
	DrvSetColorOffsets(0, 0x200, 0xa00);

	INT32 nRet = DrvInit(wrofaero68kInit, 16000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 3, 3));

	if (nRet == 0) {
		jjsquawkSetColorTable();

		memcpy (Drv68KROM + 0x100000, Drv68KROM + 0x080000, 0x080000);
		memset (Drv68KROM + 0x080000, 0, 0x080000);
	}

	return nRet;
}

struct BurnDriver BurnDrvJjsquawk = {
	"jjsquawk", NULL, NULL, NULL, "1993",
	"J. J. Squawkers\0", NULL, "Athena / Able", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_PLATFORM, 0,
	NULL, jjsquawkRomInfo, jjsquawkRomName, NULL, NULL, NULL, NULL, JjsquawkInputInfo, JjsquawkDIPInfo,
	jjsquawkInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	384, 240, 4, 3
};


// J. J. Squawkers (older)
/* Official 93111A PCB missing version sticker */

static struct BurnRomInfo jjsquawkoRomDesc[] = {
	{ "fe2001001.u3",	0x80000, 0x921c9762, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "fe2001002.u4",	0x80000, 0x0227a2be, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "fe2001009",		0x80000, 0x27441cd3, 0x03 | BRF_GRA },           		//  2 Sprites
	{ "fe2001010",		0x80000, 0xca2b42c4, 0x03 | BRF_GRA },           		//  3
	{ "fe2001007",		0x80000, 0x62c45658, 0x03 | BRF_GRA },           		//  4
	{ "fe2001008",		0x80000, 0x2690c57b, 0x03 | BRF_GRA },           		//  5

	{ "fe2001011",		0x80000, 0x98b9f4b4, 0x04 | BRF_GRA },           		//  6 Layer 1 tiles
	{ "fe2001012",		0x80000, 0xd4aa916c, 0x04 | BRF_GRA },           		//  7
	{ "fe2001003",		0x80000, 0xa5a35caf, 0x1c | BRF_GRA },           		//  8

	{ "fe2001014",		0x80000, 0x274bbb48, 0x05 | BRF_GRA },           		//  9 Layer 2 tiles
	{ "fe2001013",		0x80000, 0x51e29871, 0x05 | BRF_GRA },           		// 10
	{ "fe2001004",		0x80000, 0xa235488e, 0x1d | BRF_GRA },		 	 		// 11

	{ "fe2001005.u69",	0x80000, 0xd99f2879, 0x06 | BRF_SND },           		// 12 x1-010 Samples
	{ "fe2001006.u70",	0x80000, 0x9df1e478, 0x06 | BRF_SND },           		// 13
	
	{ "m-009.u206",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 14 plds
	{ "m-010.u116",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 15
	{ "m2-011.u14",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 16
	{ "m-012.u35",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 17
	{ "m-013.u36",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 18
	{ "m-014.u76",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 19
};

STD_ROM_PICK(jjsquawko)
STD_ROM_FN(jjsquawko)

struct BurnDriver BurnDrvJjsquawko = {
	"jjsquawko", "jjsquawk", NULL, NULL, "1993",
	"J. J. Squawkers (older)\0", NULL, "Athena / Able", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_PLATFORM, 0,
	NULL, jjsquawkoRomInfo, jjsquawkoRomName, NULL, NULL, NULL, NULL, JjsquawkInputInfo, JjsquawkDIPInfo,
	jjsquawkInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	384, 240, 4, 3
};


// J. J. Squawkers (bootleg)

static struct BurnRomInfo jjsquawkbRomDesc[] = {
	{ "3",			0x080000, 0xafd5bd07, 0x01 | BRF_PRG | BRF_ESS }, 			//  0 68k Code
	{ "2",			0x080000, 0x740a7366, 0x01 | BRF_PRG | BRF_ESS }, 			//  1

	{ "4.bin",		0x200000, 0x969502f7, 0x03 | BRF_GRA },           			//  2 Sprites
	{ "2.bin",		0x200000, 0x765253d1, 0x03 | BRF_GRA },           			//  3

	{ "3.bin",		0x200000, 0xb1e3a4bb, 0x04 | BRF_GRA },           			//  4 Layer 1 tiles
	{ "1.bin",		0x200000, 0xa5d37cf7, 0x04 | BRF_GRA },           			//  5

	{ "1",			0x100000, 0x181a55b8, 0x06 | BRF_SND },           			//  6 x1-010 Samples
	
	{ "fj-111.u50",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  7 plds
	{ "fj-012.u51",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  8
	{ "fj-013.u52",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  9
	{ "fj-014.u53",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 10
	{ "fj-015.u54",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11
};

STD_ROM_PICK(jjsquawkb)
STD_ROM_FN(jjsquawkb)

static INT32 jjsquawkbInit()
{
	DrvSetVideoOffsets(1, 1, -1, -1);
	DrvSetColorOffsets(0, 0x200, 0xa00);

	INT32 nRet = DrvInit(jjsquawkb68kInit, 16000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 3, 3));

	if (nRet == 0) {
		jjsquawkSetColorTable();
	}

	return nRet;
}

struct BurnDriver BurnDrvJjsquawkb = {
	"jjsquawkb", "jjsquawk", NULL, NULL, "1999",
	"J. J. Squawkers (bootleg)\0", NULL, "bootleg", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_PLATFORM, 0,
	NULL, jjsquawkbRomInfo, jjsquawkbRomName, NULL, NULL, NULL, NULL, JjsquawkInputInfo, JjsquawkDIPInfo,
	jjsquawkbInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	384, 240, 4, 3
};


// J. J. Squawkers (bootleg, Blandia Conversion)
/* PCB was P0-078A, which was a Blandia board converted to JJ Squawkers. 
No labels on any of the ROMs. Apparently based on jjsquawko set. */

static struct BurnRomInfo jjsquawkb2RomDesc[] = {
	{ "u3.3a",		   0x080000, 0xf94c913b, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u4.4a",		   0x080000, 0x0227a2be, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "u64.3l",		   0x100000, 0x11d8713a, 0x03 | BRF_GRA },           //  2 Sprites 	      // jj-rom9 + jj-rom10 
	{ "u63.2l",		   0x100000, 0x7a385ef0, 0x03 | BRF_GRA },           //  3         		  // jj-rom7 + jj-rom8  
	
	{ "u66.5l",		   0x100000, 0xbbaf40c5, 0x04 | BRF_GRA },           //  4 Layer 1 tiles  // jj-rom11 + jj-rom12 
	{ "u65.4l",		   0x080000, 0xa5a35caf, 0x1c | BRF_GRA },           //  5                // jj-rom3.040         

	{ "u68.7l",		   0x100000, 0xae9ae01f, 0x05 | BRF_GRA },           //  9 Layer 2 tiles  // jj-rom14 + jj-rom13 
	{ "u67.6l",	       0x080000, 0xa235488e, 0x1d | BRF_GRA },	         // 10                // jj-rom4.040 	     

	{ "u70.10l",	   0x100000, 0x181a55b8, 0x06 | BRF_SND },           // 11 x1-010 Samples // jj-rom5.040 + jj-rom6.040 
};

STD_ROM_PICK(jjsquawkb2)
STD_ROM_FN(jjsquawkb2)

struct BurnDriver BurnDrvJjsquawkb2 = {
	"jjsquawkb2", "jjsquawk", NULL, NULL, "1999",
	"J. J. Squawkers (bootleg, Blandia Conversion)\0", NULL, "bootleg", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_PLATFORM, 0,
	NULL, jjsquawkb2RomInfo, jjsquawkb2RomName, NULL, NULL, NULL, NULL, JjsquawkInputInfo, JjsquawkDIPInfo,
	jjsquawkInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	384, 240, 4, 3
};


// Simpson Junior (bootleg of J. J. Squawkers)

static struct BurnRomInfo simpsonjrRomDesc[] = {
	{ "4.bin",			0x080000, 0x469cc203, 0x01 | BRF_PRG | BRF_ESS }, 			//  0 68k Code
	{ "3.bin",			0x080000, 0x740a7366, 0x01 | BRF_PRG | BRF_ESS }, 			//  1

	{ "5.bin",			0x400000, 0x82952780, 0x03 | BRF_GRA },           			//  2 Sprites
	{ "6.bin",			0x400000, 0x5a22bb87, 0x03 | BRF_GRA },           			//  3

	{ "1.bin",			0x080000, 0xd99f2879, 0x06 | BRF_SND },           			//  6 x1-010 Samples
	{ "2.bin",			0x080000, 0x9df1e478, 0x06 | BRF_SND },           			//  6 x1-010 Samples
};

STD_ROM_PICK(simpsonjr)
STD_ROM_FN(simpsonjr)

static INT32 simpsonjrRomCallback(INT32 bLoad)
{
	if (!bLoad)
	{
		DrvROMLen[0] = 0x400000; // gfx0
		DrvROMLen[1] = 0x200000; // gfx1
		DrvROMLen[2] = 0x200000; // gfx2
		DrvROMLen[3] = 0x100000; // sound rom
	}
	else
	{
		if (BurnLoadRom(Drv68KROM + 0x0000000, 0, 1)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0100000, 1, 1)) return 1;

		UINT8* DrvGfxtemp = (UINT8*)BurnMalloc(0x800000);
		if (BurnLoadRom(DrvGfxtemp  + 0x000000, 2, 1)) return 1;
		if (BurnLoadRom(DrvGfxtemp  + 0x400000, 3, 1)) return 1;
		memcpy (DrvGfxROM0 + 0x000000, DrvGfxtemp + 0x600000, 0x200000);
		memcpy (DrvGfxROM0 + 0x200000, DrvGfxtemp + 0x200000, 0x200000);
		memcpy (DrvGfxROM1 + 0x000000, DrvGfxtemp + 0x400000, 0x100000);
		memcpy (DrvGfxROM1 + 0x100000, DrvGfxtemp + 0x000000, 0x100000);
		memcpy (DrvGfxROM2 + 0x000000, DrvGfxtemp + 0x500000, 0x100000);
		memcpy (DrvGfxROM2 + 0x100000, DrvGfxtemp + 0x100000, 0x100000);
		BurnFree(DrvGfxtemp);

		if (BurnLoadRom(DrvSndROM  + 0x000000, 4, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x080000, 5, 1)) return 1;
	}

	return 0;
}

static INT32 simpsonjrInit()
{
	DrvSetVideoOffsets(1, 1, -1, -1);
	DrvSetColorOffsets(0, 0x200, 0xa00);

	pRomLoadCallback = simpsonjrRomCallback;

	INT32 nRet = DrvInit(simpsonjr68kInit, 16000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 3, 3));

	if (nRet == 0) {
		jjsquawkSetColorTable();
	}

	return nRet;
}

struct BurnDriver BurnDrvSimpsonjr = {
	"simpsonjr", "jjsquawk", NULL, NULL, "2003",
	"Simpson Junior (bootleg of J. J. Squawkers)\0", NULL, "bootleg (Daigom Games)", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_PLATFORM, 0,
	NULL, simpsonjrRomInfo, simpsonjrRomName, NULL, NULL, NULL, NULL, JjsquawkInputInfo, JjsquawkDIPInfo,
	simpsonjrInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	384, 240, 4, 3
};


// Extreme Downhill (v1.5)

static struct BurnRomInfo extdwnhlRomDesc[] = {
	{ "fw001002.201",	0x080000, 0x24d21924, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "fw001001.200",	0x080000, 0xfb12a28b, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "fw001003.202",	0x200000, 0xac9b31d5, 0x03 | BRF_GRA },           		//  2 Sprites

	{ "fw001004.206",	0x200000, 0x0dcb1d72, 0x04 | BRF_GRA },           		//  3 Layer 1 tiles
	{ "fw001005.205",	0x100000, 0x5c33b2f1, 0x1c | BRF_GRA },           		//  4

	{ "fw001006.152",	0x200000, 0xd00e8ddd, 0x05 | BRF_GRA },           		//  5 Layer 2 tiles

	{ "fw001007.026",	0x100000, 0x16d84d7a, 0x06 | BRF_SND },           		//  6 x1-010 Samples
	
	{ "fw-001.u50",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  7 plds
	{ "fw-002.u51",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  8
	{ "fw-003.u52",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  9
	{ "fw-004.u53",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 10
	{ "fw-005.u54",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11
};

STD_ROM_PICK(extdwnhl)
STD_ROM_FN(extdwnhl)

static INT32 extdwnhlInit()
{
	watchdog_enable = 1;

	DrvSetVideoOffsets(0, 0, -2, -2);
	DrvSetColorOffsets(0, 0x400, 0x200);

	INT32 nRet = DrvInit(extdwnhl68kInit, 16000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 3, 2));

	if (nRet == 0) {
		zingzapSetColorTable();
		if (DrvGfxTransMask[2] == NULL) {
			DrvGfxTransMask[2] = DrvGfxTransMask[1]; // sokonuke fix
		}
	}

	return nRet;
}

struct BurnDriver BurnDrvExtdwnhl = {
	"extdwnhl", NULL, NULL, NULL, "1995",
	"Extreme Downhill (v1.5)\0", NULL, "Sammy Industries Japan", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_RACING, 0,
	NULL, extdwnhlRomInfo, extdwnhlRomName, NULL, NULL, NULL, NULL, ExtdwnhlInputInfo, ExtdwnhlDIPInfo,
	extdwnhlInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	320, 240, 4, 3
};


// Sokonuke Taisen Game (Japan)

static struct BurnRomInfo sokonukeRomDesc[] = {
	{ "001-001.bin",	0x080000, 0x9d0aa3ca, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "001-002.bin",	0x080000, 0x96f2ef5f, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "001-003.bin",	0x200000, 0xab9ba897, 0x03 | BRF_GRA },           		//  2 Sprites

	{ "001-004.bin",	0x100000, 0x34ca3540, 0x04 | BRF_GRA },           		//  3 Layer 1 tiles
	{ "001-005.bin",	0x080000, 0x2b95d68d, 0x1c | BRF_GRA },           		//  4

	{ "001-006.bin",	0x100000, 0xecfac767, 0x06 | BRF_SND },           		//  5 x1-010 Samples
	
	{ "fw-001.u50",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  6 plds
	{ "fw-002.u51",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	//  7
	{ "fw-003.u52",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 	8
	{ "fw-004.u53",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 	9
	{ "fw-005.u54",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 10
};

STD_ROM_PICK(sokonuke)
STD_ROM_FN(sokonuke)

struct BurnDriver BurnDrvSokonuke = {
	"sokonuke", NULL, NULL, NULL, "1995",
	"Sokonuke Taisen Game (Japan)\0", NULL, "Sammy Industries", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_PLATFORM, 0,
	NULL, sokonukeRomInfo, sokonukeRomName, NULL, NULL, NULL, NULL, SokonukeInputInfo, SokonukeDIPInfo,
	extdwnhlInit, DrvExit, DrvFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	320, 240, 4, 3
};


// Krazy Bowl

static struct BurnRomInfo krzybowlRomDesc[] = {
	{ "fv001.002",		0x40000, 0x8c03c75f, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "fv001.001",		0x40000, 0xf0630beb, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "fv001.003",		0x80000, 0x7de22749, 0x03 | BRF_GRA },           		//  2 Sprites
	{ "fv001.004",		0x80000, 0xc7d2fe32, 0x03 | BRF_GRA },           		//  3

	{ "fv001.005",		0x80000, 0x5e206062, 0x06 | BRF_SND },           		//  4 x1-010 Samples
	{ "fv001.006",		0x80000, 0x572a15e7, 0x06 | BRF_SND },           		//  5
	
	{ "fv-007.u22",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 6 plds
	{ "fv-008.u23",		0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 7
};

STD_ROM_PICK(krzybowl)
STD_ROM_FN(krzybowl)

static INT32 krzybowlInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0, 0);
	trackball_mode = 1;

	INT32 rc = DrvInit(krzybowl68kInit, 16000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, -1, -1));

	VideoOffsets[2][0] = 8;

	return rc;
}

struct BurnDriver BurnDrvKrzybowl = {
	"krzybowl", NULL, NULL, NULL, "1994",
	"Krazy Bowl\0", NULL, "American Sammy", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_SPORTSMISC, 0,
	NULL, krzybowlRomInfo, krzybowlRomName, NULL, NULL, NULL, NULL, KrzybowlInputInfo, KrzybowlDIPInfo,
	krzybowlInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	232, 320, 3, 4
};


// Wiggie Waggie
// hack of Thunder & Lightning

static struct BurnRomInfo wiggieRomDesc[] = {
	{ "wiggie.e19",		0x10000, 0x24b58f16, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "wiggie.e21",		0x10000, 0x83ba6edb, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "wiggie.a5",		0x10000, 0x8078d77b, 0x02 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "wiggie.j16",		0x20000, 0x4fb40b8a, 0x03 | BRF_GRA },           //  3 Sprites
	{ "wiggie.j18",		0x20000, 0xebc418e9, 0x03 | BRF_GRA },           //  4
	{ "wiggie.j20",		0x20000, 0xc073501b, 0x03 | BRF_GRA },           //  5
	{ "wiggie.j21",		0x20000, 0x22f6fa39, 0x03 | BRF_GRA },           //  6

	{ "wiggie.d1",		0x40000, 0x27fbe12a, 0x06 | BRF_SND },           //  7 OKI M6295 Samples
};

STD_ROM_PICK(wiggie)
STD_ROM_FN(wiggie)

static INT32 wiggieInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0, 0);

	return DrvInit(wiggie68kInit, 16000000 / 2, SET_IRQLINES(2, NOIRQ2), NO_SPRITE_BUFFER, SET_GFX_DECODE(6, -1, -1));
}

struct BurnDriver BurnDrvWiggie = {
	"wiggie", NULL, NULL, NULL, "1994",
	"Wiggie Waggie\0", NULL, "Promat", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_BREAKOUT, 0,
	NULL, wiggieRomInfo, wiggieRomName, NULL, NULL, NULL, NULL, ThunderlInputInfo, ThunderlDIPInfo,
	wiggieInit, DrvExit, DrvZ80M6295Frame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	240, 384, 3, 4
};


// Super Bar
// hack of Thunder & Lightning

static struct BurnRomInfo superbarRomDesc[] = {
	{ "promat_512-1.e19",	0x10000, 0xcc7f9e87, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "promat_512-2.e21",	0x10000, 0x5e8c7231, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "promat.a5",			0x10000, 0x8078d77b, 0x02 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "promat_1m-4.j16",	0x20000, 0x43dbc99f, 0x03 | BRF_GRA },           //  3 Sprites
	{ "promat_1m-5.j18",	0x20000, 0xc09344b0, 0x03 | BRF_GRA },           //  4
	{ "promat_1m-6.j20",	0x20000, 0x7d83f8ba, 0x03 | BRF_GRA },           //  5
	{ "promat_1m-7.j21",	0x20000, 0x734df92a, 0x03 | BRF_GRA },           //  6

	{ "promat_2m-1.d1",		0x40000, 0x27fbe12a, 0x06 | BRF_SND },           //  7 OKI M6295 Samples
};

STD_ROM_PICK(superbar)
STD_ROM_FN(superbar)

static INT32 superbarInit()
{
	DrvSetVideoOffsets(0, 0, 0, 0);
	DrvSetColorOffsets(0, 0, 0);

	return DrvInit(wiggie68kInit, 8000000, SET_IRQLINES(2, NOIRQ2), NO_SPRITE_BUFFER, SET_GFX_DECODE(7, -1, -1));
}

struct BurnDriver BurnDrvSuperbar = {
	"superbar", "wiggie", NULL, NULL, "1994",
	"Super Bar\0", NULL, "Promat", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_BREAKOUT, 0,
	NULL, superbarRomInfo, superbarRomName, NULL, NULL, NULL, NULL, ThunderlInputInfo, ThunderlDIPInfo,
	superbarInit, DrvExit, DrvZ80M6295Frame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	240, 384, 3, 4
};


// Ultra Toukon Densetsu (Japan)

static struct BurnRomInfo utoukondRomDesc[] = {
	{ "93uta010.3",		0x080000, 0xc486ef5e, 0x01 | BRF_PRG | BRF_ESS }, 		//  0 68k Code
	{ "93uta011.4",		0x080000, 0x978978f7, 0x01 | BRF_PRG | BRF_ESS }, 		//  1

	{ "93uta009.112",	0x010000, 0x67f18483, 0x02 | BRF_PRG | BRF_ESS }, 		//  2 Z80 Code

	{ "93uta04.64",		0x100000, 0x9cba0538, 0x03 | BRF_GRA },           		//  3 Sprites
	{ "93uta02.201",	0x100000, 0x884fedfa, 0x03 | BRF_GRA },           		//  4
	{ "93uta03.63",		0x100000, 0x818484a5, 0x03 | BRF_GRA },           		//  5
	{ "93uta01.200",	0x100000, 0x364de841, 0x03 | BRF_GRA },           		//  6

	{ "93uta05.66",		0x100000, 0x5e640bfb, 0x04 | BRF_GRA },           		//  7 Layer 1 tiles

	{ "93uta07.68",		0x100000, 0x67bdd036, 0x05 | BRF_GRA },           		//  8 Layer 2 tiles
	{ "93uta06.67",		0x100000, 0x294c26e4, 0x05 | BRF_GRA },           		//  9

	{ "93uta08.69",		0x100000, 0x3d50bbcd, 0x06 | BRF_SND },           		// 10 x1-010 Samples
	
	{ "93ut-a12.u206",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 11 plds
	{ "93ut-a13.u14",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 12
	{ "93ut-a14.u35",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 13
	{ "93ut-a15.u36",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 14
	{ "93ut-a16.u110",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 15
	{ "93ut-a17.u76",	0x000104, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP },	// 16
};

STD_ROM_PICK(utoukond)
STD_ROM_FN(utoukond)

static INT32 utoukondInit()
{
	DrvSetVideoOffsets(0, 0, -2, 0);
	DrvSetColorOffsets(0, 0x400, 0x200);

	return DrvInit(utoukond68kInit, 16000000, SET_IRQLINES(2, 1), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 2, 2));
}

struct BurnDriver BurnDrvUtoukond = {
	"utoukond", NULL, NULL, NULL, "1993",
	"Ultra Toukon Densetsu (Japan)\0", NULL, "Banpresto / Tsuburaya Productions", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA1, GBF_VSFIGHT, 0,
	NULL, utoukondRomInfo, utoukondRomName, NULL, NULL, NULL, NULL, UtoukondInputInfo, UtoukondDIPInfo,
	utoukondInit, DrvExit, Drv68kZ80YM3438Frame, seta2layerDraw, DrvScan, &DrvRecalc, 0x600,
	384, 224, 4, 3
};


// DownTown / Mokugeki (set 1)

static struct BurnRomInfo downtownRomDesc[] = {
	{ "ud2-001-000.3c",			0x40000, 0xf1965260, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ud2-001-003.11c",		0x40000, 0xe7d5fa5f, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ud2001002.9b",			0x10000, 0xa300e3ac, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "ud2001001.8b",			0x10000, 0xd2918094, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "ud2-002-004.17c",		0x40000, 0xbbd538b1, 0x02 | BRF_PRG | BRF_ESS }, //  4 M65c02 Code

	{ "ud2-001-005-t01.2n",		0x80000, 0x77e6d249, 0x0b | BRF_GRA },           //  5 Sprites
	{ "ud2-001-006-t02.3n",		0x80000, 0x6e381bf2, 0x0b | BRF_GRA },           //  6
	{ "ud2-001-007-t03.5n",		0x80000, 0x737b4971, 0x0b | BRF_GRA },           //  7
	{ "ud2-001-008-t04.6n",		0x80000, 0x99b9d757, 0x0b | BRF_GRA },           //  8

	{ "ud2-001-009-t05.8n",		0x80000, 0xaee6c581, 0x04 | BRF_GRA },           //  9 Layer 1 tiles
	{ "ud2-001-010-t06.9n",		0x80000, 0x3d399d54, 0x04 | BRF_GRA },           // 10

	{ "ud2-001-011-t07.14n",	0x80000, 0x9c9ff69f, 0x06 | BRF_SND },           // 11 x1-010 Samples
};

STD_ROM_PICK(downtown)
STD_ROM_FN(downtown)

static INT32 downtownInit()
{
	refresh_rate = 5742; // 57.42 hz
	DrvSetVideoOffsets(0, 0, -1, 0);
	DrvSetColorOffsets(0, 0, 0);

	INT32 nRet = DrvInit(downtown68kInit, 8000000, SET_IRQLINES(1, 2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 1, -1));

	return nRet;
}

struct BurnDriver BurnDrvDowntown = {
	"downtown", NULL, NULL, NULL, "1989",
	"DownTown / Mokugeki (set 1)\0", NULL, "Seta", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA1, GBF_SCRFIGHT, 0,
	NULL, downtownRomInfo, downtownRomName, NULL, NULL, NULL, NULL, DowntownInputInfo, DowntownDIPInfo,
	downtownInit, DrvExit, DrvDowntownFrame, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	224, 384, 3, 4
};


// DownTown / Mokugeki (set 2)

static struct BurnRomInfo downtown2RomDesc[] = {
	{ "ud2-001-000.3c",			0x40000, 0xf1965260, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ud2-001-003.11c",		0x40000, 0xe7d5fa5f, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ud2000002.9b",			0x10000, 0xca976b24, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "ud2000001.8b",			0x10000, 0x1708aebd, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "ud2-002-004.17c",		0x40000, 0xbbd538b1, 0x02 | BRF_PRG | BRF_ESS }, //  4 M65c02 Code

	{ "ud2-001-005-t01.2n",		0x80000, 0x77e6d249, 0x0b | BRF_GRA },           //  5 Sprites
	{ "ud2-001-006-t02.3n",		0x80000, 0x6e381bf2, 0x0b | BRF_GRA },           //  6
	{ "ud2-001-007-t03.5n",		0x80000, 0x737b4971, 0x0b | BRF_GRA },           //  7
	{ "ud2-001-008-t04.6n",		0x80000, 0x99b9d757, 0x0b | BRF_GRA },           //  8

	{ "ud2-001-009-t05.8n",		0x80000, 0xaee6c581, 0x04 | BRF_GRA },           //  9 Layer 1 tiles
	{ "ud2-001-010-t06.9n",		0x80000, 0x3d399d54, 0x04 | BRF_GRA },           // 10

	{ "ud2-001-011-t07.14n",	0x80000, 0x9c9ff69f, 0x06 | BRF_SND },           // 11 x1-010 Samples
};

STD_ROM_PICK(downtown2)
STD_ROM_FN(downtown2)

struct BurnDriver BurnDrvDowntown2 = {
	"downtown2", "downtown", NULL, NULL, "1989",
	"DownTown / Mokugeki (set 2)\0", NULL, "Seta", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA1, GBF_SCRFIGHT, 0,
	NULL, downtown2RomInfo, downtown2RomName, NULL, NULL, NULL, NULL, DowntownInputInfo, DowntownDIPInfo,
	downtownInit, DrvExit, DrvDowntownFrame, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	224, 384, 3, 4
};


// DownTown / Mokugeki (joystick hack)

static struct BurnRomInfo downtownjRomDesc[] = {
	{ "ud2-001-000.3c",			0x40000, 0xf1965260, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ud2-001-003.11c",		0x40000, 0xe7d5fa5f, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "u37.9b",					0x10000, 0x73047657, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "u31.8b",					0x10000, 0x6a050240, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "ud2-002-004.17c",		0x40000, 0xbbd538b1, 0x02 | BRF_PRG | BRF_ESS }, //  4 M65c02 Code

	{ "ud2-001-005-t01.2n",		0x80000, 0x77e6d249, 0x0b | BRF_GRA },           //  5 Sprites
	{ "ud2-001-006-t02.3n",		0x80000, 0x6e381bf2, 0x0b | BRF_GRA },           //  6
	{ "ud2-001-007-t03.5n",		0x80000, 0x737b4971, 0x0b | BRF_GRA },           //  7
	{ "ud2-001-008-t04.6n",		0x80000, 0x99b9d757, 0x0b | BRF_GRA },           //  8

	{ "ud2-001-009-t05.8n",		0x80000, 0xaee6c581, 0x04 | BRF_GRA },           //  9 Layer 1 tiles
	{ "ud2-001-010-t06.9n",		0x80000, 0x3d399d54, 0x04 | BRF_GRA },           // 10

	{ "ud2-001-011-t07.14n",	0x80000, 0x9c9ff69f, 0x06 | BRF_SND },           // 11 x1-010 Samples
};

STD_ROM_PICK(downtownj)
STD_ROM_FN(downtownj)

struct BurnDriver BurnDrvDowntownj = {
	"downtownj", "downtown", NULL, NULL, "1989",
	"DownTown / Mokugeki (joystick hack)\0", NULL, "Seta", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA1, GBF_SCRFIGHT, 0,
	NULL, downtownjRomInfo, downtownjRomName, NULL, NULL, NULL, NULL, DowntownInputInfo, DowntownDIPInfo,
	downtownInit, DrvExit, DrvDowntownFrame, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	224, 384, 3, 4
};


// DownTown / Mokugeki (prototype)

static struct BurnRomInfo downtownpRomDesc[] = {
	{ "ud2-001-000.3c",			0x40000, 0xf1965260, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ud2-001-003.11c",		0x40000, 0xe7d5fa5f, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ud2_even_v061.9b",		0x10000, 0x251d6552, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "ud2_odd_v061.8b",		0x10000, 0x6394a7c0, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "ud2-002-004.17c",		0x40000, 0xbbd538b1, 0x02 | BRF_PRG | BRF_ESS }, //  4 M65c02 Code

	{ "ud2-001-005-t01.2n",		0x80000, 0x77e6d249, 0x0b | BRF_GRA },           //  5 Sprites
	{ "ud2-001-006-t02.3n",		0x80000, 0x6e381bf2, 0x0b | BRF_GRA },           //  6
	{ "ud2-001-007-t03.5n",		0x80000, 0x737b4971, 0x0b | BRF_GRA },           //  7
	{ "ud2-001-008-t04.6n",		0x80000, 0x99b9d757, 0x0b | BRF_GRA },           //  8

	{ "ud2-001-009-t05.8n",		0x80000, 0xaee6c581, 0x04 | BRF_GRA },           //  9 Layer 1 tiles
	{ "ud2-001-010-t06.9n",		0x80000, 0x3d399d54, 0x04 | BRF_GRA },           // 10

	{ "ud2-001-011-t07.14n",	0x80000, 0x9c9ff69f, 0x06 | BRF_SND },           // 11 x1-010 Samples
};

STD_ROM_PICK(downtownp)
STD_ROM_FN(downtownp)

struct BurnDriver BurnDrvDowntownp = {
	"downtownp", "downtown", NULL, NULL, "1989",
	"DownTown / Mokugeki (prototype)\0", NULL, "Seta", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA1, GBF_SCRFIGHT, 0,
	NULL, downtownpRomInfo, downtownpRomName, NULL, NULL, NULL, NULL, DowntownInputInfo, DowntownDIPInfo,
	downtownInit, DrvExit, DrvDowntownFrame, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	224, 384, 3, 4
};


// Thundercade / Twin Formation

static struct BurnRomInfo tndrcadeRomDesc[] = {
	{ "ua0-4.u19",		0x20000, 0x73bd63eb, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ua0-2.u17",		0x20000, 0xe96194b1, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ua0-3.u18",		0x20000, 0x0a7b1c41, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "ua0-1.u16",		0x20000, 0xfa906626, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "ua10-5.u24",		0x20000, 0x8eff6122, 0x02 | BRF_PRG | BRF_ESS }, //  4 M65c02 Code

	{ "ua0-10.u12",		0x40000, 0xaa7b6757, 0x03 | BRF_GRA },           //  5 Sprites
	{ "ua0-11.u13",		0x40000, 0x11eaf931, 0x03 | BRF_GRA },           //  6
	{ "ua0-12.u14",		0x40000, 0x00b5381c, 0x03 | BRF_GRA },           //  7
	{ "ua0-13.u15",		0x40000, 0x8f9a0ed3, 0x03 | BRF_GRA },           //  8
	{ "ua0-6.u8",		0x40000, 0x14ecc7bb, 0x03 | BRF_GRA },           //  9
	{ "ua0-7.u9",		0x40000, 0xff1a4e68, 0x03 | BRF_GRA },           // 10
	{ "ua0-8.u10",		0x40000, 0x936e1884, 0x03 | BRF_GRA },           // 11
	{ "ua0-9.u11",		0x40000, 0xe812371c, 0x03 | BRF_GRA },           // 12
};

STD_ROM_PICK(tndrcade)
STD_ROM_FN(tndrcade)

static INT32 tndrcadeInit()
{
	DrvSetVideoOffsets(-1, 0, 0, 0);
	DrvSetColorOffsets(0, 0, 0);

	oisipuzl_hack = 1;

	return DrvInit(tndrcade68kInit, 8000000, SET_IRQLINES(2, NOIRQ2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, -1, -1));
}

struct BurnDriver BurnDrvTndrcade = {
	"tndrcade", NULL, NULL, NULL, "1987",
	"Thundercade / Twin Formation\0", NULL, "Seta (Taito license)", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, tndrcadeRomInfo, tndrcadeRomName, NULL, NULL, NULL, NULL, TndrcadeInputInfo, TndrcadeDIPInfo,
	tndrcadeInit, DrvExit, DrvTndrcadeFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	224, 384, 3, 4
};


// Tokusyu Butai U.A.G. (Japan)

static struct BurnRomInfo tndrcadejRomDesc[] = {
	{ "ua0-4.u19",		0x20000, 0x73bd63eb, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ua0-2.u17",		0x20000, 0xe96194b1, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ua0-3.u18",		0x20000, 0x0a7b1c41, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "ua0-1.u16",		0x20000, 0xfa906626, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "thcade5.u24",	0x20000, 0x8cb9df7b, 0x02 | BRF_PRG | BRF_ESS }, //  4 M65c02 Code

	{ "ua0-10.u12",		0x40000, 0xaa7b6757, 0x03 | BRF_GRA },           //  5 Sprites
	{ "ua0-11.u13",		0x40000, 0x11eaf931, 0x03 | BRF_GRA },           //  6
	{ "ua0-12.u14",		0x40000, 0x00b5381c, 0x03 | BRF_GRA },           //  7
	{ "ua0-13.u15",		0x40000, 0x8f9a0ed3, 0x03 | BRF_GRA },           //  8
	{ "ua0-6.u8",		0x40000, 0x14ecc7bb, 0x03 | BRF_GRA },           //  9
	{ "ua0-7.u9",		0x40000, 0xff1a4e68, 0x03 | BRF_GRA },           // 10
	{ "ua0-8.u10",		0x40000, 0x936e1884, 0x03 | BRF_GRA },           // 11
	{ "ua0-9.u11",		0x40000, 0xe812371c, 0x03 | BRF_GRA },           // 12
};

STD_ROM_PICK(tndrcadej)
STD_ROM_FN(tndrcadej)

struct BurnDriver BurnDrvTndrcadej = {
	"tndrcadej", "tndrcade", NULL, NULL, "1987",
	"Tokusyu Butai U.A.G. (Japan)\0", NULL, "Seta (Taito license)", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, tndrcadejRomInfo, tndrcadejRomName, NULL, NULL, NULL, NULL, TndrcadeInputInfo, TndrcadjDIPInfo,
	tndrcadeInit, DrvExit, DrvTndrcadeFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	224, 384, 3, 4
};


// Arbalester

static struct BurnRomInfo arbalestRomDesc[] = {
	{ "uk-001-003",			0x40000, 0xee878a2c, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "uk-001-004",			0x40000, 0x902bb4e3, 0x01 | BRF_PRG | BRF_ESS }, //  1

	/* Label is correct, 1st & 2nd halves identical is correct. Chip is a 27128 - Verified on 2 different PCBs */
	{ "uk6005",				0x04000, 0x48c73a4a, 0x02 | BRF_PRG | BRF_ESS }, //  2 M65c02 Code

	{ "uk001.06",			0x40000, 0x11c75746, 0x0b | BRF_GRA },           //  3 Sprites
	{ "uk001.07",			0x40000, 0x01b166c7, 0x0b | BRF_GRA },           //  4
	{ "uk001.08",			0x40000, 0x78d60ba3, 0x0b | BRF_GRA },           //  5
	{ "uk001.09",			0x40000, 0xb4748ae0, 0x0b | BRF_GRA },           //  6

	{ "uk-001-010-t26",		0x80000, 0xc1e2f823, 0x04 | BRF_GRA },           //  7 Layer 1 tiles
	{ "uk-001-011-t27",		0x80000, 0x09dfe56a, 0x04 | BRF_GRA },           //  8
	{ "uk-001-012-t28",		0x80000, 0x818a4085, 0x04 | BRF_GRA },           //  9
	{ "uk-001-013-t29",		0x80000, 0x771fa164, 0x04 | BRF_GRA },           // 10

	{ "uk-001-015-t31",		0x80000, 0xce9df5dd, 0x06 | BRF_SND },           // 11 x1-010 Samples
	{ "uk-001-014-t30",		0x80000, 0x016b844a, 0x06 | BRF_SND },           // 12
};

STD_ROM_PICK(arbalest)
STD_ROM_FN(arbalest)

static INT32 arbalestInit()
{
	INT32 rc = 0;
	DrvSetVideoOffsets(0, 1, -2, -1);
	DrvSetColorOffsets(0, 0, 0);
	X1010_Arbalester_Mode = 1;
	rc = DrvInit(metafox68kInit, 8000000, SET_IRQLINES(3, NOIRQ2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 1, -1));

	if (!rc) clear_opposites = 1;

	return rc;
}

struct BurnDriver BurnDrvArbalest = {
	"arbalest", NULL, NULL, NULL, "1989",
	"Arbalester\0", NULL, "Jordan I.S. / Seta", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, arbalestRomInfo, arbalestRomName, NULL, NULL, NULL, NULL, MetafoxInputInfo, ArbalestDIPInfo,
	arbalestInit, DrvExit, DrvTwineaglFrame, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	224, 384, 3, 4
};


// Meta Fox

static struct BurnRomInfo metafoxRomDesc[] = {
	{ "p1003161",		0x40000, 0x4fd6e6a1, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "p1004162",		0x40000, 0xb6356c9a, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "up001002",		0x10000, 0xce91c987, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "up001001",		0x10000, 0x0db7a505, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "up001005",		0x02000, 0x2ac5e3e3, 0x02 | BRF_PRG | BRF_ESS }, //  4 m65c02 Code

	{ "p1006163",		0x40000, 0x80f69c7c, 0x0b | BRF_GRA },           //  5 Sprites
	{ "p1007164",		0x40000, 0xd137e1a3, 0x0b | BRF_GRA },           //  6
	{ "p1008165",		0x40000, 0x57494f2b, 0x0b | BRF_GRA },           //  7
	{ "p1009166",		0x40000, 0x8344afd2, 0x0b | BRF_GRA },           //  8

	{ "up001010",		0x80000, 0xbfbab472, 0x04 | BRF_GRA },           //  9 Layer 1 tiles
	{ "up001011",		0x80000, 0x26cea381, 0x04 | BRF_GRA },           // 10
	{ "up001012",		0x80000, 0xfed2c5f9, 0x04 | BRF_GRA },           // 11
	{ "up001013",		0x80000, 0xadabf9ea, 0x04 | BRF_GRA },           // 12

	{ "up001015",		0x80000, 0x2e20e39f, 0x06 | BRF_SND },           // 13 x1-010 Samples
	{ "up001014",		0x80000, 0xfca6315e, 0x06 | BRF_SND },           // 14
};

STD_ROM_PICK(metafox)
STD_ROM_FN(metafox)

static void __fastcall metafox_protection_write_byte(UINT32 address, UINT8 data)
{
	Drv68KRAM2[(address / 2) & 0x1fff] = data;
}

static UINT8 __fastcall metafox_protection_read_byte(UINT32 address)
{
	switch (address & 0x3ffe)
	{
		case 0x0000: // 21c001
			return Drv68KRAM2[0x0101/2];

		case 0x1000: // 21d001
			return Drv68KRAM2[0x10a1/2];

		case 0x2000: // 21e001
			return Drv68KRAM2[0x2149/2];
	}

	return Drv68KRAM2[(address / 2) & 0x1fff];
}

static void metafox_protection_install()
{
	SekOpen(0);
	SekMapHandler(4,			0x21c000, 0x21ffff, MAP_READ | MAP_WRITE);
	SekSetReadByteHandler (4,		metafox_protection_read_byte);
	SekSetWriteByteHandler(4,		metafox_protection_write_byte);
	SekClose();
}

static INT32 metafoxInit()
{
	DrvSetVideoOffsets(0, 0, 16, -19);
	DrvSetColorOffsets(0, 0, 0);

	INT32 nRet = DrvInit(metafox68kInit, 8000000, SET_IRQLINES(3, NOIRQ2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 1, -1));

	if (nRet == 0) {
		clear_opposites = 1;
		metafox_protection_install();
	}

	return nRet;
}

struct BurnDriver BurnDrvMetafox = {
	"metafox", NULL, NULL, NULL, "1989",
	"Meta Fox\0", NULL, "Jordan I.S. / Seta", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, metafoxRomInfo, metafoxRomName, NULL, NULL, NULL, NULL, MetafoxInputInfo, MetafoxDIPInfo,
	metafoxInit, DrvExit, DrvTwineaglFrame, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	224, 384, 3, 4
};


// Pro Mahjong Kiwame

static struct BurnRomInfo kiwameRomDesc[] = {
	{ "fp001001.bin",	0x40000, 0x31b17e39, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "fp001002.bin",	0x40000, 0x5a6e2efb, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "fp001003.bin",	0x80000, 0x0f904421, 0x03 | BRF_GRA },           //  2 Sprites

	{ "fp001006.bin",	0x80000, 0x96cf395d, 0x06 | BRF_SND },           //  3 x1-010 Samples
	{ "fp001005.bin",	0x80000, 0x65b5fe9a, 0x06 | BRF_SND },           //  4
	
	{ "nvram.bin",		0x10000, 0x1f719400, 0x00 | BRF_OPT },			 //  5
};

STD_ROM_PICK(kiwame)
STD_ROM_FN(kiwame)

static INT32 kiwameInit()
{
	DrvSetVideoOffsets(0, -16, 0, 0);
	DrvSetColorOffsets(0, 0, 0);

	return DrvInit(kiwame68kInit, 16000000, SET_IRQLINES(1, NOIRQ2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 1, -1));
}

struct BurnDriverD BurnDrvKiwame = {
	"kiwame", NULL, NULL, NULL, "1994",
	"Pro Mahjong Kiwame\0", NULL, "Athena", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_SETA1, GBF_MAHJONG, 0,
	NULL, kiwameRomInfo, kiwameRomName, NULL, NULL, NULL, NULL, KiwameInputInfo, KiwameDIPInfo,
	kiwameInit, DrvExit, DrvFrame, setaNoLayersDraw, DrvScan, &DrvRecalc, 0x200,
	448, 240, 4, 3
};


// Twin Eagle - Revenge Joe's Brother

static struct BurnRomInfo twineaglRomDesc[] = {
	{ "ua2-1",		0x80000, 0x5c3fe531, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code

	{ "ua2-2",		0x02000, 0x783ca84e, 0x02 | BRF_PRG | BRF_ESS }, //  1 M65c02 Code

	{ "ua2-4",		0x40000, 0x8b7532d6, 0x0b | BRF_GRA },           //  2 Sprites
	{ "ua2-3",		0x40000, 0x1124417a, 0x0b | BRF_GRA },           //  3
	{ "ua2-6",		0x40000, 0x99d8dbba, 0x0b | BRF_GRA },           //  4
	{ "ua2-5",		0x40000, 0x6e450d28, 0x0b | BRF_GRA },           //  5

	{ "ua2-8",		0x80000, 0x7d3a8d73, 0x04 | BRF_GRA },           //  6 Layer 1 tiles
	{ "ua2-10",		0x80000, 0x5bbe1f56, 0x04 | BRF_GRA },           //  7
	{ "ua2-7",		0x80000, 0xfce56907, 0x04 | BRF_GRA },           //  8
	{ "ua2-9",		0x80000, 0xa451eae9, 0x04 | BRF_GRA },           //  9

	{ "ua2-11",		0x80000, 0x624e6057, 0x06 | BRF_SND },           // 10 x1-010 Samples
	{ "ua2-12",		0x80000, 0x3068ff64, 0x06 | BRF_SND },           // 11
};

STD_ROM_PICK(twineagl)
STD_ROM_FN(twineagl)

static INT32 twineaglInit()
{
	twineagle = 1;

	DrvSetVideoOffsets(0, 0, 0, -3);
	DrvSetColorOffsets(0, 0, 0);
	clear_opposites = 1;

	return DrvInit(twineagle68kInit, 8000000, SET_IRQLINES(3, NOIRQ2), NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 1, -1));
}

struct BurnDriver BurnDrvTwineagl = {
	"twineagl", NULL, NULL, NULL, "1988",
	"Twin Eagle - Revenge Joe's Brother\0", NULL, "Seta (Taito license)", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SETA1, GBF_VERSHOOT, 0,
	NULL, twineaglRomInfo, twineaglRomName, NULL, NULL, NULL, NULL, TwineaglInputInfo, TwineaglDIPInfo,
	twineaglInit, DrvExit, DrvTwineaglFrame /*DrvM65c02Frame*/, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	224, 384, 3, 4
};


// U.S. Classic

static struct BurnRomInfo usclssicRomDesc[] = {
	{ "ue2001.u20",		0x20000, 0x18b41421, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ue2000.u14",		0x20000, 0x69454bc2, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ue2002.u22",		0x20000, 0xa7bbe248, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "ue2003.u30",		0x20000, 0x29601906, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "ue002u61.004",	0x40000, 0x476e9f60, 0x02 | BRF_PRG | BRF_ESS }, //  4 M65c02 Code

	{ "ue001009.119",	0x80000, 0xdc065204, 0x0b | BRF_GRA },           //  5 Sprites
	{ "ue001008.118",	0x80000, 0x5947d9b5, 0x0b | BRF_GRA },           //  6
	{ "ue001007.117",	0x80000, 0xb48a885c, 0x0b | BRF_GRA },           //  7
	{ "ue001006.116",	0x80000, 0xa6ab6ef4, 0x0b | BRF_GRA },           //  8

	{ "ue001010.120",	0x80000, 0xdd683031, 0x04 | BRF_GRA },           //  9 Layer 1 tiles
	{ "ue001011.121",	0x80000, 0x0e27bc49, 0x04 | BRF_GRA },           // 10
	{ "ue001012.122",	0x80000, 0x961dfcdc, 0x04 | BRF_GRA },           // 11
	{ "ue001013.123",	0x80000, 0x03e9eb79, 0x04 | BRF_GRA },           // 12
	{ "ue001014.124",	0x80000, 0x9576ace7, 0x04 | BRF_GRA },           // 13
	{ "ue001015.125",	0x80000, 0x631d6eb1, 0x04 | BRF_GRA },           // 14
	{ "ue001016.126",	0x80000, 0xf44a8686, 0x04 | BRF_GRA },           // 15
	{ "ue001017.127",	0x80000, 0x7f568258, 0x04 | BRF_GRA },           // 16
	{ "ue001018.128",	0x80000, 0x4bd98f23, 0x04 | BRF_GRA },           // 17
	{ "ue001019.129",	0x80000, 0x6d9f5a33, 0x04 | BRF_GRA },           // 18
	{ "ue001020.130",	0x80000, 0xbc07403f, 0x04 | BRF_GRA },           // 19
	{ "ue001021.131",	0x80000, 0x98c03efd, 0x04 | BRF_GRA },           // 20

	{ "ue001005.132",	0x80000, 0xc5fea37c, 0x06 | BRF_SND },           // 21 x1-010 Samples

	{ "ue1-023.prm",	0x00200, 0xa13192a4, 0x0f | BRF_GRA },           // 22 Color PROMs
	{ "ue1-022.prm",	0x00200, 0x1a23129e, 0x0f | BRF_GRA },           // 23
};

STD_ROM_PICK(usclssic)
STD_ROM_FN(usclssic)

static INT32 usclssicInit()
{
	watchdog_enable = 1;
	DrvSetColorOffsets(0, 0x200, 0);
	DrvSetVideoOffsets(1, 2, 0, -1);
	trackball_mode = 1; // for trackball
	usclssic = 1;

	INT32 nRet = DrvInit(usclssic68kInit, 8000000, SET_IRQLINES(0x80, 0x80) /*custom*/, NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 4, -1));

	if (nRet == 0) {
		usclssicSetColorTable();
	}

	return nRet;
}

struct BurnDriver BurnDrvUsclssic = {
	"usclssic", NULL, NULL, NULL, "1989",
	"U.S. Classic\0", NULL, "Seta", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA1, GBF_SPORTSMISC, 0,
	NULL, usclssicRomInfo, usclssicRomName, NULL, NULL, NULL, NULL, UsclssicInputInfo, UsclssicDIPInfo,
	usclssicInit, DrvExit, DrvCalibr50Frame, seta1layerDraw, DrvScan, &DrvRecalc, 0x1200,
	240, 384, 3, 4
};


// Caliber 50 (Ver. 1.01)

static struct BurnRomInfo calibr50RomDesc[] = {
	{ "uh-002-001.3b",	0x40000, 0xeb92e7ed, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "uh-002-004.11b",	0x40000, 0x5a0ed31e, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "uh_001_003.9b",	0x10000, 0x0d30d09f, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "uh_001_002.7b",	0x10000, 0x7aecc3f9, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "uh-001-005.17e",	0x40000, 0x4a54c085, 0x02 | BRF_PRG | BRF_ESS }, //  4 m65c02 Code

	{ "uh-001-006.2m",	0x80000, 0xfff52f91, 0x0b | BRF_GRA },           //  5 Sprites
	{ "uh-001-007.4m",	0x80000, 0xb6c19f71, 0x0b | BRF_GRA },           //  6
	{ "uh-001-008.5m",	0x80000, 0x7aae07ef, 0x0b | BRF_GRA },           //  7
	{ "uh-001-009.6m",	0x80000, 0xf85da2c5, 0x0b | BRF_GRA },           //  8

	{ "uh-001-010.8m",	0x80000, 0xf986577a, 0x04 | BRF_GRA },           //  9 Layer 1 tiles
	{ "uh-001-011.9m",	0x80000, 0x08620052, 0x04 | BRF_GRA },           // 10

	{ "uh-001-013.12m",	0x80000, 0x09ec0df6, 0x06 | BRF_SND },           // 11 x1-010 Samples
	{ "uh-001-012.11m",	0x80000, 0xbb996547, 0x06 | BRF_SND },           // 12
};

STD_ROM_PICK(calibr50)
STD_ROM_FN(calibr50)

static INT32 calibr50Init()
{
	refresh_rate = 5742; // 57.42 hz
	watchdog_enable = 1;
	DrvSetColorOffsets(0, 0, 0);
	DrvSetVideoOffsets(-1, 2, -3, -2);

	has_raster = 1;

	INT32 nRet = DrvInit(calibr5068kInit, 8000000, SET_IRQLINES(0x80, 0x80) /*custom*/, NO_SPRITE_BUFFER, SET_GFX_DECODE(0, 1, -1));

	return nRet;
}

struct BurnDriver BurnDrvCalibr50 = {
	"calibr50", NULL, NULL, NULL, "1989",
	"Caliber 50 (Ver. 1.01)\0", NULL, "Athena / Seta", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SETA1, GBF_RUNGUN, 0,
	NULL, calibr50RomInfo, calibr50RomName, NULL, NULL, NULL, NULL, Calibr50InputInfo, Calibr50DIPInfo,
	calibr50Init, DrvExit, DrvCalibr50Frame, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	240, 384, 3, 4
};


// Crazy Fight

static struct BurnRomInfo crazyfgtRomDesc[] = {
	{ "rom.u3",			0x40000, 0xbf333e75, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "rom.u4",			0x40000, 0x505e9d47, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "rom.u228",		0x80000, 0x7181618e, 0x03 | BRF_GRA },           //  2 Sprites
	{ "rom.u227",		0x80000, 0x7905b5f2, 0x03 | BRF_GRA },           //  3
	{ "rom.u226",		0x80000, 0xef210e34, 0x03 | BRF_GRA },           //  4
	{ "rom.u225",		0x80000, 0x451b4419, 0x03 | BRF_GRA },           //  5

	{ "rom.u67",		0x40000, 0xec8c6831, 0x04 | BRF_GRA },           //  6 Layer 1 tiles
	{ "rom.u68",		0x80000, 0x2124312e, 0x04 | BRF_GRA },           //  7

	{ "rom.u65",		0x40000, 0x58448231, 0x05 | BRF_GRA },           //  8 Layer 2 tiles
	{ "rom.u66",		0x80000, 0xc6f7735b, 0x05 | BRF_GRA },           //  9

	{ "rom.u85",		0x40000, 0x7b95d0bb, 0x06 | BRF_SND },           // 10 OKI M6295 Samples

	{ "ds2430a.bin",	0x00028, 0x3b5ea5d2, 0x00 | BRF_OPT },           // 11
};

STD_ROM_PICK(crazyfgt)
STD_ROM_FN(crazyfgt)

static INT32 crazyfgtInit()
{
	DrvSetColorOffsets(0, 0xa00, 0x200);
	DrvSetVideoOffsets(8, 0, 6, 0);

	INT32 nRet = DrvInit(crazyfgt68kInit, 16000000, SET_IRQLINES(0x80, 0x80) /*custom*/, NO_SPRITE_BUFFER, SET_GFX_DECODE(5, 4, 4));

	if (nRet == 0) {
		gundharaSetColorTable();
	}
	
	return nRet;
}

struct BurnDriver BurnDrvCrazyfgt = {
	"crazyfgt", NULL, NULL, NULL, "1996",
	"Crazy Fight\0", NULL, "Subsino", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA1, GBF_SHOOT, 0,
	NULL, crazyfgtRomInfo, crazyfgtRomName, NULL, NULL, NULL, NULL, CrazyfgtInputInfo, CrazyfgtDIPInfo,
	crazyfgtInit, DrvExit, CrazyfgtFrame, seta2layerDraw, DrvScan, &DrvRecalc, 0x1200,
	384, 224, 4, 3
};


//----------------------------------------------------------------------------------------------------------
// should be moved into its own file

static UINT8 msm6242_reg[3];
static time_t msm6242_hold_time;

UINT8 msm6242_read(UINT32 offset)
{
	tm *systime;

	if (msm6242_reg[0] & 1) {
		systime = localtime(&msm6242_hold_time);
	} else {
		time_t curtime = time(NULL);
		systime = localtime(&curtime);
	}

	switch (offset)
	{
		case 0x00: return systime->tm_sec % 10;
		case 0x01: return systime->tm_sec / 10;
		case 0x02: return systime->tm_min % 10;
		case 0x03: return systime->tm_min / 10;
		case 0x04:
		case 0x05:
		{
			INT32 hour = systime->tm_hour, pm = 0;

			if ((msm6242_reg[2] & 0x04) == 0) // 12 hour mode?
			{
				if (hour >= 12) pm = 1;
				hour %= 12;
				if (hour == 0) hour = 12;
			}

			if (offset == 0x04) return hour % 10;

			return (hour / 10) | (pm <<2);
		}

		case 0x06: return  systime->tm_mday % 10;
		case 0x07: return  systime->tm_mday / 10;
		case 0x08: return (systime->tm_mon+1) % 10;
		case 0x09: return (systime->tm_mon+1) / 10;
		case 0x0a: return  systime->tm_year % 10;
		case 0x0b: return (systime->tm_year % 100) / 10;
		case 0x0c: return  systime->tm_wday;
		case 0x0d: return msm6242_reg[0];
		case 0x0e: return msm6242_reg[1];
		case 0x0f: return msm6242_reg[2];
	}

	return 0;
}

void msm6242_write(UINT32 offset, UINT8 data)
{
	if (offset == 0x0d) {
		msm6242_reg[0] = data & 0x0f;
		if (data & 1) msm6242_hold_time = time(NULL);
	} else if (offset == 0x0e) {
		msm6242_reg[1] = data & 0x0f;
	} else if (offset == 0x0f) {
		if ((data ^ msm6242_reg[2]) & 0x04) {
			msm6242_reg[2] = (msm6242_reg[2] & 0x04) | (data & ~0x04);

			if (msm6242_reg[2] & 1)	msm6242_reg[2] = (msm6242_reg[2] & ~0x04) | (data & 0x04);
		} else {
			msm6242_reg[2] = data & 0x0f;
		}
	}
}

void msm6242_reset()
{
	memset (msm6242_reg, 0, 3);
	msm6242_hold_time = time(NULL);
}

//--------------------------------------------------------------------------------------------------------------------

static UINT16 jockeyc_dsw_read(INT32 offset)
{
	INT32 dip2 = DrvDips[1] | (DrvDips[2] << 8);
	INT32 shift = offset << 2;

	return	((((DrvDips[0] >> shift) & 0xf)) << 0) | ((((dip2 >> shift) & 0xf)) << 4) | ((((dip2 >> (shift+8)) & 0xf)) << 8);
}

UINT16 __fastcall jockeyc_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x200000:
		case 0x200001: {
			INT32 i;
			for (i = 3; i < 8; i++) {
				if (usclssic_port_select & (1 << i)) return DrvInputs[(i - 3) + 2];
			}
			return 0xffff;
		}

		case 0x200002:
		case 0x200003:
			return DrvInputs[0];

		case 0x200010:
		case 0x200011: 
			return DrvInputs[1] & 0x7fff;

		case 0x500000:
		case 0x500001:
		case 0x500002:
		case 0x500003:
			return jockeyc_dsw_read(address & 2);

		case 0x600000:
		case 0x600001:
		case 0x600002:
		case 0x600003:
			return ~0;
	}

	if ((address & 0xfffffe0) == 0x800000) {
		return msm6242_read((address & 0x1e) / 2);
	}

	return 0;
}

UINT8 __fastcall jockeyc_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x200000:
		case 0x200001: {
			INT32 i;
			for (i = 3; i < 8; i++) {
				if (usclssic_port_select & (1 << i)) return DrvInputs[(i - 3) + 2];
			}
			return 0xff;
		}

		case 0x200002:
			return DrvInputs[0] >> 8;

		case 0x200003:
			return DrvInputs[0];

		case 0x200010:
			return (DrvInputs[1] >> 8) & 0x7f;

		case 0x200011:
			return DrvInputs[1];

		case 0x500000:
		case 0x500001:
		case 0x500002:
		case 0x500003:
			return jockeyc_dsw_read(address & 2);

		case 0x600000:
		case 0x600001:
		case 0x600002:
		case 0x600003:
			return ~0;
	}

	if ((address & 0xfffffe0) == 0x800000) {
		return msm6242_read((address & 0x1e) / 2);
	}

	return 0;
}

void __fastcall jockeyc_write_word(UINT32 address, UINT16 data)
{
	SetaVidRAMCtrlWriteWord(0, 0xa00000)

	switch (address)
	{
		case 0x200000:
		case 0x200001:
			usclssic_port_select = data & 0xf8;
		return;

		case 0x300000:
		case 0x300001:
			watchdog = 0;
		return;
	}

	if ((address & 0xfffffe0) == 0x800000) {
		msm6242_write((address & 0x1e) / 2, data);
		return;
	}
}

void __fastcall jockeyc_write_byte(UINT32 address, UINT8 data)
{
	SetaVidRAMCtrlWriteByte(0, 0xa00000)

	switch (address)
	{
		case 0x200000:
		case 0x200001:
			usclssic_port_select = data & 0xf8;
		return;

		case 0x300000:
		case 0x300001:
			watchdog = 0;
		return;
	}

	if ((address & 0xfffffe0) == 0x800000) {
		msm6242_write((address & 0x1e) / 2, data);
		return;

	}
}

static void jockeyc68kInit()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 		0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(DrvVidRAM0,		0xb00000, 0xb07fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,		0xd00000, 0xd00607 | 0x7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,		0xe00000, 0xe03fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xffc000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,		jockeyc_write_word);
	SekSetWriteByteHandler(0,		jockeyc_write_byte);
	SekSetReadWordHandler(0,		jockeyc_read_word);
	SekSetReadByteHandler(0,		jockeyc_read_byte);

	SekMapHandler(1,			0x900000, 0x903fff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler (1,		setaSoundRegReadWord);
	SekSetReadByteHandler (1,		setaSoundRegReadByte);
	SekSetWriteWordHandler(1,		setaSoundRegWriteWord);
	SekSetWriteByteHandler(1,		setaSoundRegWriteByte);
	SekClose();

	memcpy (Drv68KROM + 0x100000, Drv68KROM + 0x020000, 0x080000);
	memset (Drv68KROM + 0x020000, 0xff, 0x60000);
	memset (Drv68KROM + 0x180000, 0xff, 0x80000);

	memmove (DrvGfxROM1 + 0x60000, DrvGfxROM1 + 0x40000, 0x40000);
	memset  (DrvGfxROM1 + 0x40000, 0, 0x20000);
	memset  (DrvGfxROM1 + 0xa0000, 0, 0x20000);

	DrvROMLen[4] = 1; // force use of pal ram

	msm6242_reset();
}

static INT32 jockeycInit()
{
	watchdog_enable = 1;
	DrvSetColorOffsets(0, 0, 0);
	DrvSetVideoOffsets(0, 0, 0, 0);

	return DrvInit(jockeyc68kInit, 16000000, SET_IRQLINES(0x80, 0x80) /*custom*/, NO_SPRITE_BUFFER, SET_GFX_DECODE(5, 1, -1));
}

static void jockeycFrameCallback()
{
	DrvInputs[0] ^= 0xffff;
	DrvInputs[0] ^= DrvDips[3] | (DrvDips[4] << 8);
	DrvInputs[1] ^= 0xffff;
	DrvInputs[1] ^= DrvDips[5] | (DrvDips[6] << 8);

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[1] = { cpuspeed / 60 };
	INT32 nCyclesDone[1]  = { 0 };

	INT32 irqs[10] = { 4, 1, 2, 6, 6, 6, 6, 6, 6, 6 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		SekSetIRQLine(irqs[9-i], CPU_IRQSTATUS_AUTO); // ?
	}

	SekClose();

	if (pBurnSoundOut) {
		x1010_sound_update();
	}
}

static INT32 jockeycFrame()
{
	return DrvCommonFrame(jockeycFrameCallback);
}


// Jockey Club (v1.18)

static struct BurnRomInfo jockeycRomDesc[] = {
	{ "ya_007_002.u23",	0x10000, 0xc499bf4d, 0x01 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ya_007_003.u33",	0x10000, 0xe7b0677e, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ya-002-001.u18",	0x80000, 0xdd108016, 0x01 | BRF_PRG | BRF_ESS }, //  2

	{ "ya-001-004-t74.u10",	0x80000, 0xeb74d2e0, 0x03 | BRF_GRA },           //  3 Sprites
	{ "ya-001-005-t75.u17",	0x80000, 0x4a6c804b, 0x03 | BRF_GRA },           //  4
	{ "ya-001-006-t76.u22",	0x80000, 0xbfae01a5, 0x03 | BRF_GRA },           //  5
	{ "ya-001-007-t77.u27",	0x80000, 0x2dc7a294, 0x03 | BRF_GRA },           //  6

	{ "ya-001-008-t59.u35",	0x40000, 0x4b890f83, 0x04 | BRF_GRA },           //  7 Layer 1 tiles
	{ "ya-001-009-t60.u41",	0x40000, 0xcaa5e3c1, 0x04 | BRF_GRA },           //  8
// double this so that we can use the graphics decoding routines...
	{ "ya-001-009-t60.u41",	0x40000, 0xcaa5e3c1, 0x04 | BRF_GRA },           //  9

	{ "ya-001-013.u71",	0x80000, 0x2bccaf47, 0x06 | BRF_SND },           // 10 x1snd
	{ "ya-001-012.u64",	0x80000, 0xa8015ce6, 0x06 | BRF_SND },           // 11

	{ "ya1-011.prom",	0x00200, 0xbd4fe2f6, 0x0f | BRF_GRA },           // 13
	{ "ya1-010.prom",	0x00200, 0x778094b3, 0x0f | BRF_GRA },           // 12 Color PROMs
};

STD_ROM_PICK(jockeyc)
STD_ROM_FN(jockeyc)

struct BurnDriverD BurnDrvJockeyc = {
	"jockeyc", NULL, NULL, NULL, "1990",
	"Jockey Club (v1.18)\0", NULL, "Seta (Visco license)", "Seta",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_SETA1, GBF_SPORTSMISC, 0,
	NULL, jockeycRomInfo, jockeycRomName, NULL, NULL, NULL, NULL, JockeycInputInfo, JockeycDIPInfo,
	jockeycInit, DrvExit, jockeycFrame, seta1layerDraw, DrvScan, &DrvRecalc, 0x200,
	384, 240, 4, 3
};
