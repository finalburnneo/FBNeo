// FB Neo Donkey Kong driver module
// Based on MAME driver by Couriersud

// still need:

// sbdk
// 8ballact
// 8ballact2
// shootgal
// splforc
// splfrcii
// strtheat



#include "tiles_generic.h"
#include "z80_intf.h"
#include "s2650_intf.h"
#include "m6502_intf.h"
#include "samples.h"
#include "eeprom.h"
#include "bitswap.h"
#include "8257dma.h"
#include "mcs48.h"
#include "dac.h"
#include "tms5110.h"
#include "nes_apu.h"
#include "resnet.h"
#include <math.h> // for exp()
#include "biquad.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv2650ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvMapROM;
static UINT8 *DrvZ80RAM;
static UINT8 *Drv2650RAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static INT32 *DrvRevMap;
static UINT8 *DrvSndRAM0;
static UINT8 *DrvSndRAM1;

static UINT8 *i8039_t;
static UINT8 *i8039_p;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static INT32 nExtraCycles[2];

static UINT8 *soundlatch;
static UINT8 *gfx_bank;
static UINT8 *sprite_bank;
static UINT8 *palette_bank;
static UINT8 *grid_enable;
static UINT8 *grid_color;
static UINT8 *flipscreen;
static UINT8 *nmi_mask;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 vblank;
static void (*DrvPaletteUpdate)();

// set in init
static UINT8 draktonmode = 0;
static UINT8 brazemode = 0;
static UINT8 radarscp = 0;
static UINT8 radarscp1 = 0;
static INT32 s2650_protection = 0;

// driver variables
static INT32 sound_cpu_in_reset; // dkong3
static UINT8 dkongjr_walk = 0;
static UINT8 sndpage = 0;
static UINT8 sndstatus;
static UINT8 sndgrid_en;
static UINT8 dma_latch = 0;
static UINT8 sample_state[8];
static UINT8 sample_count;
static UINT8 climb_data;
static double envelope_ctr;
static INT32 decay;
static INT32 braze_bank = 0; // for braze & drakton(epos) banking
static UINT8 decrypt_counter = 0; // drakton (epos)
static INT32 palette_type = -1;

static BIQ biqdac;
static BIQ biqdac2;

static INT32 hunch_prot_ctr = 0; // hunchback (s2650)
static UINT8 hunchloopback = 0;
static UINT8 main_fo = 0;

// radar scope
#define TRS_J1  (1)
#define RADARSCP_BCK_COL_OFFSET         256
#define RADARSCP_GRID_COL_OFFSET        (RADARSCP_BCK_COL_OFFSET + 256)
#define RADARSCP_STAR_COL               (RADARSCP_GRID_COL_OFFSET + 8)

#define RC1     (2.2e3 * 22e-6) /*  22e-6; */
#define RC17    (33e-6 * 1e3 * (0*4.7+1.0/(1.0/10.0+1.0/20.0+0.0/0.3)))
#define RC2     (10e3 * 33e-6)
#define RC31    (38e3 * 33e-6) // erase grid time
// note about RC32:
// - https://www.youtube.com/watch?v=IDbozRrjQks was used as reference for voice version (radarscp1)
// - https://www.youtube.com/watch?v=nSkOayc9YIQ was used as reference for non-voice version (radarscp and radarscpc)
// - the non-voice version arguably seems more "natural": the grid reaches the borders, then change color at the next frame, then the bottom ui appears at the next frame
// - it might be that the voice version pcb from that 1st video simply has a bad capacitor
#define RC32    ((18e3 + 68e3) * (radarscp1 == 1 ? 333e-6 : 111e-6)) // display grid time
#define RC4     (90e3 * 0.47e-6)
#define dt      (1./60./(double) (264))
#define period2 (((INT64)(6144000) * ( 33L * 68L )) / (INT32)10000000L / 3)  /*  period/2 in pixel ... */

static const double cd4049_vl = 1.5/5.0;
static const double cd4049_vh = 3.5/5.0;
static const double cd4049_al = 0.01;
static const double cd4049_b = (log(0.0 - log(cd4049_al)) - log(0.0 - log((1.0-cd4049_al))) ) / log(cd4049_vh/cd4049_vl);
static const double cd4049_a = log(0.0 - log(cd4049_al)) - cd4049_b * log(cd4049_vh);
static UINT8 sig30Hz;
static UINT8 lfsr_5I;
static UINT8 grid_sig;
static UINT8 rflip_sig;
static UINT8 star_ff;
static UINT8 blue_level;
static double cv1;
static double cv2;
static double cv3;
static double cv4;
static double vg1;
static double vg2;
static double vg3;
static double vc17;
static INT32 pixelcnt;

static struct BurnInputInfo DkongInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Dkong)

static struct BurnInputInfo Dkong3InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Service",	    	BIT_DIGITAL,	DrvJoy1 + 7,	"service"},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Dkong3)

static struct BurnInputInfo RadarscpInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Radarscp)

static struct BurnInputInfo PestplceInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Pestplce)

static struct BurnInputInfo HerodkInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Herodk)

static struct BurnDIPInfo DkongNoDipDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x01, NULL						},

	{0   , 0xfe, 0   ,    3, "Palette"					},
	{0x0f, 0x01, 0x03, 0x01, "New"						},
	{0x0f, 0x01, 0x03, 0x00, "Radarscope Conversion"	},
	{0x0f, 0x01, 0x03, 0x02, "Legacy"					},
};

STDDIPINFO(DkongNoDip)

static struct BurnDIPInfo DkongDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x80, NULL			},
	{0x0f, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0e, 0x01, 0x03, 0x00, "3"			},
	{0x0e, 0x01, 0x03, 0x01, "4"			},
	{0x0e, 0x01, 0x03, 0x02, "5"			},
	{0x0e, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0e, 0x01, 0x0c, 0x00, "7000"			},
	{0x0e, 0x01, 0x0c, 0x04, "10000"		},
	{0x0e, 0x01, 0x0c, 0x08, "15000"		},
	{0x0e, 0x01, 0x0c, 0x0c, "20000"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0e, 0x01, 0x70, 0x70, "5 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x50, "4 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x30, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x10, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x00, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x70, 0x20, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x70, 0x40, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x70, 0x60, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x80, 0x80, "Upright"		},
	{0x0e, 0x01, 0x80, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    3, "Palette"					},
	{0x0f, 0x01, 0x03, 0x01, "New"						},
	{0x0f, 0x01, 0x03, 0x00, "Radarscope Conversion"	},
	{0x0f, 0x01, 0x03, 0x02, "Legacy"					},
};

STDDIPINFO(Dkong)

static struct BurnDIPInfo DkongfDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x80, NULL			},
	{0x0f, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0e, 0x01, 0x03, 0x00, "3"			},
	{0x0e, 0x01, 0x03, 0x01, "4"			},
	{0x0e, 0x01, 0x03, 0x02, "5"			},
	{0x0e, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0e, 0x01, 0x0c, 0x00, "7000"			},
	{0x0e, 0x01, 0x0c, 0x04, "10000"		},
	{0x0e, 0x01, 0x0c, 0x08, "15000"		},
	{0x0e, 0x01, 0x0c, 0x0c, "20000"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0e, 0x01, 0x70, 0x70, "Free Play"		},
	{0x0e, 0x01, 0x70, 0x50, "4 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x30, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x10, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x00, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x70, 0x20, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x70, 0x40, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x70, 0x60, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x80, 0x80, "Upright"		},
	{0x0e, 0x01, 0x80, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    3, "Palette"					},
	{0x0f, 0x01, 0x03, 0x01, "New"						},
	{0x0f, 0x01, 0x03, 0x00, "Radarscope Conversion"	},
	{0x0f, 0x01, 0x03, 0x02, "Legacy"					},
};

STDDIPINFO(Dkongf)

static struct BurnDIPInfo Dkong3bDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x00, NULL			},
	{0x0f, 0xff, 0xff, 0x05, NULL			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0e, 0x01, 0x07, 0x02, "3 Coins 1 Credit"	},
	{0x0e, 0x01, 0x07, 0x04, "2 Coins 1 Credit"	},
	{0x0e, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x07, 0x01, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x0e, 0x01, 0x07, 0x05, "1 Coin  5 Credits"	},
	{0x0e, 0x01, 0x07, 0x07, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x08, 0x00, "Upright"		},
	{0x0e, 0x01, 0x08, 0x08, "Cocktail"		},

	{0   , 0xfe, 0   ,    3, "Palette"	},
	{0x0f, 0x01, 0x07, 0x05, "New"		},
	{0x0f, 0x01, 0x07, 0x06, "Legacy"	},
};

STDDIPINFO(Dkong3b)

static struct BurnDIPInfo Dkong3DIPList[]=
{
	{0x10, 0xff, 0xff, 0x00, NULL			},
	{0x11, 0xff, 0xff, 0x00, NULL			},
	{0x12, 0xff, 0xff, 0x05, NULL			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x10, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x07, 0x01, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0x07, 0x05, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0x07, 0x07, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x40, 0x00, "Off"			},
	{0x10, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x80, 0x00, "Upright"		},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x03, 0x00, "3"			},
	{0x11, 0x01, 0x03, 0x01, "4"			},
	{0x11, 0x01, 0x03, 0x02, "5"			},
	{0x11, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x0c, 0x00, "30000"		},
	{0x11, 0x01, 0x0c, 0x04, "40000"		},
	{0x11, 0x01, 0x0c, 0x08, "50000"		},
	{0x11, 0x01, 0x0c, 0x0c, "None"			},

	{0   , 0xfe, 0   ,    4, "Additional Bonus"	},
	{0x11, 0x01, 0x30, 0x00, "30000"		},
	{0x11, 0x01, 0x30, 0x10, "40000"		},
	{0x11, 0x01, 0x30, 0x20, "50000"		},
	{0x11, 0x01, 0x30, 0x30, "None"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0xc0, 0x00, "(1) Easy"			},
	{0x11, 0x01, 0xc0, 0x40, "(2)"				},
	{0x11, 0x01, 0xc0, 0x80, "(3)"				},
	{0x11, 0x01, 0xc0, 0xc0, "(4) Hard"			},

	{0   , 0xfe, 0   ,    3, "Palette"	},
	{0x12, 0x01, 0x07, 0x05, "New"		},
	{0x12, 0x01, 0x07, 0x06, "Legacy"	},
};

STDDIPINFO(Dkong3)

static struct BurnDIPInfo RadarscpDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x80, NULL			},
	{0x0b, 0xff, 0xff, 0x03, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0a, 0x01, 0x03, 0x00, "3"			},
	{0x0a, 0x01, 0x03, 0x01, "4"			},
	{0x0a, 0x01, 0x03, 0x02, "5"			},
	{0x0a, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0a, 0x01, 0x0c, 0x00, "7000"			},
	{0x0a, 0x01, 0x0c, 0x04, "10000"		},
	{0x0a, 0x01, 0x0c, 0x08, "15000"		},
	{0x0a, 0x01, 0x0c, 0x0c, "20000"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0a, 0x01, 0x70, 0x70, "5 Coins 1 Credits"	},
	{0x0a, 0x01, 0x70, 0x50, "4 Coins 1 Credits"	},
	{0x0a, 0x01, 0x70, 0x30, "3 Coins 1 Credits"	},
	{0x0a, 0x01, 0x70, 0x10, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0x70, 0x00, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0x70, 0x20, "1 Coin  2 Credits"	},
	{0x0a, 0x01, 0x70, 0x40, "1 Coin  3 Credits"	},
	{0x0a, 0x01, 0x70, 0x60, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0a, 0x01, 0x80, 0x80, "Upright"		},
	{0x0a, 0x01, 0x80, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    3, "Palette"	},
	{0x0b, 0x01, 0x03, 0x03, "New"		},
	{0x0b, 0x01, 0x03, 0x02, "Legacy"	},
};

STDDIPINFO(Radarscp)

static struct BurnDIPInfo Radarscp1DIPList[]=
{
	{0x0a, 0xff, 0xff, 0x80, NULL			},
	{0x0b, 0xff, 0xff, 0x04, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0a, 0x01, 0x03, 0x00, "3"			},
	{0x0a, 0x01, 0x03, 0x01, "4"			},
	{0x0a, 0x01, 0x03, 0x02, "5"			},
	{0x0a, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0a, 0x01, 0x0c, 0x00, "7000"			},
	{0x0a, 0x01, 0x0c, 0x04, "10000"		},
	{0x0a, 0x01, 0x0c, 0x08, "15000"		},
	{0x0a, 0x01, 0x0c, 0x0c, "20000"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0a, 0x01, 0x70, 0x70, "5 Coins 1 Credits"	},
	{0x0a, 0x01, 0x70, 0x50, "4 Coins 1 Credits"	},
	{0x0a, 0x01, 0x70, 0x30, "3 Coins 1 Credits"	},
	{0x0a, 0x01, 0x70, 0x10, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0x70, 0x00, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0x70, 0x20, "1 Coin  2 Credits"	},
	{0x0a, 0x01, 0x70, 0x40, "1 Coin  3 Credits"	},
	{0x0a, 0x01, 0x70, 0x60, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0a, 0x01, 0x80, 0x80, "Upright"		},
	{0x0a, 0x01, 0x80, 0x00, "Cocktail"		},

#if 0
	// note : legacy palette never worked properly for this one
	{0   , 0xfe, 0   ,    3, "Palette"	},
	{0x0b, 0x01, 0x07, 0x04, "New"		},
	{0x0b, 0x01, 0x07, 0x02, "Legacy"	},
#endif
};

STDDIPINFO(Radarscp1)

static struct BurnDIPInfo PestplceDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x20, NULL			},
	{0x0f, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0e, 0x01, 0x03, 0x00, "3"			},
	{0x0e, 0x01, 0x03, 0x01, "4"			},
	{0x0e, 0x01, 0x03, 0x02, "5"			},
	{0x0e, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0e, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x1c, 0x10, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x1c, 0x18, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x1c, 0x04, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x1c, 0x0c, "1 Coin  4 Credits"	},
	{0x0e, 0x01, 0x1c, 0x14, "1 Coin  5 Credits"	},
	{0x0e, 0x01, 0x1c, 0x1c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "2 Players Game"	},
	{0x0e, 0x01, 0x20, 0x00, "1 Credit"		},
	{0x0e, 0x01, 0x20, 0x20, "2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0e, 0x01, 0xc0, 0x00, "20000"		},
	{0x0e, 0x01, 0xc0, 0x40, "30000"		},
	{0x0e, 0x01, 0xc0, 0x80, "40000"		},
	{0x0e, 0x01, 0xc0, 0xc0, "None"			},

	{0   , 0xfe, 0   ,    3, "Palette"					},
	{0x0f, 0x01, 0x03, 0x01, "New"						},
	{0x0f, 0x01, 0x03, 0x00, "Radarscope Conversion"	},
	{0x0f, 0x01, 0x03, 0x02, "Legacy"					},
};

STDDIPINFO(Pestplce)

static struct BurnDIPInfo HerbiedkDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x80, NULL			},
	{0x0f, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0e, 0x01, 0x70, 0x70, "5 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x50, "4 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x30, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x10, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x00, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x70, 0x20, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x70, 0x40, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x70, 0x60, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x80, 0x80, "Upright"		},
	{0x0e, 0x01, 0x80, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    3, "Palette"					},
	{0x0f, 0x01, 0x03, 0x01, "New"						},
	{0x0f, 0x01, 0x03, 0x00, "Radarscope Conversion"	},
	{0x0f, 0x01, 0x03, 0x02, "Legacy"					},
};

STDDIPINFO(Herbiedk)

static struct BurnDIPInfo HunchbkdDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x80, NULL			},
	{0x0f, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0e, 0x01, 0x02, 0x00, "3"			},
	{0x0e, 0x01, 0x02, 0x02, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0e, 0x01, 0x0c, 0x00, "10000"		},
	{0x0e, 0x01, 0x0c, 0x04, "20000"		},
	{0x0e, 0x01, 0x0c, 0x08, "40000"		},
	{0x0e, 0x01, 0x0c, 0x0c, "80000"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0e, 0x01, 0x70, 0x70, "5 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x50, "4 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x30, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x10, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x70, 0x00, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x70, 0x20, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x70, 0x40, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x70, 0x60, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x80, 0x80, "Upright"		},
	{0x0e, 0x01, 0x80, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    3, "Palette"					},
	{0x0f, 0x01, 0x03, 0x01, "New"						},
	{0x0f, 0x01, 0x03, 0x00, "Radarscope Conversion"	},
	{0x0f, 0x01, 0x03, 0x02, "Legacy"					},
};

STDDIPINFO(Hunchbkd)

static struct BurnDIPInfo HerodkDIPList[]=
{
	{0x10, 0xff, 0xff, 0x81, NULL			},
	{0x11, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x10, 0x01, 0x02, 0x00, "3"			},
	{0x10, 0x01, 0x02, 0x02, "5"			},

	{0   , 0xfe, 0   ,    4, "Difficulty?"		},
	{0x10, 0x01, 0x0c, 0x00, "0"			},
	{0x10, 0x01, 0x0c, 0x04, "1"			},
	{0x10, 0x01, 0x0c, 0x08, "2"			},
	{0x10, 0x01, 0x0c, 0x0c, "3"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x10, 0x01, 0x70, 0x70, "5 Coins 1 Credits"	},
	{0x10, 0x01, 0x70, 0x50, "4 Coins 1 Credits"	},
	{0x10, 0x01, 0x70, 0x30, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x70, 0x10, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x70, 0x00, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x70, 0x20, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x70, 0x40, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x70, 0x60, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x80, 0x80, "Upright"		},
	{0x10, 0x01, 0x80, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    3, "Palette"					},
	{0x11, 0x01, 0x03, 0x01, "New"						},
	{0x11, 0x01, 0x03, 0x00, "Radarscope Conversion"	},
	{0x11, 0x01, 0x03, 0x02, "Legacy"					},
};

STDDIPINFO(Herodk)

static struct BurnDIPInfo DraktonDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x1d, NULL		},
	{0x0f, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0e, 0x01, 0x01, 0x00, "Off"		},
	{0x0e, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0e, 0x01, 0x06, 0x00, "3"		},
	{0x0e, 0x01, 0x06, 0x02, "4"		},
	{0x0e, 0x01, 0x06, 0x04, "5"		},
	{0x0e, 0x01, 0x06, 0x06, "6"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0e, 0x01, 0x08, 0x00, "Easy"		},
	{0x0e, 0x01, 0x08, 0x08, "Normal"		},

	{0   , 0xfe, 0   ,    8, "Bonus Life"		},
	{0x0e, 0x01, 0x70, 0x00, "10000"		},
	{0x0e, 0x01, 0x70, 0x10, "20000"		},
	{0x0e, 0x01, 0x70, 0x20, "30000"		},
	{0x0e, 0x01, 0x70, 0x30, "40000"		},
	{0x0e, 0x01, 0x70, 0x40, "50000"		},
	{0x0e, 0x01, 0x70, 0x50, "60000"		},
	{0x0e, 0x01, 0x70, 0x60, "70000"		},
	{0x0e, 0x01, 0x70, 0x70, "80000"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x80, 0x00, "Upright"		},
	{0x0e, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    3, "Palette"					},
	{0x0f, 0x01, 0x03, 0x01, "New"						},
	{0x0f, 0x01, 0x03, 0x00, "Radarscope Conversion"	},
	{0x0f, 0x01, 0x03, 0x02, "Legacy"					},
};

STDDIPINFO(Drakton)

static void dkong_sample_play(INT32 offset, UINT8 data)
{
	INT32 sample_order[7] = { 1, 2, 1, 2, 0, 1, 0 };

	if (sample_state[offset] != data)
	{
		if (data) {
			if (radarscp) {
				// radarscrap (7d07 mapped to offset 3 !)
				BurnSamplePlay(offset);
			} else {
				// dkong
				if (offset) {
					BurnSamplePlay(offset+2);
				} else {
					BurnSamplePlay(sample_order[sample_count]);
					sample_count++;
					if (sample_count == 7) sample_count = 0;
				}
			}
		} else {
			if (radarscp && offset == 3) {
				BurnSampleStop(3); // siren
			}
		}

		sample_state[offset] = data;
	}
}

static void sync_sound()
{
	INT32 cyc = (INT64)ZetTotalCycles(0) * (6000000 / 15) / 3072000;
	cyc -= mcs48TotalCycles();
	if (cyc > 0) {
		mcs48Run(cyc);
	}
}

static void __fastcall dkong_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x7800) {
		i8257Write(address, data);
		return;
	}

	switch (address)
	{
		case 0x7c00:
			sync_sound();
			*soundlatch = (data & 0x0f) ^ 0x0f;
		return;

		case 0x7c80:
			*gfx_bank = data & 1; // inverted for dkong3
		return;

		case 0x7d00:
		case 0x7d01:
		case 0x7d02:
			dkong_sample_play(address & 3, data);
		return;

		case 0x7d03:
			sync_sound();
			i8039_p[2] = (i8039_p[2] & ~0x20) | ((~data & 1) << 5);
		return;

		case 0x7d04:
			sync_sound();
			i8039_t[1] = ~data & 1;
		return;

		case 0x7d05:
			sync_sound();
			i8039_t[0] = ~data & 1;
		return;

		case 0x7d07:
			if (radarscp) dkong_sample_play(3, data);
		return;

		case 0x7d80:
			sync_sound();
			mcs48SetIRQLine(0, data ? 1 : 0);
		return;

		case 0x7d82:
			*flipscreen = data & 0x01;
		return;

		case 0x7d83:
			*sprite_bank = data & 0x01;
		return;

		case 0x7d84:
			*nmi_mask = data & 0x01;
			if (~data & 0x01) {
				ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			}
		return;

		case 0x7d85:
			i8257_drq_write(0, data & 0x01);
			i8257_drq_write(1, data & 0x01);
			i8257_do_transfer(data & 0x01);
		return;

		case 0x7d86:
		case 0x7d87:
			if (data & 0x01) {
				*palette_bank |=  (1 << (address & 1));
			} else {
				*palette_bank &= ~(1 << (address & 1));
			}
		return;
	}
}

static UINT8 __fastcall dkong_main_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x7800) {
		return i8257Read(address);
	}

	switch (address)
	{
		case 0x7c00:
			return DrvInputs[0];

		case 0x7c80:
			return DrvInputs[1];

		case 0x7d00:
			{
				sync_sound();
				UINT8 ret = DrvInputs[2] & 0xbf;
				if (ret & 0x10) ret = (ret & ~0x10) | 0x80;
				ret |= sndstatus << 6;
				return ret;
			}
			return 0;

		case 0x7d80:
			return DrvDips[0];
	}



	return 0;
}

static inline void dkongjr_climb_write(UINT8 data)
{
	INT32 sample_order[7] = { 1, 2, 1, 2, 0, 1, 0 };

	if (climb_data != data)
	{
		if (data)
		{
			BurnSamplePlay(sample_order[sample_count]+((dkongjr_walk) ? 8 : 3));
			sample_count++;
			if (sample_count == 7) sample_count = 0;
		}
		climb_data = data;
	}
}

static inline void dkongjr_sample_play(INT32 offs, UINT8 data, INT32 stop) // jump, land[s], roar, snapjaw[s], death[s], drop
{
	UINT8 sample[8] = { 0, 1, 2, 11, 6, 7 };

	if (sample_state[offs] != data)
	{
		if (stop) {
			if (data) BurnSampleStop(7);
			BurnSamplePlay(sample[offs]);
		} else {
			if (data) BurnSamplePlay(sample[offs]);
		}

		sample_state[offs] = data;
	}
}

