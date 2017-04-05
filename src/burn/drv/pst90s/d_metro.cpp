// FB Alpha Metro driver module
// Based on MAME driver by Luca Elia and David Haywood

/*
	Needs porting:
		dokyusei	(ym2413+msm6295 sound)
		dokyusp		(ym2413+msm6295 sound)
		mouja		(ym2413+msm6295 sound)
		gakusai		(ym2413+msm6295 sound)
		gakusai2	(ym2413+msm6295 sound)

	Unemulated
		puzzlet		(h8 main cpu)

	Broken
		daitorid, crashes in the blitter after the book opens(attract)
*/

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "upd7810_intf.h"
#include "eeprom.h"
#include "konamiic.h"
#include "burn_ym2610.h"
#include "burn_ym2413.h"
#include "burn_ym2151.h"
#include "burn_ymf278b.h"
#include "msm6295.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvUpdROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvRozROM;
static UINT8 *DrvYMROMA;
static UINT8 *DrvYMROMB;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvVidRAM2;
static UINT8 *DrvUnkRAM;
static UINT8 *Drv68KRAM0;
static UINT8 *Drv68KRAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvTileRAM;
static UINT8 *DrvK053936RAM;
static UINT8 *DrvK053936LRAM;
static UINT8 *DrvK053936CRAM;
static UINT8 *DrvUpdRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvWindow;
static UINT8 *DrvScroll;
static UINT8 *DrvVidRegs;
static UINT8 *DrvBlitter;

static UINT8 DrvRecalc;

static UINT32 gfxrom_bank;
static UINT16 soundlatch;
static UINT16 irq_enable;
static UINT16 screen_control;
static UINT8 requested_int[8];
static INT32 flip_screen;
static INT32 irq_levels[8];
static INT32 blit_timer = -1;

static UINT8 sound_status;
static UINT8 sound_busy;
static UINT8 updportA_data;
static UINT8 updportB_data;

static INT32 sound_system = 0;	// 1 = Z80 + YM2610, 2 = uPD7810 + YM2413 + M6295, 3 = YMF278B, 4 = YM2413 + M6295, 5 = uPD7810 + YM2151 + M6295
static INT32 m_sprite_xoffs_dx = 0;
static INT32 m_sprite_yoffs_dx = 0;
static INT32 m_tilemap_scrolldx[3] = { 8, 8, 8 };
static UINT32 graphics_length;
static INT32 vblank_bit = 0;
static INT32 irq_line = 1;
static INT32 blitter_bit = 0;
static INT32 support_8bpp = 1;
static INT32 support_16x16 = 1;
static INT32 has_zoom = 0;
static UINT32 main_cpu_cycles = 12000000 / 60;
static INT32 ymf278bint = 0;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvDips[4];
static UINT16 DrvInputs[4];
static UINT8 DrvReset;

static struct BurnInputInfo BlzntrndInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 fire 4"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p3 start"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p3 fire 4"	},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p4 start"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p4 fire 3"	},
	{"P4 Button 4",		BIT_DIGITAL,	DrvJoy2 + 15,	"p4 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Blzntrnd)

static struct BurnInputInfo Gstrik2InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 1,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Gstrik2)

static struct BurnInputInfo SkyalertInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Skyalert)

static struct BurnInputInfo LadykillInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy2 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Ladykill)

static struct BurnInputInfo PururunInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pururun)

static struct BurnInputInfo KaratourInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy2 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Karatour)

static struct BurnInputInfo DaitoridInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Daitorid)

static struct BurnInputInfo GunmastInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Gunmast)

static struct BurnInputInfo PuzzliInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Puzzli)

static struct BurnInputInfo BalcubeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Balcube)

static struct BurnDIPInfo BlzntrndDIPList[]=
{
	{0x28, 0xff, 0xff, 0x0e, NULL				},
	{0x29, 0xff, 0xff, 0xff, NULL				},
	{0x2a, 0xff, 0xff, 0xff, NULL				},
	{0x2b, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x28, 0x01, 0x07, 0x07, "Beginner"			},
	{0x28, 0x01, 0x07, 0x06, "Easiest"			},
	{0x28, 0x01, 0x07, 0x05, "Easy"				},
	{0x28, 0x01, 0x07, 0x04, "Normal"			},
	{0x28, 0x01, 0x07, 0x03, "Hard"				},
	{0x28, 0x01, 0x07, 0x02, "Hardest"			},
	{0x28, 0x01, 0x07, 0x01, "Expert"			},
	{0x28, 0x01, 0x07, 0x00, "Master"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x28, 0x01, 0x08, 0x08, "Off"				},
	{0x28, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x28, 0x01, 0x10, 0x10, "Off"				},
	{0x28, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x28, 0x01, 0x20, 0x20, "No"				},
	{0x28, 0x01, 0x20, 0x00, "Yes"				},

	{0   , 0xfe, 0   ,    3, "Control Panel"		},
	{0x28, 0x01, 0xc0, 0x00, "4 Players"			},
	{0x28, 0x01, 0xc0, 0x80, "1P & 2P Tag only"		},
	{0x28, 0x01, 0xc0, 0xc0, "1P & 2P vs only"		},

	{0   , 0xfe, 0   ,    4, "Half Continue"		},
	{0x29, 0x01, 0x03, 0x00, "6C to start, 3C to continue"	},
	{0x29, 0x01, 0x03, 0x01, "4C to start, 2C to continue"	},
	{0x29, 0x01, 0x03, 0x02, "2C to start, 1C to continue"	},
	{0x29, 0x01, 0x03, 0x03, "Disabled"			},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x29, 0x01, 0x07, 0x04, "4 Coins 1 Credits"		},
	{0x29, 0x01, 0x07, 0x05, "3 Coins 1 Credits"		},
	{0x29, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x29, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x29, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x29, 0x01, 0x07, 0x02, "1 Coin  3 Credits"		},
	{0x29, 0x01, 0x07, 0x01, "1 Coin  4 Credits"		},
	{0x29, 0x01, 0x07, 0x00, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x29, 0x01, 0x38, 0x20, "4 Coins 1 Credits"		},
	{0x29, 0x01, 0x38, 0x28, "3 Coins 1 Credits"		},
	{0x29, 0x01, 0x38, 0x30, "2 Coins 1 Credits"		},
	{0x29, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x29, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x29, 0x01, 0x38, 0x10, "1 Coin  3 Credits"		},
	{0x29, 0x01, 0x38, 0x08, "1 Coin  4 Credits"		},
	{0x29, 0x01, 0x38, 0x00, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x29, 0x01, 0x40, 0x40, "Off"				},
	{0x29, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x29, 0x01, 0x80, 0x80, "Off"				},
	{0x29, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "CP Single"			},
	{0x2a, 0x01, 0x03, 0x03, "2:00"				},
	{0x2a, 0x01, 0x03, 0x02, "2:30"				},
	{0x2a, 0x01, 0x03, 0x01, "3:00"				},
	{0x2a, 0x01, 0x03, 0x00, "3:30"				},

	{0   , 0xfe, 0   ,    4, "CP Tag"			},
	{0x2a, 0x01, 0x0c, 0x0c, "2:00"				},
	{0x2a, 0x01, 0x0c, 0x08, "2:30"				},
	{0x2a, 0x01, 0x0c, 0x04, "3:00"				},
	{0x2a, 0x01, 0x0c, 0x00, "3:30"				},

	{0   , 0xfe, 0   ,    4, "Vs Single"			},
	{0x2a, 0x01, 0x30, 0x30, "2:30"				},
	{0x2a, 0x01, 0x30, 0x20, "3:00"				},
	{0x2a, 0x01, 0x30, 0x10, "4:00"				},
	{0x2a, 0x01, 0x30, 0x00, "5:00"				},

	{0   , 0xfe, 0   ,    4, "Vs Tag"			},
	{0x2a, 0x01, 0xc0, 0xc0, "2:30"				},
	{0x2a, 0x01, 0xc0, 0x80, "3:00"				},
	{0x2a, 0x01, 0xc0, 0x40, "4:00"				},
	{0x2a, 0x01, 0xc0, 0x00, "5:00"				},
};

STDDIPINFO(Blzntrnd)

static struct BurnDIPInfo Gstrik2DIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL				},
	{0x16, 0xff, 0xff, 0xfc, NULL				},
	{0x17, 0xff, 0xff, 0xff, NULL				},
	{0x18, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Player Vs Com"		},
	{0x15, 0x01, 0x03, 0x03, "1:00"				},
	{0x15, 0x01, 0x03, 0x02, "1:30"				},
	{0x15, 0x01, 0x03, 0x01, "2:00"				},
	{0x15, 0x01, 0x03, 0x00, "2:30"				},

	{0   , 0xfe, 0   ,    4, "1P Vs 2P"			},
	{0x15, 0x01, 0x0c, 0x0c, "0:45"				},
	{0x15, 0x01, 0x0c, 0x08, "1:00"				},
	{0x15, 0x01, 0x0c, 0x04, "1:30"				},
	{0x15, 0x01, 0x0c, 0x00, "2:00"				},

	{0   , 0xfe, 0   ,    4, "Extra Time"			},
	{0x15, 0x01, 0x30, 0x30, "0:30"				},
	{0x15, 0x01, 0x30, 0x20, "0:45"				},
	{0x15, 0x01, 0x30, 0x10, "1:00"				},
	{0x15, 0x01, 0x30, 0x00, "Off"				},

	{0   , 0xfe, 0   ,    2, "Time Period"			},
	{0x15, 0x01, 0x80, 0x80, "Sudden Death"			},
	{0x15, 0x01, 0x80, 0x00, "Full"				},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x16, 0x01, 0x07, 0x07, "Very Easy"			},
	{0x16, 0x01, 0x07, 0x06, "Easier"			},
	{0x16, 0x01, 0x07, 0x05, "Easy"				},
	{0x16, 0x01, 0x07, 0x04, "Normal"			},
	{0x16, 0x01, 0x07, 0x03, "Medium"			},
	{0x16, 0x01, 0x07, 0x02, "Hard"				},
	{0x16, 0x01, 0x07, 0x01, "Hardest"			},
	{0x16, 0x01, 0x07, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x08, 0x00, "Off"				},
	{0x16, 0x01, 0x08, 0x08, "On"				},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x16, 0x01, 0x10, 0x10, "Off"				},
	{0x16, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x16, 0x01, 0x20, 0x20, "Off"				},
	{0x16, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x16, 0x01, 0x40, 0x40, "Off"				},
	{0x16, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x16, 0x01, 0x80, 0x80, "Off"				},
	{0x16, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    32, "Coin A"			},
	{0x17, 0x01, 0x1f, 0x1c, "4 Coins 1 Credits"		},
	{0x17, 0x01, 0x1f, 0x1d, "3 Coins 1 Credits"		},
	{0x17, 0x01, 0x1f, 0x18, "4 Coins 2 Credits"		},
	{0x17, 0x01, 0x1f, 0x1e, "2 Coins 1 Credits"		},
	{0x17, 0x01, 0x1f, 0x19, "3 Coins 2 Credits"		},
	{0x17, 0x01, 0x1f, 0x14, "4 Coins 3 Credits"		},
	{0x17, 0x01, 0x1f, 0x10, "4 Coins 4 Credits"		},
	{0x17, 0x01, 0x1f, 0x15, "3 Coins 3 Credits"		},
	{0x17, 0x01, 0x1f, 0x1a, "2 Coins 2 Credits"		},
	{0x17, 0x01, 0x1f, 0x1f, "1 Coin  1 Credits"		},
	{0x17, 0x01, 0x1f, 0x0c, "4 Coins 5 Credits"		},
	{0x17, 0x01, 0x1f, 0x11, "3 Coins 4 Credits"		},
	{0x17, 0x01, 0x1f, 0x08, "4 Coins/6 Credits"		},
	{0x17, 0x01, 0x1f, 0x16, "2 Coins 3 Credits"		},
	{0x17, 0x01, 0x1f, 0x0d, "3 Coins/5 Credits"		},
	{0x17, 0x01, 0x1f, 0x04, "4 Coins 7 Credits"		},
	{0x17, 0x01, 0x1f, 0x00, "4 Coins/8 Credits"		},
	{0x17, 0x01, 0x1f, 0x09, "3 Coins/6 Credits"		},
	{0x17, 0x01, 0x1f, 0x12, "2 Coins 4 Credits"		},
	{0x17, 0x01, 0x1f, 0x1b, "1 Coin  2 Credits"		},
	{0x17, 0x01, 0x1f, 0x05, "3 Coins/7 Credits"		},
	{0x17, 0x01, 0x1f, 0x0e, "2 Coins 5 Credits"		},
	{0x17, 0x01, 0x1f, 0x01, "3 Coins/8 Credits"		},
	{0x17, 0x01, 0x1f, 0x0a, "2 Coins 6 Credits"		},
	{0x17, 0x01, 0x1f, 0x17, "1 Coin  3 Credits"		},
	{0x17, 0x01, 0x1f, 0x06, "2 Coins 7 Credits"		},
	{0x17, 0x01, 0x1f, 0x02, "2 Coins 8 Credits"		},
	{0x17, 0x01, 0x1f, 0x13, "1 Coin  4 Credits"		},
	{0x17, 0x01, 0x1f, 0x0f, "1 Coin  5 Credits"		},
	{0x17, 0x01, 0x1f, 0x0b, "1 Coin  6 Credits"		},
	{0x17, 0x01, 0x1f, 0x07, "1 Coin  7 Credits"		},
	{0x17, 0x01, 0x1f, 0x03, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x17, 0x01, 0xe0, 0xc0, "1 Coin  1 Credits"		},
	{0x17, 0x01, 0xe0, 0xa0, "1 Coin  2 Credits"		},
	{0x17, 0x01, 0xe0, 0x80, "1 Coin  3 Credits"		},
	{0x17, 0x01, 0xe0, 0x60, "1 Coin  4 Credits"		},
	{0x17, 0x01, 0xe0, 0x40, "1 Coin  5 Credits"		},
	{0x17, 0x01, 0xe0, 0x20, "1 Coin  6 Credits"		},
	{0x17, 0x01, 0xe0, 0x00, "1 Coin  7 Credits"		},
	{0x17, 0x01, 0xe0, 0xe0, "Same as Coin A"		},

	{0   , 0xfe, 0   ,    4, "Credits to Start"		},
	{0x18, 0x01, 0x03, 0x03, "1"				},
	{0x18, 0x01, 0x03, 0x02, "2"				},
	{0x18, 0x01, 0x03, 0x01, "3"				},
	{0x18, 0x01, 0x03, 0x00, "4"				},

	{0   , 0xfe, 0   ,    4, "Credits to Continue"		},
	{0x18, 0x01, 0x0c, 0x0c, "1"				},
	{0x18, 0x01, 0x0c, 0x08, "2"				},
	{0x18, 0x01, 0x0c, 0x04, "3"				},
	{0x18, 0x01, 0x0c, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Continue"			},
	{0x18, 0x01, 0x10, 0x00, "Off"				},
	{0x18, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x18, 0x01, 0x20, 0x20, "Off"				},
	{0x18, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Playmode"			},
	{0x18, 0x01, 0x40, 0x40, "1 Credit for 1 Player"	},
	{0x18, 0x01, 0x40, 0x00, "1 Credit for 2 Players"	},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x18, 0x01, 0x80, 0x80, "Off"				},
	{0x18, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Gstrik2)

static struct BurnDIPInfo SkyalertDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL				},
	{0x14, 0xff, 0xff, 0xff, NULL				},
	{0x15, 0xff, 0xff, 0xff, NULL				},
	{0x16, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x03, 0x02, "Easy"				},
	{0x14, 0x01, 0x03, 0x03, "Normal"			},
	{0x14, 0x01, 0x03, 0x01, "Hard"				},
	{0x14, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x14, 0x01, 0x0c, 0x08, "1"				},
	{0x14, 0x01, 0x0c, 0x04, "2"				},
	{0x14, 0x01, 0x0c, 0x0c, "3"				},
	{0x14, 0x01, 0x0c, 0x00, "4"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x14, 0x01, 0x30, 0x30, "100K, every 400K"		},
	{0x14, 0x01, 0x30, 0x20, "200K, every 400K"		},
	{0x14, 0x01, 0x30, 0x10, "200K"				},
	{0x14, 0x01, 0x30, 0x00, "None"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x40, 0x00, "No"				},
	{0x14, 0x01, 0x40, 0x40, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x80, 0x00, "Off"				},
	{0x14, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Skyalert)

static struct BurnDIPInfo PangpomsDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL				},
	{0x14, 0xff, 0xff, 0xef, NULL				},
	{0x15, 0xff, 0xff, 0xff, NULL				},
	{0x16, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Time Speed"			},
	{0x14, 0x01, 0x03, 0x00, "Slowest"			},
	{0x14, 0x01, 0x03, 0x01, "Slow"				},
	{0x14, 0x01, 0x03, 0x03, "Normal"			},
	{0x14, 0x01, 0x03, 0x02, "Fast"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x14, 0x01, 0x0c, 0x08, "1"				},
	{0x14, 0x01, 0x0c, 0x04, "2"				},
	{0x14, 0x01, 0x0c, 0x0c, "3"				},
	{0x14, 0x01, 0x0c, 0x00, "4"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x14, 0x01, 0x30, 0x20, "400k and 800k"		},
	{0x14, 0x01, 0x30, 0x30, "400k"				},
	{0x14, 0x01, 0x30, 0x10, "800k"				},
	{0x14, 0x01, 0x30, 0x00, "None"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x40, 0x00, "No"				},
	{0x14, 0x01, 0x40, 0x40, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x80, 0x00, "Off"				},
	{0x14, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Pangpoms)

static struct BurnDIPInfo PoittoDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x11, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x11, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x11, 0x01, 0x40, 0x40, "Off"				},
	{0x11, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x80, "Off"				},
	{0x11, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x00, "Easy"				},
	{0x12, 0x01, 0x03, 0x03, "Normal"			},
	{0x12, 0x01, 0x03, 0x02, "Hard"				},
	{0x12, 0x01, 0x03, 0x01, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x12, 0x01, 0x20, 0x00, "No"				},
	{0x12, 0x01, 0x20, 0x20, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x40, 0x00, "Off"				},
	{0x12, 0x01, 0x40, 0x40, "On"				},
};

STDDIPINFO(Poitto)

static struct BurnDIPInfo LastfortDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL				},
	{0x14, 0xff, 0xff, 0xff, NULL				},
	{0x15, 0xff, 0xff, 0xff, NULL				},
	{0x16, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x03, 0x02, "Easy"				},
	{0x14, 0x01, 0x03, 0x03, "Normal"			},
	{0x14, 0x01, 0x03, 0x01, "Hard"				},
	{0x14, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Retry Level On Continue"	},
	{0x14, 0x01, 0x08, 0x08, "Ask Player"			},
	{0x14, 0x01, 0x08, 0x00, "Yes"				},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x14, 0x01, 0x10, 0x10, "2 Credits"			},
	{0x14, 0x01, 0x10, 0x00, "1 Credit"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x20, 0x00, "No"				},
	{0x14, 0x01, 0x20, 0x20, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x40, 0x00, "Off"				},
	{0x14, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    1, "Tiles"			},
//	{0x14, 0x01, 0x80, 0x00, "Cards"			},
	{0x14, 0x01, 0x80, 0x80, "Mahjong"			},
};

STDDIPINFO(Lastfort)

static struct BurnDIPInfo LastferoDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL				},
	{0x14, 0xff, 0xff, 0xff, NULL				},
	{0x15, 0xff, 0xff, 0xff, NULL				},
	{0x16, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x03, 0x00, "Easiest"			},
	{0x14, 0x01, 0x03, 0x01, "Easy"				},
	{0x14, 0x01, 0x03, 0x03, "Normal"			},
	{0x14, 0x01, 0x03, 0x02, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Retry Level On Continue"	},
	{0x14, 0x01, 0x08, 0x08, "Ask Player"			},
	{0x14, 0x01, 0x08, 0x00, "Yes"				},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x14, 0x01, 0x10, 0x10, "2 Credits"			},
	{0x14, 0x01, 0x10, 0x00, "1 Credit"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x20, 0x00, "No"				},
	{0x14, 0x01, 0x20, 0x20, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x40, 0x00, "Off"				},
	{0x14, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    1, "Tiles"			},
	//{0x14, 0x01, 0x80, 0x00, "Cards"			},
	{0x14, 0x01, 0x80, 0x80, "Mahjong"			},
};

STDDIPINFO(Lastfero)

