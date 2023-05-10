// FB Neo Midway 68k-based driver module
// Based on MAME driver by Aaron Giles, Bryan McPhail

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6809_intf.h"
#include "midsg.h"
#include "midcvsd.h"
#include "burn_ym2151.h" // midcvsd (ym timer)
#include "midtcs.h"
#include "dac.h"
#include "watchdog.h"
#include "burn_pal.h"
#include "burn_gun.h"
#include "burn_shift.h"
#include "6840ptm.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *Drv68KRAMA;
static UINT8 *Drv68KRAMB;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSndRAM;
static UINT8 *DrvM6809RAM;
static UINT8 DrvTransTab[2][16*4];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 control_data;
static UINT16 protection_data[5];

static void (*control_write)(UINT16 offset) = NULL;
static INT32 sprite_xoffset = 0;
static INT32 sprite_clip = 0;
static INT32 spriteram_size = 0x1000;
static INT32 nGraphicsLen[2];

static UINT16 DrvInputs[3];
static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoyF[16]; // p1,p2 shift up/down
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static INT16 Analog[4];

static ButtonToggle Diag;

static INT32 gear_shifter[2];

static INT32 nCyclesExtra[5];

static INT32 vb_offset = 0; // special timing config for blasted, trisport
static INT32 scanline;

static INT32 is_spyhunt2 = 0;
static INT32 is_trisport = 0;
static INT32 is_pigskin = 0;

static struct BurnInputInfo XenophobInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 14,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p3 coin"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 14,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 13,	"p3 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 7,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Xenophob)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static struct BurnInputInfo Spyhunt2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	A("P1 Wheel",       BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Accelerator", BIT_ANALOG_REL, &Analog[1],		"p1 fire 1"),
	{"P1 R Trigger",	BIT_DIGITAL,	DrvJoy2 + 14,	"p1 fire 2"	},
	{"P1 R Button",		BIT_DIGITAL,	DrvJoy2 + 15,	"p1 fire 3"	},
	{"P1 L Trigger",	BIT_DIGITAL,	DrvJoy2 + 12,	"p1 fire 4"	},
	{"P1 L Button",		BIT_DIGITAL,	DrvJoy2 + 13,	"p1 fire 5"	},
	{"P1 Gear Down",	BIT_DIGITAL,	DrvJoyF + 0,	"p1 fire 6"	},
	{"P1 Gear Up",		BIT_DIGITAL,	DrvJoyF + 1,	"p1 fire 7"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	A("P2 Wheel",       BIT_ANALOG_REL, &Analog[2],		"p2 x-axis"),
	A("P2 Accelerator", BIT_ANALOG_REL, &Analog[3],		"p2 fire 1"),
	{"P2 R Trigger",	BIT_DIGITAL,	DrvJoy2 + 10,	"p2 fire 2"	},
	{"P2 R Button",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 fire 3"	},
	{"P2 L Trigger",	BIT_DIGITAL,	DrvJoy2 + 8,	"p2 fire 4"	},
	{"P2 L Button",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 5"	},
	{"P2 Gear Down",	BIT_DIGITAL,	DrvJoyF + 2,	"p2 fire 6"	},
	{"P2 Gear Up",		BIT_DIGITAL,	DrvJoyF + 3,	"p2 fire 7"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 7,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Spyhunt2)

static struct BurnInputInfo BlastedInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 7,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Blasted)

static struct BurnInputInfo IntlaserInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 fire 1"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p3 coin"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 10,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 11,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 9,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy1 + 10,	"p3 fire 1"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p4 coin"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy2 + 12,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy2 + 14,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy2 + 15,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy2 + 13,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy1 + 11,	"p4 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 7,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Intlaser)

static struct BurnInputInfo ArchrivlInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 start"	},
	A("P1 Stick X",     BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Stick Y",     BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 start"	},
	A("P2 Stick X",     BIT_ANALOG_REL, &Analog[2],		"p2 x-axis"),
	A("P2 Stick Y",     BIT_ANALOG_REL, &Analog[3],		"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 7,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Archrivl)

static struct BurnInputInfo ArchrivlbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 15,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 14,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 13,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 12,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 11,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 10,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 7,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Archrivlb)

static struct BurnInputInfo PigskinInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	A("P1 Stick X",     BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Stick Y",     BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	A("P2 Stick X",     BIT_ANALOG_REL, &Analog[2],		"p2 x-axis"),
	A("P2 Stick Y",     BIT_ANALOG_REL, &Analog[3],		"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 5,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pigskin)

static struct BurnInputInfo TrisportInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 5,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Trisport)

static struct BurnDIPInfo XenophobDIPList[]=
{
	DIP_OFFSET(0x1c)
	{0x00, 0xff, 0xff, 0x3f, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x00, 0x01, 0x04, 0x04, "Off"						},
	{0x00, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Coins per Life Unit"		},
	{0x00, 0x01, 0x08, 0x08, "1"						},
	{0x00, 0x01, 0x08, 0x00, "2"						},

	{0   , 0xfe, 0   ,    2, "Life Unit"				},
	{0x00, 0x01, 0x10, 0x10, "1000"						},
	{0x00, 0x01, 0x10, 0x00, "2000"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x20, 0x00, "Off"						},
	{0x00, 0x01, 0x20, 0x20, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x00, 0x01, 0xc0, 0x40, "Easy"						},
	{0x00, 0x01, 0xc0, 0x00, "Medium"					},
	{0x00, 0x01, 0xc0, 0x80, "Hard"						},
	{0x00, 0x01, 0xc0, 0xc0, "Medium (duplicate)"		},
};

STDDIPINFO(Xenophob)

static struct BurnDIPInfo Spyhunt2DIPList[]=
{
	DIP_OFFSET(0x18)
	{0x00, 0xff, 0xff, 0xef, NULL						},
	{0x01, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x00, 0x01, 0x03, 0x02, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x03, 0x00, "1C/2C (duplicate)"		},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x00, 0x01, 0x04, 0x04, "Off"						},
	{0x00, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Point Thresholds"			},
	{0x00, 0x01, 0x18, 0x08, "Easy"						},
	{0x00, 0x01, 0x18, 0x18, "Medium"					},
	{0x00, 0x01, 0x18, 0x10, "Hard"						},
	{0x00, 0x01, 0x18, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Free Timer After"			},
	{0x00, 0x01, 0x60, 0x00, "30 sec"					},
	{0x00, 0x01, 0x60, 0x40, "45 sec"					},
	{0x00, 0x01, 0x60, 0x60, "60 sec"					},
	{0x00, 0x01, 0x60, 0x20, "90 sec"					},

	{0   , 0xfe, 0   ,    2, "Rack Advance (Cheat)"		},
	{0x00, 0x01, 0x80, 0x80, "Off"						},
	{0x00, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Spyhunt2)

static struct BurnDIPInfo BlastedDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xf3, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x00, 0x01, 0x03, 0x02, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x03, 0x00, "1C/2C (duplicate)"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x00, 0x01, 0x0c, 0x08, "Easy"						},
	{0x00, 0x01, 0x0c, 0x00, "Medium"					},
	{0x00, 0x01, 0x0c, 0x04, "Hard"						},
	{0x00, 0x01, 0x0c, 0x0c, "Medium (duplicate)"		},

	{0   , 0xfe, 0   ,    2, "Dollar Receptor"			},
	{0x00, 0x01, 0x20, 0x20, "Off"						},
	{0x00, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x40, 0x00, "Off"						},
	{0x00, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Rack Advance (Cheat)"		},
	{0x00, 0x01, 0x80, 0x80, "Off"						},
	{0x00, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Blasted)

static struct BurnDIPInfo IntlaserDIPList[]=
{
	DIP_OFFSET(0x1c)
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x00, 0x01, 0x03, 0x02, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x03, 0x00, "1C/1C (duplicate)"		},

	{0   , 0xfe, 0   ,    2, "Rack Advance (Cheat)"		},
	{0x01, 0x01, 0x80, 0x80, "Off"						},
	{0x01, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Intlaser)

static struct BurnDIPInfo ArchrivlDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Game Time"				},
	{0x00, 0x01, 0x03, 0x03, "Preset Time"				},
	{0x00, 0x01, 0x03, 0x02, "Preset + 10sec"			},
	{0x00, 0x01, 0x03, 0x01, "Preset + 20sec"			},
	{0x00, 0x01, 0x03, 0x00, "Preset + 30sec"			},

	{0   , 0xfe, 0   ,    8, "Coinage"					},
	{0x00, 0x01, 0x1c, 0x14, "3 Coins 1 Credits"		},
	{0x00, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x1c, 0x10, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x1c, 0x08, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x1c, 0x04, "1 Coin  5 Credits"		},
	{0x00, 0x01, 0x1c, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Team Names"				},
	{0x00, 0x01, 0x20, 0x20, "Default"					},
	{0x00, 0x01, 0x20, 0x00, "Hometown Heroes"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x40, 0x00, "Off"						},
	{0x00, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x00, 0x01, 0x80, 0x80, "Off"						},
	{0x00, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Archrivl)

static struct BurnDIPInfo ArchrivlbDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Game Time"				},
	{0x00, 0x01, 0x03, 0x03, "Preset Time"				},
	{0x00, 0x01, 0x03, 0x02, "Preset + 10sec"			},
	{0x00, 0x01, 0x03, 0x01, "Preset + 20sec"			},
	{0x00, 0x01, 0x03, 0x00, "Preset + 30sec"			},

	{0   , 0xfe, 0   ,    8, "Coinage"					},
	{0x00, 0x01, 0x1c, 0x14, "3 Coins 1 Credits"		},
	{0x00, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x1c, 0x10, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x1c, 0x08, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x1c, 0x04, "1 Coin  5 Credits"		},
	{0x00, 0x01, 0x1c, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Team Names"				},
	{0x00, 0x01, 0x20, 0x20, "Default"					},
	{0x00, 0x01, 0x20, 0x00, "Hometown Heroes"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x40, 0x00, "Off"						},
	{0x00, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x00, 0x01, 0x80, 0x80, "Off"						},
	{0x00, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Archrivlb)

static struct BurnDIPInfo PigskinDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    4, "Game Time"				},
	{0x00, 0x01, 0x03, 0x00, "Shortest"					},
	{0x00, 0x01, 0x03, 0x02, "Short"					},
	{0x00, 0x01, 0x03, 0x03, "Medium"					},
	{0x00, 0x01, 0x03, 0x01, "Long"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x00, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x0c, 0x00, "Free Play"				},
	{0x00, 0x01, 0x0c, 0x04, "Set Your Own"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x10, 0x00, "Off"						},
	{0x00, 0x01, 0x10, 0x10, "On"						},

	{0   , 0xfe, 0   ,    2, "Test Switch"				},
	{0x00, 0x01, 0x20, 0x20, "Off"						},
	{0x00, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Coin Chutes"				},
	{0x00, 0x01, 0x40, 0x00, "Individual"				},
	{0x00, 0x01, 0x40, 0x40, "Common"					},

	{0   , 0xfe, 0   ,    2, "Joystick"					},
	{0x00, 0x01, 0x80, 0x80, "Standard"					},
	{0x00, 0x01, 0x80, 0x00, "Rotated"					},
};

STDDIPINFO(Pigskin)

static struct BurnDIPInfo TrisportDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    8, "Coinage"					},
	{0x00, 0x01, 0x07, 0x02, "4 Coins 1 Credits"		},
	{0x00, 0x01, 0x07, 0x03, "3 Coins 1 Credits"		},
	{0x00, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x07, 0x05, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x07, 0x04, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x07, 0x01, "Free Play"				},
	{0x00, 0x01, 0x07, 0x00, "Battery Options"			},

	{0   , 0xfe, 0   ,    4, "Pool Turns"				},
	{0x00, 0x01, 0x18, 0x10, "5"						},
	{0x00, 0x01, 0x18, 0x08, "6"						},
	{0x00, 0x01, 0x18, 0x18, "7"						},
	{0x00, 0x01, 0x18, 0x00, "8"						},

	{0   , 0xfe, 0   ,    2, "Bowling Difficulty"		},
	{0x00, 0x01, 0x20, 0x20, "Standard"					},
	{0x00, 0x01, 0x20, 0x00, "Advanced"					},

	{0   , 0xfe, 0   ,    2, "Shot Timer"				},
	{0x00, 0x01, 0x40, 0x00, "Slower"					},
	{0x00, 0x01, 0x40, 0x40, "Standard"					},

	{0   , 0xfe, 0   ,    2, "Golf Holes"				},
	{0x00, 0x01, 0x80, 0x80, "3"						},
	{0x00, 0x01, 0x80, 0x00, "4"						},
};

STDDIPINFO(Trisport)

static void sync_ptm()
{
	INT32 cyc = (SekTotalCycles() / 10) - ptm6840TotalCycles();
	if (cyc > 0) {
		ptm6840Run(cyc);
	}
}

static void __fastcall mcr68_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffff0) == 0x0a0000) {
		bprintf(0, _T("ptm_write.w %x  %x\n"),(address & 0xf) >> 1,data);
		sync_ptm();
		ptm6840_write((address & 0xf) >> 1, (data >> 8) & 0xff);
		// ptm6840 write - mask = 0x00ff, cswidth = 16?
		return;
	}

	if ((address & 0xff0000) == 0x0b0000) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0xff0000) == 0x0c0000) {
		control_data = data;
		if (control_write) control_write(address);
		return;
	}

	bprintf(0, _T("mww  %x  %x\n"), address, data);
}

