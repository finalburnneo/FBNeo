// FB Alpha Metro driver module
// Based on MAME driver by Luca Elia and David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "konamiic.h"
#include "burn_ym2610.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
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

static INT32 has_z80 = 1;
static INT32 m_sprite_xoffs_dx = 0;
static INT32 m_tilemap_scrolldx[3] = { 8, 8, 8 };
static UINT32 graphics_length;
static INT32 vblank_bit = 0;
static INT32 irq_line = 1;
static INT32 blitter_bit = 0;
static INT32 support_8bpp = 1;
static INT32 support_16x16 = 1;
static INT32 has_zoom = 0;

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
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

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

static struct BurnInputInfo PoittoInputList[] = {
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Poitto)

static struct BurnInputInfo LadykillInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy2 + 1,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"},

};

STDINPUTINFO(Ladykill)

static struct BurnInputInfo PururunInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Pururun)

static struct BurnInputInfo DharmaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 1,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Dharma)

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

	{0   , 0xfe, 0   ,    0, "Coin A"			},
	{0x29, 0x01, 0x07, 0x04, "4 Coins 1 Credits"		},
	{0x29, 0x01, 0x07, 0x05, "3 Coins 1 Credits"		},
	{0x29, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x29, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x29, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x29, 0x01, 0x07, 0x02, "1 Coin  3 Credits"		},
	{0x29, 0x01, 0x07, 0x01, "1 Coin  4 Credits"		},
	{0x29, 0x01, 0x07, 0x00, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    0, "Coin B"			},
	{0x29, 0x01, 0x38, 0x20, "4 Coins 1 Credits"		},
	{0x29, 0x01, 0x38, 0x28, "3 Coins 1 Credits"		},
	{0x29, 0x01, 0x38, 0x30, "2 Coins 1 Credits"		},
	{0x29, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x29, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x29, 0x01, 0x38, 0x10, "1 Coin  3 Credits"		},
	{0x29, 0x01, 0x38, 0x08, "1 Coin  4 Credits"		},
	{0x29, 0x01, 0x38, 0x00, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    0, "Free Play"			},
	{0x29, 0x01, 0x40, 0x40, "Off"				},
	{0x29, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    0, "Service Mode"			},
	{0x29, 0x01, 0x80, 0x80, "Off"				},
	{0x29, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    0, "CP Single"			},
	{0x2a, 0x01, 0x03, 0x03, "2:00"				},
	{0x2a, 0x01, 0x03, 0x02, "2:30"				},
	{0x2a, 0x01, 0x03, 0x01, "3:00"				},
	{0x2a, 0x01, 0x03, 0x00, "3:30"				},

	{0   , 0xfe, 0   ,    0, "CP Tag"			},
	{0x2a, 0x01, 0x0c, 0x0c, "2:00"				},
	{0x2a, 0x01, 0x0c, 0x08, "2:30"				},
	{0x2a, 0x01, 0x0c, 0x04, "3:00"				},
	{0x2a, 0x01, 0x0c, 0x00, "3:30"				},

	{0   , 0xfe, 0   ,    8, "Vs Single"			},
	{0x2a, 0x01, 0x30, 0x30, "2:30"				},
	{0x2a, 0x01, 0x30, 0x20, "3:00"				},
	{0x2a, 0x01, 0x30, 0x10, "4:00"				},
	{0x2a, 0x01, 0x30, 0x00, "5:00"				},

	{0   , 0xfe, 0   ,    8, "Vs Tag"			},
	{0x2a, 0x01, 0xc0, 0xc0, "2:30"				},
	{0x2a, 0x01, 0xc0, 0x80, "3:00"				},
	{0x2a, 0x01, 0xc0, 0x40, "4:00"				},
	{0x2a, 0x01, 0xc0, 0x00, "5:00"				},
};

STDDIPINFO(Blzntrnd)

