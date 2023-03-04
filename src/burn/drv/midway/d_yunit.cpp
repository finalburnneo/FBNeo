// FinalBurn Neo Williams Midway Z/Y Unit driver module
// Based on MAME driver by Alex Pasadyn, Zsolt Vasvari, Ernesto Corvi, Aaron Giles

#include "tiles_generic.h"
#include "tms34_intf.h"
#include "m6809_intf.h"
#include "z80_intf.h"
#include "narc_sound.h"
#include "williams_adpcm.h"
#include "williams_cvsd.h"
#include "yawdim_sound.h"
#include "burn_ym2151.h"
#include "burn_pal.h"
#include "burn_gun.h"

/*
 to do:
 	test (clones not tested)
	strkforc keeps resetting the sound hw after boot(?) -dink
*/

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM[2];
static UINT8 *DrvVidRAM;
static UINT8 *DrvCMOSRAM;
static UINT8 *DrvMainRAM;
static UINT16 *DrvPalMAP;

static UINT16 *local_videoram; // 512 * 512 * 2
static INT32 *pen_map; // 64k entries

enum
{
	DMA_COMMAND = 0,
	DMA_ROWBYTES,
	DMA_OFFSETLO,
	DMA_OFFSETHI,
	DMA_XSTART,
	DMA_YSTART,
	DMA_WIDTH,
	DMA_HEIGHT,
	DMA_PALETTE,
	DMA_COLOR
};

struct protection_data
{
	UINT16 reset_sequence[3];
	UINT16 data_sequence[100];
};

struct dma_state_t
{
	UINT32	offset;         // source offset, in bits
	INT32	rowbytes;       // source bytes to skip each row
	INT32	xpos;           // x position, clipped
	INT32	ypos;           // y position, clipped
	INT32	width;          // horizontal pixel count
	INT32	height;         // vertical pixel count
	UINT16  palette;        // palette base
	UINT16  color;          // current foreground color with palette
};

static dma_state_t dma_state;
static UINT16 dma_register[0x10];
static INT32 cmos_page;
static INT32 videobank_select;
static INT32 autoerase_enable;
static const protection_data *prot_data = NULL;
static UINT16 prot_result;
static INT32 prot_index;
static UINT16 prot_sequence[3];
static UINT16 palette_mask;
static UINT8 cmos_w_enable;
static INT32 t2_analog_sel;

static void (*sound_write)(UINT16) = NULL;
static void (*sound_reset_write)(UINT16) = NULL;
static UINT16 (*sound_response_read)() = NULL;
static UINT16 (*sound_irq_state_read)() = NULL;
static void (*sound_reset)() = NULL;
static INT32 (*sound_scan)(INT32, INT32 *) = NULL;
static void (*sound_exit)() = NULL;
static void (*sound_update)(INT16 *, INT32 ) = NULL;
static INT32 (*sound_in_reset)() = NULL;

static UINT32 master_clock = 48000000;
static INT32 flip_screen_x = 0;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoyF[16];
static UINT8 DrvDips[3];
static UINT16 DrvInputs[6];
static UINT8 DrvReset;
static INT16 Gun[4];

static INT32 nExtraCycles = 0;

static ButtonToggle service;
static UINT8 DrvServ[1]; // service mode/diag toggle (f2)

static INT32 is_totcarn = 0;
static INT32 is_term2 = 0;
static INT32 is_mkturbo = 0;
static INT32 is_yawdim = 0;
static INT32 has_ym2151 = 0;

static INT32 vb_start = 0;
static INT32 v_total = 0;