static struct BurnDIPInfo LadykillDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x6f, NULL				},
	{0x0c, 0xff, 0xff, 0xff, NULL				},
	{0x0d, 0xff, 0xff, 0xff, NULL				},
	{0x0e, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0b, 0x01, 0x03, 0x01, "1"				},
	{0x0b, 0x01, 0x03, 0x00, "2"				},
	{0x0b, 0x01, 0x03, 0x03, "3"				},
	{0x0b, 0x01, 0x03, 0x02, "4"				},
	
	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0b, 0x01, 0x0c, 0x08, "Easy"				},
	{0x0b, 0x01, 0x0c, 0x0c, "Normal"			},
	{0x0b, 0x01, 0x0c, 0x04, "Hard"				},
	{0x0b, 0x01, 0x0c, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "Nudity"			},
	{0x0b, 0x01, 0x10, 0x10, "Partial"			},
	{0x0b, 0x01, 0x10, 0x00, "Full"				},

	{0   , 0xfe, 0   ,    2, "Service Mode / Free Play"	},
	{0x0b, 0x01, 0x20, 0x20, "Off"				},
	{0x0b, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x0b, 0x01, 0x40, 0x00, "No"				},
	{0x0b, 0x01, 0x40, 0x40, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0b, 0x01, 0x80, 0x80, "Off"				},
	{0x0b, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x0c, 0x01, 0x07, 0x00, "3 Coins 1 Credits"		},
	{0x0c, 0x01, 0x07, 0x01, "2 Coins 1 Credits"		},
	{0x0c, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x0c, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x0c, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x0c, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x0c, 0x01, 0x07, 0x03, "1 Coin  5 Credits"		},
	{0x0c, 0x01, 0x07, 0x02, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x0c, 0x01, 0x38, 0x00, "3 Coins 1 Credits"		},
	{0x0c, 0x01, 0x38, 0x08, "2 Coins 1 Credits"		},
	{0x0c, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x0c, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x0c, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x0c, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x0c, 0x01, 0x38, 0x18, "1 Coin  5 Credits"		},
	{0x0c, 0x01, 0x38, 0x10, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0c, 0x01, 0x40, 0x40, "Off"				},
	{0x0c, 0x01, 0x40, 0x00, "On"				},
};

STDDIPINFO(Ladykill)

static struct BurnDIPInfo DharmaDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x12, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x12, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x40, 0x40, "Off"				},
	{0x12, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x03, 0x02, "Easy"				},
	{0x13, 0x01, 0x03, 0x03, "Normal"			},
	{0x13, 0x01, 0x03, 0x01, "Hard"				},
	{0x13, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Timer"			},
	{0x13, 0x01, 0x0c, 0x08, "Slow"				},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"			},
	{0x13, 0x01, 0x0c, 0x04, "Fast"				},
	{0x13, 0x01, 0x0c, 0x00, "Fastest"			},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x13, 0x01, 0x10, 0x10, "2 Credits"			},
	{0x13, 0x01, 0x10, 0x00, "1 Credit"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x13, 0x01, 0x20, 0x00, "No"				},
	{0x13, 0x01, 0x20, 0x20, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x00, "Off"				},
	{0x13, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Freeze (Cheat)"		},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Dharma)

static struct BurnDIPInfo PururunDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x12, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x12, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x38, 0x00, "Free Play"			},
	
	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x40, 0x40, "Off"				},
	{0x12, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x03, 0x02, "Easiest"			},
	{0x13, 0x01, 0x03, 0x01, "Easy"				},
	{0x13, 0x01, 0x03, 0x03, "Normal"			},
	{0x13, 0x01, 0x03, 0x00, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Join In"			},
	{0x13, 0x01, 0x04, 0x00, "No"				},
	{0x13, 0x01, 0x04, 0x04, "Yes"				},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x13, 0x01, 0x08, 0x00, "1 Credit"			},
	{0x13, 0x01, 0x08, 0x08, "2 Credits"			},

	{0   , 0xfe, 0   ,    2, "Bombs"			},
	{0x13, 0x01, 0x10, 0x10, "1"				},
	{0x13, 0x01, 0x10, 0x00, "2"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x13, 0x01, 0x20, 0x00, "No"				},
	{0x13, 0x01, 0x20, 0x20, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x00, "Off"				},
	{0x13, 0x01, 0x40, 0x40, "On"				},
};

STDDIPINFO(Pururun)

static struct BurnDIPInfo KaratourDIPList[]=
{
	{0x13, 0xff, 0xff, 0x7f, NULL				},
	{0x14, 0xff, 0xff, 0xff, NULL				},
	{0x15, 0xff, 0xff, 0xff, NULL				},
	{0x16, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x13, 0x01, 0x03, 0x01, "1"				},
	{0x13, 0x01, 0x03, 0x00, "2"				},
	{0x13, 0x01, 0x03, 0x03, "3"				},
	{0x13, 0x01, 0x03, 0x02, "4"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x0c, 0x08, "Easy"				},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"			},
	{0x13, 0x01, 0x0c, 0x04, "Hard"				},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Time"				},
	{0x13, 0x01, 0x10, 0x10, "60"				},
	{0x13, 0x01, 0x10, 0x00, "40"				},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x13, 0x01, 0x20, 0x20, "Off"				},
	{0x13, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x13, 0x01, 0x40, 0x00, "No"				},
	{0x13, 0x01, 0x40, 0x40, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x14, 0x01, 0x07, 0x00, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x07, 0x01, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x07, 0x03, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0x07, 0x02, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x14, 0x01, 0x38, 0x00, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x38, 0x08, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0x38, 0x10, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x40, 0x40, "Off"				},
	{0x14, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x80, 0x80, "Off"				},
	{0x14, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Karatour)

static struct BurnDIPInfo DaitoridDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0x7f, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x11, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x11, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x11, 0x01, 0x40, 0x40, "Off"				},
	{0x11, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x80, "Off"				},
	{0x11, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Timer Speed"			},
	{0x12, 0x01, 0x03, 0x02, "Slower"			},
	{0x12, 0x01, 0x03, 0x03, "Normal"			},
	{0x12, 0x01, 0x03, 0x01, "Fast"				},
	{0x12, 0x01, 0x03, 0x00, "Fastest"			},

	{0   , 0xfe, 0   ,    2, "Winning Rounds (P VS P)"	},
	{0x12, 0x01, 0x08, 0x00, "1/1"				},
	{0x12, 0x01, 0x08, 0x08, "2/3"				},

	{0   , 0xfe, 0   ,    4, "Allow Continue"		},
	{0x12, 0x01, 0x30, 0x30, "Retry Level"			},
	{0x12, 0x01, 0x30, 0x20, "Ask Player"			},
	{0x12, 0x01, 0x30, 0x10, "No"				},
	{0x12, 0x01, 0x30, 0x00, "Retry Level"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x40, 0x00, "Off"				},
	{0x12, 0x01, 0x40, 0x40, "On"				},
};

STDDIPINFO(Daitorid)

static struct BurnDIPInfo GunmastDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL				},
	{0x16, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x15, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x15, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x15, 0x01, 0x40, 0x40, "Off"				},
	{0x15, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x15, 0x01, 0x80, 0x80, "Off"				},
	{0x15, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x16, 0x01, 0x03, 0x03, "Normal"			},
	{0x16, 0x01, 0x03, 0x02, "Hard"				},
	{0x16, 0x01, 0x03, 0x01, "Harder"			},
	{0x16, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x16, 0x01, 0x04, 0x00, "No"				},
	{0x16, 0x01, 0x04, 0x04, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Allow P2 to Join Game"	},
	{0x16, 0x01, 0x08, 0x00, "No"				},
	{0x16, 0x01, 0x08, 0x08, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x10, 0x00, "Off"				},
	{0x16, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x16, 0x01, 0x20, 0x20, "1"				},
	{0x16, 0x01, 0x20, 0x00, "2"				},
};

STDDIPINFO(Gunmast)

static struct BurnDIPInfo _3kokushiDIPList[]=
{
	{0x13, 0xff, 0xff, 0x7f, NULL				},
	{0x14, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x03, 0x02, "Easy"				},
	{0x14, 0x01, 0x03, 0x03, "Normal"			},
	{0x14, 0x01, 0x03, 0x01, "Hard"				},
	{0x14, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x10, 0x00, "No"				},
	{0x14, 0x01, 0x10, 0x10, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Service Mode / Free Play"	},
	{0x14, 0x01, 0x20, 0x20, "Off"				},
	{0x14, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Helps"			},
	{0x14, 0x01, 0xc0, 0x00, "1"				},
	{0x14, 0x01, 0xc0, 0x40, "2"				},
	{0x14, 0x01, 0xc0, 0xc0, "3"				},
	{0x14, 0x01, 0xc0, 0x80, "4"				},
};

STDDIPINFO(_3kokushi)

static struct BurnDIPInfo PuzzliDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL				},
	{0x14, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    3, "Difficulty"			},
	{0x14, 0x01, 0x03, 0x02, "Easy"				},
	{0x14, 0x01, 0x03, 0x03, "Normal"			},
	{0x14, 0x01, 0x03, 0x00, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Join In"			},
	{0x14, 0x01, 0x04, 0x00, "No"				},
	{0x14, 0x01, 0x04, 0x04, "Yes"				},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x14, 0x01, 0x08, 0x00, "1 Credit"			},
	{0x14, 0x01, 0x08, 0x08, "2 Credits"			},

	{0   , 0xfe, 0   ,    2, "Winning Rounds (P VS P)"	},
	{0x14, 0x01, 0x10, 0x00, "1/1"				},
	{0x14, 0x01, 0x10, 0x10, "2/3"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x20, 0x00, "No"				},
	{0x14, 0x01, 0x20, 0x20, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x40, 0x00, "Off"				},
	{0x14, 0x01, 0x40, 0x40, "On"				},
};

STDDIPINFO(Puzzli)

static struct BurnDIPInfo Toride2gDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL				},
	{0x14, 0xff, 0xff, 0xf7, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Timer Speed"			},
	{0x14, 0x01, 0x03, 0x02, "Slower"			},
	{0x14, 0x01, 0x03, 0x03, "Normal"			},
	{0x14, 0x01, 0x03, 0x01, "Fast"				},
	{0x14, 0x01, 0x03, 0x00, "Fastest"			},

	{0   , 0xfe, 0   ,    2, "Tile Arrangement"		},
	{0x14, 0x01, 0x04, 0x04, "Normal"			},
	{0x14, 0x01, 0x04, 0x00, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Retry Level On Continue"	},
	{0x14, 0x01, 0x08, 0x00, "Ask Player"			},
	{0x14, 0x01, 0x08, 0x08, "Yes"				},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x14, 0x01, 0x10, 0x10, "2 Credits"			},
	{0x14, 0x01, 0x10, 0x00, "1 Credit"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x20, 0x00, "No"				},
	{0x14, 0x01, 0x20, 0x20, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x40, 0x00, "Off"				},
	{0x14, 0x01, 0x40, 0x40, "On"				},
};

STDDIPINFO(Toride2g)

static struct BurnDIPInfo MsgogoDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL				},
	{0x14, 0xff, 0xff, 0x7f, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x03, 0x02, "Easy"				},
	{0x14, 0x01, 0x03, 0x03, "Normal"			},
	{0x14, 0x01, 0x03, 0x01, "Hard"				},
	{0x14, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x14, 0x01, 0x04, 0x04, "Off"				},
	{0x14, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x14, 0x01, 0x08, 0x08, "Off"				},
	{0x14, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Allow P2 to Join Game"	},
	{0x14, 0x01, 0x10, 0x00, "Off"				},
	{0x14, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x14, 0x01, 0x20, 0x20, "2"				},
	{0x14, 0x01, 0x20, 0x00, "3"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x40, 0x00, "Off"				},
	{0x14, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x14, 0x01, 0x80, 0x80, "Japanese"			},
	{0x14, 0x01, 0x80, 0x00, "English"			},
};

STDDIPINFO(Msgogo)

static struct BurnDIPInfo BalcubeDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x11, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x11, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x11, 0x01, 0x40, 0x40, "Off"				},
	{0x11, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x80, "Off"				},
	{0x11, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x02, "Easy"				},
	{0x12, 0x01, 0x03, 0x03, "Normal"			},
	{0x12, 0x01, 0x03, 0x01, "Hard"				},
	{0x12, 0x01, 0x03, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x12, 0x01, 0x04, 0x00, "1 Credit"			},
	{0x12, 0x01, 0x04, 0x04, "2 Credits"			},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x12, 0x01, 0x08, 0x08, "2"				},
	{0x12, 0x01, 0x08, 0x00, "3"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x12, 0x01, 0x10, 0x00, "No"				},
	{0x12, 0x01, 0x10, 0x10, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x80, 0x00, "Off"				},
	{0x12, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Balcube)

static struct BurnDIPInfo BangballDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0x7f, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x11, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x11, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x38, 0x00, "Free Play"			},
	
	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x11, 0x01, 0x40, 0x40, "Off"				},
	{0x11, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x80, "Off"				},
	{0x11, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x02, "Easy"				},
	{0x12, 0x01, 0x03, 0x03, "Normal"			},
	{0x12, 0x01, 0x03, 0x01, "Hard"				},
	{0x12, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x0c, 0x08, "2"				},
	{0x12, 0x01, 0x0c, 0x04, "3"				},
	{0x12, 0x01, 0x0c, 0x0c, "4"				},
	{0x12, 0x01, 0x0c, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x12, 0x01, 0x10, 0x00, "No"				},
	{0x12, 0x01, 0x10, 0x10, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x20, 0x00, "Off"				},
	{0x12, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x12, 0x01, 0x40, 0x40, "Off"				},
	{0x12, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x12, 0x01, 0x80, 0x80, "Japanese"			},
	{0x12, 0x01, 0x80, 0x00, "English"			},
};

STDDIPINFO(Bangball)

static struct BurnDIPInfo BatlbublDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x11, 0x01, 0x03, 0x02, "Easy"				},
	{0x11, 0x01, 0x03, 0x03, "Normal"			},
	{0x11, 0x01, 0x03, 0x01, "Hard"				},
	{0x11, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x11, 0x01, 0x0c, 0x08, "2"				},
	{0x11, 0x01, 0x0c, 0x04, "3"				},
	{0x11, 0x01, 0x0c, 0x0c, "4"				},
	{0x11, 0x01, 0x0c, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x11, 0x01, 0x10, 0x00, "No"				},
	{0x11, 0x01, 0x10, 0x10, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x20, 0x00, "Off"				},
	{0x11, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x12, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x12, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x40, 0x40, "Off"				},
	{0x12, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Batlbubl)

static UINT16 metro_irq_cause_r()
{
	UINT16 res = 0;
	for (INT32 i = 0; i < 8; i++)
		res |= (requested_int[i] << i);

	return res;
}

static void update_irq_state()
{
	UINT16 irq = metro_irq_cause_r() & ~irq_enable;

	if (irq_line == -1)
	{
		UINT8 irq_level[8] = { 0,0,0,0,0,0,0,0 };

		for (INT32 i = 0; i < 8; i++)
			if (BIT(irq, i))
				irq_level[irq_levels[i] & 7] = 1;

		for (INT32 i = 0; i < 8; i++)
			SekSetIRQLine(i, irq_level[i] ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	}
	else
	{
		SekSetIRQLine(irq_line, irq ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	}
}

static void metro_irq_cause_w(UINT16 data)
{
	data &= ~irq_enable;

	for (INT32 i = 0; i < 8; i++)
		if (BIT(data, i)) requested_int[i] = 0;

	update_irq_state();
}

static void metro_blitter_write(INT32 offset)
{
	offset &= 0xe;

	if (offset == 0x0c)
	{
		UINT16 *m_blitter_regs = (UINT16*)DrvBlitter;
		UINT8 *ramdst[4] = { NULL, DrvVidRAM0, DrvVidRAM1, DrvVidRAM2 };

		UINT8 *src     = DrvGfxROM;
		size_t src_len = graphics_length;

		UINT32 tmap     = (m_blitter_regs[0x00 / 2] << 16) + m_blitter_regs[0x02 / 2];
		UINT32 src_offs = (m_blitter_regs[0x04 / 2] << 16) + m_blitter_regs[0x06 / 2];
		UINT32 dst_offs = (m_blitter_regs[0x08 / 2] << 16) + m_blitter_regs[0x0a / 2];

		UINT8 *dst = ramdst[tmap];

		INT32 offs2 = (~dst_offs >> 7) & 1;
		dst_offs >>=  8;

		while (1)
		{
			UINT16 b1, b2, count;

			src_offs %= src_len;
			b1 = src[src_offs];
			src_offs++;

			count = ((~b1) & 0x3f) + 1;

			switch ((b1 & 0xc0) >> 6)
			{
				case 0:
				{
					if (b1 == 0)
					{
						requested_int[blitter_bit] = 1;
						blit_timer = 1;
						return;
					}

					while (count--)
					{
						src_offs %= src_len;
						b2 = src[src_offs];
						src_offs++;
	
						dst_offs &= 0xffff;
						dst[dst_offs*2+offs2] = b2;
						dst_offs = ((dst_offs + 1) & (0x100 - 1)) | (dst_offs & (~(0x100 - 1)));
					}
					break;
				}

				case 1:
				{
					src_offs %= src_len;
					b2 = src[src_offs];
					src_offs++;

					while (count--)
					{
						dst_offs &= 0xffff;
						dst[dst_offs*2+offs2] = b2;
						dst_offs = ((dst_offs + 1) & (0x100 - 1)) | (dst_offs & (~(0x100 - 1)));
						b2++;
					}
					break;
				}

				case 2:
				{
					src_offs %= src_len;
					b2 = src[src_offs];
					src_offs++;

					while (count--)
					{
						dst_offs &= 0xffff;
						dst[dst_offs*2+offs2] = b2;
						dst_offs = ((dst_offs + 1) & (0x100 - 1)) | (dst_offs & (~(0x100 - 1)));
					}
					break;
				}

				case 3:
				{
					if (b1 == 0xc0)
					{
						dst_offs +=   0x100;
						dst_offs &= ~(0x100 - 1);
						dst_offs |=  (0x100 - 1) & (m_blitter_regs[0x0a / 2] >> (7 + 1));
					}
					else
					{
						dst_offs += count;
					}
					break;
				}
			}
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------------

static void palette_write(INT32 offset)
{
	offset &= 0x1ffe;

	UINT16 p = *((UINT16*)(DrvPalRAM + offset));

	INT32 r = (p >>  6) & 0x1f;
	INT32 g = (p >> 11) & 0x1f;
	INT32 b = (p >>  1) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	konami_palette32[offset/2] = r * 0x10000 | g * 0x100 | b;
}

static void __fastcall metro_common_palette_write_byte(UINT32 address, UINT8 data)
{
	DrvPalRAM[(address & 0x1fff)^1] = data;
	palette_write(address);	
}

static void __fastcall metro_common_palette_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvPalRAM + (address & 0x1ffe))) = data;
	palette_write(address);	
}

static UINT8 __fastcall metro_common_read_byte(UINT32 address)
{
	address &= 0x7ffff;

	if ((address & 0xfff0000) == 0x060000) {
		return DrvGfxROM[(gfxrom_bank + (address & 0xffff)) ^ 0];
	}

	switch (address)
	{
		case 0x0788a3:
			return metro_irq_cause_r();
	}

	bprintf (0, _T("Common RB %5.5x\n"), address);

	return 0;
}

static UINT16 __fastcall metro_common_read_word(UINT32 address)
{
	address &= 0x7ffff;

	if ((address & 0xfff0000) == 0x060000) {
		INT32 offset = gfxrom_bank + (address & 0xfffe);
		return DrvGfxROM[offset + 0] * 256 + DrvGfxROM[offset + 1];
	}

// karatour
	if ((address & 0xffff000) == 0x075000) {
		UINT16 *dst = (UINT16*)DrvVidRAM0;
		return dst[((address & 0x7f) + ((address & 0xf80) * 4)) / 2];
	}

	if ((address & 0xffff000) == 0x076000) {
		UINT16 *dst = (UINT16*)DrvVidRAM1;
		return dst[((address & 0x7f) + ((address & 0xf80) * 4)) / 2];
	}

	if ((address & 0xffff000) == 0x077000) {
		UINT16 *dst = (UINT16*)DrvVidRAM2;
		return dst[((address & 0x7f) + ((address & 0xf80) * 4)) / 2];
	}
// end

	switch (address)
	{
		case 0x0788a2:
			return metro_irq_cause_r();
	}

	bprintf (0, _T("Common RW %5.5x\n"), address);

	return 0;
}

static void __fastcall metro_common_write_word(UINT32 address, UINT16 data)
{
	address &= 0x7ffff;

// karatour
	if ((address & 0xffff000) == 0x075000) {
		UINT16 *dst = (UINT16*)DrvVidRAM0;
		dst[((address & 0x7f) + ((address & 0xf80) * 4)) / 2] = data;
		return;
	}

	if ((address & 0xffff000) == 0x076000) {
		UINT16 *dst = (UINT16*)DrvVidRAM1;
		dst[((address & 0x7f) + ((address & 0xf80) * 4)) / 2] = data;
		return;
	}

	if ((address & 0xffff000) == 0x077000) {
		UINT16 *dst = (UINT16*)DrvVidRAM2;
		dst[((address & 0x7f) + ((address & 0xf80) * 4)) / 2] = data;
		return;
	}
// end

	// mirror or due to chip revision?
	if ((address >= 0x078800 && address <= 0x078813) || (address >= 0x079700 && address <= 0x079713)) {
		*((UINT16*)(DrvVidRegs + (address & 0x1e))) = data;
		return;
	}

	if (address >= 0x078860 && address <= 0x07886b) {
		*((UINT16*)(DrvWindow + (address & 0xe))) = data;
		return;
	}

	if (address >= 0x078870 && address <= 0x07887b) {
		*((UINT16*)(DrvScroll + (address & 0xe))) = data;
		return;
	}

	if (address >= 0x078840 && address <= 0x07884d) {
		*((UINT16*)(DrvBlitter + (address & 0xe))) = data;
		metro_blitter_write(address);
		return;
	}

	switch (address)
	{
		case 0x078850:
		case 0x078852:
		case 0x078880:
		case 0x078890:
		case 0x0788a0:
		case 0x0788a6: // crt registers?
		return;

		case 0x0788a2:
			metro_irq_cause_w(data);
		return;

		case 0x0788a4:
			irq_enable = data;
		return;

		case 0x0788a8:
			soundlatch = data;
			sound_busy = 1;
			if (sound_system == 2 || sound_system == 5) {
				upd7810SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_ACK);
			}
		return;

		case 0x0788aa:
			gfxrom_bank = (data & 0x1ff) * 0x10000;
			if (gfxrom_bank >= graphics_length) gfxrom_bank = graphics_length-0x10000;
		return;

		case 0x0788ac:
			screen_control = data;
		return;
	}

	bprintf (0, _T("Common WW %5.5x\n"), address);
}

static void __fastcall metro_common_write_byte(UINT32 address, UINT8 data)
{
	address &= 0x7ffff;

	bprintf (0, _T("Common WB %5.5x, %2.2x\n"), address, data);
}

// Map Imagetek I4100/I4220 and main RAM
static void metro_common_map_ram(UINT32 chip_address, INT32 main_ram_address)
{
	SekMapMemory(DrvVidRAM0,	chip_address + 0x000000, chip_address + 0x01ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,	chip_address + 0x020000, chip_address + 0x03ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM2,	chip_address + 0x040000, chip_address + 0x05ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,	chip_address + 0x070000, chip_address + 0x071fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		chip_address + 0x072000, chip_address + 0x073fff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		chip_address + 0x074000, chip_address + 0x074fff, MAP_RAM);
	SekMapMemory(DrvTileRAM,	chip_address + 0x078000, chip_address + 0x0787ff, MAP_RAM);

	SekMapHandler(3,		chip_address + 0x060000, chip_address + 0x06ffff, MAP_READ | MAP_WRITE);
	SekMapHandler(1,		chip_address + 0x075000, chip_address + 0x077fff, MAP_READ | MAP_WRITE);
	SekMapHandler(2,		chip_address + 0x078800, chip_address + 0x07ffff, MAP_READ | MAP_WRITE);
	SekMapHandler(4,		chip_address + 0x072000, chip_address + 0x073fff, MAP_WRITE);

	SekSetWriteWordHandler(1, 	metro_common_write_word);
	SekSetWriteByteHandler(1, 	metro_common_write_byte);
	SekSetWriteWordHandler(2, 	metro_common_write_word);
	SekSetWriteByteHandler(2, 	metro_common_write_byte);
	SekSetWriteWordHandler(4,	metro_common_palette_write_word);
	SekSetWriteByteHandler(4,	metro_common_palette_write_byte);

	SekSetReadWordHandler(1,	metro_common_read_word);
	SekSetReadByteHandler(1,	metro_common_read_byte);
	SekSetReadWordHandler(2,	metro_common_read_word);
	SekSetReadByteHandler(2,	metro_common_read_byte);
	SekSetReadWordHandler(3,	metro_common_read_word);
	SekSetReadByteHandler(3,	metro_common_read_byte);

	if (main_ram_address == -1) return;

	for (INT32 i = 0; i < 0x10; i++) {
		SekMapMemory(Drv68KRAM1,	main_ram_address + (i * 0x10000), main_ram_address + 0xffff + (i * 0x10000), MAP_RAM);
	}

}

//------------------------------------------------------------------------------------------------------------------------------------------

static void __fastcall blzntrnd_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0xe00002:
			soundlatch = data >> 8;
			ZetNmi();
		return;
	}
}

