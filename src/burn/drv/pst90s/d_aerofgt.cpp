// FinalBurn Neo Video System Co. hardware driver module
// Based on MAME driver by Nicola Salmoria

// aerfboot & other boots - gfx ish
// wbbc97		- bitmap scrolling is not quite right, same in mame...
// kickball		- bad sprites, bad sound

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2610.h"
#include "burn_ym3812.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "upd7759.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[5];
static UINT8 *DrvSndROM[2];
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvWRAM[2];
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprLutRAM[2];
static UINT8 *DrvExtraRAM;
static UINT8 *DrvRasterRAM;
static UINT8 *DrvBitmapRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 gfxbank[8];
static INT32 spritepalettebank;
static INT32 charpalettebank;
static INT32 flipscreen;
static INT32 soundlatch[2];
static INT32 scrollx[2];
static INT32 scrolly[2];
static INT32 bitmap_enable;
static INT32 spikes91_lookup;
static INT32 bankdata;

//#define USE_GGA

#ifdef USE_GGA
static INT32 gga_address_latch;
static UINT8 gga_regs[16];
#endif

enum sound_systems { Z80_YM2610, NOZ80_MSM6295, Z80_MSM6295, Z80_YM3812_MSM6295, Z80_YM3812_UPD7759, Z80_YM2151 };

static INT32 sound_type = Z80_YM2610;
static INT32 sek_irq_line = 1;
static INT32 zet_frequency = 5000000;
static INT32 sprite_lut_mask[2] = { 0x3fff, 0x3fff };

static INT32 screen_offsets[2] = { 0, 0 };

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvJoy5[16];
static UINT8 DrvJoy6[16];
static UINT8 DrvDips[3];
static UINT16 DrvInputs[6];
static UINT8 DrvReset;

static INT32 nExtraCycles;

static struct BurnInputInfo PspikesInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pspikes)

static struct BurnInputInfo SpinlbrkInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Spinlbrk)

static struct BurnInputInfo KaratblzInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy3 + 8,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 10,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 fire 4"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy3 + 9,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy3 + 11,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy4 + 0,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy4 + 1,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy4 + 2,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p4 fire 3"	},
	{"P4 Button 4",		BIT_DIGITAL,	DrvJoy4 + 7,	"p4 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 14,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Karatblz)

static struct BurnInputInfo TurbofrcInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy1 + 15,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 13,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Turbofrc)

static struct BurnInputInfo AerofgtInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Aerofgt)

static struct BurnInputInfo AerofgtbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Aerofgtb)

static struct BurnDIPInfo PspikesDIPList[]=
{
	{0x14, 0xff, 0xff, 0xbf, NULL							},
	{0x15, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Coin A"						},
	{0x14, 0x01, 0x03, 0x01, "3 Coins 1 Credits"			},
	{0x14, 0x01, 0x03, 0x02, "2 Coins 1 Credits"			},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  1 Credits"			},
	{0x14, 0x01, 0x03, 0x00, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin B"						},
	{0x14, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"			},
	{0x14, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"			},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"			},
	{0x14, 0x01, 0x0c, 0x00, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x14, 0x01, 0x40, 0x40, "Off"							},
	{0x14, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x14, 0x01, 0x80, 0x80, "Off"							},
	{0x14, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x15, 0x01, 0x01, 0x01, "Off"							},
	{0x15, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "1 Player Starting Score"		},
	{0x15, 0x01, 0x06, 0x06, "12-12"						},
	{0x15, 0x01, 0x06, 0x04, "11-11"						},
	{0x15, 0x01, 0x06, 0x02, "11-12"						},
	{0x15, 0x01, 0x06, 0x00, "10-12"						},

	{0   , 0xfe, 0   ,    4, "2 Players Starting Score"		},
	{0x15, 0x01, 0x18, 0x18, "9-9"							},
	{0x15, 0x01, 0x18, 0x10, "7-7"							},
	{0x15, 0x01, 0x18, 0x08, "5-5"							},
	{0x15, 0x01, 0x18, 0x00, "0-0"							},

	{0   , 0xfe, 0   ,    2, "Difficulty"					},
	{0x15, 0x01, 0x20, 0x20, "Normal"						},
	{0x15, 0x01, 0x20, 0x00, "Hard"							},

	{0   , 0xfe, 0   ,    2, "2 Players Time Per Credit"	},
	{0x15, 0x01, 0x40, 0x40, "3 min"						},
	{0x15, 0x01, 0x40, 0x00, "2 min"						},

	{0   , 0xfe, 0   ,    2, "Debug"						},
	{0x15, 0x01, 0x80, 0x80, "Off"							},
	{0x15, 0x01, 0x80, 0x00, "On"							},
};

STDDIPINFO(Pspikes)

static struct BurnDIPInfo PspikesbDIPList[]=
{
	{0x14, 0xff, 0xff, 0xbf, NULL							},
	{0x15, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Coin A"						},
	{0x14, 0x01, 0x03, 0x01, "3 Coins 1 Credits"			},
	{0x14, 0x01, 0x03, 0x02, "2 Coins 1 Credits"			},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  1 Credits"			},
	{0x14, 0x01, 0x03, 0x00, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin B"						},
	{0x14, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"			},
	{0x14, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"			},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"			},
	{0x14, 0x01, 0x0c, 0x00, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x14, 0x01, 0x40, 0x40, "Off"							},
	{0x14, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "1 Player Starting Score"		},
	{0x15, 0x01, 0x06, 0x06, "12-12"						},
	{0x15, 0x01, 0x06, 0x04, "11-11"						},
	{0x15, 0x01, 0x06, 0x02, "11-12"						},
	{0x15, 0x01, 0x06, 0x00, "10-12"						},

	{0   , 0xfe, 0   ,    4, "2 Players Starting Score"		},
	{0x15, 0x01, 0x18, 0x18, "9-9"							},
	{0x15, 0x01, 0x18, 0x10, "7-7"							},
	{0x15, 0x01, 0x18, 0x08, "5-5"							},
	{0x15, 0x01, 0x18, 0x00, "0-0"							},

	{0   , 0xfe, 0   ,    2, "Difficulty"					},
	{0x15, 0x01, 0x20, 0x20, "Normal"						},
	{0x15, 0x01, 0x20, 0x00, "Hard"							},

	{0   , 0xfe, 0   ,    2, "2 Players Time Per Credit"	},
	{0x15, 0x01, 0x40, 0x40, "3 min"						},
	{0x15, 0x01, 0x40, 0x00, "2 min"						},

	{0   , 0xfe, 0   ,    2, "Debug"						},
	{0x15, 0x01, 0x80, 0x80, "Off"							},
	{0x15, 0x01, 0x80, 0x00, "On"							},
};

STDDIPINFO(Pspikesb)

static struct BurnDIPInfo PspikescDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL							},
	{0x15, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Coin A"						},
	{0x14, 0x01, 0x03, 0x01, "3 Coins 1 Credits"			},
	{0x14, 0x01, 0x03, 0x02, "2 Coins 1 Credits"			},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  1 Credits"			},
	{0x14, 0x01, 0x03, 0x00, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin B"						},
	{0x14, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"			},
	{0x14, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"			},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"			},
	{0x14, 0x01, 0x0c, 0x00, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Region"						},
	{0x14, 0x01, 0x30, 0x30, "China"						},
	{0x14, 0x01, 0x30, 0x20, "Taiwans"						},
	{0x14, 0x01, 0x30, 0x10, "Hong Kong"					},
	{0x14, 0x01, 0x30, 0x00, "China"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x14, 0x01, 0x40, 0x40, "Off"							},
	{0x14, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "1 Player Starting Score"		},
	{0x15, 0x01, 0x06, 0x06, "12-12"						},
	{0x15, 0x01, 0x06, 0x04, "11-11"						},
	{0x15, 0x01, 0x06, 0x02, "11-12"						},
	{0x15, 0x01, 0x06, 0x00, "10-12"						},

	{0   , 0xfe, 0   ,    4, "2 Players Starting Score"		},
	{0x15, 0x01, 0x18, 0x18, "9-9"							},
	{0x15, 0x01, 0x18, 0x10, "7-7"							},
	{0x15, 0x01, 0x18, 0x08, "5-5"							},
	{0x15, 0x01, 0x18, 0x00, "0-0"							},

	{0   , 0xfe, 0   ,    2, "Difficulty"					},
	{0x15, 0x01, 0x20, 0x20, "Normal"						},
	{0x15, 0x01, 0x20, 0x00, "Hard"							},

	{0   , 0xfe, 0   ,    2, "2 Players Time Per Credit"	},
	{0x15, 0x01, 0x40, 0x40, "3 min"						},
	{0x15, 0x01, 0x40, 0x00, "2 min"						},

	{0   , 0xfe, 0   ,    2, "Debug"						},
	{0x15, 0x01, 0x80, 0x80, "Off"							},
	{0x15, 0x01, 0x80, 0x00, "On"							},
};

STDDIPINFO(Pspikesc)

static struct BurnDIPInfo SpinlbrkDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL							},
	{0x15, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,   16, "Coin A"						},
	{0x14, 0x01, 0x0f, 0x0f, "1 Credit 1 Health Pack"		},
	{0x14, 0x01, 0x0f, 0x0e, "1 Credit 2 Health Packs"		},
	{0x14, 0x01, 0x0f, 0x0d, "1 Credit 3 Health Packs"		},
	{0x14, 0x01, 0x0f, 0x0c, "1 Credit 4 Health Packs"		},
	{0x14, 0x01, 0x0f, 0x0b, "1 Credit 5 Health Packs"		},
	{0x14, 0x01, 0x0f, 0x0a, "1 Credit 6 Health Packs"		},
	{0x14, 0x01, 0x0f, 0x09, "2 Credits 1 Health Pack"		},
	{0x14, 0x01, 0x0f, 0x08, "3 Credits 1 Health Pack"		},
	{0x14, 0x01, 0x0f, 0x07, "4 Credits 1 Health Pack"		},
	{0x14, 0x01, 0x0f, 0x06, "5 Credits 1 Health Pack"		},
	{0x14, 0x01, 0x0f, 0x05, "2 Credits 2 Health Packs"		},
	{0x14, 0x01, 0x0f, 0x04, "2-1-1C  1-1-1 HPs"			},
	{0x14, 0x01, 0x0f, 0x03, "2-2C 1-2 HPs"					},
	{0x14, 0x01, 0x0f, 0x02, "1-1-1-1-1C 1-1-1-1-2 HPs"		},
	{0x14, 0x01, 0x0f, 0x01, "1-1-1-1C 1-1-1-2 HPs"			},
	{0x14, 0x01, 0x0f, 0x00, "1-1C 1-2 HPs"					},

	{0   , 0xfe, 0   ,   16, "Coin B"						},
	{0x14, 0x01, 0xf0, 0xf0, "1 Credit 1 Health Pack"		},
	{0x14, 0x01, 0xf0, 0xe0, "1 Credit 2 Health Packs"		},
	{0x14, 0x01, 0xf0, 0xd0, "1 Credit 3 Health Packs"		},
	{0x14, 0x01, 0xf0, 0xc0, "1 Credit 4 Health Packs"		},
	{0x14, 0x01, 0xf0, 0xb0, "1 Credit 5 Health Packs"		},
	{0x14, 0x01, 0xf0, 0xa0, "1 Credit 6 Health Packs"		},
	{0x14, 0x01, 0xf0, 0x90, "2 Credits 1 Health Pack"		},
	{0x14, 0x01, 0xf0, 0x80, "3 Credits 1 Health Pack"		},
	{0x14, 0x01, 0xf0, 0x70, "4 Credits 1 Health Pack"		},
	{0x14, 0x01, 0xf0, 0x60, "5 Credits 1 Health Pack"		},
	{0x14, 0x01, 0xf0, 0x50, "2 Credits 2 Health Packs"		},
	{0x14, 0x01, 0xf0, 0x40, "2-1-1C  1-1-1 HPs"			},
	{0x14, 0x01, 0xf0, 0x30, "2-2C 1-2 HPs"					},
	{0x14, 0x01, 0xf0, 0x20, "1-1-1-1-1C 1-1-1-1-2 HPs"		},
	{0x14, 0x01, 0xf0, 0x10, "1-1-1-1C 1-1-1-2 HPs"			},
	{0x14, 0x01, 0xf0, 0x00, "1-1C 1-2 HPs"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x15, 0x01, 0x03, 0x03, "Normal"						},
	{0x15, 0x01, 0x03, 0x02, "Easy"							},
	{0x15, 0x01, 0x03, 0x01, "Hard"							},
	{0x15, 0x01, 0x03, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Credits For Extra Hitpoints"	},
	{0x15, 0x01, 0x04, 0x00, "Off"							},
	{0x15, 0x01, 0x04, 0x04, "On"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x15, 0x01, 0x08, 0x08, "Off"							},
	{0x15, 0x01, 0x08, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Lever Type"					},
	{0x15, 0x01, 0x10, 0x10, "Digital"						},
	{0x15, 0x01, 0x10, 0x00, "Analog"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x15, 0x01, 0x20, 0x20, "Off"							},
	{0x15, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Health Pack"					},
	{0x15, 0x01, 0x40, 0x40, "32 Hitpoints"					},
	{0x15, 0x01, 0x40, 0x00, "40 Hitpoints"					},

	{0   , 0xfe, 0   ,    2, "Life Restoration"				},
	{0x15, 0x01, 0x80, 0x80, "10 Points"					},
	{0x15, 0x01, 0x80, 0x00, "5 Points"						},
};

STDDIPINFO(Spinlbrk)

static struct BurnDIPInfo SpinlbrkuDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL							},
	{0x15, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,   16, "Coin A"						},
	{0x14, 0x01, 0x0f, 0x0f, "1 Credit 1 Health Pack"		},
	{0x14, 0x01, 0x0f, 0x0e, "1 Credit 2 Health Packs"		},
	{0x14, 0x01, 0x0f, 0x0d, "1 Credit 3 Health Packs"		},
	{0x14, 0x01, 0x0f, 0x0c, "1 Credit 4 Health Packs"		},
	{0x14, 0x01, 0x0f, 0x0b, "1 Credit 5 Health Packs"		},
	{0x14, 0x01, 0x0f, 0x0a, "1 Credit 6 Health Packs"		},
	{0x14, 0x01, 0x0f, 0x09, "2 Credits 1 Health Pack"		},
	{0x14, 0x01, 0x0f, 0x08, "3 Credits 1 Health Pack"		},
	{0x14, 0x01, 0x0f, 0x07, "4 Credits 1 Health Pack"		},
	{0x14, 0x01, 0x0f, 0x06, "5 Credits 1 Health Pack"		},
	{0x14, 0x01, 0x0f, 0x05, "2 Credits 2 Health Packs"		},
	{0x14, 0x01, 0x0f, 0x04, "2-1-1C  1-1-1 HPs"			},
	{0x14, 0x01, 0x0f, 0x03, "2-2C 1-2 HPs"					},
	{0x14, 0x01, 0x0f, 0x02, "1-1-1-1-1C 1-1-1-1-2 HPs"		},
	{0x14, 0x01, 0x0f, 0x01, "1-1-1-1C 1-1-1-2 HPs"			},
	{0x14, 0x01, 0x0f, 0x00, "1-1C 1-2 HPs"					},

	{0   , 0xfe, 0   ,   16, "Coin B"						},
	{0x14, 0x01, 0xf0, 0xf0, "1 Credit 1 Health Pack"		},
	{0x14, 0x01, 0xf0, 0xe0, "1 Credit 2 Health Packs"		},
	{0x14, 0x01, 0xf0, 0xd0, "1 Credit 3 Health Packs"		},
	{0x14, 0x01, 0xf0, 0xc0, "1 Credit 4 Health Packs"		},
	{0x14, 0x01, 0xf0, 0xb0, "1 Credit 5 Health Packs"		},
	{0x14, 0x01, 0xf0, 0xa0, "1 Credit 6 Health Packs"		},
	{0x14, 0x01, 0xf0, 0x90, "2 Credits 1 Health Pack"		},
	{0x14, 0x01, 0xf0, 0x80, "3 Credits 1 Health Pack"		},
	{0x14, 0x01, 0xf0, 0x70, "4 Credits 1 Health Pack"		},
	{0x14, 0x01, 0xf0, 0x60, "5 Credits 1 Health Pack"		},
	{0x14, 0x01, 0xf0, 0x50, "2 Credits 2 Health Packs"		},
	{0x14, 0x01, 0xf0, 0x40, "2-1-1C  1-1-1 HPs"			},
	{0x14, 0x01, 0xf0, 0x30, "2-2C 1-2 HPs"					},
	{0x14, 0x01, 0xf0, 0x20, "1-1-1-1-1C 1-1-1-1-2 HPs"		},
	{0x14, 0x01, 0xf0, 0x10, "1-1-1-1C 1-1-1-2 HPs"			},
	{0x14, 0x01, 0xf0, 0x00, "1-1C 1-2 HPs"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x15, 0x01, 0x03, 0x03, "Normal"						},
	{0x15, 0x01, 0x03, 0x02, "Easy"							},
	{0x15, 0x01, 0x03, 0x01, "Hard"							},
	{0x15, 0x01, 0x03, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Credits For Extra Hitpoints"	},
	{0x15, 0x01, 0x04, 0x00, "Off"							},
	{0x15, 0x01, 0x04, 0x04, "On"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x15, 0x01, 0x08, 0x08, "Off"							},
	{0x15, 0x01, 0x08, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Lever Type"					},
	{0x15, 0x01, 0x10, 0x10, "Digital"						},
	{0x15, 0x01, 0x10, 0x00, "Analog"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x15, 0x01, 0x20, 0x20, "Off"							},
	{0x15, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Health Pack"					},
	{0x15, 0x01, 0x40, 0x40, "20 Hitpoints"					},
	{0x15, 0x01, 0x40, 0x00, "32 Hitpoints"					},

	{0   , 0xfe, 0   ,    2, "Life Restoration"				},
	{0x15, 0x01, 0x80, 0x80, "10 Points"					},
	{0x15, 0x01, 0x80, 0x00, "5 Points"						},
};

STDDIPINFO(Spinlbrku)

static struct BurnDIPInfo KaratblzDIPList[]=
{
	{0x2b, 0xff, 0xff, 0xff, NULL							},
	{0x2c, 0xff, 0xff, 0xbf, NULL							},

	{0   , 0xfe, 0   ,    8, "Coinage"						},
	{0x2b, 0x01, 0x07, 0x04, "4 Coins 1 Credits"			},
	{0x2b, 0x01, 0x07, 0x05, "3 Coins 1 Credits"			},
	{0x2b, 0x01, 0x07, 0x06, "2 Coins 1 Credits"			},
	{0x2b, 0x01, 0x07, 0x07, "1 Coin  1 Credits"			},
	{0x2b, 0x01, 0x07, 0x03, "1 Coin  2 Credits"			},
	{0x2b, 0x01, 0x07, 0x02, "1 Coin  3 Credits"			},
	{0x2b, 0x01, 0x07, 0x01, "1 Coin  5 Credits"			},
	{0x2b, 0x01, 0x07, 0x00, "1 Coin  6 Credits"			},

	{0   , 0xfe, 0   ,    2, "Continue Coin"				},
	{0x2b, 0x01, 0x08, 0x08, "Start 1 Coin/Continue 1 Coin"	},
	{0x2b, 0x01, 0x08, 0x00, "Start 2 Coin/Continue 1 Coin"	},

	{0   , 0xfe, 0   ,    2, "Lives"						},
	{0x2b, 0x01, 0x10, 0x00, "1"							},
	{0x2b, 0x01, 0x10, 0x10, "2"							},

	{0   , 0xfe, 0   ,    4, "Cabinet"						},
	{0x2b, 0x01, 0x60, 0x60, "2 Players"					},
	{0x2b, 0x01, 0x60, 0x40, "3 Players"					},
	{0x2b, 0x01, 0x60, 0x20, "4 Players"					},
	{0x2b, 0x01, 0x60, 0x00, "4 Players (Team)"				},

	{0   , 0xfe, 0   ,    2, "Coin Slot"					},
	{0x2b, 0x01, 0x80, 0x80, "Same"							},
	{0x2b, 0x01, 0x80, 0x00, "Individual"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x2c, 0x01, 0x01, 0x01, "Off"							},
	{0x2c, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Number of Enemies"			},
	{0x2c, 0x01, 0x06, 0x04, "Easy"							},
	{0x2c, 0x01, 0x06, 0x06, "Normal"						},
	{0x2c, 0x01, 0x06, 0x02, "Hard"							},
	{0x2c, 0x01, 0x06, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Strength of Enemies"			},
	{0x2c, 0x01, 0x18, 0x10, "Easy"							},
	{0x2c, 0x01, 0x18, 0x18, "Normal"						},
	{0x2c, 0x01, 0x18, 0x08, "Hard"							},
	{0x2c, 0x01, 0x18, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Freeze"						},
	{0x2c, 0x01, 0x20, 0x20, "Off"							},
	{0x2c, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x2c, 0x01, 0x40, 0x40, "Off"							},
	{0x2c, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x2c, 0x01, 0x80, 0x80, "Off"							},
	{0x2c, 0x01, 0x80, 0x00, "On"							},
};

STDDIPINFO(Karatblz)

static struct BurnDIPInfo TurbofrcDIPList[]=
{
	{0x18, 0xff, 0xff, 0x8f, NULL							},
	{0x19, 0xff, 0xff, 0xf9, NULL							},

	{0   , 0xfe, 0   ,    8, "Coinage"						},
	{0x18, 0x01, 0x07, 0x04, "4 Coins 1 Credits"			},
	{0x18, 0x01, 0x07, 0x05, "3 Coins 1 Credits"			},
	{0x18, 0x01, 0x07, 0x06, "2 Coins 1 Credits"			},
	{0x18, 0x01, 0x07, 0x07, "1 Coin  1 Credits"			},
	{0x18, 0x01, 0x07, 0x03, "1 Coin  2 Credits"			},
	{0x18, 0x01, 0x07, 0x02, "1 Coin  3 Credits"			},
	{0x18, 0x01, 0x07, 0x01, "1 Coin  5 Credits"			},
	{0x18, 0x01, 0x07, 0x00, "1 Coin  6 Credits"			},

	{0   , 0xfe, 0   ,    2, "Continue Coin"				},
	{0x18, 0x01, 0x08, 0x08, "Start 1 Coin/Continue 1 Coin"	},
	{0x18, 0x01, 0x08, 0x00, "Start 2 Coin/Continue 1 Coin"	},

	{0   , 0xfe, 0   ,    2, "Coin Slot"					},
	{0x18, 0x01, 0x10, 0x10, "Same"							},
	{0x18, 0x01, 0x10, 0x00, "Individual"					},

	{0   , 0xfe, 0   ,    2, "Play Mode"					},
	{0x18, 0x01, 0x20, 0x20, "2 Players"					},
	{0x18, 0x01, 0x20, 0x00, "3 Players"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x18, 0x01, 0x40, 0x40, "Off"							},
	{0x18, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x18, 0x01, 0x80, 0x80, "Off"							},
	{0x18, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x19, 0x01, 0x01, 0x01, "Off"							},
	{0x19, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    8, "Difficulty"					},
	{0x19, 0x01, 0x0e, 0x0e, "1 (Easiest)"					},
	{0x19, 0x01, 0x0e, 0x0c, "2"							},
	{0x19, 0x01, 0x0e, 0x0a, "3"							},
	{0x19, 0x01, 0x0e, 0x08, "4 (Normal)"					},
	{0x19, 0x01, 0x0e, 0x06, "5"							},
	{0x19, 0x01, 0x0e, 0x04, "6"							},
	{0x19, 0x01, 0x0e, 0x02, "7"							},
	{0x19, 0x01, 0x0e, 0x00, "8 (Hardest)"					},

	{0   , 0xfe, 0   ,    2, "Lives"						},
	{0x19, 0x01, 0x10, 0x00, "2"							},
	{0x19, 0x01, 0x10, 0x10, "3"							},

	{0   , 0xfe, 0   ,    2, "Bonus Life"					},
	{0x19, 0x01, 0x20, 0x20, "200000"						},
	{0x19, 0x01, 0x20, 0x00, "300000"						},
};

STDDIPINFO(Turbofrc)

static struct BurnDIPInfo AerofgtDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL							},
	{0x13, 0xff, 0xff, 0xfd, NULL							},
	{0x14, 0xff, 0xff, 0x00, NULL							},

	{0   , 0xfe, 0   ,    2, "Coin Slot"					},
	{0x12, 0x01, 0x01, 0x01, "Same"							},
	{0x12, 0x01, 0x01, 0x00, "Individual"					},

	{0   , 0xfe, 0   ,    8, "Coin A"						},
	{0x12, 0x01, 0x0e, 0x0a, "3 Coins 1 Credits"			},
	{0x12, 0x01, 0x0e, 0x0c, "2 Coins 1 Credits"			},
	{0x12, 0x01, 0x0e, 0x0e, "1 Coin  1 Credits"			},
	{0x12, 0x01, 0x0e, 0x08, "1 Coin  2 Credits"			},
	{0x12, 0x01, 0x0e, 0x06, "1 Coin  3 Credits"			},
	{0x12, 0x01, 0x0e, 0x04, "1 Coin  4 Credits"			},
	{0x12, 0x01, 0x0e, 0x02, "1 Coin  5 Credits"			},
	{0x12, 0x01, 0x0e, 0x00, "1 Coin  6 Credits"			},

	{0   , 0xfe, 0   ,    8, "Coin B"						},
	{0x12, 0x01, 0x70, 0x50, "3 Coins 1 Credits"			},
	{0x12, 0x01, 0x70, 0x60, "2 Coins 1 Credits"			},
	{0x12, 0x01, 0x70, 0x70, "1 Coin  1 Credits"			},
	{0x12, 0x01, 0x70, 0x40, "1 Coin  2 Credits"			},
	{0x12, 0x01, 0x70, 0x30, "1 Coin  3 Credits"			},
	{0x12, 0x01, 0x70, 0x20, "1 Coin  4 Credits"			},
	{0x12, 0x01, 0x70, 0x10, "1 Coin  5 Credits"			},
	{0x12, 0x01, 0x70, 0x00, "1 Coin  6 Credits"			},

	{0   , 0xfe, 0   ,    2, "Continue Coin"				},
	{0x12, 0x01, 0x80, 0x80, "Start 1 Coin/Continue 1 Coin"	},
	{0x12, 0x01, 0x80, 0x00, "Start 2 Coin/Continue 1 Coin"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x13, 0x01, 0x01, 0x01, "Off"							},
	{0x13, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x13, 0x01, 0x02, 0x02, "Off"							},
	{0x13, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x13, 0x01, 0x0c, 0x08, "Easy"							},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"						},
	{0x13, 0x01, 0x0c, 0x04, "Hard"							},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x13, 0x01, 0x30, 0x20, "1"							},
	{0x13, 0x01, 0x30, 0x10, "2"							},
	{0x13, 0x01, 0x30, 0x30, "3"							},
	{0x13, 0x01, 0x30, 0x00, "4"							},

	{0   , 0xfe, 0   ,    2, "Bonus Life"					},
	{0x13, 0x01, 0x40, 0x40, "200000"						},
	{0x13, 0x01, 0x40, 0x00, "300000"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x13, 0x01, 0x80, 0x80, "Off"							},
	{0x13, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    5, "Region"						},
	{0x14, 0x01, 0x0f, 0x00, "Any"							},
	{0x14, 0x01, 0x0f, 0x0f, "USA/Canada"					},
	{0x14, 0x01, 0x0f, 0x0e, "Korea"						},
	{0x14, 0x01, 0x0f, 0x0d, "Hong Kong"					},
	{0x14, 0x01, 0x0f, 0x0b, "Taiwan"						},
};

STDDIPINFO(Aerofgt)

static struct BurnDIPInfo AerofgtbDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL							},
	{0x13, 0xff, 0xff, 0xfd, NULL							},
	{0x14, 0xff, 0xff, 0x00, NULL							},

	{0   , 0xfe, 0   ,    2, "Coin Slot"					},
	{0x12, 0x01, 0x01, 0x01, "Same"							},
	{0x12, 0x01, 0x01, 0x00, "Individual"					},

	{0   , 0xfe, 0   ,    8, "Coin A"						},
	{0x12, 0x01, 0x0e, 0x0a, "3 Coins 1 Credits"			},
	{0x12, 0x01, 0x0e, 0x0c, "2 Coins 1 Credits"			},
	{0x12, 0x01, 0x0e, 0x0e, "1 Coin  1 Credits"			},
	{0x12, 0x01, 0x0e, 0x08, "1 Coin  2 Credits"			},
	{0x12, 0x01, 0x0e, 0x06, "1 Coin  3 Credits"			},
	{0x12, 0x01, 0x0e, 0x04, "1 Coin  4 Credits"			},
	{0x12, 0x01, 0x0e, 0x02, "1 Coin  5 Credits"			},
	{0x12, 0x01, 0x0e, 0x00, "1 Coin  6 Credits"			},

	{0   , 0xfe, 0   ,    8, "Coin B"						},
	{0x12, 0x01, 0x70, 0x50, "3 Coins 1 Credits"			},
	{0x12, 0x01, 0x70, 0x60, "2 Coins 1 Credits"			},
	{0x12, 0x01, 0x70, 0x70, "1 Coin  1 Credits"			},
	{0x12, 0x01, 0x70, 0x40, "1 Coin  2 Credits"			},
	{0x12, 0x01, 0x70, 0x30, "1 Coin  3 Credits"			},
	{0x12, 0x01, 0x70, 0x20, "1 Coin  4 Credits"			},
	{0x12, 0x01, 0x70, 0x10, "1 Coin  5 Credits"			},
	{0x12, 0x01, 0x70, 0x00, "1 Coin  6 Credits"			},

	{0   , 0xfe, 0   ,    2, "Continue Coin"				},
	{0x12, 0x01, 0x80, 0x80, "Start 1 Coin/Continue 1 Coin"	},
	{0x12, 0x01, 0x80, 0x00, "Start 2 Coin/Continue 1 Coin"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x13, 0x01, 0x01, 0x01, "Off"							},
	{0x13, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x13, 0x01, 0x02, 0x02, "Off"							},
	{0x13, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x13, 0x01, 0x0c, 0x08, "Easy"							},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"						},
	{0x13, 0x01, 0x0c, 0x04, "Hard"							},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x13, 0x01, 0x30, 0x20, "1"							},
	{0x13, 0x01, 0x30, 0x10, "2"							},
	{0x13, 0x01, 0x30, 0x30, "3"							},
	{0x13, 0x01, 0x30, 0x00, "4"							},

	{0   , 0xfe, 0   ,    2, "Bonus Life"					},
	{0x13, 0x01, 0x40, 0x40, "200000"						},
	{0x13, 0x01, 0x40, 0x00, "300000"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x13, 0x01, 0x80, 0x80, "Off"							},
	{0x13, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Region"						},
	{0x14, 0x01, 0x01, 0x00, "Japan"						},
	{0x14, 0x01, 0x01, 0x01, "Taiwan"						},
};

STDDIPINFO(Aerofgtb)

#ifdef USE_GGA
#define WRITE_GGA_REGS()									\
	if ((address & 0x0ffffc) == 0x0ff400) {					\
		if (address & 2) gga_address_latch = data & 0x0f;	\
		else gga_regs[gga_address_latch] = data & 0xff;		\
		return;												\
	}
#else
#define WRITE_GGA_REGS()									\
	if ((address & 0x0ffffc) == 0x0ff400) {					\
		return;												\
	}
#endif

static void sync_z80()
{
	switch (sound_type)
	{
		case Z80_YM2610:
		case Z80_YM2151:
		case Z80_YM3812_MSM6295:
		case Z80_YM3812_UPD7759:
			{
				INT32 sek = ((UINT64)10000000 * nBurnCPUSpeedAdjust) / 256;
				INT32 zet = zet_frequency;
				INT32 cyc = (UINT64)SekTotalCycles() * zet / sek;
//				bprintf(0, _T("snd sync  %d\n"), cyc - ZetTotalCycles());
				BurnTimerUpdate(cyc);
			}
			break;
	}
}

static void __fastcall pspikes_main_write_word(UINT32 address, UINT16 data)
{
	WRITE_GGA_REGS()

	switch (address)
	{
		case 0xfff004:
			scrolly[0] = data;
		return;

		case 0xfff00e: // wbbc97
			bitmap_enable = data;
		return;
	}
}

static void __fastcall pspikes_main_write_byte(UINT32 address, UINT8 data)
{
	WRITE_GGA_REGS()

	switch (address)
	{
		case 0xfff001:
			spritepalettebank = data & 0x03;
			charpalettebank = (data & 0x1c) >> 2;
			flipscreen = (data & 0x80);
		return;

		case 0xfff003:
			gfxbank[0] = data >> 4;
			gfxbank[1] = data & 0x0f;
		return;

		case 0xfff005:
			scrolly[0] = data;
		return;

		case 0xfff007:
			sync_z80();
			soundlatch[0] = data & 0xff;
			soundlatch[1] = 1;
			ZetNmi();
		return;
	}
}

static UINT8 __fastcall pspikes_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xfff000:
			return DrvInputs[0] >> 8;

		case 0xfff001:
			return DrvInputs[0];

		case 0xfff002:
			return DrvInputs[1] >> 8;

		case 0xfff003:
			return DrvInputs[1];

		case 0xfff004:
			return DrvDips[1];

		case 0xfff005:
			return DrvDips[0];

		case 0xfff007:
			sync_z80();
			return soundlatch[1];
	}

	bprintf (0, _T("MRB: %5.5x\n"), address);

	return 0;
}

static UINT16 __fastcall pspikes_main_read_word(UINT32 address)
{
	//bprintf (0, _T("MRW: %5.5x\n"), address);
	return (pspikes_main_read_byte(address + 0) << 8) |
		   (pspikes_main_read_byte(address + 1) << 0);
}

static void set_okibank(INT32 data)
{
	MSM6295SetBank(0, DrvSndROM[0] + data * 0x20000, 0x20000, 0x3ffff);
}

static void __fastcall pspikesb_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0xc04000:
		return; // nop

		case 0xfff007:
			MSM6295Write(0, data);
		return;

		case 0xfff009:
			set_okibank(data & 3);
		return;
	}

	pspikes_main_write_byte(address, data);
}

static UINT8 __fastcall pspikesb_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xfff007:
			return MSM6295Read(0);
	}

	return pspikes_main_read_byte(address);
}

static void __fastcall spinlbrk_main_write_word(UINT32 address, UINT16 data)
{
	WRITE_GGA_REGS()

	switch (address)
	{
		case 0xfff000:
			gfxbank[0] =(data & 0x0007) >>0;
			gfxbank[1] =(data & 0x0038) >> 3;
			flipscreen = data & 0x8000;
		return;

		case 0xfff002:
			scrollx[1] = data;
		return;
	}
}

static void __fastcall spinlbrk_main_write_byte(UINT32 address, UINT8 data)
{
	WRITE_GGA_REGS()

	switch (address)
	{
		case 0xfff000:
			gfxbank[0] = data & 0x07;
			gfxbank[1] =(data & 0x38) >> 3;
		return;

		case 0xfff001:
			flipscreen = data & 0x80;
		return;

		case 0xfff003:
			scrollx[1] = data;
		return;

		case 0xfff007:
			sync_z80();
			soundlatch[0] = data & 0xff;
			soundlatch[1] = 1;
			ZetNmi();
		return;
	}
}

static void __fastcall karatblz_main_write_word(UINT32 address, UINT16 data)
{
	WRITE_GGA_REGS()

	switch (address)
	{
		case 0xff000:
			spritepalettebank = data & 0x03;
			charpalettebank = (data & 0x1c) >> 2;
			flipscreen = (data & 0x80);
		return;

		case 0xff002:
			gfxbank[0] = (data & 0x01);
			gfxbank[1] = (data & 0x08) >> 3;
		return;

		case 0xff006:
			sync_z80();
			soundlatch[0] = data & 0xff;
			soundlatch[1] = 1;
			ZetNmi();
		return;

		case 0xff008:
			scrollx[0] = data;
		return;

		case 0xff00a:
			scrolly[0] = data;
		return;

		case 0xff00c:
			scrollx[1] = data;
		return;

		case 0xff00e:
			scrolly[1] = data;
		return;
	}
}

static void __fastcall karatblz_main_write_byte(UINT32 address, UINT8 data)
{
	WRITE_GGA_REGS()

	switch (address)
	{
		case 0xff000:
			spritepalettebank = data & 0x03;
			charpalettebank = (data & 0x1c) >> 2;
			flipscreen = (data & 0x80);
		return;

		case 0xff002:
			gfxbank[0] = (data & 0x01);
			gfxbank[1] = (data & 0x08) >> 3;
		return;

		case 0xff007:
			sync_z80();
			soundlatch[0] = data & 0xff;
			soundlatch[1] = 1;
			ZetNmi();
		return;
	}

	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall karatblz_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xff000:
			return DrvInputs[0];

		case 0xff002:
			return DrvInputs[1];

		case 0xff004:
			return DrvInputs[2];

		case 0xff006:
			return DrvInputs[3];

		case 0xff008:
			return DrvDips[0] + (DrvDips[1] * 256);

		case 0xff00a:
			sync_z80();
			return soundlatch[1];
	}

	bprintf (0, _T("MRW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall karatblz_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xff000:
			return DrvInputs[0] >> 8;

		case 0xff001:
			return DrvInputs[0];

		case 0xff002:
			return DrvInputs[1] >> 8;

		case 0xff003:
			return DrvInputs[1];

		case 0xff004:
			return DrvInputs[2] >> 8;

		case 0xff005:
			return DrvInputs[2];

		case 0xff006:
			return DrvInputs[3] >> 8;

		case 0xff007:
			return DrvInputs[3];

		case 0xff008:
			return DrvDips[1];

		case 0xff009:
			return DrvDips[0];

		case 0xff00b:
			sync_z80();
			return soundlatch[1];
	}

	bprintf (0, _T("MRB: %5.5x\n"), address);

	return 0;
}

static void __fastcall turbofrc_main_write_word(UINT32 address, UINT16 data)
{
	address |= 0x1000; // aerofgtb

	WRITE_GGA_REGS()

	switch (address)
	{
		case 0xff000:
			flipscreen = data & 0x80;
		return;

		case 0xff002:
			scrolly[0] = data;
		return;

		case 0xff004:
			scrollx[1] = data;
		return;

		case 0xff006:
			scrolly[1] = data;
		return;

		case 0xff008:
		case 0xff00a:
			gfxbank[(address & 2) * 2 + 0] = (data >> 0) & 0xf;
			gfxbank[(address & 2) * 2 + 1] = (data >> 4) & 0xf;
			gfxbank[(address & 2) * 2 + 2] = (data >> 8) & 0xf;
			gfxbank[(address & 2) * 2 + 3] = (data >> 12) & 0xf;
		return;

		case 0xff00c:
		return; // nop

		case 0xfff00e:
			sync_z80();
			soundlatch[0] = data & 0xff;
			soundlatch[1] = 1;
			ZetNmi();
		return;
	}
}

static void __fastcall turbofrc_main_write_byte(UINT32 address, UINT8 data)
{
	address |= 0x1000; // aerofgtb

	WRITE_GGA_REGS()

	switch (address)
	{
		case 0xff000:
		case 0xff001:
			flipscreen = data & 0x80;
		return;

		case 0xff002:
			scrolly[0] = data;
		return;

		case 0xff004:
			scrollx[1] = data;
		return;

		case 0xff006:
			scrolly[1] = data;
		return;

		case 0xff008:
		case 0xff009:
		case 0xff00a:
		case 0xfff0b:
			bprintf(0, _T("turbofrc_gfxbank?  %x %x\n"), address, data);
		return;

		case 0xff00c:
		case 0xff00d:
		return; // nop

		case 0xff00e:
		case 0xff00f:
			sync_z80();
			soundlatch[0] = data & 0xff;
			soundlatch[1] = 1;
			ZetNmi();
		return;
	}
}

static UINT16 __fastcall turbofrc_main_read_word(UINT32 address)
{
	address |= 0x1000; // aerofgtb

	switch (address)
	{
		case 0xff000:
			return DrvInputs[0];

		case 0xff002:
			return DrvInputs[1];

		case 0xff004:
			return (DrvDips[0] + (DrvDips[1] * 256));

		case 0xff006:
			sync_z80();
			return soundlatch[1];

		case 0xff008:
			return DrvInputs[2];
	}

	return 0;
}

static UINT8 __fastcall turbofrc_main_read_byte(UINT32 address)
{
	address |= 0x1000; // aerofgtb

	switch (address)
	{
		case 0xff000:
			return DrvInputs[0] >> 8;

		case 0xff001:
			return DrvInputs[0];

		case 0xff002:
			return DrvInputs[1] >> 8;

		case 0xff003:
			return DrvInputs[1];

		case 0xff004:
			return DrvDips[1];

		case 0xff005:
			return DrvDips[0];

		case 0xff006:
			sync_z80();
			return soundlatch[1];

		case 0xff008:
			return DrvInputs[2] >> 8;

		case 0xff009:
			return DrvInputs[2];
	}

	return 0;
}

static UINT16 __fastcall aerofgtb_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xfe000:
			return DrvInputs[0];

		case 0xfe002:
			return DrvInputs[1];

		case 0xfe004:
			return (DrvDips[0] + (DrvDips[1] * 256));

		case 0xfe006:
			return soundlatch[1];

		case 0xfe008:
			return DrvDips[2];
	}