static struct BurnDIPInfo Gstrik2DIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL				},
	{0x14, 0xff, 0xff, 0xfc, NULL				},
	{0x15, 0xff, 0xff, 0xff, NULL				},
	{0x16, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Player Vs Com"		},
	{0x13, 0x01, 0x03, 0x03, "1:00"				},
	{0x13, 0x01, 0x03, 0x02, "1:30"				},
	{0x13, 0x01, 0x03, 0x01, "2:00"				},
	{0x13, 0x01, 0x03, 0x00, "2:30"				},

	{0   , 0xfe, 0   ,    4, "1P Vs 2P"			},
	{0x13, 0x01, 0x0c, 0x0c, "0:45"				},
	{0x13, 0x01, 0x0c, 0x08, "1:00"				},
	{0x13, 0x01, 0x0c, 0x04, "1:30"				},
	{0x13, 0x01, 0x0c, 0x00, "2:00"				},

	{0   , 0xfe, 0   ,    4, "Extra Time"			},
	{0x13, 0x01, 0x30, 0x30, "0:30"				},
	{0x13, 0x01, 0x30, 0x20, "0:45"				},
	{0x13, 0x01, 0x30, 0x10, "1:00"				},
	{0x13, 0x01, 0x30, 0x00, "Off"				},

	{0   , 0xfe, 0   ,    2, "Time Period"			},
	{0x13, 0x01, 0x80, 0x80, "Sudden Death"			},
	{0x13, 0x01, 0x80, 0x00, "Full"				},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x14, 0x01, 0x07, 0x07, "Very Easy"			},
	{0x14, 0x01, 0x07, 0x06, "Easier"			},
	{0x14, 0x01, 0x07, 0x05, "Easy"				},
	{0x14, 0x01, 0x07, 0x04, "Normal"			},
	{0x14, 0x01, 0x07, 0x03, "Medium"			},
	{0x14, 0x01, 0x07, 0x02, "Hard"				},
	{0x14, 0x01, 0x07, 0x01, "Hardest"			},
	{0x14, 0x01, 0x07, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x08, 0x00, "Off"				},
	{0x14, 0x01, 0x08, 0x08, "On"				},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x14, 0x01, 0x10, 0x10, "Off"				},
	{0x14, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x14, 0x01, 0x20, 0x20, "Off"				},
	{0x14, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x40, 0x40, "Off"				},
	{0x14, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x80, 0x80, "Off"				},
	{0x14, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    32, "Coin A"			},
	{0x15, 0x01, 0x1f, 0x1c, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x1f, 0x1d, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x1f, 0x18, "4 Coins 2 Credits"		},
	{0x15, 0x01, 0x1f, 0x1e, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x1f, 0x19, "3 Coins 2 Credits"		},
	{0x15, 0x01, 0x1f, 0x14, "4 Coins 3 Credits"		},
	{0x15, 0x01, 0x1f, 0x10, "4 Coins 4 Credits"		},
	{0x15, 0x01, 0x1f, 0x15, "3 Coins 3 Credits"		},
	{0x15, 0x01, 0x1f, 0x1a, "2 Coins 2 Credits"		},
	{0x15, 0x01, 0x1f, 0x1f, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0x1f, 0x0c, "4 Coins 5 Credits"		},
	{0x15, 0x01, 0x1f, 0x11, "3 Coins 4 Credits"		},
	{0x15, 0x01, 0x1f, 0x08, "4 Coins/6 Credits"		},
	{0x15, 0x01, 0x1f, 0x16, "2 Coins 3 Credits"		},
	{0x15, 0x01, 0x1f, 0x0d, "3 Coins/5 Credits"		},
	{0x15, 0x01, 0x1f, 0x04, "4 Coins 7 Credits"		},
	{0x15, 0x01, 0x1f, 0x00, "4 Coins/8 Credits"		},
	{0x15, 0x01, 0x1f, 0x09, "3 Coins/6 Credits"		},
	{0x15, 0x01, 0x1f, 0x12, "2 Coins 4 Credits"		},
	{0x15, 0x01, 0x1f, 0x1b, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x1f, 0x05, "3 Coins/7 Credits"		},
	{0x15, 0x01, 0x1f, 0x0e, "2 Coins 5 Credits"		},
	{0x15, 0x01, 0x1f, 0x01, "3 Coins/8 Credits"		},
	{0x15, 0x01, 0x1f, 0x0a, "2 Coins 6 Credits"		},
	{0x15, 0x01, 0x1f, 0x17, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x1f, 0x06, "2 Coins 7 Credits"		},
	{0x15, 0x01, 0x1f, 0x02, "2 Coins 8 Credits"		},
	{0x15, 0x01, 0x1f, 0x13, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x1f, 0x0f, "1 Coin  5 Credits"		},
	{0x15, 0x01, 0x1f, 0x0b, "1 Coin  6 Credits"		},
	{0x15, 0x01, 0x1f, 0x07, "1 Coin  7 Credits"		},
	{0x15, 0x01, 0x1f, 0x03, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x15, 0x01, 0xe0, 0xc0, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0xe0, 0xa0, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0xe0, 0x80, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0xe0, 0x60, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0xe0, 0x40, "1 Coin  5 Credits"		},
	{0x15, 0x01, 0xe0, 0x20, "1 Coin  6 Credits"		},
	{0x15, 0x01, 0xe0, 0x00, "1 Coin  7 Credits"		},
	{0x15, 0x01, 0xe0, 0xe0, "Same as Coin A"		},

	{0   , 0xfe, 0   ,    4, "Credits to Start"		},
	{0x16, 0x01, 0x03, 0x03, "1"				},
	{0x16, 0x01, 0x03, 0x02, "2"				},
	{0x16, 0x01, 0x03, 0x01, "3"				},
	{0x16, 0x01, 0x03, 0x00, "4"				},

	{0   , 0xfe, 0   ,    4, "Credits to Continue"		},
	{0x16, 0x01, 0x0c, 0x0c, "1"				},
	{0x16, 0x01, 0x0c, 0x08, "2"				},
	{0x16, 0x01, 0x0c, 0x04, "3"				},
	{0x16, 0x01, 0x0c, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Continue"			},
	{0x16, 0x01, 0x10, 0x00, "Off"				},
	{0x16, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x16, 0x01, 0x20, 0x20, "Off"				},
	{0x16, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Playmode"			},
	{0x16, 0x01, 0x40, 0x40, "1 Credit for 1 Player"	},
	{0x16, 0x01, 0x40, 0x00, "1 Credit for 2 Players"	},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x16, 0x01, 0x80, 0x80, "Off"				},
	{0x16, 0x01, 0x80, 0x00, "On"				},
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
	{0x0b, 0xff, 0xff, 0xff, NULL				},
	{0x0c, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x0b, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x0b, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x0b, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x0b, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x0b, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x0b, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x0b, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x0b, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x0b, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x0b, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x0b, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x0b, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x0b, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x0b, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0b, 0x01, 0x40, 0x40, "Off"				},
	{0x0b, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0b, 0x01, 0x80, 0x80, "Off"				},
	{0x0b, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0c, 0x01, 0x03, 0x00, "Easy"				},
	{0x0c, 0x01, 0x03, 0x03, "Normal"			},
	{0x0c, 0x01, 0x03, 0x02, "Hard"				},
	{0x0c, 0x01, 0x03, 0x01, "Hardest"			},

	{0   , 0xfe, 0   ,    0, "Allow Continue"		},
	{0x0c, 0x01, 0x20, 0x00, "No"				},
	{0x0c, 0x01, 0x20, 0x20, "Yes"				},

	{0   , 0xfe, 0   ,    0, "Demo Sounds"			},
	{0x0c, 0x01, 0x40, 0x00, "Off"				},
	{0x0c, 0x01, 0x40, 0x40, "On"				},
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

static struct BurnDIPInfo LadykillDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x6f, NULL		},
	{0x0e, 0xff, 0xff, 0xff, NULL		},
	{0x0f, 0xff, 0xff, 0xff, NULL		},
	{0x10, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0d, 0x01, 0x03, 0x01, "1"		},
	{0x0d, 0x01, 0x03, 0x00, "2"		},
	{0x0d, 0x01, 0x03, 0x03, "3"		},
	{0x0d, 0x01, 0x03, 0x02, "4"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0d, 0x01, 0x0c, 0x08, "Easy"		},
	{0x0d, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x0d, 0x01, 0x0c, 0x04, "Hard"		},
	{0x0d, 0x01, 0x0c, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Nudity"		},
	{0x0d, 0x01, 0x10, 0x10, "Partial"		},
	{0x0d, 0x01, 0x10, 0x00, "Full"		},

	{0   , 0xfe, 0   ,    2, "Service Mode / Free Play"		},
	{0x0d, 0x01, 0x20, 0x20, "Off"		},
	{0x0d, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x0d, 0x01, 0x40, 0x00, "No"		},
	{0x0d, 0x01, 0x40, 0x40, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0d, 0x01, 0x80, 0x80, "Off"		},
	{0x0d, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x0e, 0x01, 0x07, 0x00, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0x07, 0x01, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x0e, 0x01, 0x07, 0x03, "1 Coin  5 Credits"		},
	{0x0e, 0x01, 0x07, 0x02, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x0e, 0x01, 0x38, 0x00, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0x38, 0x08, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x0e, 0x01, 0x38, 0x18, "1 Coin  5 Credits"		},
	{0x0e, 0x01, 0x38, 0x10, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0e, 0x01, 0x40, 0x40, "Off"		},
	{0x0e, 0x01, 0x40, 0x00, "On"		},
};

STDDIPINFO(Ladykill)