static void __fastcall blzntrnd_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0xe00001:
		return;
	}

	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall blzntrnd_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xe00000:
			return (DrvDips[0] << 0) | (DrvDips[1] << 8);

		case 0xe00002:
			return (DrvDips[2] << 0) | (DrvDips[3] << 8);

		case 0xe00004:
			return DrvInputs[0];

		case 0xe00006:
			return DrvInputs[1];

		case 0xe00008:
			return DrvInputs[2];
	}

	return 0;
}

static UINT8 __fastcall blzntrnd_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xe00000:
		case 0xe00001:
		case 0xe00002:
		case 0xe00003:
			return DrvDips[(address & 3) ^ 1];

		case 0xe00004:
		case 0xe00005:
		case 0xe00006:
		case 0xe00007:
		case 0xe00008:
		case 0xe00009:
			return DrvInputs[(address - 0xe00004)/2] >> ((~address & 1) * 8);
	}

	return 0;
}

static void __fastcall skyalert_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x400000:
			sound_status = data & 0x01;
		return;

		case 0x400002:
			// coin lockout
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall skyalert_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x400001:
			sound_status = data & 0x01;
		return;
	}

	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall skyalert_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
			return ~sound_busy & 1; // sound status

		case 0x400002:
			return 0; // nop

		case 0x400004:
			return DrvInputs[0];

		case 0x400006:
			return DrvInputs[1];

		case 0x400008:
			return DrvInputs[2];

		case 0x40000a:
			return (DrvDips[0] << 0) | (DrvDips[1] << 8);

		case 0x40000c:
			return (DrvDips[2] << 0) | (DrvDips[3] << 8);

		case 0x40000e:
			return DrvInputs[3];
	}

	return 0;
}

static UINT8 __fastcall skyalert_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x40000a:
		case 0x40000b:
		case 0x40000c:
		case 0x40000d:
			return DrvDips[(address - 0x40000a) ^ 1]; // dip0

		case 0x400004:
		case 0x400005:
		case 0x400006:
		case 0x400007:
		case 0x400008:
		case 0x400009:
			return DrvInputs[(address - 0x400004)/2] >> ((~address & 1) * 8);

		case 0x40000e:
		case 0x40000f:
			return DrvInputs[3] >> ((~address & 1) * 8);
	}

	return 0;
}

static void __fastcall pangpoms_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x800000:
			sound_status = data & 0x01;
		return;

		case 0x800002:
			// coin lockout
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall pangpoms_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall pangpoms_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x800000:
			return ~sound_busy & 1; // sound status

		case 0x800002:
			return 0; // nop

		case 0x800004:
			return DrvInputs[0];

		case 0x800006:
			return DrvInputs[1];

		case 0x800008:
			return DrvInputs[2];

		case 0x80000a:
			return (DrvDips[0] << 0) | (DrvDips[1] << 8);

		case 0x80000c:
			return (DrvDips[2] << 0) | (DrvDips[3] << 8);

		case 0x80000e:
			return DrvInputs[3];
	}

	return 0;
}

static UINT8 __fastcall pangpoms_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x80000a:
		case 0x80000b:
		case 0x80000c:
		case 0x80000d:
			return DrvDips[(address - 0x80000a) ^ 1]; // dip0

		case 0x800004:
		case 0x800005:
		case 0x800006:
		case 0x800007:
		case 0x800008:
		case 0x800009:
			return DrvInputs[(address - 0x800004)/2] >> ((~address & 1) * 8);

		case 0x80000e:
		case 0x80000f:
			return DrvInputs[3] >> ((~address & 1) * 8);
	}

	return 0;
}

static void __fastcall poitto_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x800000:
			sound_status = data & 0x01;
		return;

		case 0x800002:
		case 0x800004:
		case 0x800006:
		case 0x800008:
			// coin lockout
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall poitto_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall poitto_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x800000:
			return (DrvInputs[0] & 0xff7f) | ((sound_busy & 1) << 7);

		case 0x800002:
			return DrvInputs[1];

		case 0x800004:
			return (DrvDips[0] << 0) | (DrvDips[1] << 8);

		case 0x800006:
			return DrvInputs[2];
	}

	return 0;
}

static UINT8 __fastcall poitto_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x800000:
			return DrvInputs[0] >> 8;

		case 0x800001:
			return (DrvInputs[0] & 0x7f) | ((sound_busy & 1) << 7); // sound status

		case 0x800002:
			return DrvInputs[1] >> 8;

		case 0x800003:
			return DrvInputs[1];

		case 0x800004:
		case 0x800005:
			return DrvDips[(address & 1) ^ 1];

		case 0x800006:
			return DrvInputs[2] >> 8;

		case 0x800007:
			return DrvInputs[2];
	}

	return 0;
}

static void __fastcall lastforg_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x400000:
			sound_status = data & 0x01;
		return;

		case 0x400002:
			// coin lockout
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall lastforg_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);

	switch (address)
	{
		case 0x400001:
			sound_status = data & 0x01;
		return;
	}
}

static UINT16 __fastcall lastforg_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
			return ~sound_busy & 1;

		case 0x400002:
			return DrvInputs[0];

		case 0x400004:
			return DrvInputs[1];

		case 0x400006:
			return (DrvDips[0] << 0) | (DrvDips[1] << 8);

		case 0x40000a:
			return (DrvDips[2] << 0) | (DrvDips[3] << 8);

		case 0x40000c:
			return DrvInputs[2];
	}

	bprintf (0, _T("RW: %5.5x\n"), address);
	return 0;
}

static UINT8 __fastcall lastforg_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x400001:
			return ~sound_busy & 1; // sound status

		case 0x400002:
		case 0x400003:
		case 0x400004:
		case 0x400005:
			return DrvInputs[(address - 0x400002)/2] >> ((~address & 1) * 8);

		case 0x400006:
		case 0x400007:
			return DrvDips[(address - 0x400006) ^ 1]; // dip0

		case 0x40000a:
		case 0x40000b:
			return DrvDips[((address - 0x40000a) ^ 1)+2]; // dip0

		case 0x40000c:
		case 0x40000d:
			return DrvInputs[2] >> ((~address & 1) * 8);
	}

	bprintf (0, _T("RB: %5.5x\n"), address);
	return 0;
}

static void __fastcall lastfort_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0xc00000:
			sound_status = data & 0x01;
		return;

		case 0xc00002:
			// coin lockout
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall lastfort_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall lastfort_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xc00000:
			return ~sound_busy & 1; // sound status

		case 0xc00002:
			return 0; // nop

		case 0xc00004:
			return DrvInputs[0];

		case 0xc00006:
			return DrvInputs[1];

		case 0xc00008:
			return DrvInputs[2];

		case 0xc0000a:
			return (DrvDips[0] << 0) | (DrvDips[1] << 8);

		case 0xc0000c:
			return (DrvDips[2] << 0) | (DrvDips[3] << 8);

		case 0xc0000e:
			return DrvInputs[3];
	}

	return 0;
}

static UINT8 __fastcall lastfort_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xc0000a:
		case 0xc0000b:
		case 0xc0000c:
		case 0xc0000d:
			return DrvDips[(address - 0xc0000a) ^ 1]; // dip0

		case 0xc00004:
		case 0xc00005:
		case 0xc00006:
		case 0xc00007:
		case 0xc00008:
		case 0xc00009:
			return DrvInputs[(address - 0xc00004)/2] >> ((~address & 1) * 8);

		case 0xc0000e:
		case 0xc0000f:
			return DrvInputs[3] >> ((~address & 1) * 8);
	}

	return 0;
}

static void __fastcall dharma_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0xc00000:
			sound_status = data & 0x01;
		return;

		case 0xc00002:
			// coin lockout
		return;

		case 0xc00004:
			// ?
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall dharma_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall dharma_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xc00000:
			return (DrvInputs[0] & 0xff7f) | ((sound_busy & 1) << 7);

		case 0xc00002:
			return DrvInputs[1];

		case 0xc00004:
			return (DrvDips[0] << 0) | (DrvDips[1] << 8);

		case 0xc00006:
			return DrvInputs[2];
	}

	return 0;
}

static UINT8 __fastcall dharma_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xc00000:
			return DrvInputs[0] >> 8;

		case 0xc00001:
			return (DrvInputs[0] & 0x7f) | ((sound_busy & 1) << 7);

		case 0xc00002:
		case 0xc00003:
			return DrvInputs[1] >> ((~address & 1) * 8);

		case 0xc00004:
		case 0xc00005:
			return DrvDips[(address - 0xc00004) ^ 1]; // dip0

		case 0xc00006:
		case 0xc00007:
			return DrvInputs[2] >> ((~address & 1) * 8);
	}

	return 0;
}

static void __fastcall pururun_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x400000:
			sound_status = data & 0x01;
		return;

		case 0x400002:
			// coin lockout
		return;

		case 0x400004:
			// ?
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall pururun_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall pururun_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
			return (DrvInputs[0] & 0xff7f) | ((sound_busy & 1) << 7);

		case 0x400002:
			return DrvInputs[1];

		case 0x400004:
			return (DrvDips[0] << 0) | (DrvDips[1] << 8);

		case 0x400006:
			return DrvInputs[2];
	}

	return 0;
}

static UINT8 __fastcall pururun_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
			return DrvInputs[0] >> 8;

		case 0x400001:
			return (DrvInputs[0] & 0x7f) | ((sound_busy & 1) << 7);

		case 0x400002:
		case 0x400003:
			return DrvInputs[1] >> ((~address & 1) * 8);

		case 0x400004:
		case 0x400005:
			return DrvDips[(address - 0x400004) ^ 1]; // dip0

		case 0x400006:
		case 0x400007:
			return DrvInputs[2] >> ((~address & 1) * 8);
	}

	return 0;
}

static void __fastcall karatour_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x400000:
			sound_status = data & 0x01;
		return;

		case 0x400002:
			// coin lockout
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall karatour_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x400003:
			// coin lockout
		return;
	}

	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall karatour_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
			return ~sound_busy & 1; // sound status

		case 0x400002:
			return DrvInputs[0];

		case 0x400004:
			return DrvInputs[1];

		case 0x400006:
			return (DrvDips[0] << 0) | (DrvDips[1] << 8);

		case 0x40000a:
			return (DrvDips[2] << 0) | (DrvDips[3] << 8);

		case 0x40000c:
			return DrvInputs[2];
	}

	return 0;
}

static UINT8 __fastcall karatour_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x400001:
			return ~sound_busy & 1; // sound status

		case 0x400006:
		case 0x400007:
			return DrvDips[(address & 1)^1]; // dip0

		case 0x40000a:
		case 0x40000b:
			return DrvDips[((address & 1)^1)+2]; // dip0

		case 0x400002:
		case 0x400003:
		case 0x400004:
		case 0x400005:
			return DrvInputs[(address - 0x400002)/2] >> ((~address & 1) * 8);

		case 0x40000c:
		case 0x40000d:
			return DrvInputs[2] >> ((~address & 1) * 8);
	}

	return 0;
}

static void __fastcall daitorid_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0xc00000:
			sound_status = data & 0x01;
		return;

		case 0xc00002:
			// coin lockout
		return;

		case 0xc00004:
			// ?
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall daitorid_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall daitorid_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xc00000:
			return (DrvInputs[0] & 0xff7f) | ((sound_busy & 1) << 7);

		case 0xc00002:
			return DrvInputs[1];

		case 0xc00004:
			return (DrvDips[0] << 0) | (DrvDips[1] << 8);

		case 0xc00006:
			return DrvInputs[2];
	}

	return 0;
}

static UINT8 __fastcall daitorid_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xc00000:
			return DrvInputs[0] >> 8;

		case 0xc00001:
			return (DrvInputs[0] & 0x7f) | ((sound_busy & 1) << 7);

		case 0xc00002:
		case 0xc00003:
			return DrvInputs[1] >> ((~address & 1) * 8);

		case 0xc00004:
		case 0xc00005:
			return DrvDips[(address - 0xc00004) ^ 1]; // dip0

		case 0xc00006:
		case 0xc00007:
			return DrvInputs[2] >> ((~address & 1) * 8);
	}

	return 0;
}

static UINT16 balcube_dip_read(UINT32 address)
{
	UINT16 d0 = DrvDips[0] + (DrvDips[1] * 256);
	UINT16 d1 = DrvInputs[3] & 0xff;

	address = ~address & 0x1fffe;

	for (UINT32 i = 1; i < 17; i++) {
		if (address == (UINT32)(1 << i)) {
			return (((d0 >> (i - 1)) & 1) * 0x40) + (((d1 >> (i - 1)) & 1) * 0x80);
		}
	}

	return 0xffff;
}

static void __fastcall msgogo_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x200002:
		case 0x200004:
		case 0x200006:
		case 0x200008:
			// coin lockout
		return;

		case 0x400000:
		case 0x400004:
		case 0x400008:
			BurnYMF278BSelectRegister((address / 4) & 3, data & 0xff);
		return;

		case 0x400002:
		case 0x400006:
		case 0x40000a:
			BurnYMF278BWriteRegister((address / 4) & 3, data & 0xff);
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall msgogo_main_write_byte(UINT32 address, UINT8 data)
{

	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);

	switch (address)
	{
		case 0x400001:
		case 0x400003:
		case 0x400005:
			BurnYMF278BSelectRegister((address / 2) & 3, data & 0xff);
		return;

		case 0x400007:
		case 0x400009:
		case 0x40000b:
			BurnYMF278BWriteRegister(((address - 0x400007)/2) & 3, data & 0xff);
		return;
	}
}