	return 0;
}

static UINT8 __fastcall aerofgtb_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xfe000:
			return DrvInputs[0] >> 8;

		case 0xfe001:
			return DrvInputs[0];

		case 0xfe002:
			return DrvInputs[1] >> 8;

		case 0xfe003:
			return DrvInputs[1];

		case 0xfe004:
			return DrvDips[1];

		case 0xfe005:
			return DrvDips[0];

		case 0xfe006:
			return soundlatch[1];

		case 0xfe008:
			return 0;

		case 0xfe009:
			return DrvDips[2];
	}

	return 0;
}

static void __fastcall aerfboot_main_write_word(UINT32 address, UINT16 data)
{
	switch (address & 0xffffe)
	{
		case 0xfe002:
		case 0xfe003:
			scrolly[0] = data;
		return;

		case 0xfe004:
		case 0xfe005:
			scrollx[1] = data;
		return;

		case 0xfe006:
		case 0xfe007:
			scrolly[1] = data;
		return;

		case 0xfe008:
		case 0xfe00a:
			gfxbank[(address & 2) * 2 + 0] = (data >>  0) & 0xf;
			gfxbank[(address & 2) * 2 + 1] = (data >>  4) & 0xf;
			gfxbank[(address & 2) * 2 + 2] = (data >>  8) & 0xf;
			gfxbank[(address & 2) * 2 + 3] = (data >> 12) & 0xf;
		return;

		case 0xfe00e:
			soundlatch[0] = (data >> 8) & 0xff;
			soundlatch[1] = 1;
			ZetNmi();
		return;
	}
	if (!(address >= 0x10c000 && address <= 0x117fff) && address != 0xfe00c) {
		bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
	}
}

static void __fastcall aerfboot_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address & 0xfffff)
	{
		case 0xfe00e:
			soundlatch[0] = data & 0xff;
			soundlatch[1] = 1;
			ZetNmi();
		return;
	}

	bprintf (0, _T("WB: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall aerofgt_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffff8) == 0xffff80) {
		address = (address & 6);
		gfxbank[address + 0] = data >> 8;
		gfxbank[address + 1] = data & 0xf;
		return;
	}

	if ((address & 0xffffe0) == 0xffffa0) {
		// vs9209 write
		return;
	}

	switch (address)
	{
		case 0xffff88:
			scrolly[0] = data;
		return;

		case 0xffff90:
			scrolly[1] = data;
		return;

		case 0xffffc0:
			sync_z80();
			soundlatch[0] = data;
			soundlatch[1] = 1;
			ZetNmi();
		return;
	}
}

static void __fastcall aerofgt_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffff8) == 0xffff80) {
		gfxbank[address & 7] = data;
		return;
	}

	if ((address & 0xffffe0) == 0xffffa0) {
		// vs9209 write
		return;
	}

	switch (address)
	{
		case 0xffff89:
			scrolly[0] = data;
		return;

		case 0xffff91:
			scrolly[1] = data;
		return;

		case 0xffffc1:
			sync_z80();
			soundlatch[0] = data;
			soundlatch[1] = 1;
			ZetNmi();
		return;
	}
}

static UINT16 __fastcall aerofgt_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xffffa0:
			return DrvInputs[0]; // p1

		case 0xffffa2:
			return DrvInputs[1]; // p2

		case 0xffffa4:
			return DrvInputs[2]; // system

		case 0xffffa6:
			return DrvDips[0]; // dsw1

		case 0xffffa8:
			return DrvDips[1]; // dsw2

		case 0xffffac:
			return soundlatch[1];

		case 0xffffae:
			return DrvDips[2]; // jp1
	}

	return 0;
}

static UINT8 __fastcall aerofgt_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xffffa0:
			return DrvInputs[0] >> 8; // p1

		case 0xffffa1:
			return DrvInputs[0]; // p1

		case 0xffffa2:
			return DrvInputs[1] >> 8; // p2

		case 0xffffa3:
			return DrvInputs[1]; // p2

		case 0xffffa4:
			return DrvInputs[2] >> 8; // system

		case 0xffffa5:
			return DrvInputs[2]; // system

		case 0xffffa7:
			return DrvDips[0]; // dsw1

		case 0xffffa9:
			return DrvDips[1]; // dsw2

		case 0xffffad:
			return soundlatch[1];

		case 0xffffaf:
			return DrvDips[2]; // jp1
	}

	bprintf(0, _T("aerofgt mrb %x \n"), address);

	return 0;
}

static void __fastcall kickball_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0xfff000:
			spritepalettebank = data & 0x03;
			charpalettebank = (data & 0x1c) >> 2;
			flipscreen = data & 0x80;
		return;

		case 0xfff002:
			gfxbank[0] = (data & 0xe);
			gfxbank[1] = (data & 0xf);
		return;

		case 0xfff004:
			scrolly[0] = data;
		return;

		case 0xfff006:
			soundlatch[0] = data & 0xff;
			soundlatch[1] = 1;
			ZetNmi();
		return;
	}
}