static void __fastcall dkongjr_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x7c00:
			sync_sound();
			*soundlatch = data;
		return;

		case 0x7c81:
			sync_sound();
			i8039_p[2] = (i8039_p[2] & ~0x40) | ((~data & 1) << 6);
		return;

		case 0x7d00:
			dkongjr_climb_write(data);
		return;

		case 0x7d01:
			dkongjr_sample_play(0, data, 0);
		return;

		case 0x7d02:
			dkongjr_sample_play(1, data, 1);
		return;

		case 0x7d03:
			dkongjr_sample_play(2, data, 0);
		return;

		case 0x7d06:
			dkongjr_sample_play(3, data, 1);
		return;

		case 0x7d07:
			dkongjr_walk = data;
		return;

		case 0x7d80:
			dkongjr_sample_play(4, data, 1);
		return;

		case 0x7d81:
			dkongjr_sample_play(5, data, 0);
		return;
	}

	dkong_main_write(address, data);
}

static void __fastcall radarscp_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x7c80:
			*grid_color = (data & 0x07) ^ 0x07;
		return;

		case 0x7d81:
			*grid_enable = data & 0x01;
		return;
	}

	dkong_main_write(address, data);
}

static void __fastcall dkong3_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x7c00:
			soundlatch[0] = data;
		return;

		case 0x7c80:
			soundlatch[1] = data;
		return;

		case 0x7d00:
			soundlatch[2] = data;
		return;

		case 0x7d80:
			if (data & 1) {
				sound_cpu_in_reset = 0;
				M6502Open(0);
				M6502Reset();
				M6502Close();

				M6502Open(1);
				M6502Reset();
				M6502Close();
			} else {
				sound_cpu_in_reset = 1;
			}
		return;

		case 0x7e80:
			// coin_counter
		return;

		case 0x7e81:
			*gfx_bank = data & 0x01;
		return;

		case 0x7e82:
			*flipscreen = ~data & 0x01;
		return;

		case 0x7e83:
			*sprite_bank = data & 0x01;
		return;

		case 0x7e84:
			*nmi_mask = data & 0x01;
		return;

		case 0x7e85: // dma
		return;

		case 0x7e86:
		case 0x7e87:
			if (data & 0x01) {
				*palette_bank |=  (1 << (address & 1));
			} else {
				*palette_bank &= ~(1 << (address & 1));
			}
		return;
	}
}

static UINT8 __fastcall dkong3_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x7c00:
			return DrvInputs[0];

		case 0x7c80:
			return DrvInputs[1];

		case 0x7d00:
			return DrvDips[0];

		case 0x7d80:
			return DrvDips[1];
	}

	return 0;
}


static void braze_bankswitch(INT32 data)
{
	braze_bank = data;
	INT32 bank = (data & 0x01) * 0x8000;

	ZetMapMemory(DrvZ80ROM + bank, 0x0000, 0x5fff, MAP_ROM);

	// second bank dished out in braze_main_read() below..
}

static void __fastcall braze_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc800:
			EEPROMWrite((data & 0x02), (data & 0x04), (data & 0x01));
		return;

		case 0xe000:
			braze_bankswitch(data);
		return;
	}

	dkong_main_write(address, data);
}

static UINT8 __fastcall braze_main_read(UINT16 address)
{
	// work-around for eeprom reading
	if (address & 0x8000) // 8000-ffff
	{
		if (address == 0xc800) {
			return (EEPROMRead() & 1);
		}

		return DrvZ80ROM[(braze_bank & 0x01) * 0x8000 + (address & 0x7fff)];
	}

	return dkong_main_read(address);
}

static const eeprom_interface braze_eeprom_intf =
{
	7,			// address bits
	8,			// data bits
	"*110",		// read command
	"*101",		// write command
	0,			// erase command
	"*10000xxxxx",	// lock command
	"*10011xxxxx",	// unlock command
	0,0
};

static void braze_decrypt_rom()
{
	UINT8 *tmp = BurnMalloc(0x10000);

	for (INT32 i = 0; i < 0x10000; i++) {
		tmp[(BITSWAP08(i >> 8, 7,2,3,1,0,6,4,5) << 8) | (i & 0xff)] = BITSWAP08(DrvZ80ROM[i], 1,4,5,7,6,0,3,2);
	}

	memcpy (DrvZ80ROM, tmp, 0x10000);

	BurnFree (tmp);
}


static void s2650_main_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("mw %4.4x, %2.2x\n"), address, data);

	if (address >= 0x2000) { // mirrors
		s2650Write(address & 0x1fff, data);
		return;
	}

	if ((address & 0xff80) == 0x1f00) {
		DrvSprRAM[0x400 + (address & 0x3ff)] = data;
		return;
	}

	if ((address & 0xfff0) == 0x1f80) {
		i8257Write(address,data);
		return;
	}

	switch (address)
	{
		case 0x1400:
			*soundlatch = data ^ 0x0f; // ?
		return;

		case 0x1500:
		case 0x1501:
		case 0x1502:
		case 0x1503:
		case 0x1504:
		case 0x1505:
		case 0x1506:
		case 0x1507:
		return;		// latch8_bit0_w

		case 0x1580:
			mcs48SetIRQLine(0, data ? 1 : 0);
		return;

		case 0x1582:
			*flipscreen = ~data & 0x01;
		return;

		case 0x1583:
			*sprite_bank = data & 0x01;
		return;

		case 0x1584:
		return; // nop

		case 0x1585:
			i8257_drq_write(0, data & 0x01);
			i8257_drq_write(1, data & 0x01);
			i8257_do_transfer(data & 0x01);
		return;

		case 0x1586:
		case 0x1587:
			if (data & 0x01) {
				*palette_bank |=  (1 << (address & 1));
			} else {
				*palette_bank &= ~(1 << (address & 1));
			}
		return;
	}
}

static UINT8 s2650_main_read(UINT16 address)
{
//	bprintf (0, _T("mr %4.4x\n"), address);

	if (address >= 0x2000) { // mirrors
		return s2650Read(address & 0x1fff);
	}

	if ((address & 0xff80) == 0x1f00) {
		return DrvSprRAM[0x400 + (address & 0x3ff)];
	}

	if ((address & 0xfff0) == 0x1f80) {
		return i8257Read(address);
	}

	if ((address & 0xfe80) == 0x1400) address &= ~0x7f; // mirrored

	switch (address)
	{
		case 0x1400:
			return DrvInputs[0];

		case 0x1480:
			return DrvInputs[1];

		case 0x1500:
			{
				UINT8 ret = DrvInputs[2] & 0xbf;
				if (ret & 0x10) ret = (ret & ~0x10) | 0x80;
				ret |= sndstatus << 6;
				return ret;
			}
			return 0;

		case 0x1580:
			return DrvDips[0];
	}

	return 0;
}

static void s2650_main_write_port(UINT16 port, UINT8 data)
{
//	bprintf (0, _T("pw %4.4x, %2.2x\n"), port, data);

	switch (port)
	{
		case 0x101:
			hunchloopback = data;
		return;

		case 0x0103:
		{
			main_fo = data;

			if (data) hunchloopback = 0xfb;
		}
		return;
	}
}

static UINT8 s2650_main_read_port(UINT16 port)
{
//	bprintf (0, _T("pr %4.4x\n"), port);

	switch (port)
	{
		case 0x00:
			switch (s2650_protection)
			{
				case 0x02:
			        	if (main_fo)
			        		return hunchloopback;
			        	else
			        		return hunchloopback--;

				default:
			        	if (!main_fo)
			        		return hunchloopback;
			        	else
			        		return hunchloopback--;
			}

		case 0x01:
			switch (s2650_protection)
			{
				case 0x01:
			        	if (hunchloopback & 0x80)
			        		return hunch_prot_ctr;
			        	else
			        		return ++hunch_prot_ctr;

				case 0x02:
					return hunchloopback--;
			}

		case S2650_SENSE_PORT:
			return vblank^0x80;
	}

	return 0;
}

static UINT8 i8039_sound_read_port(UINT32 port)
{
	if (port < 0x100) {
		if (radarscp1 || sndpage & 0x40) return *soundlatch;

		return DrvSndROM0[0x1000 + (sndpage & 7) * 0x100 + (port & 0xff)];
	}

	switch (port)
	{
		case MCS48_P1:
			if (radarscp1) {
				UINT8 ret = (m58817_status_read() << 6) & 0x40;
				ret |= (i8039_p[2] << 2) & 0x80;
				return ret;
			}
			return i8039_p[1];

		case MCS48_P2:
			return (radarscp1) ? 0x00 : i8039_p[2];

		case MCS48_T0:
			return i8039_t[0];

		case MCS48_T1:
			return i8039_t[1];
	}

	return 0xff;
}

static void dkong_dac_write(UINT8 data)
{
	DACWrite(0, (INT32)(data * exp(-envelope_ctr)));
	if (decay) {
		envelope_ctr += 0.001;
	} else {
		if (envelope_ctr>0.088) envelope_ctr -= 0.088; // bring decay back to 0 nicely to avoid clicks
		else if (envelope_ctr>0.001) envelope_ctr -= 0.001;
		else envelope_ctr = 0.0;
	}
}

static void i8039_sound_write_port(UINT32 port, UINT8 data)
{
	if (radarscp1 && port < 0x100) {
		dkong_dac_write(data);
		return;
	}

	switch (port)
	{
		case MCS48_P1:
			if (radarscp1) {
				tms5110_CTL_set(data & 0x0f);
				tms5110_PDC_set((data >> 4) & 0x01);
			} else {
				dkong_dac_write(data);
			}
		return;

		case MCS48_P2:
			decay = !(data & 0x80);
			sndpage = (data & 0x47);
			sndstatus = ((~data & 0x10) >> 4);
			sndgrid_en = (data & 0x20) >> 5;
		return;
	}
}

static void dkong3_sound0_write(UINT16 a, UINT8 d)
{
	if (a >= 0x4000 && a <= 0x4017) {
		nesapuWrite(0, a - 0x4000, d);
		return;
	}
}

static UINT8 dkong3_sound0_read(UINT16 a)
{
	switch (a) {
		case 0x4016: return soundlatch[0];
		case 0x4017: return soundlatch[1];
	}
	if (a >= 0x4000 && a <= 0x4015) {
		return nesapuRead(0, a - 0x4000);
	}

	return 0;
}

static void dkong3_sound1_write(UINT16 a, UINT8 d)
{
	if (a >= 0x4000 && a <= 0x4017) {
		nesapuWrite(1, a - 0x4000, d);
		return;
	}
}

static UINT8 dkong3_sound1_read(UINT16 a)
{
	if (a >= 0x4000 && a <= 0x4017) {
		if (a == 0x4016) return soundlatch[2];

		return nesapuRead(1, a - 0x4000);
	}

	return 0;
}

static void p8257ControlWrite(UINT16,UINT8 data)
{
	dma_latch = data;
}

static UINT8 p8257ControlRead(UINT16)
{
	return dma_latch;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	// before mcs48Reset()!
	memset(i8039_p, 0xff, 4);
	memset(i8039_t, 0x01, 4);
	decay = 0;
	sndpage = 0;
	sndstatus = 0;
	sndgrid_en = 0;

	mcs48Open(0);
	mcs48Reset();
	mcs48Close();
	dkongjr_walk = 0;
	dma_latch = 0;
	memset(sample_state, 0, sizeof(sample_state));
	sample_count = 0;
	climb_data = 0;
	envelope_ctr = 0;
	decrypt_counter = 0x09;
	*soundlatch = 0x0f;

	// radar scope
	sig30Hz = 0;
	lfsr_5I = 0;
	grid_sig = 0;
	rflip_sig = 0;
	star_ff = 0;
	blue_level = 0;
	cv1 = 0;
	cv2 = 0;
	cv3 = 0;
	cv4 = 0;
	vg1 = 0;
	vg2 = 0;
	vg3 = 0;
	vc17 = 0;
	pixelcnt = 0;

	if (brazemode) {
		ZetOpen(0);
		braze_bankswitch(0);
		ZetClose();
	}

	BurnSampleReset();
	DACReset();
	if (radarscp1) {
		tms5110_reset();
	}

	i8257Reset();

	EEPROMReset();

	HiscoreReset();

	nExtraCycles[0] = nExtraCycles[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv2650ROM		= Next;
	DrvZ80ROM		= Next; Next += 0x020000;
	DrvSndROM0		= Next; Next += 0x002000;
	DrvSndROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x000800;
	DrvGfxROM3		= Next; Next += 0x000100;

	DrvColPROM		= Next; Next += 0x000400;
	DrvMapROM		= Next; Next += 0x000200; // for s2650 sets

	DrvRevMap		= (INT32*)Next; Next += 0x000200 * sizeof(INT32);

	DrvPalette		= (UINT32*)Next; Next += 0x0209 * sizeof(UINT32);

	AllRam			= Next;

	Drv2650RAM		= Next;
	DrvZ80RAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x000b00;
	DrvVidRAM		= Next; Next += 0x000400;

	DrvSndRAM0		= Next; Next += 0x000200;
	DrvSndRAM1		= Next; Next += 0x000200;

	soundlatch		= Next; Next += 0x000005;
	gfx_bank		= Next; Next += 0x000001;
	sprite_bank		= Next; Next += 0x000001;
	palette_bank	= Next; Next += 0x000001;
	flipscreen		= Next; Next += 0x000001;
	nmi_mask		= Next; Next += 0x000001;

	grid_color		= Next; Next += 0x000001;
	grid_enable		= Next; Next += 0x000001;

	i8039_t         = Next; Next += 0x000004;
	i8039_p         = Next; Next += 0x000004;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static const res_net_decode_info dkong_decode_info = {
	2, 0, 255,
	{ 256,  256,    0,    0,    0,    0},
	{   1,   -2,    0,    0,    2,    0},
	{0x07, 0x04, 0x03, 0x00, 0x03, 0x00}
};

static const res_net_info dkong_net_info = {
	RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_MB7052 | RES_NET_MONITOR_SANYO_EZV20,
	{ { RES_NET_AMP_DARLINGTON, 470, 0, 3, { 1000, 470, 220 } },
	  { RES_NET_AMP_DARLINGTON, 470, 0, 3, { 1000, 470, 220 } },
	  { RES_NET_AMP_EMITTER,    680, 0, 2, {  470, 220,   0 } } }
};

static const res_net_info dkong_net_bg_info = {
	RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_MB7052 | RES_NET_MONITOR_SANYO_EZV20,
	{ { RES_NET_AMP_DARLINGTON, 470, 0, 0, { 0 } },
	  { RES_NET_AMP_DARLINGTON, 470, 0, 0, { 0 } },
	  { RES_NET_AMP_EMITTER,    680, 0, 0, { 0 } } }
};

static const res_net_info radarscp_net_info = {
	RES_NET_VCC_5V | RES_NET_VBIAS_TTL | RES_NET_VIN_MB7052 | RES_NET_MONITOR_SANYO_EZV20,
	{ { RES_NET_AMP_DARLINGTON, 470 * TRS_J1, 470*(1-TRS_J1), 3, { 1000, 470, 220 } },
	  { RES_NET_AMP_DARLINGTON, 470 * TRS_J1, 470*(1-TRS_J1), 3, { 1000, 470, 220 } },
	  { RES_NET_AMP_EMITTER,    680 * TRS_J1, 680*(1-TRS_J1), 2, {  470, 220,   0 } } }
};

static const res_net_info radarscp_net_bck_info = {
	RES_NET_VCC_5V | RES_NET_VBIAS_TTL | RES_NET_VIN_MB7052 | RES_NET_MONITOR_SANYO_EZV20,
	{ { RES_NET_AMP_DARLINGTON, 470, 4700, 0, { 0 } },
	  { RES_NET_AMP_DARLINGTON, 470, 4700, 0, { 0 } },
	  { RES_NET_AMP_EMITTER,    470, 4700, 0, { 0 } } }
};

static const res_net_info radarscp_stars_net_info = {
	RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_TTL_OUT | RES_NET_MONITOR_SANYO_EZV20,
	{ { RES_NET_AMP_DARLINGTON, 4700, 470, 0, { 0 } },
	  { RES_NET_AMP_DARLINGTON,    1,   0, 0, { 0 } },
	  { RES_NET_AMP_EMITTER,       1,   0, 0, { 0 } } }
};

static const res_net_info radarscp_blue_net_info = {
	RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_VCC | RES_NET_MONITOR_SANYO_EZV20,
	{ { RES_NET_AMP_DARLINGTON,  470, 4700, 0, { 0 } },
	  { RES_NET_AMP_DARLINGTON,  470, 4700, 0, { 0 } },
	  { RES_NET_AMP_EMITTER,       0,    0, 8, { 128,64,32,16,8,4,2,1 } } }
};

static const res_net_info radarscp_grid_net_info = {
	RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_TTL_OUT | RES_NET_MONITOR_SANYO_EZV20,
	{ { RES_NET_AMP_DARLINGTON,    0,   0, 1, { 1 } },
	  { RES_NET_AMP_DARLINGTON,    0,   0, 1, { 1 } },
	  { RES_NET_AMP_EMITTER,       0,   0, 1, { 1 } } }
};

static const res_net_info radarscp1_net_info = {
	RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_TTL_OUT | RES_NET_MONITOR_SANYO_EZV20,
	{ { RES_NET_AMP_DARLINGTON, 0,      0, 4, { 39000, 20000, 10000, 4990 } },
	  { RES_NET_AMP_DARLINGTON, 0,      0, 4, { 39000, 20000, 10000, 4990 } },
	  { RES_NET_AMP_EMITTER,    0,      0, 4, { 39000, 20000, 10000, 4990 } } }
};

static const res_net_decode_info dkong3_decode_info = {
	1, 0, 255,
	{   0,   0, 256 },
	{   4,   0,   0 },
	{0x0F,0x0F,0x0F }
};

static const res_net_info dkong3_net_info = {
	RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_MB7052 | RES_NET_MONITOR_SANYO_EZV20,
	{ { RES_NET_AMP_DARLINGTON, 470,      0, 4, { 2200, 1000, 470, 220 } },
	  { RES_NET_AMP_DARLINGTON, 470,      0, 4, { 2200, 1000, 470, 220 } },
	  { RES_NET_AMP_DARLINGTON, 470,      0, 4, { 2200, 1000, 470, 220 } } }
};

static void dkongNewPaletteInit()
{
	compute_res_net_all(DrvPalette, DrvColPROM, dkong_decode_info, dkong_net_info);

	for (INT32 i = 0; i < 256; i++) {
		if (!(i & 0x03)) {
			INT32 r = compute_res_net(1, 0, dkong_net_bg_info);
			INT32 g = compute_res_net(1, 1, dkong_net_bg_info);
			INT32 b = compute_res_net(1, 2, dkong_net_bg_info);

			DrvPalette[i] = BurnHighCol(r, g, b, 0);
		}
	}
}

static void radarscpPaletteInit()
{
	for (INT32 i = 0; i < 256; i++)
	{
		INT32 r = compute_res_net((DrvColPROM[i+256] >> 1) & 0x07, 0, radarscp_net_info);
		INT32 g = compute_res_net(((DrvColPROM[i+256] << 2) & 0x04) | ((DrvColPROM[i] >> 2) & 0x03), 1, radarscp_net_info);
		INT32 b = compute_res_net((DrvColPROM[i] >> 0) & 0x03, 2, radarscp_net_info);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 i = 0; i < 256; i++) {
		if ((palette_type != 0x00) && !(i & 0x03)) {
			INT32 r = compute_res_net(1, 0, radarscp_net_bck_info);
			INT32 g = compute_res_net(1, 1, radarscp_net_bck_info);
			INT32 b = compute_res_net(1, 2, radarscp_net_bck_info);

			DrvPalette[i] = BurnHighCol(r, g, b, 0);
		}
	}

	DrvPalette[RADARSCP_STAR_COL] = BurnHighCol(
		compute_res_net(1, 0, radarscp_stars_net_info),
		compute_res_net(0, 1, radarscp_stars_net_info),
		compute_res_net(0, 2, radarscp_stars_net_info),
		0
	);

	/* note: Oscillating background isn't implemented yet */
	for (INT32 i = 0; i < 256; i++)
	{
		INT32 r = compute_res_net(0, 0, radarscp_blue_net_info);
		INT32 g = compute_res_net(0, 1, radarscp_blue_net_info);
		INT32 b = compute_res_net(i, 2, radarscp_blue_net_info);

		DrvPalette[RADARSCP_BCK_COL_OFFSET + i] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 i = 0; i < 8; i++)
	{
		INT32 r = compute_res_net(BIT(i, 0), 0, radarscp_grid_net_info);
		INT32 g = compute_res_net(BIT(i, 1), 1, radarscp_grid_net_info);
		INT32 b = compute_res_net(BIT(i, 2), 2, radarscp_grid_net_info);

		DrvPalette[RADARSCP_GRID_COL_OFFSET + i] = BurnHighCol(r, g, b, 0);
	}
}

static void radarscp1PaletteInit()
{
	for (INT32 i = 0; i < 256; i++)
	{
		INT32 r = compute_res_net(DrvColPROM[i+512], 0, radarscp1_net_info);
		INT32 g = compute_res_net(DrvColPROM[i+256], 1, radarscp1_net_info);
		INT32 b = compute_res_net(DrvColPROM[i], 2, radarscp1_net_info);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 i = 0; i < 256; i++) {
		if (!(i & 0x03)) {
			INT32 r = compute_res_net(0, 0, radarscp1_net_info);
			INT32 g = compute_res_net(0, 1, radarscp1_net_info);
			INT32 b = compute_res_net(0, 2, radarscp1_net_info);

			DrvPalette[i] = BurnHighCol(r, g, b, 0);
		}
	}

	DrvPalette[RADARSCP_STAR_COL] = BurnHighCol(
		compute_res_net(1, 0, radarscp_stars_net_info),
		compute_res_net(0, 1, radarscp_stars_net_info),
		compute_res_net(0, 2, radarscp_stars_net_info),
		0
	);

	/* note: Oscillating background isn't implemented yet */
	for (INT32 i = 0; i < 256; i++)
	{
		INT32 r = compute_res_net(0, 0, radarscp_blue_net_info);
		INT32 g = compute_res_net(0, 1, radarscp_blue_net_info);
		INT32 b = compute_res_net(i, 2, radarscp_blue_net_info);

		DrvPalette[RADARSCP_BCK_COL_OFFSET + i] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 i = 0; i < 8; i++)
	{
		INT32 r = compute_res_net(BIT(i, 0), 0, radarscp_grid_net_info);
		INT32 g = compute_res_net(BIT(i, 1), 1, radarscp_grid_net_info);
		INT32 b = compute_res_net(BIT(i, 2), 2, radarscp_grid_net_info);

		DrvPalette[RADARSCP_GRID_COL_OFFSET + i] = BurnHighCol(r, g, b, 0);
	}
}

static void dkong3NewPaletteInit()
{
	compute_res_net_all(DrvPalette, DrvColPROM, dkong3_decode_info, dkong3_net_info);
}

static void dkongPaletteInit()
{
	for (INT32 i = 0; i < 256; i++)
	{
		INT32 bit0 = (DrvColPROM[i+256] >> 1) & 1;
		INT32 bit1 = (DrvColPROM[i+256] >> 2) & 1;
		INT32 bit2 = (DrvColPROM[i+256] >> 3) & 1;
		INT32 r = 255 - (0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2);

		bit0 = (DrvColPROM[i+0] >> 2) & 1;
		bit1 = (DrvColPROM[i+0] >> 3) & 1;
		bit2 = (DrvColPROM[i+256] >> 0) & 1;
		INT32 g = 255 - (0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2);

		bit0 = (DrvColPROM[i+0] >> 0) & 1;
		bit1 = (DrvColPROM[i+0] >> 1) & 1;
		INT32 b = 255 - (0x55 * bit0 + 0xaa * bit1);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);

		// used by radarscp
		DrvPalette[RADARSCP_BCK_COL_OFFSET + i] = BurnHighCol(0, 0, 0, 0); // black
	}

	// used by radarscp
	DrvPalette[RADARSCP_STAR_COL]        = BurnHighCol(0xff, 0, 0, 0); // red
	for (INT32 i = 0; i < 8; i++)
		DrvPalette[RADARSCP_GRID_COL_OFFSET + i] = BurnHighCol(0, 0, 0xff, 0); // blue
}

static void dkong3PaletteInit()
{
	for (INT32 i = 0; i < 256; i++)
	{
		INT32 bit0 = (DrvColPROM[i+0] >> 4) & 0x01;
		INT32 bit1 = (DrvColPROM[i+0] >> 5) & 0x01;
		INT32 bit2 = (DrvColPROM[i+0] >> 6) & 0x01;
		INT32 bit3 = (DrvColPROM[i+0] >> 7) & 0x01;
		INT32 r = 255 - (0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3);

		bit0 = (DrvColPROM[i+0] >> 0) & 0x01;
		bit1 = (DrvColPROM[i+0] >> 1) & 0x01;
		bit2 = (DrvColPROM[i+0] >> 2) & 0x01;
		bit3 = (DrvColPROM[i+0] >> 3) & 0x01;
		INT32 g = 255 - (0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3);

		bit0 = (DrvColPROM[i+256] >> 0) & 0x01;
		bit1 = (DrvColPROM[i+256] >> 1) & 0x01;
		bit2 = (DrvColPROM[i+256] >> 2) & 0x01;
		bit3 = (DrvColPROM[i+256] >> 3) & 0x01;
		INT32 b = 255 - (0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void update_palette_type(INT32 dip_offset)
{
	if (palette_type != (DrvDips[dip_offset]))
	{
		DrvRecalc = 1;
		palette_type = DrvDips[dip_offset];
		switch (palette_type)
		{
			case 0x00: DrvPaletteUpdate = radarscpPaletteInit;  break; // radarscpPaletteInit in conversion mode
			case 0x01: DrvPaletteUpdate = dkongNewPaletteInit;  break;
			case 0x02: DrvPaletteUpdate = dkongPaletteInit;     break;
			case 0x03: DrvPaletteUpdate = radarscpPaletteInit;  break; // radarscpPaletteInit in normal mode
			case 0x04: DrvPaletteUpdate = radarscp1PaletteInit; break;
			case 0x05: DrvPaletteUpdate = dkong3NewPaletteInit; break;
			case 0x06: DrvPaletteUpdate = dkong3PaletteInit;    break;
		}
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 0x2000/2*8, 0 };
	INT32 Plane1[2]  = { 0x4000/2*8, 0 };
	INT32 XOffs0[16] = { STEP8(0,1), STEP8((0x2000/4)*8,1) };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8((0x4000/4)*8,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane0, XOffs0, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x0100, 2, 16, 16, Plane1, XOffs1, YOffs, 0x080, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static ior_in_functs dkong_dma_read_functions[4] = { NULL, p8257ControlRead, NULL, NULL };
static ior_out_functs dkong_dma_write_functions[4] = { p8257ControlWrite, NULL, NULL, NULL };

static INT32 DrvInit(INT32 (*pRomLoadCallback)(), UINT32 map_flags)
{
	BurnAllocMemIndex();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM, 0x0000, 0x5fff, MAP_ROM);

	if (map_flags & 2) { // hack
		ZetMapMemory(DrvZ80RAM, 0x6000, 0x68ff, MAP_RAM);
		ZetMapMemory(DrvSprRAM, 0x6900, 0x73ff, MAP_RAM); // 900-a7f
	} else {
		ZetMapMemory(DrvZ80RAM, 0x6000, 0x6fff, MAP_RAM);
		ZetMapMemory(DrvSprRAM, 0x7000, 0x73ff, MAP_RAM);
	}

	ZetMapMemory(DrvVidRAM, 0x7400, 0x77ff, MAP_RAM);

	if (map_flags & 1) {
		ZetMapMemory(DrvZ80ROM + 0x8000, 0x8000, 0xffff, MAP_ROM);
	}

	ZetSetWriteHandler(dkong_main_write);
	ZetSetReadHandler(dkong_main_read);
	ZetClose();

	mcs48Init(0, 8884, DrvSndROM0);
	mcs48Open(0);
	mcs48_set_read_port(i8039_sound_read_port);
	mcs48_set_write_port(i8039_sound_write_port);
	mcs48Close();

	DACInit(0, 0, 0, mcs48TotalCycles, 6000000 / 15);
	DACSetRoute(0, 0.70, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);

	if (radarscp1) {
		tms5110_init(640000, DrvSndROM1);
		tms5110_set_variant(TMS5110_IS_M58817);
		tms5110_set_buffered(mcs48TotalCycles, 6000000 / 15);
	}

	biqdac.init(FILT_LOWPASS, nBurnSoundRate, 2000, 0.8, 0);
	biqdac2.init(FILT_LOWPASS, nBurnSoundRate, 2000, 0.8, 0);

	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.20, BURN_SND_ROUTE_BOTH);
	BurnSampleSetBuffered(ZetTotalCycles, 3072000);

	i8257Init();
	i8257Config(ZetReadByte, ZetWriteByte, ZetIdle, dkong_dma_read_functions, dkong_dma_write_functions);

	EEPROMInit(&braze_eeprom_intf);

	{
		if (pRomLoadCallback) {
			if (pRomLoadCallback()) return 1;
		}

		update_palette_type(1);
		DrvPaletteUpdate();

		DrvGfxDecode();
	}

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	mcs48Exit();
	i8257Exit();

	BurnSampleExit();
	DACExit();
	biqdac.exit();
	biqdac2.exit();

	if (radarscp1) {
		tms5110_exit();
	}

	EEPROMExit();

	BurnFree(AllMem);

	radarscp = 0;
	radarscp1 = 0;
	brazemode = 0;
	draktonmode = 0;
	palette_type = -1;

	return 0;
}

static INT32 Dkong3DoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	M6502Close();

	nesapuReset();

	sound_cpu_in_reset = 0;

	return 0;
}

static UINT32 dkong3_nesapu_sync(INT32 samples_rate)
{
	return (samples_rate * M6502TotalCycles()) / 29830 /* 1789773 / 60 */;
}

static INT32 Dkong3Init()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x8000,  3, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000,  7, 1)) return 1;

		memcpy (DrvGfxROM0 + 0x0000, DrvGfxROM1 + 0x0800, 0x0800);
		memcpy (DrvGfxROM0 + 0x0800, DrvGfxROM1 + 0x0000, 0x0800);
		memcpy (DrvGfxROM0 + 0x1000, DrvGfxROM1 + 0x1800, 0x0800);
		memcpy (DrvGfxROM0 + 0x1800, DrvGfxROM1 + 0x1000, 0x0800);

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x3000, 11, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0200, 14, 1)) return 1;

		update_palette_type(2);
		DrvPaletteUpdate();
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM, 0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM, 0x6000, 0x68ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM, 0x6900, 0x73ff, MAP_RAM); // 900-a7f
	ZetMapMemory(DrvVidRAM, 0x7400, 0x77ff, MAP_RAM);

	ZetMapMemory(DrvZ80ROM + 0x8000, 0x8000, 0xffff, MAP_ROM);
	ZetSetWriteHandler(dkong3_main_write);
	ZetSetReadHandler(dkong3_main_read);
	ZetClose();

	M6502Init(0, TYPE_N2A03);
	M6502Open(0);
	M6502MapMemory(DrvSndRAM0, 0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvSndROM0, 0xe000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(dkong3_sound0_write);
	M6502SetReadHandler(dkong3_sound0_read);
	M6502Close();

	M6502Init(1, TYPE_N2A03);
	M6502Open(1);
	M6502MapMemory(DrvSndRAM1, 0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvSndROM1, 0xe000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(dkong3_sound1_write);
	M6502SetReadHandler(dkong3_sound1_read);
	M6502Close();

	nesapuInit(0, 1789773, 0, dkong3_nesapu_sync, 0);
	nesapuSetAllRoutes(0, 0.95, BURN_SND_ROUTE_BOTH);
	nesapuSetArcade(1);

	nesapuInit(1, 1789773, 0, dkong3_nesapu_sync, 1);
	nesapuSetAllRoutes(1, 0.95, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	Dkong3DoReset();

	return 0;
}

static INT32 Dkong3Exit()
{
	GenericTilesExit();

	ZetExit();
	M6502Exit();
	nesapuExit();

	BurnFree(AllMem);

	palette_type = -1;

	return 0;
}

static INT32 s2650DkongDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	s2650Open(0);
	s2650Reset();
	s2650Close();

	mcs48Open(0);
	mcs48Reset();
	mcs48Close();

	BurnSampleReset();
	DACReset();

	i8257Reset();

	hunchloopback = 0;

	return 0;
}

static UINT8 hb_dma_read_byte(UINT16 offset)
{
	offset = ((DrvRevMap[(offset >> 10) & 0x1ff] << 7) & 0x7c00) | (offset & 0x3ff);

	return s2650Read(offset);
}

static void hb_dma_write_byte(UINT16 offset, UINT8 data)
{
	offset = ((DrvRevMap[(offset >> 10) & 0x1ff] << 7) & 0x7c00) | (offset & 0x3ff);

	s2650Write(offset, data);
}

static INT32 s2650_irq_callback(INT32)
{
	return 0x03;
}

static void s2650RevMapConvert()
{
	for (INT32 i = 0; i < 0x200; i++) {
		DrvRevMap[DrvMapROM[i]] = i;
	}
}

static INT32 s2650DkongInit(INT32 (*pRomLoadCallback)())
{
	BurnAllocMemIndex();

	{
		if (pRomLoadCallback) {
			if (pRomLoadCallback()) return 1;
		}

		update_palette_type(1);

		s2650RevMapConvert();
		DrvGfxDecode();
	}

	s2650Init(1);
	s2650Open(0);
	s2650MapMemory(Drv2650ROM + 0x0000, 0x0000, 0x0fff, MAP_ROM);
	s2650MapMemory(Drv2650RAM + 0x0000, 0x1000, 0x13ff, MAP_RAM); // sprite ram (after dma)
	s2650MapMemory(DrvSprRAM  + 0x0000, 0x1600, 0x17ff, MAP_RAM);
	s2650MapMemory(DrvVidRAM  + 0x0000, 0x1800, 0x1bff, MAP_RAM);
	s2650MapMemory(DrvSprRAM  + 0x0400, 0x1c00, 0x1eff, MAP_RAM);
	s2650MapMemory(Drv2650ROM + 0x2000, 0x2000, 0x2fff, MAP_ROM);
	s2650MapMemory(Drv2650ROM + 0x4000, 0x4000, 0x4fff, MAP_ROM);
	s2650MapMemory(Drv2650ROM + 0x6000, 0x6000, 0x6fff, MAP_ROM);
	s2650SetIrqCallback(s2650_irq_callback);
	s2650SetWriteHandler(s2650_main_write);
	s2650SetReadHandler(s2650_main_read);
	s2650SetOutHandler(s2650_main_write_port);
	s2650SetInHandler(s2650_main_read_port);
	s2650Close();

	mcs48Init(0, 8884, DrvSndROM0);
	mcs48Open(0);
	mcs48_set_read_port(i8039_sound_read_port);
	mcs48_set_write_port(i8039_sound_write_port);
	mcs48Close();

	DACInit(0, 0, 0, mcs48TotalCycles, 6000000 / 15);
	DACSetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);

	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.75, BURN_SND_ROUTE_BOTH);

	i8257Init();
	i8257Config(hb_dma_read_byte, hb_dma_write_byte, /*s2650Idle*/NULL, dkong_dma_read_functions, dkong_dma_write_functions);

	GenericTilesInit();

	s2650DkongDoReset();

	return 0;
}