static UINT16 __fastcall msgogo_main_read_word(UINT32 address)
{
	if ((address & 0xfe0000) == 0x300000) {
		return balcube_dip_read(address);
	}

	switch (address)
	{
		case 0x400000:
			return BurnYMF278BReadStatus();

		case 0x200000:
			return DrvInputs[0];

		case 0x200002:
			return DrvInputs[1];

		case 0x200006:
			return 0;
	}

	bprintf (0, _T("RW %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall msgogo_main_read_byte(UINT32 address)
{
	if ((address & 0xfe0000) == 0x300000) {
		return balcube_dip_read(address);
	}

	switch (address)
	{
		case 0x400001:
			return BurnYMF278BReadStatus();

		case 0x200000:
			return DrvInputs[0] >> 8;

		case 0x200001:
			return DrvInputs[0];

		case 0x200002:
			return DrvInputs[1] >> 8;

		case 0x200003:
			return DrvInputs[1];

		case 0x200006:
			return 0;

		case 0x200007:
			return 0;
	}

	return 0;
}

static void __fastcall balcube_main_write_word(UINT32 address, UINT16 data)
{
//	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);

	switch (address)
	{
		case 0x500002:
		case 0x500004:
		case 0x500006:
		case 0x500008:
			// coin lockout
		return;

		case 0x300000:
		case 0x300004:
		case 0x300008:
			BurnYMF278BSelectRegister((address / 4) & 3, data & 0xff);
		return;

		case 0x300002:
		case 0x300006:
		case 0x30000a:
			BurnYMF278BWriteRegister((address / 4) & 3, data & 0xff);
		return;
	}
}

static void __fastcall balcube_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);

	switch (address)
	{
		case 0x300001:
		case 0x300003:
		case 0x300005:
			BurnYMF278BSelectRegister((address / 4) & 3, data & 0xff);
		return;

		case 0x300007:
		case 0x300009:
		case 0x30000b:
			BurnYMF278BWriteRegister((address / 4) & 3, data & 0xff);
		return;
	}
}

static UINT16 __fastcall balcube_main_read_word(UINT32 address)
{
	if ((address & 0xfe0000) == 0x400000) {
		return balcube_dip_read(address);
	}

	switch (address)
	{
		case 0x300000:
			return BurnYMF278BReadStatus();

		case 0x500000:
			return DrvInputs[0];

		case 0x500002:
			return DrvInputs[1];

		case 0x500006:
			return 0;
	}

	bprintf (0, _T("RW %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall balcube_main_read_byte(UINT32 address)
{
//	bprintf (0, _T("RB %5.5x\n"), address);

	if ((address & 0xfe0000) == 0x400000) {
		return balcube_dip_read(address);
	}

	switch (address)
	{
		case 0x300001:
			return BurnYMF278BReadStatus();

		case 0x500000:
			return DrvInputs[0] >> 8;

		case 0x500001:
			return DrvInputs[0];

		case 0x500002:
			return DrvInputs[1] >> 8;

		case 0x500003:
			return DrvInputs[1];

		case 0x500006:
			return 0;

		case 0x500007:
			return 0;
	}

	return 0;
}

static void __fastcall bangball_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0xd00002:
		case 0xd00004:
		case 0xd00006:
		case 0xd00008:
			// coin lockout
		return;

		case 0xb00000:
		case 0xb00004:
		case 0xb00008:
			BurnYMF278BSelectRegister((address / 4) & 3, data & 0xff);
		return;

		case 0xb00002:
		case 0xb00006:
		case 0xb0000a:
			BurnYMF278BWriteRegister((address / 4) & 3, data & 0xff);
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall bangball_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
	switch (address)
	{
		case 0xb00001:
		case 0xb00003:
		case 0xb00005:
			BurnYMF278BSelectRegister((address / 2) & 3, data & 0xff);
		return;

		case 0xb00007:
		case 0xb00009:
		case 0xb0000b:
			BurnYMF278BWriteRegister(((address - 0xb00007)/2) & 3, data & 0xff);
		return;
	}
}

static UINT16 __fastcall bangball_main_read_word(UINT32 address)
{
	if ((address & 0xfe0000) == 0xc00000) {
		return balcube_dip_read(address);
	}

	switch (address)
	{
		case 0xb00000:
			return BurnYMF278BReadStatus();

		case 0xd00000:
			return DrvInputs[0];

		case 0xd00002:
			return DrvInputs[1];

		case 0xd00006:
			return 0;
	}

	bprintf (0, _T("RW %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall bangball_main_read_byte(UINT32 address)
{
	if ((address & 0xfe0000) == 0xc00000) {
		return balcube_dip_read(address);
	}

	switch (address)
	{
		case 0xb00001:
			return BurnYMF278BReadStatus();

		case 0xd00000:
			return DrvInputs[0] >> 8;

		case 0xd00001:
			return DrvInputs[0];

		case 0xd00002:
			return DrvInputs[1] >> 8;

		case 0xd00003:
			return DrvInputs[1];

		case 0xd00006:
			return 0;

		case 0xd00007:
			return 0;
	}

	return 0;
}

static UINT16 __fastcall batlbubl_main_read_word(UINT32 address)
{
	if ((address & 0xfe0000) == 0x300000) {
		return balcube_dip_read(address);
	}

	switch (address)
	{
		case 0x400000:
			return BurnYMF278BReadStatus();

		case 0x200000:
			return DrvInputs[1];

		case 0x200002:
			return DrvDips[0] + (DrvDips[1] * 256);

		case 0x200004:
			return DrvInputs[0];

		case 0x200006:
			return DrvInputs[2];
	}

	bprintf (0, _T("RW %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall batlbubl_main_read_byte(UINT32 address)
{
	if ((address & 0xfe0000) == 0x300000) {
		return balcube_dip_read(address);
	}

	switch (address)
	{
		case 0x400001:
			return BurnYMF278BReadStatus();

		case 0x200000:
			return DrvInputs[1] >> 8;

		case 0x200001:
			return DrvInputs[1];

		case 0x200002:
			return DrvDips[1];

		case 0x200003:
			return DrvDips[0];

		case 0x200004:
			return DrvInputs[0] >> 8;

		case 0x200005:
			return DrvInputs[0];

		case 0x200006:
			return DrvInputs[2] >> 8;

		case 0x200007:
			return DrvInputs[2];
	}

	return 0;
}


static void __fastcall kokushi_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0xc00000:
			sound_status = data & 0x01;
		return;

		case 0xc00002:
		case 0xc00004:
		case 0xc00006:
		case 0xc00008:
			// coin lockout
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall kokushi_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall kokushi_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xc00000:
			return (DrvInputs[0] & 0xff7f) | ((sound_busy & 1) << 7);

		case 0xc00002:
			return DrvInputs[1];

		case 0xc00004:
			return DrvDips[0] + (DrvDips[1] * 256);
	}

	bprintf (0, _T("RW %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall kokushi_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xc00000:
			return DrvInputs[0] >> 8;

		case 0xc00001:
			return (DrvInputs[0] & 0x7f) | ((sound_busy & 1) << 7);

		case 0xc00002:
			return DrvInputs[1] >> 8;

		case 0xc00003:
			return DrvInputs[1];

		case 0xc00004:
			return DrvDips[1];

		case 0xc00005:
			return DrvDips[0];
	}

	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}


static void z80_bankswitch(INT32 data)
{
	INT32 bank = (data & 0x3) * 0x4000 + 0x10000;

	ZetMapMemory(DrvZ80ROM + bank,	0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall blzntrnd_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			z80_bankswitch(data);
		return;

		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
			BurnYM2610Write(port & 3, data);
		return;
	}
}

static UINT8 __fastcall blzntrnd_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x40:
			return soundlatch;

		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
			return BurnYM2610Read(port & 3);
	}

	return 0;
}

static void pBlzntrnd_roz_callback(INT32 offset, UINT16 *ram, INT32 *code, INT32 *color, INT32 *, INT32 *, INT32 *, INT32 *)
{
	*code = ram[offset] & 0x7fff;
	*color = 0xe00;
}

static void pGstrik2_roz_callback(INT32 offset, UINT16 *ram, INT32 *code, INT32 *color, INT32 *sx, INT32 *sy, INT32 *, INT32 *)
{
	//bprintf(0, _T("%X,"), offset);
	/**sy  = (offset >> 9) & 0x3f;
	*sy += (offset & 1) << 6;
	*sy += (offset & 0x100) >> 1;
	*sy *= 16;

	*sx  = (offset >> 1) & 0x7f;
	*sx *= 16; */
	int val;
	int row = *sy;
	int col = *sx;

	val = (row & 0x3f) * (256 * 2) + (col * 2);

	if (row & 0x40) val += 1;
	if (row & 0x80) val += 256;

	*code = (ram[val] & 0x7fff) >> 2;
	*color = 0xe00;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	SekSetIRQLine(2, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	SekRun(100); // lame hack for overlapped irq's causing the ymf278b timer to die
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)SekTotalCycles() * nSoundRate / 16000000;
}

static void blzntrndFMIRQHandler(INT32, INT32 nStatus)
{
	if (ZetGetActive() == -1) return;

	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 blzntrndSynchroniseStream(INT32 nSoundRate)
{
	if (ZetGetActive() == -1) return 0;

	return (INT64)ZetTotalCycles() * nSoundRate / 8000000;
}

static double blzntrndGetTime()
{
	if (ZetGetActive() == -1) return 0;

	return (double)ZetTotalCycles() / 8000000.0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	if (sound_system == 1) {
		ZetOpen(0);
		ZetReset();
		BurnYM2610Reset();
		ZetClose();
	}

	if (sound_system == 2) {
		upd7810Reset();
		BurnYM2413Reset();
		MSM6295Reset(0);
	}

	if (sound_system == 3) {
		SekOpen(0);
		BurnYMF278BReset();
		SekClose();
	}

	if (sound_system == 4) {
		BurnYM2413Reset();
		MSM6295Reset(0);
	}

	if (sound_system == 5) {
		upd7810Reset();
		BurnYM2151Reset();
		MSM6295Reset(0);
	}

	if (has_zoom) {
		K053936Reset();
	}

	memset (requested_int, 0, 8);

	gfxrom_bank=0;
	soundlatch=0;
	sound_status=0;
	sound_busy=0;
	updportA_data = 0;
	updportB_data = 0;
	irq_enable = 0;
	screen_control = 0;
	flip_screen = 0;
	blit_timer = -1;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x200000;

	DrvUpdROM		= Next;
	DrvZ80ROM		= Next; Next += 0x020000;

	DrvGfxROM		= Next; Next += graphics_length;
	DrvGfxROM0		= Next; Next += graphics_length*2;

	DrvRozROM		= Next; Next += 0x200000;

	MSM6295ROM		= Next;
	DrvYMROMA		= Next; Next += 0x200000;
	DrvYMROMB		= Next; Next += 0x400000;

	konami_palette32	= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM0		= Next; Next += 0x020000;
	DrvVidRAM1		= Next; Next += 0x020000;
	DrvVidRAM2		= Next; Next += 0x020000;
	DrvUnkRAM		= Next; Next += 0x010000;
	Drv68KRAM0		= Next; Next += 0x002000;
	Drv68KRAM1		= Next; Next += 0x010000;
	DrvPalRAM		= Next; Next += 0x002000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvTileRAM		= Next; Next += 0x000800;
	DrvK053936RAM		= Next; Next += 0x040000;
	DrvK053936LRAM		= Next; Next += 0x001000;
	DrvK053936CRAM		= Next; Next += 0x000400;

	DrvUpdRAM		= Next;
	DrvZ80RAM		= Next; Next += 0x002000;

	DrvWindow		= Next; Next += 0x000010;
	DrvScroll		= Next; Next += 0x000010;
	DrvVidRegs		= Next; Next += 0x000020;
	DrvBlitter		= Next; Next += 0x000010;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void expand_4bpp(INT32 len)
{
	for (INT32 i = 0; i < len; i++)
	{
		DrvGfxROM0[i*2+1] = DrvGfxROM[i] >> 4;
		DrvGfxROM0[i*2+0] = DrvGfxROM[i] & 0xf;
	}
}

static INT32 blzntrndInit()
{
	graphics_length = 0x1800000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0100001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0100000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x0000000,  4, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM + 0x0000000,  5, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000002,  6, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000004,  7, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000006,  8, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0800000,  9, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0800002, 10, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0800004, 11, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0800006, 12, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x1000000, 13, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x1000002, 14, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x1000004, 15, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x1000006, 16, 8, LD_GROUP(2))) return 1;

		expand_4bpp(graphics_length);

		if (BurnLoadRom(DrvRozROM + 0x0000000, 17, 1)) return 1;

		if (BurnLoadRom(DrvYMROMA + 0x0000000, 18, 1)) return 1;

		if (BurnLoadRom(DrvYMROMB + 0x0000000, 19, 1)) return 1;
		if (BurnLoadRom(DrvYMROMB + 0x0200000, 20, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x1fffff, MAP_ROM);
	metro_common_map_ram(0x200000, 0xf00000);
	SekMapMemory(DrvK053936RAM,	0x400000, 0x43ffff, MAP_RAM);
	SekMapMemory(DrvK053936LRAM,	0x500000, 0x500fff, MAP_RAM);
	SekMapMemory(DrvK053936CRAM,	0x600000, 0x6003ff, MAP_RAM);
	SekSetWriteWordHandler(0,	blzntrnd_main_write_word);
	SekSetWriteByteHandler(0,	blzntrnd_main_write_byte);
	SekSetReadWordHandler(0,	blzntrnd_main_read_word);
	SekSetReadByteHandler(0,	blzntrnd_main_read_byte);
	SekClose();

	sound_system = 1;
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xe000, 0xffff, MAP_RAM);
	ZetSetOutHandler(blzntrnd_sound_write_port);
	ZetSetInHandler(blzntrnd_sound_read_port);
	ZetClose();

	INT32 RomSndSizeA = 0x80000;
	INT32 RomSndSizeB = 0x400000;
	BurnYM2610Init(8000000, DrvYMROMB, &RomSndSizeB, DrvYMROMA, &RomSndSizeA, &blzntrndFMIRQHandler, blzntrndSynchroniseStream, blzntrndGetTime, 0);
	BurnTimerAttachZet(8000000);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	K053936Init(0, DrvK053936RAM, 0x40000, 256 * 8, 512 * 8, pBlzntrnd_roz_callback);
	K053936SetOffset(0, -69-8, -21);

	m_sprite_xoffs_dx = 0;
	m_sprite_yoffs_dx = 0;
	m_tilemap_scrolldx[0] = m_tilemap_scrolldx[1] = m_tilemap_scrolldx[2] = 0;
	vblank_bit = 0;
	irq_line = 1;
	blitter_bit = 0;
	support_8bpp = 1;
	support_16x16 = 1;
	has_zoom = 1;

	DrvDoReset();

	return 0;
}

static INT32 gstrik2Init()
{
	graphics_length = 0x1000000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0100001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0100000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x0000000,  4, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM + 0x0000000,  5, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000002,  6, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000004,  7, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000006,  8, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0800000,  9, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0800002, 10, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0800004, 11, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0800006, 12, 8, LD_GROUP(2))) return 1;

		expand_4bpp(graphics_length);

		if (BurnLoadRom(DrvRozROM + 0x0000000, 13, 1)) return 1;

		if (BurnLoadRom(DrvYMROMA + 0x0000000, 14, 1)) return 1;

		if (BurnLoadRom(DrvYMROMB + 0x0000000, 15, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x1fffff, MAP_ROM);
	metro_common_map_ram(0x200000, 0xf00000);
	SekMapMemory(DrvK053936RAM,	0x400000, 0x43ffff, MAP_RAM);
	SekMapMemory(DrvK053936LRAM,	0x500000, 0x500fff, MAP_RAM);
	SekMapMemory(DrvK053936CRAM,	0x600000, 0x6003ff, MAP_RAM);
	SekSetWriteWordHandler(0,	blzntrnd_main_write_word);
	SekSetWriteByteHandler(0,	blzntrnd_main_write_byte);
	SekSetReadWordHandler(0,	blzntrnd_main_read_word);
	SekSetReadByteHandler(0,	blzntrnd_main_read_byte);
	SekClose();

	sound_system = 1;
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xe000, 0xffff, MAP_RAM);
	ZetSetOutHandler(blzntrnd_sound_write_port);
	ZetSetInHandler(blzntrnd_sound_read_port);
	ZetClose();

	INT32 RomSndSizeA = 0x200000;
	INT32 RomSndSizeB = 0x200000;
	BurnYM2610Init(8000000, DrvYMROMB, &RomSndSizeB, DrvYMROMA, &RomSndSizeA, &blzntrndFMIRQHandler, blzntrndSynchroniseStream, blzntrndGetTime, 0);
	BurnTimerAttachZet(8000000);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	K053936Init(0, DrvK053936RAM, 0x40000, 128 * 16, 256 * 16, pGstrik2_roz_callback);
	K053936SetOffset(0, -69, -19);

	m_sprite_xoffs_dx = 0;
	m_sprite_yoffs_dx = 0;
	m_tilemap_scrolldx[0] = m_tilemap_scrolldx[1] = m_tilemap_scrolldx[2] = 8;

	vblank_bit = 0;
	irq_line = 1;
	blitter_bit = 0;
	support_8bpp = 1;
	support_16x16 = 1;
	has_zoom = 2;

	DrvDoReset();

	return 0;
}

static INT32 metro_upd7810_callback(INT32 , INT32 )
{
	UINT8 ret = soundlatch & 1;

	soundlatch >>= 1;

	return ret;
}


static void metro_sound_bankswitch(INT32 data)
{
	INT32 bank = ((data >> 4) & 0x7) * 0x4000;

	upd7810MapMemory(DrvUpdROM + bank, 0x4000, 0x7fff, MAP_ROM);
}

static void metro_portB_write(UINT8 data)
{
	if (BIT(updportB_data, 7) && !BIT(data, 7))
	{
		sound_busy = 0;
		updportB_data = data;
		return;
	}

	if (BIT(updportB_data, 5) && !BIT(data, 5))
	{
		if (!BIT(data, 2))
		{
			BurnYM2413Write(BIT(data,1), updportA_data);
		}
		updportB_data = data;
		return;
	}

	if (BIT(updportB_data, 2) && !BIT(data, 2))
	{
		if (!BIT(data, 4))
			MSM6295Command(0, updportA_data);
	}

	updportB_data = data;
}

static void ym2151_portB_write(UINT8 data)
{
	if (BIT(updportB_data, 7) && !BIT(data, 7))   /* clock 1->0 */
	{
		sound_busy = 0;
		updportB_data = data;
		return;
	}

	if (BIT(updportB_data, 6) && !BIT(data, 6))   /* clock 1->0 */
	{
		if (!BIT(data, 2))
		{
			if (BIT(data,1)) {
				BurnYM2151WriteRegister(updportA_data);
			} else {
				BurnYM2151SelectRegister(updportA_data);
			}
			/* write */
		//	downcast<ym2151_device *>(m_ymsnd.target())->write(space, BIT(data, 1), updportA_data);
		}

		if (!BIT(data, 3))
		{
			/* read */
			updportA_data = (BIT(data,1) ? BurnYM2151ReadStatus() : 0xff);
		}

		updportB_data = data;
		return;
	}

	if (BIT(updportB_data, 2) && !BIT(data, 2))   /* clock 1->0 */
	{
		if (!BIT(data, 4))
			MSM6295Command(0, updportA_data);
	}

	if (BIT(updportB_data, 3) && !BIT(data, 3))   /* clock 1->0 */
	{
		if (!BIT(data, 4))
			updportA_data = MSM6295ReadStatus(0);
	}

	updportB_data = data;
}

static UINT8 metro_upd7810_read_port(UINT8 port)
{
//	bprintf (0, _T("IOR: %4.4x\n"), port);
	switch (port)
	{
		case UPD7810_PORTA:
			return updportA_data;
	}

	return 0;
}

static void metro_upd7810_write_port(UINT8 port, UINT8 data)
{
//	//bprintf (0, _T("IOW: %4.4x, %2.2x\n"), port, data);
	switch (port)
	{
		case UPD7810_PORTA:
			updportA_data = data;
		return;

		case UPD7810_PORTB:
			metro_portB_write(data);
		return;

		case UPD7810_PORTC:
			metro_sound_bankswitch(data);
		return;
	}
}

static void ym2151_upd7810_write_port(UINT8 port, UINT8 data)
{
//	bprintf (0, _T("IOW: %4.4x, %2.2x\n"), port, data);
	switch (port)
	{
		case UPD7810_PORTA:
			updportA_data = data;
		return;

		case UPD7810_PORTB:
			ym2151_portB_write(data);
		return;

		case UPD7810_PORTC:
			metro_sound_bankswitch(data);
		return;
	}
}


static void YM2151IrqHandler(INT32 status)
{
//bprintf (0, _T("irq in\n"));
	upd7810SetIRQLine(UPD7810_INTF2, (status) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
//bprintf (0, _T("irq out\n"));
}


static INT32 common_type1_init(INT32 video_type, INT32 gfx_len, INT32 load_roms, void (*pMapCallback)(), void (*pRomCallback)(), INT32 sound_type)
{
	graphics_length = gfx_len;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (load_roms == 1)
	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvUpdROM + 0x0000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x0000000,  3, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000001,  4, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000002,  5, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000003,  6, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000004,  7, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000005,  8, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000006,  9, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000007, 10, 8)) return 1;

		if (BurnLoadRom(DrvYMROMA + 0x0000000, 11, 1)) return 1;
	}
	else if (load_roms == 2)
	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvUpdROM + 0x0000000,  2, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM + 0x0000000,  3, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000002,  4, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000004,  5, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000006,  6, 8, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvYMROMA + 0x0000000,  7, 1)) return 1;
	}
	else if (load_roms == 3)
	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM + 0x0000000,  2, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000002,  3, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000004,  4, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000006,  5, 8, LD_GROUP(2))) return 1;

		if (BurnLoadRom(MSM6295ROM + 0x0000000,  6, 1)) return 1;
	}
	else if (load_roms == 4)
	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM + 0x0000000,  2, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000002,  3, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000004,  4, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000006,  5, 8, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvYMROMB + 0x0000000,  6, 1)) return 1;
		if (BurnLoadRom(DrvYMROMB + 0x0200000,  7, 1)) return 1;
	}

	if (pRomCallback) {
		pRomCallback();
	}

	expand_4bpp(graphics_length);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	if (pMapCallback) {
		pMapCallback();
	}
	SekClose();

	sound_system = sound_type;

	if (sound_system == 2)
	{
		upd7810Init(metro_upd7810_callback);
		upd7810MapMemory(DrvUpdROM,		0x0000, 0x3fff, MAP_ROM);
	//	banked rom 4000-7ff
		upd7810MapMemory(DrvUpdRAM,		0x8000, 0x87ff, MAP_RAM);
		upd7810MapMemory(DrvUpdRAM + 0x800,	0xff00, 0xffff, MAP_RAM);
		upd7810SetReadPortHandler(metro_upd7810_read_port);
		upd7810SetWritePortHandler(metro_upd7810_write_port);

		BurnYM2413Init(3579545);
		BurnYM2413SetAllRoutes(0.90, BURN_SND_ROUTE_BOTH);

		MSM6295Init(0, 1056000 / 132, 1);
		MSM6295SetRoute(0, 0.10, BURN_SND_ROUTE_BOTH);
	}

	if (sound_system == 5)
	{
		upd7810Init(metro_upd7810_callback);
		upd7810MapMemory(DrvUpdROM,		0x0000, 0x3fff, MAP_ROM);
	//	banked rom 4000-7ff
		upd7810MapMemory(DrvUpdRAM,		0x8000, 0x87ff, MAP_RAM);
		upd7810MapMemory(DrvUpdRAM + 0x800,	0xff00, 0xffff, MAP_RAM);
		upd7810SetReadPortHandler(metro_upd7810_read_port);
		upd7810SetWritePortHandler(ym2151_upd7810_write_port);

		BurnYM2151Init(3579545);
		BurnYM2151SetIrqHandler(&YM2151IrqHandler);
		BurnYM2151SetAllRoutes(0.60, BURN_SND_ROUTE_BOTH);

		MSM6295Init(0, 1056000 / 132, 1);
		MSM6295SetRoute(0, 0.10, BURN_SND_ROUTE_BOTH);
	}

	if (sound_system == 3)
	{
		BurnYMF278BInit(0, DrvYMROMB, 0x280000, &DrvFMIRQHandler, DrvSynchroniseStream);
		BurnYMF278BSetAllRoutes(0.100, BURN_SND_ROUTE_BOTH);
		BurnTimerAttachSek(16000000);
	}

	if (sound_system == 4)
	{
		BurnYM2413Init(3579545);
		BurnYM2413SetAllRoutes(0.90, BURN_SND_ROUTE_BOTH);

		MSM6295Init(0, 1056000 / 132, 1);
		MSM6295SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	}

	// video configuration
	m_sprite_xoffs_dx = 0;
	m_sprite_yoffs_dx = 0;
	m_tilemap_scrolldx[0] = 0;
	m_tilemap_scrolldx[1] = 0;
	m_tilemap_scrolldx[2] = 0;
	support_8bpp = (video_type > 4100) ? 1 : 0;
	support_16x16 = (video_type >= 4100) ? 1 : 0;
	has_zoom = 0;

	// irq configuration
	vblank_bit = 0;
	irq_line = 2;
	blitter_bit = 2;

	GenericTilesInit();
	KonamiAllocateBitmaps();

	DrvDoReset();

	return 0;
}