static void __fastcall kickball_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0xfff001:
			spritepalettebank = data & 0x03;
			charpalettebank = (data & 0x1c) >> 2;
			flipscreen = data & 0x80;
		return;

		case 0xfff003:
			gfxbank[0] = (data & 0xe);
			gfxbank[1] = (data & 0xf);
		return;

		case 0xfff007:
			soundlatch[0] = data & 0xff;
			soundlatch[1] = 1;
			ZetNmi();
		return;
	}
}

static UINT16 __fastcall kickball_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xfff000:
			return DrvInputs[0];

		case 0xfff002:
			return DrvInputs[1];

		case 0xfff004:
			return DrvDips[0] + (DrvDips[1] * 256);

		case 0xfff006:
		case 0xfff007:
			return soundlatch[1];
	}

	return 0;
}

static UINT8 __fastcall kickball_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xfff000:
			return DrvInputs[0] >> 8;

		case 0xfff001:
			return DrvInputs[0];

		case 0xfff002:
			return DrvInputs[1] >> 8;

		case 0xfff003:
			return DrvInputs[1];

		case 0xfff004:
			return DrvDips[1];

		case 0xfff005:
			return DrvDips[0];

		case 0xfff006:
		case 0xfff007:
			return soundlatch[1];
	}

	return 0;
}

static void __fastcall aerfboo2_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0xfe002:
			scrolly[0] = data;
		return;

		case 0xfe004:
			scrollx[1] = data;
		return;

		case 0xfe006:
			scrolly[1] = data;
		return;

		case 0xfe008:
		case 0xfe00a:
			gfxbank[(address & 2) * 2 + 0] = (data >>  0) & 0xf;
			gfxbank[(address & 2) * 2 + 1] = (data >>  4) & 0xf;
			gfxbank[(address & 2) * 2 + 2] = (data >>  8) & 0xf;
			gfxbank[(address & 2) * 2 + 3] = (data >> 12) & 0xf;
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall aerfboo2_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0xfe00e:
			MSM6295Write(0, data & 0xff);
		return;

		case 0xfe01e:
		case 0xfe01f:
		//	set_okibank(data);	// not emulated yet
		return;
	}

	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall aerfboo2_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xfe000:
			return DrvInputs[0];

		case 0xfe002:
			return DrvInputs[1];

		case 0xfe004:
			return (DrvDips[0] + (DrvDips[1] * 256));

		case 0xfe006:
			return MSM6295Read(0);

		case 0xfe008:
			return DrvDips[2];
	}

	return 0;
}

static UINT8 __fastcall aerfboo2_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xfe000:
			return DrvInputs[0] >> 8;

		case 0xfe001:
			return DrvInputs[0];

		case 0xfe002:
			return DrvInputs[1] >> 8;

		case 0xfe003:
			return DrvInputs[1];

		case 0xfe004:
			return DrvDips[1];

		case 0xfe005:
			return DrvDips[0];

		case 0xfe006:
			return MSM6295Read(0);

		case 0xfe008:
			return 0;

		case 0xfe009:
			return DrvDips[2];
	}

	return 0;
}

static void __fastcall spikes91_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0xfff002:
		case 0xfff003:
			gfxbank[0] = (data >> 12);
			gfxbank[1] = (data >> 8) & 0xf;
		return;

		case 0xfff004:
		case 0xfff005:
			scrolly[0] = data;
		return;

		case 0xfff006:
		case 0xfff007:
			soundlatch[0] = data & 0xff;
			soundlatch[1] = 1;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0xfff008:
		case 0xfff009:
			spikes91_lookup = data & 1;
		return;
	}
}

static void __fastcall spikes91_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0xfff002:
		case 0xfff003:
			gfxbank[0] = (data >> 4);
			gfxbank[1] = (data & 0xf);
		return;

		case 0xfff004:
		case 0xfff005:
			scrolly[0] = data;
		return;

		case 0xfff006:
		case 0xfff007:
			soundlatch[0] = data & 0xff;
			soundlatch[1] = 1;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			return;

		case 0xfff008:
		case 0xfff009:
			spikes91_lookup = data & 1;
		return;
	}
}

static inline void palette_update(INT32 offset) // 5rgb
{
	UINT16 p = *((UINT16*)(DrvPalRAM + offset));

	INT32 r = (p >> 10) & 0x1f;
	INT32 g = (p >>  5) & 0x1f;
	INT32 b = (p >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r,g,b,0);
}

static void __fastcall palette_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvPalRAM + (address & 0xffe))) = data;
	palette_update(address & 0xffe);
}

static void __fastcall palette_write_byte(UINT32 address, UINT8 data)
{
	DrvPalRAM[(address & 0xfff) ^ 1] = data;
	palette_update(address & 0xffe);
}

static void install_palette_handler(UINT32 start_address, UINT32 end_address)
{
	SekMapHandler(1,				start_address, end_address, MAP_WRITE);
	SekSetWriteWordHandler(1,		palette_write_word);
	SekSetWriteByteHandler(1,		palette_write_byte);
}

static void sound_bankswitch(INT32 data)
{
	bankdata = data & 3;
//	bprintf(0, _T("sound bank  %x\n"), data);
	ZetMapMemory(DrvZ80ROM + (bankdata + 1) * 0x8000, 0x8000, 0xffff, MAP_ROM);
}

static void __fastcall standard_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			sound_bankswitch(data);
		return;

		case 0x14:
			soundlatch[1] = 0;
		return;

		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
			BurnYM2610Write(port & 3, data);
		return;
	}
}

static UINT8 __fastcall standard_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x14:
			return soundlatch[0];

		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
			return BurnYM2610Read(port & 3);
	}

	return 0;
}

static void __fastcall aerofgt_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			BurnYM2610Write(port & 3, data);
		return;

		case 0x04:
			sound_bankswitch(data);
		return;

		case 0x08:
			soundlatch[1] = 0;
		return;
	}
}

static UINT8 __fastcall aerofgt_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			return BurnYM2610Read(port & 3);

		case 0x0c:
			return soundlatch[0];
	}

	return 0;
}

static UINT8 __fastcall kickball_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			return soundlatch[0];
	}

	return 0;
}

static void __fastcall kickball_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x20:
			BurnYM3812Write(0, (port / 0x20) & 1, data);
		return;

		case 0x40:
			MSM6295Write(0, data);
		return;

		case 0xc0:
			soundlatch[1] = 0;
		return;
	}
}

static UINT8 __fastcall kickball_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return BurnYM3812Read(0, 0);

		case 0x40:
			return MSM6295Read(0);
	}

	return 0;
}

static void __fastcall wbbc97_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf800:
			MSM6295Write(0, data);
		return;

		case 0xf810:
		case 0xf811:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0xfc00:
		return; // nop
	}
}

static UINT8 __fastcall wbbc97_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			return MSM6295Read(0);

		case 0xfc20:
			soundlatch[1] = 0;
			return soundlatch[0];
	}

	return 0;
}

static void __fastcall spikes91_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
		case 0xe001:
			BurnYM2151Write(address & 1, data);
		return;
	}
}

static UINT8 __fastcall spikes91_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
		case 0xe001:
			return BurnYM2151Read();

		case 0xe800:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			soundlatch[1] = 0;
			return soundlatch[0];
	}

	return 0;
}

static void __fastcall aerfboot_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9000:
			if (data & 0x04) {
				set_okibank(data & 0x03);
			}
		return;

		case 0x9800:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 __fastcall aerfboot_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x9800:
			return MSM6295Read(0);

		case 0xa000:
			soundlatch[1] = 0;
			return soundlatch[0];
	}

	return 0;
}

static UINT8 __fastcall karatblzbl_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			soundlatch[1] = 0;
			return soundlatch[0];
	}

	return 0;
}

static void __fastcall karatblzbl_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x20:
			BurnYM3812Write(0, (port / 0x20) & 1, data);
		return;

		case 0x40:
			UPD7759PortWrite(0, data);
			UPD7759StartWrite(0, 0);
			UPD7759StartWrite(0, 1);
		return;

		case 0x80:
			UPD7759ResetWrite(0, data & 0x80);
		return;
	}
}

static UINT8 __fastcall karatblzbl_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return BurnYM3812Read(0,0);
	}

	return 0;
}

static tilemap_callback( spinlbrk_bg )
{
	INT32 attr = *((UINT16*)(DrvWRAM[0] + offs * 2));

	TILE_SET_INFO(0, (attr & 0xfff) | (gfxbank[0] << 12), attr >> 12, 0);
}

static tilemap_callback( karatblz_bg )
{
	INT32 attr = *((UINT16*)(DrvWRAM[0] + offs * 2));

	TILE_SET_INFO(0, (attr & 0x1fff) | (gfxbank[0] << 13), attr >> 13, 0);
}

static tilemap_callback( karatblz_fg )
{
	INT32 attr = *((UINT16*)(DrvWRAM[1] + offs * 2));

	TILE_SET_INFO(1, (attr & 0x1fff) | (gfxbank[1] << 13), attr >> 13, 0);
}

static tilemap_callback( pspikes_bg )
{
	INT32 attr = *((UINT16*)(DrvWRAM[0] + offs * 2));

	TILE_SET_INFO(0, (attr & 0xfff) + (gfxbank[(attr >> 12) & 1] << 12), (attr >> 13) + (charpalettebank << 3), 0);
}

static tilemap_callback( aerofgt_bg )
{
	INT32 attr = *((UINT16*)(DrvWRAM[0] + offs * 2));

	TILE_SET_INFO(0, (attr & 0x7ff) + (gfxbank[((attr & 0x1800) >> 11) + 0] << 11), attr >> 13, 0);
}

static tilemap_callback( aerofgt_fg )
{
	INT32 attr = *((UINT16*)(DrvWRAM[1] + offs * 2));

	TILE_SET_INFO(1, (attr & 0x7ff) + (gfxbank[((attr & 0x1800) >> 11) + 4] << 11), attr >> 13, 0);
}

static tilemap_callback( spikes91_tx )
{
	INT32 attr = *((UINT16*)(DrvWRAM[1] + offs * 2));

	TILE_SET_INFO(0, attr & 0x1fff, attr >> 13, 0);
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	switch (sound_type)
	{
		case Z80_YM2610:
			ZetOpen(0);
			ZetReset();
			sound_bankswitch(0); // spinalbrk
			BurnYM2610Reset();
			ZetClose();
		break;

		case Z80_MSM6295:
			ZetReset(0);
			MSM6295Reset(0);
		break;

		case NOZ80_MSM6295:
			MSM6295Reset(0);
		break;

		case Z80_YM3812_MSM6295:
			ZetOpen(0);
			ZetReset();
			BurnYM3812Reset();
			ZetClose();
			MSM6295Reset(0);
		break;

		case Z80_YM3812_UPD7759:
			ZetOpen(0);
			ZetReset(0);
			BurnYM3812Reset();
			ZetClose();
			UPD7759Reset();
		break;

		case Z80_YM2151:
			ZetOpen(0);
			ZetReset();
			BurnYM2151Reset();
			ZetClose();
		break;
	}

	memset (gfxbank,	0, sizeof(gfxbank));
	memset (soundlatch,	0, sizeof(soundlatch));
	memset (scrollx,	0, sizeof(scrollx));
	memset (scrolly,	0, sizeof(scrolly));

	spritepalettebank	= 0;
	charpalettebank		= 0;
	flipscreen			= 0;
	bitmap_enable		= 0;
	spikes91_lookup		= 0;

#ifdef USE_GGA
	memset (gga_regs,	0, sizeof(gga_regs));
	gga_address_latch	= 0;
#endif

	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex(INT32 gfxlen0, INT32 gfxlen1, INT32 gfxlen2, INT32 gfxlen3, INT32 sndrom0, INT32 sndrom1, INT32 bitmap_ram)
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x400000;
	DrvZ80ROM			= Next; Next += 0x030000;

	DrvGfxROM[0]		= Next; Next += gfxlen0;
	DrvGfxROM[1]		= Next; Next += gfxlen1;
	DrvGfxROM[2]		= Next; Next += gfxlen2;
	DrvGfxROM[3]		= Next; Next += gfxlen3;
	DrvGfxROM[4]		= Next; Next += 0x20000;

	DrvSndROM[1]		= Next; Next += sndrom1;
	MSM6295ROM			= Next;
	DrvSndROM[0]		= Next; Next += (sndrom0 < 0x40000) ? 0x40000 : sndrom0;

	AllRam				= Next;

	Drv68KRAM			= Next; Next += 0x010000;
	DrvExtraRAM			= Next; Next += 0x010000;
	DrvPalRAM			= Next; Next += 0x001000;
	DrvWRAM[0]			= Next; Next += 0x002000;
	DrvWRAM[1]			= Next; Next += 0x002000;
	DrvSprRAM			= Next; Next += 0x004000;
	DrvSprLutRAM[0]		= Next; Next += 0x010000;
	DrvSprLutRAM[1]		= Next; Next += 0x010000;
	DrvRasterRAM		= Next; Next += 0x001000;
	DrvBitmapRAM		= Next; Next += (bitmap_ram) ? 0x040000 : 0x000000;

	DrvZ80RAM			= Next; Next += 0x000800;

	RamEnd				= Next;

	DrvPalette			= (UINT32*)Next; Next += 0x8800 * sizeof(UINT32);

	MemEnd				= Next;

	return 0;
}

static void standard_sound_system(INT32 aerofgt, INT32 soundlen0, INT32 soundlen1)
{
	sound_type = Z80_YM2610;
	zet_frequency = 5000000;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x77ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x7800, 0x7fff, MAP_RAM);
	ZetSetOutHandler(aerofgt ? aerofgt_sound_write_port : standard_sound_write_port);
	ZetSetInHandler(aerofgt ? aerofgt_sound_read_port : standard_sound_read_port);
	ZetClose();

	BurnYM2610Init(8000000, DrvSndROM[1], &soundlen1, DrvSndROM[0], &soundlen0, &DrvFMIRQHandler, 0);
	BurnTimerAttachZet(zet_frequency);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE,   0.25, BURN_SND_ROUTE_BOTH);
}

static INT32 PspikesInit()
{
	AllMem = NULL;
	MemIndex(0x100000, 0x200000, 0, 0, 0x040000, 0x100000, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x100000, 0x200000, 0, 0, 0x040000, 0x100000, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM     + 0x000000, k++, 1)) return 1;
		memmove (DrvZ80ROM + 0x08000, DrvZ80ROM, 0x20000);

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[1] + 0x000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x000002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvSndROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1]  + 0x000000, k++, 1)) return 1;

		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x080000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[1], NULL, 0x100000, 1, 0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x100000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x200000, 0x203fff, MAP_RAM);
	SekMapMemory(DrvWRAM[0],		0xff8000, 0xff8fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0xffc000, 0xffcfff, MAP_RAM);
	SekMapMemory(DrvRasterRAM,		0xffd000, 0xffdfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xffe000, 0xffefff, MAP_RAM);
	SekSetWriteWordHandler(0,		pspikes_main_write_word);
	SekSetWriteByteHandler(0,		pspikes_main_write_byte);
	SekSetReadWordHandler(0,		pspikes_main_read_word);
	SekSetReadByteHandler(0,		pspikes_main_read_byte);

	install_palette_handler(0xffe000, 0xffefff);
	SekClose();

	standard_sound_system(0, 0x040000, 0x100000);
	// pspikes needs more volume!
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.80, BURN_SND_ROUTE_LEFT);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.80, BURN_SND_ROUTE_RIGHT);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, pspikes_bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x100000, 0x000, 0x3f);
	GenericTilemapSetGfx(2, DrvGfxROM[1], 4, 16, 16, 0x200000, 0x400, 0x3f);
	GenericTilemapSetScrollRows(0, 256);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -4, 0);

	DrvDoReset();

	return 0;
}

static INT32 Spikes91GfxDecode()
{
	INT32 Plane0[4]  = { STEP4(0, 0x20000*8) };
	INT32 Plane1[4]  = { STEP4(0x40000*8*3, -(0x40000*8)) };
	INT32 XOffs[16] = { STEP16(0,1) };
	INT32 YOffs0[8] = { STEP8(0,8) };
	INT32 YOffs1[16]  = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x100000);

	GfxDecode(0x4000, 4,  8,  8, Plane0, XOffs, YOffs0, 0x040, tmp, DrvGfxROM[0]);

	for (INT32 i = 0; i < 0x100000; i++) tmp[i] = DrvGfxROM[1][i] ^ 0xff;

	GfxDecode(0x2000, 4, 16, 16, Plane1, XOffs, YOffs1, 0x100, tmp, DrvGfxROM[1]);

	BurnFree (tmp);

	return 0;
}

static INT32 Spikes91Init()
{
	AllMem = NULL;
	MemIndex(0x100000, 0x200000, 0, 0, 0, 0, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x100000, 0x200000, 0, 0, 0, 0, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x000001, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x060000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x0c0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[4]  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[4]  + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM     + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM     + 0x010000, k++, 1)) return 1;

		Spikes91GfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x100000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x200000, 0x203fff, MAP_RAM);
	SekMapMemory(DrvWRAM[0],		0xff8000, 0xff8fff, MAP_RAM);
	SekMapMemory(DrvWRAM[1],		0xffa000, 0xffbfff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0xffc000, 0xffcfff, MAP_RAM);
	SekMapMemory(DrvRasterRAM,		0xffd000, 0xffdfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xffe000, 0xffefff, MAP_RAM);
	SekSetWriteWordHandler(0,		spikes91_main_write_word);
	SekSetWriteByteHandler(0,		spikes91_main_write_byte);
	SekSetReadWordHandler(0,		pspikes_main_read_word);
	SekSetReadByteHandler(0,		pspikes_main_read_byte);

	install_palette_handler(0xffe000, 0xffefff);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(spikes91_sound_write);
	ZetSetReadHandler(spikes91_sound_read);
	ZetClose();

	zet_frequency = 3000000;
	sound_type = Z80_YM2151;

	BurnYM2151InitBuffered(3000000, 1, NULL, 0);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachZet(zet_frequency);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, pspikes_bg_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, spikes91_tx_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x100000, 0x000, 0x3f);
	GenericTilemapSetGfx(2, DrvGfxROM[1], 4, 16, 16, 0x200000, 0x400, 0x3f);
	GenericTilemapSetScrollRows(0, 256);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -4, 0);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapBuildSkipTable(1, 0, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 PspikesbGfxDecode()
{
	INT32 Plane0[4] = { STEP4(0, 0x20000*8) };
	INT32 Plane1[4] = { STEP4(0, 0x40000*8) };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x080000; i++) tmp[i] = DrvGfxROM[0][i] ^ 0xff;

	GfxDecode(0x4000, 4,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM[0]);

	for (INT32 i = 0; i < 0x100000; i++) tmp[i] = DrvGfxROM[1][i] ^ 0xff;

	GfxDecode(0x2000, 4, 16, 16, Plane1, XOffs, YOffs, 0x100, tmp, DrvGfxROM[1]);

	BurnFree (tmp);

	return 0;
}

static INT32 PspikesbInit()
{
	AllMem = NULL;
	MemIndex(0x100000, 0x200000, 0, 0, 0x080000, 0, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x100000, 0x200000, 0, 0, 0x080000, 0, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x060000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x0c0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[0]  + 0x000000, k++, 1)) return 1;

		PspikesbGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x100000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x200000, 0x203fff, MAP_RAM);
	SekMapMemory(DrvWRAM[0],		0xff8000, 0xff8fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0xffc000, 0xffcbff, MAP_RAM);
	SekMapMemory(DrvRasterRAM,		0xffd000, 0xffdfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xffe000, 0xffefff, MAP_RAM);
	SekSetWriteWordHandler(0,		pspikes_main_write_word);
	SekSetWriteByteHandler(0,		pspikesb_main_write_byte);
	SekSetReadWordHandler(0,		pspikes_main_read_word);
	SekSetReadByteHandler(0,		pspikesb_main_read_byte);

	install_palette_handler(0xffe000, 0xffefff);
	SekClose();

	MSM6295Init(0, 1056000 / MSM6295_PIN7_LOW, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	sound_type = NOZ80_MSM6295;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, pspikes_bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x100000, 0x000, 0x3f);
	GenericTilemapSetGfx(2, DrvGfxROM[1], 4, 16, 16, 0x200000, 0x400, 0x3f);
	GenericTilemapSetScrollRows(0, 256);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -4, 0);

	DrvDoReset();

	return 0;
}

static INT32 PspikescInit()
{
	AllMem = NULL;
	MemIndex(0x100000, 0x200000, 0, 0, 0x080000, 0, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x100000, 0x200000, 0, 0, 0x080000, 0, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[1] + 0x000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x000002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvSndROM[0]  + 0x000000, k++, 1)) return 1;

		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x080000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[1], NULL, 0x100000, 1, 0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x100000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x200000, 0x203fff, MAP_RAM);
	SekMapMemory(DrvWRAM[0],		0xff8000, 0xff8fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0xffc000, 0xffcfff, MAP_RAM);
	SekMapMemory(DrvRasterRAM,		0xffd000, 0xffdfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xffe000, 0xffefff, MAP_RAM);
	SekSetWriteWordHandler(0,		pspikes_main_write_word);
	SekSetWriteByteHandler(0,		pspikesb_main_write_byte);
	SekSetReadWordHandler(0,		pspikes_main_read_word);
	SekSetReadByteHandler(0,		pspikesb_main_read_byte);

	install_palette_handler(0xffe000, 0xffefff);
	SekClose();

	MSM6295Init(0, 1056000 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	sound_type = NOZ80_MSM6295;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, pspikes_bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x100000, 0x000, 0x3f);
	GenericTilemapSetGfx(2, DrvGfxROM[1], 4, 16, 16, 0x200000, 0x400, 0x3f);
	GenericTilemapSetScrollRows(0, 256);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -4, 0);

	DrvDoReset();

	return 0;
}