static void __fastcall mcr68_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffff0) == 0x0a0000) {
	   // bprintf(0, _T("ptm_write.b %x  %x\n"),(address & 0xf) ,data);
		sync_ptm();
		ptm6840_write((address & 0xf) >> 1, data & 0xff);
		// ptm6840 write - mask = 0x00ff, cswidth = 16?
		return;
	}

	if ((address & 0xff0000) == 0x0b0000) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0xff0000) == 0x0c0000) {
		control_data &= 0xff << (( address & 1) * 8);
		control_data |= data << ((~address & 1) * 8);
		if (control_write) control_write(address);
		return;
	}

	bprintf(0, _T("mwb  %x  %x\n"), address, data);
}

static UINT16 __fastcall mcr68_main_read_word(UINT32 address)
{
	switch (address & 0x0f0000)
	{
		case 0x0a0000:
			sync_ptm();
			return (ptm6840_read((address & 0xf) >> 1) << 8) | 0x00ff;

		case 0x0d0000:
			return DrvInputs[0]; // in0

		case 0x0e0000:
			return DrvInputs[1]; // in1

		case 0x0f0000:
			return (DrvDips[1] * 256) + DrvDips[0];
	}

	bprintf(0, _T("mrw  %x\n"), address);

	return 0xffff;
}

static UINT8 __fastcall mcr68_main_read_byte(UINT32 address)
{
	switch (address & 0x0f0001)
	{
		case 0x0a0000: // spyhunt2, xenophobe
		case 0x0a0001: // blasted, archriv
			sync_ptm();
			return ptm6840_read((address & 0xf) >> 1);

		case 0x0d0000:
		case 0x0d0001:
			return DrvInputs[0] >> ((~address & 1) * 8); // in0

		case 0x0e0000:
		case 0x0e0001:
			return DrvInputs[1] >> ((~address & 1) * 8); // in1

		case 0x0f0000:
			return DrvDips[1];

		case 0x0f0001:
			return DrvDips[0];
	}

	bprintf(0, _T("mrb  %x\n"), address);

	return 0xff;
}

static void __fastcall pigskin_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0x1f0000) == 0x0e0000) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0x1f0000) == 0x1a0000) {
		control_data = data;
		if (control_write) control_write(address);
		return;
	}

	if ((address & 0xfffff0) == 0x180000) {
		sync_ptm();
		ptm6840_write((address & 0xf) >> 1, (data >> 8) & 0xff);
		// ptm6840 write - mask = 0xff00
		return;
	}
}

static void __fastcall pigskin_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0x1f0000) == 0x0e0000) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0x1f0000) == 0x1a0000) {
		control_data &= 0xff << (( address & 1) * 8);
		control_data |= data << ((~address & 1) * 8);
		if (control_write) control_write(address);
		return;
	}

	if ((address & 0xfffff1) == 0x180000) {
		sync_ptm();
		ptm6840_write((address & 0xf) >> 1, data);
		// ptm6840 write - mask = 0xff00
		return;
	}

	if ((address & 0xfffffe) == 0x120000) {
		protection_data[0] = protection_data[1];
		protection_data[1] = protection_data[2];
		protection_data[2] = protection_data[3];
		protection_data[3] = protection_data[4];
		protection_data[4] = data;
		return;
	}
}

static UINT8 read_49way(INT16 analog, INT32 reversed)
{
	static const UINT8 translate49[7] = { 0x7, 0x3, 0x1, 0x0, 0xc, 0xe, 0xf }; // 49-way joystick conversion table

	return translate49[ProcessAnalog(analog, reversed, INPUT_DEADZONE, 0x00, 0x6f) >> 4];
}

static UINT16 __fastcall pigskin_main_read_word(UINT32 address)
{
	if ((address & 0x1f0000) == 0x080000) {
		return (DrvInputs[1] & 0xff) | (read_49way(Analog[0], 1) << 12) | (read_49way(Analog[1], 0) << 8);
	}

	if ((address & 0x1f0000) == 0x0a0000) {
		return DrvDips[0] | (read_49way(Analog[2], 1) << 12) | (read_49way(Analog[3], 0) << 8);
	}

	if ((address & 0x1ffff0) == 0x180000) {
		sync_ptm();
		return ptm6840_read((address & 0xf) >> 1) << 8;
	}

	if ((address & 0x1f0000) == 0x1e0000) {
		return DrvInputs[0];
	}

	return 0;
}