static void skyalertMapCallback()
{
	metro_common_map_ram(0x800000, 0xc00000);
	SekSetWriteWordHandler(0,	skyalert_main_write_word);
	SekSetWriteByteHandler(0,	skyalert_main_write_byte);
	SekSetReadWordHandler(0,	skyalert_main_read_word);
	SekSetReadByteHandler(0,	skyalert_main_read_byte);
}

static INT32 skyalertInit()
{
	return common_type1_init(4100, 0x200000, 1, skyalertMapCallback, NULL, 2/*udp*/);
}

static void pangpomsMapCallback()
{
	metro_common_map_ram(0x400000, 0xc00000);
	SekSetWriteWordHandler(0,	pangpoms_main_write_word);
	SekSetWriteByteHandler(0,	pangpoms_main_write_byte);
	SekSetReadWordHandler(0,	pangpoms_main_read_word);
	SekSetReadByteHandler(0,	pangpoms_main_read_byte);
}

static INT32 pangpomsInit()
{
	return common_type1_init(4100, 0x100000, 1, pangpomsMapCallback, NULL, 2/*udp*/);
}

static void poittoMapCallback()
{
	metro_common_map_ram(0xc00000, 0x400000);
	SekSetWriteWordHandler(0,	poitto_main_write_word);
	SekSetWriteByteHandler(0,	poitto_main_write_byte);
	SekSetReadWordHandler(0,	poitto_main_read_word);
	SekSetReadByteHandler(0,	poitto_main_read_byte);
}

static INT32 poittoInit()
{
	return common_type1_init(4100, 0x200000, 2, poittoMapCallback, NULL, 2/*udp*/);
}

static void lastfortMapCallback()
{
	metro_common_map_ram(0x800000, 0x400000);
	SekSetWriteWordHandler(0,	lastfort_main_write_word);
	SekSetWriteByteHandler(0,	lastfort_main_write_byte);
	SekSetReadWordHandler(0,	lastfort_main_read_word);
	SekSetReadByteHandler(0,	lastfort_main_read_byte);
}

static INT32 lastfortInit()
{
	main_cpu_cycles = 12000000 / 58;

	return common_type1_init(4100, 0x200000, 1, lastfortMapCallback, NULL, 2/*udp*/);
}

static void lastforgMapCallback()
{
	metro_common_map_ram(0x880000, 0xc00000);
	SekSetWriteWordHandler(0,	lastforg_main_write_word);
	SekSetWriteByteHandler(0,	lastforg_main_write_byte);
	SekSetReadWordHandler(0,	lastforg_main_read_word);
	SekSetReadByteHandler(0,	lastforg_main_read_byte);
}

static INT32 lastforgInit()
{
	main_cpu_cycles = 12000000 / 58;

	return common_type1_init(4100, 0x200000, 2, lastforgMapCallback, NULL, 2/*udp*/);
}

static void dharmaMapCallback()
{
	metro_common_map_ram(0x800000, 0x400000);
	SekSetWriteWordHandler(0,	dharma_main_write_word);
	SekSetWriteByteHandler(0,	dharma_main_write_byte);
	SekSetReadWordHandler(0,	dharma_main_read_word);
	SekSetReadByteHandler(0,	dharma_main_read_byte);
}

static void dharmaRomCallback()
{
	for (INT32 i = 0; i < 0x200000; i += 4)
	{
		DrvGfxROM[i + 1] = BITSWAP08(DrvGfxROM[i + 1], 7,3,2,4, 5,6,1,0);
		DrvGfxROM[i + 3] = BITSWAP08(DrvGfxROM[i + 3], 7,2,5,4, 3,6,1,0);
	}
}

static INT32 dharmaInit()
{
	return common_type1_init(4200, 0x200000, 2, dharmaMapCallback, dharmaRomCallback, 2/*udp*/);
}

static INT32 dharmajInit()
{
	return common_type1_init(4200, 0x200000, 2, dharmaMapCallback, NULL, 2/*udp*/);
}

static void pururunMapCallback()
{
	metro_common_map_ram(0xc00000, 0x800000);
	SekSetWriteWordHandler(0,	pururun_main_write_word);
	SekSetWriteByteHandler(0,	pururun_main_write_byte);
	SekSetReadWordHandler(0,	pururun_main_read_word);
	SekSetReadByteHandler(0,	pururun_main_read_byte);
}

static INT32 pururunInit()
{
	return common_type1_init(4200, 0x200000, 2, pururunMapCallback, NULL, 5/*udp + ym2151*/);
}

static void karatourMapCallback()
{
	metro_common_map_ram(0x800000, 0xf00000);
	SekSetWriteWordHandler(0,	karatour_main_write_word);
	SekSetWriteByteHandler(0,	karatour_main_write_byte);
	SekSetReadWordHandler(0,	karatour_main_read_word);
	SekSetReadByteHandler(0,	karatour_main_read_byte);
}

static INT32 karatourInit()
{
	return common_type1_init(4200, 0x400000, 2, karatourMapCallback, NULL, 2/*udp*/);
}

static void kokushiMapCallback()
{
	metro_common_map_ram(0x800000, 0x700000);
	SekSetWriteWordHandler(0,	kokushi_main_write_word);
	SekSetWriteByteHandler(0,	kokushi_main_write_byte);
	SekSetReadWordHandler(0,	kokushi_main_read_word);
	SekSetReadByteHandler(0,	kokushi_main_read_byte);
}

static INT32 kokushiInit()
{
	INT32 nRet = common_type1_init(4200, 0x200000, 2, kokushiMapCallback, NULL, 2/*udp*/);

	for (INT32 i = 0; i < 0x20000; i++) {
		DrvVidRAM0[i] = rand();
		DrvVidRAM1[i] = rand();
		DrvVidRAM2[i] = rand();
	}

	return nRet;
}

static void daitoridMapCallback()
{
	metro_common_map_ram(0x400000, 0x800000);
	SekSetWriteWordHandler(0,	daitorid_main_write_word);
	SekSetWriteByteHandler(0,	daitorid_main_write_byte);
	SekSetReadWordHandler(0,	daitorid_main_read_word);
	SekSetReadByteHandler(0,	daitorid_main_read_byte);
}

static INT32 daitoridInit()
{
	main_cpu_cycles = 16000000 / 58;

	INT32 nRet = common_type1_init(4200, 0x200000, 2, daitoridMapCallback, NULL, 5/*udp + ym2151*/);

	m_tilemap_scrolldx[0] = -2;
	m_tilemap_scrolldx[1] = -2;
	m_tilemap_scrolldx[2] = -2;

	return nRet;
}

static void toride2gMapCallback()
{
	metro_common_map_ram(0xc00000, 0x400000);
	SekSetWriteWordHandler(0,	poitto_main_write_word);
	SekSetWriteByteHandler(0,	poitto_main_write_byte);
	SekSetReadWordHandler(0,	poitto_main_read_word);
	SekSetReadByteHandler(0,	poitto_main_read_byte);
}

static INT32 toride2gInit()
{
	return common_type1_init(4200, 0x200000, 2, toride2gMapCallback, NULL, 2/*udp*/);
}

static void msgogoMapCallback()
{
	metro_common_map_ram(0x100000, 0xf00000);
	SekSetWriteWordHandler(0,	msgogo_main_write_word);
	SekSetWriteByteHandler(0,	msgogo_main_write_byte);
	SekSetReadWordHandler(0,	msgogo_main_read_word);
	SekSetReadByteHandler(0,	msgogo_main_read_byte);
}

static void balcubeRomCallback()
{
	for (unsigned i = 0; i < graphics_length; i+=2)
	{
		DrvGfxROM[i]  = BITSWAP08(DrvGfxROM[i],0,1,2,3,4,5,6,7);
	}
}

static INT32 msgogoInit()
{
	main_cpu_cycles = 16000000 / 60;

	INT32 nRet = common_type1_init(4200, 0x200000, 4, msgogoMapCallback, balcubeRomCallback, 3/*ymf278b*/);

	irq_line = 1;
	m_tilemap_scrolldx[0] = -2;
	m_tilemap_scrolldx[1] = -2;
	m_tilemap_scrolldx[2] = -2;
	ymf278bint = 1;

	return nRet;
}

static INT32 daitoridaInit()
{
	INT32 rc = msgogoInit();

	ymf278bint = 8;

	return rc;
}

static void balcubeMapCallback()
{
	metro_common_map_ram(0x600000, 0xf00000);
	SekSetWriteWordHandler(0,	balcube_main_write_word);
	SekSetWriteByteHandler(0,	balcube_main_write_byte);
	SekSetReadWordHandler(0,	balcube_main_read_word);
	SekSetReadByteHandler(0,	balcube_main_read_byte);
}

static INT32 balcubeInit()
{
	main_cpu_cycles = 16000000 / 60;

	INT32 nRet = common_type1_init(4200, 0x200000, 4, balcubeMapCallback, balcubeRomCallback, 3/*ymf278b*/);

	irq_line = 1;
	m_tilemap_scrolldx[0] = -2;
	m_tilemap_scrolldx[1] = -2;
	m_tilemap_scrolldx[2] = -2;
	ymf278bint = 8;

	return nRet;
}

static void bangballMapCallback()
{
	metro_common_map_ram(0xe00000, 0xf00000);
	SekSetWriteWordHandler(0,	bangball_main_write_word);
	SekSetWriteByteHandler(0,	bangball_main_write_byte);
	SekSetReadWordHandler(0,	bangball_main_read_word);
	SekSetReadByteHandler(0,	bangball_main_read_byte);
}

static INT32 bangballInit()
{
	main_cpu_cycles = 16000000 / 60;

	INT32 nRet = common_type1_init(4200, 0x400000, 4, bangballMapCallback, balcubeRomCallback, 3/*ymf278b*/);

	irq_line = 1;
	m_tilemap_scrolldx[0] = -2;
	m_tilemap_scrolldx[1] = -2;
	m_tilemap_scrolldx[2] = -2;
	ymf278bint = 1;

	return nRet;
}

static void batlbublMapCallback()
{
	metro_common_map_ram(0x100000, 0xf00000);
	SekSetWriteWordHandler(0,	msgogo_main_write_word);
	SekSetWriteByteHandler(0,	msgogo_main_write_byte);
	SekSetReadWordHandler(0,	batlbubl_main_read_word);
	SekSetReadByteHandler(0,	batlbubl_main_read_byte);
}

static void batlbublRomCallback()
{
	BurnLoadRom(Drv68KROM + 0x000000, 0, 1);
	BurnLoadRom(Drv68KROM + 0x080000, 1, 1);

	balcubeRomCallback();
}

static INT32 batlbublInit()
{
	main_cpu_cycles = 16000000 / 60;

	INT32 nRet = common_type1_init(4200, 0x800000, 4, batlbublMapCallback, batlbublRomCallback, 3/*ymf278b*/);

	irq_line = 1;
	m_tilemap_scrolldx[0] = -2;
	m_tilemap_scrolldx[1] = -2;
	m_tilemap_scrolldx[2] = -2;
	ymf278bint = 1;

	return nRet;
}