static INT32 SpinlbrkInit()
{
	AllMem = NULL;
	MemIndex(0x200000, 0x400000, 0x200000, 0x400000, 0x000000, 0x100000, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x200000, 0x400000, 0x200000, 0x400000, 0x000000, 0x100000, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x020000, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM     + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM     + 0x008000, k++, 1)) return 1;
		memmove (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x10000); // make compatible w/turbofrc banking

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x080000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x100000, k++, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[2]  + 0x000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2]  + 0x000002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRomExt(DrvGfxROM[3]  + 0x000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[3]  + 0x100000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[3]  + 0x000002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[3]  + 0x100002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvGfxROM[4]  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[4]  + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvSndROM[1]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[1]  + 0x080000, k++, 1)) return 1;

		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x100000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[1], NULL, 0x200000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[2], NULL, 0x100000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[3], NULL, 0x200000, 1, 0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvWRAM[0],		0x080000, 0x080fff, MAP_RAM);
	SekMapMemory(DrvWRAM[1],		0x082000, 0x082fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xff8000, 0xffbfff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0xffc000, 0xffcfff, MAP_RAM);
	SekMapMemory(DrvRasterRAM,		0xffd000, 0xffdfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xffe000, 0xffefff, MAP_RAM);
	SekSetWriteWordHandler(0,		spinlbrk_main_write_word);
	SekSetWriteByteHandler(0,		spinlbrk_main_write_byte);
	SekSetReadWordHandler(0,		pspikes_main_read_word);
	SekSetReadByteHandler(0,		pspikes_main_read_byte);

	install_palette_handler(0xffe000, 0xffefff);
	SekClose();

	standard_sound_system(0, 0x000000, 0x100000);

	screen_offsets[0] = -8;
	screen_offsets[1] = 0;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, spinlbrk_bg_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, karatblz_fg_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x200000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,  8,  8, 0x400000, 0x100, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x200000, 0x200, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4, 16, 16, 0x400000, 0x300, 0xf);
	GenericTilemapSetScrollRows(0, 512);
	GenericTilemapSetOffsets(TMAP_GLOBAL, screen_offsets[0], screen_offsets[1]);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapBuildSkipTable(1, 1, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 KaratblzInit()
{
	AllMem = NULL;
	MemIndex(0x100000, 0x100000, 0x800000, 0x200000, 0x080000, 0x100000, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x100000, 0x100000, 0x800000, 0x200000, 0x080000, 0x100000, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x040000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM     + 0x000000, k++, 1)) return 1;
		memmove (DrvZ80ROM + 0x08000, DrvZ80ROM + 0x00000, 0x20000); // make compatible w/turbofrc banking

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[2]  + 0x000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2]  + 0x200000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2]  + 0x000002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2]  + 0x200002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRomExt(DrvGfxROM[3]  + 0x000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[3]  + 0x000002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvSndROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1]  + 0x000000, k++, 1)) return 1;

		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x080000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[1], NULL, 0x080000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[2], NULL, 0x400000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[3], NULL, 0x100000, 1, 0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekSetAddressMask(0xfffff);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvWRAM[0],		0x080000, 0x081fff, MAP_RAM);
	SekMapMemory(DrvWRAM[1],		0x082000, 0x083fff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x0a0000, 0x0affff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[1],	0x0b0000, 0x0bffff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0x0c0000, 0x0cffff, MAP_RAM);
	SekMapMemory(DrvExtraRAM,		0x0f8000, 0x0fbfff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x0fc000, 0x0fcfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x0fe000, 0x0fefff, MAP_RAM);
	SekSetWriteWordHandler(0,		karatblz_main_write_word);
	SekSetWriteByteHandler(0,		karatblz_main_write_byte);
	SekSetReadWordHandler(0,		karatblz_main_read_word);
	SekSetReadByteHandler(0,		karatblz_main_read_byte);

	install_palette_handler(0xfe000, 0xfefff);
	SekClose();

	sprite_lut_mask[0] = sprite_lut_mask[1] = 0xffff;

	standard_sound_system(0, 0x080000, 0x100000);

	screen_offsets[0] = -8;
	screen_offsets[1] = 0;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, karatblz_bg_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, karatblz_fg_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x100000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,  8,  8, 0x100000, 0x100, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x800000, 0x200, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4, 16, 16, 0x200000, 0x300, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, screen_offsets[0], screen_offsets[1]);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapBuildSkipTable(1, 1, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 KaratblzblInit()
{
	AllMem = NULL;
	MemIndex(0x100000, 0x100000, 0x800000, 0x200000, 0x020000, 0, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x100000, 0x100000, 0x800000, 0x200000, 0x020000, 0, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM     + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2]  + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x080000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x080001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x100000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x100001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x180000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x180001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x200000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x200001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x000002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x000003, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x080002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x080003, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x100002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x100003, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x180002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x180003, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x200002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x200003, k++, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM[3]  + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[3]  + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[3]  + 0x000002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[3]  + 0x000003, k++, 4)) return 1;

		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x080000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[1], NULL, 0x080000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[2], NULL, 0x400000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[3], NULL, 0x100000, 1, 0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekSetAddressMask(0xfffff);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvWRAM[0],		0x080000, 0x081fff, MAP_RAM);
	SekMapMemory(DrvWRAM[1],		0x082000, 0x083fff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x0a0000, 0x0affff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[1],	0x0b0000, 0x0bffff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0x0c0000, 0x0cffff, MAP_RAM);
	SekMapMemory(DrvExtraRAM,		0x0f8000, 0x0fbfff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x0fc000, 0x0fcfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x0fe000, 0x0fefff, MAP_RAM);
	SekSetWriteWordHandler(0,		karatblz_main_write_word);
	SekSetWriteByteHandler(0,		karatblz_main_write_byte);
	SekSetReadWordHandler(0,		karatblz_main_read_word);
	SekSetReadByteHandler(0,		karatblz_main_read_byte);

	install_palette_handler(0xfe000, 0xfefff);
	SekClose();

	sprite_lut_mask[0] = sprite_lut_mask[1] = 0xffff;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xf000, 0xf7ff, MAP_RAM);
	ZetSetReadHandler(karatblzbl_sound_read);
	ZetSetOutHandler(karatblzbl_sound_write_port);
	ZetSetInHandler(karatblzbl_sound_read_port);
	ZetClose();

	BurnYM3812Init(1, 4000000, &DrvFMIRQHandler, 0);
	BurnTimerAttachZet(4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	UPD7759Init(0, UPD7759_STANDARD_CLOCK, DrvSndROM[0]);
	UPD7759SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	sound_type = Z80_YM3812_UPD7759;

	screen_offsets[0] = 8;
	screen_offsets[1] = 0;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, karatblz_bg_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, karatblz_fg_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x100000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,  8,  8, 0x100000, 0x100, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x800000, 0x200, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4, 16, 16, 0x200000, 0x300, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, screen_offsets[0], screen_offsets[1]);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapBuildSkipTable(1, 1, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 TurbofrcInit()
{
	AllMem = NULL;
	MemIndex(0x140000, 0x140000, 0x400000, 0x100000, 0x040000, 0x100000, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x140000, 0x140000, 0x400000, 0x100000, 0x040000, 0x100000, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x080000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM     + 0x000000, k++, 1)) return 1;
		memmove (DrvZ80ROM + 0x08000, DrvZ80ROM + 0x00000, 0x20000); // make compatible w/turbofrc banking

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x080000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x080000, k++, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[2]  + 0x000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2]  + 0x100000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2]  + 0x000002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2]  + 0x100002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRomExt(DrvGfxROM[3]  + 0x000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[3]  + 0x000002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvSndROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1]  + 0x000000, k++, 1)) return 1;

		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x0a0000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[1], NULL, 0x0a0000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[2], NULL, 0x200000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[3], NULL, 0x080000, 1, 0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekSetAddressMask(0xfffff);
	SekMapMemory(Drv68KROM,			0x000000, 0x0bffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x0c0000, 0x0cffff, MAP_RAM);
	SekMapMemory(DrvWRAM[0],		0x0d0000, 0x0d1fff, MAP_RAM);
	SekMapMemory(DrvWRAM[1],		0x0d2000, 0x0d3fff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x0e0000, 0x0e3fff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[1],	0x0e4000, 0x0e7fff, MAP_RAM);
	SekMapMemory(DrvExtraRAM,		0x0f8000, 0x0fbfff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x0fc000, 0x0fcfff, MAP_RAM);
	SekMapMemory(DrvRasterRAM,		0x0fd000, 0x0fdfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x0fe000, 0x0fefff, MAP_RAM);
	SekSetWriteWordHandler(0,		turbofrc_main_write_word);
	SekSetWriteByteHandler(0,		turbofrc_main_write_byte);
	SekSetReadWordHandler(0,		turbofrc_main_read_word);
	SekSetReadByteHandler(0,		turbofrc_main_read_byte);

	install_palette_handler(0xfe000, 0xfefff);
	SekClose();

	standard_sound_system(0, 0x040000, 0x100000);

	screen_offsets[0] = 0;
	screen_offsets[1] = 0;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, aerofgt_bg_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, aerofgt_fg_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x140000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,  8,  8, 0x140000, 0x100, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x400000, 0x200, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4, 16, 16, 0x100000, 0x300, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, screen_offsets[0], screen_offsets[1]);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapBuildSkipTable(1, 1, 0xf);
	GenericTilemapSetScrollRows(0, 512);

	DrvDoReset();

	return 0;
}

static INT32 AerofgtInit()
{
	AllMem = NULL;
	MemIndex(0x200000, 0x800000, 0, 0, 0x080000, 0x100000, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x200000, 0x800000, 0, 0, 0x080000, 0x100000, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM     + 0x000000, k++, 1)) return 1;
		memmove (DrvZ80ROM + 0x08000, DrvZ80ROM + 0x00000, 0x20000); // make compatible w/turbofrc banking

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x080000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x200000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1]  + 0x000000, k++, 1)) return 1;

		BurnByteswap(DrvGfxROM[0], 0x100000);
		BurnByteswap(DrvGfxROM[1], 0x400000);
		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x100000, 0, 0);
		BurnNibbleExpand(DrvGfxROM[1], NULL, 0x400000, 0, 0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x1a0000, 0x1a07ff, MAP_RAM);
	SekMapMemory(DrvRasterRAM,		0x1b0000, 0x1b0fff, MAP_RAM);
	SekMapMemory(DrvWRAM[0],		0x1b2000, 0x1b3fff, MAP_RAM);
	SekMapMemory(DrvWRAM[1],		0x1b4000, 0x1b5fff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x1c0000, 0x1c7fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x1d0000, 0x1d1fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xfef000, 0xffefff, MAP_RAM);
	SekSetWriteWordHandler(0,		aerofgt_main_write_word);
	SekSetWriteByteHandler(0,		aerofgt_main_write_byte);
	SekSetReadWordHandler(0,		aerofgt_main_read_word);
	SekSetReadByteHandler(0,		aerofgt_main_read_byte);

	install_palette_handler(0x1a0000, 0x1a07ff);
	SekClose();

	sprite_lut_mask[0] = 0x7fff;

	standard_sound_system(1, 0x080000, 0x100000);

	screen_offsets[0] = 0;
	screen_offsets[1] = 0;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, aerofgt_bg_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, aerofgt_fg_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x200000, 0x000, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM[0], 4,  8,  8, 0x200000, 0x100, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[1], 4, 16, 16, 0x800000, 0x200, 0x1f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, screen_offsets[0], screen_offsets[1]);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapBuildSkipTable(1, 1, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 AerofgtbInit()
{
	AllMem = NULL;
	MemIndex(0x140000, 0x140000, 0x400000, 0x100000, 0x040000, 0x100000, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x140000, 0x140000, 0x400000, 0x100000, 0x040000, 0x100000, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM     + 0x000000, k++, 1)) return 1;
		memmove (DrvZ80ROM + 0x08000, DrvZ80ROM + 0x00000, 0x20000); // make compatible w/turbofrc banking

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[2]  + 0x000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2]  + 0x000002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRomExt(DrvGfxROM[3]  + 0x000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[3]  + 0x000002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvSndROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1]  + 0x000000, k++, 1)) return 1;

		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x080000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[1], NULL, 0x080000, 1, 0);
	    BurnNibbleExpand(DrvGfxROM[2], NULL, 0x100000, 1|2, 0);
		BurnNibbleExpand(DrvGfxROM[3], NULL, 0x080000, 1|2, 0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x0bffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x0c0000, 0x0cffff, MAP_RAM);
	SekMapMemory(DrvWRAM[0],		0x0d0000, 0x0d1fff, MAP_RAM);
	SekMapMemory(DrvWRAM[1],		0x0d2000, 0x0d3fff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x0e0000, 0x0e3fff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[1],	0x0e4000, 0x0e7fff, MAP_RAM);
	SekMapMemory(DrvExtraRAM,		0x0f8000, 0x0fbfff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x0fc000, 0x0fcfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x0fd000, 0x0fdfff, MAP_RAM);
	SekMapMemory(DrvRasterRAM,		0x0ff000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,		turbofrc_main_write_word);
	SekSetWriteByteHandler(0,		turbofrc_main_write_byte);
	SekSetReadWordHandler(0,		aerofgtb_main_read_word);
	SekSetReadByteHandler(0,		aerofgtb_main_read_byte);

	install_palette_handler(0x0fd000, 0x0fdfff);
	SekClose();

	standard_sound_system(1, 0x040000, 0x100000);

	screen_offsets[0] = -12;
	screen_offsets[1] = 0;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, aerofgt_bg_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, aerofgt_fg_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x100000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,  8,  8, 0x100000, 0x100, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x400000, 0x200, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4, 16, 16, 0x100000, 0x300, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, screen_offsets[0], screen_offsets[1]);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapBuildSkipTable(1, 1, 0xf);
	GenericTilemapSetScrollRows(0, 512);

	// sprite offsets slightly diff. from tilemaps
	screen_offsets[0] = -9;
	screen_offsets[1] = -1;

	DrvDoReset();

	return 0;
}

static INT32 AerfbootGfxDecode()
{
	INT32 x = 0x80000*8;
	INT32 Plane0[4]  = { STEP4(0x40000*8*3,-(0x40000*8)) };
	INT32 Plane1[4]  = { STEP4(0,1) };
	INT32 XOffs0[ 8] = { STEP8(7,-1) };
	INT32 YOffs0[ 8] = { STEP8(0,8) };
	INT32 XOffs1[16] = { 2*4, 3*4, x+2*4, x+3*4, 0*4, 1*4, x+0*4, x+1*4, 6*4, 7*4, x+6*4, x+7*4, 4*4, 5*4, x+4*4, x+5*4 };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x100000);

	GfxDecode(0x8000, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x100000);

	GfxDecode(0x1000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM[2]);

	BurnFree (tmp);

	return 0;
}

static INT32 AerfbootInit()
{
	AllMem = NULL;
	MemIndex(0x200000, 0x200000, 0x100000, 0, 0x0a0000, 0, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x200000, 0x200000, 0x100000, 0, 0x0a0000, 0, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM     + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x0c0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x080000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x080000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[0]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[0]  + 0x020000, k++, 1)) return 1;

		AerfbootGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x0c0000, 0x0cffff, MAP_RAM);
	SekMapMemory(DrvWRAM[0],		0x0d0000, 0x0d1fff, MAP_RAM);
	SekMapMemory(DrvWRAM[1],		0x0d2000, 0x0d3fff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x0e0000, 0x0e3fff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[1],	0x0e4000, 0x0e7fff, MAP_RAM);
	SekMapMemory(DrvExtraRAM,		0x0f8000, 0x0fcfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x0fd000, 0x0fdfff, MAP_RAM);
	SekMapMemory(DrvRasterRAM,		0x0ff000, 0x0fffff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x108000, 0x10bfff, MAP_RAM);
	SekSetWriteWordHandler(0,		aerfboot_main_write_word);
	SekSetWriteByteHandler(0,		aerfboot_main_write_byte);
	SekSetReadWordHandler(0,		aerofgtb_main_read_word);
	SekSetReadByteHandler(0,		aerofgtb_main_read_byte);

	install_palette_handler(0xfd000, 0xfdfff);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(aerfboot_sound_write);
	ZetSetReadHandler(aerfboot_sound_read);
	ZetClose();

	MSM6295Init(0, 1056000 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	sound_type = Z80_MSM6295;
	zet_frequency = 4000000;

	screen_offsets[0] = -8;
	screen_offsets[1] = 0;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, aerofgt_bg_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, aerofgt_fg_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM[0] + 0x000000, 4,  8,  8, 0x100000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[0] + 0x100000, 4,  8,  8, 0x100000, 0x100, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[1] + 0x000000, 4, 16, 16, 0x200000, 0x200, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM[2] + 0x000000, 4, 16, 16, 0x100000, 0x300, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, screen_offsets[0], screen_offsets[1]);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapBuildSkipTable(1, 1, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 Aerfboo2GfxDecode()
{
	INT32 Plane[4]   = { STEP4(0,1) };
	INT32 XOffs0[ 8] = { 1*4, 0*4, 0x40000*8+1*4, 0x40000*8+0*4, 3*4, 2*4, 0x40000*8+3*4, 0x40000*8+2*4 };
	INT32 YOffs0[ 8] = { STEP8(0,16) };
	INT32 XOffs1[16] = { STEP8(28,-4), STEP8(60,-4) };
	INT32 YOffs1[16] = { STEP16(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x080000);

	GfxDecode(0x4000, 4,  8,  8, Plane, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x080000);

	GfxDecode(0x4000, 4,  8,  8, Plane, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM[2]);

	BurnFree (tmp);

	return 0;
}

static INT32 Aerfboo2Init()
{
	AllMem = NULL;
	MemIndex(0x100000, 0x100000, 0x400000, 0, 0x100000, 0, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x100000, 0x100000, 0x400000, 0, 0x100000, 0, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2]  + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x000002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[2]  + 0x000003, k++, 4)) return 1;

		if (BurnLoadRom(DrvSndROM[0]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[0]  + 0x080000, k++, 1)) return 1;

		Aerfboo2GfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x0c0000, 0x0cffff, MAP_RAM);
	SekMapMemory(DrvWRAM[0],		0x0d0000, 0x0d1fff, MAP_RAM);
	SekMapMemory(DrvWRAM[1],		0x0d2000, 0x0d3fff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x0e0000, 0x0e3fff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[1],	0x0e4000, 0x0e7fff, MAP_RAM);
	SekMapMemory(DrvExtraRAM,		0x0f8000, 0x0fbfff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x0fc000, 0x0fcfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x0fd000, 0x0fdfff, MAP_RAM);
	SekMapMemory(DrvRasterRAM,		0x0ff000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,		aerfboo2_main_write_word);
	SekSetWriteByteHandler(0,		aerfboo2_main_write_byte);
	SekSetReadWordHandler(0,		aerfboo2_main_read_word);
	SekSetReadByteHandler(0,		aerfboo2_main_read_byte);

	install_palette_handler(0xfd000, 0xfdfff);
	SekClose();

	MSM6295Init(0, 1056000 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	sound_type = NOZ80_MSM6295;
	sek_irq_line = 2;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, aerofgt_bg_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, aerofgt_fg_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM[0] + 0x000000, 4,  8,  8, 0x100000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1] + 0x000000, 4,  8,  8, 0x100000, 0x100, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[2] + 0x000000, 4, 16, 16, 0x200000, 0x200, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM[2] + 0x200000, 4, 16, 16, 0x200000, 0x300, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -8, 0);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapBuildSkipTable(1, 1, 0xf);
	GenericTilemapSetScrollRows(0, 64);

	DrvDoReset();

	return 0;
}

static INT32 KickballGfxDecode()
{
	INT32 Plane[4]  = { STEP4(0, 0x80000*8) };
	INT32 XOffs[16] = { 6,7, 4,5, 2,3, 0, 1, 14, 15, 12, 13, 10, 11, 8, 9 };
	INT32 YOffs[16]  = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[1], 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM[1]);

	BurnFree (tmp);

	return 0;
}

static INT32 KickballInit()
{
	AllMem = NULL;
	MemIndex(0x200000, 0x400000, 0, 0, 0x040000, 0, 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x200000, 0x400000, 0, 0, 0x040000, 0, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM     + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0]  + 0x080000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x100000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x180000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[0]  + 0x000000, k++, 1)) return 1;

		for (INT32 i = 0; i < 0x80000; i++) {
			DrvGfxROM[1][i] = (DrvGfxROM[1][i] & 0x9f) | ((DrvGfxROM[1][i] & 0x20) << 1) | ((DrvGfxROM[1][i] >> 1) & 0x20);
		}

		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x100000, 1, 0);
		KickballGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x100000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvWRAM[0],		0xff8000, 0xff8fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0xffc000, 0xffc3ff, MAP_WRITE);
	SekMapMemory(DrvRasterRAM,		0xffd000, 0xffdfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xffe000, 0xffefff, MAP_RAM);
	SekSetWriteWordHandler(0,		kickball_main_write_word);
	SekSetWriteByteHandler(0,		kickball_main_write_byte);
	SekSetReadWordHandler(0,		kickball_main_read_word);
	SekSetReadByteHandler(0,		kickball_main_read_byte);

	install_palette_handler(0xffe000, 0xffefff);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xf000, 0xf7ff, MAP_RAM);
	ZetSetReadHandler(kickball_sound_read);
	ZetSetOutHandler(kickball_sound_write_port);
	ZetSetInHandler(kickball_sound_read_port);
	ZetClose();

	BurnYM3812Init(1, 4000000, &DrvFMIRQHandler, 0);
	BurnTimerAttachZet(4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1000000 / MSM6295_PIN7_LOW, 1);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);

	sprite_lut_mask[1] = 0xffff;
	sound_type = Z80_YM3812_MSM6295;
	zet_frequency = 4000000;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, pspikes_bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x200000, 0x000, 0x3f);
	GenericTilemapSetGfx(2, DrvGfxROM[1], 4, 16, 16, 0x400000, 0x400, 0x3f);
	GenericTilemapSetScrollRows(0, 256);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -4, 0);

	DrvDoReset();

	return 0;
}

static INT32 Wbbc97GfxDecode()
{
	INT32 x = 0x40000*8;
	INT32 Plane[4]  = { STEP4(0,1) };
	INT32 XOffs[16] = { x*3+ 4, x*3+0, x*2+ 4, x*2+0, x*1+ 4, x*1+0, x*0+ 4, x*0+0,
						x*3+12, x*3+8, x*2+12, x*2+8, x*1+12, x*1+8, x*0+12, x*0+8 };
	INT32 YOffs[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[1], 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM[1]);

	BurnFree (tmp);

	return 0;
}

static INT32 Wbbc97Init()
{
	AllMem = NULL;
	MemIndex(0x080000, 0x200000, 0, 0, 0x040000, 0, 1);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0x080000, 0x200000, 0, 0, 0x040000, 0, 1);

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM     + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x100000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x100001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x200000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x200001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x300000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM     + 0x300001, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM     + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0]  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1]  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1]  + 0x0c0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[0]  + 0x000000, k++, 1)) return 1;

		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x040000, 1, 0);

		Wbbc97GfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x3fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x500000, 0x50ffff, MAP_RAM);
	SekMapMemory(DrvSprLutRAM[0],	0x600000, 0x605fff, MAP_RAM);
	SekMapMemory(DrvBitmapRAM,		0xa00000, 0xa3ffff, MAP_RAM);
	SekMapMemory(DrvWRAM[0],		0xff8000, 0xff8fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0xffc000, 0xffc3ff, MAP_WRITE);
	SekMapMemory(DrvRasterRAM,		0xffd000, 0xffdfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xffe000, 0xffefff, MAP_RAM);
	SekSetWriteWordHandler(0,		pspikes_main_write_word);
	SekSetWriteByteHandler(0,		pspikes_main_write_byte);
	SekSetReadWordHandler(0,		pspikes_main_read_word);
	SekSetReadByteHandler(0,		pspikes_main_read_byte);

	install_palette_handler(0xffe000, 0xffefff);
	SekClose();

	sprite_lut_mask[0] = 0x5fff; // should be 5fff

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xf000, 0xf7ff, MAP_RAM);
	ZetSetReadHandler(wbbc97_sound_read);
	ZetSetWriteHandler(wbbc97_sound_write);
	ZetClose();

	BurnYM3812Init(1, 4000000, &DrvFMIRQHandler, 0);
	BurnTimerAttachZet(4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1000000 / MSM6295_PIN7_LOW, 1);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);

	sound_type = Z80_YM3812_MSM6295;
	zet_frequency = 4000000;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, pspikes_bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x080000, 0x000, 0x3f);
	GenericTilemapSetGfx(2, DrvGfxROM[1], 4, 16, 16, 0x200000, 0x400, 0x3f);
	GenericTilemapSetScrollRows(0, 256);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -4, 0);
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapBuildSkipTable(0, 0, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	switch (sound_type)
	{
		case Z80_YM2610:
			ZetExit();
			BurnYM2610Exit();
		break;

		case Z80_MSM6295:
			ZetExit();
			MSM6295Exit();
		break;

		case NOZ80_MSM6295:
			MSM6295Exit();
		break;

		case Z80_YM3812_MSM6295:
			ZetExit();
			BurnYM3812Exit();
			MSM6295Exit();
		break;

		case Z80_YM3812_UPD7759:
			ZetExit();
			BurnYM3812Exit();
			UPD7759Exit();
		break;

		case Z80_YM2151:
			ZetExit();
			BurnYM2151Exit();
		break;
	}

	SekExit();

	BurnFree (AllMem);

	sound_type = Z80_YM2610;
	sek_irq_line = 1;
	zet_frequency = 5000000;
	sprite_lut_mask[0] = 0x3fff;
	sprite_lut_mask[1] = 0x3fff;

	screen_offsets[0] = screen_offsets[1] = 0;

	return 0;
}