static INT32 s2650DkongExit()
{
	GenericTilesExit();

	s2650Exit();
	mcs48Exit();
	i8257Exit();

	BurnSampleExit();
	DACExit();

	BurnFreeMemIndex();

	s2650_protection = 0;
	palette_type = -1;

	return 0;
}

inline double CD4049(double x)
{
	if (x>0)
		return exp(-cd4049_a * pow(x,cd4049_b));
	else
		return 1.0;
}

static void radarscp_step(INT32 line_cnt)
{
	/* Condensator is illegible in schematics for TRS2 board.
	 * TRS1 board states 3.3u.
	 */

	double vg3i;
	double diff;
	INT32 sig;

	/* vsync is divided by 2 by a LS161
	 * The resulting 30 Hz signal clocks a LFSR (LS164) operating as a
	 * random number generator.
	 */

	if ( line_cnt == 0)
	{
		sig30Hz = (1-sig30Hz);
		if (sig30Hz)
			lfsr_5I = (rand() > RAND_MAX/2);
	}

	/* sound2 mixes in a 30Hz noise signal.
	 * With the current model this has no real effect
	 * Included for completeness
	 */

	/* Now mix with SND02 (sound 2) line - on 74ls259, bit2 */
	rflip_sig = sample_state[2] & lfsr_5I;

	/* blue background generation */

	line_cnt += (256 - 8) + 1; // offset 8 needed to match monitor pictures
	if (line_cnt>511)
		line_cnt -= 264;

	sig = rflip_sig ^ ((line_cnt & 0x80)>>7);

	if (radarscp1)
		rflip_sig = !rflip_sig;

	if  (sig) /*  128VF */
		diff = (0.0 - cv1);
	else
		diff = (4.8 - cv1);
	diff = diff - diff*exp(0.0 - (1.0/RC1 * dt) );
	cv1 += diff;

	diff = (cv1 - cv2 - vg1);
	diff = diff - diff*exp(0.0 - (1.0/RC2 * dt) );
	cv2 += diff;

	// FIXME: use the inverse function
	// Solve the amplifier by iteration
	for (INT32 j=1; j<=11; j++)// 11% = 1/75 / (1/75+1/10)
	{
		double f = (double) j / 100.0;
		vg1 = (cv1 - cv2)*(1-f) + f * vg2;
		vg2 = 5*CD4049(vg1/5);
	}
	// FIXME: use the inverse function
	// Solve the amplifier by iteration 50% = both resistors equal
	for (INT32 j=10; j<=20; j++)
	{
		double f = (double) j / 40.0;
		vg3i = (1.0-f) * vg2 + f * vg3;
		vg3 = 5*CD4049(vg3i/5);
	}

	diff = (vg3 - vc17);
	diff = diff - diff*exp(0.0 - (1.0/RC17 * dt) );
	vc17 += diff;

	double vo = (vg3 - vc17);
	vo = vo + 20.0 / (20.0+10.0) * 5;

	blue_level = (INT32)(vo/5.0*255);

	/*
	 * Grid signal
	 */
	if (*grid_enable && sndgrid_en)
	{
		diff = (0.0 - cv3);
		diff = diff - diff*exp(0.0 - (1.0/RC32 * dt) );
	}
	else
	{
		diff = (5.0 - cv3);
		diff = diff - diff*exp(0.0 - (1.0/RC31 * dt) );
	}
	cv3 += diff;

	diff = (vg2 - 0.8 * cv3 - cv4);
	diff = diff - diff*exp(0.0 - (1.0/RC4 * dt) );
	cv4 += diff;

	if (CD4049(CD4049((vg2 - cv4)/5.0))>2.4/5.0) /* TTL - Level */
		grid_sig = 0;
	else
		grid_sig = 1;

	/* stars */
	pixelcnt += 384;
	if (pixelcnt > period2 )
	{
		star_ff = !star_ff;
		pixelcnt = pixelcnt - period2;
	}
}

static void radarscp_draw_background()
{
	const UINT8 *table  = DrvGfxROM2;
	const UINT8 *htable = DrvGfxROM3;

	INT32 counter = 0;
	INT32 scanline = 0;

	while (scanline < 264)
	{
		INT32 y = 239-scanline;
		radarscp_step(scanline);
		if (scanline <= 16 || scanline >= 240)
			counter = 0;
		INT32 offset = (*flipscreen ^ rflip_sig) ? 0x000 : 0x400;
		INT32 col = 0;
		while (pBurnDraw && col < 384)
		{
			INT32 x = 255-col;
			UINT8 draw_ok = !(pTransDraw[(y) * nScreenWidth + x] & 0x01) && !(pTransDraw[(y) * nScreenWidth + x] & 0x02) && (y >= 0 && y < nScreenHeight && x >= 0 && x < nScreenWidth);
			if (radarscp1) /*  Check again from schematics */
				draw_ok = draw_ok  && !((htable[ (!rflip_sig<<7) | (col>>2)] >>2) & 0x01);
			if ((counter < 0x800) && (col == 4 * (table[counter|offset] & 0x7f)))
			{
				if ( star_ff && (table[counter|offset] & 0x80) && draw_ok)    /* star */
					pTransDraw[(y) * nScreenWidth + x] = RADARSCP_STAR_COL;
				else if (grid_sig && !(table[counter|offset] & 0x80) && draw_ok)           /* radar */
					pTransDraw[(y) * nScreenWidth + x] = (RADARSCP_GRID_COL_OFFSET + *grid_color);
				else if (draw_ok)
					pTransDraw[(y) * nScreenWidth + x] = RADARSCP_BCK_COL_OFFSET + blue_level;
				counter++;
			}
			else if (draw_ok)
				pTransDraw[(y) * nScreenWidth + x] = RADARSCP_BCK_COL_OFFSET + blue_level;
			col++;
		}
		while ((counter < 0x800) && (col < 4 * (table[counter|offset] & 0x7f)))
			counter++;
		scanline++;
	}
}

static void draw_sprites(UINT32 code_mask, UINT32 mask_bank, UINT32 shift_bits, UINT32 swap)
{
	INT32 bank = *sprite_bank * 0x200;
	INT32 yoffset = (swap) ? -15 : -16;

	for (INT32 offs = bank; offs < bank + 0x200; offs += 4)
	{
		if (DrvSprRAM[offs + 0])
		{
			INT32 sx    = DrvSprRAM[offs + 3] - 8;
			INT32 attr  = DrvSprRAM[offs + ((swap) ? 1 : 2)];
			INT32 code  = (DrvSprRAM[offs + ((swap) ? 2 : 1)] & code_mask) + ((attr & mask_bank) << shift_bits);
			INT32 sy    = (240 - DrvSprRAM[offs + 0] + 7) + yoffset;
			INT32 color = (attr & 0x0f) + (*palette_bank * 0x10);
			INT32 flipx = attr & 0x80;
			INT32 flipy = DrvSprRAM[offs + 1] & ((swap) ? 0x40 : 0x80);

			Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 2, 0, 0, DrvGfxROM1);

			// wraparound
			if (sx < 0) {
				Draw16x16MaskTile(pTransDraw, code, sx+256, sy, flipx, flipy, color, 2, 0, 0, DrvGfxROM1);
			}
		}
	}
}

static void draw_layer()
{
	for (INT32 offs = (32 * 2); offs < (32 * 32) - (32 * 2); offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 code = DrvVidRAM[offs] + (*gfx_bank * 256);
		INT32 color;
		if (radarscp1)
			color = (DrvColPROM[0x300 + (offs & 0x1f)] & 0x0f) | (*palette_bank<<4);
		else
			color = (DrvColPROM[0x200 + (offs & 0x1f) + ((offs / 0x80) * 0x20)] & 0x0f) + (*palette_bank * 0x10);

		Draw8x8Tile(pTransDraw, code, sx, sy - 16, 0, 0, color, 2, 0, DrvGfxROM0);
	}
}

static INT32 dkongDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	BurnTransferClear();
	if (nBurnLayer & 1) draw_layer();
	if (nSpriteEnable & 1) draw_sprites(0x7f, 0x40, 1, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 radarscpDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	BurnTransferClear();
	if (nBurnLayer & 1) draw_layer();
	if (nSpriteEnable & 1) draw_sprites(0x7f, 0x40, 1, 0);
	if (nBurnLayer & 2) radarscp_draw_background();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 pestplceDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	BurnTransferClear();
	if (nBurnLayer & 1) draw_layer();
	if (nSpriteEnable & 1) draw_sprites(0xff, 0x00, 0, 1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	mcs48NewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[2] = { (INT32)((double)3072000 / 60.606061), (INT32)((double)6000000 / 15 / 60.606061) };
	INT32 nCyclesDone[2] = { nExtraCycles[0], nExtraCycles[1] };

	ZetOpen(0);
	mcs48Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN_SYNCINT(0, Zet);
		CPU_RUN(1, mcs48);

		if (i == 239 && *nmi_mask) ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		if (i == 240) ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
	}

	if (pBurnSoundOut) {
		DACUpdate(pBurnSoundOut, nBurnSoundLen);

		if (radarscp) {
			biqdac.filter_buffer_2x_mono(pBurnSoundOut, nBurnSoundLen);
			biqdac2.filter_buffer_2x_mono(pBurnSoundOut, nBurnSoundLen);
		}

		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();

		if (radarscp1) {
			tms5110_update(pBurnSoundOut, nBurnSoundLen);
		}
	}

	nExtraCycles[0] = ZetTotalCycles() - nCyclesTotal[0]; // ZetTotalCycles() because _SYNCINT and calling ZetIdle() by the dma engine.
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	mcs48Close();
	ZetClose();

	if (pBurnDraw) {
		update_palette_type(1);
		BurnDrvRedraw();
	} else if (radarscp) {
		radarscp_draw_background(); // calculate the background stuff w/ffwd/frameskiop frames (etc)
	}

	return 0;
}

static INT32 Dkong3Frame()
{
	if (DrvReset) {
		Dkong3DoReset();
	}

	M6502NewFrame();

	{
		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 400; // ?
	INT32 nCyclesTotal[3] = { 4000000 / 60, 1789773 / 60, 1789773 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1) && *nmi_mask) ZetNmi();

		M6502Open(0);
		CPU_RUN(1, M6502);
		if (i == (nInterleave - 1)) M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
		M6502Close();

		M6502Open(1);
		CPU_RUN(2, M6502);
		if (i == (nInterleave - 1)) M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
		M6502Close();
	}

	ZetClose();

	if (pBurnSoundOut) {
		nesapuUpdate(0, pBurnSoundOut, nBurnSoundLen);
		nesapuUpdate(1, pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();
	}

	if (pBurnDraw) {
		update_palette_type(2);
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 s2650DkongFrame()
{
	if (DrvReset) {
		s2650DkongDoReset();
	}

	mcs48NewFrame();

	{
		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 32;
	INT32 nCyclesTotal[2] = { 3072000 / 2 / 60, 6000000 / 15 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	s2650Open(0);
	mcs48Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, s2650);
		CPU_RUN(1, mcs48);

		if (i == 30) {
			vblank = 0x80;

			s2650SetIRQLine(0, CPU_IRQSTATUS_ACK);
			s2650Run(10);
			s2650SetIRQLine(0, CPU_IRQSTATUS_NONE);
		}
	}

	if (pBurnSoundOut) {
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
	}

	mcs48Close();
	s2650Close();

	if (pBurnDraw) {
		update_palette_type(1);
		BurnDrvRedraw();
	}

	return 0;
}



// Radar Scope (TRS02, rev. D)

static struct BurnRomInfo radarscpRomDesc[] = {
	{ "trs2c5fd",	0x1000, 0x80bbcbb3, 1 }, //  0 maincpu
	{ "trs2c5gd",	0x1000, 0xafa8c49f, 1 }, //  1
	{ "trs2c5hd",	0x1000, 0xe3ad4239, 1 }, //  2
	{ "trs2c5kd",	0x1000, 0x260a3ec4, 1 }, //  3

	{ "trs2s3i",	0x0800, 0x78034f14, 2 }, //  4 soundcpu

	{ "trs2v3gc",	0x0800, 0xf095330e, 3 }, //  5 gfx1
	{ "trs2v3hc",	0x0800, 0x15a316f0, 3 }, //  6

	{ "trs2v3dc",	0x0800, 0xe0bb0db9, 4 }, //  7 gfx2
	{ "trs2v3cc",	0x0800, 0x6c4e7dad, 4 }, //  8
	{ "trs2v3bc",	0x0800, 0x6fdd63f1, 4 }, //  9
	{ "trs2v3ac",	0x0800, 0xbbf62755, 4 }, // 10

	{ "rs2-x.xxx",	0x0100, 0x54609d61, 6 }, // 11 proms
	{ "rs2-c.xxx",	0x0100, 0x79a7d831, 6 }, // 12
	{ "rs2-v.1hc",	0x0100, 0x1b828315, 6 }, // 13

	{ "trs2v3ec",	0x0800, 0x0eca8d6b, 5 }, // 14 gfx3
};

STD_ROM_PICK(radarscp)
STD_ROM_FN(radarscp)

static INT32 radarscpRomLoad()
{
	if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x3000,  3, 1)) return 1;

	if (BurnLoadRom(DrvSndROM0  + 0x0000,  4, 1)) return 1;
	memcpy (DrvSndROM0 + 0x0800, DrvSndROM0 + 0x0000, 0x0800); // re-load

	if (BurnLoadRom(DrvGfxROM0 + 0x0000,  5, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x1000,  6, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x0000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x2000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x3000, 10, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x0000, 11, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0100, 12, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0200, 13, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM2 + 0x0000, 14, 1)) return 1;

	return 0;
}