static UINT8 __fastcall pigskin_main_read_byte(UINT32 address)
{
	if ((address & 0x1f0000) == 0x080000) {
		return SekReadWord(address) >> ((~address & 1) * 8);
	}

	if ((address & 0x1f0000) == 0x0a0000) {
		return SekReadWord(address) >> ((~address & 1) * 8);
	}

	if ((address & 0x1ffffe) == 0x120000) {
		if (protection_data[4] == 0xe3 && protection_data[3] == 0x94)
			return 0x00;    /* must be <= 1 */
		if (protection_data[4] == 0xc7 && protection_data[3] == 0x7b && protection_data[2] == 0x36)
			return 0x00;    /* must be <= 1 */
		if (protection_data[4] == 0xc7 && protection_data[3] == 0x7b)
			return 0x07;    /* must be > 5 */
		if (protection_data[4] == 0xc7 && protection_data[3] == 0x1f && protection_data[2] == 0x03 &&
				protection_data[1] == 0x25 && protection_data[0] == 0x36)
			return 0x00;    /* must be < 3 */

		return 0;
	}

	if ((address & 0x1ffff1) == 0x180000) {
		sync_ptm();
		return ptm6840_read((address & 0xf) >> 1);
	}

	if ((address & 0x1f0000) == 0x1e0000) {
		return DrvInputs[0] >> ((~address & 1) * 8);
	}

	return 0;
}

static void __fastcall trisport_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0x1ffff0) == 0x180000) {
		sync_ptm();
		ptm6840_write((address & 0xf) >> 1, (data >> 8) & 0xff);
		// ptm6840 - umask &= 0xff00
		return;
	}

	if ((address & 0x1f0000) == 0x1a0000) {
		control_data = data;
		if (control_write) {
			control_write(address);
		}
		return;
	}

	if ((address & 0x1f0000) == 0x1c0000) {
		BurnWatchdogWrite();
		return;
	}
}

static void __fastcall trisport_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0x1ffff0) == 0x180000) {
		sync_ptm();
		ptm6840_write((address & 0xf) >> 1, data);
		// ptm6840 - umask &= 0xff00
		return;
	}

	if ((address & 0x1f0000) == 0x1a0000) {
		control_data &= (0xff << ((address & 1) * 8));
		control_data |= data << ((~address & 1) * 8);
		if (control_write) {
			control_write(address);
		}
		return;
	}

	if ((address & 0x1f0000) == 0x1c0000) {
		BurnWatchdogWrite();
		return;
	}
}

static UINT16 __fastcall trisport_main_read_word(UINT32 address)
{
	if ((address & 0x1f0000) == 0x080000) {
		return 0xff | ((BurnTrackballReadInterpolated(0, scanline) & 0xf) << 8) | ((BurnTrackballReadInterpolated(1, scanline) & 0xf) << 12);
	}

	if ((address & 0x1f0000) == 0x0a0000) {
		return (DrvDips[1] * 256) + DrvDips[0];
	}

	if ((address & 0x1ffff0) == 0x180000) {
		sync_ptm();
		return ptm6840_read((address & 0xf) >> 1) << 8;
	}

	if ((address & 0x1f0000) == 0x1e0000) {
		return DrvInputs[0];
	}
	bprintf(0, _T("mrw %x\n"), address);

	return 0xffff;
}

static UINT8 __fastcall trisport_main_read_byte(UINT32 address)
{
	if ((address & 0x1f0000) == 0x080000) {
		return SekReadWord(address) >> ((~address & 1) * 8);
	}

	if ((address & 0x1f0000) == 0x0a0000) {
		return (DrvDips[1] * 256) + DrvDips[0];
	}

	if ((address & 0x1ffff0) == 0x180000) {
		sync_ptm();
		return ptm6840_read((address & 0xf) >> 1);
	}

	if ((address & 0x1f0000) == 0x1e0000) {
		return DrvInputs[0] >> ((~address & 1) * 8);
	}

	bprintf(0, _T("mrb %x\n"), address);

	return 0xff;
}

static UINT8 __fastcall common_read_port_byte(UINT32 address)
{
	return SekReadWord(address) >> ((~address & 1) * 8);
}

static void archrivl_control_write(UINT16)
{
	M6809Open(0);
	cvsd_reset_write(~control_data & 0x0400);
	cvsd_data_write(control_data & 0x3ff);
	M6809Close();
}

static tilemap_callback( bg )
{
	UINT16 *vram = (UINT16*)DrvVidRAM;

	UINT16 attr = (vram[offs * 2] & 0xff) | (vram[offs * 2 + 1] << 8);
	UINT16 code = (attr & 0x3ff) | ((attr & 0xc000) >> 4);
	UINT16 color = ((attr >> 12) & 3) ^ 3;
	UINT32 flags = TILE_FLIPYX(attr >> 10) | TILE_GROUP(attr >> 15);

	TILE_SET_INFO(0, code, color, flags);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	soundsgood_reset();
	tcs_reset();
	cvsd_reset();

	HiscoreReset();

	control_data = 0;

	memset(nCyclesExtra, 0, sizeof(nCyclesExtra));

	gear_shifter[0] = gear_shifter[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x040000;
	DrvSndROM		= Next; Next += 0x100000;
	DrvM6809ROM		= Next; Next += 0x100000;

	DrvGfxROM0		= Next; Next += 0x100000;
	DrvGfxROM1		= Next; Next += 0x100000;

	DrvPalette		= (UINT32*)Next; Next += 0x00040 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAMA		= Next; Next += 0x004000;
	Drv68KRAMB		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x000400;
	DrvSndRAM		= Next; Next += 0x001000;
	DrvM6809RAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoadRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[3] = { Drv68KROM, DrvSndROM, DrvM6809ROM };
	UINT8 *gLoad[2] = { DrvGfxROM0, DrvGfxROM1 };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && ((ri.nType & 7) == 1 || (ri.nType & 7) == 2)) {
			INT32 type = (ri.nType - 1) & 1;
            if (bLoad) {
				if (BurnLoadRom(pLoad[type] + 1, i + 0, 2)) return 1;
				if (BurnLoadRom(pLoad[type] + 0, i + 1, 2)) return 1;
			}
			i++;
			pLoad[type] += ri.nLen * 2;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 3) {
			INT32 type = 2;
            if (bLoad) {
				memmove (pLoad[type], pLoad[type] + ri.nLen, 0x10000 - ri.nLen); // m6809 loaded at end of mem space
				if (BurnLoadRom(pLoad[type] + 0x10000 - ri.nLen, i, 1)) return 1;
			}
			continue;
		}

		if ((ri.nType & BRF_PRG) && ((ri.nType & 7) == 4)) {
			INT32 type = 2;
            if (bLoad) {
				if (BurnLoadRom(pLoad[type], i, 1)) return 1;
				for (INT32 j = ri.nLen; j < 0x20000; j += ri.nLen) { // mirror
					memcpy (pLoad[type] + j, pLoad[type], ri.nLen);
				}
			}
			pLoad[type] += 0x20000;
			continue;
		}

		if ((ri.nType & BRF_GRA) && ((ri.nType & 7) == 3 || (ri.nType & 7) == 4)) {
			INT32 type = (ri.nType - 3) & 1;
			if (bLoad) if (BurnLoadRom(gLoad[type], i, 1)) return 1;
			gLoad[type] += ri.nLen;
			continue;
		}
	}

	nGraphicsLen[0] = gLoad[0] - DrvGfxROM0;
	nGraphicsLen[1] = gLoad[1] - DrvGfxROM1;

	return 0;
}