static void DrvPaletteUpdate() // 5rgb
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		INT32 r = (p[i] >> 10) & 0x1f;
		INT32 g = (p[i] >>  5) & 0x1f;
		INT32 b = (p[i] >>  0) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static UINT32 sprite_ramlut0(UINT32 tile)
{
	return *((UINT16*)(DrvSprLutRAM[0] + ((tile * 2) % (sprite_lut_mask[0] + 1))));
}

static UINT32 sprite_ramlut1(UINT32 tile)
{
	return *((UINT16*)(DrvSprLutRAM[1] + ((tile * 2) & sprite_lut_mask[1])));
}

static UINT32 sprite_romlut(UINT32 tile)
{
	return *((UINT16*)(DrvGfxROM[4] + ((tile * 2) & 0x1fffe)));
}

static void draw_sprites(UINT8 *sprite_ram, INT32 palette_bank, INT32 pri_param, INT32 m_gfx, INT32 m_pritype, UINT32 (*cb)(UINT32))
{
	UINT16 *spriteram = (UINT16*)sprite_ram;

	INT32 first = (4 * spriteram[0x1fe]) & 0x1ff;

	if (first>0x200-4)
		first = 0x200-4;

	int start = 0x200 - 8;
	int end = first-4;
	int inc = -4;

	GenericTilesGfx *gfx = &GenericGfxData[m_gfx];

	for (INT32 attr_start = start; attr_start != end; attr_start += inc)
	{
		UINT16 *ram = &spriteram[attr_start];

		// sprite is disabled
		if (!(ram[2] & 0x0080))
			continue;

		INT32 oy    =  (ram[0] & 0x01ff);
		INT32 zoomy =  (ram[0] & 0xf000) >> 12;
		INT32 ox =     (ram[1] & 0x01ff);
		INT32 zoomx =  (ram[1] & 0xf000) >> 12;
		INT32 xsize =  (ram[2] & 0x0700) >> 8;
		INT32 flipx =  (ram[2] & 0x0800);
		INT32 ysize =  (ram[2] & 0x7000) >> 12;
		INT32 flipy =  (ram[2] & 0x8000);
		INT32 color =  (ram[2] & 0x000f);
		INT32 pri =    (ram[2] & 0x0010);
		INT32 map =    (ram[3]);

		ox += screen_offsets[0];
		oy += screen_offsets[1];

		if ((pri>>4) != pri_param)
			continue;

		INT32 usepri = 0;
		if (m_pritype == 0) // turbo force etc.
		{
			usepri = pri ? 0 : 2;
		}
		else if (m_pritype == 1) // spinlbrk
		{
			usepri = pri ? 2 : 0;
		}

		bool fx = flipx != 0;
		bool fy = flipy != 0;

		if (flipscreen)
		{
			ox = 308 - ox;
			oy = 208 - oy;
			fx = !fx;
			fy = !fy;
		}

		color = (((color + 16 * palette_bank) & gfx->color_mask) << gfx->depth) + gfx->color_offset;

		zoomx = 32 - zoomx;
		zoomy = 32 - zoomy;

		for (INT32 y = 0; y <= ysize; y++)
		{
			INT32 cy = flipy ? (ysize - y) : y;
			INT32 sy = ((oy + zoomy * (flipscreen ? -cy : cy) / 2 + 16) & 0x1ff) - 16;

			for (INT32 x = 0; x <= xsize; x++)
			{
				int cx = flipx ? (xsize - x) : x;
				int sx = ((ox + zoomx * (flipscreen ? -cx : cx) / 2 + 16) & 0x1ff) - 16;

				int curr = ((cb == NULL) ? map++ : cb(map++)) % gfx->code_mask;

				RenderZoomedPrioSprite(pTransDraw, gfx->gfxbase, curr, color, 0xf, sx-0x000, sy-0x000, fx, fy, gfx->width, gfx->height, zoomx << 11, zoomy << 11, usepri);
				RenderZoomedPrioSprite(pTransDraw, gfx->gfxbase, curr, color, 0xf, sx-0x200, sy-0x000, fx, fy, gfx->width, gfx->height, zoomx << 11, zoomy << 11, usepri);
				RenderZoomedPrioSprite(pTransDraw, gfx->gfxbase, curr, color, 0xf, sx-0x000, sy-0x200, fx, fy, gfx->width, gfx->height, zoomx << 11, zoomy << 11, usepri);
				RenderZoomedPrioSprite(pTransDraw, gfx->gfxbase, curr, color, 0xf, sx-0x200, sy-0x200, fx, fy, gfx->width, gfx->height, zoomx << 11, zoomy << 11, usepri);
			}

			if (xsize == 2) map += 1;
			if (xsize == 4) map += 3;
			if (xsize == 5) map += 2;
			if (xsize == 6) map += 1;
		}
	}
}

static void aerofgt_draw_sprites(INT32 prihack_mask, INT32 prihack_val)
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;
	UINT16 *spritelut = (UINT16*)DrvSprLutRAM[0];

	for (INT32 offs = 0; offs < 0x2000 / 2; offs++)
	{
		if (spriteram[offs] & 0x4000) break;

		if ((spriteram[offs] & 0x8000) == 0x0000)
		{
			INT32 attr_start = 4 * (spriteram[offs] & 0x03ff);

			UINT16 *ram = &spriteram[attr_start];

			INT32 oy =    (ram[0] & 0x01ff);
			INT32 ysize = (ram[0] & 0x0e00) >> 9;
			INT32 zoomy = (ram[0] & 0xf000) >> 12;
			INT32 	ox  = (ram[1] & 0x01ff);
			INT32 xsize = (ram[1] & 0x0e00) >> 9;
			INT32 zoomx = (ram[1] & 0xf000) >> 12;
			INT32 flipx = (ram[2] & 0x4000);
			INT32 flipy = (ram[2] & 0x8000);
			INT32 color = (ram[2] & 0x3f00) >> 8;
			INT32 pri   = (ram[2] & 0x3000) >> 12;
			INT32 map   = (ram[3] & 0xffff) | ((ram[2] & 0x0001) << 16);

			if ((pri & prihack_mask) != prihack_val) continue;

			GenericTilesGfx *gfx = &GenericGfxData[2];

			color = ((color & gfx->color_mask) << gfx->depth) + gfx->color_offset;

			oy += screen_offsets[1];
			ox += screen_offsets[0];

			zoomx = 32 - zoomx;
			zoomy = 32 - zoomy;

			INT32 ystart, yend, yinc;

			if (!flipy) { ystart = 0; yend = ysize+1; yinc = 1; }
			else        { ystart = ysize; yend = -1; yinc = -1; }

			int ycnt = ystart;
			while (ycnt != yend)
			{
				INT32 xstart, xend, xinc;

				if (!flipx) { xstart = 0; xend = xsize+1; xinc = 1; }
				else        { xstart = xsize; xend = -1; xinc = -1; }

				INT32 xcnt = xstart;

				while (xcnt != xend)
				{
					int startno = spritelut[(map++) & 0x7fff] % gfx->code_mask;

					RenderZoomedTile(pTransDraw, gfx->gfxbase, startno, color, 0xf, ox + xcnt * zoomx/2,        oy + ycnt * zoomy/2,        flipx, flipy, gfx->width, gfx->height, zoomx << 11, zoomy << 11);
					RenderZoomedTile(pTransDraw, gfx->gfxbase, startno, color, 0xf, -0x200+ox + xcnt * zoomx/2, oy + ycnt * zoomy/2,        flipx, flipy, gfx->width, gfx->height, zoomx << 11, zoomy << 11);
					RenderZoomedTile(pTransDraw, gfx->gfxbase, startno, color, 0xf, ox + xcnt * zoomx/2,        -0x200+oy + ycnt * zoomy/2, flipx, flipy, gfx->width, gfx->height, zoomx << 11, zoomy << 11);
					RenderZoomedTile(pTransDraw, gfx->gfxbase, startno, color, 0xf, -0x200+ox + xcnt * zoomx/2, -0x200+oy + ycnt * zoomy/2, flipx, flipy, gfx->width, gfx->height, zoomx << 11, zoomy << 11);

					xcnt+=xinc;
				}
				ycnt+=yinc;
			}
		}
	}
}

static INT32 PspikesDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollY(0, scrolly[0]);

	{
		UINT16 *rs = (UINT16*)DrvRasterRAM;

		for (INT32 i = 0; i < 256; i++) {
			GenericTilemapSetScrollRow(0, (i + scrolly[0]) & 0xff, rs[i]);
		}
	}

	BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);

	if (nSpriteEnable & 1) draw_sprites(DrvSprRAM, spritepalettebank, 1, 2, 0, sprite_ramlut0);
	if (nSpriteEnable & 2) draw_sprites(DrvSprRAM, spritepalettebank, 0, 2, 0, sprite_ramlut0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 SpinlbrkDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	{
		UINT16 *rs = (UINT16*)DrvRasterRAM;

		for (INT32 i = 0; i < 256; i++) {
			GenericTilemapSetScrollRow(0, i, rs[i] - 8);
		}
	}

	GenericTilemapSetScrollX(1, scrollx[1] - 4);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 1);

	if (nSpriteEnable & 1) draw_sprites(DrvSprRAM + 0x000, spritepalettebank, 0, 2, 1, NULL);
	if (nSpriteEnable & 2) draw_sprites(DrvSprRAM + 0x000, spritepalettebank, 1, 2, 1, NULL);
	if (nSpriteEnable & 4) draw_sprites(DrvSprRAM + 0x400, spritepalettebank, 0, 3, 1, sprite_romlut);
	if (nSpriteEnable & 8) draw_sprites(DrvSprRAM + 0x400, spritepalettebank, 1, 3, 1, sprite_romlut);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 KaratblzDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;		// force
	}

	GenericTilemapSetScrollX(0, scrollx[0] - 8);
	GenericTilemapSetScrollY(0, scrolly[0]);
	GenericTilemapSetScrollX(1, scrollx[1] - 4);
	GenericTilemapSetScrollY(1, scrolly[1]);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);

	if (nSpriteEnable & 1) draw_sprites(DrvSprRAM + 0x400, spritepalettebank, 1, 3, 0, sprite_ramlut1);
	if (nSpriteEnable & 2) draw_sprites(DrvSprRAM + 0x400, spritepalettebank, 0, 3, 0, sprite_ramlut1);
	if (nSpriteEnable & 4) draw_sprites(DrvSprRAM + 0x000, spritepalettebank, 1, 2, 0, sprite_ramlut0);
	if (nSpriteEnable & 8) draw_sprites(DrvSprRAM + 0x000, spritepalettebank, 0, 2, 0, sprite_ramlut0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 TurbofrcDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;		// force
	}

	GenericTilemapSetScrollY(0, scrolly[0] + 2);
	GenericTilemapSetScrollX(1, scrollx[1] - 7);
	GenericTilemapSetScrollY(1, scrolly[1] + 2);

	{
		UINT16 *rs = (UINT16*)DrvRasterRAM;

		for (INT32 i = 0; i < 256; i++) {
			GenericTilemapSetScrollRow(0, (i + scrolly[0] + 2) & 0x1ff, rs[7]-11);
		}
	}

	BurnTransferClear();
	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 1);

	if (nSpriteEnable & 1) draw_sprites(DrvSprRAM + 0x400, spritepalettebank, 1, 3, 0, sprite_ramlut1);
	if (nSpriteEnable & 2) draw_sprites(DrvSprRAM + 0x400, spritepalettebank, 0, 3, 0, sprite_ramlut1);
	if (nSpriteEnable & 4) draw_sprites(DrvSprRAM + 0x000, spritepalettebank, 1, 2, 0, sprite_ramlut0);
	if (nSpriteEnable & 8) draw_sprites(DrvSprRAM + 0x000, spritepalettebank, 0, 2, 0, sprite_ramlut0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 AerofgtDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;		// force
	}

	UINT16 *rs = (UINT16*)DrvRasterRAM;

	GenericTilemapSetScrollX(0, rs[0x000] - 18);
	GenericTilemapSetScrollY(0, scrolly[0]);
	GenericTilemapSetScrollX(1, rs[0x200] - 20);
	GenericTilemapSetScrollY(1, scrolly[1]);

	BurnTransferClear();
	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);

	if (nSpriteEnable & 1) aerofgt_draw_sprites(3, 0);
	if (nSpriteEnable & 2) aerofgt_draw_sprites(3, 1);

	if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);

	if (nSpriteEnable & 4) aerofgt_draw_sprites(3, 2);
	if (nSpriteEnable & 8) aerofgt_draw_sprites(3, 3);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void pspikesb_draw_sprites()
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	for (INT32 i = 4; i < 0x1000 / 2; i += 4)
	{
		if (spriteram[i + 3 - 4] & 0x8000)
			break;

		INT32 xpos = (spriteram[i + 2] & 0x1ff) - 34;
		INT32 ypos = 256 - (spriteram[i + 3 - 4] & 0x1ff) - 33;
		INT32 code = spriteram[i + 0] & 0x1fff;
		INT32 flipx = spriteram[i + 1] & 0x0800;
		INT32 color = spriteram[i + 1] & 0x000f;

		DrawGfxMaskTile(0, 2, code, xpos, ypos, flipx, 0, color, 0xf);
		DrawGfxMaskTile(0, 2, code, xpos, ypos + 512, flipx, 0, color, 0xf);
	}
}

static INT32 PspikesbDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;		// force
	}

	GenericTilemapSetScrollY(0, scrolly[0]);

	{
		UINT16 *rs = (UINT16*)DrvRasterRAM;

		gfxbank[0] = rs[0x100] >> 12;
		gfxbank[1] = (rs[0x100] >> 8) & 0xf;

		for (INT32 i = 0; i < 256; i++) {
			GenericTilemapSetScrollRow(0, (i + scrolly[0]) & 0xff, rs[i] + 22);
		}
	}

	BurnTransferClear();
	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);

	if (nSpriteEnable & 1) pspikesb_draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void aerfboot_draw_sprites(INT32 top)
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;
	UINT16 *rasterram = (UINT16*)DrvRasterRAM;

	INT32 last = ((rasterram[(top ? 0x402 : 0x404) / 2] << 5) - 0x8000) / 2;

	for (INT32 attr_start = ((top ? (0x4000 / 2) : 0x4000) / 2) - 4; attr_start >= last; attr_start -= 4)
	{
		INT32 ox	=  spriteram[attr_start + 1] & 0x01ff;
		INT32 oy	=  spriteram[attr_start + 0] & 0x01ff;
		INT32 flipx =  spriteram[attr_start + 2] & 0x0800;
		INT32 flipy =  spriteram[attr_start + 2] & 0x8000;
		INT32 color =  spriteram[attr_start + 2] & 0x000f;
		INT32 zoomx = (spriteram[attr_start + 1] & 0xf000) >> 12;
		INT32 zoomy = (spriteram[attr_start + 0] & 0xf000) >> 12;
		INT32 pri	=  spriteram[attr_start + 2] & 0x0010;
		INT32 code	= (spriteram[attr_start + 3] & 0x1fff) | ((~spriteram[attr_start + 2] & 0x0040) << 7);

		zoomx = 32 + zoomx;
		zoomy = 32 + zoomy;

		INT32 sy = ((oy + 16 - 1) & 0x1ff) - 16;
		INT32 sx = ((ox + 16 + 3) & 0x1ff) - 16;

		GenericTilesGfx *gfx = &GenericGfxData[(code >= 0x1000) ? 2 : 3];

		RenderZoomedPrioSprite(pTransDraw, gfx->gfxbase, code % gfx->code_mask, (color << gfx->depth) + gfx->color_offset, 0xf, sx, sy, flipx, flipy, gfx->width, gfx->height, zoomx << 11, zoomy << 11, pri ? 0 : 2);
	}
}

static INT32 AerfbootDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;		// force
	}

	UINT16 *rs = (UINT16*)DrvRasterRAM;

	GenericTilemapSetScrollX(0, rs[7] + 174);
	GenericTilemapSetScrollY(0, scrolly[0] + 2);
	GenericTilemapSetScrollX(1, scrollx[1] + 172);
	GenericTilemapSetScrollY(1, scrolly[1] + 2);

	BurnTransferClear();

	if ( nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);
	if ( nBurnLayer & 2) GenericTilemapDraw(1, 0, 1);

	if (nSpriteEnable & 1) aerfboot_draw_sprites(0);
	if (nSpriteEnable & 2) aerfboot_draw_sprites(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void aerfboo2_draw_sprites(INT32 chip, INT32 chip_disabled_pri, UINT32 (*cb)(UINT32))
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;
	GenericTilesGfx *gfx = &GenericGfxData[2 + chip];

	for (INT32 attr_start = (chip * 0x200) + 0x0200 - 4; attr_start >= (chip * 0x200); attr_start -= 4)
	{
		if (!(spriteram[attr_start + 2] & 0x0080))
			continue;

		INT32 pri = spriteram[attr_start + 2] & 0x0010;

		if (chip_disabled_pri && !pri) continue;

		if ((!chip_disabled_pri) && (pri >> 4)) continue;

		INT32 ox     = spriteram[attr_start + 1] & 0x01ff;
		INT32 xsize = (spriteram[attr_start + 2] & 0x0700) >> 8;
		INT32 zoomx = (spriteram[attr_start + 1] & 0xf000) >> 12;
		INT32 oy     = spriteram[attr_start + 0] & 0x01ff;
		INT32 ysize = (spriteram[attr_start + 2] & 0x7000) >> 12;
		INT32 zoomy = (spriteram[attr_start + 0] & 0xf000) >> 12;
		INT32 flipx =  spriteram[attr_start + 2] & 0x0800;
		INT32 flipy =  spriteram[attr_start + 2] & 0x8000;
		INT32 color = (spriteram[attr_start + 2] & 0x000f) + (16 * spritepalettebank);

		INT32 map_start = spriteram[attr_start + 3];

		zoomx = 32 - zoomx;
		zoomy = 32 - zoomy;

		for (INT32 y = 0; y <= ysize; y++)
		{
			INT32 cy = (((flipy ? (oy + zoomy * (ysize - y)/2) : (oy + zoomy * y / 2)) + 16) & 0x1ff) - 16;

			for (INT32 x = 0; x <= xsize; x++)
			{
				INT32 cx = (((flipx ? (ox + zoomx * (xsize - x) / 2) : (ox + zoomx * x / 2)) + 16) & 0x1ff) - 16;

				INT32 curr = cb(map_start++) % gfx->code_mask;
				RenderZoomedPrioSprite(pTransDraw, gfx->gfxbase, curr, (color << gfx->depth) + gfx->color_offset, 0xf, cx, cy, flipx, flipy, gfx->width, gfx->height, zoomx << 11, zoomy << 11, pri ? 0 : 2);
			}

			if (xsize == 2) map_start += 1;
			if (xsize == 4) map_start += 3;
			if (xsize == 5) map_start += 2;
			if (xsize == 6) map_start += 1;
		}
	}
}

static INT32 Aerfboo2Draw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;		// force
	}

	if (0) {
		UINT16 *rs = (UINT16*)DrvRasterRAM;

		for (INT32 i = 0; i < 0x400; i+=0x10) {
			GenericTilemapSetScrollRow(0, ((i / 0x10) + (scrolly[0] / 8)) & (0x1ff/8), rs[i + (0xe/2)]);
		}
	}

	UINT16 *rs = (UINT16*)DrvRasterRAM;
	GenericTilemapSetScrollX(0, rs[0x00e/2] - 11);
	GenericTilemapSetScrollY(0, scrolly[0] + 2);
	GenericTilemapSetScrollX(1, scrollx[1] - 7);
	GenericTilemapSetScrollY(1, scrolly[1] + 2);

	BurnTransferClear();
	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);

	if (nSpriteEnable & 1) aerfboo2_draw_sprites(1, -1, sprite_ramlut1); //ship
	if (nSpriteEnable & 2) aerfboo2_draw_sprites(1,  0, sprite_ramlut1); //intro
	if (nSpriteEnable & 4) aerfboo2_draw_sprites(0, -1, sprite_ramlut0); //enemy
	if (nSpriteEnable & 8) aerfboo2_draw_sprites(0,  0, sprite_ramlut0); //enemy

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void wbbc97PaletteInit()
{
	for (INT32 i = 0; i < 0x8000; i++)
	{
		UINT8 r = (i >> 5) & 0x1f;
		UINT8 g = (i >> 10) & 0x1f;
		UINT8 b = (i >> 0) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[0x800 + i] = BurnHighCol(r,g,b,0);
	}
}

static void wbbc97_draw_bitmap()
{
	UINT16 *ram = (UINT16*)DrvBitmapRAM;
	UINT16 *scr = (UINT16*)DrvRasterRAM;

	for (INT32 y = 0, count = 16; y < nScreenHeight; y++)
	{
		UINT16 *dst = pTransDraw + (y * nScreenWidth);

		for (INT32 x = 0; x < 512; x++, count++)
		{
			INT32 sx = (10 + x - scr[y]) & 0x1ff; // not quite right - is there a scrolly variable for this?

			if (sx < nScreenWidth)
			{
				dst[sx] = (ram[count & 0x1ffff] >> 1) + 0x800;
			}
		}
	}
}