static INT32 DrvExit()
{
	if (sound_system == 1) {
		BurnYM2610Exit();
		ZetExit();
	}

	if (sound_system == 2) {
		upd7810Exit();
		MSM6295Exit(0);
		MSM6295ROM = NULL;
		BurnYM2413Exit();
	}

	if (sound_system == 3) {
		BurnYMF278BExit();
	}

	if (sound_system == 4) {
		MSM6295Exit(0);
		MSM6295ROM = NULL;
		BurnYM2413Exit();
	}

	if (sound_system == 5) {
		upd7810Exit();
		MSM6295Exit(0);
		MSM6295ROM = NULL;
		BurnYM2151Exit();
	}

	KonamiICExit();
	GenericTilesExit();

	SekExit();
	
	BurnFree(AllMem);

	sound_system = 0;
	has_zoom = 0;
	main_cpu_cycles = 12000000 / 60;
	m_sprite_yoffs_dx = 0;
	m_sprite_yoffs_dx = 0;
	ymf278bint = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x2000 / 2; i++)
	{
		INT32 r = (p[i] >>  6) & 0x1f;
		INT32 g = (p[i] >> 11) & 0x1f;
		INT32 b = (p[i] >>  1) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		konami_palette32[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	UINT16 *m_videoregs = (UINT16*)DrvVidRegs;
	UINT16 *m_spriteram = (UINT16*)DrvSprRAM;

	INT32 m_sprite_xoffs = m_videoregs[0x06 / 2] - nScreenWidth  / 2 + m_sprite_xoffs_dx;
	INT32 m_sprite_yoffs = m_videoregs[0x04 / 2] - nScreenHeight / 2 + m_sprite_yoffs_dx;

	UINT8 *base_gfx4 = DrvGfxROM0;
	UINT8 *base_gfx8 = DrvGfxROM;
	UINT32 gfx_size = graphics_length;

	int max_x = nScreenWidth;
	int max_y = nScreenHeight;

	int max_sprites = 0x1000 / 8;
	int sprites     = m_videoregs[0x00/2] % max_sprites;

	int color_start = (m_videoregs[0x08/2] & 0x0f) << 4;

	int i, j, pri;
	static const int primask[4] = { 0x0000, 0xff00, 0xff00 | 0xf0f0, 0xff00 | 0xf0f0 | 0xcccc };

	UINT16 *src;
	int inc;

	if (sprites == 0)
		return;

	for (i = 0; i < 0x20; i++)
	{
		if (!(m_videoregs[0x02/2] & 0x8000))
		{
			src = m_spriteram + (sprites - 1) * (8 / 2);
			inc = -(8 / 2);
		} else {
			src = m_spriteram;
			inc = (8 / 2);
		}

		for (j = 0; j < sprites; j++)
		{
			int x, y, attr, code, color, flipx, flipy, zoom, curr_pri, width, height;

			static const int zoomtable[0x40] = {
				0xAAC,0x800,0x668,0x554,0x494,0x400,0x390,0x334,0x2E8,0x2AC,0x278,0x248,0x224,0x200,0x1E0,0x1C8,
				0x1B0,0x198,0x188,0x174,0x164,0x154,0x148,0x13C,0x130,0x124,0x11C,0x110,0x108,0x100,0x0F8,0x0F0,
				0x0EC,0x0E4,0x0DC,0x0D8,0x0D4,0x0CC,0x0C8,0x0C4,0x0C0,0x0BC,0x0B8,0x0B4,0x0B0,0x0AC,0x0A8,0x0A4,
				0x0A0,0x09C,0x098,0x094,0x090,0x08C,0x088,0x080,0x078,0x070,0x068,0x060,0x058,0x050,0x048,0x040
			};

			x = src[0];
			curr_pri = (x & 0xf800) >> 11;

			if ((curr_pri == 0x1f) || (curr_pri != i))
			{
				src += inc;
				continue;
			}

			pri = (m_videoregs[0x02/2] & 0x0300) >> 8;

			if (!(m_videoregs[0x02/2] & 0x8000))
			{
				if (curr_pri > (m_videoregs[0x02/2] & 0x1f))
					pri = (m_videoregs[0x02/2] & 0x0c00) >> 10;
			}

			y     = src[1];
			attr  = src[2];
			code  = src[3];

			flipx =  attr & 0x8000;
			flipy =  attr & 0x4000;
			color = (attr & 0xf0) >> 4;

			zoom = zoomtable[(y & 0xfc00) >> 10] << (16 - 8);

			x = (x & 0x07ff) - m_sprite_xoffs;
			y = (y & 0x03ff) - m_sprite_yoffs;

			width  = (((attr >> 11) & 0x7) + 1) * 8;
			height = (((attr >>  8) & 0x7) + 1) * 8;

			UINT32 gfxstart = (8 * 8 * 4 / 8) * (((attr & 0x000f) << 16) + code);

			if (flip_screen)
			{
				flipx = !flipx;     x = max_x - x - width;
				flipy = !flipy;     y = max_y - y - height;
			}

			if (support_8bpp && color == 0xf)  /* 8bpp */
			{
				if ((gfxstart + width * height - 1) >= gfx_size)
					continue;

				konami_draw_16x16_priozoom_tile(base_gfx8 + gfxstart, 0, 8, color_start >> 4, 255, x, y, flipx, flipy, width, height, zoom, zoom, primask[pri]);
			}
			else
			{
				if ((gfxstart + width / 2 * height - 1) >= gfx_size*2)
					continue;

				konami_draw_16x16_priozoom_tile(base_gfx4 + 2 * gfxstart, 0, 4, color + color_start, 15, x, y, flipx, flipy, width, height, zoom, zoom, primask[pri]);
			}

			src += inc;
		}
	}
}

inline UINT8 get_tile_pix(UINT16 code, UINT8 x, UINT8 y, INT32 big, UINT16 *pix)
{
	UINT16 *tiletable = (UINT16*)DrvTileRAM;

	INT32 table_index = (code & 0x1ff0) >> 3;
	UINT32 tile = (tiletable[table_index + 0] << 16) + tiletable[table_index + 1];

	if (code & 0x8000)
	{
		*pix = code & 0x0fff;

		if ((*pix & 0xf) != 0xf)
			return 1;
		else
			return 0;
	}
	else if (((tile & 0x00f00000) == 0x00f00000) && (support_8bpp))
	{
		UINT32 tile2 = big ? ((((tile & 0xfffff) + (8*(code & 0xf))))) : ((((tile & 0xfffff) + (2*(code & 0xf)))));

		tile2 *= (big) ? 0x80 : 0x20; // big is a guess

		if (tile2 >= graphics_length) {
			return 0;
		}

		const UINT8 *data = DrvGfxROM + tile2;

		switch (code & 0x6000)
		{
			case 0x0000: *pix = data[(y              * (big?16:8)) + x];              break;
			case 0x2000: *pix = data[(((big?15:7)-y) * (big?16:8)) + x];              break;
			case 0x4000: *pix = data[(y              * (big?16:8)) + ((big?15:7)-x)]; break;
			case 0x6000: *pix = data[(((big?15:7)-y) * (big?16:8)) + ((big?15:7)-x)]; break;
		}

		if (*pix == 0xff) {
			return 0;
		}

		*pix |= (tile & 0x0f000000) >> 16;

		return 1;
	}
	else
	{
		UINT32 tile2 = big ? ((((tile & 0xfffff) + 4*(code & 0xf))) * 0x100) : ((((tile & 0xfffff) + (code & 0xf))) * 0x40);

		if (tile2 >= (graphics_length*2)) {
			return 0;
		}

		const UINT8 *data = DrvGfxROM0 + tile2;

		switch (code & 0x6000)
		{
			case 0x0000: *pix = data[(y              * (big?16:8)) + x];              break;
			case 0x2000: *pix = data[(((big?15:7)-y) * (big?16:8)) + x];              break;
			case 0x4000: *pix = data[(y              * (big?16:8)) + ((big?15:7)-x)]; break;
			case 0x6000: *pix = data[(((big?15:7)-y) * (big?16:8)) + ((big?15:7)-x)]; break;
		}

		if (*pix == 0xf) {
			return 0;
		}

		*pix |= (tile & 0x0ff00000) >> 16;

		return 1;
	}
}

static void draw_tilemap(UINT32 ,UINT32 pcode,int sx, int sy, int wx, int wy, int big, UINT16 *tilemapram, int layer)
{
	UINT8 * priority_bitmap = konami_priority_bitmap;

	int width  = big ? 4096 : 2048;
	int height = big ? 4096 : 2048;

	int scrwidth  = nScreenWidth;
	int scrheight = nScreenHeight;

	int windowwidth  = width >> 2;
	int windowheight = height >> 3;

	sx += m_tilemap_scrolldx[layer] * (flip_screen ? 1 : -1);

	for (INT32 y = 0; y < scrheight; y++)
	{
		int scrolly = (sy+y-wy)&(windowheight-1);
		int x;
		UINT32 *dst;
		UINT8 *priority_baseaddr;
		int srcline = (wy+scrolly)&(height-1);
		int srctilerow = srcline >> (big ? 4 : 3);

		if (!flip_screen)
		{
			dst = konami_bitmap32 + (y * nScreenWidth);
			priority_baseaddr = priority_bitmap + (y * nScreenWidth);

			for (x = 0; x < scrwidth; x++)
			{
				int scrollx = (sx+x-wx)&(windowwidth-1);
				int srccol = (wx+scrollx)&(width-1);
				int srctilecol = srccol >> (big ? 4 : 3);
				int tileoffs = srctilecol + srctilerow * 0x100;

				UINT16 dat = 0;

				UINT16 tile = tilemapram[tileoffs];
				UINT8 draw = get_tile_pix(tile, big ? (srccol&0xf) : (srccol&0x7), big ? (srcline&0xf) : (srcline&0x7), big, &dat);

				if (draw)
				{
					dst[x] = konami_palette32[dat];
					priority_baseaddr[x] = (priority_baseaddr[x] & (pcode >> 8)) | pcode;
				}
			}
		}
		else // flipped case
		{
			dst = konami_bitmap32 + ((scrheight-y-1) * nScreenWidth);
			priority_baseaddr = priority_bitmap + ((scrheight-y-1) * nScreenWidth);

			for (x = 0; x < scrwidth; x++)
			{
				int scrollx = (sx+x-wx)&(windowwidth-1);
				int srccol = (wx+scrollx)&(width-1);
				int srctilecol = srccol >> (big ? 4 : 3);
				int tileoffs = srctilecol + srctilerow * 0x100;

				UINT16 dat = 0;

				UINT16 tile = tilemapram[tileoffs];
				UINT8 draw = get_tile_pix(tile, big ? (srccol&0xf) : (srccol&0x7), big ? (srcline&0xf) : (srcline&0x7), big, &dat);

				if (draw)
				{
					dst[scrwidth-x-1] = konami_palette32[dat];
					priority_baseaddr[scrwidth-x-1] = (priority_baseaddr[scrwidth-x-1] & (pcode >> 8)) | pcode;
				}
			}
		}
	}
}

static void draw_layers(int pri)
{
	UINT16 *m_videoregs = (UINT16*)DrvVidRegs;
	UINT16 *m_scroll = (UINT16*)DrvScroll;
	UINT16 *m_window = (UINT16*)DrvWindow;
	UINT16 layers_pri = m_videoregs[0x10 / 2];
	int layer;

	for (layer = 2; layer >= 0; layer--)
	{
		if (pri == ((layers_pri >> (layer * 2)) & 3))
		{
			UINT16 sy = m_scroll[layer * 2 + 0];
			UINT16 sx = m_scroll[layer * 2 + 1];
			UINT16 wy = m_window[layer * 2 + 0];
			UINT16 wx = m_window[layer * 2 + 1];

			UINT16 *tilemapram = NULL;

			switch (layer)
			{
				case 0: tilemapram = (UINT16*)DrvVidRAM0;   break;
				case 1: tilemapram = (UINT16*)DrvVidRAM1;   break;
				case 2: tilemapram = (UINT16*)DrvVidRAM2;   break;
			}

			int big = support_16x16 && (screen_control & (0x0020 << layer));

			draw_tilemap(0, 1 << (3 - pri), sx, sy, wx, wy, big, tilemapram, layer);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc)
	{
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	UINT16 *videoregs = (UINT16*)DrvVidRegs;

	KonamiClearBitmaps(konami_palette32[videoregs[0x12/2] & 0x0fff]);

	if ((screen_control & 2) == 0)
	{
		flip_screen = screen_control & 1;

		if (has_zoom == 1) K053936PredrawTiles3(0, DrvRozROM,  8,  8, 0);
		if (has_zoom == 2) K053936PredrawTiles3(0, DrvRozROM, 16, 16, 0);

		if (has_zoom && (nBurnLayer & 1)) K053936Draw(0, (UINT16*)DrvK053936CRAM, (UINT16*)DrvK053936LRAM, (1 << 8) | 0);

		{
			for (INT32 pri = 3; pri >= 0; pri--)
			{
				if (nBurnLayer & 2) draw_layers(pri);
			}
		}

		if (nBurnLayer & 4) draw_sprites();
	}

	KonamiBlendCopy(konami_palette32);

	return 0;
}

static INT32 Z80Frame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 3 * sizeof(short));

		for (INT32 i = 0; i < 16; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (DrvJoy3[0]) DrvInputs[2] ^= 0x02;
	}

	SekNewFrame();
	ZetNewFrame();

	INT32 nInterleave = 240;
	INT32 nCyclesTotal[2] = { 16000000 / 58, 8000000 / 58 };
	
	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekRun(nCyclesTotal[0] / nInterleave);

		if ((i % 28) == 26) {
			requested_int[4] = 1;
			update_irq_state();
		}

		if (i == 236) {
			requested_int[vblank_bit] = 1;
			requested_int[5] = 1;
			update_irq_state();
			SekRun(500);
			requested_int[5] = 0;
		}

		BurnTimerUpdate((nCyclesTotal[1] * (i + 1)) / nInterleave);
	}

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 NoZ80Frame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 4 * sizeof(short));

		for (INT32 i = 0; i < 16; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	SekNewFrame();
	upd7810NewFrame();

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 240;
	UINT32 nCyclesTotal[2] = { main_cpu_cycles, main_cpu_cycles };
	
	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekRun((nCyclesTotal[0] / nInterleave));

		if (sound_system == 2 || sound_system == 5) {
			upd7810Run((nCyclesTotal[1] / nInterleave));
		}

		if (i == 0 && requested_int[5] == 1) {
			requested_int[5] = 0;
		}

		if ((i % 28) == 0) {
			requested_int[4] = 1;
			update_irq_state();
		}

		if (i == 236)
		{
			requested_int[vblank_bit] = 1;
			requested_int[5] = 1;
			update_irq_state();
		}

		if (blit_timer >= 1) {
			if (blit_timer == 0) {
				update_irq_state();
			}
			blit_timer--;
		}

		if (pBurnSoundOut && (sound_system == 2 || sound_system == 4)) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2413Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
		if (pBurnSoundOut && (sound_system == 5)) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut && (sound_system == 2 || sound_system == 4)) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2413Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnSoundOut && (sound_system == 5)) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
		}
	}

	SekClose();
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 YMF278bFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 4 * sizeof(short));

		for (INT32 i = 0; i < 16; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	SekNewFrame();

	INT32 nInterleave = 240;
	UINT32 nCyclesTotal[1] = { main_cpu_cycles };
	
	SekOpen(0);

	INT32 sound_int_mod = (ymf278bint == 1) ? sound_int_mod = nInterleave : 28;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		BurnTimerUpdate((i + 1) * (nCyclesTotal[0] / nInterleave));

		if ((i % sound_int_mod) == 0) {
			requested_int[4] = 1;
			update_irq_state();
		}

		if (i == 237) {
			requested_int[vblank_bit] = 1;
			update_irq_state();
		}

		if (blit_timer >= 0) {
			if (blit_timer == 0) {
				update_irq_state();
			}
			blit_timer--;
		}
	}

	BurnTimerEndFrame(nCyclesTotal[0]);

	if (pBurnSoundOut) {
		BurnYMF278BUpdate(nBurnSoundLen);
	}

	SekClose();
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin =  0x029702;
	}

	if (nAction & ACB_MEMORY_ROM)
	{	
		ba.Data		= Drv68KROM;
		ba.nLen		= 0x0200000;
		ba.nAddress	= 0;
		ba.szName	= "68K ROM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_MEMORY_RAM)
	{	
		ba.Data		= DrvVidRAM0;
		ba.nLen		= 0x0020000;
		ba.nAddress	= 0x200000;
		ba.szName	= "Bg RAM 0";
		BurnAcb(&ba);

		ba.Data		= DrvVidRAM1;
		ba.nLen		= 0x0020000;
		ba.nAddress	= 0x220000;
		ba.szName	= "Bg RAM 1";
		BurnAcb(&ba);

		ba.Data		= DrvVidRAM2;
		ba.nLen		= 0x0020000;
		ba.nAddress	= 0x240000;
		ba.szName	= "Bg RAM 2";
		BurnAcb(&ba);

		ba.Data		= DrvUnkRAM;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0x260000;
		ba.szName	= "Unk RAM";
		BurnAcb(&ba);

		ba.Data		= Drv68KRAM0;
		ba.nLen		= 0x0002000;
		ba.nAddress	= 0x270000;
		ba.szName	= "68K RAM 0";
		BurnAcb(&ba);

		ba.Data		= Drv68KRAM1;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0x300000;
		ba.szName	= "68K RAM 1";
		BurnAcb(&ba);

		ba.Data		= DrvPalRAM;
		ba.nLen		= 0x0002000;
		ba.nAddress	= 0x272000;
		ba.szName	= "Palette RAM";
		BurnAcb(&ba);

		ba.Data		= DrvSprRAM;
		ba.nLen		= 0x0001000;
		ba.nAddress	= 0x274000;
		ba.szName	= "Sprite RAM";
		BurnAcb(&ba);

		ba.Data		= DrvTileRAM;
		ba.nLen		= 0x0000800;
		ba.nAddress	= 0x278000;
		ba.szName	= "Tile RAM";
		BurnAcb(&ba);

		ba.Data		= DrvZ80RAM;
		ba.nLen		= 0x0002000;
		ba.nAddress	= 0xff000000;
		ba.szName	= "z80 / Upd RAM";
		BurnAcb(&ba);

		ba.Data		= DrvWindow;
		ba.nLen		= 0x000010;
		ba.nAddress	= 0x278800;
		ba.szName	= "Window Regs";
		BurnAcb(&ba);

		ba.Data		= DrvScroll;
		ba.nLen		= 0x000010;
		ba.nAddress	= 0x278810;
		ba.szName	= "Scroll Regs";
		BurnAcb(&ba);

		ba.Data		= DrvVidRegs;
		ba.nLen		= 0x000020;
		ba.nAddress	= 0x278820;
		ba.szName	= "Video Regs";
		BurnAcb(&ba);

		ba.Data		= DrvBlitter;
		ba.nLen		= 0x000010;
		ba.nAddress	= 0x278840;
		ba.szName	= "Blitter Regs";
		BurnAcb(&ba);

		if (has_zoom)
		{
			ba.Data		= DrvK053936RAM;
			ba.nLen		= 0x0040000;
			ba.nAddress	= 0x400000;
			ba.szName	= "K053936 RAM";
			BurnAcb(&ba);

			ba.Data		= DrvK053936LRAM;
			ba.nLen		= 0x0001000;
			ba.nAddress	= 0x500000;
			ba.szName	= "K053936 Line RAM";
			BurnAcb(&ba);

			ba.Data		= DrvK053936CRAM;
			ba.nLen		= 0x0000400;
			ba.nAddress	= 0x600000;
			ba.szName	= "K053936 Ctrl RAM";
			BurnAcb(&ba);
		}
	}

	if (nAction & ACB_VOLATILE)
	{
		SekScan(nAction);

		if (sound_system == 1)
		{
			ZetScan(nAction);

			ZetOpen(0);
			BurnYM2610Scan(nAction, pnMin);
			ZetClose();
		}

		if (sound_system == 2)
		{
			upd7810Scan(nAction);
			BurnYM2413Scan(nAction);
			MSM6295Scan(0, nAction);
		}

		if (sound_system == 3)
		{
			BurnYMF278BScan(nAction, pnMin);
		}

		if (sound_system == 4)
		{
			BurnYM2413Scan(nAction);
			MSM6295Scan(0, nAction);
		}

		if (sound_system == 5)
		{
			upd7810Scan(nAction);
			BurnYM2151Scan(nAction);
			MSM6295Scan(0, nAction);
		}

		KonamiICScan(nAction);

		SCAN_VAR(gfxrom_bank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(irq_enable);
		SCAN_VAR(screen_control);
		SCAN_VAR(requested_int);
		SCAN_VAR(flip_screen);
		SCAN_VAR(irq_levels);
		SCAN_VAR(blit_timer);

		SCAN_VAR(sound_status);
		SCAN_VAR(sound_busy);
		SCAN_VAR(updportA_data);
		SCAN_VAR(updportB_data);
	}

 	return 0;
}


// Blazing Tornado

static struct BurnRomInfo blzntrndRomDesc[] = {
	{ "1k.bin",			0x080000, 0xb007893b, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "2k.bin",			0x080000, 0xec173252, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3k.bin",			0x080000, 0x1e230ba2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4k.bin",			0x080000, 0xe98ca99e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rom5.bin",			0x020000, 0x7e90b774, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rom142.bin",			0x200000, 0xa7200598, 3 | BRF_GRA },           //  5 Graphics
	{ "rom186.bin",			0x200000, 0x6ee28ea7, 3 | BRF_GRA },           //  6
	{ "rom131.bin",			0x200000, 0xc77e75d3, 3 | BRF_GRA },           //  7
	{ "rom175.bin",			0x200000, 0x04a84f9b, 3 | BRF_GRA },           //  8
	{ "rom242.bin",			0x200000, 0x1182463f, 3 | BRF_GRA },           //  9
	{ "rom286.bin",			0x200000, 0x384424fc, 3 | BRF_GRA },           // 10
	{ "rom231.bin",			0x200000, 0xf0812362, 3 | BRF_GRA },           // 11
	{ "rom275.bin",			0x200000, 0x184cb129, 3 | BRF_GRA },           // 12
	{ "rom342.bin",			0x200000, 0xe527fee5, 3 | BRF_GRA },           // 13
	{ "rom386.bin",			0x200000, 0xd10b1401, 3 | BRF_GRA },           // 14
	{ "rom331.bin",			0x200000, 0x4d909c28, 3 | BRF_GRA },           // 15
	{ "rom375.bin",			0x200000, 0x6eb4f97c, 3 | BRF_GRA },           // 16

	{ "rom9.bin",			0x200000, 0x37ca3570, 4 | BRF_GRA },           // 17 Roz tiles

	{ "rom8.bin",			0x080000, 0x565a4086, 5 | BRF_SND },           // 18 YM2610 Delta T Samples

	{ "rom6.bin",			0x200000, 0x8b8819fc, 6 | BRF_SND },           // 19 YM2610 Samples
	{ "rom7.bin",			0x200000, 0x0089a52b, 6 | BRF_SND },           // 20
};

STD_ROM_PICK(blzntrnd)
STD_ROM_FN(blzntrnd)

struct BurnDriver BurnDrvBlzntrnd = {
	"blzntrnd", NULL, NULL, NULL, "1994",
	"Blazing Tornado\0", NULL, "Human Amusement", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, blzntrndRomInfo, blzntrndRomName, NULL, NULL, BlzntrndInputInfo, BlzntrndDIPInfo,
	blzntrndInit, DrvExit, Z80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	304, 224, 4, 3
};


// Grand Striker 2 (Europe and Oceania)

static struct BurnRomInfo gstrik2RomDesc[] = {
	{ "hum_003_g2f.rom1.u107",	0x080000, 0x2712d9ca, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "hum_003_g2f.rom2.u108",	0x080000, 0x86785c64, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.109",			0x080000, 0xead86919, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.110",			0x080000, 0xe0b026e3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sprg.30",			0x020000, 0xaeef6045, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "chr0.80",			0x200000, 0xf63a52a9, 3 | BRF_GRA },           //  5 Graphics
	{ "chr1.79",			0x200000, 0x4110c184, 3 | BRF_GRA },           //  6
	{ "chr2.78",			0x200000, 0xddb4b9ee, 3 | BRF_GRA },           //  7
	{ "chr3.77",			0x200000, 0x5ab367db, 3 | BRF_GRA },           //  8
	{ "chr4.84",			0x200000, 0x77d7ef99, 3 | BRF_GRA },           //  9
	{ "chr5.83",			0x200000, 0xa4d49e95, 3 | BRF_GRA },           // 10
	{ "chr6.82",			0x200000, 0x32eb33b0, 3 | BRF_GRA },           // 11
	{ "chr7.81",			0x200000, 0x2d30a21e, 3 | BRF_GRA },           // 12

	{ "psacrom.60",			0x200000, 0x73f1f279, 4 | BRF_GRA },           // 13 Roz tiles

	{ "sndpcm-b.22",		0x200000, 0xa5d844d2, 5 | BRF_SND },           // 14 YM2610 Delta T Samples

	{ "sndpcm-a.23",		0x200000, 0xe6d32373, 6 | BRF_SND },           // 15 YM2610 Samples
};

STD_ROM_PICK(gstrik2)
STD_ROM_FN(gstrik2)

struct BurnDriverD BurnDrvGstrik2 = {
	"gstrik2", NULL, NULL, NULL, "1996",
	"Grand Striker 2 (Europe and Oceania)\0", "ROZ layer broken", "Human Amusement", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, gstrik2RomInfo, gstrik2RomName, NULL, NULL, Gstrik2InputInfo, Gstrik2DIPInfo,
	gstrik2Init, DrvExit, Z80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	304, 224, 4, 3
};


// Grand Striker 2 (Japan)

static struct BurnRomInfo gstrik2jRomDesc[] = {
	{ "prg0.107",			0x080000, 0xe60a8c19, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "prg1.108",			0x080000, 0x853f6f7c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.109",			0x080000, 0xead86919, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.110",			0x080000, 0xe0b026e3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sprg.30",			0x020000, 0xaeef6045, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "chr0.80",			0x200000, 0xf63a52a9, 3 | BRF_GRA },           //  5 Graphics
	{ "chr1.79",			0x200000, 0x4110c184, 3 | BRF_GRA },           //  6
	{ "chr2.78",			0x200000, 0xddb4b9ee, 3 | BRF_GRA },           //  7
	{ "chr3.77",			0x200000, 0x5ab367db, 3 | BRF_GRA },           //  8
	{ "chr4.84",			0x200000, 0x77d7ef99, 3 | BRF_GRA },           //  9
	{ "chr5.83",			0x200000, 0xa4d49e95, 3 | BRF_GRA },           // 10
	{ "chr6.82",			0x200000, 0x32eb33b0, 3 | BRF_GRA },           // 11
	{ "chr7.81",			0x200000, 0x2d30a21e, 3 | BRF_GRA },           // 12

	{ "psacrom.60",			0x200000, 0x73f1f279, 4 | BRF_GRA },           // 13 Roz tiles
	
	{ "sndpcm-b.22",		0x200000, 0xa5d844d2, 5 | BRF_SND },           // 14 YM2610 Delta T Samples

	{ "sndpcm-a.23",		0x200000, 0xe6d32373, 6 | BRF_SND },           // 15 YM2610 Samples
};

STD_ROM_PICK(gstrik2j)
STD_ROM_FN(gstrik2j)

struct BurnDriverD BurnDrvGstrik2j = {
	"gstrik2j", "gstrik2", NULL, NULL, "1996",
	"Grand Striker 2 (Japan)\0", "ROZ layer broken", "Human Amusement", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, gstrik2jRomInfo, gstrik2jRomName, NULL, NULL, Gstrik2InputInfo, Gstrik2DIPInfo,
	gstrik2Init, DrvExit, Z80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	304, 224, 4, 3
};


// Sky Alert

static struct BurnRomInfo skyalertRomDesc[] = {
	{ "sa_c_09.bin",		0x020000, 0x6f14d9ae, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "sa_c_10.bin",		0x020000, 0xf10bb216, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sa_b_12.bin",		0x020000, 0xf358175d, 2 | BRF_PRG | BRF_ESS }, //  2 UPD7810

	{ "sa_a_02.bin",		0x040000, 0xf4f81d41, 3 | BRF_GRA },           //  3 Graphics
	{ "sa_a_04.bin",		0x040000, 0x7d071e7e, 3 | BRF_GRA },           //  4
	{ "sa_a_06.bin",		0x040000, 0x77e4d5e1, 3 | BRF_GRA },           //  5
	{ "sa_a_08.bin",		0x040000, 0xf2a5a093, 3 | BRF_GRA },           //  6
	{ "sa_a_01.bin",		0x040000, 0x41ec6491, 3 | BRF_GRA },           //  7
	{ "sa_a_03.bin",		0x040000, 0xe0dff10d, 3 | BRF_GRA },           //  8
	{ "sa_a_05.bin",		0x040000, 0x62169d31, 3 | BRF_GRA },           //  9
	{ "sa_a_07.bin",		0x040000, 0xa6f5966f, 3 | BRF_GRA },           // 10

	{ "sa_a_11.bin",		0x020000, 0x04842a60, 4 | BRF_SND },           // 11 MSM6295 Samples
};

STD_ROM_PICK(skyalert)
STD_ROM_FN(skyalert)

struct BurnDriver BurnDrvSkyalert = {
	"skyalert", NULL, NULL, NULL, "1992",
	"Sky Alert\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, skyalertRomInfo, skyalertRomName, NULL, NULL, SkyalertInputInfo, SkyalertDIPInfo,
	skyalertInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 360, 3, 4
};


// Pang Pom's

static struct BurnRomInfo pangpomsRomDesc[] = {
	{ "ppoms09.bin",		0x020000, 0x0c292dbc, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ppoms10.bin",		0x020000, 0x0bc18853, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ppoms12.bin",		0x020000, 0xa749357b, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "ppoms02.bin",		0x020000, 0x88f902f7, 3 | BRF_GRA },           //  3 Graphics
	{ "ppoms04.bin",		0x020000, 0x9190c2a0, 3 | BRF_GRA },           //  4
	{ "ppoms06.bin",		0x020000, 0xed15c93d, 3 | BRF_GRA },           //  5
	{ "ppoms08.bin",		0x020000, 0x9a3408b9, 3 | BRF_GRA },           //  6
	{ "ppoms01.bin",		0x020000, 0x11ac3810, 3 | BRF_GRA },           //  7
	{ "ppoms03.bin",		0x020000, 0xe595529e, 3 | BRF_GRA },           //  8
	{ "ppoms05.bin",		0x020000, 0x02226214, 3 | BRF_GRA },           //  9
	{ "ppoms07.bin",		0x020000, 0x48471c87, 3 | BRF_GRA },           // 10

	{ "ppoms11.bin",		0x020000, 0xe89bd565, 4 | BRF_SND },           // 11 MSM6295 Samples
};

STD_ROM_PICK(pangpoms)
STD_ROM_FN(pangpoms)

struct BurnDriver BurnDrvPangpoms = {
	"pangpoms", NULL, NULL, NULL, "1992",
	"Pang Pom's\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, pangpomsRomInfo, pangpomsRomName, NULL, NULL, SkyalertInputInfo, PangpomsDIPInfo,
	pangpomsInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	360, 224, 4, 3
};


// Pang Pom's (Mitchell)

static struct BurnRomInfo pangpomsmRomDesc[] = {
	{ "pa.c09",			0x020000, 0xe01a7a08, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "pa.c10",			0x020000, 0x5e509cee, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ppoms12.bin",		0x020000, 0xa749357b, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "ppoms02.bin",		0x020000, 0x88f902f7, 3 | BRF_GRA },           //  3 Graphics
	{ "pj.e04",			0x020000, 0x54bf2f10, 3 | BRF_GRA },           //  4
	{ "pj.e06",			0x020000, 0xc8b6347d, 3 | BRF_GRA },           //  5
	{ "ppoms08.bin",		0x020000, 0x9a3408b9, 3 | BRF_GRA },           //  6
	{ "ppoms01.bin",		0x020000, 0x11ac3810, 3 | BRF_GRA },           //  7
	{ "pj.e03",			0x020000, 0xd126e774, 3 | BRF_GRA },           //  8
	{ "pj.e05",			0x020000, 0x79c0ec1e, 3 | BRF_GRA },           //  9
	{ "ppoms07.bin",		0x020000, 0x48471c87, 3 | BRF_GRA },           // 10

	{ "ppoms11.bin",		0x020000, 0xe89bd565, 4 | BRF_SND },           // 11 MSM6295 Samples
};

STD_ROM_PICK(pangpomsm)
STD_ROM_FN(pangpomsm)

struct BurnDriver BurnDrvPangpomsm = {
	"pangpomsm", "pangpoms", NULL, NULL, "1992",
	"Pang Pom's (Mitchell)\0", NULL, "Metro (Mitchell license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, pangpomsmRomInfo, pangpomsmRomName, NULL, NULL, SkyalertInputInfo, PangpomsDIPInfo,
	pangpomsInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	360, 224, 4, 3
};


// Poitto!

static struct BurnRomInfo poittoRomDesc[] = {
	{ "pt-jd05.20e",		0x020000, 0x6b1be034, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "pt-jd06.20c",		0x020000, 0x3092d9d4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pt-jc08.3i",			0x020000, 0xf32d386a, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "pt-2.15i",			0x080000, 0x05d15d01, 3 | BRF_GRA },           //  3 Graphics
	{ "pt-4.19i",			0x080000, 0x8a39edb5, 3 | BRF_GRA },           //  4
	{ "pt-1.13i",			0x080000, 0xea6e2289, 3 | BRF_GRA },           //  5
	{ "pt-3.17i",			0x080000, 0x522917c1, 3 | BRF_GRA },           //  6

	{ "pt-jc07.3g",			0x040000, 0x5ae28b8d, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(poitto)
STD_ROM_FN(poitto)

struct BurnDriver BurnDrvPoitto = {
	"poitto", NULL, NULL, NULL, "1993",
	"Poitto!\0", NULL, "Metro / Able Corp.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, poittoRomInfo, poittoRomName, NULL, NULL, PururunInputInfo, PoittoDIPInfo,
	poittoInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	360, 224, 4, 3
};


// Last Fortress - Toride

static struct BurnRomInfo lastfortRomDesc[] = {
	{ "tr_jc09",			0x020000, 0x8b98a49a, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tr_jc10",			0x020000, 0x8d04da04, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tr_jb12",			0x020000, 0x8a8f5fef, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "tr_jc02",			0x020000, 0xdb3c5b79, 3 | BRF_GRA },           //  3 Graphics
	{ "tr_jc04",			0x020000, 0xf8ab2f9b, 3 | BRF_GRA },           //  4
	{ "tr_jc06",			0x020000, 0x47a7f397, 3 | BRF_GRA },           //  5
	{ "tr_jc08",			0x020000, 0xd7ba5e26, 3 | BRF_GRA },           //  6
	{ "tr_jc01",			0x020000, 0x3e3dab03, 3 | BRF_GRA },           //  7
	{ "tr_jc03",			0x020000, 0x87ac046f, 3 | BRF_GRA },           //  8
	{ "tr_jc05",			0x020000, 0x3fbbe49c, 3 | BRF_GRA },           //  9
	{ "tr_jc07",			0x020000, 0x05e1456b, 3 | BRF_GRA },           // 10

	{ "tr_jb11",			0x020000, 0x83786a09, 4 | BRF_SND },           // 11 MSM6295 Samples
};

STD_ROM_PICK(lastfort)
STD_ROM_FN(lastfort)

struct BurnDriver BurnDrvLastfort = {
	"lastfort", NULL, NULL, NULL, "1994",
	"Last Fortress - Toride\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, lastfortRomInfo, lastfortRomName, NULL, NULL, SkyalertInputInfo, LastfortDIPInfo,
	lastfortInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	360, 224, 4, 3
};


// Last Fortress - Toride (German)

static struct BurnRomInfo lastfortgRomDesc[] = {
	{ "tr_ma02.8g",			0x020000, 0xe6f40918, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tr_ma03.10g",		0x020000, 0xb00fb126, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tr_ma01.1i",			0x020000, 0x8a8f5fef, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "tr_ma04.15f",		0x080000, 0x5feafc6f, 3 | BRF_GRA },           //  3 Graphics
	{ "tr_ma07.17d",		0x080000, 0x7519d569, 3 | BRF_GRA },           //  4
	{ "tr_ma05.17f",		0x080000, 0x5d917ba5, 3 | BRF_GRA },           //  5
	{ "tr_ma06.15d",		0x080000, 0xd366c04e, 3 | BRF_GRA },           //  6

	{ "tr_ma08.1d",			0x020000, 0x83786a09, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(lastfortg)
STD_ROM_FN(lastfortg)

struct BurnDriver BurnDrvLastfortg = {
	"lastfortg", "lastfort", NULL, NULL, "1994",
	"Last Fortress - Toride (German)\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, lastfortgRomInfo, lastfortgRomName, NULL, NULL, LadykillInputInfo, LadykillDIPInfo,
	lastforgInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	360, 224, 4, 3
};


// Last Fortress - Toride (Erotic, Rev C)

static struct BurnRomInfo lastforteRomDesc[] = {
	{ "tr_hc09",			0x020000, 0x32f43390, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tr_hc10",			0x020000, 0x9536369c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tr_jb12",			0x020000, 0x8a8f5fef, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "tr_ha02",			0x020000, 0x11cfbc84, 3 | BRF_GRA },           //  3 Graphics
	{ "tr_ha04",			0x020000, 0x32bf9c26, 3 | BRF_GRA },           //  4
	{ "tr_ha06",			0x020000, 0x16937977, 3 | BRF_GRA },           //  5
	{ "tr_ha08",			0x020000, 0x6dd96a9b, 3 | BRF_GRA },           //  6
	{ "tr_ha01",			0x020000, 0xaceb44b3, 3 | BRF_GRA },           //  7
	{ "tr_ha03",			0x020000, 0xf18f1248, 3 | BRF_GRA },           //  8
	{ "tr_ha05",			0x020000, 0x79f769dd, 3 | BRF_GRA },           //  9
	{ "tr_ha07",			0x020000, 0xb6feacb2, 3 | BRF_GRA },           // 10

	{ "tr_jb11",			0x020000, 0x83786a09, 4 | BRF_SND },           // 11 MSM6295 Samples
};

STD_ROM_PICK(lastforte)
STD_ROM_FN(lastforte)

struct BurnDriver BurnDrvLastforte = {
	"lastforte", "lastfort", NULL, NULL, "1994",
	"Last Fortress - Toride (Erotic, Rev C)\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, lastforteRomInfo, lastforteRomName, NULL, NULL, SkyalertInputInfo, LastferoDIPInfo,
	lastfortInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	360, 224, 4, 3
};


// Last Fortress - Toride (Erotic, Rev A)

static struct BurnRomInfo lastforteaRomDesc[] = {
	{ "tr_ha09",			0x020000, 0x61fe8fb2, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tr_ha10",			0x020000, 0x14a9fba2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tr_jb12",			0x020000, 0x8a8f5fef, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "tr_ha02",			0x020000, 0x11cfbc84, 3 | BRF_GRA },           //  3 Graphics
	{ "tr_ha04",			0x020000, 0x32bf9c26, 3 | BRF_GRA },           //  4
	{ "tr_ha06",			0x020000, 0x16937977, 3 | BRF_GRA },           //  5
	{ "tr_ha08",			0x020000, 0x6dd96a9b, 3 | BRF_GRA },           //  6
	{ "tr_ha01",			0x020000, 0xaceb44b3, 3 | BRF_GRA },           //  7
	{ "tr_ha03",			0x020000, 0xf18f1248, 3 | BRF_GRA },           //  8
	{ "tr_ha05",			0x020000, 0x79f769dd, 3 | BRF_GRA },           //  9
	{ "tr_ha07",			0x020000, 0xb6feacb2, 3 | BRF_GRA },           // 10

	{ "tr_jb11",			0x020000, 0x83786a09, 4 | BRF_SND },           // 11 MSM6295 Samples
};

STD_ROM_PICK(lastfortea)
STD_ROM_FN(lastfortea)

struct BurnDriver BurnDrvLastfortea = {
	"lastfortea", "lastfort", NULL, NULL, "1994",
	"Last Fortress - Toride (Erotic, Rev A)\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, lastforteaRomInfo, lastforteaRomName, NULL, NULL, SkyalertInputInfo, LastferoDIPInfo,
	lastfortInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	360, 224, 4, 3
};


// Last Fortress - Toride (Korea)

static struct BurnRomInfo lastfortkRomDesc[] = {
	{ "7f-9",			0x020000, 0xd2894c1f, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "8f-10",			0x020000, 0x9696ea39, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tr_jb12",			0x020000, 0x8a8f5fef, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "7i-2",			0x040000, 0xd1fe8d7b, 3 | BRF_GRA },           //  3 Graphics
	{ "10i-4",			0x040000, 0x058126d4, 3 | BRF_GRA },           //  4
	{ "13i-6",			0x040000, 0x39a9dea2, 3 | BRF_GRA },           //  5
	{ "16i-8",			0x040000, 0x4c050baa, 3 | BRF_GRA },           //  6
	{ "5i-1",			0x040000, 0x0d503f05, 3 | BRF_GRA },           //  7
	{ "8i-3",			0x040000, 0xb6d4f753, 3 | BRF_GRA },           //  8
	{ "12i-5",			0x040000, 0xce69c805, 3 | BRF_GRA },           //  9
	{ "14i-7",			0x040000, 0x0cb38317, 3 | BRF_GRA },           // 10

	{ "tr_jb11",			0x020000, 0x83786a09, 4 | BRF_SND },           // 11 MSM6295 Samples
};

STD_ROM_PICK(lastfortk)
STD_ROM_FN(lastfortk)

struct BurnDriver BurnDrvLastfortk = {
	"lastfortk", "lastfort", NULL, NULL, "1994",
	"Last Fortress - Toride (Korea)\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, lastfortkRomInfo, lastfortkRomName, NULL, NULL, SkyalertInputInfo, LastferoDIPInfo,
	lastfortInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	360, 224, 4, 3
};


// Dharma Doujou

static struct BurnRomInfo dharmaRomDesc[] = {
	{ "dd__wea5.u39",		0x020000, 0x960319d7, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "dd__wea6.u42",		0x020000, 0x386eb6b3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "dd__wa-8.u9",		0x020000, 0xaf7ebc4c, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "dd__wa-2.u4",		0x080000, 0x2c67a5c8, 3 | BRF_GRA },           //  3 Graphics
	{ "dd__wa-4.u5",		0x080000, 0x36ca7848, 3 | BRF_GRA },           //  4
	{ "dd__wa-1.u10",		0x080000, 0xd8034574, 3 | BRF_GRA },           //  5
	{ "dd__wa-3.u11",		0x080000, 0xfe320fa3, 3 | BRF_GRA },           //  6

	{ "dd__wa-7.u3",		0x040000, 0x7ce817eb, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(dharma)
STD_ROM_FN(dharma)

struct BurnDriver BurnDrvDharma = {
	"dharma", NULL, NULL, NULL, "1994",
	"Dharma Doujou\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, dharmaRomInfo, dharmaRomName, NULL, NULL, PururunInputInfo, DharmaDIPInfo,
	dharmaInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Dharma Doujou (Japan)

static struct BurnRomInfo dharmajRomDesc[] = {
	{ "dd_jc-5",			0x020000, 0xb5d44426, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "dd_jc-6",			0x020000, 0xbc5a202e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "dd_ja-8",			0x020000, 0xaf7ebc4c, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "dd_jb-2",			0x080000, 0x2c07c29b, 3 | BRF_GRA },           //  3 Graphics
	{ "dd_jb-4",			0x080000, 0xfe15538e, 3 | BRF_GRA },           //  4
	{ "dd_jb-1",			0x080000, 0xe6ca9bf6, 3 | BRF_GRA },           //  5
	{ "dd_jb-3",			0x080000, 0x6ecbe193, 3 | BRF_GRA },           //  6

	{ "dd_ja-7",			0x040000, 0x7ce817eb, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(dharmaj)
STD_ROM_FN(dharmaj)

struct BurnDriver BurnDrvDharmaj = {
	"dharmaj", "dharma", NULL, NULL, "1994",
	"Dharma Doujou (Japan)\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, dharmajRomInfo, dharmajRomName, NULL, NULL, PururunInputInfo, DharmaDIPInfo,
	dharmajInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Dharma Doujou (Korea)

static struct BurnRomInfo dharmakRomDesc[] = {
	{ "5.bin",			0x020000, 0x7dec1f77, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "6.bin",			0x020000, 0xa194edbe, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "8.bin",			0x020000, 0xd0e0a8e2, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "2.bin",			0x080000, 0x3cc0bb6c, 3 | BRF_GRA },           //  3 Graphics
	{ "4.bin",			0x080000, 0x2cdcdf91, 3 | BRF_GRA },           //  4
	{ "1.bin",			0x080000, 0x312ee2ec, 3 | BRF_GRA },           //  5
	{ "3.bin",			0x080000, 0xb81aede8, 3 | BRF_GRA },           //  6

	{ "7.bin",			0x040000, 0x8af698d7, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(dharmak)
STD_ROM_FN(dharmak)

struct BurnDriver BurnDrvDharmak = {
	"dharmak", "dharma", NULL, NULL, "1994",
	"Dharma Doujou (Korea)\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, dharmakRomInfo, dharmakRomName, NULL, NULL, PururunInputInfo, DharmaDIPInfo,
	dharmaInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Pururun

static struct BurnRomInfo pururunRomDesc[] = {
	{ "pu9-19-5.20e",		0x020000, 0x5a466a1b, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "pu9-19-6.20c",		0x020000, 0xd155a53c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pu9-19-8.3i",		0x020000, 0xedc3830b, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "pu9-19-2.14i",		0x080000, 0x21550b26, 3 | BRF_GRA },           //  3 Graphics
	{ "pu9-19-4.18i",		0x080000, 0x3f3e216d, 3 | BRF_GRA },           //  4
	{ "pu9-19-1.12i",		0x080000, 0x7e83a75f, 3 | BRF_GRA },           //  5
	{ "pu9-19-3.16i",		0x080000, 0xd15485c5, 3 | BRF_GRA },           //  6

	{ "pu9-19-7.3g",		0x040000, 0x51ae4926, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(pururun)
STD_ROM_FN(pururun)

struct BurnDriver BurnDrvPururun = {
	"pururun", NULL, NULL, NULL, "1995",
	"Pururun\0", NULL, "Metro / Banpresto", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, pururunRomInfo, pururunRomName, NULL, NULL, PururunInputInfo, PururunDIPInfo,
	pururunInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// The Karate Tournament

static struct BurnRomInfo karatourRomDesc[] = {
	{ "2.2FAB.8G",			0x040000, 0x199a28d4, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "3.0560.10G",			0x040000, 0xb054e683, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "KT001.1I",			0x020000, 0x1dd2008c, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "361A04.15F",			0x100000, 0xf6bf20a5, 3 | BRF_GRA },           //  3 Graphics
	{ "361A07.17D",			0x100000, 0x794cc1c0, 3 | BRF_GRA },           //  4
	{ "361A05.17F",			0x100000, 0xea9c11fc, 3 | BRF_GRA },           //  5
	{ "361A06.15D",			0x100000, 0x7e15f058, 3 | BRF_GRA },           //  6

	{ "8.4A06.1D",			0x040000, 0x8d208179, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(karatour)
STD_ROM_FN(karatour)

struct BurnDriver BurnDrvKaratour = {
	"karatour", NULL, NULL, NULL, "1992",
	"The Karate Tournament\0", NULL, "Mitchell", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, karatourRomInfo, karatourRomName, NULL, NULL, KaratourInputInfo, KaratourDIPInfo,
	karatourInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 240, 4, 3
};


// The Karate Tournament (Japan)

static struct BurnRomInfo karatourjRomDesc[] = {
	{ "kt002.8g",			0x040000, 0x316a97ec, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "kt003.10g",			0x040000, 0xabe1b991, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "kt001.1i",			0x020000, 0x1dd2008c, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "361a04.15f",			0x100000, 0xf6bf20a5, 3 | BRF_GRA },           //  3 Graphics
	{ "361a07.17d",			0x100000, 0x794cc1c0, 3 | BRF_GRA },           //  4
	{ "361a05.17f",			0x100000, 0xea9c11fc, 3 | BRF_GRA },           //  5
	{ "361a06.15d",			0x100000, 0x7e15f058, 3 | BRF_GRA },           //  6

	{ "kt008.1d",			0x040000, 0x47cf9fa1, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(karatourj)
STD_ROM_FN(karatourj)

struct BurnDriver BurnDrvKaratourj = {
	"karatourj", "karatour", NULL, NULL, "1992",
	"The Karate Tournament (Japan)\0", NULL, "Mitchell", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, karatourjRomInfo, karatourjRomName, NULL, NULL, KaratourInputInfo, KaratourDIPInfo,
	karatourInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 240, 4, 3
};

// Lady Killer

static struct BurnRomInfo ladykillRomDesc[] = {
	{ "e2.8g",			0x040000, 0x211a4865, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "e3.10g",			0x040000, 0x581a55ea, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "e1.1i",			0x020000, 0xa4d95cfb, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "ladyj-4.15f",		0x100000, 0x65e5906c, 3 | BRF_GRA },           //  3 Graphics
	{ "ladyj-7.17d",		0x100000, 0x56bd64a5, 3 | BRF_GRA },           //  4
	{ "ladyj-5.17f",		0x100000, 0xa81ffaa3, 3 | BRF_GRA },           //  5
	{ "ladyj-6.15d",		0x100000, 0x3a34913a, 3 | BRF_GRA },           //  6

	{ "e8.1d",			0x040000, 0xda88244d, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(ladykill)
STD_ROM_FN(ladykill)

struct BurnDriver BurnDrvLadykill = {
	"ladykill", NULL, NULL, NULL, "1993",
	"Lady Killer\0", "Imperfect graphics", "Yanyaka (Mitchell license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, ladykillRomInfo, ladykillRomName, NULL, NULL, LadykillInputInfo, LadykillDIPInfo,
	karatourInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	240, 320, 3, 4
};


// Moeyo Gonta!! (Japan)

static struct BurnRomInfo moegontaRomDesc[] = {
	{ "j2.8g",			0x040000, 0xaa18d130, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "j3.10g",			0x040000, 0xb555e6ab, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "e1.1i",			0x020000, 0xa4d95cfb, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "ladyj-4.15f",		0x100000, 0x65e5906c, 3 | BRF_GRA },           //  3 Graphics
	{ "ladyj-7.17d",		0x100000, 0x56bd64a5, 3 | BRF_GRA },           //  4
	{ "ladyj-5.17f",		0x100000, 0xa81ffaa3, 3 | BRF_GRA },           //  5
	{ "ladyj-6.15d",		0x100000, 0x3a34913a, 3 | BRF_GRA },           //  6

	{ "e8j.1d",			0x040000, 0xf66c2a80, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(moegonta)
STD_ROM_FN(moegonta)

struct BurnDriver BurnDrvMoegonta = {
	"moegonta", "ladykill", NULL, NULL, "1993",
	"Moeyo Gonta!! (Japan)\0", NULL, "Yanyaka", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, moegontaRomInfo, moegontaRomName, NULL, NULL, LadykillInputInfo, LadykillDIPInfo,
	karatourInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	240, 320, 3, 4
};


// Daitoride

static struct BurnRomInfo daitoridRomDesc[] = {
	{ "dt-ja-5.19e",		0x020000, 0x441efd77, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "dt-ja-6.19c",		0x020000, 0x494f9cc3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "dt-ja-8.3h",			0x020000, 0x0351ad5b, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "dt-ja-2.14h",		0x080000, 0x56881062, 3 | BRF_GRA },           //  3 Graphics
	{ "dt-ja-4.18h",		0x080000, 0x85522e3b, 3 | BRF_GRA },           //  4
	{ "dt-ja-1.12h",		0x080000, 0x2a220bf2, 3 | BRF_GRA },           //  5
	{ "dt-ja-3.16h",		0x080000, 0xfd1f58e0, 3 | BRF_GRA },           //  6

	{ "dt-ja-7.3f",			0x040000, 0x0d888cde, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(daitorid)
STD_ROM_FN(daitorid)

struct BurnDriver BurnDrvDaitorid = {
	"daitorid", NULL, NULL, NULL, "1995",
	"Daitoride\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, daitoridRomInfo, daitoridRomName, NULL, NULL, DaitoridInputInfo, DaitoridDIPInfo,
	daitoridInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Daitoride (YMF278B version)

static struct BurnRomInfo daitoridaRomDesc[] = {
	{ "dt_ja-6.6",			0x040000, 0xc753954e, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "dt_ja-5.5",			0x040000, 0xc4340290, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "dt_ja-2.2",			0x080000, 0x6a262249, 2 | BRF_GRA },           //  2 Graphics
	{ "dt_ja-4.4",			0x080000, 0xcdcef57a, 2 | BRF_GRA },           //  3
	{ "dt_ja-1.1",			0x080000, 0xa6ccb1d2, 2 | BRF_GRA },           //  4
	{ "dt_ja-3.3",			0x080000, 0x32353e04, 2 | BRF_GRA },           //  5

	{ "yrw801-m",			0x200000, 0x2a9d8d43, 3 | BRF_GRA },           //  6 YMF278b Samples
	{ "dt_ja-7.7",			0x080000, 0x7a2d3222, 3 | BRF_GRA },           //  7
};

STD_ROM_PICK(daitorida)
STD_ROM_FN(daitorida)

struct BurnDriver BurnDrvDaitorida = {
	"daitorida", "daitorid", NULL, NULL, "1996",
	"Daitoride (YMF278B version)\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, daitoridaRomInfo, daitoridaRomName, NULL, NULL, DaitoridInputInfo, DaitoridDIPInfo,
	daitoridaInit, DrvExit, YMF278bFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Gun Master

static struct BurnRomInfo gunmastRomDesc[] = {
	{ "gmja-5.20e",			0x040000, 0x7334b2a3, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "gmja-6.20c",			0x040000, 0xc38d185e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gmja-8.3i",			0x020000, 0xab4bcc56, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "gmja-2.14i",			0x080000, 0xbc9acd54, 3 | BRF_GRA },           //  3 Graphics
	{ "gmja-4.18i",			0x080000, 0xf2d72d90, 3 | BRF_GRA },           //  4
	{ "gmja-1.12i",			0x080000, 0x336d0a90, 3 | BRF_GRA },           //  5
	{ "gmja-3.16i",			0x080000, 0xa6651297, 3 | BRF_GRA },           //  6

	{ "gmja-7.3g",			0x040000, 0x3a342312, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(gunmast)
STD_ROM_FN(gunmast)

struct BurnDriver BurnDrvGunmast = {
	"gunmast", NULL, NULL, NULL, "1994",
	"Gun Master\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, gunmastRomInfo, gunmastRomName, NULL, NULL, GunmastInputInfo, GunmastDIPInfo,
	pururunInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Sankokushi (Japan)

static struct BurnRomInfo _3kokushiRomDesc[] = {
	{ "5.20e",			0x040000, 0x6104ea35, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "6.20c",			0x040000, 0xaac25540, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "8.3i",			0x020000, 0xf56cca45, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "2.14i",			0x080000, 0x291f8149, 3 | BRF_GRA },           //  3 Graphics
	{ "4.18i",			0x080000, 0x9317c359, 3 | BRF_GRA },           //  4
	{ "1.12i",			0x080000, 0xd5495759, 3 | BRF_GRA },           //  5
	{ "3.16i",			0x080000, 0x3d76bdf3, 3 | BRF_GRA },           //  6

	{ "7.3g",			0x040000, 0x78fe9d44, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(_3kokushi)
STD_ROM_FN(_3kokushi)

struct BurnDriver BurnDrv_3kokushi = {
	"3kokushi", NULL, NULL, NULL, "1996",
	"Sankokushi (Japan)\0", NULL, "Mitchell", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, _3kokushiRomInfo, _3kokushiRomName, NULL, NULL, PuzzliInputInfo, _3kokushiDIPInfo,
	kokushiInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 240, 4, 3
};


// Puzzli

static struct BurnRomInfo puzzliRomDesc[] = {
	{ "pz_jb5.20e",			0x020000, 0x33bbbd28, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "pz_jb6.20c",			0x020000, 0xe0bdea18, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pz_jb8.3i",			0x020000, 0xc652da32, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "pz_jb2.14i",			0x080000, 0x0c0997d4, 3 | BRF_GRA },           //  3 Graphics
	{ "pz_jb4.18i",			0x080000, 0x576bc5c2, 3 | BRF_GRA },           //  4
	{ "pz_jb1.12i",			0x080000, 0x29f01eb3, 3 | BRF_GRA },           //  5
	{ "pz_jb3.16i",			0x080000, 0x6753e282, 3 | BRF_GRA },           //  6

	{ "pz_jb7.3g",			0x040000, 0xb3aab610, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(puzzli)
STD_ROM_FN(puzzli)

struct BurnDriver BurnDrvPuzzli = {
	"puzzli", NULL, NULL, NULL, "1995",
	"Puzzli\0", NULL, "Metro / Banpresto", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, puzzliRomInfo, puzzliRomName, NULL, NULL, PuzzliInputInfo, PuzzliDIPInfo,
	daitoridInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Toride II Adauchi Gaiden

static struct BurnRomInfo toride2gRomDesc[] = {
	{ "tr2aja-5.20e",		0x040000, 0xb96a52f6, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tr2aja-6.20c",		0x040000, 0x2918b6b4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tr2aja-8.3i",		0x020000, 0xfdd29146, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "tr2aja-2.14i",		0x080000, 0x5c73f629, 3 | BRF_GRA },           //  3 Graphics
	{ "tr2aja-4.18i",		0x080000, 0x67ebaf1b, 3 | BRF_GRA },           //  4
	{ "tr2aja-1.12i",		0x080000, 0x96245a5c, 3 | BRF_GRA },           //  5
	{ "tr2aja-3.16i",		0x080000, 0x49013f5d, 3 | BRF_GRA },           //  6

	{ "tr2aja-7.3g",		0x020000, 0x630c6193, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(toride2g)
STD_ROM_FN(toride2g)

struct BurnDriver BurnDrvToride2g = {
	"toride2g", NULL, NULL, NULL, "1994",
	"Toride II Adauchi Gaiden\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, toride2gRomInfo, toride2gRomName, NULL, NULL, PuzzliInputInfo, Toride2gDIPInfo,
	toride2gInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Toride II Adauchi Gaiden (German)

static struct BurnRomInfo toride2ggRomDesc[] = {
	{ "trii_ge_5.20e",		0x040000, 0x5e0815a8, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "trii_ge_6.20c",		0x040000, 0x55eba67d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tr2_jb-8.3i",		0x020000, 0x0168f46f, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "trii_gb_2.14i",		0x080000, 0x5949e65f, 3 | BRF_GRA },           //  3 Graphics
	{ "trii_gb_4.18i",		0x080000, 0xadc84c7b, 3 | BRF_GRA },           //  4
	{ "trii_gb_1.12i",		0x080000, 0xbcf30944, 3 | BRF_GRA },           //  5
	{ "trii_gb_3.16i",		0x080000, 0x138e68d0, 3 | BRF_GRA },           //  6

	{ "tr2_ja_7.3g",		0x020000, 0x6ee32315, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(toride2gg)
STD_ROM_FN(toride2gg)

struct BurnDriver BurnDrvToride2gg = {
	"toride2gg", "toride2g", NULL, NULL, "1994",
	"Toride II Adauchi Gaiden (German)\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, toride2ggRomInfo, toride2ggRomName, NULL, NULL, PuzzliInputInfo, Toride2gDIPInfo,
	toride2gInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Toride II Bok Su Oi Jeon Adauchi Gaiden (Korea)

static struct BurnRomInfo toride2gkRomDesc[] = {
	{ "5",				0x040000, 0x7e3f943a, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "6",				0x040000, 0x92726910, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "8",				0x020000, 0xfdd29146, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "2",				0x080000, 0x5e7fb9db, 3 | BRF_GRA },           //  3 Graphics
	{ "4",				0x080000, 0x558c03e7, 3 | BRF_GRA },           //  4
	{ "1",				0x080000, 0x5e819ccd, 3 | BRF_GRA },           //  5
	{ "3",				0x080000, 0x24029583, 3 | BRF_GRA },           //  6

	{ "7",				0x020000, 0x630c6193, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(toride2gk)
STD_ROM_FN(toride2gk)

struct BurnDriver BurnDrvToride2gk = {
	"toride2gk", "toride2g", NULL, NULL, "1994",
	"Toride II Bok Su Oi Jeon Adauchi Gaiden (Korea)\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, toride2gkRomInfo, toride2gkRomName, NULL, NULL, PuzzliInputInfo, Toride2gDIPInfo,
	toride2gInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Toride II (Japan)

static struct BurnRomInfo toride2jRomDesc[] = {
	{ "tr2_jk-5.20e",		0x040000, 0xf2668578, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tr2_jk-6.20c",		0x040000, 0x4c87629f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tr2_jb-8.3i",		0x020000, 0x0168f46f, 2 | BRF_PRG | BRF_ESS }, //  2 uPD7810 Code

	{ "tr2_jb-2.14i",		0x080000, 0xb31754dc, 3 | BRF_GRA },           //  3 Graphics
	{ "tr2_jb-4.18i",		0x080000, 0xa855c3fa, 3 | BRF_GRA },           //  4
	{ "tr2_jb-1.12i",		0x080000, 0x856f40b7, 3 | BRF_GRA },           //  5
	{ "tr2_jb-3.16i",		0x080000, 0x78ba205f, 3 | BRF_GRA },           //  6

	{ "tr2_ja_7.3g",		0x020000, 0x6ee32315, 4 | BRF_SND },           //  7 MSM6295 Samples
};

STD_ROM_PICK(toride2j)
STD_ROM_FN(toride2j)

struct BurnDriver BurnDrvToride2j = {
	"toride2j", "toride2g", NULL, NULL, "1994",
	"Toride II (Japan)\0", "No sound", "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, toride2jRomInfo, toride2jRomName, NULL, NULL, PuzzliInputInfo, Toride2gDIPInfo,
	toride2gInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Mouse Shooter GoGo

static struct BurnRomInfo msgogoRomDesc[] = {
	{ "ms_wa-6.6",			0x040000, 0x986acac8, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ms_wa-5.5",			0x040000, 0x746d9f99, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ms_wa-2.2",			0x080000, 0x0d36c2b9, 2 | BRF_GRA },           //  2 Graphics
	{ "ms_wa-4.4",			0x080000, 0xfd387126, 2 | BRF_GRA },           //  3
	{ "ms_ja-1.1",			0x080000, 0x8ec4e81d, 2 | BRF_GRA },           //  4
	{ "ms_wa-3.3",			0x080000, 0x06cb6807, 2 | BRF_GRA },           //  5

	{ "yrw801-m",			0x200000, 0x2a9d8d43, 3 | BRF_GRA },           //  6 YMF278b Samples
	{ "ms_wa-7.7",			0x080000, 0xe19941cb, 3 | BRF_GRA },           //  7
};

STD_ROM_PICK(msgogo)
STD_ROM_FN(msgogo)

struct BurnDriver BurnDrvMsgogo = {
	"msgogo", NULL, NULL, NULL, "1995",
	"Mouse Shooter GoGo\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, msgogoRomInfo, msgogoRomName, NULL, NULL, PuzzliInputInfo, MsgogoDIPInfo,
	msgogoInit, DrvExit, YMF278bFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Bal Cube

static struct BurnRomInfo balcubeRomDesc[] = {
	{ "bal-cube_06.6",		0x040000, 0xc400f84d, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "bal-cube_05.5",		0x040000, 0x15313e3f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bal-cube_02.2",		0x080000, 0x492ca8f0, 2 | BRF_GRA },           //  2 Graphics
	{ "bal-cube_04.4",		0x080000, 0xd1acda2c, 2 | BRF_GRA },           //  3
	{ "bal-cube_01.1",		0x080000, 0x0ea3d161, 2 | BRF_GRA },           //  4
	{ "bal-cube_03.3",		0x080000, 0xeef1d3b4, 2 | BRF_GRA },           //  5

	{ "yrw801-m",			0x200000, 0x2a9d8d43, 3 | BRF_GRA },           //  6 YMF278b Samples
	{ "bal-cube_07.7",		0x080000, 0xf769287d, 3 | BRF_GRA },           //  7
};

STD_ROM_PICK(balcube)
STD_ROM_FN(balcube)

struct BurnDriver BurnDrvBalcube = {
	"balcube", NULL, NULL, NULL, "1996",
	"Bal Cube\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, balcubeRomInfo, balcubeRomName, NULL, NULL, BalcubeInputInfo, BalcubeDIPInfo,
	balcubeInit, DrvExit, YMF278bFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Bang Bang Ball (v1.05)

static struct BurnRomInfo bangballRomDesc[] = {
	{ "b-ball_j_rom#006.u18",	0x040000, 0x0e4124bc, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "b-ball_j_rom#005.u19",	0x040000, 0x3fa08587, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bp963a_u30.u30",		0x100000, 0xb0ca8e39, 2 | BRF_GRA },           //  2 Graphics
	{ "bp963a_u29.u29",		0x100000, 0xd934468f, 2 | BRF_GRA },           //  3
	{ "bp963a_u28.u28",		0x100000, 0x96d03c6a, 2 | BRF_GRA },           //  4
	{ "bp963a_u27.u27",		0x100000, 0x5e3c7732, 2 | BRF_GRA },           //  5

	{ "yrw801-m",			0x200000, 0x2a9d8d43, 3 | BRF_GRA },           //  6 YMF278b Samples
	{ "b-ball_j_rom#007.u49",	0x080000, 0x04cc91a9, 3 | BRF_GRA },           //  7
};

STD_ROM_PICK(bangball)
STD_ROM_FN(bangball)

struct BurnDriver BurnDrvBangball = {
	"bangball", NULL, NULL, NULL, "1996",
	"Bang Bang Ball (v1.05)\0", NULL, "Banpresto / Kunihiko Tashiro+Goodhouse", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, bangballRomInfo, bangballRomName, NULL, NULL, BalcubeInputInfo, BangballDIPInfo,
	bangballInit, DrvExit, YMF278bFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Battle Bubble (v2.00)

static struct BurnRomInfo batlbublRomDesc[] = {
	{ "lm-01.u11",			0x080000, 0x1d562807, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "lm-02.u12",			0x080000, 0x852e4750, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "lm-07.u30",			0x200000, 0x03d9dfd8, 2 | BRF_GRA },           //  2 Graphics
	{ "lm-06.u29",			0x200000, 0x5efb905b, 2 | BRF_GRA },           //  3
	{ "lm-05.u28",			0x200000, 0xe53ba59f, 2 | BRF_GRA },           //  4
	{ "lm-04.u27",			0x200000, 0x2e687cfb, 2 | BRF_GRA },           //  5

	{ "lm-08.u40",			0x200000, 0x2a9d8d43, 3 | BRF_GRA },           //  6 YMF278b Samples
	{ "lm-03.u42",			0x080000, 0x04cc91a9, 3 | BRF_GRA },           //  7
};

STD_ROM_PICK(batlbubl)
STD_ROM_FN(batlbubl)

struct BurnDriver BurnDrvBatlbubl = {
	"batlbubl", "bangball", NULL, NULL, "1999",
	"Battle Bubble (v2.00)\0", NULL, "Banpresto (Limenko license?)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, batlbublRomInfo, batlbublRomName, NULL, NULL, BalcubeInputInfo, BatlbublDIPInfo,
	batlbublInit, DrvExit, YMF278bFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};