static struct BurnDIPInfo DharmaDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL		},
	{0x12, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x11, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x11, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x40, 0x40, "Off"		},
	{0x11, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x80, 0x80, "Off"		},
	{0x11, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x03, 0x02, "Easy"		},
	{0x12, 0x01, 0x03, 0x03, "Normal"		},
	{0x12, 0x01, 0x03, 0x01, "Hard"		},
	{0x12, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Timer"		},
	{0x12, 0x01, 0x0c, 0x08, "Slow"		},
	{0x12, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x12, 0x01, 0x0c, 0x04, "Fast"		},
	{0x12, 0x01, 0x0c, 0x00, "Fastest"		},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x12, 0x01, 0x10, 0x10, "2 Credits"		},
	{0x12, 0x01, 0x10, 0x00, "1 Credit"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x12, 0x01, 0x20, 0x00, "No"		},
	{0x12, 0x01, 0x20, 0x20, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x40, 0x00, "Off"		},
	{0x12, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Freeze (Cheat)"		},
	{0x12, 0x01, 0x80, 0x80, "Off"		},
	{0x12, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Dharma)

static struct BurnDIPInfo PururunDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL		},
	{0x13, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x40, 0x40, "Off"		},
	{0x12, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x80, 0x80, "Off"		},
	{0x12, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x03, 0x02, "Easiest"		},
	{0x13, 0x01, 0x03, 0x01, "Easy"		},
	{0x13, 0x01, 0x03, 0x03, "Normal"		},
	{0x13, 0x01, 0x03, 0x00, "Hard"		},

	{0   , 0xfe, 0   ,    2, "Join In"		},
	{0x13, 0x01, 0x04, 0x00, "No"		},
	{0x13, 0x01, 0x04, 0x04, "Yes"		},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x13, 0x01, 0x08, 0x00, "1 Credit"		},
	{0x13, 0x01, 0x08, 0x08, "2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Bombs"		},
	{0x13, 0x01, 0x10, 0x10, "1"		},
	{0x13, 0x01, 0x10, 0x00, "2"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x13, 0x01, 0x20, 0x00, "No"		},
	{0x13, 0x01, 0x20, 0x20, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x40, 0x00, "Off"		},
	{0x13, 0x01, 0x40, 0x40, "On"		},
};

STDDIPINFO(Pururun)

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
		UINT8 irq_level[8] = { 0 };

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

	//	int shift   = (dst_offs & 0x80) ? 0 : 8;
	//	UINT16 mask = (dst_offs & 0x80) ? 0x00ff : 0xff00;
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
					dst[dst_offs*2+offs2] = b2; //blt_write(tmap, dst_offs, b2);
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
					dst[dst_offs*2+offs2] = b2; //blt_write(tmap, dst_offs, b2);
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
					dst[dst_offs*2+offs2] = b2; //blt_write(tmap, dst_offs, b2);
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
// sky alert	0x800000
// poitto	0xc00000
// pururun	0xc00000
// pangpoms	0x400000


static UINT8 __fastcall metro_common_main_read_byte(UINT32 address)
{
	address &= 0x7ffff;

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
		case 0x078880:
		case 0x078890:
		return; // nop

		case 0x0788a2:
			metro_irq_cause_w(data);
		return;

		case 0x0788a4:
			irq_enable = data;
		return;

		case 0x0788a8:
			soundlatch = data;
		return;

		case 0x0788aa:
			gfxrom_bank = (data & 0x1ff) * 0x10000;
			if (gfxrom_bank >= graphics_length) gfxrom_bank = graphics_length-0x10000;
		return;

		case 0x0788ac:
			screen_control = data;
		return;
	}
}

static void __fastcall metro_common_write_byte(UINT32 address, UINT8 data)
{
	address &= 0x7ffff;

	bprintf (0, _T("Common WB %5.5x, %2.2x\n"), address, data);
}

static void metro_common_map_ram(UINT32 chip_address, INT32 main_ram_address)
{
	SekMapMemory(DrvVidRAM0,	chip_address + 0x000000, chip_address + 0x01ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,	chip_address + 0x020000, chip_address + 0x03ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM2,	chip_address + 0x040000, chip_address + 0x05ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,	chip_address + 0x070000, chip_address + 0x071fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		chip_address + 0x072000, chip_address + 0x073fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		chip_address + 0x074000, chip_address + 0x074fff, MAP_RAM);
	SekMapMemory(DrvTileRAM,	chip_address + 0x078000, chip_address + 0x0787ff, MAP_RAM);

	if (main_ram_address == -1) return;

	for (INT32 i = 0; i < 0x10; i++) {
		SekMapMemory(Drv68KRAM1,	main_ram_address + (i * 0x10000), main_ram_address + 0xffff + (i * 0x10000), MAP_RAM);
	}
}

//------------------------------------------------------------------------------------------------------------------------------------------











static void __fastcall blzntrnd_main_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x278860 && address <= 0x27886b) {
		*((UINT16*)(DrvWindow + (address & 0xe))) = data;
		return;
	}

	if (address >= 0x278870 && address <= 0x27887b) {
		*((UINT16*)(DrvScroll + (address & 0xe))) = data;
		return;
	}

	if (address >= 0x279700 && address <= 0x279713) {
		*((UINT16*)(DrvVidRegs + (address & 0x1e))) = data;
		return;
	}

	switch (address)
	{
		case 0x278840:
		case 0x278852:
		case 0x278880:
		case 0x278890:
		case 0x2788a0:
		return; // nop

		case 0x2788a2:
			metro_irq_cause_w(data);
		return;

		case 0x2788a4:
			irq_enable = data;
		return;

		case 0x2788aa:
			gfxrom_bank = (data & 0x1ff) * 0x10000;
			if (gfxrom_bank >= 0x1800000) gfxrom_bank = 0x1800000;
		return;

		case 0x2788ac:
			screen_control = data;
		return;

		case 0xe00002:
			soundlatch = data >> 8;
			ZetNmi();
		return;
	}
}

static void __fastcall blzntrnd_main_write_byte(UINT32 address, UINT8 data)
{
	if (address != 0xe00001)
		bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall blzntrnd_main_read_word(UINT32 address)
{
	if ((address & 0xfff0000) == 0x260000) {
		INT32 offset = gfxrom_bank + (address & 0xfffe);
		return DrvGfxROM[offset + 0] * 256 + DrvGfxROM[offset + 1];
	}

	switch (address)
	{
		case 0x2788a2:
			return metro_irq_cause_r();

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
			return DrvDips[(address & 3) ^ 1]; // dip0

		case 0xe00004:
		case 0xe00005:
		case 0xe00006:
		case 0xe00007:
		case 0xe00008:
		case 0xe00009:
			return DrvInputs[(address - 0xe00004)/2] >> ((~address & 1) * 8);
	}

	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}





static void __fastcall skyalert_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff80000) == 0x800000) {
		metro_common_write_word(address, data);
		return;
	}

	switch (address)
	{
		case 0x400000:
			// soundstatus
		return;

		case 0x400002:
			// coin lockout
		return;
	}
}