static INT32 Wbbc97Draw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		wbbc97PaletteInit();
		DrvRecalc = 0;
	}

	{
		UINT16 *rs = (UINT16*)DrvRasterRAM;

		for (INT32 i = 0; i < 256; i++) {
			GenericTilemapSetScrollRow(0, (i + scrolly[0]) & 0xff, rs[i]);
		}
	}

	GenericTilemapSetScrollY(0, scrolly[0]);

	BurnTransferClear();

	if (bitmap_enable && nBurnLayer & 1) wbbc97_draw_bitmap();

	if (nBurnLayer & 2) GenericTilemapDraw(0, 0, bitmap_enable ? 0 : TMAP_FORCEOPAQUE);

	if (nSpriteEnable & 1) draw_sprites(DrvSprRAM, spritepalettebank, 1, 2, 0, sprite_ramlut0);
	if (nSpriteEnable & 2) draw_sprites(DrvSprRAM, spritepalettebank, 0, 2, 0, sprite_ramlut0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void spikes91_draw_sprites()
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;
	UINT16 *spritelut = (UINT16*)DrvGfxROM[4];

	for (INT32 i = 0x1000 / 2 - 4; i >= 4; i -= 4)
	{
		INT32 code =(spriteram[i + 0] & 0x1fff) + (spikes91_lookup * 0x2000);
		INT32 xpos = (spriteram[i + 2] & 0x01ff) - 16;
		INT32 ypos = 256 - (spriteram[i + 1] & 0x00ff) - 26;
		INT32 flipy = 0;
		INT32 flipx = spriteram[i + 3] & 0x8000;
		INT32 color = ((spriteram[i + 3] & 0x00f0) >> 4);

		DrawGfxMaskTile(0, 2, spritelut[code] & 0x3fff, xpos, ypos, flipx, flipy, color, 0xf);
		DrawGfxMaskTile(0, 2, spritelut[code] & 0x3fff, xpos, ypos + 512, flipx, flipy, color, 0xf);
	}
}

static INT32 Spikes91Draw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;		// force
	}

	{
		UINT16 *rs = (UINT16*)DrvRasterRAM;

		for (INT32 i = 0; i < 256; i++) {
			GenericTilemapSetScrollRow(0, (i + scrolly[0]) & 0xff, rs[0xf8 + i] + 172);
		}
	}

	GenericTilemapSetScrollY(0, scrolly[0]);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);

	if (nSpriteEnable & 1) spikes91_draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	if (sound_type != NOZ80_MSM6295) ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { BurnSpeedAdjust(10000000 / 60), zet_frequency / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	SekOpen(0);
	if (sound_type != NOZ80_MSM6295) ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == nInterleave-1) SekSetIRQLine(sek_irq_line, CPU_IRQSTATUS_AUTO);

		switch (sound_type)
		{
			case Z80_YM2610:
			case Z80_YM2151:
			case Z80_YM3812_MSM6295:
			case Z80_YM3812_UPD7759:
				CPU_RUN_TIMER(1);
			break;

			case Z80_MSM6295:
				CPU_RUN(1, Zet);
			break;

	//		case NOZ80_MSM6295: break;
		}
	}

	if (pBurnSoundOut)
	{
		switch (sound_type)
		{
			case Z80_YM2610:
				BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
			break;

			case Z80_MSM6295:
			case NOZ80_MSM6295:
				MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
			break;

			case Z80_YM3812_MSM6295:
				BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
				MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
			break;

			case Z80_YM3812_UPD7759:
				BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
				UPD7759Render(pBurnSoundOut, nBurnSoundLen);
			break;

			case Z80_YM2151:
				BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
			break;
		}
	}

	if (sound_type != NOZ80_MSM6295) ZetClose();
	SekClose();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

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
		SekScan(nAction);

		switch (sound_type)
		{
			case Z80_YM2610:
				ZetScan(nAction);
				BurnYM2610Scan(nAction, pnMin);
			break;

			case NOZ80_MSM6295:
				MSM6295Scan(nAction, pnMin);
			break;

			case Z80_MSM6295:
				ZetScan(nAction);
				MSM6295Scan(nAction, pnMin);
			break;

			case Z80_YM3812_MSM6295:
				ZetScan(nAction);
				MSM6295Scan(nAction, pnMin);
				BurnYM3812Scan(nAction, pnMin);
			break;

			case Z80_YM3812_UPD7759:
				ZetScan(nAction);
				UPD7759Scan(nAction, pnMin);
				BurnYM3812Scan(nAction, pnMin);
			break;

			case Z80_YM2151:
				ZetScan(nAction);
				BurnYM2151Scan(nAction, pnMin);
			break;
		}

		SCAN_VAR(bankdata);
		SCAN_VAR(gfxbank);
		SCAN_VAR(spritepalettebank);
		SCAN_VAR(charpalettebank);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(bitmap_enable);
		SCAN_VAR(spikes91_lookup);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE)
	{
		switch (sound_type)
		{
			case Z80_YM2610:
				ZetOpen(0);
				sound_bankswitch(bankdata); // spinalbrk
				ZetClose();
			break;
		}
	}

	return 0;
}


// Spinal Breakers (World)

static struct BurnRomInfo spinlbrkRomDesc[] = {
	{ "ic98",					0x010000, 0x36c2bf70, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ic104",					0x010000, 0x34a7e158, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic93",					0x010000, 0x726f4683, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic94",					0x010000, 0xc4385e03, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ic117",					0x008000, 0x625ada41, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "ic118",					0x010000, 0x1025f024, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "ic15",					0x080000, 0xe318cf3a, 3 | BRF_GRA },           //  6 Background Tiles
	{ "ic9",					0x080000, 0xe071f674, 3 | BRF_GRA },           //  7

	{ "ic17",					0x080000, 0xa63d5a55, 4 | BRF_GRA },           //  8 Foreground Tiles
	{ "ic11",					0x080000, 0x7dcc913d, 4 | BRF_GRA },           //  9
	{ "ic16",					0x080000, 0x0d84af7f, 4 | BRF_GRA },           // 10

	{ "ic12",					0x080000, 0xd63fac4e, 5 | BRF_GRA },           // 11 Sprite Chip #0 Tiles
	{ "ic18",					0x080000, 0x5a60444b, 5 | BRF_GRA },           // 12

	{ "ic14",					0x080000, 0x1befd0f3, 6 | BRF_GRA },           // 13 Sprite Chip #1 Tiles
	{ "ic20",					0x080000, 0xc2f84a61, 6 | BRF_GRA },           // 14
	{ "ic35",					0x080000, 0xeba8e1a3, 6 | BRF_GRA },           // 15
	{ "ic40",					0x080000, 0x5ef5aa7e, 6 | BRF_GRA },           // 16

	{ "ic19",					0x010000, 0xdb24eeaa, 7 | BRF_GRA },           // 17 Sprite Look-up Roms
	{ "ic13",					0x010000, 0x97025bf4, 7 | BRF_GRA },           // 18

	{ "ic166",					0x080000, 0x6e0d063a, 8 | BRF_SND },           // 19 YM2610 Samples
	{ "ic163",					0x080000, 0xe6621dfb, 8 | BRF_SND },           // 20