static void DrvBuildTransTab()
{
	for (INT32 i = 0; i < 16*4; i++) { // 4bpp pixels * 4 color banks
		DrvTransTab[0][i] = (0x0101 & (1 << (i & 0xf))) ? 0xff : 0;
		DrvTransTab[1][i] = (0xfeff & (1 << (i & 0xf))) ? 0xff : 0;
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { (nGraphicsLen[0]/2)*8+0, (nGraphicsLen[0]/2)*8+1, 0, 1 };
	INT32 XOffs0[16] = { STEP8(0,2) };
	INT32 YOffs0[16] = { STEP8(0,16) };

	INT32 L = (nGraphicsLen[1]/4)*8;
	INT32 Plane1[4]  = { STEP4(0,1) };
	INT32 XOffs1[32] = {
		L*0+0, L*0+4, L*1+0, L*1+4, L*2+0, L*2+4, L*3+0, L*3+4,
		L*0+0+8, L*0+4+8, L*1+0+8, L*1+4+8, L*2+0+8, L*2+4+8, L*3+0+8, L*3+4+8,
		L*0+0+16, L*0+4+16, L*1+0+16, L*1+4+16, L*2+0+16, L*2+4+16, L*3+0+16, L*3+4+16,
		L*0+0+24, L*0+4+24, L*1+0+24, L*1+4+24, L*2+0+24, L*2+4+24, L*3+0+24, L*3+4+24
	};
	INT32 YOffs1[32] = { STEP32(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(nGraphicsLen[1]);
	if (tmp == NULL) {
		return 1;
	}

	GfxDecode((nGraphicsLen[0] * 2) / (8 * 8), 4, 8, 8, Plane0, XOffs0, YOffs0, 0x080, DrvGfxROM0, tmp);

	for (INT32 i = 0; i < nGraphicsLen[0] * 2; i+=0x40) { // 2x size and invert pixel
		for (INT32 y = 0; y < 16; y++) {
			for (INT32 x = 0; x < 16; x++) {
				DrvGfxROM0[(i * 4) + (y * 16) + x] = tmp[i + ((y / 2) * 8) + (x / 2)] ^ 0xf;
			}
		}
	}

	memcpy (tmp, DrvGfxROM1, nGraphicsLen[1]);

	GfxDecode((nGraphicsLen[1] * 2) / (32 * 32), 4, 32, 32, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static void mcr68_irq_cb(INT32 state)
{
	SekSetIRQLine(2, (state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 Mcr68Init(INT32 sound_system)
{
	BurnSetRefreshRate(30.00);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	DrvGfxDecode();

	SekInit(0, 0x68000);
	SekOpen(0);
	SekSetAddressMask(0x1fffff);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAMA,		0x060000, 0x063fff, MAP_RAM);
	SekMapMemory(DrvVidRAM,			0x070000, 0x070fff, MAP_RAM);
	SekMapMemory(Drv68KRAMB,		0x071000, 0x071fff, MAP_RAM); //?
	SekMapMemory(DrvSprRAM,			0x080000, 0x080fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x090000, 0x09007f | 0x3ff, MAP_RAM); // Sek page size is 1k
	SekSetWriteWordHandler(0,		mcr68_main_write_word);
	SekSetWriteByteHandler(0,		mcr68_main_write_byte);
	SekSetReadWordHandler(0,		mcr68_main_read_word);
	SekSetReadByteHandler(0,		mcr68_main_read_byte);
	SekClose();

	ptm6840_init(7723800 / 10);
	ptm6840_set_irqcb(mcr68_irq_cb);

	BurnWatchdogInit(DrvDoReset, -1); // disable for now

	switch (sound_system)
	{
		case 0:
			soundsgood_init(1, 0, DrvSndROM, DrvSndRAM);
		break;

		case 1:
			soundsgood_init(1, 0, DrvSndROM, DrvSndRAM);
			tcs_init(0, 1, 1, DrvM6809ROM, DrvM6809RAM);
		break;

		case 2:
			cvsd_init(0, 0, 0, DrvM6809ROM, DrvM6809RAM);
		break;
	}

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 16, 16, (nGraphicsLen[0] * 2 * 2 * 2), 0, 3);
	GenericTilemapSetTransparent(0,0);

	spriteram_size = 0x1000;

	DrvBuildTransTab();

	DrvDoReset(1);

	return 0;
}

static INT32 PigskinInit()
{
	BurnSetRefreshRate(30.00);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	DrvGfxDecode();

	SekInit(0, 0x68000);
	SekOpen(0);
	SekSetAddressMask(0x1fffff);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x0c0000, 0x0c007f | 0x3ff, MAP_RAM); // Sek page size is 1k
	SekMapMemory(DrvVidRAM,			0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(Drv68KRAMA,		0x140000, 0x143fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x160000, 0x1607ff, MAP_RAM);
	SekSetWriteWordHandler(0,		pigskin_main_write_word);
	SekSetWriteByteHandler(0,		pigskin_main_write_byte);
	SekSetReadWordHandler(0,		pigskin_main_read_word);
	SekSetReadByteHandler(0,		pigskin_main_read_byte);
	SekClose();

	ptm6840_init(7723800 / 10);
	ptm6840_set_irqcb(mcr68_irq_cb);

	BurnWatchdogInit(DrvDoReset, -1); // disable for now

	cvsd_init(0, 0, 0, DrvM6809ROM, DrvM6809RAM);
	control_write = archrivl_control_write;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 16, 16, (nGraphicsLen[0] * 2 * 2 * 2), 0, 3);
	GenericTilemapSetTransparent(0,0);

	spriteram_size = 0x800;
	sprite_clip = 16;
	is_pigskin = 1;

	DrvBuildTransTab();

	DrvDoReset(1);

	return 0;
}

static INT32 TrisportInit()
{
	BurnSetRefreshRate(30.00);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	DrvGfxDecode();

	SekInit(0, 0x68000);
	SekOpen(0);
	SekSetAddressMask(0x1fffff);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAMA,	0x100000, 0x103fff, MAP_RAM); // nvram!!
	SekMapMemory(DrvPalRAM,		0x120000, 0x12007f | 0x3ff, MAP_RAM); // page size is 1k!
	SekMapMemory(DrvSprRAM,		0x140000, 0x1407ff, MAP_RAM);
	SekMapMemory(DrvVidRAM,		0x160000, 0x160fff, MAP_RAM);
	SekSetWriteWordHandler(0,	trisport_main_write_word);
	SekSetWriteByteHandler(0,	trisport_main_write_byte);
	SekSetReadWordHandler(0,	trisport_main_read_word);
	SekSetReadByteHandler(0,	trisport_main_read_byte);
	SekClose();

	ptm6840_init(7723800 / 10);
	ptm6840_set_irqcb(mcr68_irq_cb);

	BurnWatchdogInit(DrvDoReset, -1); // disable for now

	cvsd_init(0, 0, 0, DrvM6809ROM, DrvM6809RAM);
	control_write = archrivl_control_write;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 16, 16, (nGraphicsLen[0] * 2 * 2 * 2), 0, 3);
	GenericTilemapSetTransparent(0,0);

	spriteram_size = 0x800;
	is_trisport = 1;
	vb_offset = -2;

	BurnTrackballInit(1);

	DrvBuildTransTab();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	cvsd_exit();
	soundsgood_exit();
	tcs_exit();

	BurnFreeMemIndex();

	if (is_trisport) {
		BurnTrackballExit();
	}

	control_write = NULL;
	sprite_xoffset = 0;
	sprite_clip = 0;

	vb_offset = 0;

	is_spyhunt2 = 0;
	is_trisport = 0;
	is_pigskin = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x80/2; i++)
	{
		UINT8 r = pal3bit(p[i] >> 6);
		UINT8 g = pal3bit(p[i] >> 0);
		UINT8 b = pal3bit(p[i] >> 3);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_sprites(INT32 priority)
{
	INT32 codemask = (nGraphicsLen[1] * 2) / (32 * 32);
	UINT16 *ram = (UINT16*)DrvSprRAM;

	GenericTilesSetClip(sprite_clip, nScreenWidth - sprite_clip, -1, -1);

	memset (pPrioDraw, 1, nScreenWidth * nScreenHeight);

	for (INT32 offs = spriteram_size / 2 - 4; offs >= 0; offs -= 4)
	{
		INT32 flags = ram[offs + 1] & 0xff;
		INT32 code = (ram[offs + 2] & 0xff) + 256 * ((flags >> 3) & 0x01) + 512 * ((flags >> 6) & 0x03);

		if (code == 0 || ((flags >> 2) & 1) != priority) continue;

		INT32 color = ~flags & 0x03;
		INT32 flipx = flags & 0x10;
		INT32 flipy = flags & 0x20;
		INT32 sx = (ram[offs + 3] & 0xff) * 2 + sprite_xoffset;
		INT32 sy = (241 - (ram[offs] & 0xff)) * 2;

		if (sx > 0x1f0) sx -= 0x200;

		RenderPrioMaskTranstabSprite(pTransDraw, DrvGfxROM1, code % codemask, (color * 16), 0xff, sx, sy, flipx, flipy, 32, 32, DrvTransTab[0], 0);
		RenderPrioMaskTranstabSprite(pTransDraw, DrvGfxROM1, code % codemask, (color * 16), 0xff, sx, sy, flipx, flipy, 32, 32, DrvTransTab[1], 2);
	}

	GenericTilesClearClip();
}

static INT32 DrvDraw()
{
	DrvPaletteUpdate();

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_FORCEOPAQUE);

	if (nSpriteEnable & 1) draw_sprites(0);

	if (nBurnLayer & 2)
		if (nGraphicsLen[0] <= 0x10000) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));

	if (nSpriteEnable & 2) draw_sprites(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	SekNewFrame();
	ptm6840NewFrame();

	if (cvsd_initialized() || tcs_initialized()) M6809NewFrame();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		if (is_trisport || is_pigskin) {
			Diag.Toggle(DrvJoy1[5]);
		} else {
			Diag.Toggle(DrvJoy1[7]);
		}

		UINT16 last2 = DrvInputs[2];

		DrvInputs[0] = 0xffff;
		DrvInputs[1] = 0xffff;
		DrvInputs[2] = 0;
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoyF[i] & 1) << i;
		}

		if (is_spyhunt2) {
			// spyhunt2 3-gear shifter
			if (DrvInputs[2] & 1 && ~last2&1 && gear_shifter[0] > 0) gear_shifter[0]--;
			if (DrvInputs[2] & 2 && ~last2&2 && gear_shifter[0] < 2) gear_shifter[0]++;

			if (DrvInputs[2] & 4 && ~last2&4 && gear_shifter[1] > 0) gear_shifter[1]--;
			if (DrvInputs[2] & 8 && ~last2&8 && gear_shifter[1] < 2) gear_shifter[1]++;

			DrvInputs[1] &= ~(1<<(gear_shifter[0] + 4));
			DrvInputs[1] &= ~(1<<(gear_shifter[1] + 0));
		}

		if (is_trisport) {
			BurnTrackballConfig(0, AXIS_NORMAL, AXIS_REVERSED);
			BurnTrackballFrame(0, Analog[0], Analog[1], 0x01, 0x7f, 512);
			BurnTrackballUpdate(0);
		}
	}

	INT32 nInterleave = 512;
	INT32 nCyclesTotal[5] = { 7723800 / 30, 7723800/10 / 30 /*ptm*/, 8000000 / 30 /*sounds good*/, 2000000 / 30 /*cvsd*/, 2000000 / 30 /*tcs*/};
	INT32 nCyclesDone[5] = { 0, 0, 0, 0, 0 };

	SekIdle(0, nCyclesExtra[0]); // when CPU_RUN_SYNCINT is used, we have to Idle extra cycles. (overflow from previous frame)
	ptm6840Idle(nCyclesExtra[0]);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;
		SekOpen(0);
		if (i == 493) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
			ptm6840_set_c1(0);
			ptm6840_set_c1(1);
		}
		if (i == 493+vb_offset) SekSetIRQLine(1, CPU_IRQSTATUS_ACK); // some (blasted) need special timing
		if (i == nInterleave-1) SekSetIRQLine(1, CPU_IRQSTATUS_NONE);
		ptm6840_set_c3(0);
		ptm6840_set_c3(1);

		CPU_RUN_SYNCINT(0, Sek);
		sync_ptm();
		//CPU_RUN_SYNCINT(1, ptm6840); // no! let's sync instead (above) :)
		SekClose();

		if (soundsgood_initialized()) {
			SekOpen(1);
			if (soundsgood_reset_status())
			{
				CPU_IDLE_SYNCINT(2, Sek);
			}
			else
			{
				CPU_RUN_SYNCINT(2, Sek);
			}
			SekClose();
		}

		if (cvsd_initialized())
		{
			M6809Open(0);
			if (cvsd_reset_status())
			{
				CPU_IDLE_SYNCINT(3, M6809);
			}
			else
			{
				CPU_RUN_TIMER(3);
			}

			M6809Close();
		}

		if (tcs_initialized())
		{
			M6809Open(0);
			if (tcs_reset_status())
			{
				CPU_IDLE_SYNCINT(4, M6809);
			}
			else
			{
				CPU_RUN_SYNCINT(4, M6809);
			}
			M6809Close();
		}
	}

	nCyclesExtra[0] = SekTotalCycles(0) - nCyclesTotal[0];

	if (pBurnSoundOut) {
		if (soundsgood_initialized() && !tcs_initialized()) {
			BurnSoundClear();
			DACUpdate(pBurnSoundOut, nBurnSoundLen);
			BurnSoundDCFilter();
		}

		if (cvsd_initialized()) {
			cvsd_update(pBurnSoundOut, nBurnSoundLen);
		}

		if (soundsgood_initialized() && tcs_initialized()) {
			BurnSoundClear();
			DACUpdate(pBurnSoundOut, nBurnSoundLen);
			BurnSoundDCFilter();
		}
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);

		ptm6840_scan(nAction);

		tcs_scan(nAction, pnMin);
		soundsgood_scan(nAction, pnMin);
		cvsd_scan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		Diag.Scan();

		if (is_trisport) {
			BurnTrackballScan();
		}

		SCAN_VAR(control_data);
		SCAN_VAR(protection_data);

		SCAN_VAR(gear_shifter);

		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_NVRAM && is_trisport) { // trisport nvram
		ba.Data		= Drv68KRAMA;
		ba.nLen		= 0x04000;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Xenophobe

static struct BurnRomInfo xenophobRomDesc[] = {
	{ "xeno_pro.3c",				0x10000, 0xf44c2e60, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "xeno_pro.3b",				0x10000, 0x01609a3b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "xeno_pro.2c",				0x10000, 0xe45bf669, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "xeno_pro.2b",				0x10000, 0xda5d39d5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "xeno_snd.u7",				0x10000, 0x77561d15, 2 | BRF_PRG | BRF_ESS }, //  4 Sounds Good 68K Code
	{ "xeno_snd.u17",				0x10000, 0x837a1a71, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "xeno_snd.u8",				0x10000, 0x6e2915c7, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "xeno_snd.u18",				0x10000, 0x12492145, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "xeno_bg.11d",				0x08000, 0x3d2cf284, 3 | BRF_GRA },           //  8 Background Tiles
	{ "xeno_bg.12d",				0x08000, 0xc32288b1, 3 | BRF_GRA },           //  9

	{ "xeno_fg.7j",					0x10000, 0xb12eddb2, 4 | BRF_GRA },           // 10 Sprites
	{ "xeno_fg.8j",					0x10000, 0x20e682f5, 4 | BRF_GRA },           // 11
	{ "xeno_fg.9j",					0x10000, 0x82fb3e09, 4 | BRF_GRA },           // 12
	{ "xeno_fg.10j",				0x10000, 0x6a7a3516, 4 | BRF_GRA },           // 13

	{ "b61a-49aaj-axad.bin",		0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 14 CPU PLDs
	{ "b75a-50aaj-bxad.bin",		0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 15
	{ "b75a-50aaj-axad.bin",		0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 16
	{ "b75a-41aaj-axad.bin",		0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 17
	{ "b75a-41aaj-bxab.bin",		0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 18
	{ "a59a26axlaxhd.bin",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 19
	{ "a59a26axlbxhd.bin",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 20
	{ "a59a26axlcxhd.bin",			0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 21
	{ "0066-316bx-xxqx.bin",		0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 22
	{ "0066-314bx-xxqx.bin",		0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 23
	{ "0066-315bx-xxqx.bin",		0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 24
	{ "0066-313bx-xxqx.bin",		0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 25

	{ "e36a31axnaxad.bin",			0x000cc, 0x33e62608, 0 | BRF_OPT },           // 26 Sounds Good PLD
};

STD_ROM_PICK(xenophob)
STD_ROM_FN(xenophob)

static void xenophob_control_write(UINT16)
{
	float cycles = (SekTotalCycles() * 40000.00) / 38619.00;

	SekClose();
	SekOpen(1);
	cycles -= SekTotalCycles();
	if (cycles >= 1) SekRun((UINT32)cycles);
	soundsgood_data_write(((control_data & 0xf) << 1) | ((control_data & 0x10) >> 4));
	SekClose();
	SekOpen(0);
}

static INT32 XenophobInit()
{
	sprite_xoffset = -4;
	control_write = xenophob_control_write;

	return Mcr68Init(0);
}

struct BurnDriver BurnDrvXenophob = {
	"xenophob", NULL, NULL, NULL, "1987",
	"Xenophobe\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, xenophobRomInfo, xenophobRomName, NULL, NULL, NULL, NULL, XenophobInputInfo, XenophobDIPInfo,
	XenophobInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	512, 480, 4, 3
};


// Spy Hunter II (rev 2)

static struct BurnRomInfo spyhunt2RomDesc[] = {
	{ "sh2_3c_rev2.3c",				0x10000, 0x30b91c90, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "sh2_3b_rev2.3b",				0x10000, 0xf64513c6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sh2_2c_rev2.2c",				0x10000, 0x8ee65009, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sh2_2b_rev2.2b",				0x10000, 0x850c21ad, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "spyhunter_ii_u5.u5",			0x04000, 0x4b1d8a66, 3 | BRF_PRG | BRF_ESS }, //  4 Turbo Cheap Squeak M6809 Code
	{ "spyhunter_ii_u4.u4",			0x04000, 0x3722ce48, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "spyhunter_ii_u7_sound.u7",	0x10000, 0x02362ea4, 2 | BRF_PRG | BRF_ESS }, //  6 Sounds Good 68K Code
	{ "spyhunter_ii_u17_sound.u17",	0x10000, 0xe29a2c37, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "sh2_bg0_rev2.11d",			0x08000, 0xcb3c3d8e, 3 | BRF_GRA },           //  8 Background Tiles
	{ "sh2_bg1_rev2.12d",			0x08000, 0x029d4af1, 3 | BRF_GRA },           //  9

	{ "fg0.7j",						0x20000, 0x55ce12ea, 4 | BRF_GRA },           // 10 Sprites
	{ "fg1.8j",						0x20000, 0x692afb67, 4 | BRF_GRA },           // 11
	{ "fg2.9j",						0x20000, 0xf1aba383, 4 | BRF_GRA },           // 12
	{ "fg3.10j",					0x20000, 0xd3475ff8, 4 | BRF_GRA },           // 13

	{ "pal20l8.9b",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 14 PLDs
	{ "pal16l8.1j",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 15
	{ "pal16l8.2j",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 16
	{ "pal16r4.2k",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17
	{ "pal16r4.14k",				0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 18
	{ "pal20.u15",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
};

STD_ROM_PICK(spyhunt2)
STD_ROM_FN(spyhunt2)

static void spyhunt2_control_write(UINT16)
{
	float cycles = (SekTotalCycles() * 10000.00) / 38619.00;

	M6809Open(0);
	cycles -= M6809TotalCycles();
	if (cycles >= 1) M6809Run((UINT32)cycles);
//	tcs_reset_write(~control_data & 0x2000);
	tcs_data_write((control_data & 0x1f00) >> 8);
	M6809Close();

	cycles = (SekTotalCycles() * 40000.00) / 38619.00;

	SekClose();
	SekOpen(1);
	cycles -= SekTotalCycles();
	if (cycles >= 1) SekRun((UINT32)cycles);
	soundsgood_reset_write((~control_data & 0x2000) >> 13);
	soundsgood_data_write((control_data & 0x1f00) >> 8);
	SekClose();
	SekOpen(0);
}

static UINT16 __fastcall spyhunt2_read_port_word(UINT32 address)
{
	if ((address & 0x1f0000) == 0xd0000) {
		INT32 analog_select = (control_data >> 3) & 3;

		UINT8 temp = 0;

		switch (analog_select) {
			case 0: { // p2 accel.
				temp = ProcessAnalog(Analog[3], 1, INPUT_LINEAR | INPUT_MIGHTBEDIGITAL | INPUT_DEADZONE, 0x30, 0xff);
				break;
			}
			case 1: { // p1 accel.
				temp = ProcessAnalog(Analog[1], 1, INPUT_LINEAR | INPUT_MIGHTBEDIGITAL | INPUT_DEADZONE, 0x30, 0xff);
				break;
			}
			case 2: { // p2 wheel
				temp = ProcessAnalog(Analog[2], 1, INPUT_DEADZONE, 0x10, 0xf0);
				break;
			}
			case 3: { // p1 wheel
				temp = ProcessAnalog(Analog[0], 1, INPUT_DEADZONE, 0x10, 0xf0);
				break;
			}
		}

		return (DrvInputs[0] & 0x00df) | (soundsgood_status_read() ? 0x20 : 0) | (temp << 8);
	}

	if ((address & 0x1f0000) == 0xe0000) {
		return (DrvInputs[1] & 0xff7f) | (tcs_status_read() ? 0x80 : 0);
	}

	return 0;
}

static INT32 Spyhunt2Init()
{
	is_spyhunt2 = 1;
	sprite_xoffset = -4;
	control_write = spyhunt2_control_write;

	INT32 nRet = Mcr68Init(1);

	if (nRet == 0) {
		SekOpen(0);
		SekMapHandler(1,			0xd0000, 0xeffff, MAP_READ);
		SekSetReadWordHandler(1,	spyhunt2_read_port_word);
		SekSetReadByteHandler(1,	common_read_port_byte);
		SekClose();
	}

	return nRet;
}

struct BurnDriver BurnDrvSpyhunt2 = {
	"spyhunt2", NULL, NULL, NULL, "1987",
	"Spy Hunter II (rev 2)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, spyhunt2RomInfo, spyhunt2RomName, NULL, NULL, NULL, NULL, Spyhunt2InputInfo, Spyhunt2DIPInfo,
	Spyhunt2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	512, 480, 4, 3
};


// Spy Hunter II (rev 1)

static struct BurnRomInfo spyhunt2aRomDesc[] = {
	{ "sh2_3c.3c",					0x10000, 0x5b92aadf, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "sh2_3b.3b",					0x10000, 0x6ed0a25f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sh2_2c.2c",					0x10000, 0xbc834f3f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sh2_2b.2b",					0x10000, 0x8a9f7ef3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "spyhunter_ii_u5.u5",			0x04000, 0x4b1d8a66, 3 | BRF_PRG | BRF_ESS }, //  4 Turbo Cheap Squeak M6809 Code
	{ "spyhunter_ii_u4.u4",			0x04000, 0x3722ce48, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "spyhunter_ii_u7_sound.u7",	0x10000, 0x02362ea4, 2 | BRF_PRG | BRF_ESS }, //  6 Sounds Good 68K Code
	{ "spyhunter_ii_u17_sound.u17",	0x10000, 0xe29a2c37, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "bg0.11d",					0x08000, 0x81efef7a, 3 | BRF_GRA },           //  8 Background Tiles
	{ "bg1.12d",					0x08000, 0x6a902e4d, 3 | BRF_GRA },           //  9

	{ "fg0.7j",						0x20000, 0x55ce12ea, 4 | BRF_GRA },           // 10 Sprites
	{ "fg1.8j",						0x20000, 0x692afb67, 4 | BRF_GRA },           // 11
	{ "fg2.9j",						0x20000, 0xf1aba383, 4 | BRF_GRA },           // 12
	{ "fg3.10j",					0x20000, 0xd3475ff8, 4 | BRF_GRA },           // 13

	{ "pal20l8.9b",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 14 PLDs
	{ "pal16l8.1j",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 15
	{ "pal16l8.2j",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 16
	{ "pal16r4.2k",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17
	{ "pal16r4.14k",				0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 18
	{ "pal20.u15",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
};

STD_ROM_PICK(spyhunt2a)
STD_ROM_FN(spyhunt2a)

struct BurnDriver BurnDrvSpyhunt2a = {
	"spyhunt2a", "spyhunt2", NULL, NULL, "1987",
	"Spy Hunter II (rev 1)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, spyhunt2aRomInfo, spyhunt2aRomName, NULL, NULL, NULL, NULL, Spyhunt2InputInfo, Spyhunt2DIPInfo,
	Spyhunt2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	512, 480, 4, 3
};


// Blasted

static struct BurnRomInfo blastedRomDesc[] = {
	{ "3c",							0x10000, 0xb243b7df, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "3b",							0x10000, 0x627e30d3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2c",							0x10000, 0x026f30bf, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2b",							0x10000, 0x8e0e91a9, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "blasted.u7",					0x10000, 0x8d7c8ef6, 2 | BRF_PRG | BRF_ESS }, //  4 Sounds Good 68K Code
	{ "blasted.u17",				0x10000, 0xc79040b9, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "blasted.u8",					0x10000, 0xc53094c0, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "blasted.u18",				0x10000, 0x85688160, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "11d",						0x08000, 0xd8ed5cbc, 3 | BRF_GRA },           //  8 Background Tiles
	{ "12d",						0x08000, 0x60d00c69, 3 | BRF_GRA },           //  9

	{ "fg0",						0x20000, 0x5034ae8a, 4 | BRF_GRA },           // 10 Sprites
	{ "fg1",						0x20000, 0x4fbdba58, 4 | BRF_GRA },           // 11
	{ "fg2",						0x20000, 0x8891f6f8, 4 | BRF_GRA },           // 12
	{ "fg3",						0x20000, 0x18e4a130, 4 | BRF_GRA },           // 13

	{ "pal20l8.9b",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 14 PLDs
	{ "pal16l8.1j",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 15
	{ "pal16l8.2j",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 16
	{ "pal16r4.2k",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17
	{ "pal16r4.14k",				0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 18
	{ "pal20.u15",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
};

STD_ROM_PICK(blasted)
STD_ROM_FN(blasted)

static void blasted_control_write(UINT16)
{
	float cycles = (SekTotalCycles() * 40000.00) / 38619.00;

	SekClose();
	SekOpen(1);
	cycles -= SekTotalCycles();
	if (cycles >= 1) SekRun((UINT32)cycles);
	soundsgood_data_write((control_data >> 8) & 0x1f);
	SekClose();
	SekOpen(0);
}

static INT32 BlastedInit()
{
	control_write = blasted_control_write;

	vb_offset = -4;

	INT32 rc = Mcr68Init(0);

	if (!rc) {
		soundsgood_set_antipop_mask(0xfffe);
	}

	return rc;
}

struct BurnDriver BurnDrvBlasted = {
	"blasted", NULL, NULL, NULL, "1988",
	"Blasted\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, blastedRomInfo, blastedRomName, NULL, NULL, NULL, NULL, BlastedInputInfo, BlastedDIPInfo,
	BlastedInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	512, 480, 4, 3
};


// International Team Laser (prototype)

static struct BurnRomInfo intlaserRomDesc[] = {
	{ "3c.bin",						0x10000, 0xddab582a, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "3b.bin",						0x10000, 0xe4498eca, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2c.bin",						0x10000, 0xd2cca853, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2b.bin",						0x10000, 0x3802cfe2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "u7.bin",						0x10000, 0x19ad1e45, 2 | BRF_PRG | BRF_ESS }, //  4 Sounds Good 68K Code
	{ "u17.bin",					0x10000, 0xd6118949, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "u8.bin",						0x10000, 0xd6cc99aa, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "u18.bin",					0x10000, 0x3488a5cd, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "11d.bin",					0x08000, 0xce508d5f, 3 | BRF_GRA },           //  8 Background Tiles
	{ "12d.bin",					0x08000, 0xfbcb3391, 3 | BRF_GRA },           //  9

	{ "7j.bin",						0x20000, 0xac050bd7, 4 | BRF_GRA },           // 10 Sprites
	{ "8j.bin",						0x20000, 0xf10b12b3, 4 | BRF_GRA },           // 11
	{ "9j.bin",						0x20000, 0xa18f6911, 4 | BRF_GRA },           // 12
	{ "10j.bin",					0x20000, 0x203b55b8, 4 | BRF_GRA },           // 13

	{ "pal20l8.9b",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 14 PLDs
	{ "pal16l8.1j",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 15
	{ "pal16l8.2j",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 16
	{ "pal16r4.2k",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17
	{ "pal16r4.14k",				0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 18
	{ "pal20.u15",					0x00001, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
};

STD_ROM_PICK(intlaser)
STD_ROM_FN(intlaser)

struct BurnDriver BurnDrvIntlaser = {
	"intlaser", "blasted", NULL, NULL, "1987",
	"International Team Laser (prototype)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, intlaserRomInfo, intlaserRomName, NULL, NULL, NULL, NULL, IntlaserInputInfo, IntlaserDIPInfo,
	BlastedInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	512, 480, 4, 3
};


// Arch Rivals (rev 4.0 6/29/89)

static struct BurnRomInfo archrivlRomDesc[] = {
	{ "arch_rivals_3c_rev4.3c",		0x10000, 0x60d4b760, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "arch_rivals_3b_rev4.3b",		0x10000, 0xe0c07a8d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "arch_rivals_2c_rev4.2c",		0x10000, 0xcc2893f7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "arch_rivals_2b_rev4.2b",		0x10000, 0xfa977050, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "arch_rivals_u4_rev1.u4",		0x08000, 0x96b3c652, 4 | BRF_PRG | BRF_ESS }, //  4 CVSD M6809 Code
	{ "arch_rivals_u19_rev1.u19",	0x08000, 0xc4b3dc23, 4 | BRF_PRG | BRF_ESS }, //  5
	{ "arch_rivals_u20_rev1.u20",	0x08000, 0xf7907a02, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "arch_rivals_11d_rev1.11d",	0x10000, 0x7eb3d7c6, 3 | BRF_GRA },           //  7 Background Tiles
	{ "arch_rivals_12d_rev1.12d",	0x10000, 0x31e68050, 3 | BRF_GRA },           //  8

	{ "arch_rivals_7j_rev1.7j",		0x20000, 0x148ce28c, 4 | BRF_GRA },           //  9 Sprites
	{ "arch_rivals_8j_rev1.8j",		0x20000, 0x58187ac2, 4 | BRF_GRA },           // 10
	{ "arch_rivals_9j_rev1.9j",		0x20000, 0x0dd1204e, 4 | BRF_GRA },           // 11
	{ "arch_rivals_10j_rev1.10j",	0x20000, 0xeb3d0344, 4 | BRF_GRA },           // 12

	{ "pls153.11j",					0x000eb, 0x761c3b56, 0 | BRF_OPT },           // 13 PLDs
	{ "pls153.12j",					0x000eb, 0x48eed036, 0 | BRF_OPT },           // 14
	{ "pls153.14h",					0x000eb, 0xd4203273, 0 | BRF_OPT },           // 15
	{ "pal12h6.14e",				0x00034, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 16
	{ "pal16r4a.14k",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17
	{ "pal16r4a.2k",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 18
	{ "pal16r6a.15e",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
	{ "pal16l8a.1j",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 20
	{ "pal16l8a.2j",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 21
	{ "pal20l8a.9b",				0x00144, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 22
	{ "pl20x10a.14f",				0x000cc, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 23
	{ "pl20x10a.15f",				0x000cc, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 24
};

STD_ROM_PICK(archrivl)
STD_ROM_FN(archrivl)

static UINT16 __fastcall archrivl_read_port1_word(UINT32)
{
	return  (read_49way(Analog[3], 0) << 12) |
			(read_49way(Analog[2], 0) << 8) |
			(read_49way(Analog[1], 0) << 4) |
			(read_49way(Analog[0], 0) << 0);
}

static INT32 ArchrivlInit()
{
	sprite_clip = 16;
	control_write = archrivl_control_write;

	INT32 nRet = Mcr68Init(2);

	if (nRet == 0) {
		SekOpen(0);
		SekMapHandler(1,			0xe0000, 0xeffff, MAP_READ);
		SekSetReadWordHandler(1,	archrivl_read_port1_word);
		SekSetReadByteHandler(1,	common_read_port_byte);
		SekClose();
	}

	return nRet;
}

struct BurnDriver BurnDrvArchrivl = {
	"archrivl", NULL, NULL, NULL, "1989",
	"Arch Rivals (rev 4.0 6/29/89)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, archrivlRomInfo, archrivlRomName, NULL, NULL, NULL, NULL, ArchrivlInputInfo, ArchrivlDIPInfo,
	ArchrivlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	512, 480, 4, 3
};


// Arch Rivals (rev 2.0 5/03/89)

static struct BurnRomInfo archrivlaRomDesc[] = {
	{ "arch_rivals_3c_rev2.3c",		0x10000, 0x3c545740, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "arch_rivals_3b_rev2.3b",		0x10000, 0xbc4df2b9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "arch_rivals_2c_rev2.2c",		0x10000, 0xd6d08ff7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "arch_rivals_2b_rev2.2b",		0x10000, 0x92f3a43d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "arch_rivals_u4_rev1.u4",		0x08000, 0x96b3c652, 4 | BRF_PRG | BRF_ESS }, //  4 CVSD M6809 Code
	{ "arch_rivals_u19_rev1.u19",	0x08000, 0xc4b3dc23, 4 | BRF_PRG | BRF_ESS }, //  5
	{ "arch_rivals_u20_rev1.u20",	0x08000, 0xf7907a02, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "arch_rivals_11d_rev1.11d",	0x10000, 0x7eb3d7c6, 3 | BRF_GRA },           //  7 Background Tiles
	{ "arch_rivals_12d_rev1.12d",	0x10000, 0x31e68050, 3 | BRF_GRA },           //  8

	{ "arch_rivals_7j_rev1.7j",		0x20000, 0x148ce28c, 4 | BRF_GRA },           //  9 Sprites
	{ "arch_rivals_8j_rev1.8j",		0x20000, 0x58187ac2, 4 | BRF_GRA },           // 10
	{ "arch_rivals_9j_rev1.9j",		0x20000, 0x0dd1204e, 4 | BRF_GRA },           // 11
	{ "arch_rivals_10j_rev1.10j",	0x20000, 0xeb3d0344, 4 | BRF_GRA },           // 12

	{ "pls153.11j",					0x000eb, 0x761c3b56, 0 | BRF_OPT },           // 13 PLDs
	{ "pls153.12j",					0x000eb, 0x48eed036, 0 | BRF_OPT },           // 14
	{ "pls153.14h",					0x000eb, 0xd4203273, 0 | BRF_OPT },           // 15
	{ "pal12h6.14e",				0x00034, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 16
	{ "pal16r4a.14k",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17
	{ "pal16r4a.2k",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 18
	{ "pal16r6a.15e",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
	{ "pal16l8a.1j",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 20
	{ "pal16l8a.2j",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 21
	{ "pal20l8a.9b",				0x00144, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 22
	{ "pl20x10a.14f",				0x000cc, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 23
	{ "pl20x10a.15f",				0x000cc, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 24
};

STD_ROM_PICK(archrivla)
STD_ROM_FN(archrivla)

struct BurnDriver BurnDrvArchrivla = {
	"archrivla", "archrivl", NULL, NULL, "1989",
	"Arch Rivals (rev 2.0 5/03/89)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, archrivlaRomInfo, archrivlaRomName, NULL, NULL, NULL, NULL, ArchrivlInputInfo, ArchrivlDIPInfo,
	ArchrivlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	512, 480, 4, 3
};


// Arch Rivals (rev 2.0 5/03/89, 8-way Joystick bootleg)

static struct BurnRomInfo archrivlbRomDesc[] = {
	{ "4.bin",						0x10000, 0x1d99cce6, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "2.bin",						0x10000, 0x5d58a77b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",						0x10000, 0xd6d08ff7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1.bin",						0x10000, 0x92f3a43d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "13.bin",						0x08000, 0x96b3c652, 4 | BRF_PRG | BRF_ESS }, //  4 CVSD M6809 Code
	{ "12.bin",						0x08000, 0xc4b3dc23, 4 | BRF_PRG | BRF_ESS }, //  5
	{ "11.bin",						0x08000, 0xf7907a02, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "5.bin",						0x10000, 0x7eb3d7c6, 3 | BRF_GRA },           //  7 Background Tiles
	{ "6.bin",						0x10000, 0x31e68050, 3 | BRF_GRA },           //  8

	{ "7.bin",						0x20000, 0x148ce28c, 4 | BRF_GRA },           //  9 Sprites
	{ "8.bin",						0x20000, 0x58187ac2, 4 | BRF_GRA },           // 10
	{ "9.bin",						0x20000, 0x0dd1204e, 4 | BRF_GRA },           // 11
	{ "10.bin",						0x20000, 0xeb3d0344, 4 | BRF_GRA },           // 12

	{ "pls153.11j",					0x000eb, 0x761c3b56, 0 | BRF_OPT },           // 13 PLDs
	{ "pls153.12j",					0x000eb, 0x48eed036, 0 | BRF_OPT },           // 14
	{ "pls153.14h",					0x000eb, 0xd4203273, 0 | BRF_OPT },           // 15
	{ "pal12h6.14e",				0x00034, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 16
	{ "pal16r4a.14k",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17
	{ "pal16r4a.2k",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 18
	{ "pal16r6a.15e",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
	{ "pal16l8a.1j",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 20
	{ "pal16l8a.2j",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 21
	{ "pal20l8a.9b",				0x00144, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 22
	{ "pl20x10a.14f",				0x000cc, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 23
	{ "pl20x10a.15f",				0x000cc, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 24
};

STD_ROM_PICK(archrivlb)
STD_ROM_FN(archrivlb)

static INT32 ArchrivlbInit()
{
	sprite_clip = 16;
	control_write = archrivl_control_write;

	return Mcr68Init(2);
}

struct BurnDriver BurnDrvArchrivlb = {
	"archrivlb", "archrivl", NULL, NULL, "1989",
	"Arch Rivals (rev 2.0 5/03/89, 8-way Joystick bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, archrivlbRomInfo, archrivlbRomName, NULL, NULL, NULL, NULL, ArchrivlbInputInfo, ArchrivlbDIPInfo,
	ArchrivlbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	512, 480, 4, 3
};


// Pigskin 621AD (rev 1.1K 8/01/90)

static struct BurnRomInfo pigskinRomDesc[] = {
	{ "pigskin-k_a5_la1.a5",		0x10000, 0xab61c29b, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "pigskin-k_b5_la1.b5",		0x10000, 0x55a802aa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pigskin-k_a6_la1.a6",		0x10000, 0x4d8b7e50, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pigskin-k_b6_la1.b6",		0x10000, 0x1194f187, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pigskin_u4_sl1.u4",			0x10000, 0x6daf2d37, 4 | BRF_PRG | BRF_ESS }, //  4 CVSD M6809 Code
	{ "pigskin_u19_sl1.u19",		0x10000, 0x56fd16a3, 4 | BRF_PRG | BRF_ESS }, //  5
	{ "pigskin_u20_sl1.u20",		0x10000, 0x5d032fb8, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "pigskin_e2_la1.e2",			0x10000, 0x12d5737b, 3 | BRF_GRA },           //  7 Background Tiles
	{ "pigskin_e1_la1.e1",			0x10000, 0x460202a9, 3 | BRF_GRA },           //  8

	{ "pigskin_h15_la3.h15",		0x20000, 0x2655d03f, 4 | BRF_GRA },           //  9 Sprites
	{ "pigskin_h17_la3.h17",		0x20000, 0x31c52ea7, 4 | BRF_GRA },           // 10
	{ "pigskin_h18_la3.h18",		0x20000, 0xb36c4109, 4 | BRF_GRA },           // 11
	{ "pigskin_h14_la3.h14",		0x20000, 0x09c87104, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(pigskin)
STD_ROM_FN(pigskin)

struct BurnDriver BurnDrvPigskin = {
	"pigskin", NULL, NULL, NULL, "1990",
	"Pigskin 621AD (rev 1.1K 8/01/90)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pigskinRomInfo, pigskinRomName, NULL, NULL, NULL, NULL, PigskinInputInfo, PigskinDIPInfo,
	PigskinInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	512, 480, 4, 3
};


// Pigskin 621AD (rev 2.0 7/06/90)

static struct BurnRomInfo pigskinaRomDesc[] = {
	{ "pigskin_a5_la2.a5",			0x10000, 0xf75d36dd, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "pigskin_b5_la2.b5",			0x10000, 0xc5ffdfad, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pigskin_a6_la2.a6",			0x10000, 0x2fc91002, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pigskin_b6_la2.b6",			0x10000, 0x0b93dc66, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pigskin_u4_sl1.u4",			0x10000, 0x6daf2d37, 4 | BRF_PRG | BRF_ESS }, //  4 CVSD M6809 Code
	{ "pigskin_u19_sl1.u19",		0x10000, 0x56fd16a3, 4 | BRF_PRG | BRF_ESS }, //  5
	{ "pigskin_u20_sl1.u20",		0x10000, 0x5d032fb8, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "pigskin_e2_la1.e2",			0x10000, 0x12d5737b, 3 | BRF_GRA },           //  7 Background Tiles
	{ "pigskin_e1_la1.e1",			0x10000, 0x460202a9, 3 | BRF_GRA },           //  8

	{ "pigskin_h15_la1.h15",		0x20000, 0xe43d5d93, 4 | BRF_GRA },           //  9 Sprites
	{ "pigskin_h17_la1.h17",		0x20000, 0x6b780f1e, 4 | BRF_GRA },           // 10
	{ "pigskin_h18_la1.h18",		0x20000, 0x5e50f940, 4 | BRF_GRA },           // 11
	{ "pigskin_h14_la1.h14",		0x20000, 0xf26279f4, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(pigskina)
STD_ROM_FN(pigskina)

struct BurnDriver BurnDrvPigskina = {
	"pigskina", "pigskin", NULL, NULL, "1990",
	"Pigskin 621AD (rev 2.0 7/06/90)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pigskinaRomInfo, pigskinaRomName, NULL, NULL, NULL, NULL, PigskinInputInfo, PigskinDIPInfo,
	PigskinInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	512, 480, 4, 3
};


// Pigskin 621AD (rev 1.1 6/05/90)

static struct BurnRomInfo pigskinbRomDesc[] = {
	{ "pigskin_a5_la1.a5",			0x10000, 0x6c10028d, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "pigskin_b5_la1.b5",			0x10000, 0x2d03fbad, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pigskin_a6_la1.a6",			0x10000, 0x5fca2c4e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pigskin_b6_la1.b6",			0x10000, 0x778a75fc, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pigskin_u4_sl1.u4",			0x10000, 0x6daf2d37, 4 | BRF_PRG | BRF_ESS }, //  4 CVSD M6809 Code
	{ "pigskin_u19_sl1.u19",		0x10000, 0x56fd16a3, 4 | BRF_PRG | BRF_ESS }, //  5
	{ "pigskin_u20_sl1.u20",		0x10000, 0x5d032fb8, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "pigskin_e2_la1.e2",			0x10000, 0x12d5737b, 3 | BRF_GRA },           //  7 Background Tiles
	{ "pigskin_e1_la1.e1",			0x10000, 0x460202a9, 3 | BRF_GRA },           //  8

	{ "pigskin_h15_la1.h15",		0x20000, 0xe43d5d93, 4 | BRF_GRA },           //  9 Sprites
	{ "pigskin_h17_la1.h17",		0x20000, 0x6b780f1e, 4 | BRF_GRA },           // 10
	{ "pigskin_h18_la1.h18",		0x20000, 0x5e50f940, 4 | BRF_GRA },           // 11
	{ "pigskin_h14_la1.h14",		0x20000, 0xf26279f4, 4 | BRF_GRA },           // 12
};

STD_ROM_PICK(pigskinb)
STD_ROM_FN(pigskinb)

struct BurnDriver BurnDrvPigskinb = {
	"pigskinb", "pigskin", NULL, NULL, "1990",
	"Pigskin 621AD (rev 1.1 6/05/90)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pigskinbRomInfo, pigskinbRomName, NULL, NULL, NULL, NULL, PigskinInputInfo, PigskinDIPInfo,
	PigskinInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	512, 480, 4, 3
};


// Tri-Sports

static struct BurnRomInfo trisportRomDesc[] = {
	{ "tri_sports_a5_la3.a5",		0x10000, 0xfe1e9e37, 1 | BRF_PRG | BRF_ESS }, //  0 Main 68K Code
	{ "tri_sports_b5_la3.b5",		0x10000, 0xf352ec81, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tri_sports_a6_la3.a6",		0x10000, 0x9c6a1398, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tri_sports_b6_la3.b6",		0x10000, 0x597b564c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tri_sports_u4_sl1.u4",		0x10000, 0x0ed8c904, 4 | BRF_PRG | BRF_ESS }, //  4 CVSD M6809 Code
	{ "tri_sports_u19_sl1.u19",		0x10000, 0xb57d7d7e, 4 | BRF_PRG | BRF_ESS }, //  5
	{ "tri_sports_u20_sl1.u20",		0x08000, 0x3ae15c08, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "tri_sports_e2_la2.e2",		0x10000, 0xf61149a0, 3 | BRF_GRA },           //  7 Background Tiles
	{ "tri_sports_e1_la2.e1",		0x10000, 0xcf753497, 3 | BRF_GRA },           //  8

	{ "tri_sports_h15_la2.h15",		0x20000, 0x18a44d43, 4 | BRF_GRA },           //  9 Sprites
	{ "tri_sports_h17_la2.h17",		0x20000, 0x874cd237, 4 | BRF_GRA },           // 10
	{ "tri_sports_h18_la2.h18",		0x20000, 0xf7637a18, 4 | BRF_GRA },           // 11
	{ "tri_sports_h14_la2.h14",		0x20000, 0x403f9401, 4 | BRF_GRA },           // 12

	{ "pal20l8.g5",					0x00144, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 13 PLDs
	{ "pal20x10.f7",				0x000cc, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 14
	{ "pal20x10.e9",				0x000cc, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 15
	{ "pal20l8.d4",					0x00144, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 16
	{ "pal16l8.d6",					0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17
	{ "pal16r4a.c11",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 18
	{ "pal16r4.e10",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19
	{ "pls153a.f14",				0x000eb, 0x48eed036, 0 | BRF_OPT },           // 20
	{ "pls153a.f15",				0x000eb, 0x761c3b56, 0 | BRF_OPT },           // 21
	{ "pls153a.e19",				0x000eb, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 22
	{ "pal16l8.f8",					0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 23
	{ "pal16r6a.e11",				0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 24
};

STD_ROM_PICK(trisport)
STD_ROM_FN(trisport)

struct BurnDriver BurnDrvTrisport = {
	"trisport", NULL, NULL, NULL, "1989",
	"Tri-Sports\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, trisportRomInfo, trisportRomName, NULL, NULL, NULL, NULL, TrisportInputInfo, TrisportDIPInfo,
	TrisportInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	480, 512, 3, 4
};