static void __fastcall skyalert_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xff80000) == 0x800000) {
		metro_common_write_byte(address, data);
	}

	if (address != 0x400001)
		bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall skyalert_main_read_word(UINT32 address)
{
	if ((address & 0xff80000) == 0x800000) {
		return metro_common_read_word(address);
	}

	switch (address)
	{
		case 0x400000:
			return rand(); // sound status

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
	if ((address & 0xff80000) == 0x800000) {
		return metro_common_main_read_byte(address);
	}

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

	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}

static void __fastcall pangpoms_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff80000) == 0x400000) {
		metro_common_write_word(address, data);
		return;
	}

	switch (address)
	{
		case 0x800000:
			// soundstatus
		return;

		case 0x400002:
			// coin lockout
		return;
	}
}

static void __fastcall pangpoms_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xff80000) == 0x400000) {
		metro_common_write_byte(address, data);
	}

	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall pangpoms_main_read_word(UINT32 address)
{
	if ((address & 0xff80000) == 0x400000) {
		return metro_common_read_word(address);
	}

	switch (address)
	{
		case 0x800000:
			return rand(); // sound status

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
	if ((address & 0xff80000) == 0x400000) {
		return metro_common_main_read_byte(address);
	}

	switch (address)
	{
		case 0x80000a:
		case 0x80000b:
		case 0x80000c:
		case 0x80000d:
			return DrvDips[(address - 0x40000a) ^ 1]; // dip0

		case 0x800004:
		case 0x800005:
		case 0x800006:
		case 0x800007:
		case 0x800008:
		case 0x800009:
			return DrvInputs[(address - 0x400004)/2] >> ((~address & 1) * 8);

		case 0x80000e:
		case 0x80000f:
			return DrvInputs[3] >> ((~address & 1) * 8);
	}

	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}


static void __fastcall poitto_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff80000) == 0xc00000) {
		metro_common_write_word(address, data);
		return;
	}

	switch (address)
	{
		case 0x800000:
			// soundstatus
		return;

		case 0x800002:
		case 0x800004:
		case 0x800006:
		case 0x800008:
			// coin lockout
		return;
	}
}

static void __fastcall poitto_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xff80000) == 0xc00000) {
		metro_common_write_byte(address, data);
	}

	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall poitto_main_read_word(UINT32 address)
{
	if ((address & 0xff80000) == 0xc00000) {
		return metro_common_read_word(address);
	}

	switch (address)
	{
		case 0x800000:
			return (DrvInputs[0] & 0xff7f) | (rand() & 0x80);

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
	if ((address & 0xff80000) == 0xc00000) {
		return metro_common_main_read_byte(address);
	}

	switch (address)
	{
		case 0x800000:
			return DrvInputs[0] >> 8;

		case 0x800001:
			return (DrvInputs[0] & 0x7f) | (rand() & 0x80); // sound status

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

	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}

static void __fastcall lastforg_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff00000) == 0x800000) {
		metro_common_write_word(address, data);
		return;
	}

	switch (address)
	{
		case 0x400000:
			// soundstatus
		return;

		case 0x400002:
			// coin lockout
		return;
	}
}

static void __fastcall lastforg_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xff80000) == 0x880000) {
		metro_common_write_byte(address, data);
	}

	if (address != 0x400001)
		bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall lastforg_main_read_word(UINT32 address)
{
	if ((address & 0xff80000) == 0x880000) {
		return metro_common_read_word(address);
	}

	switch (address)
	{
		case 0x400000:
			return rand(); // sound status

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

static UINT8 __fastcall lastforg_main_read_byte(UINT32 address)
{
	if ((address & 0xff80000) == 0x880000) {
		return metro_common_main_read_byte(address);
	}

	switch (address)
	{
		case 0x400006:
		case 0x400007:
			return DrvDips[(address - 0x400006) ^ 1]; // dip0

		case 0x40000a:
		case 0x40000b:
			return DrvDips[(address - 0x40000a) ^ 1]; // dip0

		case 0x400000:
		case 0x400001:
		case 0x400002:
		case 0x400003:
			return DrvInputs[(address - 0x400000)/2] >> ((~address & 1) * 8);

		case 0x40000c:
		case 0x40000d:
			return DrvInputs[2] >> ((~address & 1) * 8);
	}

	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}


static void __fastcall lastfort_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff80000) == 0x800000) {
		metro_common_write_word(address, data);
		return;
	}

	switch (address)
	{
		case 0xc00000:
			// soundstatus
		return;

		case 0xc00002:
			// coin lockout
		return;
	}
}

static void __fastcall lastfort_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xff80000) == 0x800000) {
		metro_common_write_byte(address, data);
	}

	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall lastfort_main_read_word(UINT32 address)
{
	if ((address & 0xff80000) == 0x800000) {
		return metro_common_read_word(address);
	}

	switch (address)
	{
		case 0xc00000:
			return rand(); // sound status

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
	if ((address & 0xff80000) == 0x800000) {
		return metro_common_main_read_byte(address);
	}

	switch (address)
	{
		case 0xc0000a:
		case 0xc0000b:
		case 0xc0000c:
		case 0xc0000d:
			return DrvDips[(address - 0x40000a) ^ 1]; // dip0

		case 0xc00004:
		case 0xc00005:
		case 0xc00006:
		case 0xc00007:
		case 0xc00008:
		case 0xc00009:
			return DrvInputs[(address - 0x400004)/2] >> ((~address & 1) * 8);

		case 0xc0000e:
		case 0xc0000f:
			return DrvInputs[3] >> ((~address & 1) * 8);
	}

	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}


static void __fastcall dharma_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff80000) == 0x800000) {
		metro_common_write_word(address, data);
		return;
	}

	switch (address)
	{
		case 0xc00000:
			// soundstatus
		return;

		case 0xc00002:
			// coin lockout
		return;
	}
}

static void __fastcall dharma_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xff80000) == 0x800000) {
		metro_common_write_byte(address, data);
	}

	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall dharma_main_read_word(UINT32 address)
{
	if ((address & 0xff80000) == 0x800000) {
		return metro_common_read_word(address);
	}

	switch (address)
	{
		case 0xc00000:
			return (DrvInputs[0] & 0xff7f) | (rand() & 0x80);

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
	if ((address & 0xff80000) == 0x800000) {
		return metro_common_main_read_byte(address);
	}

	switch (address)
	{
		case 0xc00000:
			return DrvInputs[0] >> 8;

		case 0xc00001:
			return (DrvInputs[0] & 0x7f) | (rand() & 0x80);

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

	bprintf (0, _T("RB %5.5x\n"), address);

	return 0;
}

static void __fastcall pururun_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff80000) == 0xc00000) {
		metro_common_write_word(address, data);
		return;
	}

	switch (address)
	{
		case 0x400000:
			// soundstatus
		return;

		case 0x400002:
			// coin lockout
		return;
	}
}