	{ "epl16p8bp.ic133",		0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 21 plds
	{ "epl16p8bp.ic127",		0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 22
	{ "epl16p8bp.ic99",			0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 23
	{ "epl16p8bp.ic100",		0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 24
	{ "gal16v8a.ic95",			0x000117, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 25
	{ "gal16v8a.ic114",			0x000117, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 26
};

STD_ROM_PICK(spinlbrk)
STD_ROM_FN(spinlbrk)

struct BurnDriver BurnDrvSpinlbrk = {
	"spinlbrk", NULL, NULL, NULL, "1990",
	"Spinal Breakers (World)\0", NULL, "V-System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, spinlbrkRomInfo, spinlbrkRomName, NULL, NULL, NULL, NULL, SpinlbrkInputInfo, SpinlbrkDIPInfo,
	SpinlbrkInit, DrvExit, DrvFrame, SpinlbrkDraw, DrvScan, &DrvRecalc, 0x400,
	352, 240, 4, 3
};


// Spinal Breakers (US)

static struct BurnRomInfo spinlbrkuRomDesc[] = {
	{ "ic98.u5",				0x010000, 0x3a0f7667, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ic104.u6",				0x010000, 0xa0e0af31, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic93.u4",				0x010000, 0x0cf73029, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic94.u3",				0x010000, 0x5cf7c426, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ic117",					0x008000, 0x625ada41, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "ic118",					0x010000, 0x1025f024, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "ic15",					0x080000, 0xe318cf3a, 3 | BRF_GRA },           //  6 Background Tiles
	{ "ic9",					0x080000, 0xe071f674, 3 | BRF_GRA },           //  7

	{ "ic17",					0x080000, 0xa63d5a55, 4 | BRF_GRA },           //  8 Foreground Tiles
	{ "ic11",					0x080000, 0x7dcc913d, 4 | BRF_GRA },           //  9
	{ "ic16",					0x080000, 0x0d84af7f, 4 | BRF_GRA },           // 10

	{ "ic12",					0x080000, 0xd63fac4e, 5 | BRF_GRA },           // 11 Sprite Chip #0 Tiles
	{ "ic18",					0x080000, 0x5a60444b, 5 | BRF_GRA },           // 12

	{ "ic14",					0x080000, 0x1befd0f3, 6 | BRF_GRA },           // 13 Sprite Chip #1 Tiles
	{ "ic20",					0x080000, 0xc2f84a61, 6 | BRF_GRA },           // 14
	{ "ic35",					0x080000, 0xeba8e1a3, 6 | BRF_GRA },           // 15
	{ "ic40",					0x080000, 0x5ef5aa7e, 6 | BRF_GRA },           // 16

	{ "ic19",					0x010000, 0xdb24eeaa, 7 | BRF_GRA },           // 17 Sprite Look-up Table
	{ "ic13",					0x010000, 0x97025bf4, 7 | BRF_GRA },           // 18

	{ "ic166",					0x080000, 0x6e0d063a, 8 | BRF_SND },           // 19 YM2610 Samples
	{ "ic163",					0x080000, 0xe6621dfb, 8 | BRF_SND },           // 20

	{ "epl16p8bp.ic133",		0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 21 PLDs
	{ "epl16p8bp.ic127",		0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 22
	{ "epl16p8bp.ic99",			0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 23
	{ "epl16p8bp.ic100",		0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 24
	{ "gal16v8a.ic95",			0x000117, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 25
	{ "gal16v8a.ic114",			0x000117, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 26
};

STD_ROM_PICK(spinlbrku)
STD_ROM_FN(spinlbrku)

struct BurnDriver BurnDrvSpinlbrku = {
	"spinlbrku", "spinlbrk", NULL, NULL, "1990",
	"Spinal Breakers (US)\0", NULL, "V-System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, spinlbrkuRomInfo, spinlbrkuRomName, NULL, NULL, NULL, NULL, SpinlbrkInputInfo, SpinlbrkuDIPInfo,
	SpinlbrkInit, DrvExit, DrvFrame, SpinlbrkDraw, DrvScan, &DrvRecalc, 0x400,
	352, 240, 4, 3
};


// Spinal Breakers (Japan)

static struct BurnRomInfo spinlbrkjRomDesc[] = {
	{ "j5",						0x010000, 0x6a3d690e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "j6",						0x010000, 0x869593fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "j4",						0x010000, 0x33e33912, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "j3",						0x010000, 0x16ca61d0, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ic117",					0x008000, 0x625ada41, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "ic118",					0x010000, 0x1025f024, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "ic15",					0x080000, 0xe318cf3a, 3 | BRF_GRA },           //  6 Background Tiles
	{ "ic9",					0x080000, 0xe071f674, 3 | BRF_GRA },           //  7

	{ "ic17",					0x080000, 0xa63d5a55, 4 | BRF_GRA },           //  8 Foreground Tiles
	{ "ic11",					0x080000, 0x7dcc913d, 4 | BRF_GRA },           //  9
	{ "ic16",					0x080000, 0x0d84af7f, 4 | BRF_GRA },           // 10

	{ "ic12",					0x080000, 0xd63fac4e, 5 | BRF_GRA },           // 11 Sprite Chip #0 Tiles
	{ "ic18",					0x080000, 0x5a60444b, 5 | BRF_GRA },           // 12

	{ "ic14",					0x080000, 0x1befd0f3, 6 | BRF_GRA },           // 13 Sprite Chip #1 Tiles
	{ "ic20",					0x080000, 0xc2f84a61, 6 | BRF_GRA },           // 14
	{ "ic35",					0x080000, 0xeba8e1a3, 6 | BRF_GRA },           // 15
	{ "ic40",					0x080000, 0x5ef5aa7e, 6 | BRF_GRA },           // 16

	{ "ic19",					0x010000, 0xdb24eeaa, 7 | BRF_GRA },           // 17 Sprite Look-up Table
	{ "ic13",					0x010000, 0x97025bf4, 7 | BRF_GRA },           // 18

	{ "ic166",					0x080000, 0x6e0d063a, 8 | BRF_SND },           // 19 YM2610 Samples
	{ "ic163",					0x080000, 0xe6621dfb, 8 | BRF_SND },           // 20

	{ "epl16p8bp.ic133",		0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 21 PLDs
	{ "epl16p8bp.ic127",		0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 22
	{ "epl16p8bp.ic99",			0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 23
	{ "epl16p8bp.ic100",		0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 24
	{ "gal16v8a.ic95",			0x000117, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 25
	{ "gal16v8a.ic114",			0x000117, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 26
};

STD_ROM_PICK(spinlbrkj)
STD_ROM_FN(spinlbrkj)

struct BurnDriver BurnDrvSpinlbrkj = {
	"spinlbrkj", "spinlbrk", NULL, NULL, "1990",
	"Spinal Breakers (Japan)\0", NULL, "V-System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, spinlbrkjRomInfo, spinlbrkjRomName, NULL, NULL, NULL, NULL, SpinlbrkInputInfo, SpinlbrkDIPInfo,
	SpinlbrkInit, DrvExit, DrvFrame, SpinlbrkDraw, DrvScan, &DrvRecalc, 0x400,
	352, 240, 4, 3
};


// Spinal Breakers (US, prototype)
// the labels are official Video System without numbering and handwritten on top

static struct BurnRomInfo spinlbrkupRomDesc[] = {
	{ "spb0-e.ic98",				0x010000, 0x421eaff2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "spb0-o.ic104",				0x010000, 0x9576d508, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sbp1-e.ic93",				0x010000, 0xd6444d1e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sbp1-o.ic94",				0x010000, 0xa3f7bd8e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "11-14-15.00.music.ic117",	0x008000, 0x6b8c8f09, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "11-14.m.bank.ic118",			0x010000, 0xa1ed270b, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "ic15",						0x080000, 0xe318cf3a, 3 | BRF_GRA },           //  6 Background Tiles
	{ "ic9",						0x080000, 0xe071f674, 3 | BRF_GRA },           //  7

	{ "ic17",						0x080000, 0xa63d5a55, 4 | BRF_GRA },           //  8 Foreground Tiles
	{ "ic11",						0x080000, 0x7dcc913d, 4 | BRF_GRA },           //  9
	{ "ic16",						0x080000, 0x0d84af7f, 4 | BRF_GRA },           // 10

	{ "ic12",						0x080000, 0xd63fac4e, 5 | BRF_GRA },           // 11 Sprite Chip #0 Tiles
	{ "ic18",						0x080000, 0x5a60444b, 5 | BRF_GRA },           // 12

	{ "ic14",						0x080000, 0x1befd0f3, 6 | BRF_GRA },           // 13 Sprite Chip #1 Tiles
	{ "ic20",						0x080000, 0xc2f84a61, 6 | BRF_GRA },           // 14
	{ "ic35",						0x080000, 0xeba8e1a3, 6 | BRF_GRA },           // 15
	{ "ic40",						0x080000, 0x5ef5aa7e, 6 | BRF_GRA },           // 16

	{ "sbm-1-18.ic19",				0x010000, 0xe155357f, 7 | BRF_GRA },           // 17 Sprite Look-up Table
	{ "sbm-0-18.ic13",  			0x010000, 0x16b79e45, 7 | BRF_GRA },           // 18

	{ "ic166",						0x080000, 0x6e0d063a, 8 | BRF_SND },           // 19 YM2610 Samples
	{ "ic163",						0x080000, 0xe6621dfb, 8 | BRF_SND },           // 20

	{ "epl16p8bp.ic133",			0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 21 PLDs
	{ "epl16p8bp.ic127",			0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 22
	{ "epl16p8bp.ic99",				0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 23
	{ "epl16p8bp.ic100",			0x000107, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 24
	{ "gal16v8a.ic95",				0x000117, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 25
	{ "gal16v8a.ic114",				0x000117, 0x00000000, 9 | BRF_NODUMP | BRF_OPT },           // 26
};

STD_ROM_PICK(spinlbrkup)
STD_ROM_FN(spinlbrkup)

struct BurnDriver BurnDrvSpinlbrkup = {
	"spinlbrkup", "spinlbrk", NULL, NULL, "1990",
	"Spinal Breakers (US, prototype)\0", NULL, "V-System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, spinlbrkupRomInfo, spinlbrkupRomName, NULL, NULL, NULL, NULL, SpinlbrkInputInfo, SpinlbrkDIPInfo,
	SpinlbrkInit, DrvExit, DrvFrame, SpinlbrkDraw, DrvScan, &DrvRecalc, 0x400,
	352, 240, 4, 3
};


// Power Spikes (World)

static struct BurnRomInfo pspikesRomDesc[] = {
	{ "pspikes2.bin",			0x040000, 0xec0c070e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "19",						0x020000, 0x7e8ed6e5, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "g7h",					0x080000, 0x74c23c3d, 3 | BRF_GRA },           //  2 Background Tiles

	{ "g7j",					0x080000, 0x0b9e4739, 4 | BRF_GRA },           //  3 Sprites
	{ "g7l",					0x080000, 0x943139ff, 4 | BRF_GRA },           //  4

	{ "a47",					0x040000, 0xc6779dfa, 5 | BRF_SND },           //  5 YM2610 Delta-T Samples

	{ "o5b",					0x100000, 0x07d6cbac, 6 | BRF_SND },           //  6 YM2610 Samples

	{ "peel18cv8.bin",			0x000155, 0xaf5a83c9, 7 | BRF_OPT },           //  7 PLDs
};

STD_ROM_PICK(pspikes)
STD_ROM_FN(pspikes)

struct BurnDriver BurnDrvPspikes = {
	"pspikes", NULL, NULL, NULL, "1991",
	"Power Spikes (World)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pspikesRomInfo, pspikesRomName, NULL, NULL, NULL, NULL, PspikesInputInfo, PspikesDIPInfo,
	PspikesInit, DrvExit, DrvFrame, PspikesDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Power Spikes (Korea)

static struct BurnRomInfo pspikeskRomDesc[] = {
	{ "20",						0x040000, 0x75cdcee2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "19",						0x020000, 0x7e8ed6e5, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "g7h",					0x080000, 0x74c23c3d, 3 | BRF_GRA },           //  2 Background Tiles

	{ "g7j",					0x080000, 0x0b9e4739, 4 | BRF_GRA },           //  3 Sprites
	{ "g7l",					0x080000, 0x943139ff, 4 | BRF_GRA },           //  4

	{ "a47",					0x040000, 0xc6779dfa, 5 | BRF_SND },           //  5 YM2610 Delta-T Samples

	{ "o5b",					0x100000, 0x07d6cbac, 6 | BRF_SND },           //  6 YM2610 Samples

	{ "peel18cv8-1101a-u15.53",	0x000155, 0xc05e3bea, 7 | BRF_OPT },           //  7 PLDs
	{ "peel18cv8-1103-u112.76",	0x000155, 0x786da44c, 7 | BRF_OPT },           //  8
};

STD_ROM_PICK(pspikesk)
STD_ROM_FN(pspikesk)

struct BurnDriver BurnDrvPspikesk = {
	"pspikesk", "pspikes", NULL, NULL, "1991",
	"Power Spikes (Korea)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pspikeskRomInfo, pspikeskRomName, NULL, NULL, NULL, NULL, PspikesInputInfo, PspikesDIPInfo,
	PspikesInit, DrvExit, DrvFrame, PspikesDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Power Spikes (US)

static struct BurnRomInfo pspikesuRomDesc[] = {
	{ "svolly91.73",			0x040000, 0xbfbffcdb, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "19",						0x020000, 0x7e8ed6e5, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "g7h",					0x080000, 0x74c23c3d, 3 | BRF_GRA },           //  2 Background Tiles

	{ "g7j",					0x080000, 0x0b9e4739, 4 | BRF_GRA },           //  3 Sprites
	{ "g7l",					0x080000, 0x943139ff, 4 | BRF_GRA },           //  4

	{ "a47",					0x040000, 0xc6779dfa, 5 | BRF_SND },           //  5 YM2610 Delta-T Samples

	{ "o5b",					0x100000, 0x07d6cbac, 6 | BRF_SND },           //  6 YM2610 Samples
};

STD_ROM_PICK(pspikesu)
STD_ROM_FN(pspikesu)

struct BurnDriver BurnDrvPspikesu = {
	"pspikesu", "pspikes", NULL, NULL, "1991",
	"Power Spikes (US)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pspikesuRomInfo, pspikesuRomName, NULL, NULL, NULL, NULL, PspikesInputInfo, PspikesDIPInfo,
	PspikesInit, DrvExit, DrvFrame, PspikesDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Super Volley '91 (Japan)

static struct BurnRomInfo svolly91RomDesc[] = {
	{ "u11.jpn",				0x040000, 0xea2e4c82, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "19",						0x020000, 0x7e8ed6e5, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "g7h",					0x080000, 0x74c23c3d, 3 | BRF_GRA },           //  2 Background Tiles

	{ "g7j",					0x080000, 0x0b9e4739, 4 | BRF_GRA },           //  3 Sprites
	{ "g7l",					0x080000, 0x943139ff, 4 | BRF_GRA },           //  4

	{ "a47",					0x040000, 0xc6779dfa, 5 | BRF_SND },           //  5 YM2610 Delta-T Samples

	{ "o5b",					0x100000, 0x07d6cbac, 6 | BRF_SND },           //  6 YM2610 Samples
};

STD_ROM_PICK(svolly91)
STD_ROM_FN(svolly91)

struct BurnDriver BurnDrvSvolly91 = {
	"svolly91", "pspikes", NULL, NULL, "1991",
	"Super Volley '91 (Japan)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, svolly91RomInfo, svolly91RomName, NULL, NULL, NULL, NULL, PspikesInputInfo, PspikesDIPInfo,
	PspikesInit, DrvExit, DrvFrame, PspikesDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Power Spikes (bootleg)

static struct BurnRomInfo pspikesbRomDesc[] = {
	{ "2.ic63",					0x020000, 0xd25e184c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "3.ic62",					0x020000, 0x5add1a34, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "4.ic122",				0x020000, 0xea1c05a7, 2 | BRF_GRA },           //  2 Background Tiles
	{ "5.ic120",				0x020000, 0xbfdc60f4, 2 | BRF_GRA },           //  3
	{ "6.ic118",				0x020000, 0x96a5c235, 2 | BRF_GRA },           //  4
	{ "7.ic116",				0x020000, 0xa7e00b36, 2 | BRF_GRA },           //  5

	{ "8.ic121",				0x040000, 0xfc096cfc, 3 | BRF_GRA },           //  6 Sprites
	{ "9.ic119",				0x040000, 0xa45ec985, 3 | BRF_GRA },           //  7
	{ "10.ic117",				0x040000, 0x3976b372, 3 | BRF_GRA },           //  8
	{ "11.ic115",				0x040000, 0xf9249937, 3 | BRF_GRA },           //  9

	{ "1.ic21",					0x080000, 0x1b78ed0b, 4 | BRF_GRA },           // 10 user1
};

STD_ROM_PICK(pspikesb)
STD_ROM_FN(pspikesb)

struct BurnDriver BurnDrvPspikesb = {
	"pspikesb", "pspikes", NULL, NULL, "1991",
	"Power Spikes (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pspikesbRomInfo, pspikesbRomName, NULL, NULL, NULL, NULL, SpinlbrkInputInfo, PspikesbDIPInfo,
	PspikesbInit, DrvExit, DrvFrame, PspikesbDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Power Spikes (Italian bootleg)

static struct BurnRomInfo pspikesbaRomDesc[] = {
	{ "2.ic63",					0x020000, 0xdd87d28a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "3.ic62",					0x020000, 0xec505317, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "4.ic122",				0x020000, 0xea1c05a7, 2 | BRF_GRA },           //  2 Background Tiles
	{ "5.ic120",				0x020000, 0xbfdc60f4, 2 | BRF_GRA },           //  3
	{ "6.ic118",				0x020000, 0x96a5c235, 2 | BRF_GRA },           //  4
	{ "7.ic116",				0x020000, 0xa7e00b36, 2 | BRF_GRA },           //  5

	{ "8.ic121",				0x040000, 0xfc096cfc, 3 | BRF_GRA },           //  6 Sprites
	{ "9.ic119",				0x040000, 0xa45ec985, 3 | BRF_GRA },           //  7
	{ "10.ic117",				0x040000, 0x3976b372, 3 | BRF_GRA },           //  8
	{ "11.ic115",				0x040000, 0xf9249937, 3 | BRF_GRA },           //  9

	{ "1.ic21",					0x080000, 0x1b78ed0b, 4 | BRF_GRA },           // 10 MSM6295 Samples
};

STD_ROM_PICK(pspikesba)
STD_ROM_FN(pspikesba)

struct BurnDriver BurnDrvPspikesba = {
	"pspikesba", "pspikes", NULL, NULL, "1991",
	"Power Spikes (Italian bootleg)\0", NULL, "bootleg (Playmark?)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pspikesbaRomInfo, pspikesbaRomName, NULL, NULL, NULL, NULL, SpinlbrkInputInfo, PspikesbDIPInfo,
	PspikesbInit, DrvExit, DrvFrame, PspikesbDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Power Spikes (China)

static struct BurnRomInfo pspikescRomDesc[] = {
	{ "27c010.1",				0x020000, 0x06a6ed73, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "27c010.2",				0x020000, 0xff31474e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "vlh30.bin",				0x080000, 0x74c23c3d, 2 | BRF_GRA },           //  2 Background Tiles

	{ "vlh10-vh118.bin",		0x080000, 0x0b9e4739, 3 | BRF_GRA },           //  3 Sprites
	{ "vlh20-vh102.bin",		0x080000, 0x943139ff, 3 | BRF_GRA },           //  4

	{ "vlh40.bin",				0x080000, 0x27166dd4, 4 | BRF_SND },           //  5 MSM6295 Samples
};

STD_ROM_PICK(pspikesc)
STD_ROM_FN(pspikesc)

struct BurnDriver BurnDrvPspikesc = {
	"pspikesc", "pspikes", NULL, NULL, "1991",
	"Power Spikes (China, bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pspikescRomInfo, pspikescRomName, NULL, NULL, NULL, NULL, PspikesInputInfo, PspikescDIPInfo,
	PspikescInit, DrvExit, DrvFrame, PspikesDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// 1991 Spikes (Italian bootleg, set 1)

static struct BurnRomInfo spikes91RomDesc[] = {
	{ "7.ic2",					0x020000, 0x41e38d7e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "8.ic3",					0x020000, 0x9c488daa, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "3.ic76",					0x020000, 0xab451eee, 2 | BRF_GRA },           //  2 Background TIles
	{ "4.ic75",					0x020000, 0xfe857bbd, 2 | BRF_GRA },           //  3
	{ "5.ic74",					0x020000, 0xd7fcd97c, 2 | BRF_GRA },           //  4
	{ "6.ic73",					0x020000, 0xe6b9107f, 2 | BRF_GRA },           //  5

	{ "11.ic118",				0x040000, 0x6e65b4b2, 3 | BRF_GRA },           //  6 Sprites
	{ "12.ic119",				0x040000, 0x60e0d3e0, 3 | BRF_GRA },           //  7
	{ "13.ic120",				0x040000, 0x89213a8c, 3 | BRF_GRA },           //  8
	{ "14.ic121",				0x040000, 0x468cbf5b, 3 | BRF_GRA },           //  9

	{ "10.ic104",				0x010000, 0x769ade77, 4 | BRF_GRA },           // 10 Sprite Look-up Table
	{ "9.ic103",				0x010000, 0x201cb748, 4 | BRF_GRA },           // 11

	{ "1.ic140",				0x010000, 0xe3065b1d, 5 | BRF_PRG | BRF_ESS }, // 12 Z80 Code
	{ "2.ic141",				0x010000, 0x5dd8bf22, 5 | BRF_PRG | BRF_ESS }, // 13

	{ "ep910pc.ic7",			0x000884, 0xe7a3913a, 6 | BRF_GRA },           // 14 user2
};

STD_ROM_PICK(spikes91)
STD_ROM_FN(spikes91)

struct BurnDriver BurnDrvSpikes91 = {
	"spikes91", "pspikes", NULL, NULL, "1991",
	"1991 Spikes (Italian bootleg, set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, spikes91RomInfo, spikes91RomName, NULL, NULL, NULL, NULL, PspikesInputInfo, PspikesDIPInfo,
	Spikes91Init, DrvExit, DrvFrame, Spikes91Draw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// 1991 Spikes (Italian bootleg, set 2)

static struct BurnRomInfo spikes91bRomDesc[] = {
	{ "7.ic2",					0x020000, 0x46433a36, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "8.ic3",					0x020000, 0x9c488daa, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "3.ic76",					0x020000, 0xab451eee, 2 | BRF_GRA },           //  2 Background Tiles
	{ "4.ic75",					0x020000, 0xfe857bbd, 2 | BRF_GRA },           //  3
	{ "5.ic74",					0x020000, 0xd7fcd97c, 2 | BRF_GRA },           //  4
	{ "6.ic73",					0x020000, 0xe6b9107f, 2 | BRF_GRA },           //  5

	{ "11.ic118",				0x040000, 0x6e65b4b2, 3 | BRF_GRA },           //  6 Sprites
	{ "12.ic119",				0x040000, 0x60e0d3e0, 3 | BRF_GRA },           //  7
	{ "13.ic120",				0x040000, 0x89213a8c, 3 | BRF_GRA },           //  8
	{ "14.ic121",				0x040000, 0x468cbf5b, 3 | BRF_GRA },           //  9

	{ "10.ic104",				0x008000, 0xb6fe4e57, 4 | BRF_GRA },           // 10 Sprite Look-up Table
	{ "9.ic103",				0x008000, 0x5479ed35, 4 | BRF_GRA },           // 11

	{ "1.ic140",				0x010000, 0xe3065b1d, 5 | BRF_PRG | BRF_ESS }, // 12 Z80 Code
	{ "2.ic141",				0x010000, 0x5dd8bf22, 5 | BRF_PRG | BRF_ESS }, // 13

	{ "ep910pc.ic7",			0x000884, 0xe7a3913a, 6 | BRF_GRA },           // 14 user2
};

STD_ROM_PICK(spikes91b)
STD_ROM_FN(spikes91b)

struct BurnDriver BurnDrvSpikes91b = {
	"spikes91b", "pspikes", NULL, NULL, "1991",
	"1991 Spikes (Italian bootleg, set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, spikes91bRomInfo, spikes91bRomName, NULL, NULL, NULL, NULL, PspikesInputInfo, PspikesDIPInfo,
	Spikes91Init, DrvExit, DrvFrame, Spikes91Draw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Karate Blazers (World, set 1)

static struct BurnRomInfo karatblzRomDesc[] = {
	{ "rom2v3.u14",				0x040000, 0x01f772e1, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "v1.u15",					0x040000, 0xd16ee21b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "v5.u92",					0x020000, 0x97d67510, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "gha.u55",				0x080000, 0x3e0cea91, 3 | BRF_GRA },           //  3 Background Tiles

	{ "gh9.u61",				0x080000, 0x5d1676bd, 4 | BRF_GRA },           //  4 Foreground Tiles

	{ "u42",					0x100000, 0x65f0da84, 5 | BRF_GRA },           //  5 Sprite Chip #0 Tiles
	{ "v3.u44",					0x020000, 0x34bdead2, 5 | BRF_GRA },           //  6
	{ "u43",					0x100000, 0x7b349e5d, 5 | BRF_GRA },           //  7
	{ "v4.u45",					0x020000, 0xbe4d487d, 5 | BRF_GRA },           //  8

	{ "u59.ghb",				0x080000, 0x158c9cde, 6 | BRF_GRA },           //  9 Sprite Chip #1 Tiles
	{ "ghd.u60",				0x080000, 0x73180ae3, 6 | BRF_GRA },           // 10

	{ "u105.gh8",				0x080000, 0x7a68cb1b, 7 | BRF_SND },           // 11 YM2610 Delta-T Samples

	{ "u104",					0x100000, 0x5795e884, 8 | BRF_SND },           // 12 YM2610 Samples
};

STD_ROM_PICK(karatblz)
STD_ROM_FN(karatblz)

struct BurnDriver BurnDrvKaratblz = {
	"karatblz", NULL, NULL, NULL, "1991",
	"Karate Blazers (World, set 1)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, karatblzRomInfo, karatblzRomName, NULL, NULL, NULL, NULL, KaratblzInputInfo, KaratblzDIPInfo,
	KaratblzInit, DrvExit, DrvFrame, KaratblzDraw, DrvScan, &DrvRecalc, 0x400,
	352, 240, 4, 3
};


// Karate Blazers (World, Tecmo license)

static struct BurnRomInfo karatblztRomDesc[] = {
	{ "2v2.u14",				0x040000, 0x7ae17b7f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "v1.u15",					0x040000, 0xd16ee21b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "v5.u92",					0x020000, 0x97d67510, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "gha.u55",				0x080000, 0x3e0cea91, 3 | BRF_GRA },           //  3 Background Tiles

	{ "gh9.u61",				0x080000, 0x5d1676bd, 4 | BRF_GRA },           //  4 Foreground Tiles

	{ "u42",					0x100000, 0x65f0da84, 5 | BRF_GRA },           //  5 Sprite Chip #0 Tiles
	{ "v3.u44",					0x020000, 0x34bdead2, 5 | BRF_GRA },           //  6
	{ "u43",					0x100000, 0x7b349e5d, 5 | BRF_GRA },           //  7
	{ "v4.u45",					0x020000, 0xbe4d487d, 5 | BRF_GRA },           //  8

	{ "u59.ghb",				0x080000, 0x158c9cde, 6 | BRF_GRA },           //  9 Sprite Chip #1 Tiles
	{ "ghd.u60",				0x080000, 0x73180ae3, 6 | BRF_GRA },           // 10

	{ "u105.gh8",				0x080000, 0x7a68cb1b, 7 | BRF_SND },           // 11 YM2610 Delta-T Samples

	{ "u104",					0x100000, 0x5795e884, 8 | BRF_SND },           // 12 YM2610 Samples
};

STD_ROM_PICK(karatblzt)
STD_ROM_FN(karatblzt)

struct BurnDriver BurnDrvKaratblzt = {
	"karatblzt", "karatblz", NULL, NULL, "1991",
	"Karate Blazers (World, Tecmo license)\0", NULL, "Video System Co. (Tecmo license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, karatblztRomInfo, karatblztRomName, NULL, NULL, NULL, NULL, KaratblzInputInfo, KaratblzDIPInfo,
	KaratblzInit, DrvExit, DrvFrame, KaratblzDraw, DrvScan, &DrvRecalc, 0x400,
	352, 240, 4, 3
};


// Karate Blazers (World, set 2)

static struct BurnRomInfo karatblzaRomDesc[] = {
	{ "_v2.u14",				0x040000, 0x7a78976e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "_v1.u15",				0x040000, 0x47e410fe, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "v5.u92",					0x020000, 0x97d67510, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "gha.u55",				0x080000, 0x3e0cea91, 3 | BRF_GRA },           //  3 Background Tiles

	{ "gh9.u61",				0x080000, 0x5d1676bd, 4 | BRF_GRA },           //  4 Foreground Tiles

	{ "u42",					0x100000, 0x65f0da84, 5 | BRF_GRA },           //  5 Sprite Chip #0 Tiles
	{ "v3.u44",					0x020000, 0x34bdead2, 5 | BRF_GRA },           //  6
	{ "u43",					0x100000, 0x7b349e5d, 5 | BRF_GRA },           //  7
	{ "v4.u45",					0x020000, 0xbe4d487d, 5 | BRF_GRA },           //  8

	{ "u59.ghb",				0x080000, 0x158c9cde, 6 | BRF_GRA },           //  9 Sprite Chip #1 Tiles
	{ "ghd.u60",				0x080000, 0x73180ae3, 6 | BRF_GRA },           // 10

	{ "u105.gh8",				0x080000, 0x7a68cb1b, 7 | BRF_SND },           // 11 YM2610 Delta-T Samples

	{ "u104",					0x100000, 0x5795e884, 8 | BRF_SND },           // 12 YM2610 Samples
};

STD_ROM_PICK(karatblza)
STD_ROM_FN(karatblza)

struct BurnDriver BurnDrvKaratblza = {
	"karatblza", "karatblz", NULL, NULL, "1991",
	"Karate Blazers (World, set 2)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, karatblzaRomInfo, karatblzaRomName, NULL, NULL, NULL, NULL, KaratblzInputInfo, KaratblzDIPInfo,
	KaratblzInit, DrvExit, DrvFrame, KaratblzDraw, DrvScan, &DrvRecalc, 0x400,
	352, 240, 4, 3
};


// Karate Blazers (US)

static struct BurnRomInfo karatblzuRomDesc[] = {
	{ "1v2.u14",				0x040000, 0x202e6220, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "v1.u15",					0x040000, 0xd16ee21b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "v5.u92",					0x020000, 0x97d67510, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "gha.u55",				0x080000, 0x3e0cea91, 3 | BRF_GRA },           //  3 Background Tiles

	{ "gh9.u61",				0x080000, 0x5d1676bd, 4 | BRF_GRA },           //  4 Foreground Tiles

	{ "u42",					0x100000, 0x65f0da84, 5 | BRF_GRA },           //  5 Sprite Chip #0 Tiles
	{ "v3.u44",					0x020000, 0x34bdead2, 5 | BRF_GRA },           //  6
	{ "u43",					0x100000, 0x7b349e5d, 5 | BRF_GRA },           //  7
	{ "v4.u45",					0x020000, 0xbe4d487d, 5 | BRF_GRA },           //  8

	{ "u59.ghb",				0x080000, 0x158c9cde, 6 | BRF_GRA },           //  9 Sprite Chip #1 Tiles
	{ "ghd.u60",				0x080000, 0x73180ae3, 6 | BRF_GRA },           // 10

	{ "u105.gh8",				0x080000, 0x7a68cb1b, 7 | BRF_SND },           // 11 YM2610 Delta-T Samples

	{ "u104",					0x100000, 0x5795e884, 8 | BRF_SND },           // 12 YM2610 Samples
};

STD_ROM_PICK(karatblzu)
STD_ROM_FN(karatblzu)

struct BurnDriver BurnDrvKaratblzu = {
	"karatblzu", "karatblz", NULL, NULL, "1991",
	"Karate Blazers (US)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, karatblzuRomInfo, karatblzuRomName, NULL, NULL, NULL, NULL, KaratblzInputInfo, KaratblzDIPInfo,
	KaratblzInit, DrvExit, DrvFrame, KaratblzDraw, DrvScan, &DrvRecalc, 0x400,
	352, 240, 4, 3
};


// Toushin Blazers (Japan, Tecmo license)

static struct BurnRomInfo karatblzjRomDesc[] = {
	{ "2tecmo.u14",				0x040000, 0x57e52654, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "v1.u15",					0x040000, 0xd16ee21b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "v5.u92",					0x020000, 0x97d67510, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "gha.u55",				0x080000, 0x3e0cea91, 3 | BRF_GRA },           //  3 Background Tiles

	{ "gh9.u61",				0x080000, 0x5d1676bd, 4 | BRF_GRA },           //  4 Foreground Tiles

	{ "u42",					0x100000, 0x65f0da84, 5 | BRF_GRA },           //  5 Sprite Chip #0 Tiles
	{ "v3.u44",					0x020000, 0x34bdead2, 5 | BRF_GRA },           //  6
	{ "u43",					0x100000, 0x7b349e5d, 5 | BRF_GRA },           //  7
	{ "v4.u45",					0x020000, 0xbe4d487d, 5 | BRF_GRA },           //  8

	{ "u59.ghb",				0x080000, 0x158c9cde, 6 | BRF_GRA },           //  9 Sprite Chip #1 Tiles
	{ "ghd.u60",				0x080000, 0x73180ae3, 6 | BRF_GRA },           // 10

	{ "u105.gh8",				0x080000, 0x7a68cb1b, 7 | BRF_SND },           // 11 YM2610 Delta-T Samples

	{ "u104",					0x100000, 0x5795e884, 8 | BRF_SND },           // 12 YM2610 Samples
};

STD_ROM_PICK(karatblzj)
STD_ROM_FN(karatblzj)

struct BurnDriver BurnDrvKaratblzj = {
	"karatblzj", "karatblz", NULL, NULL, "1991",
	"Toushin Blazers (Japan, Tecmo license)\0", NULL, "Video System Co. (Tecmo license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, karatblzjRomInfo, karatblzjRomName, NULL, NULL, NULL, NULL, KaratblzInputInfo, KaratblzDIPInfo,
	KaratblzInit, DrvExit, DrvFrame, KaratblzDraw, DrvScan, &DrvRecalc, 0x400,
	352, 240, 4, 3
};


// Karate Blazers (bootleg with Street Smart sound hardware)

static struct BurnRomInfo karatblzblRomDesc[] = {
	{ "9.u5",					0x040000, 0x33c3d3cd, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "10.u2",					0x040000, 0xdbed7323, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "2.u80",					0x010000, 0xca4b171e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "1.u88",					0x020000, 0x47db1605, 3 | BRF_SND },           //  3 UPD7759 Samples

	{ "3.u81",					0x080000, 0x3e0cea91, 4 | BRF_GRA },           //  4 Background Tiles

	{ "4.u82",					0x080000, 0x5d1676bd, 5 | BRF_GRA },           //  5 Foreground Tiles

	{ "gfx14.u58",				0x020000, 0x41e18169, 6 | BRF_GRA },           //  6 Sprite Chip #0 Tiles
	{ "gfx19.u63",				0x020000, 0x09833f2a, 6 | BRF_GRA },           //  7
	{ "gfx13.u59",				0x020000, 0x666a8c19, 6 | BRF_GRA },           //  8
	{ "gfx18.u64",				0x020000, 0x4be67468, 6 | BRF_GRA },           //  9
	{ "gfx12.u60",				0x020000, 0xf425fe4c, 6 | BRF_GRA },           // 10
	{ "gfx17.u65",				0x020000, 0x96e77e04, 6 | BRF_GRA },           // 11
	{ "gfx11.u61",				0x020000, 0x09ae152b, 6 | BRF_GRA },           // 12
	{ "gfx16.u66",				0x020000, 0xcc3a2c8f, 6 | BRF_GRA },           // 13
	{ "gfx15.u57",				0x020000, 0x876cf42e, 6 | BRF_GRA },           // 14
	{ "gfx20.u62",				0x020000, 0x8b759fde, 6 | BRF_GRA },           // 15
	{ "gfx24.u68",				0x020000, 0xa4417bf8, 6 | BRF_GRA },           // 16
	{ "gfx29.u73",				0x020000, 0x5affb5d0, 6 | BRF_GRA },           // 17
	{ "gfx23.u69",				0x020000, 0x3ff332e2, 6 | BRF_GRA },           // 18
	{ "gfx28.u74",				0x020000, 0xccfdd9ad, 6 | BRF_GRA },           // 19
	{ "gfx22.u70",				0x020000, 0x5d673b74, 6 | BRF_GRA },           // 20
	{ "gfx27.u75",				0x020000, 0xa38802d4, 6 | BRF_GRA },           // 21
	{ "gfx21.u71",				0x020000, 0xffd66ea0, 6 | BRF_GRA },           // 22
	{ "gfx26.u76",				0x020000, 0x7ae76103, 6 | BRF_GRA },           // 23
	{ "gfx25.u67",				0x020000, 0x1195b559, 6 | BRF_GRA },           // 24
	{ "gfx30.u72",				0x020000, 0x7593679f, 6 | BRF_GRA },           // 25

	{ "5.u62",					0x040000, 0x1ed12174, 7 | BRF_GRA },           // 26 Sprite Chip #1 Tiles
	{ "6.u63",					0x040000, 0x874c5251, 7 | BRF_GRA },           // 27
	{ "7.u64",					0x040000, 0xc2ed2666, 7 | BRF_GRA },           // 28
	{ "8.u65",					0x040000, 0xb491201a, 7 | BRF_GRA },           // 29
};

STD_ROM_PICK(karatblzbl)
STD_ROM_FN(karatblzbl)

struct BurnDriver BurnDrvKaratblzbl = {
	"karatblzbl", "karatblz", NULL, NULL, "1991",
	"Karate Blazers (bootleg with Street Smart sound hardware)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, karatblzblRomInfo, karatblzblRomName, NULL, NULL, NULL, NULL, KaratblzInputInfo, KaratblzDIPInfo,
	KaratblzblInit, DrvExit, DrvFrame, KaratblzDraw, DrvScan, &DrvRecalc, 0x400,
	352, 240, 4, 3
};


// Turbo Force (World, set 1)

static struct BurnRomInfo turbofrcRomDesc[] = {
	{ "4v2.subpcb.u2",			0x040000, 0x721300ee, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "4v1.subpcb.u1",			0x040000, 0x6cd5312b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4v3.u14",				0x040000, 0x63f50557, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "6.u166",					0x020000, 0x2ca14a65, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "lh534ggs.u94",			0x080000, 0xbaa53978, 3 | BRF_GRA },           //  4 Background Tiles
	{ "7.u95",					0x020000, 0x71a6c573, 3 | BRF_GRA },           //  5

	{ "lh534ggy.u105",			0x080000, 0x4de4e59e, 4 | BRF_GRA },           //  6 Foreground Tiles
	{ "8.u106",					0x020000, 0xc6479eb5, 4 | BRF_GRA },           //  7

	{ "lh534gh2.u116",			0x080000, 0xdf210f3b, 5 | BRF_GRA },           //  8 Sprite Chip #0 Tiles
	{ "5.u118",					0x040000, 0xf61d1d79, 5 | BRF_GRA },           //  9
	{ "lh534gh1.u117",			0x080000, 0xf70812fd, 5 | BRF_GRA },           // 10
	{ "4.u119",					0x040000, 0x474ea716, 5 | BRF_GRA },           // 11

	{ "lh532a52.u134",			0x040000, 0x3c725a48, 6 | BRF_GRA },           // 12 Sprite Chip #1 Tiles
	{ "lh532a51.u135",			0x040000, 0x95c63559, 6 | BRF_GRA },           // 13

	{ "lh532h74.u180",			0x040000, 0xa3d43254, 7 | BRF_SND },           // 14 YM2610 Delta-T Samples

	{ "lh538o7j.u179",			0x100000, 0x60ca0333, 8 | BRF_SND },           // 15 YM2610 Samples
};

STD_ROM_PICK(turbofrc)
STD_ROM_FN(turbofrc)

struct BurnDriver BurnDrvTurbofrc = {
	"turbofrc", NULL, NULL, NULL, "1991",
	"Turbo Force (World, set 1)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 3, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, turbofrcRomInfo, turbofrcRomName, NULL, NULL, NULL, NULL, TurbofrcInputInfo, TurbofrcDIPInfo,
	TurbofrcInit, DrvExit, DrvFrame, TurbofrcDraw, DrvScan, &DrvRecalc, 0x400,
	240, 352, 3, 4
};


// Turbo Force (World, set 2)

static struct BurnRomInfo turbofrcoRomDesc[] = {
	{ "3v2.subpcb.u2",			0x040000, 0x721300ee, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "3v1.subpcb.u1",			0x040000, 0x71b6431b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3v3.u14",				0x040000, 0x63f50557, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "6.u166",					0x020000, 0x2ca14a65, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "lh534ggs.u94",			0x080000, 0xbaa53978, 3 | BRF_GRA },           //  4 Background Tiles
	{ "7.u95",					0x020000, 0x71a6c573, 3 | BRF_GRA },           //  5

	{ "lh534ggy.u105",			0x080000, 0x4de4e59e, 4 | BRF_GRA },           //  6 Foreground Tiles
	{ "8.u106",					0x020000, 0xc6479eb5, 4 | BRF_GRA },           //  7

	{ "lh534gh2.u116",			0x080000, 0xdf210f3b, 5 | BRF_GRA },           //  8 Sprite Chip #0 Tiles
	{ "5.u118",					0x040000, 0xf61d1d79, 5 | BRF_GRA },           //  9
	{ "lh534gh1.u117",			0x080000, 0xf70812fd, 5 | BRF_GRA },           // 10
	{ "4.u119",					0x040000, 0x474ea716, 5 | BRF_GRA },           // 11

	{ "lh532a52.u134",			0x040000, 0x3c725a48, 6 | BRF_GRA },           // 12 Sprite Chip #1 Tiles
	{ "lh532a51.u135",			0x040000, 0x95c63559, 6 | BRF_GRA },           // 13

	{ "lh532h74.u180",			0x040000, 0xa3d43254, 7 | BRF_SND },           // 14 YM2610 Delta-T Samples

	{ "lh538o7j.u179",			0x100000, 0x60ca0333, 8 | BRF_SND },           // 15 YM2610 Samples
};

STD_ROM_PICK(turbofrco)
STD_ROM_FN(turbofrco)

struct BurnDriver BurnDrvTurbofrco = {
	"turbofrco", "turbofrc", NULL, NULL, "1991",
	"Turbo Force (World, set 2)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 3, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, turbofrcoRomInfo, turbofrcoRomName, NULL, NULL, NULL, NULL, TurbofrcInputInfo, TurbofrcDIPInfo,
	TurbofrcInit, DrvExit, DrvFrame, TurbofrcDraw, DrvScan, &DrvRecalc, 0x400,
	240, 352, 3, 4
};


// Turbo Force (US, set 1)

static struct BurnRomInfo turbofrcuRomDesc[] = {
	{ "8v2.subpcb.u2",			0x040000, 0x721300ee, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "8v1.subpcb.u1",			0x040000, 0xcc324da6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "8v3.u14",				0x040000, 0xc0a15480, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "6.u166",					0x020000, 0x2ca14a65, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "lh534ggs.u94",			0x080000, 0xbaa53978, 3 | BRF_GRA },           //  4 Background Tiles
	{ "7.u95",					0x020000, 0x71a6c573, 3 | BRF_GRA },           //  5

	{ "lh534ggy.u105",			0x080000, 0x4de4e59e, 4 | BRF_GRA },           //  6 Foreground Tiles
	{ "8.u106",					0x020000, 0xc6479eb5, 4 | BRF_GRA },           //  7

	{ "lh534gh2.u116",			0x080000, 0xdf210f3b, 5 | BRF_GRA },           //  8 Sprite Chip #0 Tiles
	{ "5.u118",					0x040000, 0xf61d1d79, 5 | BRF_GRA },           //  9
	{ "lh534gh1.u117",			0x080000, 0xf70812fd, 5 | BRF_GRA },           // 10
	{ "4.u119",					0x040000, 0x474ea716, 5 | BRF_GRA },           // 11

	{ "lh532a52.u134",			0x040000, 0x3c725a48, 6 | BRF_GRA },           // 12 Sprite Chip #1 Tiles
	{ "lh532a51.u135",			0x040000, 0x95c63559, 6 | BRF_GRA },           // 13

	{ "lh532h74.u180",			0x040000, 0xa3d43254, 7 | BRF_SND },           // 14 YM2610 Delta-T Samples

	{ "lh538o7j.u179",			0x100000, 0x60ca0333, 8 | BRF_SND },           // 15 YM2610 Samples
};

STD_ROM_PICK(turbofrcu)
STD_ROM_FN(turbofrcu)

struct BurnDriver BurnDrvTurbofrcu = {
	"turbofrcu", "turbofrc", NULL, NULL, "1991",
	"Turbo Force (US, set 1)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 3, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, turbofrcuRomInfo, turbofrcuRomName, NULL, NULL, NULL, NULL, TurbofrcInputInfo, TurbofrcDIPInfo,
	TurbofrcInit, DrvExit, DrvFrame, TurbofrcDraw, DrvScan, &DrvRecalc, 0x400,
	240, 352, 3, 4
};


// Turbo Force (US, set 2)

static struct BurnRomInfo turbofrcuaRomDesc[] = {
	{ "7v2.subpcb.u2",			0x040000, 0x721300ee, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "7v1.subpcb.u1",			0x040000, 0xd1513f96, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "7v3.u14",				0x040000, 0x63f50557, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "6.u166",					0x020000, 0x2ca14a65, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "lh534ggs.u94",			0x080000, 0xbaa53978, 3 | BRF_GRA },           //  4 Background Tiles
	{ "7.u95",					0x020000, 0x71a6c573, 3 | BRF_GRA },           //  5
	
	{ "lh534ggy.u105",			0x080000, 0x4de4e59e, 4 | BRF_GRA },           //  6 Foreground Tiles
	{ "8.u106",					0x020000, 0xc6479eb5, 4 | BRF_GRA },           //  7
	
	{ "lh534gh2.u116",			0x080000, 0xdf210f3b, 5 | BRF_GRA },           //  8 Sprite Chip #0 Tiles
	{ "5.u118",					0x040000, 0xf61d1d79, 5 | BRF_GRA },           //  9
	{ "lh534gh1.u117",			0x080000, 0xf70812fd, 5 | BRF_GRA },           // 10
	{ "4.u119",					0x040000, 0x474ea716, 5 | BRF_GRA },           // 11

	{ "lh532a52.u134",			0x040000, 0x3c725a48, 6 | BRF_GRA },           // 12 Sprite Chip #1 Tiles
	{ "lh532a51.u135",			0x040000, 0x95c63559, 6 | BRF_GRA },           // 13
	
	{ "lh532h74.u180",			0x040000, 0xa3d43254, 7 | BRF_SND },           // 14 YM2610 Delta-T Samples

	{ "lh538o7j.u179",			0x100000, 0x60ca0333, 8 | BRF_SND },           // 15 YM2610 Samples
};

STD_ROM_PICK(turbofrcua)
STD_ROM_FN(turbofrcua)

struct BurnDriver BurnDrvTurbofrcua = {
	"turbofrcua", "turbofrc", NULL, NULL, "1991",
	"Turbo Force (US, set 2)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 3, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, turbofrcuaRomInfo, turbofrcuaRomName, NULL, NULL, NULL, NULL, TurbofrcInputInfo, TurbofrcDIPInfo,
	TurbofrcInit, DrvExit, DrvFrame, TurbofrcDraw, DrvScan, &DrvRecalc, 0x400,
	240, 352, 3, 4
};


// Aero Fighters (World / USA + Canada / Korea / Hong Kong / Taiwan) (newer hardware)

static struct BurnRomInfo aerofgtRomDesc[] = {
	{ "1.u4",					0x080000, 0x6fdff0a2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code

	{ "2.153",					0x020000, 0xa1ef64ec, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "538a54.124",				0x080000, 0x4d2c4df2, 3 | BRF_GRA },           //  2 Background Tiles
	{ "1538a54.124",			0x080000, 0x286d109e, 3 | BRF_GRA },           //  3

	{ "538a53.u9",				0x100000, 0x630d8e0b, 4 | BRF_GRA },           //  4 Sprites
	{ "534g8f.u18",				0x080000, 0x76ce0926, 4 | BRF_GRA },           //  5

	{ "it-19-01",				0x040000, 0x6d42723d, 5 | BRF_SND },           //  6 YM2610 Delta-T Samples

	{ "it-19-06",				0x100000, 0xcdbbdb1d, 6 | BRF_SND },           //  7 YM2610 Samples
};

STD_ROM_PICK(aerofgt)
STD_ROM_FN(aerofgt)

struct BurnDriver BurnDrvAerofgt = {
	"aerofgt", NULL, NULL, NULL, "1992",
	"Aero Fighters (World / USA + Canada / Korea / Hong Kong / Taiwan) (newer hardware)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, aerofgtRomInfo, aerofgtRomName, NULL, NULL, NULL, NULL, AerofgtInputInfo, AerofgtDIPInfo,
	AerofgtInit, DrvExit, DrvFrame, AerofgtDraw, DrvScan, &DrvRecalc, 0x400,
	224, 320, 3, 4
};


// Aero Fighters (Taiwan / Japan, set 1)

static struct BurnRomInfo aerofgtbRomDesc[] = {
	{ "v2",						0x040000, 0x5c9de9f0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "v1",						0x040000, 0x89c1dcf4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "v3",						0x020000, 0xcbb18cf4, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "it-19-03",				0x080000, 0x85eba1a4, 3 | BRF_GRA },           //  3 Background Tiles

	{ "it-19-02",				0x080000, 0x4f57f8ba, 4 | BRF_GRA },           //  4 Foreground Tiles

	{ "it-19-04",				0x080000, 0x3b329c1f, 5 | BRF_GRA },           //  5 Sprite Chip #0 Tiles
	{ "it-19-05",				0x080000, 0x02b525af, 5 | BRF_GRA },           //  6

	{ "g27",					0x040000, 0x4d89cbc8, 6 | BRF_GRA },           //  7 Sprite Chip #1 Tiles
	{ "g26",					0x040000, 0x8072c1d2, 6 | BRF_GRA },           //  8

	{ "it-19-01",				0x040000, 0x6d42723d, 7 | BRF_SND },           //  9 YM2610 Delta-T Samples

	{ "it-19-06",				0x100000, 0xcdbbdb1d, 8 | BRF_SND },           // 10 YM2610 Samples
};

STD_ROM_PICK(aerofgtb)
STD_ROM_FN(aerofgtb)

struct BurnDriver BurnDrvAerofgtb = {
	"aerofgtb", "aerofgt", NULL, NULL, "1992",
	"Aero Fighters (Taiwan / Japan, set 1)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, aerofgtbRomInfo, aerofgtbRomName, NULL, NULL, NULL, NULL, AerofgtbInputInfo, AerofgtbDIPInfo,
	AerofgtbInit, DrvExit, DrvFrame, TurbofrcDraw, DrvScan, &DrvRecalc, 0x400,
	224, 320, 3, 4
};


// Aero Fighters (bootleg, set 1)

static struct BurnRomInfo aerfbootRomDesc[] = {
	{ "afb_ep2.u3",				0x040000, 0x2bb9edf7, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "afb_ep3.u2",				0x040000, 0x475d3df3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "afb_ep1.u17",			0x008000, 0xd41b5ab2, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "afb_ep9.hh",				0x040000, 0x41233923, 3 | BRF_GRA },           //  3 Background & Foreground Tiles
	{ "afb_ep8.hi",				0x040000, 0x97607ad3, 3 | BRF_GRA },           //  4
	{ "afb_ep7.hj",				0x040000, 0x01dc793e, 3 | BRF_GRA },           //  5
	{ "afb_ep6.hk",				0x040000, 0xcad7862a, 3 | BRF_GRA },           //  6

	{ "afb_ep12.tc",			0x080000, 0x1e692065, 4 | BRF_GRA },           //  7 Sprite Chip #0 Tiles
	{ "afb_ep10.ta",			0x080000, 0xe50db1a7, 4 | BRF_GRA },           //  8

	{ "afb_ep13.td",			0x040000, 0x1830f70c, 5 | BRF_GRA },           //  9 Sprite Chip #1 Tiles
	{ "afb_ep11.tb",			0x040000, 0x6298c0eb, 5 | BRF_GRA },           // 10

	{ "afb_ep5.u29",			0x020000, 0x3559609a, 6 | BRF_SND },           // 11 MSM6295 Samples
	{ "afb_ep4.u30",			0x080000, 0xf9652163, 6 | BRF_SND },           // 12
};

STD_ROM_PICK(aerfboot)
STD_ROM_FN(aerfboot)

struct BurnDriver BurnDrvAerfboot = {
	"aerfboot", "aerofgt", NULL, NULL, "1992",
	"Aero Fighters (bootleg, set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, aerfbootRomInfo, aerfbootRomName, NULL, NULL, NULL, NULL, AerofgtbInputInfo, AerofgtbDIPInfo,
	AerfbootInit, DrvExit, DrvFrame, AerfbootDraw, DrvScan, &DrvRecalc, 0x400,
	224, 320, 3, 4
};


// Aero Fighters (bootleg, set 2)

static struct BurnRomInfo aerfboo2RomDesc[] = {
	{ "p2",						0x040000, 0x6c4ec09b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "p1",						0x040000, 0x841c513a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "g5",						0x080000, 0x1c2bd86c, 2 | BRF_GRA },           //  2 Background Tiles

	{ "g6",						0x080000, 0xb9b1b9b0, 3 | BRF_GRA },           //  3 Foreground Tiles

	{ "g2",						0x080000, 0x84774dbd, 4 | BRF_GRA },           //  4 Sprites
	{ "g1",						0x080000, 0x4ab31e69, 4 | BRF_GRA },           //  5
	{ "g4",						0x080000, 0x97725694, 4 | BRF_GRA },           //  6
	{ "g3",						0x080000, 0x7be8cef0, 4 | BRF_GRA },           //  7

	{ "s2",						0x080000, 0x2e316ee8, 5 | BRF_SND },           //  8 MSM6295 Samples
	{ "s1",						0x080000, 0x9e09813d, 5 | BRF_SND },           //  9
};

STD_ROM_PICK(aerfboo2)
STD_ROM_FN(aerfboo2)

struct BurnDriver BurnDrvAerfboo2 = {
	"aerfboo2", "aerofgt", NULL, NULL, "1992",
	"Aero Fighters (bootleg, set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, aerfboo2RomInfo, aerfboo2RomName, NULL, NULL, NULL, NULL, AerofgtbInputInfo, AerofgtbDIPInfo,
	Aerfboo2Init, DrvExit, DrvFrame, Aerfboo2Draw, DrvScan, &DrvRecalc, 0x400,
	224, 320, 3, 4
};


// Aero Fighters (Taiwan / Japan, set 2)

static struct BurnRomInfo aerofgtcRomDesc[] = {
	{ "v2.149",					0x040000, 0xf187aec6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "v1.111",					0x040000, 0x9e684b19, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "2.153",					0x020000, 0xa1ef64ec, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "it-19-03",				0x080000, 0x85eba1a4, 3 | BRF_GRA },           //  3 Background Tiles

	{ "it-19-02",				0x080000, 0x4f57f8ba, 4 | BRF_GRA },           //  4 Foreground Tiles

	{ "it-19-04",				0x080000, 0x3b329c1f, 5 | BRF_GRA },           //  5 Sprite Chip #0 Tiles
	{ "it-19-05",				0x080000, 0x02b525af, 5 | BRF_GRA },           //  6

	{ "g27",					0x040000, 0x4d89cbc8, 6 | BRF_GRA },           //  7 Sprite Chip #1 Tiles
	{ "g26",					0x040000, 0x8072c1d2, 6 | BRF_GRA },           //  8

	{ "it-19-01",				0x040000, 0x6d42723d, 7 | BRF_SND },           //  9 YM2610 Delta-T Samples

	{ "it-19-06",				0x100000, 0xcdbbdb1d, 8 | BRF_SND },           // 10 YM2610 Samples
};

STD_ROM_PICK(aerofgtc)
STD_ROM_FN(aerofgtc)

struct BurnDriver BurnDrvAerofgtc = {
	"aerofgtc", "aerofgt", NULL, NULL, "1992",
	"Aero Fighters (Taiwan / Japan, set 2)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, aerofgtcRomInfo, aerofgtcRomName, NULL, NULL, NULL, NULL, AerofgtbInputInfo, AerofgtbDIPInfo,
	AerofgtbInit, DrvExit, DrvFrame, TurbofrcDraw, DrvScan, &DrvRecalc, 0x400,
	224, 320, 3, 4
};


// Sonic Wings (Japan)

static struct BurnRomInfo sonicwiRomDesc[] = {
	{ "2.149",					0x040000, 0x3d1b96ba, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "1.111",					0x040000, 0xa3d09f94, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "2.153",					0x020000, 0xa1ef64ec, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "it-19-03",				0x080000, 0x85eba1a4, 3 | BRF_GRA },           //  3 Background Tiles

	{ "it-19-02",				0x080000, 0x4f57f8ba, 4 | BRF_GRA },           //  4 Foreground Tiles

	{ "it-19-04",				0x080000, 0x3b329c1f, 5 | BRF_GRA },           //  5 Sprite Chip #0 Tiles
	{ "it-19-05",				0x080000, 0x02b525af, 5 | BRF_GRA },           //  6

	{ "g27",					0x040000, 0x4d89cbc8, 6 | BRF_GRA },           //  7 Sprite Chip #1 Tiles
	{ "g26",					0x040000, 0x8072c1d2, 6 | BRF_GRA },           //  8

	{ "it-19-01",				0x040000, 0x6d42723d, 7 | BRF_SND },           //  9 YM2610 Delta-T Samples

	{ "it-19-06",				0x100000, 0xcdbbdb1d, 8 | BRF_SND },           // 10 YM2610 Samples
};

STD_ROM_PICK(sonicwi)
STD_ROM_FN(sonicwi)

struct BurnDriver BurnDrvSonicwi = {
	"sonicwi", "aerofgt", NULL, NULL, "1992",
	"Sonic Wings (Japan)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, sonicwiRomInfo, sonicwiRomName, NULL, NULL, NULL, NULL, AerofgtbInputInfo, AerofgtbDIPInfo,
	AerofgtbInit, DrvExit, DrvFrame, TurbofrcDraw, DrvScan, &DrvRecalc, 0x400,
	224, 320, 3, 4
};


// Kick Ball

static struct BurnRomInfo kickballRomDesc[] = {
	{ "kickball.1",				0x040000, 0xf0fd971d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "kickball.2",				0x040000, 0x7dab432d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "kickball.4",				0x010000, 0xef10c2bf, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "kickball.9",				0x080000, 0x19be87f3, 3 | BRF_GRA },           //  3 Background Tiles
	{ "kickball.10",			0x080000, 0xe3b4f894, 3 | BRF_GRA },           //  4

	{ "kickball.5",				0x080000, 0x050b6387, 4 | BRF_GRA },           //  5 Sprites
	{ "kickball.6",				0x080000, 0x1e55252f, 4 | BRF_GRA },           //  6
	{ "kickball.7",				0x080000, 0xb2ee5218, 4 | BRF_GRA },           //  7
	{ "kickball.8",				0x080000, 0x5f1b07f8, 4 | BRF_GRA },           //  8

	{ "kickball.3",				0x040000, 0x2f3ed4c1, 5 | BRF_SND },           //  9 MSM6295 Samples
};

STD_ROM_PICK(kickball)
STD_ROM_FN(kickball)

struct BurnDriver BurnDrvKickball = {
	"kickball", NULL, NULL, NULL, "1998",
	"Kick Ball\0", NULL, "Seoung Youn", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, kickballRomInfo, kickballRomName, NULL, NULL, NULL, NULL, PspikesInputInfo, PspikesDIPInfo,
	KickballInit, DrvExit, DrvFrame, PspikesDraw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};


// Beach Festival World Championship 1997

static struct BurnRomInfo wbbc97RomDesc[] = {
	{ "03.27c040.ur4.rom",		0x080000, 0xfb4e48fc, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "07.27c040.uo4.rom",		0x080000, 0x87605dcc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "04.27c4000.ur4a.rom",	0x080000, 0x2dd6ff07, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "08.27c4000.uo4a.rom",	0x080000, 0x1b96ef5b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "05.27c4000.ur4b.rom",	0x080000, 0x84104886, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "09.27c4000.uo4b.rom",	0x080000, 0x0367043c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "06.27c4000.ur4c.rom",	0x080000, 0xb22d11c4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "10.27c040.uo4c.rom",		0x080000, 0xfe403e8b, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "02.27c512.su11.rom",		0x010000, 0xf03178e9, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 Code

	{ "15.27c020.uu10.rom",		0x040000, 0x965bc99e, 3 | BRF_GRA },           //  9 Background Tiles

	{ "11.27c020.ue12.rom",		0x040000, 0xa0b23c8a, 4 | BRF_GRA },           // 10 Sprites
	{ "12.27c020.ue11.rom",		0x040000, 0x4e529623, 4 | BRF_GRA },           // 11
	{ "13.27c020.ue10.rom",		0x040000, 0x3745f892, 4 | BRF_GRA },           // 12
	{ "14.27c020.ue9.rom",		0x040000, 0x2814f4d2, 4 | BRF_GRA },           // 13

	{ "01.27c020.su10.rom",		0x040000, 0xc024e48c, 5 | BRF_SND },           // 14 MSM6295 Samples

	{ "82s147a.rom",			0x000200, 0x72cec9d2, 0 | BRF_OPT },           // 15 Unknown
};

STD_ROM_PICK(wbbc97)
STD_ROM_FN(wbbc97)

struct BurnDriver BurnDrvWbbc97 = {
	"wbbc97", NULL, NULL, NULL, "1997",
	"Beach Festival World Championship 1997\0", NULL, "Comad", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, wbbc97RomInfo, wbbc97RomName, NULL, NULL, NULL, NULL, PspikesInputInfo, PspikesDIPInfo,
	Wbbc97Init, DrvExit, DrvFrame, Wbbc97Draw, DrvScan, &DrvRecalc, 0x800,
	352, 240, 4, 3
};