static struct BurnInputInfo NarcInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy2 + 8,	"p1 start"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",				BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",				BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 3"	},
	{"P1 Button 4",				BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 4"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy2 + 9,	"p2 start"	},
	{"P2 Up",					BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",					BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",					BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",				BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",				BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 1"	},
	{"P2 Button 2",				BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 2"	},
	{"P2 Button 3",				BIT_DIGITAL,	DrvJoy1 + 15,	"p2 fire 3"	},
	{"P2 Button 4",				BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 4"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",					BIT_DIGITAL,	DrvJoy2 + 3,	"tilt"		},
	{"Service Mode",			BIT_DIGITAL,	DrvJoy2 + 4,    "diag"		},
	{"Dip A",					BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",					BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Narc)

static struct BurnInputInfo Mkla4InputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",				BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",				BIT_DIGITAL,	DrvJoy2 + 12,	"p1 fire 4"	},
	{"P1 Button 5",				BIT_DIGITAL,	DrvJoy2 + 13,	"p1 fire 5"	},
	{"P1 Button 6",				BIT_DIGITAL,	DrvJoy2 + 15,	"p1 fire 6"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",					BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",					BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",					BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",				BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",				BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",				BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",				BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},
	{"P2 Button 4",				BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 4"	},
	{"P2 Button 5",				BIT_DIGITAL,	DrvJoy2 + 10,	"p2 fire 5"	},
	{"P2 Button 6",				BIT_DIGITAL,	DrvJoy2 + 11,	"p2 fire 6"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",					BIT_DIGITAL,	DrvJoy2 + 6,	"service"	},
	{"Service Mode",			BIT_DIGITAL,	DrvServ + 0,    "diag"		},
	{"Tilt",					BIT_DIGITAL,	DrvJoy2 + 3,	"tilt"		},
	{"Dip A",					BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",					BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Mkla4)

static struct BurnInputInfo TrogInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",					BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",					BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",					BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",				BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",				BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},

	{"P3 Coin",					BIT_DIGITAL,	DrvJoy3 + 5,	"p3 coin"	},
	{"P3 Start",				BIT_DIGITAL,	DrvJoy2 + 9,	"p3 start"	},
	{"P3 Up",					BIT_DIGITAL,	DrvJoy2 + 11,	"p3 up"		},
	{"P3 Down",					BIT_DIGITAL,	DrvJoy2 + 12,	"p3 down"	},
	{"P3 Left",					BIT_DIGITAL,	DrvJoy2 + 13,	"p3 left"	},
	{"P3 Right",				BIT_DIGITAL,	DrvJoy2 + 14,	"p3 right"	},
	{"P3 Button 1",				BIT_DIGITAL,	DrvJoy2 + 15,	"p3 fire 1"	},

	{"P4 Start",				BIT_DIGITAL,	DrvJoy2 + 10,	"p4 start"	},
	{"P4 Up",					BIT_DIGITAL,	DrvJoy3 + 0,	"p4 up"		},
	{"P4 Down",					BIT_DIGITAL,	DrvJoy3 + 1,	"p4 down"	},
	{"P4 Left",					BIT_DIGITAL,	DrvJoy3 + 2,	"p4 left"	},
	{"P4 Right",				BIT_DIGITAL,	DrvJoy3 + 3,	"p4 right"	},
	{"P4 Button 1",				BIT_DIGITAL,	DrvJoy3 + 4,	"p4 fire 1"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",					BIT_DIGITAL,	DrvJoy2 + 6,	"service"	},
	{"Service Mode",			BIT_DIGITAL,	DrvServ + 0,    "diag"		},
	{"Tilt",					BIT_DIGITAL,	DrvJoy2 + 3,	"tilt"		},
	{"Dip A",					BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",					BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Trog)

static struct BurnInputInfo SmashtvInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Stick Up",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Stick Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Stick Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Stick Right",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Fire Up",				BIT_DIGITAL,	DrvJoy1 + 4,	"p3 up"		},
	{"P1 Fire Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p3 down"	},
	{"P1 Fire Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p3 left"	},
	{"P1 Fire Right",			BIT_DIGITAL,	DrvJoy1 + 7,	"p3 right"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Stick Up",				BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Stick Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Stick Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Stick Right",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Fire Up",				BIT_DIGITAL,	DrvJoy1 + 12,	"p4 up"		},
	{"P2 Fire Down",			BIT_DIGITAL,	DrvJoy1 + 13,	"p4 down"	},
	{"P2 Fire Left",			BIT_DIGITAL,	DrvJoy1 + 14,	"p4 left"	},
	{"P2 Fire Right",			BIT_DIGITAL,	DrvJoy1 + 15,	"p4 right"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",					BIT_DIGITAL,	DrvJoy2 + 6,	"service"	},
	{"Service Mode",			BIT_DIGITAL,	DrvJoy2 + 4,	"diag"		},
	{"Tilt",					BIT_DIGITAL,	DrvJoy2 + 3,	"tilt"		},
	{"Dip A",					BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",					BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Smashtv)

static struct BurnInputInfo HiimpactInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",					BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",					BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",					BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",				BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",				BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},

	{"P3 Coin",					BIT_DIGITAL,	DrvJoy2 + 7,	"p3 coin"	},
	{"P3 Start",				BIT_DIGITAL,	DrvJoy2 + 9,	"p3 start"	},
	{"P3 Up",					BIT_DIGITAL,	DrvJoy2 + 11,	"p3 up"		},
	{"P3 Down",					BIT_DIGITAL,	DrvJoy2 + 12,	"p3 down"	},
	{"P3 Left",					BIT_DIGITAL,	DrvJoy2 + 13,	"p3 left"	},
	{"P3 Right",				BIT_DIGITAL,	DrvJoy2 + 14,	"p3 right"	},
	{"P3 Button 1",				BIT_DIGITAL,	DrvJoy2 + 15,	"p3 fire 1"	},

	{"P4 Coin",					BIT_DIGITAL,	DrvJoy3 + 5,	"p4 coin"	},
	{"P4 Start",				BIT_DIGITAL,	DrvJoy2 + 10,	"p4 start"	},
	{"P4 Up",					BIT_DIGITAL,	DrvJoy3 + 0,	"p4 up"		},
	{"P4 Down",					BIT_DIGITAL,	DrvJoy3 + 1,	"p4 down"	},
	{"P4 Left",					BIT_DIGITAL,	DrvJoy3 + 2,	"p4 left"	},
	{"P4 Right",				BIT_DIGITAL,	DrvJoy3 + 3,	"p4 right"	},
	{"P4 Button 1",				BIT_DIGITAL,	DrvJoy3 + 4,	"p4 fire 1"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",					BIT_DIGITAL,	DrvJoy2 + 6,	"service"	},
	{"Service Mode",			BIT_DIGITAL,	DrvServ + 0,    "diag"		},
	{"Tilt",					BIT_DIGITAL,	DrvJoy2 + 3,	"tilt"		},
	{"Dip A",					BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",					BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Hiimpact)

static struct BurnInputInfo StrkforcInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",					BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",					BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",					BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",				BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},
	{"P1 Button 3",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",					BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",					BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",					BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",				BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",				BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 1"	},
	{"P2 Button 2",				BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 2"	},
	{"P2 Button 3",				BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 3"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",					BIT_DIGITAL,	DrvJoy2 + 6,	"service"	},
	{"Service Mode",			BIT_DIGITAL,	DrvServ + 0,    "diag"		},
	{"Tilt",					BIT_DIGITAL,	DrvJoy2 + 3,	"tilt"		},
	{"Dip A",					BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",					BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Strkforc)

static struct BurnInputInfo TotcarnInputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Left Stick Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Left Stick Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left Stick Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Left Stick Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Right Stick Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p3 up"		},
	{"P1 Right Stick Down",		BIT_DIGITAL,	DrvJoy1 + 5,	"p3 down"	},
	{"P1 Right Stick Left",		BIT_DIGITAL,	DrvJoy1 + 6,	"p3 left"	},
	{"P1 Right Stick Right",	BIT_DIGITAL,	DrvJoy1 + 7,	"p3 right"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoyF + 2,	"p1 fire 1"	},

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Left Stick Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Left Stick Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left Stick Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Left Stick Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Right Stick Up",		BIT_DIGITAL,	DrvJoy1 + 12,	"p4 up"		},
	{"P2 Right Stick Down",		BIT_DIGITAL,	DrvJoy1 + 13,	"p4 down"	},
	{"P2 Right Stick Left",		BIT_DIGITAL,	DrvJoy1 + 14,	"p4 left"	},
	{"P2 Right Stick Right",	BIT_DIGITAL,	DrvJoy1 + 15,	"p4 right"	},
	{"P2 Button 1",				BIT_DIGITAL,	DrvJoyF + 5,	"p2 fire 1"	},

	{"P3 Coin",					BIT_DIGITAL,	DrvJoy2 + 9,	"p3 coin"	},

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",					BIT_DIGITAL,	DrvJoy2 + 6,	"service"	},
	{"Service Mode",			BIT_DIGITAL,	DrvServ + 0,    "diag"		},
	{"Tilt",					BIT_DIGITAL,	DrvJoy2 + 3,	"tilt"		},
	{"Dip A",					BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",					BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Totcarn)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo Term2InputList[] = {
	{"P1 Coin",					BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",				BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Button 1",				BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",				BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	A("P1 Gun X",    			BIT_ANALOG_REL, &Gun[0],    	"mouse x-axis" ),
	A("P1 Gun Y",    			BIT_ANALOG_REL, &Gun[1],    	"mouse y-axis" ),

	{"P2 Coin",					BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Button 1",				BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",				BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	A("P2 Gun X",    			BIT_ANALOG_REL, &Gun[2],    	"p2 x-axis" ),
	A("P2 Gun Y",    			BIT_ANALOG_REL, &Gun[3],    	"p2 y-axis" ),

	{"Reset",					BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",					BIT_DIGITAL,	DrvJoy2 + 6,	"service"	},
	{"Service Mode",			BIT_DIGITAL,	DrvServ + 0,    "diag"		},
	{"Tilt",					BIT_DIGITAL,	DrvJoy2 + 3,	"tilt"		},
	{"Dip A",					BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",					BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",					BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};
#undef A
STDINPUTINFO(Term2)

static struct BurnDIPInfo NarcDIPList[]=
{
	DIP_OFFSET(0x17)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
};

STDDIPINFO(Narc)

static struct BurnDIPInfo Mkla4DIPList[]=
{
	DIP_OFFSET(0x1c)
	{0x00, 0xff, 0xff, 0xf8, NULL					},
	{0x01, 0xff, 0xff, 0x7d, NULL					},

	{0   , 0xfe, 0   ,    2, "Comic Book Offer"		},
	{0x00, 0x01, 0x08, 0x00, "Off"					},
	{0x00, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    2, "Attract Sound"		},
	{0x00, 0x01, 0x10, 0x00, "Off"					},
	{0x00, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Low Blows"			},
	{0x00, 0x01, 0x20, 0x00, "Off"					},
	{0x00, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Blood"				},
	{0x00, 0x01, 0x40, 0x00, "Off"					},
	{0x00, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Violence"				},
	{0x00, 0x01, 0x80, 0x00, "Off"					},
	{0x00, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    2, "Test Switch"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Counters"				},
	{0x01, 0x01, 0x02, 0x02, "One"					},
	{0x01, 0x01, 0x02, 0x00, "Two"					},

	{0   , 0xfe, 0   ,   19, "Coinage"				},
	{0x01, 0x01, 0x7c, 0x7c, "USA-1"				},
	{0x01, 0x01, 0x7c, 0x3c, "USA-2"				},
	{0x01, 0x01, 0x7c, 0x5c, "USA-3"				},
	{0x01, 0x01, 0x7c, 0x1c, "USA-4"				},
	{0x01, 0x01, 0x7c, 0x6c, "USA-ECA"				},
	{0x01, 0x01, 0x7c, 0x0c, "USA-Free Play"		},
	{0x01, 0x01, 0x7c, 0x74, "German-1"				},
	{0x01, 0x01, 0x7c, 0x34, "German-2"				},
	{0x01, 0x01, 0x7c, 0x54, "German-3"				},
	{0x01, 0x01, 0x7c, 0x14, "German-4"				},
	{0x01, 0x01, 0x7c, 0x64, "German-5"				},
	{0x01, 0x01, 0x7c, 0x24, "German-ECA"			},
	{0x01, 0x01, 0x7c, 0x04, "German-Free Play"		},
	{0x01, 0x01, 0x7c, 0x78, "French-1"				},
	{0x01, 0x01, 0x7c, 0x38, "French-2"				},
	{0x01, 0x01, 0x7c, 0x58, "French-3"				},
	{0x01, 0x01, 0x7c, 0x18, "French-4"				},
	{0x01, 0x01, 0x7c, 0x68, "French-ECA"			},
	{0x01, 0x01, 0x7c, 0x08, "French-Free Play"		},

	{0   , 0xfe, 0   ,    2, "Coinage Source"		},
	{0x01, 0x01, 0x80, 0x80, "Dipswitch"			},
	{0x01, 0x01, 0x80, 0x00, "CMOS"					},
};

STDDIPINFO(Mkla4)

static struct BurnDIPInfo TrogDIPList[]=
{
	DIP_OFFSET(0x1f)
	{0x00, 0xff, 0xff, 0xbf, NULL					},
	{0x01, 0xff, 0xff, 0xcf, NULL					},

	{0   , 0xfe, 0   ,    6, "Coinage"				},
	{0x00, 0x01, 0x38, 0x38, "1"					},
	{0x00, 0x01, 0x38, 0x18, "2"					},
	{0x00, 0x01, 0x38, 0x28, "3"					},
	{0x00, 0x01, 0x38, 0x08, "4"					},
	{0x00, 0x01, 0x38, 0x30, "ECA"					},
	{0x00, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Coinage Source"		},
	{0x00, 0x01, 0x40, 0x40, "Dipswitch"			},
	{0x00, 0x01, 0x40, 0x00, "CMOS"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x80, 0x80, "Upright"				},
	{0x00, 0x01, 0x80, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Test Switch"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Video Freeze"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Players"				},
	{0x01, 0x01, 0x0c, 0x0c, "4 Players"			},
	{0x01, 0x01, 0x0c, 0x04, "3 Players"			},
	{0x01, 0x01, 0x0c, 0x08, "2 Players"			},
	{0x01, 0x01, 0x0c, 0x00, "1 Player"				},

	{0   , 0xfe, 0   ,    2, "Coin Counters"		},
	{0x01, 0x01, 0x10, 0x10, "One Counter"			},
	{0x01, 0x01, 0x10, 0x00, "Two Counters"			},

	{0   , 0xfe, 0   ,    2, "Powerup Test"			},
	{0x01, 0x01, 0x20, 0x00, "Off"					},
	{0x01, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    3, "Country"				},
	{0x01, 0x01, 0xc0, 0xc0, "USA"					},
	{0x01, 0x01, 0xc0, 0x80, "French"				},
	{0x01, 0x01, 0xc0, 0x40, "German"				},
};

STDDIPINFO(Trog)


static struct BurnDIPInfo HiimpactDIPList[]=
{
	DIP_OFFSET(0x20)
	{0x00, 0xff, 0xff, 0xf8, NULL					},
	{0x01, 0xff, 0xff, 0xf1, NULL					},

	{0   , 0xfe, 0   ,    17, "Coinage"				},
	{0x00, 0x01, 0x78, 0x78, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x78, 0x58, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x78, 0x68, "2 Coins/1 Credit 4/3"	},
	{0x00, 0x01, 0x78, 0x48, "2 Coins/1 Credit 4/4"	},
	{0x00, 0x01, 0x78, 0x70, "ECA"					},
	{0x00, 0x01, 0x78, 0x78, "1DM/1 Credit 6/5"		},
	{0x00, 0x01, 0x78, 0x58, "1DM/1 Credit 7/5"		},
	{0x00, 0x01, 0x78, 0x68, "1DM/1 Credit 8/5"		},
	{0x00, 0x01, 0x78, 0x48, "1DM/1 Credit"			},
	{0x00, 0x01, 0x78, 0x70, "ECA"					},
	{0x00, 0x01, 0x78, 0x78, "5F/2 Credits 10/5"	},
	{0x00, 0x01, 0x78, 0x58, "5F/2 Credits"			},
	{0x00, 0x01, 0x78, 0x68, "5F/1 Credit 10/3"		},
	{0x00, 0x01, 0x78, 0x48, "5F/1 Credit"			},
	{0x00, 0x01, 0x78, 0x70, "ECA"					},
	{0x00, 0x01, 0x78, 0x40, "Free Play"			},
	{0x00, 0x01, 0x78, 0x38, "Other (Service Menu)"	},

	{0   , 0xfe, 0   ,    2, "Players"				},
	{0x00, 0x01, 0x80, 0x80, "4"					},
	{0x00, 0x01, 0x80, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Test Switch"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Coin Counters"		},
	{0x01, 0x01, 0x10, 0x10, "1"					},
	{0x01, 0x01, 0x10, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Power-Up Test"		},
	{0x01, 0x01, 0x20, 0x00, "Off"					},
	{0x01, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    3, "Country"				},
	{0x01, 0x01, 0xc0, 0xc0, "USA"					},
	{0x01, 0x01, 0xc0, 0x40, "German"				},
	{0x01, 0x01, 0xc0, 0x80, "French"				},
};

STDDIPINFO(Hiimpact)

static struct BurnDIPInfo ShimpactDIPList[]=
{
	DIP_OFFSET(0x20)
	{0x00, 0xff, 0xff, 0xf8, NULL					},
	{0x01, 0xff, 0xff, 0xf1, NULL					},

	{0   , 0xfe, 0   ,    2, "Card Dispenser"		}, // only diff from Hiimpact
	{0x00, 0x01, 0x01, 0x01, "On"					},
	{0x00, 0x01, 0x01, 0x00, "Off"					},

	{0   , 0xfe, 0   ,    17, "Coinage"				},
	{0x00, 0x01, 0x78, 0x78, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x78, 0x58, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x78, 0x68, "2 Coins/1 Credit 4/3"	},
	{0x00, 0x01, 0x78, 0x48, "2 Coins/1 Credit 4/4"	},
	{0x00, 0x01, 0x78, 0x70, "ECA"					},
	{0x00, 0x01, 0x78, 0x78, "1DM/1 Credit 6/5"		},
	{0x00, 0x01, 0x78, 0x58, "1DM/1 Credit 7/5"		},
	{0x00, 0x01, 0x78, 0x68, "1DM/1 Credit 8/5"		},
	{0x00, 0x01, 0x78, 0x48, "1DM/1 Credit"			},
	{0x00, 0x01, 0x78, 0x70, "ECA"					},
	{0x00, 0x01, 0x78, 0x78, "5F/2 Credits 10/5"	},
	{0x00, 0x01, 0x78, 0x58, "5F/2 Credits"			},
	{0x00, 0x01, 0x78, 0x68, "5F/1 Credit 10/3"		},
	{0x00, 0x01, 0x78, 0x48, "5F/1 Credit"			},
	{0x00, 0x01, 0x78, 0x70, "ECA"					},
	{0x00, 0x01, 0x78, 0x40, "Free Play"			},
	{0x00, 0x01, 0x78, 0x38, "Other (Service Menu)"	},

	{0   , 0xfe, 0   ,    2, "Players"				},
	{0x00, 0x01, 0x80, 0x80, "4"					},
	{0x00, 0x01, 0x80, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Test Switch"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Coin Counters"		},
	{0x01, 0x01, 0x10, 0x10, "1"					},
	{0x01, 0x01, 0x10, 0x00, "2"					},

	{0   , 0xfe, 0   ,    2, "Power-Up Test"		},
	{0x01, 0x01, 0x20, 0x00, "Off"					},
	{0x01, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    3, "Country"				},
	{0x01, 0x01, 0xc0, 0xc0, "USA"					},
	{0x01, 0x01, 0xc0, 0x40, "German"				},
	{0x01, 0x01, 0xc0, 0x80, "French"				},
};

STDDIPINFO(Shimpact)

static struct BurnDIPInfo SmashtvDIPList[]=
{
	DIP_OFFSET(0x18)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Smashtv)

static struct BurnDIPInfo StrkforcDIPList[]=
{
	DIP_OFFSET(0x16)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Coin Meter"			},
	{0x00, 0x01, 0x01, 0x01, "Shared"				},
	{0x00, 0x01, 0x01, 0x00, "Independent"			},

	{0   , 0xfe, 0   ,    2, "Credits to Start"		},
	{0x00, 0x01, 0x02, 0x02, "1"					},
	{0x00, 0x01, 0x02, 0x00, "2"					},

	{0   , 0xfe, 0   ,    4, "Points for Ship"		},
	{0x00, 0x01, 0x0c, 0x08, "40000"				},
	{0x00, 0x01, 0x0c, 0x0c, "50000"				},
	{0x00, 0x01, 0x0c, 0x04, "75000"				},
	{0x00, 0x01, 0x0c, 0x00, "100000"				},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x00, 0x01, 0x10, 0x10, "3"					},
	{0x00, 0x01, 0x10, 0x00, "4"					},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x00, 0x01, 0xe0, 0x80, "Level 1"				},
	{0x00, 0x01, 0xe0, 0xc0, "Level 2"				},
	{0x00, 0x01, 0xe0, 0xa0, "Level 3"				},
	{0x00, 0x01, 0xe0, 0xe0, "Level 4"				},
	{0x00, 0x01, 0xe0, 0x60, "Level 5"				},
	{0x00, 0x01, 0xe0, 0x40, "Level 6"				},
	{0x00, 0x01, 0xe0, 0x20, "Level 7"				},
	{0x00, 0x01, 0xe0, 0x00, "Level 8"				},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x01, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x01, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x01, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x01, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x01, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},
	{0x01, 0x01, 0x07, 0x02, "1 Coin  6 Credits"	},
	{0x01, 0x01, 0x07, 0x01, "U.K. Elect."			},
	{0x01, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x01, 0x01, 0x78, 0x30, "5 Coins 1 Credits"	},
	{0x01, 0x01, 0x78, 0x38, "4 Coins 1 Credits"	},
	{0x01, 0x01, 0x78, 0x40, "3 Coins 1 Credits"	},
	{0x01, 0x01, 0x78, 0x48, "2 Coins 1 Credits"	},
	{0x01, 0x01, 0x78, 0x78, "1 Coin  1 Credits"	},
	{0x01, 0x01, 0x78, 0x70, "1 Coin  2 Credits"	},
	{0x01, 0x01, 0x78, 0x68, "1 Coin  3 Credits"	},
	{0x01, 0x01, 0x78, 0x60, "1 Coin  4 Credits"	},
	{0x01, 0x01, 0x78, 0x58, "1 Coin  5 Credits"	},
	{0x01, 0x01, 0x78, 0x50, "1 Coin  6 Credits"	},
	{0x01, 0x01, 0x78, 0x28, "1 Coin/1 Credit - 2 Coins/3 Credits"	},
	{0x01, 0x01, 0x78, 0x20, "1 Coin/1 Credit - 3 Coins/4 Credits"	},
	{0x01, 0x01, 0x78, 0x18, "1 Coin/1 Credit - 4 Coins/5 Credits"	},
	{0x01, 0x01, 0x78, 0x10, "1 Coin/1 Credit - 5 Coins/6 Credits"	},
	{0x01, 0x01, 0x78, 0x08, "3 Coin/1 Credit - 5 Coins/2 Credits"	},
	{0x01, 0x01, 0x78, 0x00, "1 Coin/2 Credits - 2 Coins/5 Credits"	},

	{0   , 0xfe, 0   ,    2, "Test Switch"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Strkforc)

static struct BurnDIPInfo TotcarnDIPList[]=
{
	DIP_OFFSET(0x1b)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    27, "Coinage"				},
	{0x00, 0x01, 0x1f, 0x1f, "USA 1"				},
	{0x00, 0x01, 0x1f, 0x1e, "USA 2"				},
	{0x00, 0x01, 0x1f, 0x1d, "USA 3"				},
	{0x00, 0x01, 0x1f, 0x1c, "German 1"				},
	{0x00, 0x01, 0x1f, 0x1b, "German 2"				},
	{0x00, 0x01, 0x1f, 0x1a, "German 3"				},
	{0x00, 0x01, 0x1f, 0x19, "France 2"				},
	{0x00, 0x01, 0x1f, 0x18, "France 3"				},
	{0x00, 0x01, 0x1f, 0x17, "France 4"				},
	{0x00, 0x01, 0x1f, 0x16, "Swiss 1"				},
	{0x00, 0x01, 0x1f, 0x15, "Italy"				},
	{0x00, 0x01, 0x1f, 0x14, "U.K. 1"				},
	{0x00, 0x01, 0x1f, 0x13, "U.K. 2"				},
	{0x00, 0x01, 0x1f, 0x12, "U.K. ECA"				},
	{0x00, 0x01, 0x1f, 0x11, "Spain 1"				},
	{0x00, 0x01, 0x1f, 0x10, "Australia 1"			},
	{0x00, 0x01, 0x1f, 0x0f, "Japan 1"				},
	{0x00, 0x01, 0x1f, 0x0e, "Japan 2"				},
	{0x00, 0x01, 0x1f, 0x0d, "Austria 1"			},
	{0x00, 0x01, 0x1f, 0x0c, "Belgium 1"			},
	{0x00, 0x01, 0x1f, 0x0b, "Belgium 2"			},
	{0x00, 0x01, 0x1f, 0x0a, "Sweden"				},
	{0x00, 0x01, 0x1f, 0x09, "New Zealand 1"		},
	{0x00, 0x01, 0x1f, 0x08, "Netherlands"			},
	{0x00, 0x01, 0x1f, 0x07, "Finland"				},
	{0x00, 0x01, 0x1f, 0x06, "Norway"				},
	{0x00, 0x01, 0x1f, 0x05, "Denmark"				},

	{0   , 0xfe, 0   ,    2, "Dipswitch Coinage"	},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Keys for Pleasure Dome"},
	{0x00, 0x01, 0x80, 0x80, "220"					},
	{0x00, 0x01, 0x80, 0x00, "200"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},
};

STDDIPINFO(Totcarn)

static struct BurnDIPInfo Term2DIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xfb, NULL					},
	{0x01, 0xff, 0xff, 0xcf, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    8, "Credits"				},
	{0x00, 0x01, 0x07, 0x07, "2 Start/1 Continue"	},
	{0x00, 0x01, 0x07, 0x06, "4 Start/1 Continue"	},
	{0x00, 0x01, 0x07, 0x05, "2 Start/2 Continue"	},
	{0x00, 0x01, 0x07, 0x04, "4 Start/2 Continue"	},
	{0x00, 0x01, 0x07, 0x03, "1 Start/1 Continue"	},
	{0x00, 0x01, 0x07, 0x02, "3 Start/2 Continue"	},
	{0x00, 0x01, 0x07, 0x01, "3 Start/1 Continue"	},
	{0x00, 0x01, 0x07, 0x00, "3 Start/3 Continue"	},

	{0   , 0xfe, 0   ,    6, "Coinage"				},
	{0x00, 0x01, 0x38, 0x38, "1"					},
	{0x00, 0x01, 0x38, 0x18, "2"					},
	{0x00, 0x01, 0x38, 0x28, "3"					},
	{0x00, 0x01, 0x38, 0x08, "4"					},
	{0x00, 0x01, 0x38, 0x30, "ECA"					},
	{0x00, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Dipswitch Coinage"	},
	{0x00, 0x01, 0x40, 0x00, "Off"					},
	{0x00, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Normal Display"		},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Test Switch"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Video Freeze"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Unused"				},
	{0x01, 0x01, 0x04, 0x04, "Off"					},
	{0x01, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Players"				},
	{0x01, 0x01, 0x08, 0x08, "2 Players"			},
	{0x01, 0x01, 0x08, 0x00, "1 Player"				},

	{0   , 0xfe, 0   ,    2, "Two Counters"			},
	{0x01, 0x01, 0x10, 0x10, "Off"					},
	{0x01, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Powerup Test"			},
	{0x01, 0x01, 0x20, 0x00, "Off"					},
	{0x01, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    3, "Country"				},
	{0x01, 0x01, 0xc0, 0xc0, "USA"					},
	{0x01, 0x01, 0xc0, 0x80, "French"				},
	{0x01, 0x01, 0xc0, 0x40, "German"				},

	{0   , 0xfe, 0   ,    2, "Emulated Crosshair"	},
	{0x02, 0x01, 0x01, 0x00, "Off"					},
	{0x02, 0x01, 0x01, 0x01, "On"					},
};

STDDIPINFO(Term2)

static void midyunit_vram_write(UINT32 address, UINT16 data)
{
	INT32 offset = (address & 0x1fffff) >> 3; // (offset >> 4) << 1; - *2

	if (videobank_select)
	{
		local_videoram[offset+0] = (data & 0x00ff) | (dma_register[DMA_PALETTE] << 8);
		local_videoram[offset+1] = (data >> 8)     | (dma_register[DMA_PALETTE] & 0xff00);
	}
	else
	{
		local_videoram[offset+0] = (local_videoram[offset+0] & 0x00ff) | (data << 8);
		local_videoram[offset+1] = (local_videoram[offset+1] & 0x00ff) | (data & 0xff00);
	}
}

static UINT16 midyunit_vram_read(UINT32 address)
{
	INT32 offset = (address & 0x1fffff) >> 3; // (offset >> 4) << 1; - *2

	if (videobank_select)
	{
		return (local_videoram[offset + 0] & 0x00ff) | (local_videoram[offset + 1] << 8);
	}
	else
	{
		return (local_videoram[offset + 0] >> 8) | (local_videoram[offset + 1] & 0xff00);
	}
}

static UINT16 midyunit_mkyturbo_hack(UINT32 address)
{
	if ((address & 0xfffffff0) == 0xfffff400) {
		return BurnRandom() & 0xffff;
	}
	INT32 offset = (address & 0x7fffff) >> 3;
	return *((UINT16*)(DrvMainROM + offset));
}

static void midyunit_term2_hack(UINT32 address, UINT16 data)
{
	INT32 offset = (address & 0xfffff) >> 3;
	*((UINT16*)(DrvMainRAM + offset)) = data;

	if ((address == 0x010aa0e0 || address == 0x010aa0f0) && (TMS34010GetPC() & 0xffff0000) == 0xffce0000) {
		if ((address == 0x010aa0f0 && TMS34010GetPC() == 0xffce6520) ||
		    (address == 0x010aa0e0 && TMS34010GetPC() == 0xffce5230) ||
		    (address == 0x010aa0e0 && TMS34010GetPC() == 0xffce4b80) ||
			(address == 0x010aa0e0 && TMS34010GetPC() == 0xffce33f0) )
		{
			bprintf(0, _T("t2hack anti-freeze  %x  %x\tPC: %X\n"), address, data, TMS34010GetPC());
			*((UINT16*)(DrvMainRAM + offset)) = 0x0000;
		}
	}
}

static void midyunit_palette_write(UINT32 address, UINT16 data)
{
	INT32 offset = (address & 0x1fff0) >> 3;
	*((UINT16*)(BurnPalRAM + offset)) = data;

	BurnPalette[(offset / 2) & palette_mask] = BurnHighCol(pal5bit(data >> 10), pal5bit(data >> 5), pal5bit(data), 0);

	// Palette ram is written to kindof random, relying on the mask to put it in
	// the right slot.  since we can't derive the palette from the ram itself,
	// let's keep track of it here:    -dink jan27 2021
	DrvPalMAP[(offset / 2) & palette_mask] = data;
}

static void DrvRecalculatePalette()
{
	UINT16 *ram = (UINT16*)DrvPalMAP;

	for (INT32 i = 0; i < (palette_mask + 1); i++)
	{
		 BurnPalette[i] = BurnHighCol(pal5bit(ram[i] >> 10), pal5bit(ram[i] >> 5), pal5bit(ram[i]), 0);
	}
}

static void cmos_enable_write(UINT32 , UINT16 data)
{
	cmos_w_enable = (~data >> 9) & 1;

	if (prot_data)
	{
		data &= 0x0f00;
		prot_sequence[0] = prot_sequence[1];
		prot_sequence[1] = prot_sequence[2];
		prot_sequence[2] = data;

		if (prot_data->reset_sequence[0] == 0x1234)
		{
			if (data == 0x500)
			{
				prot_result = TMS34010ReadWord(0x10a4390 >> 3) << 4;

				//bprintf(0, _T("---- desired result PROT  %x\n"), prot_result);
			}
		}
		else
		{
			if (prot_sequence[0] == prot_data->reset_sequence[0] && prot_sequence[1] == prot_data->reset_sequence[1] && prot_sequence[2] == prot_data->reset_sequence[2])
			{
				prot_index = 0;
			}

			if ((prot_sequence[1] & 0x0800) != 0 && (prot_sequence[2] & 0x0800) == 0)
			{
				prot_result = prot_data->data_sequence[prot_index++];
			}
		}
	}
}

static void cmos_bankswitch()
{
	TMS34010MapMemory(DrvCMOSRAM + cmos_page,		0x01400000, 0x0140ffff, MAP_READ | MAP_WRITE);
}

static void dma_draw(UINT16 command)
{
	int dx = (command & 0x10) ? -1 : 1;
	int height = dma_state.height;
	int width = dma_state.width;
	UINT8 *base = DrvGfxROM;
	UINT32 offset = dma_state.offset >> 3;
	UINT16 pal = dma_state.palette;
	UINT16 color = pal | dma_state.color;
	int x, y;

	/* we only need the low 4 bits of the command */
	command &= 0x0f;

	/* loop over the height */
	for (y = 0; y < height; y++)
	{
		int tx = dma_state.xpos;
		int ty = dma_state.ypos;
		UINT32 o = offset;
		UINT16 *dest;

		/* determine Y position */
		ty = (ty + y) & 0x1ff;
		offset += dma_state.rowbytes;

		/* determine destination pointer */
		dest = &local_videoram[ty * 512];

		/* check for overruns if they are relevant */
		if (o >= 0x06000000 && command < 0x0c)
			continue;

		/* switch off the zero/non-zero options */
		switch (command)
		{
			case 0x00:  /* draw nothing */
				break;

			case 0x01:  /* draw only 0 pixels */
				for (x = 0; x < width; x++, tx += dx)
					if (base[o++] == 0)
						dest[tx] = pal;
				break;

			case 0x02:  /* draw only non-0 pixels */
				for (x = 0; x < width; x++, tx += dx)
				{
					int pixel = base[o++];
					if (pixel != 0)
						dest[tx] = pal | pixel;
				}
				break;

			case 0x03:  /* draw all pixels */
				for (x = 0; x < width; x++, tx += dx)
					dest[tx] = pal | base[o++];
				break;

			case 0x04:  /* color only 0 pixels */
			case 0x05:  /* color only 0 pixels */
				for (x = 0; x < width; x++, tx += dx)
					if (base[o++] == 0)
						dest[tx] = color;
				break;

			case 0x06:  /* color only 0 pixels, copy the rest */
			case 0x07:  /* color only 0 pixels, copy the rest */
				for (x = 0; x < width; x++, tx += dx)
				{
					int pixel = base[o++];
					dest[tx] = (pixel == 0) ? color : (pal | pixel);
				}
				break;

			case 0x08:  /* color only non-0 pixels */
			case 0x0a:  /* color only non-0 pixels */
				for (x = 0; x < width; x++, tx += dx)
					if (base[o++] != 0)
						dest[tx] = color;
				break;

			case 0x09:  /* color only non-0 pixels, copy the rest */
			case 0x0b:  /* color only non-0 pixels, copy the rest */
				for (x = 0; x < width; x++, tx += dx)
				{
					int pixel = base[o++];
					dest[tx] = (pixel != 0) ? color : (pal | pixel);
				}
				break;

			case 0x0c:  /* color all pixels */
			case 0x0d:  /* color all pixels */
			case 0x0e:  /* color all pixels */
			case 0x0f:  /* color all pixels */
				for (x = 0; x < width; x++, tx += dx)
					dest[tx] = color;
				break;
		}
	}
}

static void dma_callback()
{
	dma_register[DMA_COMMAND] &= ~0x8000; /* tell the cpu we're done */
	TMS34010GenerateIRQ(0);
}

static void dma_write(UINT32 offset, UINT16 )
{
	UINT32 gfxoffset;
	int command;

	/* only writes to DMA_COMMAND actually cause actions */
	if (offset != DMA_COMMAND)
		return;

	/* high bit triggers action */
	command = dma_register[DMA_COMMAND];
	TMS34010ClearIRQ(0);

	if (!(command & 0x8000))
		return;

	/* fill in the basic data */
	dma_state.rowbytes = (INT16)dma_register[DMA_ROWBYTES];
	dma_state.xpos = (INT16)dma_register[DMA_XSTART];
	dma_state.ypos = (INT16)dma_register[DMA_YSTART];
	dma_state.width = dma_register[DMA_WIDTH];
	dma_state.height = dma_register[DMA_HEIGHT];
	dma_state.palette = dma_register[DMA_PALETTE] << 8;
	dma_state.color = dma_register[DMA_COLOR] & 0xff;

	gfxoffset = dma_register[DMA_OFFSETLO] | (dma_register[DMA_OFFSETHI] << 16);
	if (command & 0x10)
	{
		if (!is_yawdim)
		{
			gfxoffset -= (dma_state.width - 1) * 8;
			dma_state.rowbytes = (dma_state.rowbytes - dma_state.width + 3) & ~3;
		}
		else
			dma_state.rowbytes = (dma_state.rowbytes + dma_state.width + 3) & ~3;
		dma_state.xpos += dma_state.width - 1;
	}
	else
		dma_state.rowbytes = (dma_state.rowbytes + dma_state.width + 3) & ~3;

	/* apply Y clipping */
	if (dma_state.ypos < 0)
	{
		dma_state.height -= -dma_state.ypos;
		dma_state.offset += (-dma_state.ypos * dma_state.rowbytes) << 3;
		dma_state.ypos = 0;
	}
	if (dma_state.ypos + dma_state.height > 512)
		dma_state.height = 512 - dma_state.ypos;

	/* apply X clipping */
	if (!(command & 0x10))
	{
		if (dma_state.xpos < 0)
		{
			dma_state.width -= -dma_state.xpos;
			dma_state.offset += -dma_state.xpos << 3;
			dma_state.xpos = 0;
		}
		if (dma_state.xpos + dma_state.width > 512)
			dma_state.width = 512 - dma_state.xpos;
	}
	else
	{
		if (dma_state.xpos >= 512)
		{
			dma_state.width -= dma_state.xpos - 511;
			dma_state.offset += (dma_state.xpos - 511) << 3;
			dma_state.xpos = 511;
		}
		if (dma_state.xpos - dma_state.width < 0)
			dma_state.width = dma_state.xpos;
	}

	/* determine the location and draw */
	if (gfxoffset < 0x02000000)
		gfxoffset += 0x02000000;
	{
		dma_state.offset = gfxoffset - 0x02000000;
		dma_draw(command);
	}

	// dma timer controls game speed -dink
	TMS34010TimerSet(((double)((double)master_clock/8/1000000000) * (41 * dma_state.width * dma_state.height)));
}

static void sync_sound()
{
	if (is_yawdim) {
		INT32 cyc = (TMS34010TotalCycles() * 4000000 / (master_clock / 8)) - ZetTotalCycles(0);
		if (cyc > 0) {
			ZetRun(0, cyc);
		}
		return;
	}

	INT32 cyc = (TMS34010TotalCycles() * 2000000 / (master_clock / 8)) - M6809TotalCycles(0);
	if (cyc > 0) {
		M6809Run(0, cyc);
	}
	if (palette_mask == 0x1fff) {
		// NARC 2x m6809!
		cyc = (TMS34010TotalCycles() * 2000000 / (master_clock / 8)) - M6809TotalCycles(1);
		if (cyc > 0) {
			M6809Run(1, cyc);
		}
	}
}

static void midyunit_main_write(UINT32 address, UINT16 data)
{
	if ((address & 0xfff7ff00) == 0x1a00000) {
		INT32 offset = (address >> 4) & 0xf;
		dma_register[offset] = data;
		dma_write(offset, data);
		return;
	}

	if (address >= 0x1820000 && address <= 0x1a1eff0) return; // nop - many games write to this entire range after post... why?

	if ((address & 0xffffffe0) == 0x1c00060) {
		INT32 offset = (address >> 4) & 0x1;
		cmos_enable_write(offset, data);
		return;
	}

	if ((address & 0xffffffe0) == 0x1e00000) {
		INT32 offset = (address >> 4) & 0x1;
		sync_sound();
		if (sound_reset_write) sound_reset_write((~data & 0x100) >> 8);
		if (sound_write) sound_write(data);
		if (offset == 0) t2_analog_sel = (data & 0x3000) >> 12;
		return;
	}

	if ((address & 0xffffffe0) == 0x1f00000) {
		cmos_page = ((data >> 6) & 3) * 0x2000; // 0x1000 word page, (0x2000 in bytes)
		cmos_bankswitch();
		videobank_select = (data >> 5) & 1;
		autoerase_enable = ((data & 0x10) == 0);
		return;
	}
}

static UINT16 midyunit_main_read(UINT32 address)
{
	if ((address & 0xfff7ff00) == 0x1a00000) {
		INT32 offset = (address >> 4) & 0xf;
		return dma_register[offset];
	}

	// inputs & prot
	if ((address & 0xffffff80) == 0x1c00000) {
		INT32 offset = (address >> 4) & 0x7;
		UINT16 rv = 0xffff;
		switch (offset)
		{
			case 0:
				rv = DrvInputs[0];
				break;

			case 1:
			{
				UINT16 ret = DrvInputs[1];
				sync_sound();
				if (sound_response_read) ret = (ret & ~0x0400) | (sound_response_read() & 0x100) << 2; // narc
				if (sound_irq_state_read) ret = (ret & ~0x4000) | (sound_irq_state_read() ? 0 : 0x4000); // adpcm
				rv = ret;
				break;
			}

			case 2:
			{
				if (is_term2 == 0) {
					UINT16 ret = DrvInputs[2];
					sync_sound();
					if (sound_response_read) ret = (ret & 0xff00) | (sound_response_read() & 0xff);
					rv = ret;
				} else {
					switch (t2_analog_sel) {
						case 0: rv = (0xff - BurnGunReturnX(0)) | 0xff00; break;
						case 1: rv = BurnGunReturnY(0) | 0xff00; break;
						case 2: rv = (0xff - BurnGunReturnX(1)) | 0xff00; break;
						case 3: rv = BurnGunReturnY(1) | 0xff00; break;
					}
				}
				break;
			}

			case 3:
				rv = (DrvDips[0] << 0) | (DrvDips[1] << 8);
				break;

			case 4:
			case 5:
				rv = 0xffff; // ?
				break;

			case 6:
			case 7:
				rv = prot_result;
				break;
		}
		return rv;
	}

	if (address >= 0x02000000 && address <= 0x05ffffff) {
		INT32 offset = (address - 0x02000000) >> 3; // * 2
		UINT16 ret = (DrvGfxROM[offset] << 0) | (DrvGfxROM[offset + 1] << 8);
		if (palette_mask == 0x00ff) ret |= ret << 4;
		return ret;
	}

	return 0xffff;
}

static void midyunit_cvsd_write(UINT16 data)
{
	williams_cvsd_write((data & 0xff) | ((data & 0x200) >> 1));
}

static void to_shiftreg(UINT32 address, UINT16 *shiftreg)
{
	memcpy(shiftreg, &local_videoram[address >> 3], 2 * 512 * sizeof(UINT16));
}

static void from_shiftreg(UINT32 address, UINT16 *shiftreg)
{
	memcpy(&local_videoram[address >> 3], shiftreg, 2 * 512 * sizeof(UINT16));
}

static void autoerase_line(INT32 scanline)
{
	if (autoerase_enable && scanline >= 0 && scanline < 510) {
		memcpy(&local_videoram[512 * scanline], &local_videoram[512 * (510 + (scanline & 1))], 512 * sizeof(UINT16));
	}
}

static INT32 scanline_callback(INT32 scanline, TMS34010Display *params)
{
	scanline -= params->veblnk; // top clipping
	if (scanline < 0 || scanline >= nScreenHeight) return 0;

	UINT16 *src = &local_videoram[(params->rowaddr << 9) & 0x3fe00];
	UINT16 *dest = pTransDraw + (scanline * nScreenWidth);
	INT32 coladdr = params->coladdr << 1;

	vb_start = params->vsblnk;
	v_total = (params->vtotal) ? params->vtotal + 1 : nScreenHeight + 33; // sometimes it's set to 0 on first frame

#if 0
	if (scanline == 127) {
		bprintf(0, _T("ENAB %d\n"), params->enabled);
		bprintf(0, _T("he %d\n"), params->heblnk*2);
		bprintf(0, _T("hs %d\n"), params->hsblnk*2);
		bprintf(0, _T("ve %d\n"), params->veblnk);
		bprintf(0, _T("vs %d\n"), params->vsblnk);
		bprintf(0, _T("vt %d\n"), params->vtotal);
		bprintf(0, _T("ht %d\n"), params->htotal);
	}
#endif

	INT32 heblnk = params->heblnk * 2;
	INT32 hsblnk = params->hsblnk * 2;

	if (!params->enabled) heblnk = hsblnk; // blank line!

	if ((hsblnk - heblnk) < nScreenWidth) {
		for (INT32 x = 0; x < nScreenWidth; x++) {
			dest[x] = 0;
		}
	}

	if (flip_screen_x) {
		for (INT32 x = heblnk; x < hsblnk; x++) {
			INT32 ex = x - heblnk;
			if (ex >= 0 && ex < nScreenWidth) {
				dest[(nScreenWidth - 1) - ex] = pen_map[src[coladdr++ & 0x1ff]];
			}
		}
	} else {
		for (INT32 x = heblnk; x < hsblnk; x++) {
			INT32 ex = x - heblnk;
			if (ex >= 0 && ex < nScreenWidth) {
				dest[ex] = pen_map[src[coladdr++ & 0x1ff]];
			}
		}
	}

	autoerase_line(params->rowaddr - 1);

	if (scanline == nScreenHeight-1) { // last visible line
		autoerase_line(params->rowaddr);
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0x00, RamEnd - AllRam);

	TMS34010Open(0);
	TMS34010Reset();
	TMS34010Close();

	if (sound_reset) sound_reset();

	BurnRandomSetSeed(0xbeef1eaf);

	cmos_page = 0;
	videobank_select = 0;
	autoerase_enable = 0;
	prot_result = 0;
	prot_index = 0;
	memset (prot_sequence, 0, sizeof(prot_sequence));
	t2_analog_sel = 0;

	// service mode toggle
	// games which need toggling service mode button use DrvServ[0] as their diag button
	// games which don't need toggling use DrvJoy2[4]
	DrvServ[0] = 0;
	DrvJoy2[4] = 0; // this needs to be cleared for games that use toggle.

	nExtraCycles = 0;

	v_total = nScreenHeight + 33;
	vb_start = (nScreenHeight == 400) ? 427 : 274;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM			= Next; Next += 0x100000;
	DrvGfxROM			= Next; Next += 0x800000;

	DrvSndROM[0]		= Next; Next += 0x100000;
	DrvSndROM[1]		= Next; Next += 0x200000;

	pen_map				= ( INT32*)Next; Next += 0x010000 * sizeof( INT32);

	BurnPalette			= (UINT32*)Next; Next += 0x002000 * sizeof(UINT32);

	DrvCMOSRAM			= Next; Next += 0x008000;

	AllRam				= Next;

	local_videoram		= (UINT16*)Next;
	DrvVidRAM			= Next; Next += 0x080000;
	BurnPalRAM			= Next; Next += 0x004000;
	DrvMainRAM			= Next; Next += 0x020000;
	DrvPalMAP           = (UINT16*)Next; Next += 0x002000 * sizeof(UINT16); // in ram section!

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static void midyunit_4bit()
{
	for (INT32 i = 0; i < 0x10000; i++)
		pen_map[i] = ((i & 0xf000) >> 8) | (i & 0x000f);

	palette_mask = 0x00ff;
}

static void midyunit_6bit()
{
	for (INT32 i = 0; i < 0x10000; i++)
		pen_map[i] = ((i & 0xc000) >> 8) | (i & 0x0f3f);

	palette_mask = 0x0fff;
}

static void midzunit_8bit()
{
	for (INT32 i = 0; i < 0x10000; i++)
		pen_map[i] = i & 0x1fff;

	palette_mask = 0x1fff;
}

static void reorder_6bpp_graphics()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x800000);

	memcpy (tmp, DrvGfxROM, 0x800000);

	for (INT32 i = 0; i < 0x800000; i+=2)
	{
		INT32 d1 = ((tmp[0 * 0x200000 + (i + 0) / 4]) >> (2 * ((i + 0) & 3))) & 3;
		INT32 d2 = ((tmp[1 * 0x200000 + (i + 0) / 4]) >> (2 * ((i + 0) & 3))) & 3;
		INT32 d3 = ((tmp[2 * 0x200000 + (i + 0) / 4]) >> (2 * ((i + 0) & 3))) & 3;
		INT32 d4 = ((tmp[0 * 0x200000 + (i + 1) / 4]) >> (2 * ((i + 1) & 3))) & 3;
		INT32 d5 = ((tmp[1 * 0x200000 + (i + 1) / 4]) >> (2 * ((i + 1) & 3))) & 3;
		INT32 d6 = ((tmp[2 * 0x200000 + (i + 1) / 4]) >> (2 * ((i + 1) & 3))) & 3;

		DrvGfxROM[i + 0] = d1 | (d2 << 2) | (d3 << 4);
		DrvGfxROM[i + 1] = d4 | (d5 << 2) | (d6 << 4);
	}

	BurnFree(tmp);
}

static void reorder_8bpp_graphics()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x800000);

	memcpy (tmp, DrvGfxROM, 0x800000);

	for (INT32 i = 0; i < 0x800000; i+=4)
	{
		DrvGfxROM[i + 0] = tmp[0 * 0x200000 + i / 4];
		DrvGfxROM[i + 1] = tmp[1 * 0x200000 + i / 4];
		DrvGfxROM[i + 2] = tmp[2 * 0x200000 + i / 4];
		DrvGfxROM[i + 3] = tmp[3 * 0x200000 + i / 4];
	}

	BurnFree(tmp);
}

static INT32 DrvLoadRoms(INT32 gfxbpp)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pDest[4] = { DrvSndROM[0] + 0x10000, DrvSndROM[1], DrvMainROM, DrvGfxROM };
	UINT8 *pStart[2] = { DrvSndROM[0] + 0x10000, DrvSndROM[1] };

	if (gfxbpp == 8) // narc
	{
		bprintf(0, _T("NARC loading roms..\n"));
		pDest[0]  = DrvSndROM[0] + 0x50000;
		pDest[1]  = DrvSndROM[1] + 0x10000;
		pStart[0] = DrvSndROM[0];
		pStart[1] = DrvSndROM[1];
	}

	INT32 nGfxRoms = 0, nGfxOffset = 0;
	{
		for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
			BurnDrvGetRomInfo(&ri, i);
			if ((ri.nType & BRF_GRA) && (ri.nType & 7) == 4) {
				if (nGfxRoms == 0) nGfxOffset = i;
				nGfxRoms++;
			}
		}
		INT32 k = nGfxOffset;
		for (INT32 i = 0; i < gfxbpp/2; i++) { // planes
			for (INT32 j = 0, z = 0; j < nGfxRoms / (gfxbpp / 2); j++) {
				BurnDrvGetRomInfo(&ri, k);
			//	bprintf (0, _T("GFX: %2.2d P(%d) -> %5.5x\n"), k, i, z + (i * 0x200000)); 
				if (BurnLoadRom(DrvGfxROM + (i * 0x200000) + z, k++, 1)) return 1;
				z += ri.nLen;
			}
		}
	}

	for (INT32 i = 0; i < nGfxOffset; i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && ((ri.nType & 7) == 1 || (ri.nType & 7) == 2)) { // M6809 Code
			if (BurnLoadRom(pDest[(ri.nType - 1)  & 7], i, 1)) return 1;
			//bprintf(0, _T("loading snd rom #%x @ %x\n"), (ri.nType - 1) & 7, pDest[(ri.nType - 1) & 7] - pStart[(ri.nType - 1) & 7]);
			if (ri.nLen == 0x10000) {
				//bprintf(0, _T("mirroring it to + 0x10000\n"));
				memmove (pDest[(ri.nType - 1) & 7] + 0x10000, pDest[(ri.nType - 1) & 7], 0x10000);
				pDest[(ri.nType - 1) & 7] += 0x10000;
			}
			pDest[(ri.nType - 1) & 7] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_SND) && (ri.nType & 7) == 2) { // MSM6295 Samples
			if (BurnLoadRom(pDest[(ri.nType - 1) & 7], i, 1)) return 1;
			if (ri.nLen < 0x40000) memmove (pDest[(ri.nType - 1) & 7] + ri.nLen, pDest[(ri.nType - 1) & 7], 0x20000);
			if (ri.nLen < 0x80000) memmove (pDest[(ri.nType - 1) & 7] + ri.nLen, pDest[(ri.nType - 1) & 7], 0x40000);
			pDest[(ri.nType - 1) & 7] += 0x80000;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0xf) == 3) { // TMS34010 Code
			memmove (DrvMainROM, DrvMainROM + (ri.nLen * 2), 0x100000 - (ri.nLen * 2));
			if (BurnLoadRom(DrvMainROM + 0x100000 + 0 - (ri.nLen * 2), i+0, 2)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x100000 + 1 - (ri.nLen * 2), i+1, 2)) return 1;
			i++;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0xf) == (3|8)) { // TMS34010 Code, staggered
			memmove (DrvMainROM, DrvMainROM + (ri.nLen * 4), 0x100000 - (ri.nLen * 4));
			if (BurnLoadRom(DrvMainROM + 0x100000 + 0 - (ri.nLen * 4), i+0, 2)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x100000 + 1 - (ri.nLen * 4), i+1, 2)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x100000 + 0x20000 + 0 - (ri.nLen * 4), i+0, 2)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x100000 + 0x20000 + 1 - (ri.nLen * 4), i+1, 2)) return 1;
			i++;
			continue;
		}
	}

	return 0;
}

static UINT8 term2_nvram_chunk[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0x00,0xff,0x00,0x80,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0x00,0xff,0x00,0x80,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xff,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0x00,0x00,0x00,0x82,0x00,
	0x00,0x00,0x80,0x00,0xfe,0x00,0xfd,0x00,0x00,0x00,0xff,0x00,0x00,0x00,0xff,0x00,
	0xfe,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0xff,0x00,0xfe,0x00,
	0x00,0x00,0x7e,0x00,0x00,0x00,0x80,0x00,0xff,0x00,0x01,0x00,0x00,0x00,0xfb,0x00,
	0x00,0x00,0xfb,0x00,0xfe,0x00,0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

static INT32 CommonInit(void (*load_callback)(), INT32 sound_system, UINT32 clock, INT32 gfx_depth, INT32 sndprot_start, INT32 sndprot_end)
{
	GenericTilesInit(); // get screen dims

	BurnSetRefreshRate((nScreenHeight == 400) ? 54.82 : 54.70);

	BurnAllocMemIndex();

	{
		if (DrvLoadRoms(gfx_depth)) return 1;

		if (load_callback) load_callback();
		
		switch (gfx_depth) {
			case 4: reorder_6bpp_graphics(); midyunit_4bit(); break;
			case 6: reorder_6bpp_graphics(); midyunit_6bit(); break;
			case 8: reorder_8bpp_graphics(); midzunit_8bit(); break;
		}
	}

	master_clock = clock;

	TMS34010Init(0);
	TMS34010Open(0);
	TMS34010MapHandler(0, 				0x00000000, 0xbfffffff, MAP_READ | MAP_WRITE);
	TMS34010SetHandlers(0, 				midyunit_main_read, midyunit_main_write);

	TMS34010MapHandler(1, 				0x00000000, 0x001fffff, MAP_READ | MAP_WRITE);
	TMS34010SetHandlers(1, 				midyunit_vram_read, midyunit_vram_write);

	TMS34010MapMemory(DrvMainRAM,		0x01000000, 0x010fffff, MAP_READ | MAP_WRITE);
	TMS34010MapMemory(DrvCMOSRAM,		0x01400000, 0x0140ffff, MAP_READ | MAP_WRITE);	// bankable! 3 banks!
	TMS34010MapMemory(BurnPalRAM,		0x01800000, 0x0181ffff, MAP_READ);		// read through map

	TMS34010MapHandler(2,				0x01800000, 0x0181ffff, MAP_WRITE);		// write through handler
	TMS34010SetWriteHandler(2, 			midyunit_palette_write);

	if (is_term2) {
		TMS34010MapHandler(3,				0x010aa000, 0x010aafff, MAP_WRITE);		// write through handler
		TMS34010SetWriteHandler(3, 			midyunit_term2_hack);
	}

	TMS34010MapMemory(DrvMainROM,		0xff800000, 0xffffffff, MAP_READ);

	if (is_mkturbo) {
		TMS34010MapHandler(3,				0xfffff000, 0xffffffff, MAP_READ);		// read through handler
		TMS34010SetReadHandler(3, 			midyunit_mkyturbo_hack);
	}

	TMS34010SetPixClock((nScreenHeight == 400 ? 48000000 : 24000000) / 6, 1);
	TMS34010SetCpuCyclesPerFrame((INT32)((master_clock / 8) * 100) / nBurnFPS);
	TMS34010SetToShift(to_shiftreg);
	TMS34010SetFromShift(from_shiftreg);
	TMS34010SetHaltOnReset(0);
	TMS34010SetScanlineRender(scanline_callback);
	TMS34010TimerSetCB(dma_callback);
	TMS34010Close();

	has_ym2151 = 0;

	switch (sound_system & 3)
	{
		case 0: // adpcm
		{
			williams_adpcm_init(DrvSndROM[0], DrvSndROM[1], sndprot_start, sndprot_end);

			sound_write = williams_adpcm_sound_write;
			sound_reset_write = williams_adpcm_reset_write;
			sound_response_read = NULL;
			sound_irq_state_read = williams_adpcm_sound_irq_read;
			sound_reset = williams_adpcm_reset;
			sound_scan = williams_adpcm_scan;
			sound_exit = williams_adpcm_exit;
			sound_update = williams_adpcm_update;
			sound_in_reset = williams_adpcm_sound_in_reset;
			has_ym2151 = 1;
		}
		break;

		case 1: // CVSD
		{
			williams_cvsd_init(DrvSndROM[0], sndprot_start, sndprot_end, sound_system & 0x80 /* small/large mode */);
			sound_write = midyunit_cvsd_write;
			sound_reset_write = williams_cvsd_reset_write;
			sound_response_read = NULL;
			sound_irq_state_read = NULL;
			sound_reset = williams_cvsd_reset;
			sound_scan = williams_cvsd_scan;
			sound_exit = williams_cvsd_exit;
			sound_update = williams_cvsd_update;
			sound_in_reset = williams_cvsd_in_reset;
			has_ym2151 = 1;
		}
		break;

		case 2: // narc
		{
			narc_sound_init(DrvSndROM[0], DrvSndROM[1]);
			sound_write = narc_sound_write;
			sound_reset_write = NULL;
			sound_response_read = narc_sound_response_read;
			sound_irq_state_read = NULL;
			sound_reset = narc_sound_reset;
			sound_scan = narc_sound_scan;
			sound_exit = narc_sound_exit;
			sound_update = narc_sound_update;
			sound_in_reset = narc_sound_in_reset;
			has_ym2151 = 1;
		}
		break;

		case 3: // yawdim (bootleg)
		{
			yawdim_sound_init(DrvSndROM[0], DrvSndROM[1], sound_system & 4); // yawdim2?
			sound_write = yawdim_soundlatch_write;
			sound_reset_write = NULL;
			sound_response_read = NULL;
			sound_irq_state_read = NULL;
			sound_reset = yawdim_sound_reset;
			sound_scan = yawdim_sound_scan;
			sound_exit = yawdim_sound_exit;
			sound_update = yawdim_sound_update;
		}
		break;
	}

	if (is_term2) {
		BurnGunInit(2, true);
		memcpy(DrvCMOSRAM + 0x2000, term2_nvram_chunk, sizeof(term2_nvram_chunk));
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	BurnFreeMemIndex();
	TMS34010Exit();

	if (sound_exit) sound_exit();

	sound_write = NULL;
	sound_reset_write = NULL;
	sound_response_read = NULL;
	sound_irq_state_read = NULL;
	sound_reset = NULL;
	sound_scan = NULL;
	sound_exit = NULL;
	sound_update = NULL;
	sound_in_reset = NULL;

	is_yawdim = 0;
	flip_screen_x = 0;

	if (is_term2) {
		BurnGunExit();
	}

	is_term2 = 0;
	is_mkturbo = 0;
	is_totcarn = 0;

	return 0;
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvRecalculatePalette();
		BurnRecalc = 0;
	}

	BurnTransferCopy(BurnPalette);

	if (is_term2 && DrvDips[2] & 1) {
		// The emulated crosshair is only needed for calibration.
		// The game has it's own set of crosshairs.
		BurnGunDrawTargets();
	}

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		// service mode toggle
		if (service.Toggle(DrvServ[0])) {
			DrvJoy2[4] = DrvServ[0];
		}

		// add button for bomb on total carnage
		if (is_totcarn) {
			DrvJoy2[2] |= DrvJoyF[2];
			DrvJoy2[5] |= DrvJoyF[5];
		}

		// process inputs
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (is_term2) {
			// lightgun stuff
			BurnGunMakeInputs(0, Gun[0], Gun[1]);
			BurnGunMakeInputs(1, Gun[2], Gun[3]);
		}
	}

	TMS34010NewFrame();
	M6809NewFrame();

	INT32 nInterleave = (palette_mask == 0x1fff) ? 433 : 289; // narc : others
	INT32 nCyclesTotal[3] = { (INT32)((master_clock / 8) * 100) / nBurnFPS, (2000060 * 100) / nBurnFPS, (2000060 * 100) / nBurnFPS };
	INT32 nCyclesDone[3] = { nExtraCycles, 0, 0 };

	TMS34010Open(0);
	if (pBurnSoundOut) BurnSoundClear();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 our_line = (i + vb_start) % v_total; // start at vblank (some games(narc) have different vbl)

		M6809Open(0); // main (TMS340) accesses this via sound*

		CPU_RUN(0, TMS34010);
		TMS34010GenerateScanline(our_line);

		if (sound_in_reset()) {
			CPU_IDLE(1, M6809);
		} else {
			CPU_RUN_TIMER(1);
		}
		M6809Close();

		if (palette_mask != 0x1fff) continue; // only narc!

		M6809Open(1);
		if (sound_in_reset()) {
			CPU_IDLE(2, M6809);
		} else {
			CPU_RUN(2, M6809);
		}
		M6809Close();
    }

	nExtraCycles = TMS34010TotalCycles() - nCyclesTotal[0];

	TMS34010Close();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	if (pBurnSoundOut) {
		M6809Open(0);
		if (sound_update) sound_update(pBurnSoundOut, nBurnSoundLen);
		M6809Close();
    }

    return 0;
}

static INT32 YawdimFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	TMS34010NewFrame();
	ZetNewFrame();

	INT32 nInterleave = 304;
	INT32 nCyclesTotal[2] = { (INT32)(((master_clock / 8) * 100) / nBurnFPS), (4000000 * 100) / nBurnFPS };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };
	INT32 bDrawn = 0;

	TMS34010Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, TMS34010);
		TMS34010GenerateScanline(i);

		if (i == vb_start && pBurnDraw) {
			BurnDrvRedraw();
			bDrawn = 1;
		}

		CPU_RUN(1, Zet);
    }

	nExtraCycles = TMS34010TotalCycles() - nCyclesTotal[0];

	ZetClose();
	TMS34010Close();

	if (pBurnDraw && bDrawn == 0) {
		BurnDrvRedraw();
	}

	if (pBurnSoundOut) {
		if (sound_update) sound_update(pBurnSoundOut, nBurnSoundLen);
    }

    return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		TMS34010Scan(nAction);

		if (sound_scan) sound_scan(nAction, pnMin);
		if (is_term2) BurnGunScan();

		SCAN_VAR(dma_state);
		SCAN_VAR(dma_register);
		SCAN_VAR(cmos_page);
		SCAN_VAR(videobank_select);
		SCAN_VAR(autoerase_enable);
		SCAN_VAR(prot_result);
		SCAN_VAR(prot_index);
		SCAN_VAR(prot_sequence);
		SCAN_VAR(palette_mask);
		SCAN_VAR(cmos_w_enable);
		SCAN_VAR(t2_analog_sel);

		SCAN_VAR(nExtraCycles);

		service.Scan();
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvCMOSRAM;
		ba.nLen		= 0x8000;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE) {
		TMS34010Open(0);
		cmos_bankswitch();
		TMS34010Close();
	}

	return 0;
}

// Narc (rev 7.00)

static struct BurnRomInfo narcRomDesc[] = {
	{ "narcrev2.u4",									0x10000, 0x450a591a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code (Sound)
	{ "narcrev2.u5",									0x10000, 0xe551e5e3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "narcrev2.u35",									0x10000, 0x81295892, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code (Sound)
	{ "narcrev2.u36",									0x10000, 0x16cdbb13, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "narcrev2.u37",									0x10000, 0x29dbeffd, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "narcrev2.u38",									0x10000, 0x09b03b80, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "narcrev7.u42",									0x20000, 0xd1111b76, 3 | BRF_PRG | BRF_ESS }, //  6 TMS34010 Code
	{ "narcrev7.u24",									0x20000, 0xaa0d3082, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "narcrev7.u41",									0x20000, 0x3903191f, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "narcrev7.u23",									0x20000, 0x7a316582, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "u94",											0x10000, 0xca3194e4, 4 | BRF_GRA },           // 10 Graphics (Blitter data)
	{ "u93",											0x10000, 0x0ed7f7f5, 4 | BRF_GRA },           // 11
	{ "u92",											0x10000, 0x40d2fc66, 4 | BRF_GRA },           // 12
	{ "u91",											0x10000, 0xf39325e0, 4 | BRF_GRA },           // 13
	{ "u90",											0x10000, 0x0132aefa, 4 | BRF_GRA },           // 14
	{ "u89",											0x10000, 0xf7260c9e, 4 | BRF_GRA },           // 15
	{ "u88",											0x10000, 0xedc19f42, 4 | BRF_GRA },           // 16
	{ "u87",											0x10000, 0xd9b42ff9, 4 | BRF_GRA },           // 17
	{ "u86",											0x10000, 0xaf7daad3, 4 | BRF_GRA },           // 18
	{ "u85",											0x10000, 0x095fae6b, 4 | BRF_GRA },           // 19
	{ "u84",											0x10000, 0x3fdf2057, 4 | BRF_GRA },           // 20
	{ "u83",											0x10000, 0xf2d27c9f, 4 | BRF_GRA },           // 21
	{ "u82",											0x10000, 0x962ce47c, 4 | BRF_GRA },           // 22
	{ "u81",											0x10000, 0x00fe59ec, 4 | BRF_GRA },           // 23
	{ "u80",											0x10000, 0x147ba8e9, 4 | BRF_GRA },           // 24
	{ "u76",											0x10000, 0x1cd897f4, 4 | BRF_GRA },           // 25
	{ "u75",											0x10000, 0x78abfa01, 4 | BRF_GRA },           // 26
	{ "u74",											0x10000, 0x66d2a234, 4 | BRF_GRA },           // 27
	{ "u73",											0x10000, 0xefa5cd4e, 4 | BRF_GRA },           // 28
	{ "u72",											0x10000, 0x70638eb5, 4 | BRF_GRA },           // 29
	{ "u71",											0x10000, 0x61226883, 4 | BRF_GRA },           // 30
	{ "u70",											0x10000, 0xc808849f, 4 | BRF_GRA },           // 31
	{ "u69",											0x10000, 0xe7f9c34f, 4 | BRF_GRA },           // 32
	{ "u68",											0x10000, 0x88a634d5, 4 | BRF_GRA },           // 33
	{ "u67",											0x10000, 0x4ab8b69e, 4 | BRF_GRA },           // 34
	{ "u66",											0x10000, 0xe1da4b25, 4 | BRF_GRA },           // 35
	{ "u65",											0x10000, 0x6df0d125, 4 | BRF_GRA },           // 36
	{ "u64",											0x10000, 0xabab1b16, 4 | BRF_GRA },           // 37
	{ "u63",											0x10000, 0x80602f31, 4 | BRF_GRA },           // 38
	{ "u62",											0x10000, 0xc2a476d1, 4 | BRF_GRA },           // 39
	{ "u58",											0x10000, 0x8a7501e3, 4 | BRF_GRA },           // 40
	{ "u57",											0x10000, 0xa504735f, 4 | BRF_GRA },           // 41
	{ "u56",											0x10000, 0x55f8cca7, 4 | BRF_GRA },           // 42
	{ "u55",											0x10000, 0xd3c932c1, 4 | BRF_GRA },           // 43
	{ "u54",											0x10000, 0xc7f4134b, 4 | BRF_GRA },           // 44
	{ "u53",											0x10000, 0x6be4da56, 4 | BRF_GRA },           // 45
	{ "u52",											0x10000, 0x1ea36a4a, 4 | BRF_GRA },           // 46
	{ "u51",											0x10000, 0x9d4b0324, 4 | BRF_GRA },           // 47
	{ "u50",											0x10000, 0x6f9f0c26, 4 | BRF_GRA },           // 48
	{ "u49",											0x10000, 0x80386fce, 4 | BRF_GRA },           // 49
	{ "u48",											0x10000, 0x05c16185, 4 | BRF_GRA },           // 50
	{ "u47",											0x10000, 0x4c0151f1, 4 | BRF_GRA },           // 51
	{ "u46",											0x10000, 0x5670bfcb, 4 | BRF_GRA },           // 52
	{ "u45",											0x10000, 0x27f10d98, 4 | BRF_GRA },           // 53
	{ "u44",											0x10000, 0x93b8eaa4, 4 | BRF_GRA },           // 54
	{ "u40",											0x10000, 0x7fcaebc7, 4 | BRF_GRA },           // 55
	{ "u39",											0x10000, 0x7db5cf52, 4 | BRF_GRA },           // 56
	{ "u38",											0x10000, 0x3f9f3ef7, 4 | BRF_GRA },           // 57
	{ "u37",											0x10000, 0xed81826c, 4 | BRF_GRA },           // 58
	{ "u36",											0x10000, 0xe5d855c0, 4 | BRF_GRA },           // 59
	{ "u35",											0x10000, 0x3a7b1329, 4 | BRF_GRA },           // 60
	{ "u34",											0x10000, 0xfe982b0e, 4 | BRF_GRA },           // 61
	{ "u33",											0x10000, 0x6bc7eb0f, 4 | BRF_GRA },           // 62
	{ "u32",											0x10000, 0x5875a6d3, 4 | BRF_GRA },           // 63
	{ "u31",											0x10000, 0x2fa4b8e5, 4 | BRF_GRA },           // 64
	{ "u30",											0x10000, 0x7e4bb8ee, 4 | BRF_GRA },           // 65
	{ "u29",											0x10000, 0x45136fd9, 4 | BRF_GRA },           // 66
	{ "u28",											0x10000, 0xd6cdac24, 4 | BRF_GRA },           // 67
	{ "u27",											0x10000, 0x4d33bbec, 4 | BRF_GRA },           // 68
	{ "u26",											0x10000, 0xcb19f784, 4 | BRF_GRA },           // 69
};

STD_ROM_PICK(narc)
STD_ROM_FN(narc)

static INT32 NarcInit()
{
	prot_data = NULL;

	return CommonInit(NULL, 2, 48000000, 8, 0xcdff, 0xce29);
}

struct BurnDriver BurnDrvNarc = {
	"narc", NULL, NULL, NULL, "1988",
	"Narc (rev 7.00)\0", NULL, "Williams", "Z Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, narcRomInfo, narcRomName, NULL, NULL, NULL, NULL, NarcInputInfo, NarcDIPInfo,
	NarcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 8192,
	512, 400, 4, 3
};


// Narc (rev 6.00)

static struct BurnRomInfo narc6RomDesc[] = {
	{ "rev2_narc_sound_rom_u4.u4",						0x10000, 0x450a591a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code (Sound)
	{ "rev2_narc_sound_rom_u5.u5",						0x10000, 0xe551e5e3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rev2_narc_sound_rom_u35.u35",					0x10000, 0x81295892, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code (Sound)
	{ "rev2_narc_sound_rom_u36.u36",					0x10000, 0x16cdbb13, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "rev2_narc_sound_rom_u37.u37",					0x10000, 0x29dbeffd, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "rev2_narc_sound_rom_u38.u38",					0x10000, 0x09b03b80, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "rev6_narc_game_rom_u78.u78",						0x10000, 0x2c9e799b, (3 | 8) | BRF_PRG | BRF_ESS }, //  6 TMS34010 Code
	{ "rev6_narc_game_rom_u60.u60",						0x10000, 0x5f6b0429, (3 | 8) | BRF_PRG | BRF_ESS }, //  7
	{ "rev6_narc_game_rom_u77.u77",						0x10000, 0x508cfa38, (3 | 8) | BRF_PRG | BRF_ESS }, //  8
	{ "rev6_narc_game_rom_u59.u59",						0x10000, 0x84bc91fc, (3 | 8) | BRF_PRG | BRF_ESS }, //  9
	{ "rev6_narc_game_rom_u42.u42",						0x10000, 0xee8ae9d4, (3 | 8) | BRF_PRG | BRF_ESS }, // 10
	{ "rev6_narc_game_rom_u24.u24",						0x10000, 0x4fbe2ff5, (3 | 8) | BRF_PRG | BRF_ESS }, // 11
	{ "rev6_narc_game_rom_u41.u41",						0x10000, 0x43a1bbbc, (3 | 8) | BRF_PRG | BRF_ESS }, // 12
	{ "rev6_narc_game_rom_u23.u23",						0x10000, 0xed0d149d, (3 | 8) | BRF_PRG | BRF_ESS }, // 13

	{ "rev2_narc_image_rom_u94.u94",					0x10000, 0xca3194e4, 4 | BRF_GRA },           // 14 Graphics (Blitter data)
	{ "rev2_narc_image_rom_u93.u93",					0x10000, 0x0ed7f7f5, 4 | BRF_GRA },           // 15
	{ "rev2_narc_image_rom_u92.u92",					0x10000, 0x40d2fc66, 4 | BRF_GRA },           // 16
	{ "rev2_narc_image_rom_u91.u91",					0x10000, 0xf39325e0, 4 | BRF_GRA },           // 17
	{ "rev2_narc_image_rom_u90.u90",					0x10000, 0x0132aefa, 4 | BRF_GRA },           // 18
	{ "rev2_narc_image_rom_u89.u89",					0x10000, 0xf7260c9e, 4 | BRF_GRA },           // 19
	{ "rev2_narc_image_rom_u88.u88",					0x10000, 0xedc19f42, 4 | BRF_GRA },           // 20
	{ "rev2_narc_image_rom_u87.u87",					0x10000, 0xd9b42ff9, 4 | BRF_GRA },           // 21
	{ "rev2_narc_image_rom_u86.u86",					0x10000, 0xaf7daad3, 4 | BRF_GRA },           // 22
	{ "rev2_narc_image_rom_u85.u85",					0x10000, 0x095fae6b, 4 | BRF_GRA },           // 23
	{ "rev2_narc_image_rom_u84.u84",					0x10000, 0x3fdf2057, 4 | BRF_GRA },           // 24
	{ "rev2_narc_image_rom_u83.u83",					0x10000, 0xf2d27c9f, 4 | BRF_GRA },           // 25
	{ "rev2_narc_image_rom_u82.u82",					0x10000, 0x962ce47c, 4 | BRF_GRA },           // 26
	{ "rev2_narc_image_rom_u81.u81",					0x10000, 0x00fe59ec, 4 | BRF_GRA },           // 27
	{ "rev2_narc_image_rom_u80.u80",					0x10000, 0x147ba8e9, 4 | BRF_GRA },           // 28
	{ "rev2_narc_image_rom_u76.u76",					0x10000, 0x1cd897f4, 4 | BRF_GRA },           // 29
	{ "rev2_narc_image_rom_u75.u75",					0x10000, 0x78abfa01, 4 | BRF_GRA },           // 30
	{ "rev2_narc_image_rom_u74.u74",					0x10000, 0x66d2a234, 4 | BRF_GRA },           // 31
	{ "rev2_narc_image_rom_u73.u73",					0x10000, 0xefa5cd4e, 4 | BRF_GRA },           // 32
	{ "rev2_narc_image_rom_u72.u72",					0x10000, 0x70638eb5, 4 | BRF_GRA },           // 33
	{ "rev2_narc_image_rom_u71.u71",					0x10000, 0x61226883, 4 | BRF_GRA },           // 34
	{ "rev2_narc_image_rom_u70.u70",					0x10000, 0xc808849f, 4 | BRF_GRA },           // 35
	{ "rev2_narc_image_rom_u69.u69",					0x10000, 0xe7f9c34f, 4 | BRF_GRA },           // 36
	{ "rev2_narc_image_rom_u68.u68",					0x10000, 0x88a634d5, 4 | BRF_GRA },           // 37
	{ "rev2_narc_image_rom_u67.u67",					0x10000, 0x4ab8b69e, 4 | BRF_GRA },           // 38
	{ "rev2_narc_image_rom_u66.u66",					0x10000, 0xe1da4b25, 4 | BRF_GRA },           // 39
	{ "rev2_narc_image_rom_u65.u65",					0x10000, 0x6df0d125, 4 | BRF_GRA },           // 40
	{ "rev2_narc_image_rom_u64.u64",					0x10000, 0xabab1b16, 4 | BRF_GRA },           // 41
	{ "rev2_narc_image_rom_u63.u63",					0x10000, 0x80602f31, 4 | BRF_GRA },           // 42
	{ "rev2_narc_image_rom_u62.u62",					0x10000, 0xc2a476d1, 4 | BRF_GRA },           // 43
	{ "rev2_narc_image_rom_u58.u58",					0x10000, 0x8a7501e3, 4 | BRF_GRA },           // 44
	{ "rev2_narc_image_rom_u57.u57",					0x10000, 0xa504735f, 4 | BRF_GRA },           // 45
	{ "rev2_narc_image_rom_u56.u56",					0x10000, 0x55f8cca7, 4 | BRF_GRA },           // 46
	{ "rev2_narc_image_rom_u55.u55",					0x10000, 0xd3c932c1, 4 | BRF_GRA },           // 47
	{ "rev2_narc_image_rom_u54.u54",					0x10000, 0xc7f4134b, 4 | BRF_GRA },           // 48
	{ "rev2_narc_image_rom_u53.u53",					0x10000, 0x6be4da56, 4 | BRF_GRA },           // 49
	{ "rev2_narc_image_rom_u52.u52",					0x10000, 0x1ea36a4a, 4 | BRF_GRA },           // 50
	{ "rev2_narc_image_rom_u51.u51",					0x10000, 0x9d4b0324, 4 | BRF_GRA },           // 51
	{ "rev2_narc_image_rom_u50.u50",					0x10000, 0x6f9f0c26, 4 | BRF_GRA },           // 52
	{ "rev2_narc_image_rom_u49.u49",					0x10000, 0x80386fce, 4 | BRF_GRA },           // 53
	{ "rev2_narc_image_rom_u48.u48",					0x10000, 0x05c16185, 4 | BRF_GRA },           // 54
	{ "rev2_narc_image_rom_u47.u47",					0x10000, 0x4c0151f1, 4 | BRF_GRA },           // 55
	{ "rev2_narc_image_rom_u46.u46",					0x10000, 0x5670bfcb, 4 | BRF_GRA },           // 56
	{ "rev2_narc_image_rom_u45.u45",					0x10000, 0x27f10d98, 4 | BRF_GRA },           // 57
	{ "rev2_narc_image_rom_u44.u44",					0x10000, 0x93b8eaa4, 4 | BRF_GRA },           // 58
	{ "rev2_narc_image_rom_u40.u40",					0x10000, 0x7fcaebc7, 4 | BRF_GRA },           // 59
	{ "rev2_narc_image_rom_u39.u39",					0x10000, 0x7db5cf52, 4 | BRF_GRA },           // 60
	{ "rev2_narc_image_rom_u38.u38",					0x10000, 0x3f9f3ef7, 4 | BRF_GRA },           // 61
	{ "rev2_narc_image_rom_u37.u37",					0x10000, 0xed81826c, 4 | BRF_GRA },           // 62
	{ "rev2_narc_image_rom_u36.u36",					0x10000, 0xe5d855c0, 4 | BRF_GRA },           // 63
	{ "rev2_narc_image_rom_u35.u35",					0x10000, 0x3a7b1329, 4 | BRF_GRA },           // 64
	{ "rev2_narc_image_rom_u34.u34",					0x10000, 0xfe982b0e, 4 | BRF_GRA },           // 65
	{ "rev2_narc_image_rom_u33.u33",					0x10000, 0x6bc7eb0f, 4 | BRF_GRA },           // 66
	{ "rev2_narc_image_rom_u32.u32",					0x10000, 0x5875a6d3, 4 | BRF_GRA },           // 67
	{ "rev2_narc_image_rom_u31.u31",					0x10000, 0x2fa4b8e5, 4 | BRF_GRA },           // 68
	{ "rev2_narc_image_rom_u30.u30",					0x10000, 0x7e4bb8ee, 4 | BRF_GRA },           // 69
	{ "rev2_narc_image_rom_u29.u29",					0x10000, 0x45136fd9, 4 | BRF_GRA },           // 70
	{ "rev2_narc_image_rom_u28.u28",					0x10000, 0xd6cdac24, 4 | BRF_GRA },           // 71
	{ "rev2_narc_image_rom_u27.u27",					0x10000, 0x4d33bbec, 4 | BRF_GRA },           // 72
	{ "rev2_narc_image_rom_u26.u26",					0x10000, 0xcb19f784, 4 | BRF_GRA },           // 73
};

STD_ROM_PICK(narc6)
STD_ROM_FN(narc6)

struct BurnDriver BurnDrvNarc6 = {
	"narc6", "narc", NULL, NULL, "1988",
	"Narc (rev 6.00)\0", NULL, "Williams", "Z Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, narc6RomInfo, narc6RomName, NULL, NULL, NULL, NULL, NarcInputInfo, NarcDIPInfo,
	NarcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 8192,
	512, 400, 4, 3
};


// Narc (rev 4.00)

static struct BurnRomInfo narc4RomDesc[] = {
	{ "rev2_narc_sound_rom_u4.u4",						0x10000, 0x450a591a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code (Sound)
	{ "rev2_narc_sound_rom_u5.u5",						0x10000, 0xe551e5e3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rev2_narc_sound_rom_u35.u35",					0x10000, 0x81295892, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code (Sound)
	{ "rev2_narc_sound_rom_u36.u36",					0x10000, 0x16cdbb13, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "rev2_narc_sound_rom_u37.u37",					0x10000, 0x29dbeffd, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "rev2_narc_sound_rom_u38.u38",					0x10000, 0x09b03b80, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "rev4_narc_game_rom_u78.u78",						0x10000, 0x99bbd587, (3 | 8) | BRF_PRG | BRF_ESS }, //  6 TMS34010 Code
	{ "rev4_narc_game_rom_u60.u60",						0x10000, 0xbeec5f1a, (3 | 8) | BRF_PRG | BRF_ESS }, //  7
	{ "rev4_narc_game_rom_u77.u77",						0x10000, 0x0b9bdd76, (3 | 8) | BRF_PRG | BRF_ESS }, //  8
	{ "rev4_narc_game_rom_u59.u59",						0x10000, 0x0169e4c3, (3 | 8) | BRF_PRG | BRF_ESS }, //  9
	{ "rev4_narc_game_rom_u42.u42",						0x10000, 0xa7b0347d, (3 | 8) | BRF_PRG | BRF_ESS }, // 10
	{ "rev4_narc_game_rom_u24.u24",						0x10000, 0x613c9f54, (3 | 8) | BRF_PRG | BRF_ESS }, // 11
	{ "rev4_narc_game_rom_u41.u41",						0x10000, 0x80e83440, (3 | 8) | BRF_PRG | BRF_ESS }, // 12
	{ "rev4_narc_game_rom_u23.u23",						0x10000, 0x425a3f8f, (3 | 8) | BRF_PRG | BRF_ESS }, // 13

	{ "rev2_narc_image_rom_u94.u94",					0x10000, 0xca3194e4, 4 | BRF_GRA },           // 14 Graphics (Blitter data)
	{ "rev2_narc_image_rom_u93.u93",					0x10000, 0x0ed7f7f5, 4 | BRF_GRA },           // 15
	{ "rev2_narc_image_rom_u92.u92",					0x10000, 0x40d2fc66, 4 | BRF_GRA },           // 16
	{ "rev2_narc_image_rom_u91.u91",					0x10000, 0xf39325e0, 4 | BRF_GRA },           // 17
	{ "rev2_narc_image_rom_u90.u90",					0x10000, 0x0132aefa, 4 | BRF_GRA },           // 18
	{ "rev2_narc_image_rom_u89.u89",					0x10000, 0xf7260c9e, 4 | BRF_GRA },           // 19
	{ "rev2_narc_image_rom_u88.u88",					0x10000, 0xedc19f42, 4 | BRF_GRA },           // 20
	{ "rev2_narc_image_rom_u87.u87",					0x10000, 0xd9b42ff9, 4 | BRF_GRA },           // 21
	{ "rev2_narc_image_rom_u86.u86",					0x10000, 0xaf7daad3, 4 | BRF_GRA },           // 22
	{ "rev2_narc_image_rom_u85.u85",					0x10000, 0x095fae6b, 4 | BRF_GRA },           // 23
	{ "rev2_narc_image_rom_u84.u84",					0x10000, 0x3fdf2057, 4 | BRF_GRA },           // 24
	{ "rev2_narc_image_rom_u83.u83",					0x10000, 0xf2d27c9f, 4 | BRF_GRA },           // 25
	{ "rev2_narc_image_rom_u82.u82",					0x10000, 0x962ce47c, 4 | BRF_GRA },           // 26
	{ "rev2_narc_image_rom_u81.u81",					0x10000, 0x00fe59ec, 4 | BRF_GRA },           // 27
	{ "rev2_narc_image_rom_u80.u80",					0x10000, 0x147ba8e9, 4 | BRF_GRA },           // 28
	{ "rev2_narc_image_rom_u76.u76",					0x10000, 0x1cd897f4, 4 | BRF_GRA },           // 29
	{ "rev2_narc_image_rom_u75.u75",					0x10000, 0x78abfa01, 4 | BRF_GRA },           // 30
	{ "rev2_narc_image_rom_u74.u74",					0x10000, 0x66d2a234, 4 | BRF_GRA },           // 31
	{ "rev2_narc_image_rom_u73.u73",					0x10000, 0xefa5cd4e, 4 | BRF_GRA },           // 32
	{ "rev2_narc_image_rom_u72.u72",					0x10000, 0x70638eb5, 4 | BRF_GRA },           // 33
	{ "rev2_narc_image_rom_u71.u71",					0x10000, 0x61226883, 4 | BRF_GRA },           // 34
	{ "rev2_narc_image_rom_u70.u70",					0x10000, 0xc808849f, 4 | BRF_GRA },           // 35
	{ "rev2_narc_image_rom_u69.u69",					0x10000, 0xe7f9c34f, 4 | BRF_GRA },           // 36
	{ "rev2_narc_image_rom_u68.u68",					0x10000, 0x88a634d5, 4 | BRF_GRA },           // 37
	{ "rev2_narc_image_rom_u67.u67",					0x10000, 0x4ab8b69e, 4 | BRF_GRA },           // 38
	{ "rev2_narc_image_rom_u66.u66",					0x10000, 0xe1da4b25, 4 | BRF_GRA },           // 39
	{ "rev2_narc_image_rom_u65.u65",					0x10000, 0x6df0d125, 4 | BRF_GRA },           // 40
	{ "rev2_narc_image_rom_u64.u64",					0x10000, 0xabab1b16, 4 | BRF_GRA },           // 41
	{ "rev2_narc_image_rom_u63.u63",					0x10000, 0x80602f31, 4 | BRF_GRA },           // 42
	{ "rev2_narc_image_rom_u62.u62",					0x10000, 0xc2a476d1, 4 | BRF_GRA },           // 43
	{ "rev2_narc_image_rom_u58.u58",					0x10000, 0x8a7501e3, 4 | BRF_GRA },           // 44
	{ "rev2_narc_image_rom_u57.u57",					0x10000, 0xa504735f, 4 | BRF_GRA },           // 45
	{ "rev2_narc_image_rom_u56.u56",					0x10000, 0x55f8cca7, 4 | BRF_GRA },           // 46
	{ "rev2_narc_image_rom_u55.u55",					0x10000, 0xd3c932c1, 4 | BRF_GRA },           // 47
	{ "rev2_narc_image_rom_u54.u54",					0x10000, 0xc7f4134b, 4 | BRF_GRA },           // 48
	{ "rev2_narc_image_rom_u53.u53",					0x10000, 0x6be4da56, 4 | BRF_GRA },           // 49
	{ "rev2_narc_image_rom_u52.u52",					0x10000, 0x1ea36a4a, 4 | BRF_GRA },           // 50
	{ "rev2_narc_image_rom_u51.u51",					0x10000, 0x9d4b0324, 4 | BRF_GRA },           // 51
	{ "rev2_narc_image_rom_u50.u50",					0x10000, 0x6f9f0c26, 4 | BRF_GRA },           // 52
	{ "rev2_narc_image_rom_u49.u49",					0x10000, 0x80386fce, 4 | BRF_GRA },           // 53
	{ "rev2_narc_image_rom_u48.u48",					0x10000, 0x05c16185, 4 | BRF_GRA },           // 54
	{ "rev2_narc_image_rom_u47.u47",					0x10000, 0x4c0151f1, 4 | BRF_GRA },           // 55
	{ "rev2_narc_image_rom_u46.u46",					0x10000, 0x5670bfcb, 4 | BRF_GRA },           // 56
	{ "rev2_narc_image_rom_u45.u45",					0x10000, 0x27f10d98, 4 | BRF_GRA },           // 57
	{ "rev2_narc_image_rom_u44.u44",					0x10000, 0x93b8eaa4, 4 | BRF_GRA },           // 58
	{ "rev2_narc_image_rom_u40.u40",					0x10000, 0x7fcaebc7, 4 | BRF_GRA },           // 59
	{ "rev2_narc_image_rom_u39.u39",					0x10000, 0x7db5cf52, 4 | BRF_GRA },           // 60
	{ "rev2_narc_image_rom_u38.u38",					0x10000, 0x3f9f3ef7, 4 | BRF_GRA },           // 61
	{ "rev2_narc_image_rom_u37.u37",					0x10000, 0xed81826c, 4 | BRF_GRA },           // 62
	{ "rev2_narc_image_rom_u36.u36",					0x10000, 0xe5d855c0, 4 | BRF_GRA },           // 63
	{ "rev2_narc_image_rom_u35.u35",					0x10000, 0x3a7b1329, 4 | BRF_GRA },           // 64
	{ "rev2_narc_image_rom_u34.u34",					0x10000, 0xfe982b0e, 4 | BRF_GRA },           // 65
	{ "rev2_narc_image_rom_u33.u33",					0x10000, 0x6bc7eb0f, 4 | BRF_GRA },           // 66
	{ "rev2_narc_image_rom_u32.u32",					0x10000, 0x5875a6d3, 4 | BRF_GRA },           // 67
	{ "rev2_narc_image_rom_u31.u31",					0x10000, 0x2fa4b8e5, 4 | BRF_GRA },           // 68
	{ "rev2_narc_image_rom_u30.u30",					0x10000, 0x7e4bb8ee, 4 | BRF_GRA },           // 69
	{ "rev2_narc_image_rom_u29.u29",					0x10000, 0x45136fd9, 4 | BRF_GRA },           // 70
	{ "rev2_narc_image_rom_u28.u28",					0x10000, 0xd6cdac24, 4 | BRF_GRA },           // 71
	{ "rev2_narc_image_rom_u27.u27",					0x10000, 0x4d33bbec, 4 | BRF_GRA },           // 72
	{ "rev2_narc_image_rom_u26.u26",					0x10000, 0xcb19f784, 4 | BRF_GRA },           // 73
};

STD_ROM_PICK(narc4)
STD_ROM_FN(narc4)

struct BurnDriver BurnDrvNarc4 = {
	"narc4", "narc", NULL, NULL, "1988",
	"Narc (rev 4.00)\0", NULL, "Williams", "Z Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, narc4RomInfo, narc4RomName, NULL, NULL, NULL, NULL, NarcInputInfo, NarcDIPInfo,
	NarcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 8192,
	512, 400, 4, 3
};


// Narc (rev 3.20)

static struct BurnRomInfo narc3RomDesc[] = {
	{ "rev2_narc_sound_rom_u4.u4",						0x10000, 0x450a591a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code (Sound)
	{ "rev2_narc_sound_rom_u5.u5",						0x10000, 0xe551e5e3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rev2_narc_sound_rom_u35.u35",					0x10000, 0x81295892, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code (Sound)
	{ "rev2_narc_sound_rom_u36.u36",					0x10000, 0x16cdbb13, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "rev2_narc_sound_rom_u37.u37",					0x10000, 0x29dbeffd, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "rev2_narc_sound_rom_u38.u38",					0x10000, 0x09b03b80, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "rev3_narc_game_rom_u78.u78",						0x10000, 0x388581b0, (3 | 8) | BRF_PRG | BRF_ESS }, //  6 TMS34010 Code
	{ "rev3_narc_game_rom_u60.u60",						0x10000, 0xf273bc04, (3 | 8) | BRF_PRG | BRF_ESS }, //  7
	{ "rev3_narc_game_rom_u77.u77",						0x10000, 0xbdafaccc, (3 | 8) | BRF_PRG | BRF_ESS }, //  8
	{ "rev3_narc_game_rom_u59.u59",						0x10000, 0x96314a99, (3 | 8) | BRF_PRG | BRF_ESS }, //  9
	{ "rev3_narc_game_rom_u42.u42",						0x10000, 0x56aebc81, (3 | 8) | BRF_PRG | BRF_ESS }, // 10
	{ "rev3_narc_game_rom_u24.u24",						0x10000, 0x11d7e143, (3 | 8) | BRF_PRG | BRF_ESS }, // 11
	{ "rev3_narc_game_rom_u41.u41",						0x10000, 0x6142fab7, (3 | 8) | BRF_PRG | BRF_ESS }, // 12
	{ "rev3_narc_game_rom_u23.u23",						0x10000, 0x98cdd178, (3 | 8) | BRF_PRG | BRF_ESS }, // 13

	{ "rev2_narc_image_rom_u94.u94",					0x10000, 0xca3194e4, 4 | BRF_GRA },           // 14 Graphics (Blitter data)
	{ "rev2_narc_image_rom_u93.u93",					0x10000, 0x0ed7f7f5, 4 | BRF_GRA },           // 15
	{ "rev2_narc_image_rom_u92.u92",					0x10000, 0x40d2fc66, 4 | BRF_GRA },           // 16
	{ "rev2_narc_image_rom_u91.u91",					0x10000, 0xf39325e0, 4 | BRF_GRA },           // 17
	{ "rev2_narc_image_rom_u90.u90",					0x10000, 0x0132aefa, 4 | BRF_GRA },           // 18
	{ "rev2_narc_image_rom_u89.u89",					0x10000, 0xf7260c9e, 4 | BRF_GRA },           // 19
	{ "rev2_narc_image_rom_u88.u88",					0x10000, 0xedc19f42, 4 | BRF_GRA },           // 20
	{ "rev2_narc_image_rom_u87.u87",					0x10000, 0xd9b42ff9, 4 | BRF_GRA },           // 21
	{ "rev2_narc_image_rom_u86.u86",					0x10000, 0xaf7daad3, 4 | BRF_GRA },           // 22
	{ "rev2_narc_image_rom_u85.u85",					0x10000, 0x095fae6b, 4 | BRF_GRA },           // 23
	{ "rev2_narc_image_rom_u84.u84",					0x10000, 0x3fdf2057, 4 | BRF_GRA },           // 24
	{ "rev2_narc_image_rom_u83.u83",					0x10000, 0xf2d27c9f, 4 | BRF_GRA },           // 25
	{ "rev2_narc_image_rom_u82.u82",					0x10000, 0x962ce47c, 4 | BRF_GRA },           // 26
	{ "rev2_narc_image_rom_u81.u81",					0x10000, 0x00fe59ec, 4 | BRF_GRA },           // 27
	{ "rev2_narc_image_rom_u80.u80",					0x10000, 0x147ba8e9, 4 | BRF_GRA },           // 28
	{ "rev2_narc_image_rom_u76.u76",					0x10000, 0x1cd897f4, 4 | BRF_GRA },           // 29
	{ "rev2_narc_image_rom_u75.u75",					0x10000, 0x78abfa01, 4 | BRF_GRA },           // 30
	{ "rev2_narc_image_rom_u74.u74",					0x10000, 0x66d2a234, 4 | BRF_GRA },           // 31
	{ "rev2_narc_image_rom_u73.u73",					0x10000, 0xefa5cd4e, 4 | BRF_GRA },           // 32
	{ "rev2_narc_image_rom_u72.u72",					0x10000, 0x70638eb5, 4 | BRF_GRA },           // 33
	{ "rev2_narc_image_rom_u71.u71",					0x10000, 0x61226883, 4 | BRF_GRA },           // 34
	{ "rev2_narc_image_rom_u70.u70",					0x10000, 0xc808849f, 4 | BRF_GRA },           // 35
	{ "rev2_narc_image_rom_u69.u69",					0x10000, 0xe7f9c34f, 4 | BRF_GRA },           // 36
	{ "rev2_narc_image_rom_u68.u68",					0x10000, 0x88a634d5, 4 | BRF_GRA },           // 37
	{ "rev2_narc_image_rom_u67.u67",					0x10000, 0x4ab8b69e, 4 | BRF_GRA },           // 38
	{ "rev2_narc_image_rom_u66.u66",					0x10000, 0xe1da4b25, 4 | BRF_GRA },           // 39
	{ "rev2_narc_image_rom_u65.u65",					0x10000, 0x6df0d125, 4 | BRF_GRA },           // 40
	{ "rev2_narc_image_rom_u64.u64",					0x10000, 0xabab1b16, 4 | BRF_GRA },           // 41
	{ "rev2_narc_image_rom_u63.u63",					0x10000, 0x80602f31, 4 | BRF_GRA },           // 42
	{ "rev2_narc_image_rom_u62.u62",					0x10000, 0xc2a476d1, 4 | BRF_GRA },           // 43
	{ "rev2_narc_image_rom_u58.u58",					0x10000, 0x8a7501e3, 4 | BRF_GRA },           // 44
	{ "rev2_narc_image_rom_u57.u57",					0x10000, 0xa504735f, 4 | BRF_GRA },           // 45
	{ "rev2_narc_image_rom_u56.u56",					0x10000, 0x55f8cca7, 4 | BRF_GRA },           // 46
	{ "rev2_narc_image_rom_u55.u55",					0x10000, 0xd3c932c1, 4 | BRF_GRA },           // 47
	{ "rev2_narc_image_rom_u54.u54",					0x10000, 0xc7f4134b, 4 | BRF_GRA },           // 48
	{ "rev2_narc_image_rom_u53.u53",					0x10000, 0x6be4da56, 4 | BRF_GRA },           // 49
	{ "rev2_narc_image_rom_u52.u52",					0x10000, 0x1ea36a4a, 4 | BRF_GRA },           // 50
	{ "rev2_narc_image_rom_u51.u51",					0x10000, 0x9d4b0324, 4 | BRF_GRA },           // 51
	{ "rev2_narc_image_rom_u50.u50",					0x10000, 0x6f9f0c26, 4 | BRF_GRA },           // 52
	{ "rev2_narc_image_rom_u49.u49",					0x10000, 0x80386fce, 4 | BRF_GRA },           // 53
	{ "rev2_narc_image_rom_u48.u48",					0x10000, 0x05c16185, 4 | BRF_GRA },           // 54
	{ "rev2_narc_image_rom_u47.u47",					0x10000, 0x4c0151f1, 4 | BRF_GRA },           // 55
	{ "rev2_narc_image_rom_u46.u46",					0x10000, 0x5670bfcb, 4 | BRF_GRA },           // 56
	{ "rev2_narc_image_rom_u45.u45",					0x10000, 0x27f10d98, 4 | BRF_GRA },           // 57
	{ "rev2_narc_image_rom_u44.u44",					0x10000, 0x93b8eaa4, 4 | BRF_GRA },           // 58
	{ "rev2_narc_image_rom_u40.u40",					0x10000, 0x7fcaebc7, 4 | BRF_GRA },           // 59
	{ "rev2_narc_image_rom_u39.u39",					0x10000, 0x7db5cf52, 4 | BRF_GRA },           // 60
	{ "rev2_narc_image_rom_u38.u38",					0x10000, 0x3f9f3ef7, 4 | BRF_GRA },           // 61
	{ "rev2_narc_image_rom_u37.u37",					0x10000, 0xed81826c, 4 | BRF_GRA },           // 62
	{ "rev2_narc_image_rom_u36.u36",					0x10000, 0xe5d855c0, 4 | BRF_GRA },           // 63
	{ "rev2_narc_image_rom_u35.u35",					0x10000, 0x3a7b1329, 4 | BRF_GRA },           // 64
	{ "rev2_narc_image_rom_u34.u34",					0x10000, 0xfe982b0e, 4 | BRF_GRA },           // 65
	{ "rev2_narc_image_rom_u33.u33",					0x10000, 0x6bc7eb0f, 4 | BRF_GRA },           // 66
	{ "rev2_narc_image_rom_u32.u32",					0x10000, 0x5875a6d3, 4 | BRF_GRA },           // 67
	{ "rev2_narc_image_rom_u31.u31",					0x10000, 0x2fa4b8e5, 4 | BRF_GRA },           // 68
	{ "rev2_narc_image_rom_u30.u30",					0x10000, 0x7e4bb8ee, 4 | BRF_GRA },           // 69
	{ "rev2_narc_image_rom_u29.u29",					0x10000, 0x45136fd9, 4 | BRF_GRA },           // 70
	{ "rev2_narc_image_rom_u28.u28",					0x10000, 0xd6cdac24, 4 | BRF_GRA },           // 71
	{ "rev2_narc_image_rom_u27.u27",					0x10000, 0x4d33bbec, 4 | BRF_GRA },           // 72
	{ "rev2_narc_image_rom_u26.u26",					0x10000, 0xcb19f784, 4 | BRF_GRA },           // 73
};

STD_ROM_PICK(narc3)
STD_ROM_FN(narc3)

struct BurnDriver BurnDrvNarc3 = {
	"narc3", "narc", NULL, NULL, "1988",
	"Narc (rev 3.20)\0", NULL, "Williams", "Z Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, narc3RomInfo, narc3RomName, NULL, NULL, NULL, NULL, NarcInputInfo, NarcDIPInfo,
	NarcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 8192,
	512, 400, 4, 3
};


// Narc (rev 2.00)

static struct BurnRomInfo narc2RomDesc[] = {
	{ "rev2_narc_sound_rom_u4.u4",						0x10000, 0x450a591a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code (Sound)
	{ "rev2_narc_sound_rom_u5.u5",						0x10000, 0xe551e5e3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rev2_narc_sound_rom_u35.u35",					0x10000, 0x81295892, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code (Sound)
	{ "rev2_narc_sound_rom_u36.u36",					0x10000, 0x16cdbb13, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "rev2_narc_sound_rom_u37.u37",					0x10000, 0x29dbeffd, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "rev2_narc_sound_rom_u38.u38",					0x10000, 0x09b03b80, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "rev2_narc_game_rom_u78.u78",						0x10000, 0x150c2dc4, (3 | 8) | BRF_PRG | BRF_ESS }, //  6 TMS34010 Code
	{ "rev2_narc_game_rom_u60.u60",						0x10000, 0x9720ddea, (3 | 8) | BRF_PRG | BRF_ESS }, //  7
	{ "rev2_narc_game_rom_u77.u77",						0x10000, 0x75ba4c74, (3 | 8) | BRF_PRG | BRF_ESS }, //  8
	{ "rev2_narc_game_rom_u59.u59",						0x10000, 0xf7c6c104, (3 | 8) | BRF_PRG | BRF_ESS }, //  9
	{ "rev2_narc_game_rom_u42.u42",						0x10000, 0x3db20bb8, (3 | 8) | BRF_PRG | BRF_ESS }, // 10
	{ "rev2_narc_game_rom_u24.u24",						0x10000, 0x91bae451, (3 | 8) | BRF_PRG | BRF_ESS }, // 11
	{ "rev2_narc_game_rom_u41.u41",						0x10000, 0xb0d463e1, (3 | 8) | BRF_PRG | BRF_ESS }, // 12
	{ "rev2_narc_game_rom_u23.u23",						0x10000, 0xa9eb4825, (3 | 8) | BRF_PRG | BRF_ESS }, // 13

	{ "rev2_narc_image_rom_u94.u94",					0x10000, 0xca3194e4, 4 | BRF_GRA },           // 14 Graphics (Blitter data)
	{ "rev2_narc_image_rom_u93.u93",					0x10000, 0x0ed7f7f5, 4 | BRF_GRA },           // 15
	{ "rev2_narc_image_rom_u92.u92",					0x10000, 0x40d2fc66, 4 | BRF_GRA },           // 16
	{ "rev2_narc_image_rom_u91.u91",					0x10000, 0xf39325e0, 4 | BRF_GRA },           // 17
	{ "rev2_narc_image_rom_u90.u90",					0x10000, 0x0132aefa, 4 | BRF_GRA },           // 18
	{ "rev2_narc_image_rom_u89.u89",					0x10000, 0xf7260c9e, 4 | BRF_GRA },           // 19
	{ "rev2_narc_image_rom_u88.u88",					0x10000, 0xedc19f42, 4 | BRF_GRA },           // 20
	{ "rev2_narc_image_rom_u87.u87",					0x10000, 0xd9b42ff9, 4 | BRF_GRA },           // 21
	{ "rev2_narc_image_rom_u86.u86",					0x10000, 0xaf7daad3, 4 | BRF_GRA },           // 22
	{ "rev2_narc_image_rom_u85.u85",					0x10000, 0x095fae6b, 4 | BRF_GRA },           // 23
	{ "rev2_narc_image_rom_u84.u84",					0x10000, 0x3fdf2057, 4 | BRF_GRA },           // 24
	{ "rev2_narc_image_rom_u83.u83",					0x10000, 0xf2d27c9f, 4 | BRF_GRA },           // 25
	{ "rev2_narc_image_rom_u82.u82",					0x10000, 0x962ce47c, 4 | BRF_GRA },           // 26
	{ "rev2_narc_image_rom_u81.u81",					0x10000, 0x00fe59ec, 4 | BRF_GRA },           // 27
	{ "rev2_narc_image_rom_u80.u80",					0x10000, 0x147ba8e9, 4 | BRF_GRA },           // 28
	{ "rev2_narc_image_rom_u76.u76",					0x10000, 0x1cd897f4, 4 | BRF_GRA },           // 29
	{ "rev2_narc_image_rom_u75.u75",					0x10000, 0x78abfa01, 4 | BRF_GRA },           // 30
	{ "rev2_narc_image_rom_u74.u74",					0x10000, 0x66d2a234, 4 | BRF_GRA },           // 31
	{ "rev2_narc_image_rom_u73.u73",					0x10000, 0xefa5cd4e, 4 | BRF_GRA },           // 32
	{ "rev2_narc_image_rom_u72.u72",					0x10000, 0x70638eb5, 4 | BRF_GRA },           // 33
	{ "rev2_narc_image_rom_u71.u71",					0x10000, 0x61226883, 4 | BRF_GRA },           // 34
	{ "rev2_narc_image_rom_u70.u70",					0x10000, 0xc808849f, 4 | BRF_GRA },           // 35
	{ "rev2_narc_image_rom_u69.u69",					0x10000, 0xe7f9c34f, 4 | BRF_GRA },           // 36
	{ "rev2_narc_image_rom_u68.u68",					0x10000, 0x88a634d5, 4 | BRF_GRA },           // 37
	{ "rev2_narc_image_rom_u67.u67",					0x10000, 0x4ab8b69e, 4 | BRF_GRA },           // 38
	{ "rev2_narc_image_rom_u66.u66",					0x10000, 0xe1da4b25, 4 | BRF_GRA },           // 39
	{ "rev2_narc_image_rom_u65.u65",					0x10000, 0x6df0d125, 4 | BRF_GRA },           // 40
	{ "rev2_narc_image_rom_u64.u64",					0x10000, 0xabab1b16, 4 | BRF_GRA },           // 41
	{ "rev2_narc_image_rom_u63.u63",					0x10000, 0x80602f31, 4 | BRF_GRA },           // 42
	{ "rev2_narc_image_rom_u62.u62",					0x10000, 0xc2a476d1, 4 | BRF_GRA },           // 43
	{ "rev2_narc_image_rom_u58.u58",					0x10000, 0x8a7501e3, 4 | BRF_GRA },           // 44
	{ "rev2_narc_image_rom_u57.u57",					0x10000, 0xa504735f, 4 | BRF_GRA },           // 45
	{ "rev2_narc_image_rom_u56.u56",					0x10000, 0x55f8cca7, 4 | BRF_GRA },           // 46
	{ "rev2_narc_image_rom_u55.u55",					0x10000, 0xd3c932c1, 4 | BRF_GRA },           // 47
	{ "rev2_narc_image_rom_u54.u54",					0x10000, 0xc7f4134b, 4 | BRF_GRA },           // 48
	{ "rev2_narc_image_rom_u53.u53",					0x10000, 0x6be4da56, 4 | BRF_GRA },           // 49
	{ "rev2_narc_image_rom_u52.u52",					0x10000, 0x1ea36a4a, 4 | BRF_GRA },           // 50
	{ "rev2_narc_image_rom_u51.u51",					0x10000, 0x9d4b0324, 4 | BRF_GRA },           // 51
	{ "rev2_narc_image_rom_u50.u50",					0x10000, 0x6f9f0c26, 4 | BRF_GRA },           // 52
	{ "rev2_narc_image_rom_u49.u49",					0x10000, 0x80386fce, 4 | BRF_GRA },           // 53
	{ "rev2_narc_image_rom_u48.u48",					0x10000, 0x05c16185, 4 | BRF_GRA },           // 54
	{ "rev2_narc_image_rom_u47.u47",					0x10000, 0x4c0151f1, 4 | BRF_GRA },           // 55
	{ "rev2_narc_image_rom_u46.u46",					0x10000, 0x5670bfcb, 4 | BRF_GRA },           // 56
	{ "rev2_narc_image_rom_u45.u45",					0x10000, 0x27f10d98, 4 | BRF_GRA },           // 57
	{ "rev2_narc_image_rom_u44.u44",					0x10000, 0x93b8eaa4, 4 | BRF_GRA },           // 58
	{ "rev2_narc_image_rom_u40.u40",					0x10000, 0x7fcaebc7, 4 | BRF_GRA },           // 59
	{ "rev2_narc_image_rom_u39.u39",					0x10000, 0x7db5cf52, 4 | BRF_GRA },           // 60
	{ "rev2_narc_image_rom_u38.u38",					0x10000, 0x3f9f3ef7, 4 | BRF_GRA },           // 61
	{ "rev2_narc_image_rom_u37.u37",					0x10000, 0xed81826c, 4 | BRF_GRA },           // 62
	{ "rev2_narc_image_rom_u36.u36",					0x10000, 0xe5d855c0, 4 | BRF_GRA },           // 63
	{ "rev2_narc_image_rom_u35.u35",					0x10000, 0x3a7b1329, 4 | BRF_GRA },           // 64
	{ "rev2_narc_image_rom_u34.u34",					0x10000, 0xfe982b0e, 4 | BRF_GRA },           // 65
	{ "rev2_narc_image_rom_u33.u33",					0x10000, 0x6bc7eb0f, 4 | BRF_GRA },           // 66
	{ "rev2_narc_image_rom_u32.u32",					0x10000, 0x5875a6d3, 4 | BRF_GRA },           // 67
	{ "rev2_narc_image_rom_u31.u31",					0x10000, 0x2fa4b8e5, 4 | BRF_GRA },           // 68
	{ "rev2_narc_image_rom_u30.u30",					0x10000, 0x7e4bb8ee, 4 | BRF_GRA },           // 69
	{ "rev2_narc_image_rom_u29.u29",					0x10000, 0x45136fd9, 4 | BRF_GRA },           // 70
	{ "rev2_narc_image_rom_u28.u28",					0x10000, 0xd6cdac24, 4 | BRF_GRA },           // 71
	{ "rev2_narc_image_rom_u27.u27",					0x10000, 0x4d33bbec, 4 | BRF_GRA },           // 72
	{ "rev2_narc_image_rom_u26.u26",					0x10000, 0xcb19f784, 4 | BRF_GRA },           // 73
};

STD_ROM_PICK(narc2)
STD_ROM_FN(narc2)

struct BurnDriver BurnDrvNarc2 = {
	"narc2", "narc", NULL, NULL, "1988",
	"Narc (rev 2.00)\0", NULL, "Williams", "Z Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, narc2RomInfo, narc2RomName, NULL, NULL, NULL, NULL, NarcInputInfo, NarcDIPInfo,
	NarcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 8192,
	512, 400, 4, 3
};


// Narc (rev 1.80)

static struct BurnRomInfo narc1RomDesc[] = {
	{ "rev1_narc_sound_rom_u4.u4",						0x10000, 0x345b5b0b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code (Sound)
	{ "rev1_narc_sound_rom_u5.u5",						0x10000, 0xbfa112b7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rev1_narc_sound_rom_u35.u35",					0x10000, 0xc932e69f, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code (Sound)
	{ "rev1_narc_sound_rom_u36.u36",					0x10000, 0x974ad9f4, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "rev1_narc_sound_rom_u37.u37",					0x10000, 0xb6469f79, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "rev1_narc_sound_rom_u38.u38",					0x10000, 0xc535d11c, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "rev1_narc_game_rom_u42.u42",						0x20000, 0x145e1346, 3 | BRF_PRG | BRF_ESS }, //  6 TMS34010 Code
	{ "rev1_narc_game_rom_u24.u24",						0x20000, 0x6ac3ba9a, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "rev1_narc_game_rom_u41.u41",						0x20000, 0x68723c97, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "rev1_narc_game_rom_u23.u23",						0x20000, 0x78ecc8e5, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "rev1_narc_image_rom_u94.u94",					0x10000, 0x1b4cec52, 4 | BRF_GRA },           // 10 Graphics (Blitter data)
	{ "rev1_narc_image_rom_u93.u93",					0x10000, 0x6e69f198, 4 | BRF_GRA },           // 11
	{ "rev1_narc_image_rom_u92.u92",					0x10000, 0x4a0cb243, 4 | BRF_GRA },           // 12
	{ "rev1_narc_image_rom_u91.u91",					0x10000, 0x38759eb9, 4 | BRF_GRA },           // 13
	{ "rev1_narc_image_rom_u90.u90",					0x10000, 0x33cf4379, 4 | BRF_GRA },           // 14
	{ "rev1_narc_image_rom_u89.u89",					0x10000, 0x3cbd682e, 4 | BRF_GRA },           // 15
	{ "rev1_narc_image_rom_u88.u88",					0x10000, 0x4fcfad66, 4 | BRF_GRA },           // 16
	{ "rev1_narc_image_rom_u87.u87",					0x10000, 0xafc56436, 4 | BRF_GRA },           // 17
	{ "rev1_narc_image_rom_u86.u86",					0x10000, 0xf1d02184, 4 | BRF_GRA },           // 18
	{ "rev1_narc_image_rom_u85.u85",					0x10000, 0xdfe851c8, 4 | BRF_GRA },           // 19
	{ "rev1_narc_image_rom_u84.u84",					0x10000, 0x8c28c9c4, 4 | BRF_GRA },           // 20
	{ "rev1_narc_image_rom_u83.u83",					0x10000, 0xf65cf5ac, 4 | BRF_GRA },           // 21
	{ "rev1_narc_image_rom_u82.u82",					0x10000, 0x153cb85f, 4 | BRF_GRA },           // 22
	{ "rev1_narc_image_rom_u81.u81",					0x10000, 0x61d873fb, 4 | BRF_GRA },           // 23
	{ "rev1_narc_image_rom_u76.u76",					0x10000, 0x7b5d553a, 4 | BRF_GRA },           // 24
	{ "rev1_narc_image_rom_u75.u75",					0x10000, 0xb49a3295, 4 | BRF_GRA },           // 25
	{ "rev1_narc_image_rom_u74.u74",					0x10000, 0x4767fbd3, 4 | BRF_GRA },           // 26
	{ "rev1_narc_image_rom_u73.u73",					0x10000, 0xc2c5be8b, 4 | BRF_GRA },           // 27
	{ "rev1_narc_image_rom_u72.u72",					0x10000, 0xa4b8e0e4, 4 | BRF_GRA },           // 28
	{ "rev1_narc_image_rom_u71.u71",					0x10000, 0x02fde40d, 4 | BRF_GRA },           // 29
	{ "rev1_narc_image_rom_u70.u70",					0x10000, 0xe53d378e, 4 | BRF_GRA },           // 30
	{ "rev1_narc_image_rom_u69.u69",					0x10000, 0x3b00bacc, 4 | BRF_GRA },           // 31
	{ "rev1_narc_image_rom_u68.u68",					0x10000, 0xbd3ccb95, 4 | BRF_GRA },           // 32
	{ "rev1_narc_image_rom_u67.u67",					0x10000, 0x2e30c11b, 4 | BRF_GRA },           // 33
	{ "rev1_narc_image_rom_u66.u66",					0x10000, 0x1bb2da9d, 4 | BRF_GRA },           // 34
	{ "rev1_narc_image_rom_u65.u65",					0x10000, 0x1b928ebb, 4 | BRF_GRA },           // 35
	{ "rev1_narc_image_rom_u64.u64",					0x10000, 0x69539c8a, 4 | BRF_GRA },           // 36
	{ "rev1_narc_image_rom_u63.u63",					0x10000, 0x1254a7f8, 4 | BRF_GRA },           // 37
	{ "rev1_narc_image_rom_u58.u58",					0x10000, 0x82cb4229, 4 | BRF_GRA },           // 38
	{ "rev1_narc_image_rom_u57.u57",					0x10000, 0x4309d12f, 4 | BRF_GRA },           // 39
	{ "rev1_narc_image_rom_u56.u56",					0x10000, 0x6c4b7b3a, 4 | BRF_GRA },           // 40
	{ "rev1_narc_image_rom_u55.u55",					0x10000, 0xedb40703, 4 | BRF_GRA },           // 41
	{ "rev1_narc_image_rom_u54.u54",					0x10000, 0x9643faad, 4 | BRF_GRA },           // 42
	{ "rev1_narc_image_rom_u53.u53",					0x10000, 0xd6695995, 4 | BRF_GRA },           // 43
	{ "rev1_narc_image_rom_u52.u52",					0x10000, 0x59258d8b, 4 | BRF_GRA },           // 44
	{ "rev1_narc_image_rom_u51.u51",					0x10000, 0xc0fd44fc, 4 | BRF_GRA },           // 45
	{ "rev1_narc_image_rom_u50.u50",					0x10000, 0x7cacd767, 4 | BRF_GRA },           // 46
	{ "rev1_narc_image_rom_u49.u49",					0x10000, 0x64440a70, 4 | BRF_GRA },           // 47
	{ "rev1_narc_image_rom_u48.u48",					0x10000, 0x45162e8a, 4 | BRF_GRA },           // 48
	{ "rev1_narc_image_rom_u47.u47",					0x10000, 0xac5c7f57, 4 | BRF_GRA },           // 49
	{ "rev1_narc_image_rom_u46.u46",					0x10000, 0x8bb8d8e1, 4 | BRF_GRA },           // 50
	{ "rev1_narc_image_rom_u45.u45",					0x10000, 0xb60466c0, 4 | BRF_GRA },           // 51
	{ "rev1_narc_image_rom_u40.u40",					0x10000, 0x02ce306a, 4 | BRF_GRA },           // 52
	{ "rev1_narc_image_rom_u39.u39",					0x10000, 0x6d702163, 4 | BRF_GRA },           // 53
	{ "rev1_narc_image_rom_u38.u38",					0x10000, 0x349988cf, 4 | BRF_GRA },           // 54
	{ "rev1_narc_image_rom_u37.u37",					0x10000, 0x0f8e6588, 4 | BRF_GRA },           // 55
	{ "rev1_narc_image_rom_u36.u36",					0x10000, 0x645723cd, 4 | BRF_GRA },           // 56
	{ "rev1_narc_image_rom_u35.u35",					0x10000, 0x00661dec, 4 | BRF_GRA },           // 57
	{ "rev1_narc_image_rom_u34.u34",					0x10000, 0x89dcbf17, 4 | BRF_GRA },           // 58
	{ "rev1_narc_image_rom_u33.u33",					0x10000, 0xad220197, 4 | BRF_GRA },           // 59
	{ "rev1_narc_image_rom_u32.u32",					0x10000, 0x40aa3f31, 4 | BRF_GRA },           // 60
	{ "rev1_narc_image_rom_u31.u31",					0x10000, 0x81cf74aa, 4 | BRF_GRA },           // 61
	{ "rev1_narc_image_rom_u30.u30",					0x10000, 0xb3e07443, 4 | BRF_GRA },           // 62
	{ "rev1_narc_image_rom_u29.u29",					0x10000, 0xfdaedb84, 4 | BRF_GRA },           // 63
	{ "rev1_narc_image_rom_u28.u28",					0x10000, 0x3012cd6e, 4 | BRF_GRA },           // 64
	{ "rev1_narc_image_rom_u27.u27",					0x10000, 0xe631fe7d, 4 | BRF_GRA },           // 65
};

STD_ROM_PICK(narc1)
STD_ROM_FN(narc1)

struct BurnDriver BurnDrvNarc1 = {
	"narc1", "narc", NULL, NULL, "1988",
	"Narc (rev 1.80)\0", NULL, "Williams", "Z Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, narc1RomInfo, narc1RomName, NULL, NULL, NULL, NULL, NarcInputInfo, NarcDIPInfo,
	NarcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 8192,
	512, 400, 4, 3
};


// Mortal Kombat (rev 4.0 09/28/92)

static struct BurnRomInfo mkla4RomDesc[] = {
	{ "l1_mortal_kombat_u3_sound_rom.u3",				0x40000, 0xc615844c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "l1_mortal_kombat_u12_sound_rom.u12",				0x40000, 0x258bd7f9, 2 | BRF_SND },           //  1 MSM6295 Samples
	{ "l1_mortal_kombat_u13_sound_rom.u13",				0x40000, 0x7b7ec3b6, 2 | BRF_SND },           //  2

	{ "l4_mortal_kombat_game_rom_u-105.u105",			0x80000, 0x29af348f, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "l4_mortal_kombat_game_rom_u-89.u89",				0x80000, 0x1ad76662, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkla4)
STD_ROM_FN(mkla4)

static INT32 MkInit()
{
	static const struct protection_data mk_protection_data =
	{
		{ 0x0d00, 0x0c00, 0x0900 },
		{ 0x4600, 0xf600, 0xa600, 0x0600, 0x2600, 0x9600, 0xc600, 0xe600, 0x8600, 0x7600, 0x8600, 0x8600, 0x9600, 0xd600, 0x6600, 0xb600, 0xd600, 0xe600, 0xf600, 0x7600, 0xb600, 0xa600, 0x3600 }
	};

	prot_data = &mk_protection_data;

	return CommonInit(NULL, 0, 48000000, 6, 0xfb9c, 0xfbc6);
}

struct BurnDriver BurnDrvMkla4 = {
	"mkla4", "mk", NULL, NULL, "1992",
	"Mortal Kombat (rev 4.0 09/28/92)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkla4RomInfo, mkla4RomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	MkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (rev 3.0 08/31/92)

static struct BurnRomInfo mkla3RomDesc[] = {
	{ "l1_mortal_kombat_u3_sound_rom.u3",				0x40000, 0xc615844c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "l1_mortal_kombat_u12_sound_rom.u12",				0x40000, 0x258bd7f9, 2 | BRF_SND },           //  1 MSM6295 Samples
	{ "l1_mortal_kombat_u13_sound_rom.u13",				0x40000, 0x7b7ec3b6, 2 | BRF_SND },           //  2

	{ "l3_mortal_kombat_game_rom_u-105.u105",			0x80000, 0x2ce843c5, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "l3_mortal_kombat_game_rom_u-89.u89",				0x80000, 0x49a46e10, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkla3)
STD_ROM_FN(mkla3)

struct BurnDriver BurnDrvMkla3 = {
	"mkla3", "mk", NULL, NULL, "1992",
	"Mortal Kombat (rev 3.0 08/31/92)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkla3RomInfo, mkla3RomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	MkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (rev 2.0 08/18/92)

static struct BurnRomInfo mkla2RomDesc[] = {
	{ "l1_mortal_kombat_u3_sound_rom.u3",				0x40000, 0xc615844c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "l1_mortal_kombat_u12_sound_rom.u12",				0x40000, 0x258bd7f9, 2 | BRF_SND },           //  1 MSM6295 Samples
	{ "l1_mortal_kombat_u13_sound_rom.u13",				0x40000, 0x7b7ec3b6, 2 | BRF_SND },           //  2

	{ "l2_mortal_kombat_game_rom_u-105.u105",			0x80000, 0x8531d44e, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "l2_mortal_kombat_game_rom_u-89.u89",				0x80000, 0xb88dc26e, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkla2)
STD_ROM_FN(mkla2)

struct BurnDriver BurnDrvMkla2 = {
	"mkla2", "mk", NULL, NULL, "1992",
	"Mortal Kombat (rev 2.0 08/18/92)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkla2RomInfo, mkla2RomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	MkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (rev 1.0 08/09/92)

static struct BurnRomInfo mkla1RomDesc[] = {
	{ "l1_mortal_kombat_u3_sound_rom.u3",				0x40000, 0xc615844c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "l1_mortal_kombat_u12_sound_rom.u12",				0x40000, 0x258bd7f9, 2 | BRF_SND },           //  1 MSM6295 Samples
	{ "l1_mortal_kombat_u13_sound_rom.u13",				0x40000, 0x7b7ec3b6, 2 | BRF_SND },           //  2

	{ "l1_mortal_kombat_game_rom_u-105.u105",			0x80000, 0xe1f7b4c9, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "l1_mortal_kombat_game_rom_u-89.u89",				0x80000, 0x9d38ac75, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkla1)
STD_ROM_FN(mkla1)

struct BurnDriver BurnDrvMkla1 = {
	"mkla1", "mk", NULL, NULL, "1992",
	"Mortal Kombat (rev 1.0 08/09/92)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkla1RomInfo, mkla1RomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	MkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (prototype, rev 9.0 07/28/92)

static struct BurnRomInfo mkprot9RomDesc[] = {
	{ "l1_mortal_kombat_u3_sound_rom.u3",				0x40000, 0xc615844c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "l1_mortal_kombat_u12_sound_rom.u12",				0x40000, 0x258bd7f9, 2 | BRF_SND },           //  1 MSM6295 Samples
	{ "l1_mortal_kombat_u13_sound_rom.u13",				0x40000, 0x7b7ec3b6, 2 | BRF_SND },           //  2

	{ "proto9_mortal_kombat_game_rom_u-105.u105",		0x80000, 0x20772bbd, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "proto9_mortal_kombat_game_rom_u-89.u89",			0x80000, 0x3238d45b, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkprot9)
STD_ROM_FN(mkprot9)

static INT32 Mkprot9Init()
{
	static const struct protection_data mk_protection_data =
	{
		{ 0x0d00, 0x0c00, 0x0900 },
		{ 0x4600, 0xf600, 0xa600, 0x0600, 0x2600, 0x9600, 0xc600, 0xe600, 0x8600, 0x7600, 0x8600, 0x8600, 0x9600, 0xd600, 0x6600, 0xb600, 0xd600, 0xe600, 0xf600, 0x7600, 0xb600, 0xa600, 0x3600 }
	};

	prot_data = &mk_protection_data;

	return CommonInit(NULL, 0, 50000000, 6, 0xfb9c, 0xfbc6);
}

struct BurnDriver BurnDrvMkprot9 = {
	"mkprot9", "mk", NULL, NULL, "1992",
	"Mortal Kombat (prototype, rev 9.0 07/28/92)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkprot9RomInfo, mkprot9RomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	Mkprot9Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (prototype, rev 8.0 07/21/92)

static struct BurnRomInfo mkprot8RomDesc[] = {
	{ "l1_mortal_kombat_u3_sound_rom.u3",				0x40000, 0xc615844c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "l1_mortal_kombat_u12_sound_rom.u12",				0x40000, 0x258bd7f9, 2 | BRF_SND },           //  1 MSM6295 Samples
	{ "l1_mortal_kombat_u13_sound_rom.u13",				0x40000, 0x7b7ec3b6, 2 | BRF_SND },           //  2

	{ "proto8_mortal_kombat_game_rom_u-105.u105",		0x80000, 0x2f3c095d, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "proto8_mortal_kombat_game_rom_u-89.u89",			0x80000, 0xedcf217f, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkprot8)
STD_ROM_FN(mkprot8)

struct BurnDriver BurnDrvMkprot8 = {
	"mkprot8", "mk", NULL, NULL, "1992",
	"Mortal Kombat (prototype, rev 8.0 07/21/92)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkprot8RomInfo, mkprot8RomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	Mkprot9Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (prototype, rev 4.0 07/14/92)

static struct BurnRomInfo mkprot4RomDesc[] = {
	{ "l1_mortal_kombat_u3_sound_rom.u3",				0x40000, 0xc615844c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "l1_mortal_kombat_u12_sound_rom.u12",				0x40000, 0x258bd7f9, 2 | BRF_SND },           //  1 MSM6295 Samples
	{ "l1_mortal_kombat_u13_sound_rom.u13",				0x40000, 0x7b7ec3b6, 2 | BRF_SND },           //  2

	{ "proto4_mortal_kombat_game_rom_u-105.u105",		0x80000, 0xd7f8d78b, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "proto4_mortal_kombat_game_rom_u-89.u89",			0x80000, 0xa6b5d6d2, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkprot4)
STD_ROM_FN(mkprot4)

struct BurnDriver BurnDrvMkprot4 = {
	"mkprot4", "mk", NULL, NULL, "1992",
	"Mortal Kombat (prototype, rev 4.0 07/14/92)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkprot4RomInfo, mkprot4RomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	Mkprot9Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (Turbo 3.1 09/09/93, hack)

static struct BurnRomInfo mkyturboRomDesc[] = {
	{ "l1_mortal_kombat_u3_sound_rom.u3",				0x40000, 0xc615844c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "l1_mortal_kombat_u12_sound_rom.u12",				0x40000, 0x258bd7f9, 2 | BRF_SND },           //  1 MSM6295 Samples
	{ "l1_mortal_kombat_u13_sound_rom.u13",				0x40000, 0x7b7ec3b6, 2 | BRF_SND },           //  2

	{ "kombo-rom-u105.bin",								0x80000, 0x80d5618c, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "kombo-rom-u89.bin",								0x80000, 0x450788e3, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkyturbo)
STD_ROM_FN(mkyturbo)

static INT32 MkyturboInit()
{
	is_mkturbo = 1;

	return MkInit();
}

struct BurnDriver BurnDrvMkyturbo = {
	"mkyturbo", "mk", NULL, NULL, "1992",
	"Mortal Kombat (Turbo 3.1 09/09/93, hack)\0", NULL, "hack", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkyturboRomInfo, mkyturboRomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	MkyturboInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (Turbo 3.0 08/31/92, hack)

static struct BurnRomInfo mkyturboeRomDesc[] = {
	{ "l1_mortal_kombat_u3_sound_rom.u3",				0x40000, 0xc615844c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "l1_mortal_kombat_u12_sound_rom.u12",				0x40000, 0x258bd7f9, 2 | BRF_SND },           //  1 MSM6295 Samples
	{ "l1_mortal_kombat_u13_sound_rom.u13",				0x40000, 0x7b7ec3b6, 2 | BRF_SND },           //  2

	{ "turbo30.u105",									0x80000, 0x59747c59, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "turbo30.u89",									0x80000, 0x84d66a75, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkyturboe)
STD_ROM_FN(mkyturboe)

struct BurnDriver BurnDrvMkyturboe = {
	"mkyturboe", "mk", NULL, NULL, "1992",
	"Mortal Kombat (Turbo 3.0 08/31/92, hack)\0", NULL, "hack", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkyturboeRomInfo, mkyturboeRomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	MkyturboInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (Reptile Man hack)

static struct BurnRomInfo mkrepRomDesc[] = {
	{ "l1_mortal_kombat_u3_sound_rom.u3",				0x40000, 0xc615844c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "l1_mortal_kombat_u12_sound_rom.u12",				0x40000, 0x258bd7f9, 2 | BRF_SND },           //  1 MSM6295 Samples
	{ "l1_mortal_kombat_u13_sound_rom.u13",				0x40000, 0x7b7ec3b6, 2 | BRF_SND },           //  2

	{ "u105",											0x80000, 0x49a352ed, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "u89",											0x80000, 0x5d5113b2, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkrep)
STD_ROM_FN(mkrep)

struct BurnDriverD BurnDrvMkrep = {
	"mkrep", "mk", NULL, NULL, "1992",
	"Mortal Kombat (Reptile Man hack)\0", NULL, "hack", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkrepRomInfo, mkrepRomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	MkyturboInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (Nifty Kombo, hack)

static struct BurnRomInfo mkniftyRomDesc[] = {
	{ "l1_mortal_kombat_u3_sound_rom.u3",				0x40000, 0xc615844c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "l1_mortal_kombat_u12_sound_rom.u12",				0x40000, 0x258bd7f9, 2 | BRF_SND },           //  1 MSM6295 Samples
	{ "l1_mortal_kombat_u13_sound_rom.u13",				0x40000, 0x7b7ec3b6, 2 | BRF_SND },           //  2

	{ "nifty.105",										0x80000, 0xc66fd38d, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "nifty.89",										0x80000, 0xbbf8738d, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mknifty)
STD_ROM_FN(mknifty)

struct BurnDriver BurnDrvMknifty = {
	"mknifty", "mk", NULL, NULL, "1992",
	"Mortal Kombat (Nifty Kombo, hack)\0", NULL, "hack", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkniftyRomInfo, mkniftyRomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	MkyturboInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (Nifty Kombo 666, hack)

static struct BurnRomInfo mknifty666RomDesc[] = {
	{ "l1_mortal_kombat_u3_sound_rom.u3",				0x40000, 0xc615844c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "l1_mortal_kombat_u12_sound_rom.u12",				0x40000, 0x258bd7f9, 2 | BRF_SND },           //  1 MSM6295 Samples
	{ "l1_mortal_kombat_u13_sound_rom.u13",				0x40000, 0x7b7ec3b6, 2 | BRF_SND },           //  2

	{ "mortall_kombo_rom_u105-j4.u105.bin",				0x80000, 0x243d8009, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "kombo-u89.u89",									0x80000, 0x7b26a6b1, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mknifty666)
STD_ROM_FN(mknifty666)

struct BurnDriver BurnDrvMknifty666 = {
	"mknifty666", "mk", NULL, NULL, "1992",
	"Mortal Kombat (Nifty Kombo 666, hack)\0", NULL, "hack", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mknifty666RomInfo, mknifty666RomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	MkyturboInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (Yawdim bootleg, set 1)

static struct BurnRomInfo mkyawdimRomDesc[] = {
	{ "1.u67",											0x10000, 0xb58d229e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (sound)

	{ "2.u59",											0x80000, 0xa72ad81e, 2 | BRF_SND },           //  1 Samples
	{ "3.u60",											0x80000, 0x6e68e0b0, 2 | BRF_SND },           //  2

	{ "4.u25",											0x80000, 0xb12b3bf2, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "5.u26",											0x80000, 0x7a37dc5c, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "l1_mortal_kombat_game_rom_u-111.u111",			0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "l1_mortal_kombat_game_rom_u-112.u112",			0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "l1_mortal_kombat_game_rom_u-113.u113",			0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "l1_mortal_kombat_game_rom_u-114.u114",			0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "l1_mortal_kombat_game_rom_u-95.u95",				0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "l1_mortal_kombat_game_rom_u-96.u96",				0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "l1_mortal_kombat_game_rom_u-97.u97",				0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "l1_mortal_kombat_game_rom_u-98.u98",				0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "l1_mortal_kombat_game_rom_u-106.u106",			0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "l1_mortal_kombat_game_rom_u-107.u107",			0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "l1_mortal_kombat_game_rom_u-108.u108",			0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "l1_mortal_kombat_game_rom_u-109.u109",			0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkyawdim)
STD_ROM_FN(mkyawdim)

static void MkyawdimLoadCallback()
{
	memcpy (DrvSndROM[0] + 0x00000, DrvSndROM[0] + 0x10000, 0x10000);

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);

	for (INT32 i = 0; i < 8; i++) {
		memcpy (tmp + (i & 3) * 0x40000 + (((i & 4) >> 2) * 0x20000), DrvSndROM[1] + i * 0x20000, 0x20000);
	}

	memcpy (DrvSndROM[1], tmp, 0x100000);

	BurnFree (tmp);
}

static INT32 MkyawdimInit()
{
	is_yawdim = 1;
	prot_data = NULL;

	return CommonInit(MkyawdimLoadCallback, 3, 40000000, 6, -1, -1);
}

struct BurnDriver BurnDrvMkyawdim = {
	"mkyawdim", "mk", NULL, NULL, "1992",
	"Mortal Kombat (Yawdim bootleg, set 1)\0", NULL, "bootleg (Yawdim)", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkyawdimRomInfo, mkyawdimRomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	MkyawdimInit, DrvExit, YawdimFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (Yawdim bootleg, set 2)

static struct BurnRomInfo mkyawdim2RomDesc[] = {
	{ "yawdim.u167",									0x010000, 0x16da7efb, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (sound)

	{ "yawdim.u159",									0x040000, 0x95b120af, 2 | BRF_SND },           //  1 Samples
	{ "mw-15.u160",										0x080000, 0x6e68e0b0, 2 | BRF_SND },           //  2

	{ "4.u25",											0x080000, 0xb12b3bf2, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "5.u26",											0x080000, 0x7a37dc5c, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "b-1.bin",										0x100000, 0xf41e61c6, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "b-2.bin",										0x100000, 0x8052740b, 4 | BRF_GRA },           //  6
	{ "a-1.bin",										0x100000, 0x7da3cb93, 4 | BRF_GRA },           //  7
	{ "a-2.bin",										0x100000, 0x1eedb0f8, 4 | BRF_GRA },           //  8
	{ "c-1.bin",										0x100000, 0xde27c4c3, 4 | BRF_GRA },           //  9
	{ "c-2.bin",										0x100000, 0xd99203f3, 4 | BRF_GRA },           // 10

	{ "22v10.p1",										0x0002dd, 0x15c24092, 0 | BRF_OPT },           // 11 PLDs
	{ "22v10.p2",										0x0002dd, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 12
	{ "22v10.p3",										0x0002dd, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 13
	{ "16v8.p4",										0x000117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 14
	{ "22v10.p5",										0x0002dd, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 15
	{ "16v8.p6",										0x000117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 16
	{ "16v8.p7",										0x000117, 0xfbbdc832, 0 | BRF_OPT },           // 17
	{ "16v8.p8",										0x000117, 0x8c573ab4, 0 | BRF_OPT },           // 18
	{ "22v10.p9",										0x0002e5, 0x4e68c9ba, 0 | BRF_OPT },           // 19
	{ "20v8.p10",										0x000157, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 20
	{ "20v8.p11",										0x000157, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 21
	{ "22v10.p12",										0x0002dd, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 22
	{ "22v10.p13",										0x0002e5, 0x3ccf1a6f, 0 | BRF_OPT },           // 23
	{ "22v10.p14",										0x0002dd, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 24
	{ "22v10.p15",										0x0002dd, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 25
	{ "20v8.p16",										0x000157, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 26
	{ "20v8.p17",										0x000157, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 27
	{ "22v10.p18",										0x0002dd, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 28
	{ "16v8.p19",										0x000117, 0x0346b5fc, 0 | BRF_OPT },           // 29
};

STD_ROM_PICK(mkyawdim2)
STD_ROM_FN(mkyawdim2)

static void Mkyawdim2LoadCallback()
{
	memcpy (DrvSndROM[0], DrvSndROM[0] + 0x18000, 0x08000);

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);

	for (INT32 i = 0; i < 8; i++) {
		memcpy (tmp + (i * 0x040000), DrvSndROM[1] + (((i & 4) >> 2) * 0x20000), 0x20000);
		memcpy (tmp + 0x020000 + (i * 0x40000), DrvSndROM[1] + 0x80000 + ((i & 3) * 0x20000), 0x20000);
	}

	memcpy (DrvSndROM[1], tmp, 0x200000);

	BurnFree (tmp);
}

static INT32 Mkyawdim2Init()
{
	is_yawdim = 1;
	prot_data = NULL;

	return CommonInit(Mkyawdim2LoadCallback, 3|4 /*|4 == yawdim2*/, 40000000, 6, -1, -1);
}

struct BurnDriver BurnDrvMkyawdim2 = {
	"mkyawdim2", "mk", NULL, NULL, "1992",
	"Mortal Kombat (Yawdim bootleg, set 2)\0", NULL, "bootleg (Yawdim)", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkyawdim2RomInfo, mkyawdim2RomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	Mkyawdim2Init, DrvExit, YawdimFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (Yawdim bootleg, set 3)

static struct BurnRomInfo mkyawdim3RomDesc[] = {
	{ "15.bin",											0x10000, 0xb58d229e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (sound)

	{ "13.bin",											0x20000, 0x921c613d, 2 | BRF_SND },           //  1 Samples
	{ "14.bin",											0x80000, 0x6e68e0b0, 2 | BRF_SND },           //  2

	{ "p1.bin",											0x80000, 0x2337a0f9, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "p2.bin",											0x80000, 0x7a37dc5c, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "12.bin",											0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "8.bin",											0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "10.bin",											0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "3.bin",											0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "6.bin",											0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "1.bin",											0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "11.bin",											0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "2.bin",											0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "4.bin",											0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "7.bin",											0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "9.bin",											0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "5.bin",											0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkyawdim3)
STD_ROM_FN(mkyawdim3)

static void Mkyawdim3LoadCallback()
{
	memcpy (DrvSndROM[0] + 0x00000, DrvSndROM[0] + 0x10000, 0x10000);

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);

	for (INT32 i = 0; i < 8; i++) {
		if (i < 4)
			memcpy (tmp + (i & 3) * 0x40000 + (((i & 4) >> 2) * 0x20000), DrvSndROM[1], 0x20000);
		if (i > 3)
			memcpy (tmp + (i & 3) * 0x40000 + (((i & 4) >> 2) * 0x20000), DrvSndROM[1] + i * 0x20000, 0x20000);
	}

	memcpy (DrvSndROM[1], tmp, 0x100000);

	BurnFree (tmp);
}

static INT32 Mkyawdim3Init()
{
	is_yawdim = 1;
	prot_data = NULL;

	return CommonInit(Mkyawdim3LoadCallback, 3, 40000000, 6, -1, -1);
}

struct BurnDriver BurnDrvMkyawdim3 = {
	"mkyawdim3", "mk", NULL, NULL, "1992",
	"Mortal Kombat (Yawdim bootleg, set 3)\0", NULL, "bootleg (Yawdim)", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkyawdim3RomInfo, mkyawdim3RomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	Mkyawdim3Init, DrvExit, YawdimFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Mortal Kombat (Yawdim bootleg, set 4)

static struct BurnRomInfo mkyawdim4RomDesc[] = {
	{ "14.bin",											0x10000, 0xb58d229e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (sound)

	{ "15.bin",											0x20000, 0x921c613d, 2 | BRF_SND },           //  1 Samples
	{ "16.bin",											0x80000, 0x6e68e0b0, 2 | BRF_SND },           //  2

	{ "17.bin",											0x80000, 0x671b533d, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "18.bin",											0x80000, 0x4e857747, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "22.bin",											0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "21.bin",											0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "20.bin",											0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "19.bin",											0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "26.bin",											0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "25.bin",											0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "24.bin",											0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "23.bin",											0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "30.bin",											0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "29.bin",											0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "28.bin",											0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "27.bin",											0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkyawdim4)
STD_ROM_FN(mkyawdim4)

struct BurnDriver BurnDrvMkyawdim4 = {
	"mkyawdim4", "mk", NULL, NULL, "1992",
	"Mortal Kombat (Yawdim bootleg, set 4)\0", NULL, "bootleg (Yawdim)", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkyawdim4RomInfo, mkyawdim4RomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	Mkyawdim3Init, DrvExit, YawdimFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};

// Mortal Kombat (Yawdim bootleg, set 5)
// f20v id 1408

static struct BurnRomInfo mkyawdim5RomDesc[] = {
	{ "1.u167",											0x10000, 0xb58d229e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (sound)

	{ "2.u159",											0x20000, 0x921c613d, 2 | BRF_SND },           //  1 Samples
	{ "3.u160",											0x80000, 0x6e68e0b0, 2 | BRF_SND },           //  2

	{ "4.u25",											0x80000, 0xb12b3bf2, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "5.u26",											0x80000, 0x7a37dc5c, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "9.u52",											0x80000, 0xd17096c4, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "8.u49",											0x80000, 0x993bc2e4, 4 | BRF_GRA },           //  6
	{ "7.u46",											0x80000, 0x6fb91ede, 4 | BRF_GRA },           //  7
	{ "6.u43",											0x80000, 0xed1ff88a, 4 | BRF_GRA },           //  8
	{ "13.u53",											0x80000, 0xa002a155, 4 | BRF_GRA },           //  9
	{ "12.u50",											0x80000, 0xdcee8492, 4 | BRF_GRA },           // 10
	{ "11.u47",											0x80000, 0xde88caef, 4 | BRF_GRA },           // 11
	{ "10.u44",											0x80000, 0x37eb01b4, 4 | BRF_GRA },           // 12
	{ "17.u54",											0x80000, 0x45acaf21, 4 | BRF_GRA },           // 13
	{ "16.u51",											0x80000, 0x2a6c10a0, 4 | BRF_GRA },           // 14
	{ "15.u48",											0x80000, 0x23308979, 4 | BRF_GRA },           // 15
	{ "14.u45",											0x80000, 0xcafc47bb, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(mkyawdim5)
STD_ROM_FN(mkyawdim5)

struct BurnDriver BurnDrvMkyawdim5 = {
	"mkyawdim5", "mk", NULL, NULL, "1992",
	"Mortal Kombat (Yawdim bootleg, set 5)\0", NULL, "bootleg (Yawdim)", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
	NULL, mkyawdim5RomInfo, mkyawdim5RomName, NULL, NULL, NULL, NULL, Mkla4InputInfo, Mkla4DIPInfo,
	Mkyawdim3Init, DrvExit, YawdimFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};

// Trog (rev LA5 3/29/91)

static struct BurnRomInfo trogRomDesc[] = {
	{ "trog_ii_u-4_sl_1.u4",							0x10000, 0x759d0bf4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (sound)
	{ "trog_ii_u-19_sl_1.u19",							0x10000, 0x960c333d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "trog_ii_u-20_sl_1.u20",							0x10000, 0x67f1658a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "trog_ii_u-105_la-5.u105",						0x20000, 0xd62cc51a, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "trog_ii_u-89_la-5.u89",							0x20000, 0xedde0bc8, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "trog_ii_u-111_la-1.u111",						0x20000, 0x9ded08c1, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "trog_ii_u-112_la-1.u112",						0x20000, 0x42293843, 4 | BRF_GRA },           //  6
	{ "trog_ii_u-113_la-1.u113",						0x20000, 0x77f50cbb, 4 | BRF_GRA },           //  7
	{ "trog_ii_u-106_la-1.u106",						0x20000, 0xaf2eb0d8, 4 | BRF_GRA },           //  8
	{ "trog_ii_u-107_la-1.u107",						0x20000, 0x88a7b3f6, 4 | BRF_GRA },           //  9
	{ "trog_ii_u-95_la-1.u95",							0x20000, 0xf3ba2838, 4 | BRF_GRA },           // 10
	{ "trog_ii_u-96_la-1.u96",							0x20000, 0xcfed2e77, 4 | BRF_GRA },           // 11
	{ "trog_ii_u-97_la-1.u97",							0x20000, 0x3262d1f8, 4 | BRF_GRA },           // 12
	{ "trog_ii_u-90_la-1.u90",							0x20000, 0x16e06753, 4 | BRF_GRA },           // 13
	{ "trog_ii_u-91_la-1.u91",							0x20000, 0x880a02c7, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(trog)
STD_ROM_FN(trog)

static void TrogLoadCallback()
{
	memmove (DrvGfxROM + 0x080000, DrvGfxROM + 0x060000, 0x040000);
	memmove (DrvGfxROM + 0x280000, DrvGfxROM + 0x260000, 0x040000);
}

static INT32 TrogInit()
{
	static const struct protection_data trog_protection_data =
	{
		{ 0x0f00, 0x0f00, 0x0f00 },
		{ 0x3000, 0x1000, 0x2000, 0x0000, 0x2000, 0x3000, 0x3000, 0x1000, 0x0000, 0x0000, 0x2000, 0x3000, 0x1000, 0x1000, 0x2000 }
	};

	prot_data = &trog_protection_data;

	return CommonInit(TrogLoadCallback, 1 | 0x80, 40000000, 4, 0x9eaf, 0x9ed9);
}

struct BurnDriver BurnDrvTrog = {
	"trog", NULL, NULL, NULL, "1990",
	"Trog (rev LA5 3/29/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_MIDWAY, GBF_MAZE, 0,
	NULL, trogRomInfo, trogRomName, NULL, NULL, NULL, NULL, TrogInputInfo, TrogDIPInfo,
	TrogInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// Trog (rev LA4 3/11/91)

static struct BurnRomInfo trog4RomDesc[] = {
	{ "trog_ii_u-4_sl_1.u4",							0x10000, 0x759d0bf4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "trog_ii_u-19_sl_1.u19",							0x10000, 0x960c333d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "trog_ii_u-20_sl_1.u20",							0x10000, 0x67f1658a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "trog_ii_u-105_la-4.u105",						0x20000, 0xe6095189, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "trog_ii_u-89_la-4.u89",							0x20000, 0xfdd7cc65, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "trog_ii_u-111_la-1.u111",						0x20000, 0x9ded08c1, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "trog_ii_u-112_la-1.u112",						0x20000, 0x42293843, 4 | BRF_GRA },           //  6
	{ "trog_ii_u-113_la-1.u113",						0x20000, 0x77f50cbb, 4 | BRF_GRA },           //  7
	{ "trog_ii_u-106_la-1.u106",						0x20000, 0xaf2eb0d8, 4 | BRF_GRA },           //  8
	{ "trog_ii_u-107_la-1.u107",						0x20000, 0x88a7b3f6, 4 | BRF_GRA },           //  9
	{ "trog_ii_u-95_la-1.u95",							0x20000, 0xf3ba2838, 4 | BRF_GRA },           // 10
	{ "trog_ii_u-96_la-1.u96",							0x20000, 0xcfed2e77, 4 | BRF_GRA },           // 11
	{ "trog_ii_u-97_la-1.u97",							0x20000, 0x3262d1f8, 4 | BRF_GRA },           // 12
	{ "trog_ii_u-90_la-1.u90",							0x20000, 0x16e06753, 4 | BRF_GRA },           // 13
	{ "trog_ii_u-91_la-1.u91",							0x20000, 0x880a02c7, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(trog4)
STD_ROM_FN(trog4)

struct BurnDriver BurnDrvTrog4 = {
	"trog4", "trog", NULL, NULL, "1990",
	"Trog (rev LA4 3/11/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_MIDWAY, GBF_MAZE, 0,
	NULL, trog4RomInfo, trog4RomName, NULL, NULL, NULL, NULL, TrogInputInfo, TrogDIPInfo,
	TrogInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// Trog (rev LA3 2/14/91)

static struct BurnRomInfo trog3RomDesc[] = {
	{ "trog_ii_u-4_sl_1.u4",							0x10000, 0x759d0bf4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "trog_ii_u-19_sl_1.u19",							0x10000, 0x960c333d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "trog_ii_u-20_sl_1.u20",							0x10000, 0x67f1658a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "trog_ii_u-105_la-3.u105",						0x20000, 0xd09cea97, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "trog_ii_u-89_la-3.u89",							0x20000, 0xa61e3572, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "trog_ii_u-111_la-1.u111",						0x20000, 0x9ded08c1, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "trog_ii_u-112_la-1.u112",						0x20000, 0x42293843, 4 | BRF_GRA },           //  6
	{ "trog_ii_u-113_la-1.u113",						0x20000, 0x77f50cbb, 4 | BRF_GRA },           //  7
	{ "trog_ii_u-106_la-1.u106",						0x20000, 0xaf2eb0d8, 4 | BRF_GRA },           //  8
	{ "trog_ii_u-107_la-1.u107",						0x20000, 0x88a7b3f6, 4 | BRF_GRA },           //  9
	{ "trog_ii_u-95_la-1.u95",							0x20000, 0xf3ba2838, 4 | BRF_GRA },           // 10
	{ "trog_ii_u-96_la-1.u96",							0x20000, 0xcfed2e77, 4 | BRF_GRA },           // 11
	{ "trog_ii_u-97_la-1.u97",							0x20000, 0x3262d1f8, 4 | BRF_GRA },           // 12
	{ "trog_ii_u-90_la-1.u90",							0x20000, 0x16e06753, 4 | BRF_GRA },           // 13
	{ "trog_ii_u-91_la-1.u91",							0x20000, 0x880a02c7, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(trog3)
STD_ROM_FN(trog3)

struct BurnDriver BurnDrvTrog3 = {
	"trog3", "trog", NULL, NULL, "1990",
	"Trog (rev LA3 2/14/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_MIDWAY, GBF_MAZE, 0,
	NULL, trog3RomInfo, trog3RomName, NULL, NULL, NULL, NULL, TrogInputInfo, TrogDIPInfo,
	TrogInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// Trog (rev LA3 2/10/91)

static struct BurnRomInfo trog3aRomDesc[] = {
	{ "trog_ii_u-4_sl_1.u4",							0x10000, 0x759d0bf4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "trog_ii_u-19_sl_1.u19",							0x10000, 0x960c333d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "trog_ii_u-20_sl_1.u20",							0x10000, 0x67f1658a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "trog_ii_u-105_la-3.u105",						0x20000, 0x9b3841dd, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "trog_ii_u-89_la-3.u89",							0x20000, 0x9c0e6542, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "trog_ii_u-111_la-1.u111",						0x20000, 0x9ded08c1, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "trog_ii_u-112_la-1.u112",						0x20000, 0x42293843, 4 | BRF_GRA },           //  6
	{ "trog_ii_u-113_la-1.u113",						0x20000, 0x77f50cbb, 4 | BRF_GRA },           //  7
	{ "trog_ii_u-106_la-1.u106",						0x20000, 0xaf2eb0d8, 4 | BRF_GRA },           //  8
	{ "trog_ii_u-107_la-1.u107",						0x20000, 0x88a7b3f6, 4 | BRF_GRA },           //  9
	{ "trog_ii_u-95_la-1.u95",							0x20000, 0xf3ba2838, 4 | BRF_GRA },           // 10
	{ "trog_ii_u-96_la-1.u96",							0x20000, 0xcfed2e77, 4 | BRF_GRA },           // 11
	{ "trog_ii_u-97_la-1.u97",							0x20000, 0x3262d1f8, 4 | BRF_GRA },           // 12
	{ "trog_ii_u-90_la-1.u90",							0x20000, 0x16e06753, 4 | BRF_GRA },           // 13
	{ "trog_ii_u-91_la-1.u91",							0x20000, 0x880a02c7, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(trog3a)
STD_ROM_FN(trog3a)

struct BurnDriver BurnDrvTrog3a = {
	"trog3a", "trog", NULL, NULL, "1990",
	"Trog (rev LA3 2/10/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_MIDWAY, GBF_MAZE, 0,
	NULL, trog3aRomInfo, trog3aRomName, NULL, NULL, NULL, NULL, TrogInputInfo, TrogDIPInfo,
	TrogInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// Trog (prototype, rev PA6-PAC 9/09/90)

static struct BurnRomInfo trogpa6RomDesc[] = {
	{ "trog_ii_u-4_sl_1.u4",							0x10000, 0x759d0bf4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "trog_ii_u-19_sl_1.u19",							0x10000, 0x960c333d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "trog_ii_u-20_sl_1.u20",							0x10000, 0x67f1658a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "trog_ii_u-105_rev.6.u105",						0x20000, 0x71ad1903, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "trog_ii_u-89_rev.6.u89",							0x20000, 0x04473da8, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "trog_ii_u-111_la-1.u111",						0x20000, 0x9ded08c1, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "trog_ii_u-112_la-1.u112",						0x20000, 0x42293843, 4 | BRF_GRA },           //  6
	{ "trog_ii_u-113_rev.5.u113",						0x20000, 0xae50e5ea, 4 | BRF_GRA },           //  7
	{ "trog_ii_u-106_la-1.u106",						0x20000, 0xaf2eb0d8, 4 | BRF_GRA },           //  8
	{ "trog_ii_u-107_la-1.u107",						0x20000, 0x88a7b3f6, 4 | BRF_GRA },           //  9
	{ "trog_ii_u-95_la-1.u95",							0x20000, 0xf3ba2838, 4 | BRF_GRA },           // 10
	{ "trog_ii_u-96_la-1.u96",							0x20000, 0xcfed2e77, 4 | BRF_GRA },           // 11
	{ "trog_ii_u-97_rev.5.u97",							0x20000, 0x354b1cb3, 4 | BRF_GRA },           // 12
	{ "trog_ii_u-90_la-1.u90",							0x20000, 0x16e06753, 4 | BRF_GRA },           // 13
	{ "trog_ii_u-91_la-1.u91",							0x20000, 0x880a02c7, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(trogpa6)
STD_ROM_FN(trogpa6)

struct BurnDriver BurnDrvTrogpa6 = {
	"trogpa6", "trog", NULL, NULL, "1990",
	"Trog (prototype, rev PA6-PAC 9/09/90)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_PREFIX_MIDWAY, GBF_MAZE, 0,
	NULL, trogpa6RomInfo, trogpa6RomName, NULL, NULL, NULL, NULL, TrogInputInfo, TrogDIPInfo,
	TrogInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// Trog (prototype, rev PA5-PAC 8/28/90)

static struct BurnRomInfo trogpa5RomDesc[] = {
	{ "trog_ii_u-4_sl_1.u4",							0x10000, 0x759d0bf4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "trog_ii_u-19_sl_1.u19",							0x10000, 0x960c333d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "trog_ii_u-20_sl_1.u20",							0x10000, 0x67f1658a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "trog_ii_u-105_rev.5.u105",						0x20000, 0xda645900, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "trog_ii_u-89_rev.5.u89",							0x20000, 0xd42d0f71, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "trog_ii_u-111_la-1.u111",						0x20000, 0x9ded08c1, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "trog_ii_u-112_la-1.u112",						0x20000, 0x42293843, 4 | BRF_GRA },           //  6
	{ "trog_ii_u-113_rev.5.u113",						0x20000, 0xae50e5ea, 4 | BRF_GRA },           //  7
	{ "trog_ii_u-106_la-1.u106",						0x20000, 0xaf2eb0d8, 4 | BRF_GRA },           //  8
	{ "trog_ii_u-107_la-1.u107",						0x20000, 0x88a7b3f6, 4 | BRF_GRA },           //  9
	{ "trog_ii_u-95_la-1.u95",							0x20000, 0xf3ba2838, 4 | BRF_GRA },           // 10
	{ "trog_ii_u-96_la-1.u96",							0x20000, 0xcfed2e77, 4 | BRF_GRA },           // 11
	{ "trog_ii_u-97_rev.5.u97",							0x20000, 0x354b1cb3, 4 | BRF_GRA },           // 12
	{ "trog_ii_u-90_la-1.u90",							0x20000, 0x16e06753, 4 | BRF_GRA },           // 13
	{ "trog_ii_u-91_la-1.u91",							0x20000, 0x880a02c7, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(trogpa5)
STD_ROM_FN(trogpa5)

struct BurnDriver BurnDrvTrogpa5 = {
	"trogpa5", "trog", NULL, NULL, "1990",
	"Trog (prototype, rev PA5-PAC 8/28/90)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_PREFIX_MIDWAY, GBF_MAZE, 0,
	NULL, trogpa5RomInfo, trogpa5RomName, NULL, NULL, NULL, NULL, TrogInputInfo, TrogDIPInfo,
	TrogInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// Trog (prototype, rev 4.00 7/27/90)

static struct BurnRomInfo trogpa4RomDesc[] = {
	{ "trog_ii_u-4_sl_1.u4",							0x10000, 0x759d0bf4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "trog_ii_u-19_sl_1.u19",							0x10000, 0x960c333d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "trog_ii_u-20_sl_1.u20",							0x10000, 0x67f1658a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "u105-pa4",										0x20000, 0x526a3f5b, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "u89-pa4",										0x20000, 0x38d68685, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "trog_ii_u-111_la-1.u111",						0x20000, 0x9ded08c1, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "trog_ii_u-112_la-1.u112",						0x20000, 0x42293843, 4 | BRF_GRA },           //  6
	{ "trogu113.pa4",									0x20000, 0x2980a56f, 4 | BRF_GRA },           //  7
	{ "trog_ii_u-106_la-1.u106",						0x20000, 0xaf2eb0d8, 4 | BRF_GRA },           //  8
	{ "trog_ii_u-107_la-1.u107",						0x20000, 0x88a7b3f6, 4 | BRF_GRA },           //  9
	{ "trog_ii_u-95_la-1.u95",							0x20000, 0xf3ba2838, 4 | BRF_GRA },           // 10
	{ "trog_ii_u-96_la-1.u96",							0x20000, 0xcfed2e77, 4 | BRF_GRA },           // 11
	{ "trogu97.pa4",									0x20000, 0xf94b77c1, 4 | BRF_GRA },           // 12
	{ "trog_ii_u-90_la-1.u90",							0x20000, 0x16e06753, 4 | BRF_GRA },           // 13
	{ "trog_ii_u-91_la-1.u91",							0x20000, 0x880a02c7, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(trogpa4)
STD_ROM_FN(trogpa4)

struct BurnDriver BurnDrvTrogpa4 = {
	"trogpa4", "trog", NULL, NULL, "1990",
	"Trog (prototype, rev 4.00 7/27/90)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_PREFIX_MIDWAY, GBF_MAZE, 0,
	NULL, trogpa4RomInfo, trogpa4RomName, NULL, NULL, NULL, NULL, TrogInputInfo, TrogDIPInfo,
	TrogInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// Smash T.V. (rev 8.00)

static struct BurnRomInfo smashtvRomDesc[] = {
	{ "sl2_smash_tv_sound_rom_u4.u4",					0x10000, 0x29d3f6c8, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl2_smash_tv_sound_rom_u19.u19",					0x10000, 0xac5a402a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl2_smash_tv_sound_rom_u20.u20",					0x10000, 0x875c66d9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "la8_smash_tv_game_rom_u105.u105",				0x20000, 0x48cd793f, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la8_smash_tv_game_rom_u89.u89",					0x20000, 0x8e7fe463, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_smash_tv_game_rom_u111.u111",				0x20000, 0x72f0ba84, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_smash_tv_game_rom_u112.u112",				0x20000, 0x436f0283, 4 | BRF_GRA },           //  6
	{ "la1_smash_tv_game_rom_u113.u113",				0x20000, 0x4a4b8110, 4 | BRF_GRA },           //  7
	{ "la1_smash_tv_game_rom_u95.u95",					0x20000, 0xe864a44b, 4 | BRF_GRA },           //  8
	{ "la1_smash_tv_game_rom_u96.u96",					0x20000, 0x15555ea7, 4 | BRF_GRA },           //  9
	{ "la1_smash_tv_game_rom_u97.u97",					0x20000, 0xccac9d9e, 4 | BRF_GRA },           // 10
	{ "la1_smash_tv_game_rom_u106.u106",				0x20000, 0x5c718361, 4 | BRF_GRA },           // 11
	{ "la1_smash_tv_game_rom_u107.u107",				0x20000, 0x0fba1e36, 4 | BRF_GRA },           // 12
	{ "la1_smash_tv_game_rom_u108.u108",				0x20000, 0xcb0a092f, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(smashtv)
STD_ROM_FN(smashtv)

static INT32 SmashtvInit()
{
	prot_data = NULL;

	return CommonInit(NULL, 1 | 0x80, 40000000, 6, 0x9cf6, 0x9d21);
}

struct BurnDriver BurnDrvSmashtv = {
	"smashtv", NULL, NULL, NULL, "1990",
	"Smash T.V. (rev 8.00)\0", NULL, "Williams", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_MIDWAY, GBF_RUNGUN, 0,
	NULL, smashtvRomInfo, smashtvRomName, NULL, NULL, NULL, NULL, SmashtvInputInfo, SmashtvDIPInfo,
	SmashtvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Smash T.V. (rev 6.00)

static struct BurnRomInfo smashtv6RomDesc[] = {
	{ "sl2_smash_tv_sound_rom_u4.u4",					0x10000, 0x29d3f6c8, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl2_smash_tv_sound_rom_u19.u19",					0x10000, 0xac5a402a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl2_smash_tv_sound_rom_u20.u20",					0x10000, 0x875c66d9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "la6_smash_tv_game_rom_u105.u105",				0x20000, 0xf1666017, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la6_smash_tv_game_rom_u89.u89",					0x20000, 0x908aca5d, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_smash_tv_game_rom_u111.u111",				0x20000, 0x72f0ba84, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_smash_tv_game_rom_u112.u112",				0x20000, 0x436f0283, 4 | BRF_GRA },           //  6
	{ "la1_smash_tv_game_rom_u113.u113",				0x20000, 0x4a4b8110, 4 | BRF_GRA },           //  7
	{ "la1_smash_tv_game_rom_u95.u95",					0x20000, 0xe864a44b, 4 | BRF_GRA },           //  8
	{ "la1_smash_tv_game_rom_u96.u96",					0x20000, 0x15555ea7, 4 | BRF_GRA },           //  9
	{ "la1_smash_tv_game_rom_u97.u97",					0x20000, 0xccac9d9e, 4 | BRF_GRA },           // 10
	{ "la1_smash_tv_game_rom_u106.u106",				0x20000, 0x5c718361, 4 | BRF_GRA },           // 11
	{ "la1_smash_tv_game_rom_u107.u107",				0x20000, 0x0fba1e36, 4 | BRF_GRA },           // 12
	{ "la1_smash_tv_game_rom_u108.u108",				0x20000, 0xcb0a092f, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(smashtv6)
STD_ROM_FN(smashtv6)

struct BurnDriver BurnDrvSmashtv6 = {
	"smashtv6", "smashtv", NULL, NULL, "1990",
	"Smash T.V. (rev 6.00)\0", NULL, "Williams", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_MIDWAY, GBF_RUNGUN, 0,
	NULL, smashtv6RomInfo, smashtv6RomName, NULL, NULL, NULL, NULL, SmashtvInputInfo, SmashtvDIPInfo,
	SmashtvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Smash T.V. (rev 5.00)

static struct BurnRomInfo smashtv5RomDesc[] = {
	{ "sl2_smash_tv_sound_rom_u4.u4",					0x10000, 0x29d3f6c8, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl2_smash_tv_sound_rom_u19.u19",					0x10000, 0xac5a402a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl2_smash_tv_sound_rom_u20.u20",					0x10000, 0x875c66d9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "la5_smash_tv_game_rom_u105.u105",				0x20000, 0x81f564b9, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la5_smash_tv_game_rom_u89.u89",					0x20000, 0xe5017d25, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_smash_tv_game_rom_u111.u111",				0x20000, 0x72f0ba84, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_smash_tv_game_rom_u112.u112",				0x20000, 0x436f0283, 4 | BRF_GRA },           //  6
	{ "la1_smash_tv_game_rom_u113.u113",				0x20000, 0x4a4b8110, 4 | BRF_GRA },           //  7
	{ "la1_smash_tv_game_rom_u95.u95",					0x20000, 0xe864a44b, 4 | BRF_GRA },           //  8
	{ "la1_smash_tv_game_rom_u96.u96",					0x20000, 0x15555ea7, 4 | BRF_GRA },           //  9
	{ "la1_smash_tv_game_rom_u97.u97",					0x20000, 0xccac9d9e, 4 | BRF_GRA },           // 10
	{ "la1_smash_tv_game_rom_u106.u106",				0x20000, 0x5c718361, 4 | BRF_GRA },           // 11
	{ "la1_smash_tv_game_rom_u107.u107",				0x20000, 0x0fba1e36, 4 | BRF_GRA },           // 12
	{ "la1_smash_tv_game_rom_u108.u108",				0x20000, 0xcb0a092f, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(smashtv5)
STD_ROM_FN(smashtv5)

struct BurnDriver BurnDrvSmashtv5 = {
	"smashtv5", "smashtv", NULL, NULL, "1990",
	"Smash T.V. (rev 5.00)\0", NULL, "Williams", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_MIDWAY, GBF_RUNGUN, 0,
	NULL, smashtv5RomInfo, smashtv5RomName, NULL, NULL, NULL, NULL, SmashtvInputInfo, SmashtvDIPInfo,
	SmashtvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Smash T.V. (rev 4.00)
// source docs states Smash TV Rev 4.00 released 5/4/90 to production.

static struct BurnRomInfo smashtv4RomDesc[] = {
	{ "sl2_smash_tv_sound_rom_u4.u4",					0x10000, 0x29d3f6c8, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl2_smash_tv_sound_rom_u19.u19",					0x10000, 0xac5a402a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl2_smash_tv_sound_rom_u20.u20",					0x10000, 0x875c66d9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "la4_smash_tv_game_rom_u105.u105",				0x20000, 0xa50ccb71, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la4_smash_tv_game_rom_u89.u89",					0x20000, 0xef0b0279, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_smash_tv_game_rom_u111.u111",				0x20000, 0x72f0ba84, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_smash_tv_game_rom_u112.u112",				0x20000, 0x436f0283, 4 | BRF_GRA },           //  6
	{ "la1_smash_tv_game_rom_u113.u113",				0x20000, 0x4a4b8110, 4 | BRF_GRA },           //  7
	{ "la1_smash_tv_game_rom_u95.u95",					0x20000, 0xe864a44b, 4 | BRF_GRA },           //  8
	{ "la1_smash_tv_game_rom_u96.u96",					0x20000, 0x15555ea7, 4 | BRF_GRA },           //  9
	{ "la1_smash_tv_game_rom_u97.u97",					0x20000, 0xccac9d9e, 4 | BRF_GRA },           // 10
	{ "la1_smash_tv_game_rom_u106.u106",				0x20000, 0x5c718361, 4 | BRF_GRA },           // 11
	{ "la1_smash_tv_game_rom_u107.u107",				0x20000, 0x0fba1e36, 4 | BRF_GRA },           // 12
	{ "la1_smash_tv_game_rom_u108.u108",				0x20000, 0xcb0a092f, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(smashtv4)
STD_ROM_FN(smashtv4)

struct BurnDriver BurnDrvSmashtv4 = {
	"smashtv4", "smashtv", NULL, NULL, "1990",
	"Smash T.V. (rev 4.00)\0", NULL, "Williams", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_MIDWAY, GBF_RUNGUN, 0,
	NULL, smashtv4RomInfo, smashtv4RomName, NULL, NULL, NULL, NULL, SmashtvInputInfo, SmashtvDIPInfo,
	SmashtvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Smash T.V. (rev 3.01)

static struct BurnRomInfo smashtv3RomDesc[] = {
	{ "sl1_smash_tv_sound_rom_u4.u4",					0x10000, 0xa3370987, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl1_smash_tv_sound_rom_u19.u19",					0x10000, 0xac5a402a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl1_smash_tv_sound_rom_u20.u20",					0x10000, 0x875c66d9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "la3_smash_tv_game_rom_u105.u105",				0x20000, 0x33b626c3, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la3_smash_tv_game_rom_u89.u89",					0x20000, 0x5f6fbc25, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_smash_tv_game_rom_u111.u111",				0x20000, 0x72f0ba84, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_smash_tv_game_rom_u112.u112",				0x20000, 0x436f0283, 4 | BRF_GRA },           //  6
	{ "la1_smash_tv_game_rom_u113.u113",				0x20000, 0x4a4b8110, 4 | BRF_GRA },           //  7
	{ "la1_smash_tv_game_rom_u95.u95",					0x20000, 0xe864a44b, 4 | BRF_GRA },           //  8
	{ "la1_smash_tv_game_rom_u96.u96",					0x20000, 0x15555ea7, 4 | BRF_GRA },           //  9
	{ "la1_smash_tv_game_rom_u97.u97",					0x20000, 0xccac9d9e, 4 | BRF_GRA },           // 10
	{ "la1_smash_tv_game_rom_u106.u106",				0x20000, 0x5c718361, 4 | BRF_GRA },           // 11
	{ "la1_smash_tv_game_rom_u107.u107",				0x20000, 0x0fba1e36, 4 | BRF_GRA },           // 12
	{ "la1_smash_tv_game_rom_u108.u108",				0x20000, 0xcb0a092f, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(smashtv3)
STD_ROM_FN(smashtv3)

struct BurnDriver BurnDrvSmashtv3 = {
	"smashtv3", "smashtv", NULL, NULL, "1990",
	"Smash T.V. (rev 3.01)\0", NULL, "Williams", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_MIDWAY, GBF_RUNGUN, 0,
	NULL, smashtv3RomInfo, smashtv3RomName, NULL, NULL, NULL, NULL, SmashtvInputInfo, SmashtvDIPInfo,
	SmashtvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// High Impact Football (rev LA5 02/15/91)

static struct BurnRomInfo hiimpactRomDesc[] = {
	{ "sl1_high_impact_sound_u4.u4",					0x20000, 0x28effd6a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl1_high_impact_sound_u19.u19",					0x20000, 0x0ea22c89, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl1_high_impact_sound_u20.u20",					0x20000, 0x4e747ab5, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "la5_high_impact_game_rom_u105.u105",				0x20000, 0x104c30e7, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la5_high_impact_game_rom_u89.u89",				0x20000, 0x07aa0010, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_high_impact_game_rom_u111.u111",				0x20000, 0x49560560, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_high_impact_game_rom_u112.u112",				0x20000, 0x4dd879dc, 4 | BRF_GRA },           //  6
	{ "la1_high_impact_game_rom_u113.u113",				0x20000, 0xb67aeb70, 4 | BRF_GRA },           //  7
	{ "la1_high_impact_game_rom_u114.u114",				0x20000, 0x9a4bc44b, 4 | BRF_GRA },           //  8
	{ "la1_high_impact_game_rom_u95.u95",				0x20000, 0xe1352dc0, 4 | BRF_GRA },           //  9
	{ "la1_high_impact_game_rom_u96.u96",				0x20000, 0x197d0f34, 4 | BRF_GRA },           // 10
	{ "la1_high_impact_game_rom_u97.u97",				0x20000, 0x908ea575, 4 | BRF_GRA },           // 11
	{ "la1_high_impact_game_rom_u98.u98",				0x20000, 0x6dcbab11, 4 | BRF_GRA },           // 12
	{ "la1_high_impact_game_rom_u106.u106",				0x20000, 0x7d0ead0d, 4 | BRF_GRA },           // 13
	{ "la1_high_impact_game_rom_u107.u107",				0x20000, 0xef48e8fa, 4 | BRF_GRA },           // 14
	{ "la1_high_impact_game_rom_u108.u108",				0x20000, 0x5f363e12, 4 | BRF_GRA },           // 15
	{ "la1_high_impact_game_rom_u109.u109",				0x20000, 0x3689fbbc, 4 | BRF_GRA },           // 16

	{ "ep610.u8",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17 PLDs
	{ "pls153a.u40",									0x000eb, 0x69e5143f, 0 | BRF_OPT },           // 18
	{ "ep610.u51",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
	{ "ep610.u52",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 20
	{ "ep610.u65",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 21
};

STD_ROM_PICK(hiimpact)
STD_ROM_FN(hiimpact)

static INT32 HiimpactInit()
{
	static const struct protection_data hiimpact_protection_data = {
		{ 0x0b00, 0x0b00, 0x0b00 },
		{ 0x2000, 0x4000, 0x4000, 0x0000, 0x6000, 0x6000, 0x2000, 0x4000, 0x2000, 0x4000, 0x2000, 0x0000, 0x4000, 0x6000, 0x2000 }
	};

	prot_data = &hiimpact_protection_data;

	return CommonInit(NULL, 1, 40000000, 6, 0x9b79, 0x9ba3);
}

struct BurnDriver BurnDrvHiimpact = {
	"hiimpact", NULL, NULL, NULL, "1990",
	"High Impact Football (rev LA5 02/15/91)\0", NULL, "Williams", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_MIDWAY, GBF_SPORTSFOOTBALL, 0,
	NULL, hiimpactRomInfo, hiimpactRomName, NULL, NULL, NULL, NULL, HiimpactInputInfo, HiimpactDIPInfo,
	HiimpactInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// High Impact Football (rev LA4 02/04/91)

static struct BurnRomInfo hiimpact4RomDesc[] = {
	{ "sl1_high_impact_sound_u4.u4",					0x20000, 0x28effd6a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl1_high_impact_sound_u19.u19",					0x20000, 0x0ea22c89, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl1_high_impact_sound_u20.u20",					0x20000, 0x4e747ab5, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "la4_high_impact_game_rom_u105.u105",				0x20000, 0x5f67f823, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la4_high_impact_game_rom_u89.u89",				0x20000, 0x404d260b, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_high_impact_game_rom_u111.u111",				0x20000, 0x49560560, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_high_impact_game_rom_u112.u112",				0x20000, 0x4dd879dc, 4 | BRF_GRA },           //  6
	{ "la1_high_impact_game_rom_u113.u113",				0x20000, 0xb67aeb70, 4 | BRF_GRA },           //  7
	{ "la1_high_impact_game_rom_u114.u114",				0x20000, 0x9a4bc44b, 4 | BRF_GRA },           //  8
	{ "la1_high_impact_game_rom_u95.u95",				0x20000, 0xe1352dc0, 4 | BRF_GRA },           //  9
	{ "la1_high_impact_game_rom_u96.u96",				0x20000, 0x197d0f34, 4 | BRF_GRA },           // 10
	{ "la1_high_impact_game_rom_u97.u97",				0x20000, 0x908ea575, 4 | BRF_GRA },           // 11
	{ "la1_high_impact_game_rom_u98.u98",				0x20000, 0x6dcbab11, 4 | BRF_GRA },           // 12
	{ "la1_high_impact_game_rom_u106.u106",				0x20000, 0x7d0ead0d, 4 | BRF_GRA },           // 13
	{ "la1_high_impact_game_rom_u107.u107",				0x20000, 0xef48e8fa, 4 | BRF_GRA },           // 14
	{ "la1_high_impact_game_rom_u108.u108",				0x20000, 0x5f363e12, 4 | BRF_GRA },           // 15
	{ "la1_high_impact_game_rom_u109.u109",				0x20000, 0x3689fbbc, 4 | BRF_GRA },           // 16

	{ "ep610.u8",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17 PLDs
	{ "pls153a.u40",									0x000eb, 0x69e5143f, 0 | BRF_OPT },           // 18
	{ "ep610.u51",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
	{ "ep610.u52",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 20
	{ "ep610.u65",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 21
};

STD_ROM_PICK(hiimpact4)
STD_ROM_FN(hiimpact4)

struct BurnDriver BurnDrvHiimpact4 = {
	"hiimpact4", "hiimpact", NULL, NULL, "1990",
	"High Impact Football (rev LA4 02/04/91)\0", NULL, "Williams", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_MIDWAY, GBF_SPORTSFOOTBALL, 0,
	NULL, hiimpact4RomInfo, hiimpact4RomName, NULL, NULL, NULL, NULL, HiimpactInputInfo, HiimpactDIPInfo,
	HiimpactInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// High Impact Football (rev LA3 12/27/90)

static struct BurnRomInfo hiimpact3RomDesc[] = {
	{ "sl1_high_impact_sound_u4.u4",					0x20000, 0x28effd6a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl1_high_impact_sound_u19.u19",					0x20000, 0x0ea22c89, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl1_high_impact_sound_u20.u20",					0x20000, 0x4e747ab5, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "la3_high_impact_game_rom_u105.u105",				0x20000, 0xb9190c4a, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la3_high_impact_game_rom_u89.u89",				0x20000, 0x1cbc72a5, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_high_impact_game_rom_u111.u111",				0x20000, 0x49560560, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_high_impact_game_rom_u112.u112",				0x20000, 0x4dd879dc, 4 | BRF_GRA },           //  6
	{ "la1_high_impact_game_rom_u113.u113",				0x20000, 0xb67aeb70, 4 | BRF_GRA },           //  7
	{ "la1_high_impact_game_rom_u114.u114",				0x20000, 0x9a4bc44b, 4 | BRF_GRA },           //  8
	{ "la1_high_impact_game_rom_u95.u95",				0x20000, 0xe1352dc0, 4 | BRF_GRA },           //  9
	{ "la1_high_impact_game_rom_u96.u96",				0x20000, 0x197d0f34, 4 | BRF_GRA },           // 10
	{ "la1_high_impact_game_rom_u97.u97",				0x20000, 0x908ea575, 4 | BRF_GRA },           // 11
	{ "la1_high_impact_game_rom_u98.u98",				0x20000, 0x6dcbab11, 4 | BRF_GRA },           // 12
	{ "la1_high_impact_game_rom_u106.u106",				0x20000, 0x7d0ead0d, 4 | BRF_GRA },           // 13
	{ "la1_high_impact_game_rom_u107.u107",				0x20000, 0xef48e8fa, 4 | BRF_GRA },           // 14
	{ "la1_high_impact_game_rom_u108.u108",				0x20000, 0x5f363e12, 4 | BRF_GRA },           // 15
	{ "la1_high_impact_game_rom_u109.u109",				0x20000, 0x3689fbbc, 4 | BRF_GRA },           // 16

	{ "ep610.u8",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17 PLDs
	{ "pls153a.u40",									0x000eb, 0x69e5143f, 0 | BRF_OPT },           // 18
	{ "ep610.u51",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
	{ "ep610.u52",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 20
	{ "ep610.u65",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 21
};

STD_ROM_PICK(hiimpact3)
STD_ROM_FN(hiimpact3)

struct BurnDriver BurnDrvHiimpact3 = {
	"hiimpact3", "hiimpact", NULL, NULL, "1990",
	"High Impact Football (rev LA3 12/27/90)\0", NULL, "Williams", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_MIDWAY, GBF_SPORTSFOOTBALL, 0,
	NULL, hiimpact3RomInfo, hiimpact3RomName, NULL, NULL, NULL, NULL, HiimpactInputInfo, HiimpactDIPInfo,
	HiimpactInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// High Impact Football (rev LA2 12/26/90)

static struct BurnRomInfo hiimpact2RomDesc[] = {
	{ "sl1_high_impact_sound_u4.u4",					0x20000, 0x28effd6a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl1_high_impact_sound_u19.u19",					0x20000, 0x0ea22c89, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl1_high_impact_sound_u20.u20",					0x20000, 0x4e747ab5, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "la2_high_impact_game_rom_u105.u105",				0x20000, 0x25d83ba1, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la2_high_impact_game_rom_u89.u89",				0x20000, 0x811f1253, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_high_impact_game_rom_u111.u111",				0x20000, 0x49560560, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_high_impact_game_rom_u112.u112",				0x20000, 0x4dd879dc, 4 | BRF_GRA },           //  6
	{ "la1_high_impact_game_rom_u113.u113",				0x20000, 0xb67aeb70, 4 | BRF_GRA },           //  7
	{ "la1_high_impact_game_rom_u114.u114",				0x20000, 0x9a4bc44b, 4 | BRF_GRA },           //  8
	{ "la1_high_impact_game_rom_u95.u95",				0x20000, 0xe1352dc0, 4 | BRF_GRA },           //  9
	{ "la1_high_impact_game_rom_u96.u96",				0x20000, 0x197d0f34, 4 | BRF_GRA },           // 10
	{ "la1_high_impact_game_rom_u97.u97",				0x20000, 0x908ea575, 4 | BRF_GRA },           // 11
	{ "la1_high_impact_game_rom_u98.u98",				0x20000, 0x6dcbab11, 4 | BRF_GRA },           // 12
	{ "la1_high_impact_game_rom_u106.u106",				0x20000, 0x7d0ead0d, 4 | BRF_GRA },           // 13
	{ "la1_high_impact_game_rom_u107.u107",				0x20000, 0xef48e8fa, 4 | BRF_GRA },           // 14
	{ "la1_high_impact_game_rom_u108.u108",				0x20000, 0x5f363e12, 4 | BRF_GRA },           // 15
	{ "la1_high_impact_game_rom_u109.u109",				0x20000, 0x3689fbbc, 4 | BRF_GRA },           // 16

	{ "ep610.u8",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17 PLDs
	{ "pls153a.u40",									0x000eb, 0x69e5143f, 0 | BRF_OPT },           // 18
	{ "ep610.u51",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
	{ "ep610.u52",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 20
	{ "ep610.u65",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 21
};

STD_ROM_PICK(hiimpact2)
STD_ROM_FN(hiimpact2)

struct BurnDriver BurnDrvHiimpact2 = {
	"hiimpact2", "hiimpact", NULL, NULL, "1990",
	"High Impact Football (rev LA2 12/26/90)\0", NULL, "Williams", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_MIDWAY, GBF_SPORTSFOOTBALL, 0,
	NULL, hiimpact2RomInfo, hiimpact2RomName, NULL, NULL, NULL, NULL, HiimpactInputInfo, HiimpactDIPInfo,
	HiimpactInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// High Impact Football (rev LA1 12/16/90)

static struct BurnRomInfo hiimpact1RomDesc[] = {
	{ "sl1_high_impact_sound_u4.u4",					0x20000, 0x28effd6a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl1_high_impact_sound_u19.u19",					0x20000, 0x0ea22c89, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl1_high_impact_sound_u20.u20",					0x20000, 0x4e747ab5, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "la1_high_impact_game_rom_u105.u105",				0x20000, 0xe86228ba, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la1_high_impact_game_rom_u89.u89",				0x20000, 0xf23e972e, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_high_impact_game_rom_u111.u111",				0x20000, 0x49560560, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_high_impact_game_rom_u112.u112",				0x20000, 0x4dd879dc, 4 | BRF_GRA },           //  6
	{ "la1_high_impact_game_rom_u113.u113",				0x20000, 0xb67aeb70, 4 | BRF_GRA },           //  7
	{ "la1_high_impact_game_rom_u114.u114",				0x20000, 0x9a4bc44b, 4 | BRF_GRA },           //  8
	{ "la1_high_impact_game_rom_u95.u95",				0x20000, 0xe1352dc0, 4 | BRF_GRA },           //  9
	{ "la1_high_impact_game_rom_u96.u96",				0x20000, 0x197d0f34, 4 | BRF_GRA },           // 10
	{ "la1_high_impact_game_rom_u97.u97",				0x20000, 0x908ea575, 4 | BRF_GRA },           // 11
	{ "la1_high_impact_game_rom_u98.u98",				0x20000, 0x6dcbab11, 4 | BRF_GRA },           // 12
	{ "la1_high_impact_game_rom_u106.u106",				0x20000, 0x7d0ead0d, 4 | BRF_GRA },           // 13
	{ "la1_high_impact_game_rom_u107.u107",				0x20000, 0xef48e8fa, 4 | BRF_GRA },           // 14
	{ "la1_high_impact_game_rom_u108.u108",				0x20000, 0x5f363e12, 4 | BRF_GRA },           // 15
	{ "la1_high_impact_game_rom_u109.u109",				0x20000, 0x3689fbbc, 4 | BRF_GRA },           // 16

	{ "ep610.u8",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17 PLDs
	{ "pls153a.u40",									0x000eb, 0x69e5143f, 0 | BRF_OPT },           // 18
	{ "ep610.u51",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
	{ "ep610.u52",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 20
	{ "ep610.u65",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 21
};

STD_ROM_PICK(hiimpact1)
STD_ROM_FN(hiimpact1)

struct BurnDriver BurnDrvHiimpact1 = {
	"hiimpact1", "hiimpact", NULL, NULL, "1990",
	"High Impact Football (rev LA1 12/16/90)\0", NULL, "Williams", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_MIDWAY, GBF_SPORTSFOOTBALL, 0,
	NULL, hiimpact1RomInfo, hiimpact1RomName, NULL, NULL, NULL, NULL, HiimpactInputInfo, HiimpactDIPInfo,
	HiimpactInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// High Impact Football (prototype, revision0 proto 8.6 12/09/90)

static struct BurnRomInfo hiimpactpRomDesc[] = {
	{ "sl1_high_impact_sound_u4.u4",					0x20000, 0x28effd6a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl1_high_impact_sound_u19.u19",					0x20000, 0x0ea22c89, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl1_high_impact_sound_u20.u20",					0x20000, 0x4e747ab5, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pa1_high_impact_game_rom_u105.u105",				0x20000, 0x79ef9a35, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "pa1_high_impact_game_rom_u89.u89",				0x20000, 0x2bd3de30, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_high_impact_game_rom_u111.u111",				0x20000, 0x49560560, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_high_impact_game_rom_u112.u112",				0x20000, 0x4dd879dc, 4 | BRF_GRA },           //  6
	{ "la1_high_impact_game_rom_u113.u113",				0x20000, 0xb67aeb70, 4 | BRF_GRA },           //  7
	{ "la1_high_impact_game_rom_u114.u114",				0x20000, 0x9a4bc44b, 4 | BRF_GRA },           //  8
	{ "la1_high_impact_game_rom_u95.u95",				0x20000, 0xe1352dc0, 4 | BRF_GRA },           //  9
	{ "la1_high_impact_game_rom_u96.u96",				0x20000, 0x197d0f34, 4 | BRF_GRA },           // 10
	{ "la1_high_impact_game_rom_u97.u97",				0x20000, 0x908ea575, 4 | BRF_GRA },           // 11
	{ "la1_high_impact_game_rom_u98.u98",				0x20000, 0x6dcbab11, 4 | BRF_GRA },           // 12
	{ "la1_high_impact_game_rom_u106.u106",				0x20000, 0x7d0ead0d, 4 | BRF_GRA },           // 13
	{ "la1_high_impact_game_rom_u107.u107",				0x20000, 0xef48e8fa, 4 | BRF_GRA },           // 14
	{ "la1_high_impact_game_rom_u108.u108",				0x20000, 0x5f363e12, 4 | BRF_GRA },           // 15
	{ "la1_high_impact_game_rom_u109.u109",				0x20000, 0x3689fbbc, 4 | BRF_GRA },           // 16

	{ "ep610.u8",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17 PLDs
	{ "pls153a.u40",									0x000eb, 0x69e5143f, 0 | BRF_OPT },           // 18
	{ "ep610.u51",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
	{ "ep610.u52",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 20
	{ "ep610.u65",										0x0032f, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 21
};

STD_ROM_PICK(hiimpactp)
STD_ROM_FN(hiimpactp)

struct BurnDriver BurnDrvHiimpactp = {
	"hiimpactp", "hiimpact", NULL, NULL, "1990",
	"High Impact Football (prototype, revision0 proto 8.6 12/09/90)\0", NULL, "Williams", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_PREFIX_MIDWAY, GBF_SPORTSFOOTBALL, 0,
	NULL, hiimpactpRomInfo, hiimpactpRomName, NULL, NULL, NULL, NULL, HiimpactInputInfo, HiimpactDIPInfo,
	HiimpactInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// Super High Impact (rev LA1 09/30/91)

static struct BurnRomInfo shimpactRomDesc[] = {
	{ "sl1_super_high_impact_u4_sound_rom.u4",			0x20000, 0x1e5a012c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl1_super_high_impact_u19_sound_rom.u19",		0x20000, 0x10f9684e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl1_super_high_impact_u20_sound_rom.u20",		0x20000, 0x1b4a71c1, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "la1_super_high_impact_game_rom_u105.u105",		0x20000, 0xf2cf8de3, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la1_super_high_impact_game_rom_u89.u89",			0x20000, 0xf97d9b01, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_super_high_impact_game_rom_u111.u111",		0x40000, 0x80ae2a86, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_super_high_impact_game_rom_u112.u112",		0x40000, 0x3ffc27e9, 4 | BRF_GRA },           //  6
	{ "la1_super_high_impact_game_rom_u113.u113",		0x40000, 0x01549d00, 4 | BRF_GRA },           //  7
	{ "la1_super_high_impact_game_rom_u114.u114",		0x40000, 0xa68af319, 4 | BRF_GRA },           //  8
	{ "la1_super_high_impact_game_rom_u95.u95",			0x40000, 0xe8f56ef5, 4 | BRF_GRA },           //  9
	{ "la1_super_high_impact_game_rom_u96.u96",			0x40000, 0x24ed04f9, 4 | BRF_GRA },           // 10
	{ "la1_super_high_impact_game_rom_u97.u97",			0x40000, 0xdd7f41a9, 4 | BRF_GRA },           // 11
	{ "la1_super_high_impact_game_rom_u98.u98",			0x40000, 0x23ef65dd, 4 | BRF_GRA },           // 12
	{ "la1_super_high_impact_game_rom_u106.u106",		0x40000, 0x6f5bf337, 4 | BRF_GRA },           // 13
	{ "la1_super_high_impact_game_rom_u107.u107",		0x40000, 0xa8815dad, 4 | BRF_GRA },           // 14
	{ "la1_super_high_impact_game_rom_u108.u108",		0x40000, 0xd39685a3, 4 | BRF_GRA },           // 15
	{ "la1_super_high_impact_game_rom_u109.u109",		0x40000, 0x36e0b2b2, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(shimpact)
STD_ROM_FN(shimpact)

static INT32 ShimpactInit()
{
	static const struct protection_data shimpact_protection_data = {
		{ 0x0f00, 0x0e00, 0x0d00 },
		{ 0x0000, 0x4000, 0x2000, 0x5000, 0x2000, 0x1000, 0x4000, 0x6000, 0x3000, 0x0000, 0x2000, 0x5000, 0x5000, 0x5000, 0x2000 }
	};

	prot_data = &shimpact_protection_data;

	return CommonInit(NULL, 1, 40000000, 6, 0x9c06, 0x9c15);
}

struct BurnDriver BurnDrvShimpact = {
	"shimpact", NULL, NULL, NULL, "1991",
	"Super High Impact (rev LA1 09/30/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_MIDWAY, GBF_SPORTSFOOTBALL, 0,
	NULL, shimpactRomInfo, shimpactRomName, NULL, NULL, NULL, NULL, HiimpactInputInfo, ShimpactDIPInfo,
	ShimpactInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// Super High Impact (prototype, proto 6.0 09/23/91)

static struct BurnRomInfo shimpactp6RomDesc[] = {
	{ "sl1_super_high_impact_u4_sound_rom.u4",			0x20000, 0x1e5a012c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl1_super_high_impact_u19_sound_rom.u19",		0x20000, 0x10f9684e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl1_super_high_impact_u20_sound_rom.u20",		0x20000, 0x1b4a71c1, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "proto6_super_high_impact_game_rom_u105.u105",	0x20000, 0x33e1978d, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "proto6_super_high_impact_game_rom_u89.u89",		0x20000, 0x6c070978, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_super_high_impact_game_rom_u111.u111",		0x40000, 0x80ae2a86, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_super_high_impact_game_rom_u112.u112",		0x40000, 0x3ffc27e9, 4 | BRF_GRA },           //  6
	{ "la1_super_high_impact_game_rom_u113.u113",		0x40000, 0x01549d00, 4 | BRF_GRA },           //  7
	{ "la1_super_high_impact_game_rom_u114.u114",		0x40000, 0xa68af319, 4 | BRF_GRA },           //  8
	{ "la1_super_high_impact_game_rom_u95.u95",			0x40000, 0xe8f56ef5, 4 | BRF_GRA },           //  9
	{ "la1_super_high_impact_game_rom_u96.u96",			0x40000, 0x24ed04f9, 4 | BRF_GRA },           // 10
	{ "la1_super_high_impact_game_rom_u97.u97",			0x40000, 0xdd7f41a9, 4 | BRF_GRA },           // 11
	{ "la1_super_high_impact_game_rom_u98.u98",			0x40000, 0x23ef65dd, 4 | BRF_GRA },           // 12
	{ "la1_super_high_impact_game_rom_u106.u106",		0x40000, 0x6f5bf337, 4 | BRF_GRA },           // 13
	{ "la1_super_high_impact_game_rom_u107.u107",		0x40000, 0xa8815dad, 4 | BRF_GRA },           // 14
	{ "la1_super_high_impact_game_rom_u108.u108",		0x40000, 0xd39685a3, 4 | BRF_GRA },           // 15
	{ "la1_super_high_impact_game_rom_u109.u109",		0x40000, 0x36e0b2b2, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(shimpactp6)
STD_ROM_FN(shimpactp6)

struct BurnDriver BurnDrvShimpactp6 = {
	"shimpactp6", "shimpact", NULL, NULL, "1991",
	"Super High Impact (prototype, proto 6.0 09/23/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_PREFIX_MIDWAY, GBF_SPORTSFOOTBALL, 0,
	NULL, shimpactp6RomInfo, shimpactp6RomName, NULL, NULL, NULL, NULL, HiimpactInputInfo, ShimpactDIPInfo,
	ShimpactInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// Super High Impact (prototype, proto 5.0 09/15/91)

static struct BurnRomInfo shimpactp5RomDesc[] = {
	{ "sl1_super_high_impact_u4_sound_rom.u4",			0x20000, 0x1e5a012c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl1_super_high_impact_u19_sound_rom.u19",		0x20000, 0x10f9684e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl1_super_high_impact_u20_sound_rom.u20",		0x20000, 0x1b4a71c1, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "proto5_super_high_impact_game_rom_u105.u105",	0x20000, 0x4342cd45, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "proto5_super_high_impact_game_rom_u89.u89",		0x20000, 0xcda47b73, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "proto3_super_high_impact_game_rom_u111.u111",	0x40000, 0x80ae2a86, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "proto3_super_high_impact_game_rom_u112.u112",	0x40000, 0x3ffc27e9, 4 | BRF_GRA },           //  6
	{ "proto3_super_high_impact_game_rom_u113.u113",	0x40000, 0x01549d00, 4 | BRF_GRA },           //  7
	{ "proto3_super_high_impact_game_rom_u114.u114",	0x40000, 0x56f96a67, 4 | BRF_GRA },           //  8
	{ "proto3_super_high_impact_game_rom_u95.u95",		0x40000, 0xe8f56ef5, 4 | BRF_GRA },           //  9
	{ "proto3_super_high_impact_game_rom_u96.u96",		0x40000, 0x24ed04f9, 4 | BRF_GRA },           // 10
	{ "proto3_super_high_impact_game_rom_u97.u97",		0x40000, 0xdd7f41a9, 4 | BRF_GRA },           // 11
	{ "proto3_super_high_impact_game_rom_u98.u98",		0x40000, 0x28418723, 4 | BRF_GRA },           // 12
	{ "proto3_super_high_impact_game_rom_u106.u106",	0x40000, 0x6f5bf337, 4 | BRF_GRA },           // 13
	{ "proto3_super_high_impact_game_rom_u107.u107",	0x40000, 0xa8815dad, 4 | BRF_GRA },           // 14
	{ "proto3_super_high_impact_game_rom_u108.u108",	0x40000, 0xd39685a3, 4 | BRF_GRA },           // 15
	{ "proto3_super_high_impact_game_rom_u109.u109",	0x40000, 0x58f71141, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(shimpactp5)
STD_ROM_FN(shimpactp5)

struct BurnDriver BurnDrvShimpactp5 = {
	"shimpactp5", "shimpact", NULL, NULL, "1991",
	"Super High Impact (prototype, proto 5.0 09/15/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_PREFIX_MIDWAY, GBF_SPORTSFOOTBALL, 0,
	NULL, shimpactp5RomInfo, shimpactp5RomName, NULL, NULL, NULL, NULL, HiimpactInputInfo, ShimpactDIPInfo,
	ShimpactInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// Super High Impact (prototype, proto 4.0 09/10/91)

static struct BurnRomInfo shimpactp4RomDesc[] = {
	{ "sl1_super_high_impact_u4_sound_rom.u4",			0x20000, 0x1e5a012c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl1_super_high_impact_u19_sound_rom.u19",		0x20000, 0x10f9684e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl1_super_high_impact_u20_sound_rom.u20",		0x20000, 0x1b4a71c1, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "proto4_super_high_impact_game_rom_u105.u105",	0x20000, 0x770b31ce, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "proto4_super_high_impact_game_rom_u89.u89",		0x20000, 0x96b622a5, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "proto3_super_high_impact_game_rom_u111.u111",	0x40000, 0x80ae2a86, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "proto3_super_high_impact_game_rom_u112.u112",	0x40000, 0x3ffc27e9, 4 | BRF_GRA },           //  6
	{ "proto3_super_high_impact_game_rom_u113.u113",	0x40000, 0x01549d00, 4 | BRF_GRA },           //  7
	{ "proto3_super_high_impact_game_rom_u114.u114",	0x40000, 0x56f96a67, 4 | BRF_GRA },           //  8
	{ "proto3_super_high_impact_game_rom_u95.u95",		0x40000, 0xe8f56ef5, 4 | BRF_GRA },           //  9
	{ "proto3_super_high_impact_game_rom_u96.u96",		0x40000, 0x24ed04f9, 4 | BRF_GRA },           // 10
	{ "proto3_super_high_impact_game_rom_u97.u97",		0x40000, 0xdd7f41a9, 4 | BRF_GRA },           // 11
	{ "proto3_super_high_impact_game_rom_u98.u98",		0x40000, 0x28418723, 4 | BRF_GRA },           // 12
	{ "proto3_super_high_impact_game_rom_u106.u106",	0x40000, 0x6f5bf337, 4 | BRF_GRA },           // 13
	{ "proto3_super_high_impact_game_rom_u107.u107",	0x40000, 0xa8815dad, 4 | BRF_GRA },           // 14
	{ "proto3_super_high_impact_game_rom_u108.u108",	0x40000, 0xd39685a3, 4 | BRF_GRA },           // 15
	{ "proto3_super_high_impact_game_rom_u109.u109",	0x40000, 0x58f71141, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(shimpactp4)
STD_ROM_FN(shimpactp4)

struct BurnDriver BurnDrvShimpactp4 = {
	"shimpactp4", "shimpact", NULL, NULL, "1991",
	"Super High Impact (prototype, proto 4.0 09/10/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_PREFIX_MIDWAY, GBF_SPORTSFOOTBALL, 0,
	NULL, shimpactp4RomInfo, shimpactp4RomName, NULL, NULL, NULL, NULL, HiimpactInputInfo, ShimpactDIPInfo,
	ShimpactInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	396, 256, 4, 3
};


// Strike Force (rev 1 02/25/91)

static struct BurnRomInfo strkforcRomDesc[] = {
	{ "sl1_strike_force_sound_rom_u4.u4",				0x10000, 0x8f747312, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)
	{ "sl1_strike_force_sound_rom_u19.u19",				0x10000, 0xafb29926, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sl1_strike_force_sound_rom_u20.u20",				0x10000, 0x1bc9b746, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "la1_strike_force_game_rom_u105.u105",			0x20000, 0x7895e0e3, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la1_strike_force_game_rom_u89.u89",				0x20000, 0x26114d9e, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_strike_force_game_rom_u111.u111",			0x20000, 0x878efc80, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_strike_force_game_rom_u112.u112",			0x20000, 0x93394399, 4 | BRF_GRA },           //  6
	{ "la1_strike_force_game_rom_u113.u113",			0x20000, 0x9565a79b, 4 | BRF_GRA },           //  7
	{ "la1_strike_force_game_rom_u114.u114",			0x20000, 0xb71152da, 4 | BRF_GRA },           //  8
	{ "la1_strike_force_game_rom_u106.u106",			0x20000, 0xa394d4cf, 4 | BRF_GRA },           //  9
	{ "la1_strike_force_game_rom_u107.u107",			0x20000, 0xedef1419, 4 | BRF_GRA },           // 10
	{ "la1_strike_force_game_rom_u95.u95",				0x20000, 0x519cb2b4, 4 | BRF_GRA },           // 11
	{ "la1_strike_force_game_rom_u96.u96",				0x20000, 0x61214796, 4 | BRF_GRA },           // 12
	{ "la1_strike_force_game_rom_u97.u97",				0x20000, 0xeb5dee5f, 4 | BRF_GRA },           // 13
	{ "la1_strike_force_game_rom_u98.u98",				0x20000, 0xc5c079e7, 4 | BRF_GRA },           // 14
	{ "la1_strike_force_game_rom_u90.u90",				0x20000, 0x607bcdc0, 4 | BRF_GRA },           // 15
	{ "la1_strike_force_game_rom_u91.u91",				0x20000, 0xda02547e, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(strkforc)
STD_ROM_FN(strkforc)

static INT32 StrkforcInit()
{
	static const struct protection_data strkforc_protection_data = {
		{ 0x1234 }, { 0 }
	};

	prot_data = &strkforc_protection_data;

	return CommonInit(NULL, 1 | 0x80, 48000000, 4, 0x9f7d, 0x9fa7);
}

struct BurnDriver BurnDrvStrkforc = {
	"strkforc", NULL, NULL, NULL, "1991",
	"Strike Force (rev 1 02/25/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_MIDWAY, GBF_HORSHOOT, 0,
	NULL, strkforcRomInfo, strkforcRomName, NULL, NULL, NULL, NULL, StrkforcInputInfo, StrkforcDIPInfo,
	StrkforcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Total Carnage (rev LA1 03/10/92)

static struct BurnRomInfo totcarnRomDesc[] = {
	{ "sl1_total_carnage_sound_rom_u3.u3",				0x20000, 0x5bdb4665, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "sl1_total_carnage_sound_rom_u12.u12",			0x40000, 0xd0000ac7, 2 | BRF_SND },           //  1 Samples
	{ "sl1_total_carnage_sound_rom_u13.u13",			0x40000, 0xe48e6f0c, 2 | BRF_SND },           //  2

	{ "la1_total_carnage_game_rom_u105.u105",			0x40000, 0x7c651047, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la1_total_carnage_game_rom_u89.u89",				0x40000, 0x6761daf3, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_total_carnage_game_rom_u111.u111",			0x40000, 0x13f3f231, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_total_carnage_game_rom_u112.u112",			0x40000, 0x72e45007, 4 | BRF_GRA },           //  6
	{ "la1_total_carnage_game_rom_u113.u113",			0x40000, 0x2c8ec753, 4 | BRF_GRA },           //  7
	{ "la1_total_carnage_game_rom_u114.u114",			0x40000, 0x6210c36c, 4 | BRF_GRA },           //  8
	{ "la1_total_carnage_game_rom_u95.u95",				0x40000, 0x579caeba, 4 | BRF_GRA },           //  9
	{ "la1_total_carnage_game_rom_u96.u96",				0x40000, 0xf43f1ffe, 4 | BRF_GRA },           // 10
	{ "la1_total_carnage_game_rom_u97.u97",				0x40000, 0x1675e50d, 4 | BRF_GRA },           // 11
	{ "la1_total_carnage_game_rom_u98.u98",				0x40000, 0xab06c885, 4 | BRF_GRA },           // 12
	{ "la1_total_carnage_game_rom_u106.u106",			0x40000, 0x146e3863, 4 | BRF_GRA },           // 13
	{ "la1_total_carnage_game_rom_u107.u107",			0x40000, 0x95323320, 4 | BRF_GRA },           // 14
	{ "la1_total_carnage_game_rom_u108.u108",			0x40000, 0xed152acc, 4 | BRF_GRA },           // 15
	{ "la1_total_carnage_game_rom_u109.u109",			0x40000, 0x80715252, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(totcarn)
STD_ROM_FN(totcarn)

static void TotcarnLoadCallback()
{
	memcpy (DrvSndROM[0] + 0x30000, DrvSndROM[0] + 0x10000, 0x20000);
}

static INT32 TotcarnInit()
{
	static const struct protection_data totcarn_protection_data = {
		{ 0x0f00, 0x0f00, 0x0f00 },
		{ 0x4a00, 0x6a00, 0xda00, 0x6a00, 0x9a00, 0x4a00, 0x2a00, 0x9a00, 0x1a00, 0x8a00, 0xaa00 }
	};

	prot_data = &totcarn_protection_data;

	is_totcarn = 1;

	return CommonInit(TotcarnLoadCallback, 0, 48000000, 6, 0xfc04, 0xfc2e);
}

struct BurnDriver BurnDrvTotcarn = {
	"totcarn", NULL, NULL, NULL, "1992",
	"Total Carnage (rev LA1 03/10/92)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_MIDWAY, GBF_RUNGUN, 0,
	NULL, totcarnRomInfo, totcarnRomName, NULL, NULL, NULL, NULL, TotcarnInputInfo, TotcarnDIPInfo,
	TotcarnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Total Carnage (prototype, proto v 2.0 02/10/92)

static struct BurnRomInfo totcarnp2RomDesc[] = {
	{ "sl1_total_carnage_sound_rom_u3.u3",				0x20000, 0x5bdb4665, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "sl1_total_carnage_sound_rom_u12.u12",			0x40000, 0xd0000ac7, 2 | BRF_SND },           //  1 Samples
	{ "sl1_total_carnage_sound_rom_u13.u13",			0x40000, 0xe48e6f0c, 2 | BRF_SND },           //  2

	{ "proto2_total_carnage_game_rom_u105.u105",		0x40000, 0xe273d43c, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "proto2_total_carnage_game_rom_u89.u89",			0x40000, 0xe759078b, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_total_carnage_game_rom_u111.u111",			0x40000, 0x13f3f231, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_total_carnage_game_rom_u112.u112",			0x40000, 0x72e45007, 4 | BRF_GRA },           //  6
	{ "la1_total_carnage_game_rom_u113.u113",			0x40000, 0x2c8ec753, 4 | BRF_GRA },           //  7
	{ "la1_total_carnage_game_rom_u114.u114",			0x40000, 0x6210c36c, 4 | BRF_GRA },           //  8
	{ "la1_total_carnage_game_rom_u95.u95",				0x40000, 0x579caeba, 4 | BRF_GRA },           //  9
	{ "la1_total_carnage_game_rom_u96.u96",				0x40000, 0xf43f1ffe, 4 | BRF_GRA },           // 10
	{ "la1_total_carnage_game_rom_u97.u97",				0x40000, 0x1675e50d, 4 | BRF_GRA },           // 11
	{ "la1_total_carnage_game_rom_u98.u98",				0x40000, 0xab06c885, 4 | BRF_GRA },           // 12
	{ "la1_total_carnage_game_rom_u106.u106",			0x40000, 0x146e3863, 4 | BRF_GRA },           // 13
	{ "la1_total_carnage_game_rom_u107.u107",			0x40000, 0x95323320, 4 | BRF_GRA },           // 14
	{ "la1_total_carnage_game_rom_u108.u108",			0x40000, 0xed152acc, 4 | BRF_GRA },           // 15
	{ "la1_total_carnage_game_rom_u109.u109",			0x40000, 0x80715252, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(totcarnp2)
STD_ROM_FN(totcarnp2)

struct BurnDriver BurnDrvTotcarnp2 = {
	"totcarnp2", "totcarn", NULL, NULL, "1992",
	"Total Carnage (prototype, proto v 2.0 02/10/92)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_PREFIX_MIDWAY, GBF_RUNGUN, 0,
	NULL, totcarnp2RomInfo, totcarnp2RomName, NULL, NULL, NULL, NULL, TotcarnInputInfo, TotcarnDIPInfo,
	TotcarnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Total Carnage (prototype, proto v 1.0 01/25/92)

static struct BurnRomInfo totcarnp1RomDesc[] = {
	{ "sl1_total_carnage_sound_rom_u3.u3",				0x20000, 0x5bdb4665, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "sl1_total_carnage_sound_rom_u12.u12",			0x40000, 0xd0000ac7, 2 | BRF_SND },           //  1 Samples
	{ "sl1_total_carnage_sound_rom_u13.u13",			0x40000, 0xe48e6f0c, 2 | BRF_SND },           //  2

	{ "proto1_total_carnage_game_rom_u105.u105",		0x40000, 0x7a782cae, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "proto1_total_carnage_game_rom_u89.u89",			0x40000, 0x1c899a8d, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_total_carnage_game_rom_u111.u111",			0x40000, 0x13f3f231, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_total_carnage_game_rom_u112.u112",			0x40000, 0x72e45007, 4 | BRF_GRA },           //  6
	{ "la1_total_carnage_game_rom_u113.u113",			0x40000, 0x2c8ec753, 4 | BRF_GRA },           //  7
	{ "la1_total_carnage_game_rom_u114.u114",			0x40000, 0x6210c36c, 4 | BRF_GRA },           //  8
	{ "la1_total_carnage_game_rom_u95.u95",				0x40000, 0x579caeba, 4 | BRF_GRA },           //  9
	{ "la1_total_carnage_game_rom_u96.u96",				0x40000, 0xf43f1ffe, 4 | BRF_GRA },           // 10
	{ "la1_total_carnage_game_rom_u97.u97",				0x40000, 0x1675e50d, 4 | BRF_GRA },           // 11
	{ "la1_total_carnage_game_rom_u98.u98",				0x40000, 0xab06c885, 4 | BRF_GRA },           // 12
	{ "la1_total_carnage_game_rom_u106.u106",			0x40000, 0x146e3863, 4 | BRF_GRA },           // 13
	{ "la1_total_carnage_game_rom_u107.u107",			0x40000, 0x95323320, 4 | BRF_GRA },           // 14
	{ "la1_total_carnage_game_rom_u108.u108",			0x40000, 0xed152acc, 4 | BRF_GRA },           // 15
	{ "la1_total_carnage_game_rom_u109.u109",			0x40000, 0x80715252, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(totcarnp1)
STD_ROM_FN(totcarnp1)

struct BurnDriver BurnDrvTotcarnp1 = {
	"totcarnp1", "totcarn", NULL, NULL, "1992",
	"Total Carnage (prototype, proto v 1.0 01/25/92)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_PREFIX_MIDWAY, GBF_RUNGUN, 0,
	NULL, totcarnp1RomInfo, totcarnp1RomName, NULL, NULL, NULL, NULL, TotcarnInputInfo, TotcarnDIPInfo,
	TotcarnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Terminator 2 - Judgment Day (rev LA4 08/03/92)

static struct BurnRomInfo term2RomDesc[] = {
	{ "sl1_terminator_2_u3_sound_rom.u3",				0x20000, 0x73c3f5c4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "sl1_terminator_2_u12_sound_rom.u12",				0x40000, 0xe192a40d, 2 | BRF_SND },           //  1 Samples
	{ "sl1_terminator_2_u13_sound_rom.u13",				0x40000, 0x956fa80b, 2 | BRF_SND },           //  2

	{ "la4_terminator_2_game_rom_u105.u105",			0x80000, 0xd4d8d884, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la4_terminator_2_game_rom_u89.u89",				0x80000, 0x25359415, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_terminator_2_game_rom_u111.u111",			0x80000, 0x916d0197, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_terminator_2_game_rom_u112.u112",			0x80000, 0x39ae1c87, 4 | BRF_GRA },           //  6
	{ "la1_terminator_2_game_rom_u113.u113",			0x80000, 0xcb5084e5, 4 | BRF_GRA },           //  7
	{ "la1_terminator_2_game_rom_u114.u114",			0x80000, 0x53c516ec, 4 | BRF_GRA },           //  8
	{ "la1_terminator_2_game_rom_u95.u95",				0x80000, 0xdd39cf73, 4 | BRF_GRA },           //  9
	{ "la1_terminator_2_game_rom_u96.u96",				0x80000, 0x31f4fd36, 4 | BRF_GRA },           // 10
	{ "la1_terminator_2_game_rom_u97.u97",				0x80000, 0x7f72e775, 4 | BRF_GRA },           // 11
	{ "la1_terminator_2_game_rom_u98.u98",				0x80000, 0x1a20ce29, 4 | BRF_GRA },           // 12
	{ "la1_terminator_2_game_rom_u106.u106",			0x80000, 0xf08a9536, 4 | BRF_GRA },           // 13
	{ "la1_terminator_2_game_rom_u107.u107",			0x80000, 0x268d4035, 4 | BRF_GRA },           // 14
	{ "la1_terminator_2_game_rom_u108.u108",			0x80000, 0x379fdaed, 4 | BRF_GRA },           // 15
	{ "la1_terminator_2_game_rom_u109.u109",			0x80000, 0x306a9366, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(term2)
STD_ROM_FN(term2)

static INT32 Term2Init()
{
	static const struct protection_data term2_protection_data = {
		{ 0x0f00, 0x0f00, 0x0f00 },
		{ 0x4000, 0xf000, 0xa000 }
	};

	prot_data = &term2_protection_data;

	flip_screen_x = 1;

	is_term2 = 1;

	return CommonInit(TotcarnLoadCallback, 0, 50000000, 6, 0xfa8d, 0xfa9c);
}

struct BurnDriver BurnDrvTerm2 = {
	"term2", NULL, NULL, NULL, "1991",
	"Terminator 2 - Judgment Day (rev LA4 08/03/92)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_MIDWAY, GBF_SHOOT, 0,
	NULL, term2RomInfo, term2RomName, NULL, NULL, NULL, NULL, Term2InputInfo, Term2DIPInfo,
	Term2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Terminator 2 - Judgment Day (rev LA3 03/27/92)

static struct BurnRomInfo term2la3RomDesc[] = {
	{ "sl1_terminator_2_u3_sound_rom.u3",				0x20000, 0x73c3f5c4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "sl1_terminator_2_u12_sound_rom.u12",				0x40000, 0xe192a40d, 2 | BRF_SND },           //  1 Samples
	{ "sl1_terminator_2_u13_sound_rom.u13",				0x40000, 0x956fa80b, 2 | BRF_SND },           //  2

	{ "la3_terminator_2_game_rom_u105.u105",			0x80000, 0x34142b28, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la3_terminator_2_game_rom_u89.u89",				0x80000, 0x5ffea427, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_terminator_2_game_rom_u111.u111",			0x80000, 0x916d0197, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_terminator_2_game_rom_u112.u112",			0x80000, 0x39ae1c87, 4 | BRF_GRA },           //  6
	{ "la1_terminator_2_game_rom_u113.u113",			0x80000, 0xcb5084e5, 4 | BRF_GRA },           //  7
	{ "la1_terminator_2_game_rom_u114.u114",			0x80000, 0x53c516ec, 4 | BRF_GRA },           //  8
	{ "la1_terminator_2_game_rom_u95.u95",				0x80000, 0xdd39cf73, 4 | BRF_GRA },           //  9
	{ "la1_terminator_2_game_rom_u96.u96",				0x80000, 0x31f4fd36, 4 | BRF_GRA },           // 10
	{ "la1_terminator_2_game_rom_u97.u97",				0x80000, 0x7f72e775, 4 | BRF_GRA },           // 11
	{ "la1_terminator_2_game_rom_u98.u98",				0x80000, 0x1a20ce29, 4 | BRF_GRA },           // 12
	{ "la1_terminator_2_game_rom_u106.u106",			0x80000, 0xf08a9536, 4 | BRF_GRA },           // 13
	{ "la1_terminator_2_game_rom_u107.u107",			0x80000, 0x268d4035, 4 | BRF_GRA },           // 14
	{ "la1_terminator_2_game_rom_u108.u108",			0x80000, 0x379fdaed, 4 | BRF_GRA },           // 15
	{ "la1_terminator_2_game_rom_u109.u109",			0x80000, 0x306a9366, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(term2la3)
STD_ROM_FN(term2la3)

struct BurnDriver BurnDrvTerm2la3 = {
	"term2la3", "term2", NULL, NULL, "1991",
	"Terminator 2 - Judgment Day (rev LA3 03/27/92)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_MIDWAY, GBF_SHOOT, 0,
	NULL, term2la3RomInfo, term2la3RomName, NULL, NULL, NULL, NULL, Term2InputInfo, Term2DIPInfo,
	Term2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Terminator 2 - Judgment Day (rev LA2 12/09/91)

static struct BurnRomInfo term2la2RomDesc[] = {
	{ "sl1_terminator_2_u3_sound_rom.u3",				0x20000, 0x73c3f5c4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "sl1_terminator_2_u12_sound_rom.u12",				0x40000, 0xe192a40d, 2 | BRF_SND },           //  1 Samples
	{ "sl1_terminator_2_u13_sound_rom.u13",				0x40000, 0x956fa80b, 2 | BRF_SND },           //  2

	{ "la2_terminator_2_game_rom_u105.u105",			0x80000, 0x7177de98, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la2_terminator_2_game_rom_u89.u89",				0x80000, 0x14d7b9f5, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_terminator_2_game_rom_u111.u111",			0x80000, 0x916d0197, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_terminator_2_game_rom_u112.u112",			0x80000, 0x39ae1c87, 4 | BRF_GRA },           //  6
	{ "la1_terminator_2_game_rom_u113.u113",			0x80000, 0xcb5084e5, 4 | BRF_GRA },           //  7
	{ "la1_terminator_2_game_rom_u114.u114",			0x80000, 0x53c516ec, 4 | BRF_GRA },           //  8
	{ "la1_terminator_2_game_rom_u95.u95",				0x80000, 0xdd39cf73, 4 | BRF_GRA },           //  9
	{ "la1_terminator_2_game_rom_u96.u96",				0x80000, 0x31f4fd36, 4 | BRF_GRA },           // 10
	{ "la1_terminator_2_game_rom_u97.u97",				0x80000, 0x7f72e775, 4 | BRF_GRA },           // 11
	{ "la1_terminator_2_game_rom_u98.u98",				0x80000, 0x1a20ce29, 4 | BRF_GRA },           // 12
	{ "la1_terminator_2_game_rom_u106.u106",			0x80000, 0xf08a9536, 4 | BRF_GRA },           // 13
	{ "la1_terminator_2_game_rom_u107.u107",			0x80000, 0x268d4035, 4 | BRF_GRA },           // 14
	{ "la1_terminator_2_game_rom_u108.u108",			0x80000, 0x379fdaed, 4 | BRF_GRA },           // 15
	{ "la1_terminator_2_game_rom_u109.u109",			0x80000, 0x306a9366, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(term2la2)
STD_ROM_FN(term2la2)

struct BurnDriver BurnDrvTerm2la2 = {
	"term2la2", "term2", NULL, NULL, "1991",
	"Terminator 2 - Judgment Day (rev LA2 12/09/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_MIDWAY, GBF_SHOOT, 0,
	NULL, term2la2RomInfo, term2la2RomName, NULL, NULL, NULL, NULL, Term2InputInfo, Term2DIPInfo,
	Term2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Terminator 2 - Judgment Day (rev LA1 11/01/91)

static struct BurnRomInfo term2la1RomDesc[] = {
	{ "sl1_terminator_2_u3_sound_rom.u3",				0x20000, 0x73c3f5c4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "sl1_terminator_2_u12_sound_rom.u12",				0x40000, 0xe192a40d, 2 | BRF_SND },           //  1 Samples
	{ "sl1_terminator_2_u13_sound_rom.u13",				0x40000, 0x956fa80b, 2 | BRF_SND },           //  2

	{ "la1_terminator_2_game_rom_u105.u105",			0x80000, 0xca52a8b0, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "la1_terminator_2_game_rom_u89.u89",				0x80000, 0x08535210, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_terminator_2_game_rom_u111.u111",			0x80000, 0x916d0197, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_terminator_2_game_rom_u112.u112",			0x80000, 0x39ae1c87, 4 | BRF_GRA },           //  6
	{ "la1_terminator_2_game_rom_u113.u113",			0x80000, 0xcb5084e5, 4 | BRF_GRA },           //  7
	{ "la1_terminator_2_game_rom_u114.u114",			0x80000, 0x53c516ec, 4 | BRF_GRA },           //  8
	{ "la1_terminator_2_game_rom_u95.u95",				0x80000, 0xdd39cf73, 4 | BRF_GRA },           //  9
	{ "la1_terminator_2_game_rom_u96.u96",				0x80000, 0x31f4fd36, 4 | BRF_GRA },           // 10
	{ "la1_terminator_2_game_rom_u97.u97",				0x80000, 0x7f72e775, 4 | BRF_GRA },           // 11
	{ "la1_terminator_2_game_rom_u98.u98",				0x80000, 0x1a20ce29, 4 | BRF_GRA },           // 12
	{ "la1_terminator_2_game_rom_u106.u106",			0x80000, 0xf08a9536, 4 | BRF_GRA },           // 13
	{ "la1_terminator_2_game_rom_u107.u107",			0x80000, 0x268d4035, 4 | BRF_GRA },           // 14
	{ "la1_terminator_2_game_rom_u108.u108",			0x80000, 0x379fdaed, 4 | BRF_GRA },           // 15
	{ "la1_terminator_2_game_rom_u109.u109",			0x80000, 0x306a9366, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(term2la1)
STD_ROM_FN(term2la1)

struct BurnDriver BurnDrvTerm2la1 = {
	"term2la1", "term2", NULL, NULL, "1991",
	"Terminator 2 - Judgment Day (rev LA1 11/01/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_MIDWAY, GBF_SHOOT, 0,
	NULL, term2la1RomInfo, term2la1RomName, NULL, NULL, NULL, NULL, Term2InputInfo, Term2DIPInfo,
	Term2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Terminator 2 - Judgment Day (prototype, rev PA2 10/18/91)

static struct BurnRomInfo term2pa2RomDesc[] = {
	{ "sl1_terminator_2_u3_sound_rom.u3",				0x20000, 0x73c3f5c4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "sl1_terminator_2_u12_sound_rom.u12",				0x40000, 0xe192a40d, 2 | BRF_SND },           //  1 Samples
	{ "sl1_terminator_2_u13_sound_rom.u13",				0x40000, 0x956fa80b, 2 | BRF_SND },           //  2

	{ "pa2_terminator_2_game_rom_u105.u105",			0x80000, 0xe7842129, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "pa2_terminator_2_game_rom_u89.u89",				0x80000, 0x41d50e55, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_terminator_2_game_rom_u111.u111",			0x80000, 0x916d0197, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_terminator_2_game_rom_u112.u112",			0x80000, 0x39ae1c87, 4 | BRF_GRA },           //  6
	{ "la1_terminator_2_game_rom_u113.u113",			0x80000, 0xcb5084e5, 4 | BRF_GRA },           //  7
	{ "pa1_terminator_2_game_rom_u114.u114",			0x80000, 0x2c2bda49, 4 | BRF_GRA },           //  8
	{ "la1_terminator_2_game_rom_u95.u95",				0x80000, 0xdd39cf73, 4 | BRF_GRA },           //  9
	{ "la1_terminator_2_game_rom_u96.u96",				0x80000, 0x31f4fd36, 4 | BRF_GRA },           // 10
	{ "la1_terminator_2_game_rom_u97.u97",				0x80000, 0x7f72e775, 4 | BRF_GRA },           // 11
	{ "pa1_terminator_2_game_rom_u98.u98",				0x80000, 0x3f80a9b2, 4 | BRF_GRA },           // 12
	{ "la1_terminator_2_game_rom_u106.u106",			0x80000, 0xf08a9536, 4 | BRF_GRA },           // 13
	{ "la1_terminator_2_game_rom_u107.u107",			0x80000, 0x268d4035, 4 | BRF_GRA },           // 14
	{ "la1_terminator_2_game_rom_u108.u108",			0x80000, 0x379fdaed, 4 | BRF_GRA },           // 15
	{ "pa2_terminator_2_game_rom_u109.u109",			0x80000, 0x8d115894, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(term2pa2)
STD_ROM_FN(term2pa2)

struct BurnDriver BurnDrvTerm2pa2 = {
	"term2pa2", "term2", NULL, NULL, "1991",
	"Terminator 2 - Judgment Day (prototype, rev PA2 10/18/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_MIDWAY, GBF_SHOOT, 0,
	NULL, term2pa2RomInfo, term2pa2RomName, NULL, NULL, NULL, NULL, Term2InputInfo, Term2DIPInfo,
	Term2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};


// Terminator 2 - Judgment Day (German, rev LG1 11/04/91)

static struct BurnRomInfo term2lg1RomDesc[] = {
	{ "sl1_terminator_2_u3_sound_rom.u3",				0x20000, 0x73c3f5c4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Sound)

	{ "sl1_terminator_2_u12_sound_rom.u12",				0x40000, 0xe192a40d, 2 | BRF_SND },           //  1 Samples
	{ "sl1_terminator_2_u13_sound_rom.u13",				0x40000, 0x956fa80b, 2 | BRF_SND },           //  2

	{ "lg1_terminator_2_game_rom_u105.u105",			0x80000, 0x6aad6389, 3 | BRF_PRG | BRF_ESS }, //  3 TMS34010 Code
	{ "lg1_terminator_2_game_rom_u89.u89",				0x80000, 0x5a052766, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "la1_terminator_2_game_rom_u111.u111",			0x80000, 0x916d0197, 4 | BRF_GRA },           //  5 Graphics (Blitter data)
	{ "la1_terminator_2_game_rom_u112.u112",			0x80000, 0x39ae1c87, 4 | BRF_GRA },           //  6
	{ "la1_terminator_2_game_rom_u113.u113",			0x80000, 0xcb5084e5, 4 | BRF_GRA },           //  7
	{ "lg1_terminator_2_game_rom_u114.u114",			0x80000, 0x1f0c6d8f, 4 | BRF_GRA },           //  8
	{ "la1_terminator_2_game_rom_u95.u95",				0x80000, 0xdd39cf73, 4 | BRF_GRA },           //  9
	{ "la1_terminator_2_game_rom_u96.u96",				0x80000, 0x31f4fd36, 4 | BRF_GRA },           // 10
	{ "la1_terminator_2_game_rom_u97.u97",				0x80000, 0x7f72e775, 4 | BRF_GRA },           // 11
	{ "lg1_terminator_2_game_rom_u98.u98",				0x80000, 0x800c6205, 4 | BRF_GRA },           // 12
	{ "la1_terminator_2_game_rom_u106.u106",			0x80000, 0xf08a9536, 4 | BRF_GRA },           // 13
	{ "la1_terminator_2_game_rom_u107.u107",			0x80000, 0x268d4035, 4 | BRF_GRA },           // 14
	{ "la1_terminator_2_game_rom_u108.u108",			0x80000, 0x379fdaed, 4 | BRF_GRA },           // 15
	{ "lg1_terminator_2_game_rom_u109.u109",			0x80000, 0x70dc2ff3, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(term2lg1)
STD_ROM_FN(term2lg1)

struct BurnDriver BurnDrvTerm2lg1 = {
	"term2lg1", "term2", NULL, NULL, "1991",
	"Terminator 2 - Judgment Day (German, rev LG1 11/04/91)\0", NULL, "Midway", "Y Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_MIDWAY, GBF_SHOOT, 0,
	NULL, term2lg1RomInfo, term2lg1RomName, NULL, NULL, NULL, NULL, Term2InputInfo, Term2DIPInfo,
	Term2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 256,
	400, 256, 4, 3
};