static void __fastcall pururun_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xff80000) == 0xc00000) {
		metro_common_write_byte(address, data);
	}

	bprintf (0, _T("WB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall pururun_main_read_word(UINT32 address)
{
	if ((address & 0xff80000) == 0xc00000) {
		return metro_common_read_word(address);
	}

	switch (address)
	{
		case 0x400000:
			return (DrvInputs[0] & 0xff7f) | (rand() & 0x80);

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
	if ((address & 0xff80000) == 0xc00000) {
		return metro_common_main_read_byte(address);
	}

	switch (address)
	{
		case 0x400000:
			return DrvInputs[0] >> 8;

		case 0x400001:
			return (DrvInputs[0] & 0x7f) | (rand() & 0x80);

		case 0x400002:
		case 0x400003:
			return DrvInputs[1] >> ((~address & 1) * 8);

		case 0x400004:
		case 0x400005:
			return DrvDips[(address - 0xc00004) ^ 1]; // dip0

		case 0x400006:
		case 0x400007:
			return DrvInputs[2] >> ((~address & 1) * 8);
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
	*sy  = (offset >> 9) & 0x3f;
	*sy += (offset & 1) << 6;
	*sy += (offset & 0x100) >> 1;
	*sy *= 16;

	*sx  = (offset >> 1) & 0x7f;
	*sx *= 16;

	*code = (ram[offset] & 0x7ffc) >> 2;

	*color = 0xe00;
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

	if (has_z80) {
		ZetOpen(0);
		ZetReset();
		BurnYM2610Reset();
		ZetClose();
	}

	if (has_zoom) K053936Reset();

	memset (requested_int, 0, 8);

	gfxrom_bank=0;
	soundlatch=0;
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
	DrvZ80ROM		= Next; Next += 0x020000;

	DrvGfxROM		= Next; Next += graphics_length;
	DrvGfxROM0		= Next; Next += graphics_length*2;

	DrvRozROM		= Next; Next += 0x200000;

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

	DrvZ80RAM		= Next; Next += 0x002000;

	DrvWindow		= Next; Next += 0x000010;
	DrvScroll		= Next; Next += 0x000010;
	DrvVidRegs		= Next; Next += 0x000020;
	DrvBlitter		= Next; Next += 0x00000f;

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

static INT32 DrvInit()
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
	SekMapMemory(DrvVidRAM0,	0x200000, 0x21ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,	0x220000, 0x23ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM2,	0x240000, 0x25ffff, MAP_RAM);
	SekMapMemory(DrvUnkRAM,		0x260000, 0x26ffff, MAP_WRITE);
	SekMapMemory(Drv68KRAM0,	0x270000, 0x271fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x272000, 0x273fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x274000, 0x274fff, MAP_RAM);
	SekMapMemory(DrvTileRAM,	0x278000, 0x2787ff, MAP_RAM);
	SekMapMemory(DrvK053936RAM,	0x400000, 0x43ffff, MAP_RAM);
	SekMapMemory(DrvK053936LRAM,	0x500000, 0x500fff, MAP_RAM);
	SekMapMemory(DrvK053936CRAM,	0x600000, 0x6003ff, MAP_RAM);
	for (INT32 i = 0; i < 0x10; i++) {
		SekMapMemory(Drv68KRAM1,	0xf00000 + (i * 0x10000), 0xf0ffff + (i * 0x10000), MAP_RAM);
	}
	SekSetWriteWordHandler(0,	blzntrnd_main_write_word);
	SekSetWriteByteHandler(0,	blzntrnd_main_write_byte);
	SekSetReadWordHandler(0,	blzntrnd_main_read_word);
	SekSetReadByteHandler(0,	blzntrnd_main_read_byte);
	SekClose();

	has_z80 = 1;
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
	SekMapMemory(DrvVidRAM0,	0x200000, 0x21ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,	0x220000, 0x23ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM2,	0x240000, 0x25ffff, MAP_RAM);
	SekMapMemory(DrvUnkRAM,		0x260000, 0x26ffff, MAP_WRITE);
	SekMapMemory(Drv68KRAM0,	0x270000, 0x271fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x272000, 0x273fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x274000, 0x274fff, MAP_RAM);
	SekMapMemory(DrvTileRAM,	0x278000, 0x2787ff, MAP_RAM);
	SekMapMemory(DrvK053936RAM,	0x400000, 0x43ffff, MAP_RAM);
	SekMapMemory(DrvK053936LRAM,	0x500000, 0x500fff, MAP_RAM);
	SekMapMemory(DrvK053936CRAM,	0x600000, 0x6003ff, MAP_RAM);
	for (INT32 i = 0; i < 0x10; i++) {
		SekMapMemory(Drv68KRAM1,	0xf00000 + (i * 0x10000), 0xf0ffff + (i * 0x10000), MAP_RAM);
	}
	SekSetWriteWordHandler(0,	blzntrnd_main_write_word);
	SekSetWriteByteHandler(0,	blzntrnd_main_write_byte);
	SekSetReadWordHandler(0,	blzntrnd_main_read_word);
	SekSetReadByteHandler(0,	blzntrnd_main_read_byte);
	SekClose();

	has_z80 = 1;
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

static INT32 skyalertInit()
{
	graphics_length = 0x200000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

	// 	upd rom

		if (BurnLoadRom(DrvGfxROM + 0x0000000,  3, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000001,  4, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000002,  5, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000003,  6, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000004,  7, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000005,  8, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000006,  9, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000007, 10, 8)) return 1;

		expand_4bpp(graphics_length);

		if (BurnLoadRom(DrvYMROMA + 0x0000000, 11, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	metro_common_map_ram(0x800000, 0xc00000);
	SekSetWriteWordHandler(0,	skyalert_main_write_word);
	SekSetWriteByteHandler(0,	skyalert_main_write_byte);
	SekSetReadWordHandler(0,	skyalert_main_read_word);
	SekSetReadByteHandler(0,	skyalert_main_read_byte);
	SekClose();

	has_z80 = 0;

	// sound hardware

	GenericTilesInit();
	KonamiAllocateBitmaps();

	m_sprite_xoffs_dx = 0;
	m_tilemap_scrolldx[0] = m_tilemap_scrolldx[1] = m_tilemap_scrolldx[2] = 0;

	vblank_bit = 0;
	irq_line = 2;
	blitter_bit = 2;

	support_8bpp = 0;
	support_16x16 = 0;
	has_zoom = 0;

	DrvDoReset();

	return 0;
}

static INT32 pangpomsInit()
{
	graphics_length = 0x100000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

	// 	upd rom

		if (BurnLoadRom(DrvGfxROM + 0x0000000,  3, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000001,  4, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000002,  5, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000003,  6, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000004,  7, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000005,  8, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000006,  9, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000007, 10, 8)) return 1;

		expand_4bpp(graphics_length);

		if (BurnLoadRom(DrvYMROMA + 0x0000000, 11, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	metro_common_map_ram(0x400000, 0xc00000);
	SekSetWriteWordHandler(0,	pangpoms_main_write_word);
	SekSetWriteByteHandler(0,	pangpoms_main_write_byte);
	SekSetReadWordHandler(0,	pangpoms_main_read_word);
	SekSetReadByteHandler(0,	pangpoms_main_read_byte);
	SekClose();

	has_z80 = 0;

	// sound hardware

	GenericTilesInit();
	KonamiAllocateBitmaps();

	m_sprite_xoffs_dx = 0;
	m_tilemap_scrolldx[0] = m_tilemap_scrolldx[1] = m_tilemap_scrolldx[2] = 0;
	vblank_bit = 0;
	irq_line = 2;
	blitter_bit = 2;
	support_8bpp = 0;
	support_16x16 = 0;
	has_zoom = 0;

	DrvDoReset();

	return 0;
}

static INT32 poittoInit()
{
	graphics_length = 0x200000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

	// 	upd rom

		if (BurnLoadRomExt(DrvGfxROM + 0x0000000,  3, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000002,  4, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000004,  5, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000006,  6, 8, LD_GROUP(2))) return 1;

		expand_4bpp(graphics_length);

		if (BurnLoadRom(DrvYMROMA + 0x0000000,  7, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	metro_common_map_ram(0xc00000, 0x400000);
	SekSetWriteWordHandler(0,	poitto_main_write_word);
	SekSetWriteByteHandler(0,	poitto_main_write_byte);
	SekSetReadWordHandler(0,	poitto_main_read_word);
	SekSetReadByteHandler(0,	poitto_main_read_byte);
	SekClose();

	has_z80 = 0;

	// sound hardware

	GenericTilesInit();
	KonamiAllocateBitmaps();

	m_sprite_xoffs_dx = 0;
	m_tilemap_scrolldx[0] = m_tilemap_scrolldx[1] = m_tilemap_scrolldx[2] = 0;

	vblank_bit = 0;
	irq_line = 2;
	blitter_bit = 2;

	support_8bpp = 0;
	support_16x16 = 0;
	has_zoom = 0;

	DrvDoReset();

	return 0;
}

static INT32 lastfortInit()
{
	graphics_length = 0x200000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

	// 	upd rom

		if (BurnLoadRom(DrvGfxROM + 0x0000000,  3, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000001,  4, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000002,  5, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000003,  6, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000004,  7, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000005,  8, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000006,  9, 8)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000007, 10, 8)) return 1;

		expand_4bpp(graphics_length);

		if (BurnLoadRom(DrvYMROMA + 0x0000000, 11, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	metro_common_map_ram(0x800000, 0x400000);
	SekSetWriteWordHandler(0,	lastfort_main_write_word);
	SekSetWriteByteHandler(0,	lastfort_main_write_byte);
	SekSetReadWordHandler(0,	lastfort_main_read_word);
	SekSetReadByteHandler(0,	lastfort_main_read_byte);
	SekClose();

	has_z80 = 0;

	// sound hardware

	GenericTilesInit();
	KonamiAllocateBitmaps();

	m_sprite_xoffs_dx = 0;
	m_tilemap_scrolldx[0] = m_tilemap_scrolldx[1] = m_tilemap_scrolldx[2] = 0;

	vblank_bit = 0;
	irq_line = 2;
	blitter_bit = 2;

	support_8bpp = 0;
	support_16x16 = 0;
	has_zoom = 0;

	DrvDoReset();

	return 0;
}

static INT32 lastforgInit()
{
	graphics_length = 0x200000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

	// 	upd rom

		if (BurnLoadRomExt(DrvGfxROM + 0x0000000,  3, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000002,  4, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000004,  5, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000006,  6, 8, LD_GROUP(2))) return 1;

		expand_4bpp(graphics_length);

		if (BurnLoadRom(DrvYMROMA + 0x0000000,  7, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	metro_common_map_ram(0x880000, 0xc00000);
	SekSetWriteWordHandler(0,	lastforg_main_write_word);
	SekSetWriteByteHandler(0,	lastforg_main_write_byte);
	SekSetReadWordHandler(0,	lastforg_main_read_word);
	SekSetReadByteHandler(0,	lastforg_main_read_byte);
	SekClose();

	has_z80 = 0;

	// sound hardware

	GenericTilesInit();
	KonamiAllocateBitmaps();

	m_sprite_xoffs_dx = 0;
	m_tilemap_scrolldx[0] = m_tilemap_scrolldx[1] = m_tilemap_scrolldx[2] = 0;

	vblank_bit = 0;
	irq_line = 2;
	blitter_bit = 2;

	support_8bpp = 0;
	support_16x16 = 0;
	has_zoom = 0;

	DrvDoReset();

	return 0;
}

static INT32 dharmaInit()
{
	graphics_length = 0x200000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

	// 	upd rom

		if (BurnLoadRomExt(DrvGfxROM + 0x0000000,  3, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000002,  4, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000004,  5, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000006,  6, 8, LD_GROUP(2))) return 1;

		for (INT32 i = 0; i < 0x200000; i += 4)
		{
			DrvGfxROM[i + 1] = BITSWAP08(DrvGfxROM[i + 1], 7,3,2,4, 5,6,1,0);
			DrvGfxROM[i + 3] = BITSWAP08(DrvGfxROM[i + 3], 7,2,5,4, 3,6,1,0);
		}

		expand_4bpp(graphics_length);

		if (BurnLoadRom(DrvYMROMA + 0x0000000,  7, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	metro_common_map_ram(0x800000, 0x400000);
	SekSetWriteWordHandler(0,	dharma_main_write_word);
	SekSetWriteByteHandler(0,	dharma_main_write_byte);
	SekSetReadWordHandler(0,	dharma_main_read_word);
	SekSetReadByteHandler(0,	dharma_main_read_byte);
	SekClose();

	has_z80 = 0;

	// sound hardware

	GenericTilesInit();
	KonamiAllocateBitmaps();

	m_sprite_xoffs_dx = 0;
	m_tilemap_scrolldx[0] = m_tilemap_scrolldx[1] = m_tilemap_scrolldx[2] = 0;

	vblank_bit = 0;
	irq_line = 2;
	blitter_bit = 2;

	support_8bpp = 1;
	support_16x16 = 1;
	has_zoom = 0;

	DrvDoReset();

	return 0;
}

static INT32 pururunInit()
{
	graphics_length = 0x200000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

	// 	upd rom

		if (BurnLoadRomExt(DrvGfxROM + 0x0000000,  3, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000002,  4, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000004,  5, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0000006,  6, 8, LD_GROUP(2))) return 1;

		expand_4bpp(graphics_length);

		if (BurnLoadRom(DrvYMROMA + 0x0000000,  7, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	metro_common_map_ram(0xc00000, 0x800000);
	SekSetWriteWordHandler(0,	pururun_main_write_word);
	SekSetWriteByteHandler(0,	pururun_main_write_byte);
	SekSetReadWordHandler(0,	pururun_main_read_word);
	SekSetReadByteHandler(0,	pururun_main_read_byte);
	SekClose();

	has_z80 = 0;

	// sound hardware

	GenericTilesInit();
	KonamiAllocateBitmaps();

	m_sprite_xoffs_dx = 0;
	m_tilemap_scrolldx[0] = m_tilemap_scrolldx[1] = m_tilemap_scrolldx[2] = 0;

	vblank_bit = 0;
	irq_line = 2;
	blitter_bit = 2;

	support_8bpp = 1;
	support_16x16 = 1;
	has_zoom = 0;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	if (has_z80) BurnYM2610Exit();

	KonamiICExit();
	GenericTilesExit();

	if (has_z80) ZetExit();
	SekExit();
	
	BurnFree(AllMem);

	has_z80 = 0;
	has_zoom = 0;

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
	INT32 m_sprite_yoffs = m_videoregs[0x04 / 2] - nScreenHeight / 2;

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

extern void K053936PredrawTiles3(INT32 chip, UINT8 *gfx, INT32 tile_size_x, INT32 tile_size_y, INT32 transparent);

static INT32 DrvDraw()
{
//	if (DrvRecalc)
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

static INT32 DrvFrame()
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

	INT32 nInterleave = 224;
	INT32 nCyclesTotal[2] = { 16000000 / 58, 8000000  / 58 };
	
	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekRun(nCyclesTotal[0] / nInterleave);

		if ((i % 28) == 26) {
			requested_int[4] = 1;
			update_irq_state();
		}

		if (i == 223)
		{
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

	INT32 nInterleave = 224;
	INT32 nCyclesTotal[2] = { 16000000 / 58, 8000000  / 58 };
	
	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekRun(nCyclesTotal[0] / nInterleave);

		if ((i % 28) == 26) {
			requested_int[4] = 1;
			update_irq_state();
		}

		if (i == 223)
		{
			requested_int[vblank_bit] = 1;
			requested_int[5] = 1;
			update_irq_state();
			SekRun(500);
			requested_int[5] = 0;
		}

		if (blit_timer >= 0) {
			if (blit_timer == 0) {
				update_irq_state();
			}
			blit_timer--;
		}
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

	if (nAction & ACB_MEMORY_ROM) {	
		ba.Data		= Drv68KROM;
		ba.nLen		= 0x0200000;
		ba.nAddress	= 0;
		ba.szName	= "68K ROM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_MEMORY_RAM) {	
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

		ba.Data		= Drv68KRAM1;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0xf00000;
		ba.szName	= "68K RAM 1";
		BurnAcb(&ba);
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

	{ "rom142.bin",			0x200000, 0xa7200598, 3 | BRF_GRA },           //  5 Sprites
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
	"Blazing Tornado\0", "Save states unsupported", "Human Amusement", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, blzntrndRomInfo, blzntrndRomName, NULL, NULL, BlzntrndInputInfo, BlzntrndDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	304, 224, 4, 3
};


// Grand Striker 2 (Europe and Oceania)

static struct BurnRomInfo gstrik2RomDesc[] = {
	{ "hum_003_g2f.rom1.u107",	0x080000, 0x2712d9ca, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "hum_003_g2f.rom2.u108",	0x080000, 0x86785c64, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.109",			0x080000, 0xead86919, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.110",			0x080000, 0xe0b026e3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sprg.30",			0x020000, 0xaeef6045, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "chr0.80",			0x200000, 0xf63a52a9, 3 | BRF_GRA },           //  5 Sprites
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
	"Grand Striker 2 (Europe and Oceania)\0", NULL, "Human Amusement", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, gstrik2RomInfo, gstrik2RomName, NULL, NULL, Gstrik2InputInfo, Gstrik2DIPInfo,
	gstrik2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	304, 224, 4, 3
};


// Sky Alert

static struct BurnRomInfo skyalertRomDesc[] = {
	{ "sa_c_09.bin",		0x20000, 0x6f14d9ae, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "sa_c_10.bin",		0x20000, 0xf10bb216, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sa_b_12.bin",		0x20000, 0xf358175d, 2 | BRF_PRG | BRF_ESS }, //  2 UPD7810

	{ "sa_a_02.bin",		0x40000, 0xf4f81d41, 3 | BRF_GRA },           //  3 Sprites
	{ "sa_a_04.bin",		0x40000, 0x7d071e7e, 3 | BRF_GRA },           //  4
	{ "sa_a_06.bin",		0x40000, 0x77e4d5e1, 3 | BRF_GRA },           //  5
	{ "sa_a_08.bin",		0x40000, 0xf2a5a093, 3 | BRF_GRA },           //  6
	{ "sa_a_01.bin",		0x40000, 0x41ec6491, 3 | BRF_GRA },           //  7
	{ "sa_a_03.bin",		0x40000, 0xe0dff10d, 3 | BRF_GRA },           //  8
	{ "sa_a_05.bin",		0x40000, 0x62169d31, 3 | BRF_GRA },           //  9
	{ "sa_a_07.bin",		0x40000, 0xa6f5966f, 3 | BRF_GRA },           // 10

	{ "sa_a_11.bin",		0x20000, 0x04842a60, 4 | BRF_SND },           // 11 MSM6295 Samples
};

STD_ROM_PICK(skyalert)
STD_ROM_FN(skyalert)

struct BurnDriver BurnDrvSkyalert = {
	"skyalert", NULL, NULL, NULL, "1992",
	"Sky Alert\0", "No sound or save states", "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, skyalertRomInfo, skyalertRomName, NULL, NULL, SkyalertInputInfo, SkyalertDIPInfo,
	skyalertInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 360, 3, 4
};

// Pang Pom's

static struct BurnRomInfo pangpomsRomDesc[] = {
	{ "ppoms09.bin",	0x20000, 0x0c292dbc, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ppoms10.bin",	0x20000, 0x0bc18853, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ppoms12.bin",	0x20000, 0xa749357b, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "ppoms02.bin",	0x20000, 0x88f902f7, 3 | BRF_GRA },           //  3 gfx1
	{ "ppoms04.bin",	0x20000, 0x9190c2a0, 3 | BRF_GRA },           //  4
	{ "ppoms06.bin",	0x20000, 0xed15c93d, 3 | BRF_GRA },           //  5
	{ "ppoms08.bin",	0x20000, 0x9a3408b9, 3 | BRF_GRA },           //  6
	{ "ppoms01.bin",	0x20000, 0x11ac3810, 3 | BRF_GRA },           //  7
	{ "ppoms03.bin",	0x20000, 0xe595529e, 3 | BRF_GRA },           //  8
	{ "ppoms05.bin",	0x20000, 0x02226214, 3 | BRF_GRA },           //  9
	{ "ppoms07.bin",	0x20000, 0x48471c87, 3 | BRF_GRA },           // 10

	{ "ppoms11.bin",	0x20000, 0xe89bd565, 4 | BRF_SND },           // 11 oki
};

STD_ROM_PICK(pangpoms)
STD_ROM_FN(pangpoms)

struct BurnDriver BurnDrvPangpoms = {
	"pangpoms", NULL, NULL, NULL, "1992",
	"Pang Pom's\0", "No sound or save states", "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, pangpomsRomInfo, pangpomsRomName, NULL, NULL, SkyalertInputInfo, PangpomsDIPInfo,
	pangpomsInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	360, 224, 4, 3
};


// Poitto!

static struct BurnRomInfo poittoRomDesc[] = {
	{ "pt-jd05.20e",	0x20000, 0x6b1be034, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "pt-jd06.20c",	0x20000, 0x3092d9d4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pt-jc08.3i",		0x20000, 0xf32d386a, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "pt-2.15i",		0x80000, 0x05d15d01, 3 | BRF_GRA },           //  3 gfx1
	{ "pt-4.19i",		0x80000, 0x8a39edb5, 3 | BRF_GRA },           //  4
	{ "pt-1.13i",		0x80000, 0xea6e2289, 3 | BRF_GRA },           //  5
	{ "pt-3.17i",		0x80000, 0x522917c1, 3 | BRF_GRA },           //  6

	{ "pt-jc07.3g",		0x40000, 0x5ae28b8d, 4 | BRF_SND },           //  7 oki
};

STD_ROM_PICK(poitto)
STD_ROM_FN(poitto)

struct BurnDriver BurnDrvPoitto = {
	"poitto", NULL, NULL, NULL, "1993",
	"Poitto!\0", NULL, "Metro / Able Corp.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, poittoRomInfo, poittoRomName, NULL, NULL, PoittoInputInfo, PoittoDIPInfo,
	poittoInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	360, 224, 4, 3
};



// Last Fortress - Toride

static struct BurnRomInfo lastfortRomDesc[] = {
	{ "tr_jc09",	0x20000, 0x8b98a49a, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tr_jc10",	0x20000, 0x8d04da04, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tr_jb12",	0x20000, 0x8a8f5fef, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "tr_jc02",	0x20000, 0xdb3c5b79, 3 | BRF_GRA },           //  3 gfx1
	{ "tr_jc04",	0x20000, 0xf8ab2f9b, 3 | BRF_GRA },           //  4
	{ "tr_jc06",	0x20000, 0x47a7f397, 3 | BRF_GRA },           //  5
	{ "tr_jc08",	0x20000, 0xd7ba5e26, 3 | BRF_GRA },           //  6
	{ "tr_jc01",	0x20000, 0x3e3dab03, 3 | BRF_GRA },           //  7
	{ "tr_jc03",	0x20000, 0x87ac046f, 3 | BRF_GRA },           //  8
	{ "tr_jc05",	0x20000, 0x3fbbe49c, 3 | BRF_GRA },           //  9
	{ "tr_jc07",	0x20000, 0x05e1456b, 3 | BRF_GRA },           // 10

	{ "tr_jb11",	0x20000, 0x83786a09, 4 | BRF_SND },           // 11 oki
};

STD_ROM_PICK(lastfort)
STD_ROM_FN(lastfort)

struct BurnDriver BurnDrvLastfort = {
	"lastfort", NULL, NULL, NULL, "1994",
	"Last Fortress - Toride\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, lastfortRomInfo, lastfortRomName, NULL, NULL, SkyalertInputInfo, LastfortDIPInfo,
	lastfortInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	360, 224, 4, 3
};


// Last Fortress - Toride (German)

static struct BurnRomInfo lastfortgRomDesc[] = {
	{ "tr_ma02.8g",		0x20000, 0xe6f40918, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tr_ma03.10g",	0x20000, 0xb00fb126, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tr_ma01.1i",		0x20000, 0x8a8f5fef, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "tr_ma04.15f",	0x80000, 0x5feafc6f, 3 | BRF_GRA },           //  3 gfx1
	{ "tr_ma07.17d",	0x80000, 0x7519d569, 3 | BRF_GRA },           //  4
	{ "tr_ma05.17f",	0x80000, 0x5d917ba5, 3 | BRF_GRA },           //  5
	{ "tr_ma06.15d",	0x80000, 0xd366c04e, 3 | BRF_GRA },           //  6

	{ "tr_ma08.1d",		0x20000, 0x83786a09, 4 | BRF_SND },           //  7 oki
};

STD_ROM_PICK(lastfortg)
STD_ROM_FN(lastfortg)

struct BurnDriverD BurnDrvLastfortg = {
	"lastfortg", "lastfort", NULL, NULL, "1994",
	"Last Fortress - Toride (German)\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, lastfortgRomInfo, lastfortgRomName, NULL, NULL, LadykillInputInfo, LadykillDIPInfo,
	lastforgInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	360, 224, 4, 3
};


// Dharma Doujou

static struct BurnRomInfo dharmaRomDesc[] = {
	{ "dd__wea5.u39",	0x20000, 0x960319d7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dd__wea6.u42",	0x20000, 0x386eb6b3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "dd__wa-8.u9",	0x20000, 0xaf7ebc4c, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "dd__wa-2.u4",	0x80000, 0x2c67a5c8, 3 | BRF_GRA },           //  3 gfx1
	{ "dd__wa-4.u5",	0x80000, 0x36ca7848, 3 | BRF_GRA },           //  4
	{ "dd__wa-1.u10",	0x80000, 0xd8034574, 3 | BRF_GRA },           //  5
	{ "dd__wa-3.u11",	0x80000, 0xfe320fa3, 3 | BRF_GRA },           //  6

	{ "dd__wa-7.u3",	0x40000, 0x7ce817eb, 4 | BRF_SND },           //  7 oki
};

STD_ROM_PICK(dharma)
STD_ROM_FN(dharma)

struct BurnDriver BurnDrvDharma = {
	"dharma", NULL, NULL, NULL, "1994",
	"Dharma Doujou\0", NULL, "Metro", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, dharmaRomInfo, dharmaRomName, NULL, NULL, DharmaInputInfo, DharmaDIPInfo,
	dharmaInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Pururun

static struct BurnRomInfo pururunRomDesc[] = {
	{ "pu9-19-5.20e",	0x20000, 0x5a466a1b, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "pu9-19-6.20c",	0x20000, 0xd155a53c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pu9-19-8.3i",	0x20000, 0xedc3830b, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "pu9-19-2.14i",	0x80000, 0x21550b26, 3 | BRF_GRA },           //  3 gfx1
	{ "pu9-19-4.18i",	0x80000, 0x3f3e216d, 3 | BRF_GRA },           //  4
	{ "pu9-19-1.12i",	0x80000, 0x7e83a75f, 3 | BRF_GRA },           //  5
	{ "pu9-19-3.16i",	0x80000, 0xd15485c5, 3 | BRF_GRA },           //  6

	{ "pu9-19-7.3g",	0x40000, 0x51ae4926, 4 | BRF_SND },           //  7 oki
};

STD_ROM_PICK(pururun)
STD_ROM_FN(pururun)

struct BurnDriver BurnDrvPururun = {
	"pururun", NULL, NULL, NULL, "1995",
	"Pururun\0", NULL, "Metro / Banpresto", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, pururunRomInfo, pururunRomName, NULL, NULL, PururunInputInfo, PururunDIPInfo,
	pururunInit, DrvExit, NoZ80Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};