static INT32 radarscpInit()
{
	radarscp = 1;
	INT32 ret = DrvInit(radarscpRomLoad, 0);

	if (ret == 0)
	{
		ZetOpen(0);
		ZetSetWriteHandler(radarscp_main_write);
		ZetClose();
	}

	return ret;
}

static void epos_bankswitch(INT32 bank); // forward

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029719;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {

		if (s2650_protection) {
			s2650Scan(nAction);
		} else {
			ZetScan(nAction);
		}

		i8257Scan();
		mcs48Scan(nAction);
		BurnSampleScan(nAction, pnMin);
		DACScan(nAction, pnMin);

		if (brazemode) EEPROMScan(nAction, pnMin);

		SCAN_VAR(dkongjr_walk);
		SCAN_VAR(sndpage);
		SCAN_VAR(sndstatus);
		SCAN_VAR(sndgrid_en);

		SCAN_VAR(dma_latch);
		SCAN_VAR(sample_state);
		SCAN_VAR(sample_count);
		SCAN_VAR(climb_data);
		SCAN_VAR(envelope_ctr);
		SCAN_VAR(decay);
		SCAN_VAR(braze_bank);  // braze and drakton
		SCAN_VAR(decrypt_counter);
		SCAN_VAR(hunch_prot_ctr); // hunchback (s2650)
		SCAN_VAR(hunchloopback);
		SCAN_VAR(main_fo);

		if (radarscp) {
			SCAN_VAR(sig30Hz);
			SCAN_VAR(lfsr_5I);
			SCAN_VAR(grid_sig);
			SCAN_VAR(rflip_sig);
			SCAN_VAR(star_ff);
			SCAN_VAR(blue_level);
			SCAN_VAR(cv1);
			SCAN_VAR(cv2);
			SCAN_VAR(cv3);
			SCAN_VAR(cv4);
			SCAN_VAR(vg1);
			SCAN_VAR(vg2);
			SCAN_VAR(vg3);
			SCAN_VAR(vc17);
			SCAN_VAR(pixelcnt);
		}

		SCAN_VAR(nExtraCycles);

		if (nAction & ACB_WRITE) {
			if (draktonmode) {
				ZetOpen(0);
				epos_bankswitch(braze_bank);
				ZetClose();
			}
			if (brazemode) {
				ZetOpen(0);
				braze_bankswitch(braze_bank);
				ZetClose();
			}
		}
	}

	return 0;
}

static INT32 Dkong3Scan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029719;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		M6502Scan(nAction);
		nesapuScan(nAction, pnMin);

		SCAN_VAR(dkongjr_walk);
		SCAN_VAR(sndpage);
		SCAN_VAR(sndstatus);
	}

	return 0;
}

static struct BurnSampleInfo RadarscpSampleDesc[] = {
	{ "shot",		SAMPLE_NOLOOP	},
	{ "enemy-die",	SAMPLE_NOLOOP	},
	{ "player-die",	SAMPLE_NOLOOP	},
	{ "siren",		SAMPLE_AUTOLOOP	},
	{ "",			0				}
};

STD_SAMPLE_PICK(Radarscp)
STD_SAMPLE_FN(Radarscp)


struct BurnDriver BurnDrvRadarscp = {
	"radarscp", NULL, NULL, "radarscp", "1980",
	"Radar Scope (TRS02, rev. D)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, radarscpRomInfo, radarscpRomName, NULL, NULL, RadarscpSampleInfo, RadarscpSampleName, RadarscpInputInfo, RadarscpDIPInfo,
	radarscpInit, DrvExit, DrvFrame, radarscpDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Radar Scope (TRS02?, rev. C)

static struct BurnRomInfo radarscpcRomDesc[] = {
	{ "trs2c5fc",	0x1000, 0x40949e0d, 1 }, //  0 maincpu
	{ "trs2c5gc",	0x1000, 0xafa8c49f, 1 }, //  1
	{ "trs2c5hc",	0x1000, 0x51b8263d, 1 }, //  2
	{ "trs2c5kc",	0x1000, 0x1f0101f7, 1 }, //  3

	{ "trs2s3i",	0x0800, 0x78034f14, 2 }, //  4 soundcpu

	{ "trs2v3gc",	0x0800, 0xf095330e, 3 }, //  5 gfx1
	{ "trs2v3hc",	0x0800, 0x15a316f0, 3 }, //  6

	{ "trs2v3dc",	0x0800, 0xe0bb0db9, 4 }, //  7 gfx2
	{ "trs2v3cc",	0x0800, 0x6c4e7dad, 4 }, //  8
	{ "trs2v3bc",	0x0800, 0x6fdd63f1, 4 }, //  9
	{ "trs2v3ac",	0x0800, 0xbbf62755, 4 }, // 10

	{ "rs2-x.xxx",	0x0100, 0x54609d61, 6 }, // 11 proms
	{ "rs2-c.xxx",	0x0100, 0x79a7d831, 6 }, // 12
	{ "rs2-v.1hc",	0x0100, 0x1b828315, 6 }, // 13

	{ "trs2v3ec",	0x0800, 0x0eca8d6b, 5 }, // 14 gfx3
};

STD_ROM_PICK(radarscpc)
STD_ROM_FN(radarscpc)


struct BurnDriver BurnDrvRadarscpc = {
	"radarscpc", "radarscp", NULL, "radarscp", "1980",
	"Radar Scope (TRS02?, rev. C)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, radarscpcRomInfo, radarscpcRomName, NULL, NULL, RadarscpSampleInfo, RadarscpSampleName, RadarscpInputInfo, RadarscpDIPInfo,
	radarscpInit, DrvExit, DrvFrame, radarscpDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Radar Scope (TRS01)

static struct BurnRomInfo radarscp1RomDesc[] = {
	{ "trs01_5f",		0x1000, 0x40949e0d, 1 }, //  0 maincpu
	{ "trs01_5g",		0x1000, 0xafa8c49f, 1 }, //  1
	{ "trs01_5h",		0x1000, 0x51b8263d, 1 }, //  2
	{ "trs01_5k",		0x1000, 0x1f0101f7, 1 }, //  3

	{ "trs-s__5a.5a",	0x0800, 0x5166554c, 2 }, //  4 soundcpu

	{ "trs01v3f",		0x0800, 0xf095330e, 4 }, //  5 gfx1
	{ "trs01v3g",		0x0800, 0x15a316f0, 4 }, //  6

	{ "trs01v3d",		0x0800, 0xe0bb0db9, 5 }, //  7 gfx2
	{ "trs01v3c",		0x0800, 0x6c4e7dad, 5 }, //  8
	{ "trs01v3b",		0x0800, 0x6fdd63f1, 5 }, //  9
	{ "trs01v3a",		0x0800, 0xbbf62755, 5 }, // 10

	{ "trs01c2j.bin",	0x0100, 0x2a087c87, 8 }, // 11 proms
	{ "trs01c2k.bin",	0x0100, 0x650c5daf, 8 }, // 12
	{ "trs01c2l.bin",	0x0100, 0x23087910, 8 }, // 13

	{ "trs011ha.bin",	0x0800, 0xdbcc50c2, 6 }, // 14 gfx3

	{ "trs01e3k.bin",	0x0100, 0x6c6f989c, 7 }, // 15 gfx4

	{ "trs-s__4h.4h",	0x0800, 0xd1f1b48c, 3 }, // 16 m58819 speech

	{ "trs01v1d.bin",	0x0100, 0x1b828315, 8 }, // 17 undumped prom, this one is from parent set
};

STD_ROM_PICK(radarscp1)
STD_ROM_FN(radarscp1)

static INT32 radarscp1RomLoad()
{
	if (radarscpRomLoad()) return 1;

	if (BurnLoadRom(DrvSndROM1 + 0x0000, 16, 1)) return 1; // tms speech

	if (BurnLoadRom(DrvGfxROM3 + 0x0000, 15, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0300, 17, 1)) return 1;

	return 0;
}

static INT32 radarscp1Init()
{
	radarscp = 1;
	radarscp1 = 1; // w/speech
	INT32 ret = DrvInit(radarscp1RomLoad, 0);

	if (ret == 0)
	{
		ZetOpen(0);
		ZetSetWriteHandler(radarscp_main_write);
		ZetClose();
	}

	return ret;
}

struct BurnDriver BurnDrvRadarscp1 = {
	"radarscp1", "radarscp", NULL, "radarscp", "1980",
	"Radar Scope (TRS01)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, radarscp1RomInfo, radarscp1RomName, NULL, NULL, RadarscpSampleInfo, RadarscpSampleName, RadarscpInputInfo, Radarscp1DIPInfo,
	radarscp1Init, DrvExit, DrvFrame, radarscpDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong (US set 1)

static struct BurnRomInfo dkongRomDesc[] = {
	{ "c_5et_g.bin",	0x1000, 0xba70b88b, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x1000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x1000, 0x1c97d324, 1 }, //  2
	{ "c_5at_g.bin",	0x1000, 0xb9005ac0, 1 }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 }, //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 }, // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 }, // 14
};

STD_ROM_PICK(dkong)
STD_ROM_FN(dkong)

static struct BurnSampleInfo DkongSampleDesc[] = {
#if !defined (ROM_VERIFY)
	{ "run01",   SAMPLE_NOLOOP },
	{ "run02",   SAMPLE_NOLOOP },
	{ "run03",   SAMPLE_NOLOOP },
	{ "jump",    SAMPLE_NOLOOP },
	{ "dkstomp", SAMPLE_NOLOOP },
#endif
	{ "",            0             }
};

STD_SAMPLE_PICK(Dkong)
STD_SAMPLE_FN(Dkong)

static INT32 dkongRomLoad()
{
	if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x3000,  3, 1)) return 1;

	if (BurnLoadRom(DrvSndROM0  + 0x0000,  4, 1)) return 1;
	memcpy (DrvSndROM0 + 0x0800, DrvSndROM0 + 0x0000, 0x0800); // re-load
	if (BurnLoadRom(DrvSndROM0  + 0x1000,  5, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x0000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x1000,  7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x0000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x2000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x3000, 11, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x0000, 12, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0100, 13, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0200, 14, 1)) return 1;

	return 0;
}

static INT32 dkongInit()
{
	return DrvInit(dkongRomLoad, 0);
}

struct BurnDriver BurnDrvDkong = {
	"dkong", NULL, NULL, "dkong", "1981",
	"Donkey Kong (US set 1)\0", NULL, "Nintendo of America", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongRomInfo, dkongRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong (hard kit)

static struct BurnRomInfo dkonghrdRomDesc[] = {
	{ "dk5ehard.bin",	0x1000, 0xa9445215, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x1000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x1000, 0x1c97d324, 1 }, //  2
	{ "dk5ahard.bin",	0x1000, 0xa990729b, 1 }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 }, //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 }, // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 }, // 14
};

STD_ROM_PICK(dkonghrd)
STD_ROM_FN(dkonghrd)

struct BurnDriver BurnDrvDkonghrd = {
	"dkonghrd", "dkong", NULL, "dkong", "1981",
	"Donkey Kong (hard kit)\0", NULL, "Nintendo of America", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkonghrdRomInfo, dkonghrdRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong (US set 2)

static struct BurnRomInfo dkongoRomDesc[] = {
	{ "c_5f_b.bin",		0x1000, 0x424f2b11, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x1000, 0x5ec461ec, 1 }, //  1
	{ "c_5h_b.bin",		0x1000, 0x1d28895d, 1 }, //  2
	{ "tkg3c.5k",		0x1000, 0x553b89bb, 1 }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 }, //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 }, // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 }, // 14
};

STD_ROM_PICK(dkongo)
STD_ROM_FN(dkongo)

struct BurnDriver BurnDrvDkongo = {
	"dkongo", "dkong", NULL, "dkong", "1981",
	"Donkey Kong (US set 2)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongoRomInfo, dkongoRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong (Japan set 1)

static struct BurnRomInfo dkongjRomDesc[] = {
	{ "c_5f_b.bin",	0x1000, 0x424f2b11, 1 }, //  0 maincpu
	{ "5g.cpu",	0x1000, 0xd326599b, 1 }, //  1
	{ "5h.cpu",	0x1000, 0xff31ac89, 1 }, //  2
	{ "c_5k_b.bin",	0x1000, 0x394d6007, 1 }, //  3

	{ "s_3i_b.bin",	0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",	0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",	0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_5k_b.bin",	0x0800, 0x3684f914, 3 }, //  7

	{ "l_4m_b.bin",	0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",	0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",	0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",	0x0800, 0x20f2ef7e, 4 }, // 11

	{ "c-2k.bpr",	0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",	0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",	0x0100, 0xb869b8f5, 5 }, // 14
};

STD_ROM_PICK(dkongj)
STD_ROM_FN(dkongj)

struct BurnDriver BurnDrvDkongj = {
	"dkongj", "dkong", NULL, "dkong", "1981",
	"Donkey Kong (Japan set 1)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongjRomInfo, dkongjRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong (Japan set 2)

static struct BurnRomInfo dkongjoRomDesc[] = {
	{ "c_5f_b.bin",	0x1000, 0x424f2b11, 1 }, //  0 maincpu
	{ "c_5g_b.bin",	0x1000, 0x3b2a6635, 1 }, //  1
	{ "c_5h_b.bin",	0x1000, 0x1d28895d, 1 }, //  2
	{ "c_5k_b.bin",	0x1000, 0x394d6007, 1 }, //  3

	{ "s_3i_b.bin",	0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",	0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",	0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_5k_b.bin",	0x0800, 0x3684f914, 3 }, //  7

	{ "l_4m_b.bin",	0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",	0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",	0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",	0x0800, 0x20f2ef7e, 4 }, // 11

	{ "c-2k.bpr",	0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",	0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",	0x0100, 0xb869b8f5, 5 }, // 14
};

STD_ROM_PICK(dkongjo)
STD_ROM_FN(dkongjo)

struct BurnDriver BurnDrvDkongjo = {
	"dkongjo", "dkong", NULL, "dkong", "1981",
	"Donkey Kong (Japan set 2)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongjoRomInfo, dkongjoRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong (Japan set 3)

static struct BurnRomInfo dkongjo1RomDesc[] = {
	{ "c_5f_b.bin",	0x1000, 0x424f2b11, 1 }, //  0 maincpu
	{ "5g.cpu",	0x1000, 0xd326599b, 1 }, //  1
	{ "c_5h_b.bin",	0x1000, 0x1d28895d, 1 }, //  2
	{ "5k.bin",	0x1000, 0x7961599c, 1 }, //  3

	{ "s_3i_b.bin",	0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",	0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",	0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_5k_b.bin",	0x0800, 0x3684f914, 3 }, //  7

	{ "l_4m_b.bin",	0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",	0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",	0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",	0x0800, 0x20f2ef7e, 4 }, // 11

	{ "c-2k.bpr",	0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",	0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",	0x0100, 0xb869b8f5, 5 }, // 14
};

STD_ROM_PICK(dkongjo1)
STD_ROM_FN(dkongjo1)

struct BurnDriver BurnDrvDkongjo1 = {
	"dkongjo1", "dkong", NULL, "dkong", "1981",
	"Donkey Kong (Japan set 3)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongjo1RomInfo, dkongjo1RomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Foundry (hack)

static struct BurnRomInfo dkongfRomDesc[] = {
	{ "dk_f.5et",	0x1000, 0x00b7efaf, 1 }, //  0 maincpu
	{ "dk_f.5ct",	0x1000, 0x88af9b69, 1 }, //  1
	{ "dk_f.5bt",	0x1000, 0xde74ad91, 1 }, //  2
	{ "dk_f.5at",	0x1000, 0x6a6bd420, 1 }, //  3

	{ "s_3i_b.bin",	0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",	0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",	0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_3pt.bin",	0x0800, 0x15e9c5e9, 3 }, //  7

	{ "l_4m_b.bin",	0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",	0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",	0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",	0x0800, 0x20f2ef7e, 4 }, // 11

	{ "c-2k.bpr",	0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",	0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",	0x0100, 0xb869b8f5, 5 }, // 14
};

STD_ROM_PICK(dkongf)
STD_ROM_FN(dkongf)

struct BurnDriver BurnDrvDkongf = {
	"dkongf", "dkong", NULL, "dkong", "2004",
	"Donkey Kong Foundry (hack)\0", NULL, "hack (Jeff Kulczycki)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongfRomInfo, dkongfRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Crazy Barrels Edition

static struct BurnRomInfo dkcbarrelRomDesc[] = {
	{ "dkcbarrel.5et",	0x1000, 0x78e37c41, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkcbarrel.5ct",	0x1000, 0xa46cbb85, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkcbarrel.5bt",	0x1000, 0x07da5b15, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkcbarrel.5at",	0x1000, 0x515e0639, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 | BRF_GRA },           //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 | BRF_GRA },           //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 | BRF_GRA },           //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 | BRF_GRA },           // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkcbarrel)
STD_ROM_FN(dkcbarrel)

struct BurnDriver BurnDrvDkcbarrel = {
	"dkcbarrel", "dkong", NULL, "dkong", "2019",
	"Donkey Kong Crazy Barrels Edition\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkcbarrelRomInfo, dkcbarrelRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Freerun Edition

static struct BurnRomInfo dkfreerunRomDesc[] = {
	{ "dkfreerun.5et",	0x1000, 0x2b85ddf0, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkfreerun.5ct",	0x1000, 0xef7e15d7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkfreerun.5bt",	0x1000, 0xcb390d7c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkfreerun.5at",	0x1000, 0x76fb86ba, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 | BRF_GRA },           //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 | BRF_GRA },           //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 | BRF_GRA },           //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 | BRF_GRA },           // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkfreerun)
STD_ROM_FN(dkfreerun)

struct BurnDriver BurnDrvDkfreerun = {
	"dkfreerun", "dkong", NULL, "dkong", "2019",
	"Donkey Kong Freerun Edition\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkfreerunRomInfo, dkfreerunRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Randomized Edition v1.01

static struct BurnRomInfo dkongran1RomDesc[] = {
	{ "dkongran1.5et",	0x1000, 0xfc29f234, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkongran1.5ct",	0x1000, 0x49e16508, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkongchm.5bt",	0x1000, 0xfce41e06, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkongran1.5at",	0x1000, 0x86723e5d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "v_5h_b.ran1",	0x0800, 0x17ef76ad, 3 | BRF_GRA },           //  6 gfx1
	{ "v_3pt.ran1",		0x0800, 0x49d408cd, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 | BRF_GRA },           //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 | BRF_GRA },           //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 | BRF_GRA },           // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkongran1)
STD_ROM_FN(dkongran1)

struct BurnDriver BurnDrvDkongran1 = {
	"dkongran1", "dkong", NULL, "dkong", "2020",
	"Donkey Kong Randomized Edition v1.01\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongran1RomInfo, dkongran1RomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong League Championship v1.00

static struct BurnRomInfo dkongchmRomDesc[] = {
	{ "dkongchm.5et",	0x1000, 0x26890d72, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkongchm.5ct",	0x1000, 0xd5965c23, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkongchm.5bt",	0x1000, 0xfce41e06, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkongchm.5at",	0x1000, 0xc48a4053, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "v_5h_b.chm",		0x0800, 0xa7a3772b, 3 | BRF_GRA },           //  6 gfx1
	{ "v_3pt.chm",		0x0800, 0x72b0b861, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 | BRF_GRA },           //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 | BRF_GRA },           //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 | BRF_GRA },           // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkongchm)
STD_ROM_FN(dkongchm)

struct BurnDriver BurnDrvDkongchm = {
	"dkongchm", "dkong", NULL, "dkong", "2020",
	"Donkey Kong League Championship v1.00\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongchmRomInfo, dkongchmRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Championship Edition v1.01

static struct BurnRomInfo dkongchm1RomDesc[] = {
	{ "dkongchm.5et",	0x1000, 0x26890d72, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkongchm.5ct",	0x1000, 0xd5965c23, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkongchm.5bt",	0x1000, 0xfce41e06, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkongchm1.5at",	0x1000, 0x458ff9b9, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 | BRF_GRA },           //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 | BRF_GRA },           //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 | BRF_GRA },           //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 | BRF_GRA },           // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkongchm1)
STD_ROM_FN(dkongchm1)

struct BurnDriver BurnDrvDkongchm1 = {
	"dkongchm1", "dkong", NULL, "dkong", "2020",
	"Donkey Kong Championship Edition v1.01\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongchm1RomInfo, dkongchm1RomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Springs Trainer

static struct BurnRomInfo dkongstRomDesc[] = {
	{ "dkongst.5et",	0x1000, 0x8fb6e908, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkongst.5ct",	0x1000, 0xc9d766ea, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkongst.5bt",	0x1000, 0xaef88ff5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkongst.5at",	0x1000, 0x5cf3774b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 | BRF_GRA },           //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 | BRF_GRA },           //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 | BRF_GRA },           //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 | BRF_GRA },           // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkongst)
STD_ROM_FN(dkongst)

struct BurnDriver BurnDrvDkongst = {
	"dkongst", "dkong", NULL, "dkong", "2019",
	"Donkey Kong Springs Trainer\0", NULL, "Sock Master", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongstRomInfo, dkongstRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Springs Trainer 2

static struct BurnRomInfo dkongst2RomDesc[] = {
	{ "dkongst2.5et",	0x1000, 0x21ddc6cc, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkongst2.5ct",	0x1000, 0xfa14da2c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkongst2.5bt",	0x1000, 0x32a8f924, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkongst2.5at",	0x0f00, 0x27b9c90d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 | BRF_GRA },           //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 | BRF_GRA },           //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 | BRF_GRA },           //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 | BRF_GRA },           // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkongst2)
STD_ROM_FN(dkongst2)

struct BurnDriver BurnDrvDkongst2 = {
	"dkongst2", "dkong", NULL, "dkong", "2020",
	"Donkey Kong Springs Trainer 2\0", NULL, "Sock Master", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongst2RomInfo, dkongst2RomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Skip Start v1.00

static struct BurnRomInfo dkongssRomDesc[] = {
	{ "dkongss.5et",	0x1000, 0x87c65c59, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "c_5ct_g.bin",	0x1000, 0x5ec461ec, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkongss.5bt",	0x1000, 0xf31c0c47, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkongss.5at",	0x1000, 0x87d58e2e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 | BRF_GRA },           //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 | BRF_GRA },           //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 | BRF_GRA },           //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 | BRF_GRA },           // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkongss)
STD_ROM_FN(dkongss)

struct BurnDriver BurnDrvDkongss = {
	"dkongss", "dkong", NULL, "dkong", "2020",
	"Donkey Kong Skip Start v1.00\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongssRomInfo, dkongssRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong On the Run v1.02

static struct BurnRomInfo dkongotrRomDesc[] = {
	{ "dkongotr.5et",	0x1000, 0xfd64526a, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkongotr.5ct",	0x1000, 0x6d692d1b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkongotr.5bt",	0x1000, 0xd029c495, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkongotr.5at",	0x1000, 0x9b58b813, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "dkongotr.5h",	0x0800, 0x0d588de5, 3 | BRF_GRA },           //  6 gfx1
	{ "dkongotr.3pt",	0x0800, 0xbfb2c04f, 3 | BRF_GRA },           //  7

	{ "dkongotr.4m",	0x0800, 0xf224b2bc, 4 | BRF_GRA },           //  8 gfx2
	{ "dkongotr.4n",	0x0800, 0xdef8bca4, 4 | BRF_GRA },           //  9
	{ "dkongotr.4r",	0x0800, 0x59e3e846, 4 | BRF_GRA },           // 10
	{ "dkongotr.4s",	0x0800, 0xb5a2e920, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkongotr)
STD_ROM_FN(dkongotr)

struct BurnDriver BurnDrvDkongotr = {
	"dkongotr", "dkong", NULL, "dkong", "2020",
	"Donkey Kong On the Run v1.02\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongotrRomInfo, dkongotrRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong In the Dark v1.02

static struct BurnRomInfo dkongitdRomDesc[] = {
	{ "dkongitd.5et",	0x1000, 0xabddd83e, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkongitd.5ct",	0x1000, 0xee146d99, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkongchm.5bt",	0x1000, 0xfce41e06, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkongitd.5at",	0x1000, 0x6b8d5524, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "dkongitd.5h",	0x0800, 0x3d4ea8f8, 3 | BRF_GRA },           //  6 gfx1
	{ "dkongitd.3pt",	0x0800, 0x0979cf74, 3 | BRF_GRA },           //  7

	{ "dkongitd.4m",	0x0800, 0x21a04aa5, 4 | BRF_GRA },           //  8 gfx2
	{ "dkongitd.4n",	0x0800, 0x27ddec12, 4 | BRF_GRA },           //  9
	{ "dkongitd.4r",	0x0800, 0xddfee3e1, 4 | BRF_GRA },           // 10
	{ "dkongitd.4s",	0x0800, 0x42d26b1b, 4 | BRF_GRA },           // 11

	{ "dkongitd.2k",	0x0100, 0x8d918467, 5 | BRF_GRA },           // 12 proms
	{ "dkongitd.2j",	0x0100, 0x9aadf04a, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkongitd)
STD_ROM_FN(dkongitd)

struct BurnDriver BurnDrvDkongitd = {
	"dkongitd", "dkong", NULL, "dkong", "2020",
	"Donkey Kong In the Dark v1.02\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongitdRomInfo, dkongitdRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Twisted Jungle v1.05

static struct BurnRomInfo dkongtjRomDesc[] = {
	{ "dkongtj.5et",	0x1000, 0x63e2a483, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkongtj.5ct",	0x1000, 0xed5c7b13, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkongtj.5bt",	0x1000, 0xb1990430, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkongtj.5at",	0x1000, 0xb4e0240a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "dkongtj.5h",		0x0800, 0xfccb93d6, 3 | BRF_GRA },           //  6 gfx1
	{ "dkongtj.3pt",	0x0800, 0x95408f9f, 3 | BRF_GRA },           //  7

	{ "dkongtj.4m",		0x0800, 0x5e77597e, 4 | BRF_GRA },           //  8 gfx2
	{ "dkongtj.4n",		0x0800, 0x715c29ff, 4 | BRF_GRA },           //  9
	{ "dkongtj.4r",		0x0800, 0x7b24438d, 4 | BRF_GRA },           // 10
	{ "dkongtj.4s",		0x0800, 0x57d3989a, 4 | BRF_GRA },           // 11

	{ "dkongtj.2k",		0x0100, 0x84be5373, 5 | BRF_GRA },           // 12 proms
	{ "dkongtj.2j",		0x0100, 0xa81ca93c, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkongtj)
STD_ROM_FN(dkongtj)

struct BurnDriver BurnDrvDkongtj = {
	"dkongtj", "dkong", NULL, "dkong", "2021",
	"Donkey Kong Twisted Jungle v1.05\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongtjRomInfo, dkongtjRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Barrelpalooza v1.06

static struct BurnRomInfo dkongbpRomDesc[] = {
	{ "dkongbp.5et",	0x1000, 0xbb5c7dca, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkongbp.5ct",	0x1000, 0xc742739c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkongbp.5bt",	0x1000, 0xa46859ec, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkongbp.5at",	0x1000, 0xeafd7c54, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "dkongbp.3i",		0x0800, 0x7590f5ee, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "dkongbp.5h",		0x0800, 0xcaf8820b, 3 | BRF_GRA },           //  6 gfx1
	{ "dkongbp.3pt",	0x0800, 0xb0cd1e84, 3 | BRF_GRA },           //  7

	{ "dkongbp.4m",		0x0800, 0x1b46aae1, 4 | BRF_GRA },           //  8 gfx2
	{ "dkongbp.4n",		0x0800, 0xfbaaa6f0, 4 | BRF_GRA },           //  9
	{ "dkongbp.4r",		0x0800, 0x919362a0, 4 | BRF_GRA },           // 10
	{ "dkongbp.4s",		0x0800, 0xd57098ca, 4 | BRF_GRA },           // 11

	{ "dkongbp.2k",		0x0100, 0x4826ce71, 5 | BRF_GRA },           // 12 proms
	{ "dkongbp.2j",		0x0100, 0x4a7a511b, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkongbp)
STD_ROM_FN(dkongbp)

struct BurnDriver BurnDrvDkongbp = {
	"dkongbp", "dkong", NULL, "dkong", "2021",
	"Donkey Kong Barrelpalooza v1.06\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongbpRomInfo, dkongbpRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Barrelpalooza v1.07

static struct BurnRomInfo dkongbp1RomDesc[] = {
	{ "dkongbp1.5et",	0x1000, 0xc80c0431, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkongbp.5ct",	0x1000, 0xc742739c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkongbp.5bt",	0x1000, 0xa46859ec, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkongbp1.5at",	0x1000, 0x4742a48e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "dkongbp.3i",		0x0800, 0x7590f5ee, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "dkongbp.5h",		0x0800, 0xcaf8820b, 3 | BRF_GRA },           //  6 gfx1
	{ "dkongbp.3pt",	0x0800, 0xb0cd1e84, 3 | BRF_GRA },           //  7

	{ "dkongbp.4m",		0x0800, 0x1b46aae1, 4 | BRF_GRA },           //  8 gfx2
	{ "dkongbp.4n",		0x0800, 0xfbaaa6f0, 4 | BRF_GRA },           //  9
	{ "dkongbp.4r",		0x0800, 0x919362a0, 4 | BRF_GRA },           // 10
	{ "dkongbp.4s",		0x0800, 0xd57098ca, 4 | BRF_GRA },           // 11

	{ "dkongbp.2k",		0x0100, 0x4826ce71, 5 | BRF_GRA },           // 12 proms
	{ "dkongbp.2j",		0x0100, 0x4a7a511b, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkongbp1)
STD_ROM_FN(dkongbp1)

struct BurnDriver BurnDrvDkongbp1 = {
	"dkongbp1", "dkong", NULL, "dkong", "2021",
	"Donkey Kong Barrelpalooza v1.07\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongbp1RomInfo, dkongbp1RomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong: Pauline Edition Rev 5 (2013-04-22)
// "Pauline Edition" hack (rev 5, 4-22-2013), by Clay Cowgill based on Mike Mika's NES version

static struct BurnRomInfo dkongpeRomDesc[] = {
	{ "c_5et_g.bin",	0x1000, 0xba70b88b, 1 }, //  0 maincpu
	{ "c_5ct_gp.bin",	0x1000, 0x45af403e, 1 }, //  1
	{ "c_5bt_gp.bin",	0x1000, 0x3a9783b7, 1 }, //  2
	{ "c_5at_gp.bin",	0x1000, 0x32bc20ff, 1 }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_bp.bin",	0x0800, 0x007aa348, 3 }, //  6 gfx1
	{ "v_3ptp.bin",		0x0800, 0xa967aff0, 3 }, //  7

	{ "l_4m_bp.bin",	0x0800, 0x766ae006, 4 }, //  8 gfx2
	{ "l_4n_bp.bin",	0x0800, 0x39e7ca4b, 4 }, //  9
	{ "l_4r_bp.bin",	0x0800, 0x012f2f25, 4 }, // 10
	{ "l_4s_bp.bin",	0x0800, 0x84eb5bfb, 4 }, // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 }, // 14
};

STD_ROM_PICK(dkongpe)
STD_ROM_FN(dkongpe)

struct BurnDriver BurnDrvDkongpe = {
	"dkongpe", "dkong", NULL, "dkong", "2013",
	"Donkey Kong: Pauline Edition Rev 5 (2013-04-22)\0", NULL, "hack (Clay Cowgill and Mike Mika)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongpeRomInfo, dkongpeRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong - Arcade Rainbow (hack)
// 6-29-2015 John Kowalski

static struct BurnRomInfo dkrainbowRomDesc[] = {
	{ "c_5et_g.bin",	0x1000, 0xba70b88b, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x1000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x1000, 0x1c97d324, 1 }, //  2
	{ "c_5at_g.bin",	0x1000, 0xb9005ac0, 1 }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 }, //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 }, // 11

	{ "dkr_c-2k.bpr",	0x0100, 0xc0dce2f5, 5 }, // 12 proms
	{ "dkr_c-2j.bpr",	0x0100, 0x03c3153f, 5 }, // 13
	{ "dkr_v-5e.bpr",	0x0100, 0xd9f3005a, 5 }, // 14
};

STD_ROM_PICK(dkrainbow)
STD_ROM_FN(dkrainbow)

struct BurnDriver BurnDrvDkrainbow = {
	"dkrainbow", "dkong", NULL, "dkong", "2015",
	"Donkey Kong - Arcade Rainbow (hack)\0", NULL, "hack (john Kowalski)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkrainbowRomInfo, dkrainbowRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong (2600 graphics, hack)

static struct BurnRomInfo kong2600RomDesc[] = {
	{ "c_5et_g.bin",	0x1000, 0xba70b88b, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x1000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x1000, 0x1c97d324, 1 }, //  2
	{ "c_5at_g.bin",	0x1000, 0xb9005ac0, 1 }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 }, //  5

	{ "k2600.3n",		0x0800, 0x0e6a2a6d, 3 }, //  6 gfx1
	{ "k2600.3p",		0x0800, 0xca57e0f4, 3 }, //  7

	{ "k2600.7c",		0x0800, 0xcf450a43, 4 }, //  8 gfx2
	{ "k2600.7d",		0x0800, 0xd5046907, 4 }, //  9
	{ "k2600.7e",		0x0800, 0x1539fe2a, 4 }, // 10
	{ "k2600.7f",		0x0800, 0x77cc00ab, 4 }, // 11

	{ "k2600.2k",		0x0100, 0x1e82d375, 5 }, // 12 proms
	{ "k2600.2j",		0x0100, 0x2ab01dc8, 5 }, // 13
	{ "k2600.5f",		0x0100, 0x44988665, 5 }, // 14
};

STD_ROM_PICK(kong2600)
STD_ROM_FN(kong2600)

struct BurnDriver BurnDrvKong2600 = {
	"kong2600", "dkong", NULL, "dkong", "1999",
	"Donkey Kong (2600 graphics, hack)\0", NULL, "Vic Twenty George", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, kong2600RomInfo, kong2600RomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Reverse
// Hack by Paul Goes (2019)

static struct BurnRomInfo dkongrevRomDesc[] = {
	{ "dkongrev.5et",	0x1000, 0xee02057e, 1 }, //  0 maincpu
	{ "dkongrev.5ct",	0x1000, 0xe6fabd0f, 1 }, //  1
	{ "dkongrev.5bt",	0x1000, 0x31c5bea3, 1 }, //  2
	{ "dkongrev.5at",	0x1000, 0xc7d04ef3, 1 }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 }, //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 }, // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 }, // 14

};

STD_ROM_PICK(dkongrev)
STD_ROM_FN(dkongrev)

struct BurnDriver BurnDrvDkongrev = {
	"dkongrev", "dkong", NULL, "dkong", "2019",
	"Donkey Kong Reverse (Hack)\0", NULL, "Hack (Paul Goes)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongrevRomInfo, dkongrevRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};

// Donkey Kong RNDMZR
// www.donkeykonghacks.net

static struct BurnRomInfo dkrndmzrRomDesc[] = {
	{ "dkrndmzr.5et",	0x1000, 0x281b9bb6, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkrndmzr.5ct",	0x1000, 0x7fa1be23, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkrndmzr.5bt",	0x1000, 0x50ad6d22, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkrndmzr.5at",	0x1000, 0x4c9e7085, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "v_5h_b.bin",		0x0800, 0xc77456de, 3 | BRF_GRA },           //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x994f28be, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 | BRF_GRA },           //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 | BRF_GRA },           //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 | BRF_GRA },           // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkrndmzr)
STD_ROM_FN(dkrndmzr)

struct BurnDriver BurnDrvdkrndmzr = {
	"dkrndmzr", "dkong", NULL, "dkong", "2022",
	"Donkey Kong RNDMZR\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkrndmzrRomInfo, dkrndmzrRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Wizardry

static struct BurnRomInfo dkwizardryRomDesc[] = {
	{ "dkwizardry.5et",	0x1000, 0xf42bb2e5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkwizardry.5ct",	0x1000, 0x85cf6e4c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkwizardry.5bt",	0x1000, 0x6e888d7b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkwizardry.5at",	0x1000, 0xb8e1a56d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "v_5h_b.bin",		0x0800, 0x39736a8a, 3 | BRF_GRA },           //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0xc94468a9, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0xf98da4c5, 4 | BRF_GRA },           //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0xcf3cdb75, 4 | BRF_GRA },           //  9
	{ "l_4r_b.bin",		0x0800, 0x7e2d1ef4, 4 | BRF_GRA },           // 10
	{ "l_4s_b.bin",		0x0800, 0xcc547d47, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0x193134e9, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xdcbba451, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xc24f2312, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkwizardry)
STD_ROM_FN(dkwizardry)

struct BurnDriver BurnDrvdkwizardry = {
	"dkwizardry", "dkong", NULL, "dkong", "2022",
	"Donkey Kong Wizardry\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkwizardryRomInfo, dkwizardryRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};

// Donkey Kong Hearthunt

static struct BurnRomInfo dkhrthntRomDesc[] = {
	{ "dkhrthnt.5et",	0x1000, 0xc9a84bae, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkhrthnt.5ct",	0x1000, 0xe55daf9e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkhrthnt.5bt",	0x1000, 0x34466de4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkhrthnt.5at",	0x1000, 0xd51b8307, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "hh_v_5h_b.bin",	0x0800, 0xc52ec836, 3 | BRF_GRA },           //  6 gfx1
	{ "hh_v_3pt.bin",	0x0800, 0x655b1313, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 | BRF_GRA }, 			 //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 | BRF_GRA }, 			 //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 | BRF_GRA }, 			 // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 | BRF_GRA }, 			 // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 | BRF_GRA },           // 13
	{ "hh_v-5e.bpr",	0x0100, 0x15ea25d5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkhrthnt)
STD_ROM_FN(dkhrthnt)

struct BurnDriver BurnDrvdkhrthnt = {
	"dkhrthnt", "dkong", NULL, "dkong", "2022",
	"Donkey Kong Hearthunt\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkhrthntRomInfo, dkhrthntRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};

// Donkey Kong Accelerate

static struct BurnRomInfo dkaccelRomDesc[] = {
	{ "dkaccel.5et",	0x1000, 0xcfb3a3be, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkaccel.5ct",	0x1000, 0x9a527b63, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkaccel.5bt",	0x1000, 0x107b677c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkaccel.5at",	0x1000, 0x622b283f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "dka_v_5h_b.bin",	0x0800, 0xb3a5f655, 3 | BRF_GRA },           //  6 gfx1
	{ "dka_v_3pt.bin",	0x0800, 0x0bebf954, 3 | BRF_GRA },           //  7

	{ "dka_l_4m_b.bin",	0x0800, 0x3c7c711e, 4 | BRF_GRA }, 			 //  8 gfx2
	{ "dka_l_4n_b.bin",	0x0800, 0x7f0e788f, 4 | BRF_GRA }, 			 //  9
	{ "dka_l_4r_b.bin",	0x0800, 0x89129b53, 4 | BRF_GRA }, 			 // 10
	{ "dka_l_4s_b.bin",	0x0800, 0xdf2aa287, 4 | BRF_GRA }, 			 // 11

	{ "dka_c-2k.bpr",	0x0100, 0xd46f27e1, 5 | BRF_GRA },           // 12 proms
	{ "dka_c-2j.bpr",	0x0100, 0x9e4af035, 5 | BRF_GRA },           // 13
	{ "dka_v-5e.bpr",	0x0100, 0xece3b0f2, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkaccel)
STD_ROM_FN(dkaccel)

struct BurnDriver BurnDrvdkaccel = {
	"dkaccel", "dkong", NULL, "dkong", "2023",
	"Donkey Kong Accelerate\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkaccelRomInfo, dkaccelRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};

// Donkey Kong Pac-Man Crossover
static struct BurnRomInfo dkpmxRomDesc[] = {
	{ "dkpmx.5et",	0x1000, 0xa7a913d5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkpmx.5ct",	0x1000, 0x03ce5531, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkpmx.5bt",	0x1000, 0xabaf3fa1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkpmx.5at",	0x1000, 0x843a9751, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "dkpmx_s_3i_b.bin",	0x0800, 0x4f865d26, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "dkpmx_s_3j_b.bin",	0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "dkpmx_v_5h_b.bin",	0x0800, 0xb212d49b, 3 | BRF_GRA },           //  6 gfx1
	{ "dkpmx_v_3pt.bin",	0x0800, 0x2bd67b35, 3 | BRF_GRA },           //  7

	{ "dkpmx_l_4m_b.bin",	0x0800, 0x7db40811, 4 | BRF_GRA }, 			 //  8 gfx2
	{ "dkpmx_l_4n_b.bin",	0x0800, 0xc9f3b37a, 4 | BRF_GRA }, 			 //  9
	{ "dkpmx_l_4r_b.bin",	0x0800, 0x137ad00e, 4 | BRF_GRA }, 			 // 10
	{ "dkpmx_l_4s_b.bin",	0x0800, 0x72b56559, 4 | BRF_GRA }, 			 // 11

	{ "dkpmx_c-2k.bpr",	0x0100, 0x929a396b, 5 | BRF_GRA },           // 12 proms
	{ "dkpmx_c-2j.bpr",	0x0100, 0x1aa9c17e, 5 | BRF_GRA },           // 13
	{ "dkpmx_v-5e.bpr",	0x0100, 0xd06c6ba8, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkpmx)
STD_ROM_FN(dkpmx)

struct BurnDriver BurnDrvdkpmx = {
	"dkpmx", "dkong", NULL, "dkong", "2024",
	"Donkey Kong Pac-Man Crossover\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkpmxRomInfo, dkpmxRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};

// Donkey Kong Anniversary Edition

static struct BurnRomInfo dkong40yRomDesc[] = {
	{ "dkong40y.5et",	0x1000, 0xcf44c2dd, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkong40y.5ct",	0x1000, 0x34238a95, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkong40y.5bt",	0x1000, 0x6c5614e5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkong40y.5at",	0x1000, 0x0b92803c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0x44993c29, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "v_5h_b.bin",		0x0800, 0xae4c3990, 3 | BRF_GRA },           //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x25cc07d4, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0x410ab9a2, 4 | BRF_GRA },           //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x4f7e8fd4, 4 | BRF_GRA },           //  9
	{ "l_4r_b.bin",		0x0800, 0x9eb470c0, 4 | BRF_GRA },           // 10
	{ "l_4s_b.bin",		0x0800, 0x73ef61cc, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkong40y)
STD_ROM_FN(dkong40y)

struct BurnDriver BurnDrvdkong40y = {
	"dkong40y", "dkong", NULL, "dkong", "2021",
	"Donkey Kong Anniversary Edition\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkong40yRomInfo, dkong40yRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Duel

static struct BurnRomInfo dkduelRomDesc[] = {
	{ "dkduel.5et",		0x1000, 0x87a4912f, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dkduel.5ct",		0x1000, 0x1c547c2b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dkduel.5bt",		0x1000, 0xfce41e06, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dkduel.5at",		0x1000, 0xc1e0654c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "s_3i_b.bin",		0x0800, 0xcc9aea3b, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "v_5h_b.bin",		0x0800, 0x3cf6ef8b, 3 | BRF_GRA },           //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x3775dd7b, 3 | BRF_GRA },           //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 | BRF_GRA },           //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 | BRF_GRA },           //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 | BRF_GRA },           // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 | BRF_GRA },           // 11

	{ "c-2k.bpr",		0x0100, 0x02e1f91b, 5 | BRF_GRA },           // 12 proms
	{ "c-2j.bpr",		0x0100, 0x4176057f, 5 | BRF_GRA },           // 13
	{ "v-5e.bpr",		0x0100, 0x94695888, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(dkduel)
STD_ROM_FN(dkduel)

struct BurnDriver BurnDrvdkduel = {
	"dkduel", "dkong", NULL, "dkong", "2021",
	"Donkey Kong Duel\0", NULL, "Paul Goes", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkduelRomInfo, dkduelRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongfDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong II: Jumpman Returns (hack, V1.2)

static struct BurnRomInfo dkongxRomDesc[] = {
	{ "c_5et_g.bin",	0x01000, 0xba70b88b, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x01000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x01000, 0x1c97d324, 1 }, //  2
	{ "c_5at_g.bin",	0x01000, 0xb9005ac0, 1 }, //  3

	{ "d2k12.bin",		0x10000, 0x6e95ca0d, 2 }, //  4 braze

	{ "s_3i_b.bin",		0x00800, 0x45a4ed06, 3 }, //  5 soundcpu
	{ "s_3j_b.bin",		0x00800, 0x4743fe92, 3 }, //  6

	{ "v_5h_b.bin",		0x00800, 0x12c8c95d, 4 }, //  7 gfx1
	{ "v_3pt.bin",		0x00800, 0x15e9c5e9, 4 }, //  8

	{ "l_4m_b.bin",		0x00800, 0x59f8054d, 5 }, //  9 gfx2
	{ "l_4n_b.bin",		0x00800, 0x672e4714, 5 }, // 10
	{ "l_4r_b.bin",		0x00800, 0xfeaa59ee, 5 }, // 11
	{ "l_4s_b.bin",		0x00800, 0x20f2ef7e, 5 }, // 12

	{ "c-2k.bpr",		0x00100, 0xe273ede5, 6 }, // 13 proms
	{ "c-2j.bpr",		0x00100, 0xd6412358, 6 }, // 14
	{ "v-5e.bpr",		0x00100, 0xb869b8f5, 6 }, // 15
};

STD_ROM_PICK(dkongx)
STD_ROM_FN(dkongx)

static INT32 dkongxRomLoad()
{
	if (BurnLoadRom(DrvZ80ROM  + 0x00000,  4, 1)) return 1; // "braze"

	if (BurnLoadRom(DrvSndROM0 + 0x00000,  5, 1)) return 1;
	memcpy (DrvSndROM0 + 0x0800, DrvSndROM0 + 0x0000, 0x0800); // re-load
	if (BurnLoadRom(DrvSndROM0 + 0x01000,  6, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x0000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x1000,  8, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x0000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x2000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x3000, 12, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x0000, 13, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0100, 14, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0200, 15, 1)) return 1;

	braze_decrypt_rom();

	return 0;
}

static INT32 dkongxInit()
{
	INT32 ret = DrvInit(dkongxRomLoad, 0);

	if (ret == 0)
	{
		ZetOpen(0);
		ZetSetWriteHandler(braze_main_write);
		ZetSetReadHandler(braze_main_read);
		braze_bankswitch(0);
		ZetClose();
	}

	brazemode = 1;

	return ret;
}

struct BurnDriver BurnDrvDkongx = {
	"dkongx", "dkong", NULL, "dkong", "2006",
	"Donkey Kong II: Jumpman Returns (hack, V1.2)\0", NULL, "hack (Braze Technologies)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongxRomInfo, dkongxRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongNoDipDIPInfo,
	dkongxInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong II: Jumpman Returns (hack, V1.1)

static struct BurnRomInfo dkongx11RomDesc[] = {
	{ "c_5et_g.bin",	0x01000, 0xba70b88b, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x01000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x01000, 0x1c97d324, 1 }, //  2
	{ "c_5at_g.bin",	0x01000, 0xb9005ac0, 1 }, //  3

	{ "d2k11.bin",		0x10000, 0x2048fc42, 2 }, //  4 braze

	{ "s_3i_b.bin",		0x00800, 0x45a4ed06, 3 }, //  5 soundcpu
	{ "s_3j_b.bin",		0x00800, 0x4743fe92, 3 }, //  6

	{ "v_5h_b.bin",		0x00800, 0x12c8c95d, 4 }, //  7 gfx1
	{ "v_3pt.bin",		0x00800, 0x15e9c5e9, 4 }, //  8

	{ "l_4m_b.bin",		0x00800, 0x59f8054d, 5 }, //  9 gfx2
	{ "l_4n_b.bin",		0x00800, 0x672e4714, 5 }, // 10
	{ "l_4r_b.bin",		0x00800, 0xfeaa59ee, 5 }, // 11
	{ "l_4s_b.bin",		0x00800, 0x20f2ef7e, 5 }, // 12

	{ "c-2k.bpr",		0x00100, 0xe273ede5, 6 }, // 13 proms
	{ "c-2j.bpr",		0x00100, 0xd6412358, 6 }, // 14
	{ "v-5e.bpr",		0x00100, 0xb869b8f5, 6 }, // 15
};

STD_ROM_PICK(dkongx11)
STD_ROM_FN(dkongx11)

struct BurnDriver BurnDrvDkongx11 = {
	"dkongx11", "dkong", NULL, "dkong", "2006",
	"Donkey Kong II: Jumpman Returns (hack, V1.1)\0", NULL, "hack (Braze Technologies)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongx11RomInfo, dkongx11RomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongNoDipDIPInfo,
	dkongxInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};

// Donkey Kong Remix (hack)
// Hack of Donkey Kong Christmas Remix made to look like the original Donkey Kong Remix
// from dkremix.com, it is NOT a dump of the kit.

static struct BurnRomInfo dkremixRomDesc[] = {
	{ "c_5et_g.bin",	0x01000, 0xba70b88b, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x01000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x01000, 0x1c97d324, 1 }, //  2
	{ "c_5at_g.bin",	0x01000, 0xb9005ac0, 1 }, //  3

	{ "dkremix.bin",    0x10000, 0xf47c13aa, 2 }, //  4 braze

	{ "s_3i_b.bin",		0x00800, 0x45a4ed06, 3 }, //  5 soundcpu
	{ "s_3j_b.bin",		0x00800, 0x4743fe92, 3 }, //  6

	{ "dkremix.5h",		0x00800, 0xfc82b069, 4 }, //  7 gfx1
	{ "dkremix.3pt",	0x00800, 0xfe32ee33, 4 }, //  8

	{ "dkremix.4m",		0x00800, 0x3d9784d7, 5 }, //  9 gfx2
	{ "dkremix.4n",		0x00800, 0x084c960a, 5 }, // 10
	{ "dkremix.4r",		0x00800, 0x9ac5d874, 5 }, // 11
	{ "dkremix.4s",		0x00800, 0x74a5d517, 5 }, // 12

	{ "c-2k.bpr",		0x00100, 0xe273ede5, 6 }, // 13 proms
	{ "c-2j.bpr",		0x00100, 0xd6412358, 6 }, // 14
	{ "v-5e.bpr",		0x00100, 0xb869b8f5, 6 }, // 15
};

STD_ROM_PICK(dkremix)
STD_ROM_FN(dkremix)

struct BurnDriver BurnDrvDkremix = {
	"dkremix", "dkong", NULL, "dkong", "2023",
	"Donkey Kong Remix (Hack)\0", NULL, "hack", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkremixRomInfo, dkremixRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongNoDipDIPInfo,
	dkongxInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};

// Donkey Kong Christmas Remix (Hack)

static struct BurnRomInfo dkchrmxRomDesc[] = {
	{ "c_5et_g.bin",	0x01000, 0xba70b88b, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x01000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x01000, 0x1c97d324, 1 }, //  2
	{ "c_5at_g.bin",	0x01000, 0xb9005ac0, 1 }, //  3

	{ "dkchrmx.bin",	0x10000, 0xe5273cee, 2 }, //  4 braze

	{ "s_3i_b.bin",		0x00800, 0x45a4ed06, 3 }, //  5 soundcpu
	{ "s_3j_b.bin",		0x00800, 0x4743fe92, 3 }, //  6

	{ "v_5h_b.ch",		0x00800, 0x0b92cc7a, 4 }, //  7 gfx1
	{ "v_3pt.ch",		0x00800, 0x6a04f93f, 4 }, //  8

	{ "l_4m_b.ch",		0x00800, 0xc6ddc85f, 5 }, //  9 gfx2
	{ "l_4n_b.ch",		0x00800, 0x2cd9cfdf, 5 }, // 10
	{ "l_4r_b.ch",		0x00800, 0xc1ea6688, 5 }, // 11
	{ "l_4s_b.ch",		0x00800, 0x9473d658, 5 }, // 12

	{ "c-2k.ch",		0x00100, 0xc6cee97e, 6 }, // 13 proms
	{ "c-2j.ch",		0x00100, 0x1f64ac3d, 6 }, // 14
	{ "v-5e.ch",		0x00100, 0x5a8ca805, 6 }, // 15
};

STD_ROM_PICK(dkchrmx)
STD_ROM_FN(dkchrmx)

struct BurnDriver BurnDrvDkchrmx = {
	"dkchrmx", "dkong", NULL, "dkong", "2017",
	"Donkey Kong Christmas Remix (Hack)\0", NULL, "Sock Master", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkchrmxRomInfo, dkchrmxRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongNoDipDIPInfo,
	dkongxInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Christmas Tournament Edition (Hack)

static struct BurnRomInfo dkchrteRomDesc[] = {
	{ "c_5et_g.bin",	0x01000, 0xba70b88b, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x01000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x01000, 0x1c97d324, 1 }, //  2
	{ "c_5at_g.bin",	0x01000, 0xb9005ac0, 1 }, //  3

	{ "dkchrte.bin",	0x10000, 0x3d6b05f6, 2 }, //  4 braze

	{ "s_3i_b.bin",		0x00800, 0x45a4ed06, 3 }, //  5 soundcpu
	{ "s_3j_b.bin",		0x00800, 0x4743fe92, 3 }, //  6

	{ "dkchrte.5h",		0x00800, 0x0bdb6d28, 4 }, //  7 gfx1
	{ "dkchrte.3pt",	0x00800, 0x6bb0affb, 4 }, //  8

	{ "dkchrte.4m",		0x00800, 0x3282f0a1, 5 }, //  9 gfx2
	{ "dkchrte.4n",		0x00800, 0x89ebf388, 5 }, // 10
	{ "dkchrte.4r",		0x00800, 0xfe8b84a8, 5 }, // 11
	{ "dkchrte.4s",		0x00800, 0x3b18ae70, 5 }, // 12

	{ "c-2k.ch",		0x00100, 0xc6cee97e, 6 }, // 13 proms
	{ "c-2j.ch",		0x00100, 0x1f64ac3d, 6 }, // 14
	{ "v-5e.ch",		0x00100, 0x5a8ca805, 6 }, // 15
};

STD_ROM_PICK(dkchrte)
STD_ROM_FN(dkchrte)

struct BurnDriver BurnDrvDkchrte = {
	"dkchrte", "dkong", NULL, "dkong", "2022",
	"Donkey Kong Christmas Tournament Edition (Hack)\0", NULL, "Sock Master", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkchrteRomInfo, dkchrteRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongNoDipDIPInfo,
	dkongxInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Spooky Remix (Hack)

static struct BurnRomInfo dkspkyrmxRomDesc[] = {
	{ "c_5et_g.bin",	0x01000, 0xba70b88b, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x01000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x01000, 0x1c97d324, 1 }, //  2
	{ "c_5at_g.bin",	0x01000, 0xb9005ac0, 1 }, //  3

	{ "dkspkyrmx.bin",	0x08000, 0xe68c6bfc, 2 }, //  4 braze

	{ "s_3i_b.bin",		0x00800, 0x45a4ed06, 3 }, //  5 soundcpu
	{ "s_3j_b.bin",		0x00800, 0x4743fe92, 3 }, //  6

	{ "v_5h_b.sp",		0x00800, 0xb70b0904, 4 }, //  7 gfx1
	{ "v_3pt.sp",		0x00800, 0xbe8c92c3, 4 }, //  8

	{ "l_4m_b.sp",		0x00800, 0x1d0b3b77, 5 }, //  9 gfx2
	{ "l_4n_b.sp",		0x00800, 0xcd717e7c, 5 }, // 10
	{ "l_4r_b.sp",		0x00800, 0xd019732b, 5 }, // 11
	{ "l_4s_b.sp",		0x00800, 0x04272273, 5 }, // 12

	{ "c-2k.sp",		0x00100, 0xa837a227, 6 }, // 13 proms
	{ "c-2j.sp",		0x00100, 0x244a89f9, 6 }, // 14
	{ "v-5e.sp",		0x00100, 0xc70b6f9b, 6 }, // 15
};

STD_ROM_PICK(dkspkyrmx)
STD_ROM_FN(dkspkyrmx)

struct BurnDriver BurnDrvDkspkyrmx = {
	"dkspkyrmx", "dkong", NULL, "dkong", "2018",
	"Donkey Kong Spooky Remix (Hack)\0", NULL, "Sock Master", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkspkyrmxRomInfo, dkspkyrmxRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongNoDipDIPInfo,
	dkongxInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Spooky Tournament Edition (Hack)

static struct BurnRomInfo dkspkyteRomDesc[] = {
	{ "c_5et_g.bin",	0x01000, 0xba70b88b, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x01000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x01000, 0x1c97d324, 1 }, //  2
	{ "c_5at_g.bin",	0x01000, 0xb9005ac0, 1 }, //  3

	{ "d2k11.spte",		0x10000, 0x2bb8d91e, 2 }, //  4 braze

	{ "s_3i_b.bin",		0x00800, 0x45a4ed06, 3 }, //  5 soundcpu
	{ "s_3j_b.bin",		0x00800, 0x4743fe92, 3 }, //  6

	{ "v_5h_b.spte",	0x00800, 0xebc5d9cc, 4 }, //  7 gfx1
	{ "v_3pt.spte",		0x00800, 0x9d881b5f, 4 }, //  8

	{ "l_4m_b.sp",		0x00800, 0x1d0b3b77, 5 }, //  9 gfx2
	{ "l_4n_b.sp",		0x00800, 0xcd717e7c, 5 }, // 10
	{ "l_4r_b.sp",		0x00800, 0xd019732b, 5 }, // 11
	{ "l_4s_b.sp",		0x00800, 0x04272273, 5 }, // 12

	{ "c-2k.sp",		0x00100, 0xa837a227, 6 }, // 13 proms
	{ "c-2j.sp",		0x00100, 0x244a89f9, 6 }, // 14
	{ "v-5e.sp",		0x00100, 0xc70b6f9b, 6 }, // 15
};

STD_ROM_PICK(dkspkyte)
STD_ROM_FN(dkspkyte)

struct BurnDriver BurnDrvDkspkyte = {
	"dkspkyte", "dkong", NULL, "dkong", "2022",
	"Donkey Kong Spooky Tournament Edition (Hack)\0", NULL, "Sock Master", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkspkyteRomInfo, dkspkyteRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongNoDipDIPInfo,
	dkongxInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong (Patch) by Don Hodges
// Patched Kill Screen - see http://donhodges.com/how_high_can_you_get.htm

static struct BurnRomInfo dkongpRomDesc[] = {
	{ "dkongp_c_5et",	0x1000, 0x2066139d, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x1000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x1000, 0x1c97d324, 1 }, //  2
	{ "c_5at_g.bin",	0x1000, 0xb9005ac0, 1 }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 }, //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 }, // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 }, // 14
};

STD_ROM_PICK(dkongp)
STD_ROM_FN(dkongp)

struct BurnDriver BurnDrvDkongp = {
	"dkongp", "dkong", NULL, "dkong", "2007",
	"Donkey Kong (Patched)\0", NULL, "Hack (Don Hodges)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongpRomInfo, dkongpRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Remix Demo by Sockmaster

static struct BurnRomInfo dkrdemoRomDesc[] = {
	{ "dkrdemo.5et",	0x1000, 0xf9fdff29, 1 }, //  0 maincpu
	{ "dkrdemo.5ct",	0x1000, 0xf48cb898, 1 }, //  1
	{ "dkrdemo.5bt",	0x1000, 0x660d43ec, 1 }, //  2
	{ "dkrdemo.5at",	0x1000, 0xe59d406c, 1 }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 }, //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 }, // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 }, // 14
};

STD_ROM_PICK(dkrdemo)
STD_ROM_FN(dkrdemo)

struct BurnDriver BurnDrvDkrdemo = {
	"dkrdemo", "dkong", NULL, "dkong", "2015",
	"Donkey Kong Remix (Demo)\0", NULL, "Hack (Sockmaster)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkrdemoRomInfo, dkrdemoRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong (Pacman Graphics) (Hack) by Tim Appleton

static struct BurnRomInfo dkongpacRomDesc[] = {
	{ "c_5et_g.bin",	0x1000, 0xba70b88b, 1 }, //  0 maincpu
	{ "c_5ct_g.bin",	0x1000, 0x5ec461ec, 1 }, //  1
	{ "c_5bt_g.bin",	0x1000, 0x1c97d324, 1 }, //  2
	{ "dkongpac.5a",	0x1000, 0x56d28137, 1 }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 }, //  5

	{ "dkongpac.3n",	0x0800, 0x1beba830, 3 }, //  6 gfx1
	{ "dkongpac.3p",	0x0800, 0x94d61766, 3 }, //  7

	{ "dkongpac.7c",	0x0800, 0x065e2713, 4 }, //  8 gfx2
	{ "dkongpac.7d",	0x0800, 0xa84b347d, 4 }, //  9
	{ "dkongpac.7e",	0x0800, 0x6ae6f476, 4 }, // 10
	{ "dkongpac.7f",	0x0800, 0x9d293922, 4 }, // 11

	{ "k2600.2k",		0x0100, 0x1e82d375, 5 }, // 12 proms
	{ "k2600.2j",		0x0100, 0x2ab01dc8, 5 }, // 13
	{ "k2600.5f",		0x0100, 0x44988665, 5 }, // 14
};

STD_ROM_PICK(dkongpac)
STD_ROM_FN(dkongpac)

struct BurnDriver BurnDrvDkongpac = {
	"dkongpac", "dkong", NULL, "dkong", "2001",
	"Donkey Kong (Pacman Graphics)\0", NULL, "Hack (Tim Appleton)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongpacRomInfo, dkongpacRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Trainer 1.01 (Hack) by Sock Master

static struct BurnRomInfo dktrainerRomDesc[] = {
	{ "dkt.5et",		0x1000, 0x7ed5a945, 1 }, //  0 maincpu
	{ "dkt.5ct",		0x1000, 0x98e2caa8, 1 }, //  1
	{ "dkt.5bt",		0x1000, 0x098a840a, 1 }, //  2
	{ "dkt.5at",		0x1000, 0xdd092591, 1 }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 }, //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 }, // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 }, // 14
};

STD_ROM_PICK(dktrainer)
STD_ROM_FN(dktrainer)

struct BurnDriver BurnDrvDktrainer = {
	"dktrainer", "dkong", NULL, "dkong", "2016",
	"Donkey Kong Trainer 1.01\0", NULL, "Hack (Sock Master)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dktrainerRomInfo, dktrainerRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Pace (Hack) by Sock Master

static struct BurnRomInfo dkpaceRomDesc[] = {
	{ "dkp.5et",		0x1000, 0xe05563d5, 1 }, //  0 maincpu
	{ "dkp.5ct",		0x1000, 0x88aa1ddf, 1 }, //  1
	{ "dkp.5bt",		0x1000, 0x8ee0b1d2, 1 }, //  2
	{ "dkp.5at",		0x1000, 0x0bc9c8db, 1 }, //  3

	{ "s_3i_b.bin",		0x0800, 0x45a4ed06, 2 }, //  4 soundcpu
	{ "s_3j_b.bin",		0x0800, 0x4743fe92, 2 }, //  5

	{ "v_5h_b.bin",		0x0800, 0x12c8c95d, 3 }, //  6 gfx1
	{ "v_3pt.bin",		0x0800, 0x15e9c5e9, 3 }, //  7

	{ "l_4m_b.bin",		0x0800, 0x59f8054d, 4 }, //  8 gfx2
	{ "l_4n_b.bin",		0x0800, 0x672e4714, 4 }, //  9
	{ "l_4r_b.bin",		0x0800, 0xfeaa59ee, 4 }, // 10
	{ "l_4s_b.bin",		0x0800, 0x20f2ef7e, 4 }, // 11

	{ "c-2k.bpr",		0x0100, 0xe273ede5, 5 }, // 12 proms
	{ "c-2j.bpr",		0x0100, 0xd6412358, 5 }, // 13
	{ "v-5e.bpr",		0x0100, 0xb869b8f5, 5 }, // 14
};

STD_ROM_PICK(dkpace)
STD_ROM_FN(dkpace)

struct BurnDriver BurnDrvDkpace = {
	"dkpace", "dkong", NULL, "dkong", "2016",
	"Donkey Kong Pace\0", NULL, "Hack (Sock Master)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkpaceRomInfo, dkpaceRomName, NULL, NULL, DkongSampleInfo, DkongSampleName, DkongInputInfo, DkongDIPInfo,
	dkongInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Junior (US set F-2)

static struct BurnRomInfo dkongjrRomDesc[] = {
	{ "djr1-c_5b_f-2.5b",	0x2000, 0xdea28158, 1 }, //  0 maincpu
	{ "djr1-c_5c_f-2.5c",	0x2000, 0x6fb5faf6, 1 }, //  1
	{ "djr1-c_5e_f-2.5e",	0x2000, 0xd042b6a8, 1 }, //  2

	{ "djr1-c_3h.3h",		0x1000, 0x715da5f8, 2 }, //  3 soundcpu

	{ "djr1-v.3n",			0x1000, 0x8d51aca9, 3 }, //  4 gfx1
	{ "djr1-v.3p",			0x1000, 0x4ef64ba5, 3 }, //  5

	{ "djr1-v_7c.7c",		0x0800, 0xdc7f4164, 4 }, //  6 gfx2
	{ "djr1-v_7d.7d",		0x0800, 0x0ce7dcf6, 4 }, //  7
	{ "djr1-v_7e.7e",		0x0800, 0x24d1ff17, 4 }, //  8
	{ "djr1-v_7f.7f",		0x0800, 0x0f8c083f, 4 }, //  9

	{ "djr1-c-2e.2e",		0x0100, 0x463dc7ad, 5 }, // 10 proms
	{ "djr1-c-2f.2f",		0x0100, 0x47ba0042, 5 }, // 11
	{ "djr1-v-2n.2n",		0x0100, 0xdbf185bf, 5 }, // 12
};

STD_ROM_PICK(dkongjr)
STD_ROM_FN(dkongjr)

static struct BurnSampleInfo DkongjrSampleDesc[] = {
#if !defined (ROM_VERIFY)
	{ "jump", SAMPLE_NOLOOP },
	{ "land", SAMPLE_NOLOOP },
	{ "roar", SAMPLE_NOLOOP },
	{ "climb0", SAMPLE_NOLOOP },
	{ "climb1", SAMPLE_NOLOOP },
	{ "climb2", SAMPLE_NOLOOP },
	{ "death", SAMPLE_NOLOOP },
	{ "drop", SAMPLE_NOLOOP },
  	{ "walk0", SAMPLE_NOLOOP },
 	{ "walk1", SAMPLE_NOLOOP },
	{ "walk2", SAMPLE_NOLOOP },
	{ "snapjaw", SAMPLE_NOLOOP },
#endif
	{ "", 0 }
};

STD_SAMPLE_PICK(Dkongjr)
STD_SAMPLE_FN(Dkongjr)

static INT32 dkongjrRomLoad()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);

	if (BurnLoadRom(tmp,  0, 1)) return 1;

	memcpy (DrvZ80ROM + 0x0000, tmp + 0x0000, 0x1000);
	memcpy (DrvZ80ROM + 0x3000, tmp + 0x1000, 0x1000);

	if (BurnLoadRom(tmp,  1, 1)) return 1;

	memcpy (DrvZ80ROM + 0x2000, tmp + 0x0000, 0x0800);
	memcpy (DrvZ80ROM + 0x4800, tmp + 0x0800, 0x0800);
	memcpy (DrvZ80ROM + 0x1000, tmp + 0x1000, 0x0800);
	memcpy (DrvZ80ROM + 0x5800, tmp + 0x1800, 0x0800);

	if (BurnLoadRom(tmp,  2, 1)) return 1;

	memcpy (DrvZ80ROM + 0x4000, tmp + 0x0000, 0x0800);
	memcpy (DrvZ80ROM + 0x2800, tmp + 0x0800, 0x0800);
	memcpy (DrvZ80ROM + 0x5000, tmp + 0x1000, 0x0800);
	memcpy (DrvZ80ROM + 0x1800, tmp + 0x1800, 0x0800);

	BurnFree (tmp);

	if (BurnLoadRom(DrvSndROM0  + 0x0000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x1000,  5, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x0000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x2000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x3000,  9, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x0000, 10, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0100, 11, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0200, 12, 1)) return 1;

	ZetOpen(0);
	ZetSetWriteHandler(dkongjr_main_write);
	ZetClose();

	return 0;
}

static void dkongjrsamplevol(INT32 sam, double vol)
{
	BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_1, vol, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_2, vol, BURN_SND_ROUTE_BOTH);
}

static INT32 dkongjrInit()
{
	INT32 rc = DrvInit(dkongjrRomLoad, 0);
	if (!rc) {
		dkongjrsamplevol(1, 0.35); // land
		dkongjrsamplevol(2, 0.35); // roar
		dkongjrsamplevol(3, 0.25); // climb
		dkongjrsamplevol(4, 0.25);
		dkongjrsamplevol(5, 0.25);
		dkongjrsamplevol(6, 0.25); // death
		dkongjrsamplevol(7, 0.35); // fall
		dkongjrsamplevol(8, 0.20); // walk
		dkongjrsamplevol(9, 0.20);
		dkongjrsamplevol(10, 0.20);
	}
	return rc;
}

struct BurnDriver BurnDrvDkongjr = {
	"dkongjr", NULL, NULL, "dkongjr", "1982",
	"Donkey Kong Junior (US set F-2)\0", NULL, "Nintendo of America", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongjrRomInfo, dkongjrRomName, NULL, NULL, DkongjrSampleInfo, DkongjrSampleName, DkongInputInfo, DkongDIPInfo,
	dkongjrInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Junior. (US, bootleg?)

static struct BurnRomInfo dkongjr2RomDesc[] = {
	{ "0",			0x2000, 0xdc1f1d12, 1 }, //  0 maincpu
	{ "1",			0x2000, 0xf1f286d0, 1 }, //  1
	{ "2",			0x2000, 0x4cb856c4, 1 }, //  2

	{ "8",			0x1000, 0x715da5f8, 2 }, //  3 soundcpu

	{ "9",			0x1000, 0x8d51aca9, 3 }, //  4 gfx1
	{ "10",			0x1000, 0x4ef64ba5, 3 }, //  5

	{ "v_7c.bin",	0x0800, 0xdc7f4164, 4 }, //  6 gfx2
	{ "v_7d.bin",	0x0800, 0x0ce7dcf6, 4 }, //  7
	{ "v_7e.bin",	0x0800, 0x24d1ff17, 4 }, //  8
	{ "v_7f.bin",	0x0800, 0x0f8c083f, 4 }, //  9

	{ "c-2e.bpr",	0x0100, 0x463dc7ad, 5 }, // 10 proms
	{ "c-2f.bpr",	0x0100, 0x47ba0042, 5 }, // 11
	{ "v-2n.bpr",	0x0100, 0xdbf185bf, 5 }, // 12
};

STD_ROM_PICK(dkongjr2)
STD_ROM_FN(dkongjr2)

static INT32 dkongjr2RomLoad()
{
	if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x2000,  1, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x4000,  2, 1)) return 1;

	if (BurnLoadRom(DrvSndROM0 + 0x0000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x1000,  5, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x0000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x2000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x3000,  9, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x0000, 10, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0100, 11, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0200, 12, 1)) return 1;

	ZetOpen(0);
	ZetSetWriteHandler(dkongjr_main_write);
	ZetClose();

	return 0;
}

static INT32 dkongjr2Init()
{
	INT32 rc = DrvInit(dkongjr2RomLoad, 0);
	if (!rc) {
		dkongjrsamplevol(1, 0.35); // land
		dkongjrsamplevol(2, 0.35); // roar
		dkongjrsamplevol(3, 0.25); // climb
		dkongjrsamplevol(4, 0.25);
		dkongjrsamplevol(5, 0.25);
		dkongjrsamplevol(6, 0.25); // death
		dkongjrsamplevol(7, 0.35); // fall
		dkongjrsamplevol(8, 0.20); // walk
		dkongjrsamplevol(9, 0.20);
		dkongjrsamplevol(10, 0.20);
	}
	return rc;
}

struct BurnDriver BurnDrvDkongjr2 = {
	"dkongjr2", "dkongjr", NULL, "dkongjr", "1982",
	"Donkey Kong Junior (US, bootleg?)\0", NULL, "Nintendo of America", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongjr2RomInfo, dkongjr2RomName, NULL, NULL, DkongjrSampleInfo, DkongjrSampleName, DkongInputInfo, DkongDIPInfo,
	dkongjr2Init, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Jr. (Japan)

static struct BurnRomInfo dkongjrjRomDesc[] = {
	{ "c_5ba.bin",	0x2000, 0x50a015ce, 1 }, //  0 maincpu
	{ "c_5ca.bin",	0x2000, 0xc0a18f0d, 1 }, //  1
	{ "c_5ea.bin",	0x2000, 0xa81dd00c, 1 }, //  2

	{ "c_3h.bin",	0x1000, 0x715da5f8, 2 }, //  3 soundcpu

	{ "v_3na.bin",	0x1000, 0xa95c4c63, 3 }, //  4 gfx1
	{ "v_3pa.bin",	0x1000, 0x4974ffef, 3 }, //  5

	{ "v_7c.bin",	0x0800, 0xdc7f4164, 4 }, //  6 gfx2
	{ "v_7d.bin",	0x0800, 0x0ce7dcf6, 4 }, //  7
	{ "v_7e.bin",	0x0800, 0x24d1ff17, 4 }, //  8
	{ "v_7f.bin",	0x0800, 0x0f8c083f, 4 }, //  9

	{ "c-2e.bpr",	0x0100, 0x463dc7ad, 5 }, // 10 proms
	{ "c-2f.bpr",	0x0100, 0x47ba0042, 5 }, // 11
	{ "v-2n.bpr",	0x0100, 0xdbf185bf, 5 }, // 12
};

STD_ROM_PICK(dkongjrj)
STD_ROM_FN(dkongjrj)

struct BurnDriver BurnDrvDkongjrj = {
	"dkongjrj", "dkongjr", NULL, "dkongjr", "1982",
	"Donkey Kong Jr. (Japan)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongjrjRomInfo, dkongjrjRomName, NULL, NULL, DkongjrSampleInfo, DkongjrSampleName, DkongInputInfo, DkongDIPInfo,
	dkongjrInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Junior (Japan set F-1)

static struct BurnRomInfo dkongjnrjRomDesc[] = {
	{ "dkjp.5b",	0x2000, 0x7b48870b, 1 }, //  0 maincpu
	{ "dkjp.5c",	0x2000, 0x12391665, 1 }, //  1
	{ "dkjp.5e",	0x2000, 0x6c9f9103, 1 }, //  2

	{ "c_3h.bin",	0x1000, 0x715da5f8, 2 }, //  3 soundcpu

	{ "dkj.3n",	0x1000, 0x8d51aca9, 3 }, //  4 gfx1
	{ "dkj.3p",	0x1000, 0x4ef64ba5, 3 }, //  5

	{ "v_7c.bin",	0x0800, 0xdc7f4164, 4 }, //  6 gfx2
	{ "v_7d.bin",	0x0800, 0x0ce7dcf6, 4 }, //  7
	{ "v_7e.bin",	0x0800, 0x24d1ff17, 4 }, //  8
	{ "v_7f.bin",	0x0800, 0x0f8c083f, 4 }, //  9

	{ "c-2e.bpr",	0x0100, 0x463dc7ad, 5 }, // 10 proms
	{ "c-2f.bpr",	0x0100, 0x47ba0042, 5 }, // 11
	{ "v-2n.bpr",	0x0100, 0xdbf185bf, 5 }, // 12
};

STD_ROM_PICK(dkongjnrj)
STD_ROM_FN(dkongjnrj)

struct BurnDriver BurnDrvDkongjnrj = {
	"dkongjnrj", "dkongjr", NULL, "dkongjr", "1982",
	"Donkey Kong Junior (Japan set F-1)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongjnrjRomInfo, dkongjnrjRomName, NULL, NULL, DkongjrSampleInfo, DkongjrSampleName, DkongInputInfo, DkongDIPInfo,
	dkongjrInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Jr. (bootleg)

static struct BurnRomInfo dkongjrbRomDesc[] = {
	{ "dkjr1",		0x2000, 0xec7e097f, 1 }, //  0 maincpu
	{ "c_5ca.bin",	0x2000, 0xc0a18f0d, 1 }, //  1
	{ "c_5ea.bin",	0x2000, 0xa81dd00c, 1 }, //  2

	{ "c_3h.bin",	0x1000, 0x715da5f8, 2 }, //  3 soundcpu

	{ "v_3na.bin",	0x1000, 0xa95c4c63, 3 }, //  4 gfx1
	{ "dkjr10",		0x1000, 0xadc11322, 3 }, //  5

	{ "v_7c.bin",	0x0800, 0xdc7f4164, 4 }, //  6 gfx2
	{ "v_7d.bin",	0x0800, 0x0ce7dcf6, 4 }, //  7
	{ "v_7e.bin",	0x0800, 0x24d1ff17, 4 }, //  8
	{ "v_7f.bin",	0x0800, 0x0f8c083f, 4 }, //  9

	{ "c-2e.bpr",	0x0100, 0x463dc7ad, 5 }, // 10 proms
	{ "c-2f.bpr",	0x0100, 0x47ba0042, 5 }, // 11
	{ "v-2n.bpr",	0x0100, 0xdbf185bf, 5 }, // 12
};

STD_ROM_PICK(dkongjrb)
STD_ROM_FN(dkongjrb)

struct BurnDriver BurnDrvDkongjrb = {
	"dkongjrb", "dkongjr", NULL, "dkongjr", "1982",
	"Donkey Kong Jr. (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongjrbRomInfo, dkongjrbRomName, NULL, NULL, DkongjrSampleInfo, DkongjrSampleName, DkongInputInfo, DkongDIPInfo,
	dkongjrInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Junior King (bootleg of Donkey Kong Jr.)

static struct BurnRomInfo jrkingRomDesc[] = {
	{ "b5.bin",	0x2000, 0xec7e097f, 1 }, //  0 maincpu
	{ "c5.bin",	0x2000, 0xc0a18f0d, 1 }, //  1
	{ "e5.bin",	0x2000, 0xa81dd00c, 1 }, //  2

	{ "h3.bin",	0x1000, 0x715da5f8, 2 }, //  3 soundcpu

	{ "n3.bin",	0x1000, 0x7110715d, 3 }, //  4 gfx1
	{ "p3.bin",	0x1000, 0x46476016, 3 }, //  5

	{ "c7.bin",	0x1000, 0x9f531527, 4 }, //  6 gfx2
	{ "d7.bin",	0x1000, 0x32fbd41b, 4 }, //  7
	{ "e7.bin",	0x1000, 0x2286bf8e, 4 }, //  8
	{ "f7.bin",	0x1000, 0x627007a0, 4 }, //  9

	{ "c-2e.bpr",	0x0100, 0x463dc7ad, 5 }, // 10 proms
	{ "c-2f.bpr",	0x0100, 0x47ba0042, 5 }, // 11
	{ "v-2n.bpr",	0x0100, 0xdbf185bf, 5 }, // 12
};

STD_ROM_PICK(jrking)
STD_ROM_FN(jrking)

struct BurnDriver BurnDrvJrking = {
	"jrking", "dkongjr", NULL, "dkongjr", "1982",
	"Junior King (bootleg of Donkey Kong Jr.)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, jrkingRomInfo, jrkingRomName, NULL, NULL, DkongjrSampleInfo, DkongjrSampleName, DkongInputInfo, DkongDIPInfo,
	dkongjrInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey King Jr. (bootleg of Donkey Kong Jr.)

static struct BurnRomInfo dkingjrRomDesc[] = {
	{ "1.7g",	0x2000, 0xbd07bb8d, 1 }, //  0 maincpu
	{ "2.7h",	0x2000, 0x01fbec11, 1 }, //  1
	{ "3.7k",	0x2000, 0xa81dd00c, 1 }, //  2

	{ "4.7l",	0x1000, 0x715da5f8, 2 }, //  3 soundcpu

	{ "5.6g",	0x1000, 0xcf14669d, 3 }, //  4 gfx1
	{ "6.6e",	0x1000, 0xcefed15e, 3 }, //  5

	{ "7.2t",	0x0800, 0xdc7f4164, 4 }, //  6 gfx2
	{ "8.2r",	0x0800, 0x0ce7dcf6, 4 }, //  7
	{ "9.2p",	0x0800, 0x24d1ff17, 4 }, //  8
	{ "10.2m",	0x0800, 0x0f8c083f, 4 }, //  9

	{ "mb7052.9k",	0x0100, 0x49f2d444, 5 }, // 10 proms
	{ "mb7052.9l",	0x0100, 0x487513ab, 5 }, // 11
	{ "mb7052.6b",	0x0100, 0xdbf185bf, 5 }, // 12

	{ "mb7051.8j",	0x0020, 0xa5a6f2ca, 5 }, // 13
};

STD_ROM_PICK(dkingjr)
STD_ROM_FN(dkingjr)

static INT32 dkingjrRomLoad()
{
	INT32 ret = dkongjrRomLoad();

	for (INT32 i = 0; i < 0x200; i++) {
		DrvColPROM[i] ^= 0xff;
	}

	return ret;
}

static INT32 dkingjrInit()
{
	return DrvInit(dkingjrRomLoad, 0);
}

struct BurnDriver BurnDrvDkingjr = {
	"dkingjr", "dkongjr", NULL, "dkongjr", "1982",
	"Donkey King Jr. (bootleg of Donkey Kong Jr.)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkingjrRomInfo, dkingjrRomName, NULL, NULL, DkongjrSampleInfo, DkongjrSampleName, DkongInputInfo, DkongDIPInfo,
	dkingjrInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Junior (E kit)

static struct BurnRomInfo dkongjreRomDesc[] = {
	{ "djr1-c.5b",	0x2000, 0xffe9e1a5, 1 }, //  0 maincpu
	{ "djr1-c.5c",	0x2000, 0x982e30e8, 1 }, //  1
	{ "djr1-c.5e",	0x2000, 0x24c3d325, 1 }, //  2

	{ "c_3h.bin",	0x1000, 0x715da5f8, 2 }, //  3 soundcpu

	{ "dkj.3n",		0x1000, 0x8d51aca9, 3 }, //  4 gfx1
	{ "dkj.3p",		0x1000, 0x4ef64ba5, 3 }, //  5

	{ "v_7c.bin",	0x0800, 0xdc7f4164, 4 }, //  6 gfx2
	{ "v_7d.bin",	0x0800, 0x0ce7dcf6, 4 }, //  7
	{ "v_7e.bin",	0x0800, 0x24d1ff17, 4 }, //  8
	{ "v_7f.bin",	0x0800, 0x0f8c083f, 4 }, //  9

	{ "c-2e.bpr",	0x0100, 0x463dc7ad, 5 }, // 10 proms
	{ "c-2f.bpr",	0x0100, 0x47ba0042, 5 }, // 11
	{ "v-2n.bpr",	0x0100, 0xdbf185bf, 5 }, // 12

	{ "djr1-c.5a",	0x1000, 0xbb5f5180, 1 }, // 13 extra cpu rom
};

STD_ROM_PICK(dkongjre)
STD_ROM_FN(dkongjre)

static INT32 dkongjreRomLoad()
{
	INT32 ret = dkongjrRomLoad();

	if (BurnLoadRom(DrvZ80ROM + 0x8000, 13, 1)) return 1;

	return ret;
}

static INT32 dkongjreInit()
{
	return DrvInit(dkongjreRomLoad, 0);
}

struct BurnDriverD BurnDrvDkongjre = {
	"dkongjre", "dkongjr", NULL, "dkongjr", "1982",
	"Donkey Kong Junior (E kit)\0", NULL, "Nintendo of America", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongjreRomInfo, dkongjreRomName, NULL, NULL, DkongjrSampleInfo, DkongjrSampleName, DkongInputInfo, DkongDIPInfo,
	dkongjreInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong Junior (P Kit, bootleg)

static struct BurnRomInfo dkongjrpbRomDesc[] = {
	{ "dkjr1-c.5b-p",	0x2000, 0x8d99b3e0, 1 }, //  0 maincpu
	{ "dkjr1-c.5c-p",	0x2000, 0xb92d258c, 1 }, //  1
	{ "dkjr1-c.5e",		0x2000, 0xd042b6a8, 1 }, //  2

	{ "c_3h.bin",	0x1000, 0x715da5f8, 2 }, //  3 soundcpu

	{ "dkj.3n",		0x1000, 0x8d51aca9, 3 }, //  4 gfx1
	{ "dkj.3p",		0x1000, 0x4ef64ba5, 3 }, //  5

	{ "v_7c.bin",	0x0800, 0xdc7f4164, 4 }, //  6 gfx2
	{ "v_7d.bin",	0x0800, 0x0ce7dcf6, 4 }, //  7
	{ "v_7e.bin",	0x0800, 0x24d1ff17, 4 }, //  8
	{ "v_7f.bin",	0x0800, 0x0f8c083f, 4 }, //  9

	{ "c-2e.bpr",	0x0100, 0x463dc7ad, 5 }, // 10 proms
	{ "c-2f.bpr",	0x0100, 0x47ba0042, 5 }, // 11
	{ "v-2n.bpr",	0x0100, 0xdbf185bf, 5 }, // 12
};

STD_ROM_PICK(dkongjrpb)
STD_ROM_FN(dkongjrpb)

struct BurnDriverD BurnDrvDkongjrpb = {
	"dkongjrpb", "dkongjr", NULL, "dkongjr", "1982",
	"Donkey Kong Junior (P kit, bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkongjrpbRomInfo, dkongjrpbRomName, NULL, NULL, DkongjrSampleInfo, DkongjrSampleName, DkongInputInfo, DkongDIPInfo,
	dkongjrInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Pest Place

static struct BurnRomInfo pestplceRomDesc[] = {
	{ "pest.1p",		0x2000, 0x80d50721, 1 }, //  0 maincpu
	{ "pest.2p",		0x2000, 0x9c3681cc, 1 }, //  1
	{ "pest.3p",		0x2000, 0x49853922, 1 }, //  2

	{ "pest.4",		0x1000, 0x715da5f8, 2 }, //  3 soundcpu

	{ "pest.o",		0x1000, 0x03939ece, 3 }, //  4 gfx1
	{ "pest.k",		0x1000, 0x2acacedf, 3 }, //  5

	{ "pest.b",		0x1000, 0xe760073e, 4 }, //  6 gfx2
	{ "pest.a",		0x1000, 0x1958346e, 4 }, //  7
	{ "pest.d",		0x1000, 0x3a993c17, 4 }, //  8
	{ "pest.c",		0x1000, 0xbf08f2a3, 4 }, //  9

	{ "n82s129a.bin",	0x0100, 0x0330f35f, 5 }, // 10 proms
	{ "n82s129b.bin",	0x0100, 0xba88311b, 5 }, // 11
	{ "sn74s288n.bin",	0x0020, 0xa5a6f2ca, 5 }, // 12

	{ "pest.0",		0x1000, 0x28952b56, 1 }, // 13
};

STD_ROM_PICK(pestplce)
STD_ROM_FN(pestplce)

static INT32 pestplceRomLoad()
{
	INT32 ret = dkongjrRomLoad();

	if (BurnLoadRom(DrvZ80ROM + 0xb000, 13, 1)) return 1;

	for (INT32 i = 0; i < 0x300; i++) { // invert colors
		DrvColPROM[i] ^= 0xff;
	}

	return ret;
}

static INT32 pestplceInit()
{
	return DrvInit(pestplceRomLoad, 1);
}

struct BurnDriver BurnDrvPestplce = {
	"pestplce", "mario", NULL, NULL, "1983",
	"Pest Place\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, pestplceRomInfo, pestplceRomName, NULL, NULL, NULL, NULL, PestplceInputInfo, PestplceDIPInfo,
	pestplceInit, DrvExit, DrvFrame, pestplceDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Donkey Kong 3 (US)

static struct BurnRomInfo dkong3RomDesc[] = {
	{ "dk3c.7b",	0x2000, 0x38d5f38e, 1 }, //  0 maincpu
	{ "dk3c.7c",	0x2000, 0xc9134379, 1 }, //  1
	{ "dk3c.7d",	0x2000, 0xd22e2921, 1 }, //  2
	{ "dk3c.7e",	0x2000, 0x615f14b7, 1 }, //  3

	{ "dk3c.5l",	0x2000, 0x7ff88885, 2 }, //  4 n2a03a

	{ "dk3c.6h",	0x2000, 0x36d7200c, 3 }, //  5 n2a03b

	{ "dk3v.3n",	0x1000, 0x415a99c7, 4 }, //  6 gfx1
	{ "dk3v.3p",	0x1000, 0x25744ea0, 4 }, //  7

	{ "dk3v.7c",	0x1000, 0x8ffa1737, 5 }, //  8 gfx2
	{ "dk3v.7d",	0x1000, 0x9ac84686, 5 }, //  9
	{ "dk3v.7e",	0x1000, 0x0c0af3fb, 5 }, // 10
	{ "dk3v.7f",	0x1000, 0x55c58662, 5 }, // 11

	{ "dkc1-c.1d",	0x0200, 0xdf54befc, 6 }, // 12 proms
	{ "dkc1-c.1c",	0x0200, 0x66a77f40, 6 }, // 13
	{ "dkc1-v.2n",	0x0100, 0x50e33434, 6 }, // 14

	{ "dkc1-v.5e",	0x0020, 0xd3e2eaf8, 7 }, // 15 adrdecode
};

STD_ROM_PICK(dkong3)
STD_ROM_FN(dkong3)

struct BurnDriver BurnDrvDkong3 = {
	"dkong3", NULL, NULL, NULL, "1983",
	"Donkey Kong 3 (US)\0", NULL, "Nintendo of America", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkong3RomInfo, dkong3RomName, NULL, NULL, NULL, NULL, Dkong3InputInfo, Dkong3DIPInfo,
	Dkong3Init, Dkong3Exit, Dkong3Frame, dkongDraw, Dkong3Scan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong 3 (Japan)

static struct BurnRomInfo dkong3jRomDesc[] = {
	{ "dk3c.7b",	0x2000, 0x38d5f38e, 1 }, //  0 maincpu
	{ "dk3c.7c",	0x2000, 0xc9134379, 1 }, //  1
	{ "dk3c.7d",	0x2000, 0xd22e2921, 1 }, //  2
	{ "dk3cj.7e",	0x2000, 0x25b5be23, 1 }, //  3

	{ "dk3c.5l",	0x2000, 0x7ff88885, 2 }, //  4 n2a03a

	{ "dk3c.6h",	0x2000, 0x36d7200c, 3 }, //  5 n2a03b

	{ "dk3v.3n",	0x1000, 0x415a99c7, 4 }, //  6 gfx1
	{ "dk3v.3p",	0x1000, 0x25744ea0, 4 }, //  7

	{ "dk3v.7c",	0x1000, 0x8ffa1737, 5 }, //  8 gfx2
	{ "dk3v.7d",	0x1000, 0x9ac84686, 5 }, //  9
	{ "dk3v.7e",	0x1000, 0x0c0af3fb, 5 }, // 10
	{ "dk3v.7f",	0x1000, 0x55c58662, 5 }, // 11

	{ "dkc1-c.1d",	0x0200, 0xdf54befc, 6 }, // 12 proms
	{ "dkc1-c.1c",	0x0200, 0x66a77f40, 6 }, // 13
	{ "dkc1-v.2n",	0x0100, 0x50e33434, 6 }, // 14

	{ "dkc1-v.5e",	0x0020, 0xd3e2eaf8, 7 }, // 15 adrdecode
};

STD_ROM_PICK(dkong3j)
STD_ROM_FN(dkong3j)

struct BurnDriver BurnDrvDkong3j = {
	"dkong3j", "dkong3", NULL, NULL, "1983",
	"Donkey Kong 3 (Japan)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkong3jRomInfo, dkong3jRomName, NULL, NULL, NULL, NULL, Dkong3InputInfo, Dkong3DIPInfo,
	Dkong3Init, Dkong3Exit, Dkong3Frame, dkongDraw, Dkong3Scan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Donkey Kong 3 (bootleg on Donkey Kong Jr. hardware)

static struct BurnRomInfo dkong3bRomDesc[] = {
	{ "5b.bin",	0x2000, 0x549979bc, 1 }, //  0 maincpu
	{ "5c-2.bin",	0x2000, 0xb9dcbae6, 1 }, //  1
	{ "5e-2.bin",	0x2000, 0x5a61868f, 1 }, //  2

	{ "3h.bin",	0x1000, 0x715da5f8, 2 }, //  3 soundcpu

	{ "3n.bin",	0x1000, 0xfed67d35, 3 }, //  4 gfx1
	{ "3p.bin",	0x1000, 0x3d1b87ce, 3 }, //  5

	{ "7c.bin",	0x1000, 0x8ffa1737, 4 }, //  6 gfx2
	{ "7d.bin",	0x1000, 0x9ac84686, 4 }, //  7
	{ "7e.bin",	0x1000, 0x0c0af3fb, 4 }, //  8
	{ "7f.bin",	0x1000, 0x55c58662, 4 }, //  9

	{ "dk3b-c.1d",	0x0200, 0xdf54befc, 5 }, // 10 proms
	{ "dk3b-c.1c",	0x0200, 0x66a77f40, 5 }, // 11
	{ "dk3b-v.2n",	0x0100, 0x50e33434, 5 }, // 12

	{ "5c-1.bin",	0x1000, 0x77a012d6, 1 }, // 13
	{ "5e-1.bin",	0x1000, 0x745ed767, 1 }, // 14
};

STD_ROM_PICK(dkong3b)
STD_ROM_FN(dkong3b)

static INT32 dkong3bRomLoad()
{
	INT32 ret = dkongjrRomLoad();

	if (BurnLoadRom(DrvGfxROM1 + 0x0000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x2000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x3000,  9, 1)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x9000, 13, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0xd000, 14, 1)) return 1;

	return ret;
}

static INT32 dkong3bInit()
{
	return DrvInit(dkong3bRomLoad, 1);
}

struct BurnDriver BurnDrvDkong3b = {
	"dkong3b", "dkong3", NULL, NULL, "1984",
	"Donkey Kong 3 (bootleg on Donkey Kong Jr. hardware)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, dkong3bRomInfo, dkong3bRomName, NULL, NULL, NULL, NULL, DkongInputInfo, Dkong3bDIPInfo,
	dkong3bInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};




// Herbie at the Olympics (DK conversion)

static struct BurnRomInfo herbiedkRomDesc[] = {
	{ "5f.cpu",		0x1000, 0xc7ab3ac6, 1 }, //  0 maincpu
	{ "5g.cpu",		0x1000, 0xd1031aa6, 1 }, //  1
	{ "5h.cpu",		0x1000, 0xc0daf551, 1 }, //  2
	{ "5k.cpu",		0x1000, 0x67442242, 1 }, //  3

	{ "3i.snd",		0x0800, 0x20e30406, 2 }, //  4 soundcpu

	{ "5h.vid",		0x0800, 0xea2a2547, 3 }, //  5 gfx1
	{ "5k.vid",		0x0800, 0xa8d421c9, 3 }, //  6

	{ "7c.clk",		0x0800, 0xaf646166, 4 }, //  7 gfx2
	{ "7d.clk",		0x0800, 0xd8e15832, 4 }, //  8
	{ "7e.clk",		0x0800, 0x2f7e65fa, 4 }, //  9
	{ "7f.clk",		0x0800, 0xad32d5ae, 4 }, // 10

	{ "74s287.2k",		0x0100, 0x7dc0a381, 5 }, // 11 proms
	{ "74s287.2j",		0x0100, 0x0a440c00, 5 }, // 12
	{ "74s287.vid",		0x0100, 0x5a3446cc, 5 }, // 13

	{ "82s147.hh",		0x0200, 0x46e5bc92, 6 }, // 14 user1

	{ "pls153h.bin",	0x00eb, 0xd6a04bcc, 7 }, // 15 plds
};

STD_ROM_PICK(herbiedk)
STD_ROM_FN(herbiedk)

static INT32 herbiedkRomLoad()
{
	if (BurnLoadRom(Drv2650ROM  + 0x0000,  0, 1)) return 1;
	if (BurnLoadRom(Drv2650ROM  + 0x2000,  1, 1)) return 1;
	if (BurnLoadRom(Drv2650ROM  + 0x4000,  2, 1)) return 1;
	if (BurnLoadRom(Drv2650ROM  + 0x6000,  3, 1)) return 1;

	if (BurnLoadRom(DrvSndROM0  + 0x0000,  4, 1)) return 1;
	memcpy (DrvSndROM0 + 0x0800, DrvSndROM0 + 0x0000, 0x0800); // re-load
	memset (DrvSndROM0 + 0x1000, 0xff, 0x0800);

	if (BurnLoadRom(DrvGfxROM0 + 0x0000,  5, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x1000,  6, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x0000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x2000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x3000, 10, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x0000, 11, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0100, 12, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0200, 13, 1)) return 1;

	if (BurnLoadRom(DrvMapROM  + 0x0000, 14, 1)) return 1;

	return 0;
}

static INT32 herbiedkInit()
{
	s2650_protection = 1;
	return s2650DkongInit(herbiedkRomLoad);
}

struct BurnDriver BurnDrvHerbiedk = {
	"herbiedk", "huncholy", NULL, NULL, "1984",
	"Herbie at the Olympics (DK conversion)\0", "No sound", "Seatongrove UK, Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, herbiedkRomInfo, herbiedkRomName, NULL, NULL, NULL, NULL, DkongInputInfo, HerbiedkDIPInfo,
	herbiedkInit, s2650DkongExit, s2650DkongFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Hunchback (DK conversion)

static struct BurnRomInfo hunchbkdRomDesc[] = {
	{ "hb.5e",		0x1000, 0x4c3ac070, 1 }, //  0 maincpu
	{ "hbsc-1.5c",		0x1000, 0x9b0e6234, 1 }, //  1
	{ "hb.5b",		0x1000, 0x4cde80f3, 1 }, //  2
	{ "hb.5a",		0x1000, 0xd60ef5b2, 1 }, //  3

	{ "hb.3h",		0x0800, 0xa3c240d4, 2 }, //  4 soundcpu

	{ "hb.3n",		0x0800, 0x443ed5ac, 3 }, //  5 gfx1
	{ "hb.3p",		0x0800, 0x073e7b0c, 3 }, //  6

	{ "hb.7c",		0x0800, 0x3ba71686, 4 }, //  7 gfx2
	{ "hb.7d",		0x0800, 0x5786948d, 4 }, //  8
	{ "hb.7e",		0x0800, 0xf845e8ca, 4 }, //  9
	{ "hb.7f",		0x0800, 0x52d20fea, 4 }, // 10

	{ "hbprom.2e",		0x0100, 0x37aab98f, 5 }, // 11 proms
	{ "hbprom.2f",		0x0100, 0x845b8dcc, 5 }, // 12
	{ "hbprom.2n",		0x0100, 0xdff9070a, 5 }, // 13

	{ "82s147.prm",		0x0200, 0x46e5bc92, 6 }, // 14 user1

	{ "pls153h.bin",	0x00eb, 0x00000000, 7 | BRF_NODUMP }, // 15 plds
};

STD_ROM_PICK(hunchbkd)
STD_ROM_FN(hunchbkd)

static INT32 hunchbkdInit()
{
	s2650_protection = 2;
	return s2650DkongInit(herbiedkRomLoad);
}

struct BurnDriverD BurnDrvHunchbkd = {
	"hunchbkd", "hunchbak", NULL, NULL, "1983",
	"Hunchback (DK conversion)\0", "No sound", "Century Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, hunchbkdRomInfo, hunchbkdRomName, NULL, NULL, NULL, NULL, DkongInputInfo, HunchbkdDIPInfo,
	hunchbkdInit, s2650DkongExit, s2650DkongFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};

/*

// Super Bike (DK conversion)

static struct BurnRomInfo sbdkRomDesc[] = {
	{ "sb-dk.ap",		0x1000, 0xfef0ef9c, 1 }, //  0 maincpu
	{ "sb-dk.ay",		0x1000, 0x2e9dade2, 1 }, //  1
	{ "sb-dk.as",		0x1000, 0xe6d200f3, 1 }, //  2
	{ "sb-dk.5a",		0x1000, 0xca41ca56, 1 }, //  3

	{ "sb-dk.3h",		0x0800, 0x13e60b6e, 2 }, //  4 soundcpu

	{ "sb-dk.3n",		0x0800, 0xb1d76b59, 3 }, //  5 gfx1
	{ "sb-dk.3p",		0x0800, 0xea5f9f88, 3 }, //  6

	{ "sb-dk.7c",		0x0800, 0xc12c18f2, 4 }, //  7 gfx2
	{ "sb-dk.7d",		0x0800, 0xf7a32d23, 4 }, //  8
	{ "sb-dk.7e",		0x0800, 0x8e48b13e, 4 }, //  9
	{ "sb-dk.7f",		0x0800, 0x989969f3, 4 }, // 10

	{ "sb.2e",		0x0100, 0x4f06f789, 5 }, // 11 proms
	{ "sb.2f",		0x0100, 0x2c15b1b2, 5 }, // 12
	{ "sb.2n",		0x0100, 0xdff9070a, 5 }, // 13

	{ "82s147.prm",		0x0200, 0x46e5bc92, 6 }, // 14 user1

	{ "pls153h.bin",	0x00eb, 0x00000000, 7 | BRF_NODUMP }, // 15 plds
};

STD_ROM_PICK(sbdk)
STD_ROM_FN(sbdk)

struct BurnDriverD BurnDrvSbdk = {
	"sbdk", "superbik", NULL, NULL, "1984",
	"Super Bike (DK conversion)\0", NULL, "Century Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, sbdkRomInfo, sbdkRomName, NULL, NULL, NULL, NULL, SbdkInputInfo, SbdkDIPInfo,
	hunchbkdInit, s2650DkongExit, s2650DkongFrame, dkongDraw, NULL, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
*/

// Hero in the Castle of Doom (DK conversion)

static struct BurnRomInfo herodkRomDesc[] = {
	{ "red-dot.rgt",	0x2000, 0x9c4af229, 1 }, //  0 maincpu
	{ "wht-dot.lft",	0x2000, 0xc10f9235, 1 }, //  1

	{ "silver.3h",		0x0800, 0x67863ce9, 2 }, //  2 soundcpu

	{ "pnk.3n",		0x0800, 0x574dfd7a, 3 }, //  3 gfx1
	{ "blk.3p",		0x0800, 0x16f7c040, 3 }, //  4

	{ "gold.7c",		0x0800, 0x5f5282ed, 4 }, //  5 gfx2
	{ "orange.7d",		0x0800, 0x075d99f5, 4 }, //  6
	{ "yellow.7e",		0x0800, 0xf6272e96, 4 }, //  7
	{ "violet.7f",		0x0800, 0xca020685, 4 }, //  8

	{ "82s129.2e",		0x0100, 0xda4b47e6, 5 }, //  9 proms
	{ "82s129.2f",		0x0100, 0x96e213a4, 5 }, // 10
	{ "82s126.2n",		0x0100, 0x37aece4b, 5 }, // 11

	{ "82s147.prm",		0x0200, 0x46e5bc92, 6 }, // 12 user1

	{ "pls153h.bin",	0x00eb, 0x00000000, 7 | BRF_NODUMP }, // 13 plds
};

STD_ROM_PICK(herodk)
STD_ROM_FN(herodk)

static INT32 herodkRomLoad()
{
	if (BurnLoadRom(Drv2650ROM + 0x0000,  0, 1)) return 1;
	if (BurnLoadRom(Drv2650ROM + 0x2000,  1, 1)) return 1;

	if (BurnLoadRom(DrvSndROM0  + 0x0000, 2, 1)) return 1;
	memcpy (DrvSndROM0 + 0x0800, DrvSndROM0 + 0x0000, 0x0800); // re-load
	memset (DrvSndROM0 + 0x1000, 0xff, 0x0800);

	if (BurnLoadRom(DrvGfxROM0 + 0x0000,  3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x1000,  4, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x0000,  5, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x2000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x3000,  8, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x0000,  9, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0100, 10, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0200, 11, 1)) return 1;

	if (BurnLoadRom(DrvMapROM  + 0x0000, 12, 1)) return 1;

	return 0;
}

static INT32 herodkLoad()
{
	if (herodkRomLoad()) return 1;

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);

	memcpy (tmp, Drv2650ROM, 0x4000);

	memcpy (Drv2650ROM + 0x0c00, tmp + 0x0000, 0x0400);
	memcpy (Drv2650ROM + 0x0800, tmp + 0x0400, 0x0400);
	memcpy (Drv2650ROM + 0x0400, tmp + 0x0800, 0x0400);
	memcpy (Drv2650ROM + 0x0000, tmp + 0x0c00, 0x0400);
	memcpy (Drv2650ROM + 0x2000, tmp + 0x1000, 0x0e00);
	memcpy (Drv2650ROM + 0x6e00, tmp + 0x1e00, 0x0200);
	memcpy (Drv2650ROM + 0x4000, tmp + 0x2000, 0x1000);
	memcpy (Drv2650ROM + 0x6000, tmp + 0x3000, 0x0e00);
	memcpy (Drv2650ROM + 0x2e00, tmp + 0x3e00, 0x0200);

	BurnFree (tmp);

	for (INT32 i = 0; i < 0x8000; i++)
	{
 		if ((i & 0x1000) == 0)
		{
			INT32 v = Drv2650ROM[i];
			Drv2650ROM[i] = (v & 0xe7) | ((v & 0x10) >> 1) | ((v & 0x08) << 1);
		}
	}

	return 0;
}

static INT32 herodkInit()
{
	s2650_protection = 2;

	return s2650DkongInit(herodkLoad);
}

struct BurnDriver BurnDrvHerodk = {
	"herodk", "hero", NULL, NULL, "1984",
	"Hero in the Castle of Doom (DK conversion)\0", "No sound", "Seatongrove UK, Ltd. (Crown license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, herodkRomInfo, herodkRomName, NULL, NULL, NULL, NULL, HerodkInputInfo, HerodkDIPInfo,
	herodkInit, s2650DkongExit, s2650DkongFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Hero in the Castle of Doom (DK conversion, not encrypted)

static struct BurnRomInfo herodkuRomDesc[] = {
	{ "2764.8h",		0x2000, 0x989ce053, 1 }, //  0 maincpu
	{ "2764.8f",		0x2000, 0x835e0074, 1 }, //  1

	{ "2716.3h",		0x0800, 0xcaf57bef, 2 }, //  2 soundcpu

	{ "pnk.3n",		0x0800, 0x574dfd7a, 3 }, //  3 gfx1
	{ "blk.3p",		0x0800, 0x16f7c040, 3 }, //  4

	{ "gold.7c",		0x0800, 0x5f5282ed, 4 }, //  5 gfx2
	{ "orange.7d",		0x0800, 0x075d99f5, 4 }, //  6
	{ "yellow.7e",		0x0800, 0xf6272e96, 4 }, //  7
	{ "violet.7f",		0x0800, 0xca020685, 4 }, //  8

	{ "82s129.2e",		0x0100, 0xda4b47e6, 5 }, //  9 proms
	{ "82s129.2f",		0x0100, 0x96e213a4, 5 }, // 10
	{ "82s126.2n",		0x0100, 0x37aece4b, 5 }, // 11

	{ "82s147.prm",		0x0200, 0x46e5bc92, 6 }, // 12 user1

	{ "pls153h.bin",	0x00eb, 0x00000000, 7 | BRF_NODUMP }, // 13 plds
};

STD_ROM_PICK(herodku)
STD_ROM_FN(herodku)

static INT32 herodkuLoad()
{
	if (herodkRomLoad()) return 1;

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);

	memcpy (tmp, Drv2650ROM, 0x4000);

	memcpy (Drv2650ROM + 0x0c00, tmp + 0x0000, 0x0400);
	memcpy (Drv2650ROM + 0x0800, tmp + 0x0400, 0x0400);
	memcpy (Drv2650ROM + 0x0400, tmp + 0x0800, 0x0400);
	memcpy (Drv2650ROM + 0x0000, tmp + 0x0c00, 0x0400);
	memcpy (Drv2650ROM + 0x2000, tmp + 0x1000, 0x1000);
	memcpy (Drv2650ROM + 0x4000, tmp + 0x2000, 0x1000);
	memcpy (Drv2650ROM + 0x6000, tmp + 0x3000, 0x1000);

	BurnFree (tmp);

	return 0;
}

static INT32 herodkuInit()
{
	s2650_protection = 2;

	return s2650DkongInit(herodkuLoad);
}

struct BurnDriver BurnDrvHerodku = {
	"herodku", "hero", NULL, NULL, "1984",
	"Hero in the Castle of Doom (DK conversion, not encrypted)\0", NULL, "Seatongrove UK, Ltd. (Crown license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, herodkuRomInfo, herodkuRomName, NULL, NULL, NULL, NULL, HerodkInputInfo, HerodkDIPInfo,
	herodkuInit, s2650DkongExit, s2650DkongFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


static void epos_bankswitch(INT32 bank)
{
	braze_bank = bank;
	ZetMapMemory(DrvZ80ROM + 0x10000 + (bank * 0x4000), 0x0000, 0x3fff, MAP_ROM);
}

static UINT8 __fastcall epos_main_read_port(UINT16 port)
{
	if (port & 0x01)
	{
		decrypt_counter = (decrypt_counter - 1) & 0x0f;
	}
	else
	{
		decrypt_counter = (decrypt_counter + 1) & 0x0f;
	}

	if (decrypt_counter >= 8 && decrypt_counter <= 0x0b) {
		epos_bankswitch(decrypt_counter & 3);
	}

	return 0;
}


static void epos_decrypt_rom(UINT8 mod, INT32 offs, INT32 *bs)
{
    UINT8 oldbyte,newbyte;
    UINT8 *ROM;
    INT32 mem;

    ROM = DrvZ80ROM;

    for (mem=0;mem<0x4000;mem++)
    {
        oldbyte = ROM[mem];

        newbyte = (oldbyte & mod) | (~oldbyte & ~mod);
        newbyte = BITSWAP08(newbyte, bs[0], bs[1], bs[2], bs[3], bs[4], bs[5], bs[6], bs[7]);

        ROM[mem + offs] = newbyte;
    }
}


static INT32 eposRomLoad()
{
	if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x2000,  1, 1)) return 1;

	if (BurnLoadRom(DrvSndROM0  + 0x0000, 2, 1)) return 1;
	memcpy (DrvSndROM0 + 0x0800, DrvSndROM0 + 0x0000, 0x0800); // re-load
	memset (DrvSndROM0 + 0x1000, 0x00, 0x0800);

	if (BurnLoadRom(DrvGfxROM0 + 0x0000,  3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x1000,  4, 1)) return 1;
	memcpy (DrvGfxROM0 + 0x0800, DrvGfxROM0 + 0x0000, 0x0800); // re-load
	memcpy (DrvGfxROM0 + 0x1800, DrvGfxROM0 + 0x1000, 0x0800);

	if (BurnLoadRom(DrvGfxROM1 + 0x0000,  5, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x2000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x3000,  8, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x0000,  9, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0100, 10, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x0200, 11, 1)) return 1;

	return 0;
}


// Drakton (DK conversion)

static struct BurnRomInfo draktonRomDesc[] = {
	{ "2764.u2",	0x2000, 0xd9a33205, 1 }, //  0 maincpu
	{ "2764.u3",	0x2000, 0x69583a35, 1 }, //  1

	{ "2716.3h",	0x0800, 0x3489a35b, 2 }, //  2 soundcpu

	{ "2716.3n",	0x0800, 0xea0e7f9a, 3 }, //  3 gfx1
	{ "2716.3p",	0x0800, 0x46f51b68, 3 }, //  4

	{ "2716.7c",	0x0800, 0x2925dc2d, 4 }, //  5 gfx2
	{ "2716.7d",	0x0800, 0xbdf6b1b4, 4 }, //  6
	{ "2716.7e",	0x0800, 0x4d62e62f, 4 }, //  7
	{ "2716.7f",	0x0800, 0x81d200e5, 4 }, //  8

	{ "82s126.2e",	0x0100, 0x3ff45f76, 5 }, //  9 proms
	{ "82s126.2f",	0x0100, 0x38f905be, 5 }, // 10
	{ "82s126.2n",	0x0100, 0x3c343b9b, 5 }, // 11
};

STD_ROM_PICK(drakton)
STD_ROM_FN(drakton)

static INT32 draktonLoad()
{
	INT32 bs[4][8] = {
		{7,6,1,3,0,4,2,5},
		{7,1,4,3,0,6,2,5},
		{7,6,1,0,3,4,2,5},
		{7,1,4,0,3,6,2,5},
	};

	if (eposRomLoad()) return 1;

	epos_decrypt_rom(0x02, 0x10000, bs[0]);
	epos_decrypt_rom(0x40, 0x14000, bs[1]);
	epos_decrypt_rom(0x8a, 0x18000, bs[2]);
	epos_decrypt_rom(0xc8, 0x1c000, bs[3]);

	return 0;
}

static INT32 draktonInit()
{
	INT32 ret = DrvInit(draktonLoad, 0);

	if (ret == 0)
	{
		ZetOpen(0);
		ZetSetInHandler(epos_main_read_port);
		epos_bankswitch(1);
		ZetReset(); // bankswitch changed vectors
		ZetClose();
	}

	draktonmode = 1;

	return ret;
}

struct BurnDriver BurnDrvDrakton = {
	"drakton", NULL, NULL, NULL, "1984",
	"Drakton (DK conversion)\0", "No sound", "Epos Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, draktonRomInfo, draktonRomName, NULL, NULL, NULL, NULL, DkongInputInfo, DraktonDIPInfo,
	draktonInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Drakton (DKJr conversion)

static struct BurnRomInfo drktnjrRomDesc[] = {
	{ "2764.u2",	0x2000, 0xd9a33205, 1 }, //  0 maincpu
	{ "2764.u3",	0x2000, 0x69583a35, 1 }, //  1

	{ "2716.3h1",	0x0800, 0x2a6ec016, 2 }, //  2 soundcpu

	{ "2716.3n",	0x0800, 0xea0e7f9a, 3 }, //  3 gfx1
	{ "2716.3p",	0x0800, 0x46f51b68, 3 }, //  4

	{ "2716.7c",	0x0800, 0x2925dc2d, 4 }, //  5 gfx2
	{ "2716.7d",	0x0800, 0xbdf6b1b4, 4 }, //  6
	{ "2716.7e",	0x0800, 0x4d62e62f, 4 }, //  7
	{ "2716.7f",	0x0800, 0x81d200e5, 4 }, //  8

	{ "82s126.2e",	0x0100, 0x3ff45f76, 5 }, //  9 proms
	{ "82s126.2f",	0x0100, 0x38f905be, 5 }, // 10
	{ "82s126.2n",	0x0100, 0x3c343b9b, 5 }, // 11
};

STD_ROM_PICK(drktnjr)
STD_ROM_FN(drktnjr)

static INT32 drktnjrInit()
{
	INT32 ret = DrvInit(draktonLoad, 0);

	if (ret == 0)
	{
		ZetOpen(0);
		ZetSetWriteHandler(dkongjr_main_write);
		ZetSetInHandler(epos_main_read_port);
		epos_bankswitch(1);
		ZetReset(); // bankswitch changed vectors
		ZetClose();
	}

	draktonmode = 1;

	return ret;
}

struct BurnDriver BurnDrvDrktnjr = {
	"drktnjr", "drakton", NULL, NULL, "1984",
	"Drakton (DKJr conversion)\0", "No sound", "Epos Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, drktnjrRomInfo, drktnjrRomName, NULL, NULL, NULL, NULL, DkongInputInfo, DraktonDIPInfo,
	drktnjrInit, DrvExit, DrvFrame, dkongDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
