// FB Alpha Pre-Gx driver module
// based on MAME driver by R. Belmont, Phil Stroffolino, Acho A. Tang, Nicola Salmoria

/*
	known bugs:
		violent storm:
			background layer #2 on intro (bad guy on motorcycle), bottom clipped??

		metamorphic force
			level 1 boss "fire" circle around boss priority issue
		    background in lava level is too fast. (irq?)

		martial champ
		1: missing graphics in intro & title screens. On blank screens
		   disable layer#2 to see what it should look like.
		2: missing some sounds. (watch the first attract mode)

	unkown bugs.
		probably a lot! go ahead and fix it!

	to do:
		fix bugs
		clean up
		optimize
*/

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "konamiic.h"
#include "burn_ym2151.h"
#include "k054539.h"
#include "eeprom.h"

#if defined _MSC_VER
 #define _USE_MATH_DEFINES
 #include <cmath>
#endif

static UINT8 *AllMem;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROMExp0;
static UINT8 *DrvGfxROMExp1;
static UINT8 *DrvGfxROMExp2;
static UINT8 *DrvSndROM;
static UINT8 *DrvEeprom;
static UINT8 *AllRam;
static UINT8 *Drv68KRAM;
static UINT8 *DrvSpriteRam;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvK053936Ctrl;
static UINT8 *DrvK053936RAM;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static UINT16 *pMystwarrRozBitmap = NULL;

static UINT8 *soundlatch;
static UINT8 *soundlatch2;
static UINT8 *soundlatch3;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 layer_colorbase[4];
static INT32 sprite_colorbase = 0;
static INT32 sub1_colorbase;
static INT32 cbparam;
static INT32 oinprion;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvJoy5[16];
static UINT8 DrvReset;
static UINT16 DrvInputs[6];
static UINT8 DrvDips[2];

static INT32 sound_nmi_enable = 0;
static UINT8 sound_control = 0;
static UINT16 control_data = 0;
static UINT8 mw_irq_control = 0;
static INT32 z80_bank;

static INT32 nGame = 0;

static struct BurnInputInfo MystwarrInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 15,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 10,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 11,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 8,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 12,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 13,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 14,	"p2 fire 3"},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p3 coin"},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p3 start"},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy4 + 2,	"p3 up"},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy4 + 3,	"p3 down"},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy4 + 0,	"p3 left"},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p3 right"},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p3 fire 2"},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p3 fire 3"},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p4 coin"},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 15,	"p4 start"},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 10,	"p4 up"},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 11,	"p4 down"},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 8,	"p4 left"},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 9,	"p4 right"},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 12,	"p4 fire 1"},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 13,	"p4 fire 2"},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 14,	"p4 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Mystwarr)

static struct BurnInputInfo MetamrphInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 3"},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 15,	"p3 start"},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy3 + 10,	"p3 up"},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 11,	"p3 down"},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 8,	"p3 left"},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 9,	"p3 right"},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 12,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 13,	"p3 fire 2"},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 14,	"p3 fire 3"},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 15,	"p4 start"},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 10,	"p4 up"},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 11,	"p4 down"},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 8,	"p4 left"},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 9,	"p4 right"},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 12,	"p4 fire 1"},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 13,	"p4 fire 2"},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 14,	"p4 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"service"},
	{"Service 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Metamrph)

static struct BurnInputInfo ViostormInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 3"},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 15,	"p3 start"},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy3 + 10,	"p3 up"},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 11,	"p3 down"},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 8,	"p3 left"},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 9,	"p3 right"},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 12,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 13,	"p3 fire 2"},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 14,	"p3 fire 3"},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 15,	"p4 start"},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 10,	"p4 up"},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 11,	"p4 down"},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 8,	"p4 left"},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 9,	"p4 right"},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 12,	"p4 fire 1"},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 13,	"p4 fire 2"},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 14,	"p4 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"service"},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Viostorm)

static struct BurnInputInfo DadandrnInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p3 start"},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy4 + 2,	"p3 up"},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy4 + 3,	"p3 down"},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy4 + 0,	"p3 left"},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p3 right"},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p3 fire 2"},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p3 fire 3"},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy5 + 7,	"p4 start"},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy5 + 2,	"p4 up"},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy5 + 3,	"p4 down"},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy5 + 0,	"p4 left"},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy5 + 1,	"p4 right"},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy5 + 4,	"p4 fire 1"},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy5 + 5,	"p4 fire 2"},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy5 + 6,	"p4 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"service"},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Dadandrn)

static struct BurnInputInfo MartchmpInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 3"},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 15,	"p3 start"},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy3 + 10,	"p3 up"},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 11,	"p3 down"},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 8,	"p3 left"},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 9,	"p3 right"},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 12,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 13,	"p3 fire 2"},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 14,	"p3 fire 3"},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 15,	"p4 start"},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 10,	"p4 up"},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 11,	"p4 down"},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 8,	"p4 left"},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 9,	"p4 right"},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 12,	"p4 fire 1"},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 13,	"p4 fire 2"},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 14,	"p4 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"service"},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Martchmp)

static struct BurnDIPInfo MystwarrDIPList[]=
{
	{0x25, 0xff, 0xff, 0xe4, NULL			},
	{0x26, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x25, 0x01, 0x04, 0x04, "Off"			},
	{0x25, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Sound Output"		},
	{0x25, 0x01, 0x10, 0x10, "Mono"			},
	{0x25, 0x01, 0x10, 0x00, "Stereo"		},

	{0   , 0xfe, 0   ,    2, "Coin Mechanism"	},
	{0x25, 0x01, 0x20, 0x20, "Common"		},
	{0x25, 0x01, 0x20, 0x00, "Independent"		},

	{0   , 0xfe, 0   ,    2, "Number of Players"	},
	{0x25, 0x01, 0x40, 0x00, "4"			},
	{0x25, 0x01, 0x40, 0x40, "2"			},

	{0   , 0xfe, 0   ,    2, "Debug Alpha Mode (debug console/logfile)"		},
	{0x26, 0x01, 0x01, 0x00, "Off"			},
	{0x26, 0x01, 0x01, 0x01, "On"			},
};

STDDIPINFO(Mystwarr)

static struct BurnDIPInfo MetamrphDIPList[]=
{
	{0x25, 0xff, 0xff, 0xe8, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x25, 0x01, 0x08, 0x08, "Off"				},
	{0x25, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Sound Output"			},
	{0x25, 0x01, 0x10, 0x10, "Mono"				},
	{0x25, 0x01, 0x10, 0x00, "Stereo"			},

	{0   , 0xfe, 0   ,    2, "Coin Mechanism"		},
	{0x25, 0x01, 0x20, 0x20, "Common"			},
	{0x25, 0x01, 0x20, 0x00, "Independent"			},

	{0   , 0xfe, 0   ,    2, "Number of Players"		},
	{0x25, 0x01, 0x40, 0x00, "4"				},
	{0x25, 0x01, 0x40, 0x40, "2"				},

	{0   , 0xfe, 0   ,    2, "Continuous Energy Increment"	},
	{0x25, 0x01, 0x80, 0x80, "No"				},
	{0x25, 0x01, 0x80, 0x00, "Yes"				},
};

STDDIPINFO(Metamrph)


static struct BurnDIPInfo ViostormDIPList[]=
{
	{0x25, 0xff, 0xff, 0xe8, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x25, 0x01, 0x08, 0x08, "Off"			},
	{0x25, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Sound Output"		},
	{0x25, 0x01, 0x10, 0x10, "Mono"			},
	{0x25, 0x01, 0x10, 0x00, "Stereo"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x25, 0x01, 0x20, 0x20, "Off"			},
	{0x25, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin Mechanism"	},
	{0x25, 0x01, 0x40, 0x40, "Common"		},
	{0x25, 0x01, 0x40, 0x00, "Independent"		},

	{0   , 0xfe, 0   ,    2, "Number of Players"	},
	{0x25, 0x01, 0x80, 0x00, "3"			},
	{0x25, 0x01, 0x80, 0x80, "2"			},
};

STDDIPINFO(Viostorm)

static struct BurnDIPInfo DadandrnDIPList[]=
{
	{0x25, 0xff, 0xff, 0xe8, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x25, 0x01, 0x08, 0x08, "Off"			},
	{0x25, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Sound Output"		},
	{0x25, 0x01, 0x10, 0x10, "Mono"			},
	{0x25, 0x01, 0x10, 0x00, "Stereo"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x25, 0x01, 0x20, 0x20, "Off"			},
	{0x25, 0x01, 0x20, 0x00, "On"			},
};

STDDIPINFO(Dadandrn)

static struct BurnDIPInfo MartchmpDIPList[]=
{
	{0x25, 0xff, 0xff, 0xe4, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x25, 0x01, 0x04, 0x04, "Off"			},
	{0x25, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Sound Output"		},
	{0x25, 0x01, 0x10, 0x10, "Mono"			},
	{0x25, 0x01, 0x10, 0x00, "Stereo"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x25, 0x01, 0x20, 0x20, "Off"			},
	{0x25, 0x01, 0x20, 0x00, "On"			},
};

STDDIPINFO(Martchmp)

static void __fastcall mystwarr_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xff0000) == 0x400000) {
		if ((address & 0xf0) == 0)
			K053247WriteWord(((address & 0x000e) | ((address & 0xff00) >> 4)), data);

		*((UINT16*)(DrvSpriteRam + (address & 0xfffe))) = data;
		return;
	}

	if ((address & 0xffff00) == 0x480000) {
		K055555WordWrite(address, data >> 8);
		return;
	}

	if ((address & 0xfffff0) == 0x482010) {
		K053247WriteRegsWord(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0x484000) {
		K053246Write((address & 0x06) + 0, data >> 8);
		K053246Write((address & 0x06) + 1, data&0xff);
		return;
	}

	if ((address & 0xffffe0) == 0x48a000) {
		K054338WriteWord(address, data);
		return;
	}

	if ((address & 0xffffc0) == 0x48c000) {
		K056832WordWrite(address & 0x3e, data);
		return;
	}

	if ((address & 0xffff00) == 0x49c000) {
		// k053252
		return;
	}

	if ((address & 0xffc000) == 0x600000) {
		K056832RamWriteWord(address, data);
		return;
	}

	switch (address)
	{
		case 0x49e004: // rom read enable
			K056832WritebRegsWord(address & 0x0f, data);
		break;
	}
}

static void __fastcall mystwarr_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xff0000) == 0x400000) {
		if ((address & 0xf0) == 0)
			K053247Write(((address & 0x000e) | ((address & 0xff00) >> 4) | (address & 1))^1, data);

		DrvSpriteRam[(address & 0xffff)^1] = data;
		return;
	}

	if ((address & 0xffff00) == 0x480000) {
		K055555ByteWrite(address, data);
		return;
	}

	if ((address & 0xfffff0) == 0x482010) {
		K053247WriteRegsByte(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0x484000) {
		K053246Write((address & 0x7)^0, data);
		return;
	}

	if ((address & 0xffffe0) == 0x48a000) {
		K054338WriteByte(address, data);
		return;
	}

	if ((address & 0xffffc0) == 0x48c000) {
		K056832ByteWrite(address, data);
		return;
	}

	if ((address & 0xffff00) == 0x49c000) {
		// k053252
		return;
	}

	if ((address & 0xffc000) == 0x600000) {
		K056832RamWriteByte(address, data);
		return;
	}

	switch (address)
	{
		case 0x490000:
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
		return;

		case 0x492000:
		return;		// watchdog

		case 0x49800c:
		case 0x49800d:
			*soundlatch = data;
		return;

		case 0x49800e:
		case 0x49800f:
			*soundlatch2 = data;
		return;

		case 0x49a000:
		case 0x49a001:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x49e004:
		case 0x49e005: // rom read enable
			K056832WritebRegsByte(address & 0x0f, data);
		break;

		case 0x49e007:
			mw_irq_control = data; // correct!
		return;
	}
}

static UINT16 __fastcall mystwarr_main_read_word(UINT32 address)
{
	if ((address & 0xffc000) == 0x600000) {
		return K056832RamReadWord(address);
	}

	switch (address)
	{
		case 0x494000:
			return DrvInputs[2];

		case 0x494002:
			return DrvInputs[3];

		case 0x496000:
			return DrvInputs[0] & 0xff;

		case 0x496002:
			return (DrvInputs[1] & 0xf4) | 2 | (EEPROMRead() ? 0x01 : 0);
	}

	return 0;
}


static UINT8 __fastcall mystwarr_main_read_byte(UINT32 address)
{
	if ((address & 0xffc000) == 0x600000) {
		return K056832RamReadByte(address);
	}

	switch (address)
	{
		case 0x494000:
			return DrvInputs[2] >> 8;

		case 0x494001:
			return DrvInputs[2];

		case 0x494002:
			return DrvInputs[3] >> 8;

		case 0x494003:
			return DrvInputs[3];

		case 0x496000:
			return DrvInputs[0] >> 8;

		case 0x496001:
			return DrvInputs[0];

		case 0x496002:
			return DrvInputs[1] >> 8;

		case 0x496003:
			return ((DrvInputs[1]) & 0xf4) | 2 | (EEPROMRead() ? 0x01 : 0);

		case 0x498015:
			if ((*soundlatch3 & 0xf) == 0xe) return *soundlatch3 | 1;
			return *soundlatch3;
	}

	return 0;
}


//--------------------------------------------------------------------------------------------------------------


static UINT16 prot_data[0x20];

static UINT16 K055550_word_read(INT32 offset)
{
	return prot_data[(offset/2)&0x1f];
}

static void K055550_word_write(INT32 offset, UINT16 data, UINT16 mask)
{
	offset = (offset & 0x3e) / 2;

	UINT32 adr, bsize, count, i, lim;
	INT32 src, tgt, srcend, tgtend, skip, cx1, sx1, wx1, cy1, sy1, wy1, cz1, sz1, wz1, c2, s2, w2;
	INT32 dx, dy, angle;

	if (offset == 0 && (mask & 0x00ff))
	{
		if (mask == 0xffff) data >>= 8;

		switch (data)
		{
			case 0x97: // memset() (Dadandarn at 0x639dc)
			case 0x9f: // memset() (Violent Storm at 0x989c)
				adr   = (prot_data[7] << 16) | prot_data[8];
				bsize = (prot_data[10] << 16) | prot_data[11];
				count = (prot_data[0] & 0xff) + 1;

				lim = adr+bsize*count;

				for(i=adr; i<lim; i+=2)
					SekWriteWord(i, prot_data[0x1a/2]);
			break;

			// WARNING: The following cases are speculation based with questionable accuracy!(AAT)

			case 0x87: // unknown memory write (Violent Storm at 0x00b6ea)
				// Violent Storm writes the following data to the 55550 in mode 0x87.
				// All values are hardcoded and the write happens each frame during
				// gameplay. It refers to a 32x8-word list at 0x210e00 and seems to
				// be tied with another 13x128-byte table at 0x205080.
				// Both tables appear "check-only" and have little effect on gameplay.
				count =(prot_data[0] & 0xff) + 1;          // unknown ( byte 0x00)
				i     = prot_data[1];                      // unknown ( byte 0x1f)
				adr   = prot_data[7]<<16 | prot_data[8];   // address (dword 0x210e00)
				lim   = prot_data[9];                      // unknown ( word 0x0010)
				src   = prot_data[10]<<16 | prot_data[11]; // unknown (dword zero)
				tgt   = prot_data[12]<<16 | prot_data[13]; // unknown (dword zero)
			break;

			case 0xa0: // update collision detection table (Violent Storm at 0x018b42)
				count = prot_data[0] & 0xff;             // number of objects - 1
				skip  = prot_data[1]>>(8-1);             // words to skip in each entry to reach the "hit list"
				adr   = prot_data[2]<<16 | prot_data[3]; // where the table is located
				bsize = prot_data[5]<<16 | prot_data[6]; // object entry size in bytes

				srcend = adr + bsize * count;
				tgtend = srcend + bsize;

				// let's hope GCC will inline the mem24bew calls
				for (src=adr; src<srcend; src+=bsize)
				{
					cx1 = (INT16)SekReadWord(src);
					sx1 = (INT16)SekReadWord(src + 2);
					wx1 = (INT16)SekReadWord(src + 4);

					cy1 = (INT16)SekReadWord(src + 6);
					sy1 = (INT16)SekReadWord(src + 8);
					wy1 = (INT16)SekReadWord(src +10);

					cz1 = (INT16)SekReadWord(src +12);
					sz1 = (INT16)SekReadWord(src +14);
					wz1 = (INT16)SekReadWord(src +16);

					count = i = src + skip;
					tgt = src + bsize;

					for (; count<(UINT32)tgt; count++) SekWriteByte(count, 0);

					for (; tgt<tgtend; i++, tgt+=bsize)
					{
						c2 = (INT16)SekReadWord(tgt);
						s2 = (INT16)SekReadWord(tgt + 2);
						w2 = (INT16)SekReadWord(tgt + 4);
						if (abs((cx1+sx1)-(c2+s2))>=wx1+w2) continue; // X rejection

						c2 = (INT16)SekReadWord(tgt + 6);
						s2 = (INT16)SekReadWord(tgt + 8);
						w2 = (INT16)SekReadWord(tgt +10);
						if (abs((cy1+sy1)-(c2+s2))>=wy1+w2) continue; // Y rejection

						c2 = (INT16)SekReadWord(tgt +12);
						s2 = (INT16)SekReadWord(tgt +14);
						w2 = (INT16)SekReadWord(tgt +16);
						if (abs((cz1+sz1)-(c2+s2))>=wz1+w2) continue; // Z rejection

						SekWriteByte(i, 0x80); // collision confirmed
					}
				}
			break;

			case 0xc0: // calculate object "homes-in" vector (Violent Storm at 0x03da9e)
				dx = (INT16)prot_data[0xc];
				dy = (INT16)prot_data[0xd];

				// it's not necessary to use lookup tables because Violent Storm
				// only calls the service once per enemy per frame.
				if (dx)
				{
					if (dy)
					{
						angle = (INT32)((atan((double)dy / dx) * 128.0) / M_PI);
						if (dx < 0) angle += 128;
						i = (angle - 0x40) & 0xff;
					}
					else
						i = (dx > 0) ? 0xc0 : 0x40;
				}
				else
					if (dy > 0) i = 0;
				else
					if (dy < 0) i = 0x80;
				else
					i = BurnRandom() & 0xff; // vector direction indeterminate

				prot_data[0x10] = i;
			break;
		}
	}
}

static void __fastcall metamrph_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff000) == 0x210000) {
		K053247Write(address & 0xffe, data | (1<<16));
		return;
	}

	if ((address & 0xfffff8) == 0x240000) {
		K053246Write((address & 0x06) + 0, data >> 8);
		K053246Write((address & 0x06) + 1, data&0xff);
		return;
	}

	if ((address & 0xfffff0) == 0x244010) {
		K053247WriteRegsWord(address, data);
		return;
	}

	if ((address & 0xfffff0) == 0x250000) {
		K053250RegWrite(0, address, data);
		return;	
	}

	if ((address & 0xffffe0) == 0x254000) {
		K054338WriteWord(address, data);
		return;
	}

	if ((address & 0xffff00) == 0x258000) {
	//	bprintf (0, _T("WW %5.5x, %2.2x\n"), address, data);
		K055555WordWrite(address, data >> 8);
		return;
	}

	if ((address & 0xffffe0) == 0x260000) {
	//	bprintf (0, _T("k053252 word wo: %5.5x, %4.4x\n"), address, data);
		return;
	}

	if ((address & 0xfffff8) == 0x26c000) {
		// AM_RANGE(0x26c000, 0x26c007) AM_DEVWRITE("k056832", k056832_device,b_word_w)
		return;
	}

	if ((address & 0xffffc0) == 0x270000) {
		K056832WordWrite(address & 0x3e, data);
		return;
	}

	if ((address & 0xffc000) == 0x300000) {
		K056832RamWriteWord(address & 0x1fff, data);
		return;
	}

	if ((address & 0xffffc0) == 0x25c000) {
		prot_data[(address & 0x3f)/2] = data;
		K055550_word_write(address, data, 0xffff);
		return;
	}

	switch (address)
	{
		case 0x264000:
		case 0x264001:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x26800c:
		case 0x26800d:
			*soundlatch = data;
		return;

		case 0x26800e:
		case 0x26800f:
			*soundlatch2 = data;
		return;

		case 0x27c000:
		case 0x27c001:
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
		return;
	}
}

static void __fastcall metamrph_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff000) == 0x210000) {
		K053247Write((address & 0xfff) ^ 1, data);
		return;
	}

	if ((address & 0xfffff8) == 0x240000) {
		K053246Write((address & 0x07) ^ 0, data);
		return;
	}

	if ((address & 0xfffff0) == 0x244010) {
		K053247WriteRegsByte(address, data);
		return;
	}

	if ((address & 0xfffff0) == 0x250000) {
		K053250RegWrite(0, address, data);
		return;	
	}

	if ((address & 0xffffe0) == 0x254000) {
		K054338WriteByte(address, data);
		return;
	}

	if ((address & 0xffff00) == 0x258000) {
		K055555ByteWrite(address, data);
		return;
	}

	if ((address & 0xffffe0) == 0x260000) {
		//bprintf (0, _T("k053252 byte wo: %5.5x, %4.4x\n"), address, data);
		return;
	}

	if ((address & 0xfffff8) == 0x26c000) {
		// AM_RANGE(0x26c000, 0x26c007) AM_DEVWRITE("k056832", k056832_device,b_word_w)
		return;
	}

	if ((address & 0xffffc0) == 0x270000) {
		K056832ByteWrite(address & 0x3f, data);
		return;
	}

	if ((address & 0xffc000) == 0x300000) {
		K056832RamWriteByte(address & 0x1fff, data);
		return;
	}

	if ((address & 0xffffc0) == 0x25c000) {
		UINT8 *prot = (UINT8*)&prot_data;
		prot[(address & 0x3f) ^ 1] = data;
		K055550_word_write(address, data, 0xff << ((address & 1) * 8));
		return;
	}

	switch (address)
	{
		case 0x264000:
		case 0x264001:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x26800c:
		case 0x26800d:
			*soundlatch = data;
		return;

		case 0x26800e:
		case 0x26800f:
			*soundlatch2 = data;
		return;

		case 0x27c001:
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
		return;
	}
}

static UINT16 __fastcall metamrph_main_read_word(UINT32 address)
{
	if ((address & 0xfff000) == 0x210000) {
		return K053247Read((address & 0xffe)|1)*256+K053247Read((address & 0xffe)|0);
	}

	if ((address & 0xfffff0) == 0x250000) {
		return K053250RegRead(0, address);	
	}

	if ((address & 0xffffe0) == 0x260000) {
	//	bprintf (0, _T("k053252 word ro: %5.5x\n"), address);
		return 0;
	}

	if ((address & 0xffc000) == 0x300000) {
		return K056832RamReadWord(address & 0x1fff);
	}

	if ((address & 0xffe000) == 0x310000) {
		return 0;		// mw_rom_word_r
	}

	if ((address & 0xffe000) == 0x320000) {
		return K053250RomRead(0, address);
	}

	if ((address & 0xffffc0) == 0x25c000) {
		return K055550_word_read(address);
	}

	switch (address)
	{
		case 0x274000:
		case 0x274001:
			return DrvInputs[2];

		case 0x274002:
		case 0x274003:
			return DrvInputs[3];

		case 0x278000:
		case 0x278001:
			return DrvInputs[0];

		case 0x278002:
		case 0x278003:
			return (DrvInputs[1] & 0xfff8) | 2 | (EEPROMRead() ? 0x0001 : 0);
	}

	return 0;
}

static UINT8 __fastcall metamrph_main_read_byte(UINT32 address)
{
	if ((address & 0xfff000) == 0x210000) {
		return K053247Read((address & 0xfff)^1);
	}

	if ((address & 0xfffff0) == 0x250000) {
		return K053250RegRead(0, address) >> ((~address & 1) * 8);	
	}

	if ((address & 0xffffe0) == 0x260000) {
		bprintf (0, _T("k053252 word ro: %5.5x\n"), address);
		return 0;
	}

	if ((address & 0xffc000) == 0x300000) {
		return K056832RamReadByte(address & 0x1fff);
	}

	if ((address & 0xffe000) == 0x310000) {
		return 0;		// mw_rom_word_r
	}

	if ((address & 0xffe000) == 0x320000) {
		return K053250RomRead(0, address) >> ((~address & 1) * 8);
	}

	if ((address & 0xffffc0) == 0x25c000) {
		return K055550_word_read(address) >> ((~address & 1) * 8);
	}

	switch (address)
	{
		case 0x268014:
		case 0x268015:
			if ((*soundlatch3 & 0xf)==0xe) return *soundlatch3|1;
			return *soundlatch3;

		case 0x274000:
			return DrvInputs[2] >> 8;

		case 0x274001:
			return DrvInputs[2];

		case 0x274002:
			return DrvInputs[3] >> 8;

		case 0x274003:
			return DrvInputs[3];

		case 0x278000:
			return DrvInputs[0] >> 8;

		case 0x278001:
			return DrvInputs[0];

		case 0x278002:
			return DrvInputs[1] >> 8;

		case 0x278003:
			return (DrvInputs[1] & 0xfff8) | 2 | (EEPROMRead() ? 0x01 : 0);
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------

static void K053990_word_write(INT32 offset, UINT16 /*data*/, UINT16 mask)
{
	offset = (offset & 0x3e) / 2;

	INT32 src_addr, src_count, src_skip;
	INT32 dst_addr, /*dst_count,*/ dst_skip;
	INT32 mod_addr, mod_count, mod_skip, mod_offs;
	INT32 mode, i, element_size = 1;
	UINT16 mod_val, mod_data;

	if (offset == 0x0c && (mask & 0x00ff))
	{
		mode = ((prot_data[0x0d]<<8) & 0xff00) | (prot_data[0x0f] & 0xff);

		switch (mode)
		{
			case 0xffff: // word copy
				element_size = 2;
			case 0xff00: // byte copy
				src_addr  = prot_data[0x0];
				src_addr |= prot_data[0x1]<<16 & 0xff0000;
				dst_addr  = prot_data[0x2];
				dst_addr |= prot_data[0x3]<<16 & 0xff0000;
				src_count = prot_data[0x8]>>8;
				//dst_count = prot_data[0x9]>>8;
				src_skip  = prot_data[0xa] & 0xff;
				dst_skip  = prot_data[0xb] & 0xff;

				if ((prot_data[0x8] & 0xff) == 2) src_count <<= 1;
				src_skip += element_size;
				dst_skip += element_size;

				if (element_size == 1)
				for (i=src_count; i; i--)
				{
					SekWriteByte(dst_addr, SekReadByte(src_addr));
					src_addr += src_skip;
					dst_addr += dst_skip;
				}
				else for (i=src_count; i; i--)
				{
					SekWriteWord(dst_addr, SekReadWord(src_addr));
					src_addr += src_skip;
					dst_addr += dst_skip;
				}
			break;

			case 0x00ff: // sprite list modifier
				src_addr  = prot_data[0x0];
				src_addr |= prot_data[0x1]<<16 & 0xff0000;
				src_skip  = prot_data[0x1]>>8;
				dst_addr  = prot_data[0x2];
				dst_addr |= prot_data[0x3]<<16 & 0xff0000;
				dst_skip  = prot_data[0x3]>>8;
				mod_addr  = prot_data[0x4];
				mod_addr |= prot_data[0x5]<<16 & 0xff0000;
				mod_skip  = prot_data[0x5]>>8;
				mod_offs  = prot_data[0x8] & 0xff;
				mod_offs<<= 1;
				mod_count = 0x100;

				src_addr += mod_offs;
				dst_addr += mod_offs;

				for (i=mod_count; i; i--)
				{
					mod_val  = SekReadWord(mod_addr);
					mod_addr += mod_skip;

					mod_data = SekReadWord(src_addr);
					src_addr += src_skip;

					mod_data += mod_val;

					SekWriteWord(dst_addr, mod_data);
					dst_addr += dst_skip;
				}
			break;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------

static void __fastcall martchmp_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff00) == 0x400000) {
		K055555WordWrite(address, data >> 8);
		return;
	}

	if ((address & 0xfffff0) == 0x402010) {
		K053247WriteRegsWord(address&0xf, data);
		return;
	}

	if ((address & 0xfffff8) == 0x404000) {
		K053246Write((address & 0x06) + 0, data >> 8);
		K053246Write((address & 0x06) + 1, data&0xff);
		return;
	}

	if ((address & 0xffffe0) == 0x40a000) {
		K054338WriteWord(address, data);
		return;
	}

	if ((address & 0xffffc0) == 0x40c000) {
		K056832WordWrite(address & 0x3e, data);
		return;
	}

	if ((address & 0xffffc0) == 0x40e000) {
		prot_data[(address & 0x3e)/2] = data;
		K053990_word_write(address, data, 0xffff);
		return;
	}

	if ((address & 0xffffe0) == 0x41c000) {
		// k053252
		return;
	}

	if ((address & 0xfffff8) == 0x41e000) {
		// k056832 b_word_w
		return;
	}

	if ((address & 0xffc000) == 0x480000) {
		if ((address & 0x30) == 0)
			K053247WriteWord(((address & 0x000e) | ((address & 0x3FC0) >> 2)), data);

		*((UINT16*)(DrvSpriteRam + (address & 0x3ffe))) = data;
		return;
	}

	if ((address & 0xffc000) == 0x680000) {
		K056832RamWriteWord(address & 0x1fff, data);
		return;
	}
}

static void __fastcall martchmp_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffff00) == 0x400000) {
		K055555ByteWrite(address, data);
		return;
	}

	if ((address & 0xfffff0) == 0x402010) {
		K053247WriteRegsByte(address&0xf, data);
		return;
	}

	if ((address & 0xfffff8) == 0x404000) {
		K053246Write((address & 0x07) ^ 0, data);
		return;
	}

	if ((address & 0xffffe0) == 0x40a000) {
		K054338WriteByte(address, data);
		return;
	}

	if ((address & 0xffffc0) == 0x40c000) {
		K056832ByteWrite(address & 0x3f, data);
		return;
	}

	if ((address & 0xffffc0) == 0x40e000) {
		UINT8 *prot = (UINT8*)&prot_data;
		prot[(address & 0x3f) ^ 1] = data;
		K053990_word_write(address, data, 0xff << ((address & 1) * 8));
		return;
	}

	if ((address & 0xffffe0) == 0x41c000) {
		// k053252
		return;
	}

	if ((address & 0xfffff8) == 0x41e000) {
		// k056832 b_word_w
		return;
	}

	if ((address & 0xffc000) == 0x480000) {
		if ((address & 0x30) == 0)
			K053247Write(((address & 0x000e) | ((address & 0x3FC0) >> 2) | (address & 1))^1, data);

		DrvSpriteRam[(address & 0x3fff)^1] = data;
		return;
	}

	if ((address & 0xffc000) == 0x680000) {
		K056832RamWriteByte(address & 0x1fff, data);
		return;
	}

	switch (address)
	{
		case 0x410000:
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
		return;

		case 0x412000:
			mw_irq_control = data;
		return;

		case 0x412001:
			K053246_set_OBJCHA_line(data & 0x04);
		return;

		case 0x41800c:
		case 0x41800d:
			*soundlatch = data;
		return;

		case 0x41800e:
		case 0x41800f:
			*soundlatch2 = data;
		return;

		case 0x41a000:
		case 0x41a001:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

static UINT16 __fastcall martchmp_main_read_word(UINT32 address)
{
	if ((address & 0xffc000) == 0x680000) {
		return K056832RamReadWord(address);
	}

	switch (address)
	{
		case 0x414000:
			return DrvInputs[2];

		case 0x414002:
			return DrvInputs[3];

		case 0x416000:
			return DrvInputs[0] & 0xff;

		case 0x416002:
			return (DrvInputs[1] & 0xf4) | 2 | (EEPROMRead() ? 0x01 : 0);
	}

	return 0;
}

static UINT8 __fastcall martchmp_main_read_byte(UINT32 address)
{
	if ((address & 0xffc000) == 0x680000) {
		return K056832RamReadByte(address);
	}

	switch (address)
	{
		case 0x412000:
			return mw_irq_control;

		case 0x414000:
			return DrvInputs[2] >> 8;

		case 0x414001:
			return DrvInputs[2];

		case 0x414002:
			return DrvInputs[3] >> 8;

		case 0x414003:
			return DrvInputs[3];

		case 0x416000:
			return DrvInputs[0] >> 8;

		case 0x416001:
			return DrvInputs[0];

		case 0x416002:
			return DrvInputs[1] >> 8;

		case 0x416003:
			return ((DrvInputs[1]) & 0xf4) | 2 | (EEPROMRead() ? 0x01 : 0);

		case 0x418015:
			if ((*soundlatch3 & 0xf) == 0xe) return *soundlatch3 | 1;
			return *soundlatch3;
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------

static void __fastcall dadandrn_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xff0000) == 0x400000) {
		if ((address & 0xf0) == 0)
			K053247WriteWord(((address & 0x000e) | ((address & 0xff00) >> 4)), data);

		*((UINT16*)(DrvSpriteRam + (address & 0xfffe))) = data;
		return;
	}

	if ((address & 0xffc000) == 0x410000) {
		K056832RamWriteWord(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0x430000) {
		K053246Write((address & 0x06) + 0, data >> 8);
		K053246Write((address & 0x06) + 1, data&0xff);
		return;
	}

	if ((address & 0xfffff0) == 0x450010) {
		K053247WriteRegsWord(address, data);
		return;
	}

	if ((address & 0xffffc0) == 0x480000) {
		K056832WordWrite(address & 0x3e, data);
		return;
	}

	if ((address & 0xfffff8) == 0x482000) {
		// k056832 b regs
		return;
	}

	if ((address & 0xfffffe) == 0x484000) {
		INT32 sizetab[4] = { 4, 4, 2, 1 };
		INT32 clip[4] = { 0, 0, 0, 0 };

		clip[0] = (data >>  0) & 0x3f; 		// min (clip) x
		clip[1] = (data >>  6) & 0x3f; 		// min (clip) y
		clip[2] = sizetab[(data >> 12) & 0x03];	// size x
		clip[3] = sizetab[(data >> 14) & 0x03];	// size y

		clip[2] = ((clip[0] + clip[2]) << 7) - 1;
		clip[3] = ((clip[1] + clip[3]) << 7) - 1;
		clip[0] <<= 7;
		clip[1] <<= 7;

		K053936GP_set_cliprect(0, clip[0], clip[2], clip[1], clip[3]);
		return;
	}

	if ((address & 0xfffffe) == 0x484002) {
		K053936GP_clip_enable(0, (data >> 8) & 1);
		return;
	}

	if ((address & 0xffffe0) == 0x486000) {
		// k053252
		return;
	}

	if ((address & 0xffff00) == 0x488000) {
		K055555WordWrite(address, data >> 8);
		return;
	}

	if ((address & 0xffffe0) == 0x48c000) {
		K054338WriteWord(address, data);
		return;
	}

	if ((address & 0xffffc0) == 0x660000) {
		K054000Write((address/2)&0x1f, data);
		return;
	}

	if ((address & 0xffffc0) == 0x680000) {
		prot_data[(address & 0x3f)/2] = data;
		K055550_word_write(address, data, 0xffff);
		return;
	}
}

static void __fastcall dadandrn_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xff0000) == 0x400000) {
		if ((address & 0xf0) == 0)
			K053247Write(((address & 0x000e) | ((address & 0xff00) >> 4) | (address & 1))^1, data);

		DrvSpriteRam[(address & 0xffff)^1] = data;
		return;
	}

	if ((address & 0xffc000) == 0x410000) {
		K056832RamWriteByte(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0x430000) {
		K053246Write((address & 0x7)^0, data);
		return;
	}

	if ((address & 0xfffff0) == 0x450010) {
		K053247WriteRegsByte(address, data);
		return;
	}

	if ((address & 0xffffc0) == 0x480000) {
		K056832ByteWrite(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0x482000) {
		// k056832 b regs
		return;
	}

	//if ((address & 0xfffffc) == 0x484000) {
	//	bprintf (0, _T("ClipB: %2.2x, %4.4x\n"), address & 3, data);
	//	return;
	//}

	if ((address & 0xffffe0) == 0x486000) {
		// k053252
		return;
	}

	if ((address & 0xffff00) == 0x488000) {
		K055555ByteWrite(address, data);
		return;
	}

	if ((address & 0xffffe0) == 0x48c000) {
		K054338WriteByte(address, data);
		return;
	}

	if ((address & 0xffffc0) == 0x660000) {
		K054000Write((address/2)&0x1f, data);
		return;
	}

	if ((address & 0xffffc0) == 0x680000) {
		UINT8 *prot = (UINT8*)&prot_data;
		prot[(address & 0x3f) ^ 1] = data;
		K055550_word_write(address, data, 0xff << ((address & 1) * 8));
		return;
	}

	switch (address)
	{
		case 0x484002:
			//bprintf (0, _T("clipb enable %2.2x\n"), data);
			K053936GP_clip_enable(0, data & 0x01);
		return;

		case 0x48a00c:
			*soundlatch = data;
		return;

		case 0x48a00e:
			*soundlatch2 = data;
		return;

		case 0x6a0001:
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
		return;

		case 0x6c0000:
		case 0x6c0001:
			K053936GP_enable(0, data & 0x01);
			// 053936_enable
		//	bprintf (0, _T("'936b enable %2.2x\n"), data);
		return;

		case 0x6e0000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0xe00000:
		return;	// watchdog
	}
}

static UINT16 __fastcall dadandrn_main_read_word(UINT32 address)
{
	if ((address & 0xffc000) == 0x410000) {
		return K056832RamReadWord(address & 0x1fff);
	}

	if ((address & 0xffffc0) == 0x680000) {	
		return K055550_word_read(address);
	}

	if ((address & 0xffff00) == 0x660000) {
		return K054000Read((address / 2) & 0x1f);
	}

	switch (address)
	{
		case 0x480a14:
			if ((*soundlatch3 & 0xf)==0xe) return *soundlatch3|1;
			return *soundlatch3;

		case 0x48e000:
			return DrvInputs[0];

		case 0x48e020:
			return (DrvInputs[1] << 8) | (DrvInputs[2] & 0xff); // ????
	}

	return 0;
}

static UINT8 __fastcall dadandrn_main_read_byte(UINT32 address)
{
	if ((address & 0xffc000) == 0x410000) {
		return K056832RamReadByte(address & 0x1fff);
	}

	if ((address & 0xffffc0) == 0x680000) {
		return K055550_word_read(address) >> ((~address & 1) * 8);
	}

	if ((address & 0xffffc0) == 0x660000) {
		return K054000Read((address / 2) & 0x1f);
	}

	switch (address)
	{
		case 0x480a14:
		case 0x48a014:
			if ((*soundlatch3 & 0xf)==0xe) return *soundlatch3|1;
			return *soundlatch3;

		case 0x48e000:
			return DrvInputs[0] >> 8;

		case 0x48e001:
			return DrvInputs[0];

		case 0x48e020:
			return (DrvInputs[1] & 0xf8) | 2 | (EEPROMRead() ? 0x0001 : 0);

		case 0x48e021:
			return (DrvInputs[2] >> 8);
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------

static void bankswitch(INT32 data)
{
	z80_bank = data;
	ZetMapMemory(DrvZ80ROM + ((data & 0x0f) * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall mystwarr_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf000:
			*soundlatch3 = data;
		return;

		case 0xf800:
			sound_control = data & 0x10;
			bankswitch(data);
		//	if (!sound_control) ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);	// CLEAR NMI LINE!
		return;
	}

	if (address >= 0xe000 && address <= 0xe22f) {
		K054539Write(0, address - 0xe000, data);
	}

	if (address >= 0xe400 && address <= 0xe62f) {
		K054539Write(1, address - 0xe400, data);
	}

	if (address >= 0xe000 && address <= 0xe7ff) {
		DrvZ80RAM[0x2000 + (address & 0x7ff)] = data;
	}

}

static UINT8 __fastcall mystwarr_sound_read(UINT16 address)
{
	if (address >= 0xe000 && address <= 0xe22f) {
		return K054539Read(0, address - 0xe000);
	}

	if (address >= 0xe400 && address <= 0xe62f) {
		return K054539Read(1, address - 0xe400);
	}

	if (address >= 0xe000 && address <= 0xe7ff) {
		return DrvZ80RAM[0x2000 + (address & 0x7ff)];
	}

	switch (address)
	{
		case 0xf002:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch;

		case 0xf003:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch2;
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------
static INT32 superblend = 0;
static INT32 oldsuperblend = 0;
static INT32 superblendoff = 0;

static void mystwarr_tile_callback(INT32 layer, INT32 *code, INT32 *color, INT32 *flags)
{
	if (layer == 1) {
		/**/ if ((*code & 0xff00) + (*color) == 0x4101) superblend++; // water
		else if ((*code & 0xff00) + (*color) == 0xA30D) superblend++; // giant cargo plane
		else if ((*code & 0xff00) + (*color) == 0xA40D) superblend++; // giant cargo plane
		else if ((*code & 0xff00) + (*color) == 0xA50D) superblend++; // giant cargo plane
		else if ((*code & 0xff00) + (*color) == 0xFA01) superblend++; // intro "but behind the scenes..." part 1/x
		else if ((*code & 0xff00) + (*color) == 0xFA05) superblend++; // intro "but behind the scenes..." part 2
		else if ((*code & 0xff00) + (*color) == 0xFB01) superblend++; // part 3.
		else if ((*code & 0xff00) + (*color) == 0xFB05) superblend++; // part 4.
		else if ((*code & 0xff00) + (*color) == 0xFC05) superblend++; // part 5.
		else if ((*code & 0xff00) + (*color) == 0xD001) superblend++; // Title Screen
		else if ((*code & 0xff00) + (*color) == 0xC700) superblendoff++; // End Boss death scene (anti superblend)

		//if (counter) bprintf(0, _T("%X %X (%X), "), *code, *color, (*code & 0xff00) + (*color)); /* save this! -dink */
	}
	*color = layer_colorbase[layer] | ((*color >> 1) & 0x1e);
}

static void metamrph_tile_callback(INT32 layer, INT32 */*code*/, INT32 *color, INT32 */*flags*/)
{
	*color = layer_colorbase[layer] | (*color >> 2 & 0x0f);
}

static void game5bpp_tile_callback(INT32 layer, INT32 */*code*/, INT32 *color, INT32 */*flags*/)
{
	*color = layer_colorbase[layer] | ((*color >> 1) & 0x1e);
}
static void mystwarr_sprite_callback(INT32 */*code*/, INT32 *color, INT32 *priority)
{
	INT32 c = *color;
	*color = sprite_colorbase | (c & 0x001f);
	*priority = c & 0x00f0;
}

static void metamrph_sprite_callback(INT32 */*code*/, INT32 *color, INT32 *priority)
{
	INT32 c = *color;
	INT32 attr = c;

	c = (c & 0x1f) | sprite_colorbase;

	if ((attr & 0x300) != 0x300)
	{
		*color = c;
		*priority = (attr & 0xe0) >> 2;
	}
	else
	{
		*color = c | 3<<K055555_MIXSHIFT | K055555_SKIPSHADOW;
		*priority = 0x1c;
	}
}

static void martchmp_sprite_callback(INT32 */*code*/, INT32 *color, INT32 *priority)
{
	INT32 c = *color;

	if ((c & 0x3ff) == 0x11f)
		*color = K055555_FULLSHADOW;
	else
		*color = sprite_colorbase | (c & 0x1f);

	if (oinprion & 0xf0)
		*priority = cbparam;
	else
		*priority = c & 0xf0;
}

static void gaiapolis_sprite_callback(INT32 */*code*/, INT32 *color, INT32 *priority)
{
	INT32 c = *color;

	*color = sprite_colorbase | (c>>4 & 0x20) | (c & 0x001f);
	*priority = c & 0x00e0;
}

static const eeprom_interface mystwarr_eeprom_interface =
{
	7,			/* address bits */
	8,			/* data bits */
	"011000",		/*  read command */
	"011100",		/* write command */
	"0100100000000",/* erase command */
	"0100000000000",/* lock command */
	"0100110000000", /* unlock command */
	0,
	0
};

static const eeprom_interface gaiapolis_eeprom_interface =
{
	7,			/* address bits */
	8,			/* data bits */
	"011000",		/*  read command */
	"010100",		/* write command */
	"0100100000000",/* erase command */
	"0100000000000",/* lock command */
	"0100110000000",/* unlock command */
	0,
	0
};

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	bankswitch(2);
	ZetClose();

	KonamiICReset();

	K054539Reset(0);
	K054539Reset(1);

	EEPROMReset();

	if (EEPROMAvailable() == 0) {
		EEPROMFill(DrvEeprom, 0, 128);
	}

	control_data = 0;

	for (INT32 i = 0; i < 4; i++)
	{
		layer_colorbase[i] = 0;
	}

	sprite_colorbase = 0;
	cbparam = 0;
	oinprion = 0;
	sound_nmi_enable = 0;

	superblend = 0; // for mystwarr alpha tile count
	oldsuperblend = 0;
	superblendoff = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x300000;
	DrvZ80ROM		= Next; Next += 0x040000;

	DrvGfxROM0		= Next; Next += 0x600000;
	DrvGfxROM1		= Next; Next += 0xa00000;
	DrvGfxROM2		= Next; Next += 0x500000;
	DrvGfxROM3		= Next; Next += 0x100000;

	DrvGfxROMExp0		= Next; Next += 0xc00000;
	DrvGfxROMExp1		= Next; Next += 0x1000000;
	DrvGfxROMExp2		= Next; Next += 0x800000;

	DrvSndROM		= Next; Next += 0x400000;

	DrvEeprom		= Next; Next += 0x000080;

	konami_palette32	= (UINT32*)Next;
	DrvPalette		= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	DrvSpriteRam		= Next; Next += 0x010000;
	DrvPalRAM		= Next; Next += 0x002000;

	DrvK053936Ctrl		= Next; Next += 0x000400;
	DrvK053936RAM		= Next; Next += 0x001000;

	DrvZ80RAM		= Next; Next += 0x002800;

	soundlatch		= Next; Next += 0x000001;
	soundlatch2		= Next; Next += 0x000001;
	soundlatch3		= Next; Next += 0x000001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void decode_gfx1(UINT8 *src, UINT8 *d, INT32 len)
{
	UINT8 *s = src;
	UINT8 *pFinish = s+len-3;

	while (s < pFinish)
	{
		INT32 d0 = ((s[0]&0x80)<<0)|((s[0]&0x08)<<3)|((s[1]&0x80)>>2)|((s[1]&0x08)<<1)|((s[2]&0x80)>>4)|((s[2]&0x08)>>1)|((s[3]&0x80)>>6)|((s[3]&0x08)>>3);
		INT32 d1 = ((s[0]&0x40)<<1)|((s[0]&0x04)<<4)|((s[1]&0x40)>>1)|((s[1]&0x04)<<2)|((s[2]&0x40)>>3)|((s[2]&0x04)>>0)|((s[3]&0x40)>>5)|((s[3]&0x04)>>2);
		INT32 d2 = ((s[0]&0x20)<<2)|((s[0]&0x02)<<5)|((s[1]&0x20)<<0)|((s[1]&0x02)<<3)|((s[2]&0x20)>>2)|((s[2]&0x02)<<1)|((s[3]&0x20)>>4)|((s[3]&0x02)>>1);
		INT32 d3 = ((s[0]&0x10)<<3)|((s[0]&0x01)<<6)|((s[1]&0x10)<<1)|((s[1]&0x01)<<4)|((s[2]&0x10)>>1)|((s[2]&0x01)<<2)|((s[3]&0x10)>>3)|((s[3]&0x01)>>0);

		d[0] = d3;
		d[1] = d1;
		d[2] = d2;
		d[3] = d0;
		d[4] = s[4];

		s += 5;
		d += 5;
	}

	INT32 nLen = len;
	INT32 Plane[5] = { 32, 24, 8, 16, 0 };
	INT32 XOffs[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	INT32 YOffs[8] = { 0, 5*8, 5*8*2, 5*8*3, 5*8*4, 5*8*5, 5*8*6, 5*8*7 };

	UINT8 *tmp = (UINT8*)BurnMalloc(nLen);

	memcpy (tmp, DrvGfxROMExp0, nLen);

	GfxDecode(((nLen * 8) / 5) / 0x40, 5, 8, 8, Plane, XOffs, YOffs, 8*8*5, tmp, DrvGfxROMExp0);

	BurnFree (tmp);
}

static void DecodeSprites(UINT8 *rom, UINT8 *exprom, INT32 len)
{
	INT32 Plane[5] = { 32, 24, 16, 8, 0 };
	INT32 XOffs[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 40, 41, 42, 43, 44, 45, 46, 47 };
	INT32 YOffs[16] = { 0, 10*8, 10*8*2, 10*8*3, 10*8*4, 10*8*5, 10*8*6, 10*8*7, 10*8*8,
			10*8*9, 10*8*10, 10*8*11, 10*8*12, 10*8*13, 10*8*14, 10*8*15 };

	INT32 size4 = (len/(1024*1024))/5;
	size4 *= 4*1024*1024;

	UINT8 *tmp = (UINT8*)BurnMalloc(size4 * 5);

	UINT8 *d = tmp;
	UINT8 *s1 = rom;
	UINT8 *s2 = rom + (size4);

	for (INT32 i = 0; i < size4; i+=4)
	{
		*d++ = *s1++;
		*d++ = *s1++;
		*d++ = *s1++;
		*d++ = *s1++;
		*d++ = *s2++;
	}

	GfxDecode(size4 / 128, 5, 16, 16, Plane, XOffs, YOffs, 16*16*5, tmp, exprom);

	BurnFree (tmp);
}

static void Metamrph_sprite_decode()
{
	INT32 Plane[4] = { 24, 16, 8, 0 };
	INT32 XOffs[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 32, 33, 34, 35, 36, 37, 38, 39 };
	INT32 YOffs[16] = { 0, 64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960 };

	GfxDecode(0x10000, 4, 16, 16, Plane, XOffs, YOffs, 16*16*4, DrvGfxROM1, DrvGfxROMExp1);
}

static INT32 MystwarrInit()
{
	nGame = 1;

	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  5, 5, LD_GROUP(2) | LD_REVERSE)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  6, 5, LD_GROUP(2) | LD_REVERSE)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000004, 7, 5)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  8, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  9, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004, 10, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006, 11, 8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x400000, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x400001, 13, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 14, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x200000, 15, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 16, 1)) return 1;

		decode_gfx1(DrvGfxROM0, DrvGfxROMExp0, 0x500000);
		DecodeSprites(DrvGfxROM1, DrvGfxROMExp1, 0x500000);
	}

	K055555Init();
	K054338Init();

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x400000, mystwarr_tile_callback);
	K056832SetGlobalOffsets(24, 16);
	K056832SetLayerOffsets(0, -2-3, 0);
	K056832SetLayerOffsets(1,  0-3, 0);
	K056832SetLayerOffsets(2,  2-3, 0);
	K056832SetLayerOffsets(3,  3-3, 0);

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x7fffff, mystwarr_sprite_callback, 3);
	K053247SetSpriteOffset(-25-48, -15-24);
	K053247SetBpp(5);

	konamigx_mixer_init(0);
	konamigx_mystwarr_kludge = 1; // konamigx_mixer

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvSpriteRam,		0x400000, 0x40ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x700000, 0x701fff, MAP_RAM);
	SekSetWriteWordHandler(0,		mystwarr_main_write_word);
	SekSetWriteByteHandler(0,		mystwarr_main_write_byte);
	SekSetReadWordHandler(0,		mystwarr_main_read_word);
	SekSetReadByteHandler(0,		mystwarr_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(mystwarr_sound_write);
	ZetSetReadHandler(mystwarr_sound_read);
	ZetClose();

	EEPROMInit(&mystwarr_eeprom_interface);

	K054539Init(0, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	K054539_set_gain(0, 0, 0.80);
	K054539_set_gain(0, 1, 0.80);
	K054539_set_gain(0, 2, 0.80);
	K054539_set_gain(0, 3, 0.80);
	K054539_set_gain(0, 4, 2.00);
	K054539_set_gain(0, 5, 2.00);
	K054539_set_gain(0, 6, 2.00);
	K054539_set_gain(0, 7, 2.00);

	K054539Init(1, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	K054539_set_gain(1, 0, 0.50);
	K054539_set_gain(1, 1, 0.50);
	K054539_set_gain(1, 2, 0.50);
	K054539_set_gain(1, 3, 0.50);
	K054539_set_gain(1, 4, 0.50);
	K054539_set_gain(1, 5, 0.50);
	K054539_set_gain(1, 6, 0.50);
	K054539_set_gain(1, 7, 0.50);

	DrvDoReset();

	return 0;
}

static INT32 MetamrphInit()
{
	nGame = 2;

	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000001,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100000,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100001,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  5, 5, LD_GROUP(2) | LD_REVERSE)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  6, 5, LD_GROUP(2) | LD_REVERSE)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  7, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  8, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004,  9, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006, 10, 8, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 11, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 12, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x200000, 13, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 14, 1)) return 1;

		decode_gfx1(DrvGfxROM0, DrvGfxROMExp0, 0x500000);

		Metamrph_sprite_decode();
	}

	K055555Init();
	K054338Init();

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x200000, metamrph_tile_callback);
	K056832SetGlobalOffsets(24, 15);
	K056832SetLayerOffsets(0, -2+4, 2);
	K056832SetLayerOffsets(1,  0+4, 2);
	K056832SetLayerOffsets(2,  2+4, 2);
	K056832SetLayerOffsets(3,  3+4, 2);
	K056832Metamorphic_Fixup();

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x7fffff, metamrph_sprite_callback, 1);
	K053247SetSpriteOffset(-51-24, -24-15+0);

	K053250Init(0, DrvGfxROM2, DrvGfxROMExp2, 0x40000);
	K053250SetOffsets(0, -60+29, -16); // verify

	konamigx_mixer_init(0);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvSpriteRam,		0x211000, 0x21ffff, MAP_RAM);
	SekMapMemory((UINT8*)K053250Ram,	0x24c000, 0x24ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x330000, 0x331fff, MAP_RAM);
	SekSetWriteWordHandler(0,		metamrph_main_write_word);
	SekSetWriteByteHandler(0,		metamrph_main_write_byte);
	SekSetReadWordHandler(0,		metamrph_main_read_word);
	SekSetReadByteHandler(0,		metamrph_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(mystwarr_sound_write);
	ZetSetReadHandler(mystwarr_sound_read);
	ZetClose();

	EEPROMInit(&mystwarr_eeprom_interface);

	K054539Init(0, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	K054539_set_gain(0, 0, 0.80);
	K054539_set_gain(0, 1, 0.80);
	K054539_set_gain(0, 2, 0.80);
	K054539_set_gain(0, 3, 0.80);
	K054539_set_gain(0, 4, 1.80);
	K054539_set_gain(0, 5, 1.80);
	K054539_set_gain(0, 6, 1.80);
	K054539_set_gain(0, 7, 1.80);

	K054539Init(1, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	K054539_set_gain(1, 0, 0.80);
	K054539_set_gain(1, 1, 0.80);
	K054539_set_gain(1, 2, 0.80);
	K054539_set_gain(1, 3, 0.80);
	K054539_set_gain(1, 4, 0.80);
	K054539_set_gain(1, 5, 0.80);
	K054539_set_gain(1, 6, 0.80);
	K054539_set_gain(1, 7, 0.80);

	DrvDoReset();

	return 0;
}

static INT32 ViostormInit()
{
	nGame = 3;

	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000001,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  3, 5, LD_GROUP(2) | LD_REVERSE)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  4, 5, LD_GROUP(2) | LD_REVERSE)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  5, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  6, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004,  7, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006,  8, 8, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000,  9, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x200000, 10, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 11, 1)) return 1;

		decode_gfx1(DrvGfxROM0, DrvGfxROMExp0, 0x600000);

		Metamrph_sprite_decode();
	}

	K055555Init();
	K054338Init();

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x200000, metamrph_tile_callback);
	K056832SetGlobalOffsets(40, 16);
	K056832SetLayerOffsets(0, -2+1, 0);
	K056832SetLayerOffsets(1,  0+1, 0);
	K056832SetLayerOffsets(2,  2+1, 0);
	K056832SetLayerOffsets(3,  3+1, 0);

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x7fffff, metamrph_sprite_callback, 3);
	K053247SetSpriteOffset(-62-40, -23-16);

	K053250Init(0, DrvGfxROM2, DrvGfxROMExp2, 1); // doesn't exist on this hardware

	konamigx_mixer_init(0);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvSpriteRam,		0x211000, 0x21ffff, MAP_RAM);
	SekMapMemory((UINT8*)K053250Ram,	0x24c000, 0x24ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x330000, 0x331fff, MAP_RAM);
	SekSetWriteWordHandler(0,		metamrph_main_write_word);
	SekSetWriteByteHandler(0,		metamrph_main_write_byte);
	SekSetReadWordHandler(0,		metamrph_main_read_word);
	SekSetReadByteHandler(0,		metamrph_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(mystwarr_sound_write);
	ZetSetReadHandler(mystwarr_sound_read);
	ZetClose();

	EEPROMInit(&mystwarr_eeprom_interface);

	K054539Init(0, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	K054539_set_gain(0, 0, 2.00);
	K054539_set_gain(0, 1, 2.00);
	K054539_set_gain(0, 2, 2.00);
	K054539_set_gain(0, 3, 2.00);
	K054539_set_gain(0, 4, 2.00);
	K054539_set_gain(0, 5, 2.00);
	K054539_set_gain(0, 6, 2.00);
	K054539_set_gain(0, 7, 2.00);

	K054539Init(1, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();

	return 0;
}

static INT32 MartchmpInit()
{
	nGame = 4;

	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  5, 5, LD_GROUP(2) | LD_REVERSE)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  6, 5, LD_GROUP(2) | LD_REVERSE)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000004, 7, 5)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  8, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  9, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004, 10, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006, 11, 8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x800000, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x800001, 13, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 14, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x200000, 15, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 16, 1)) return 1;

		decode_gfx1(DrvGfxROM0, DrvGfxROMExp0, 0x500000);

		DecodeSprites(DrvGfxROM1, DrvGfxROMExp1, 0xa00000);
	}

	K055555Init();
	K054338Init();

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x400000, game5bpp_tile_callback);
	K056832SetGlobalOffsets(32, 16);
	K056832SetLayerOffsets(0, -2-4, 0);
	K056832SetLayerOffsets(1,  0-4, 0);
	K056832SetLayerOffsets(2,  2-4, 0);
	K056832SetLayerOffsets(3,  3-4, 0);

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x7fffff, martchmp_sprite_callback, 3);
	K053247SetSpriteOffset((-23-58-9), (-16-23-14));
	K053247SetBpp(5);

	konamigx_mixer_init(0);
	K054338_invert_alpha(0);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM + 0x000000,	0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x100000, 0x10ffff, MAP_RAM);
	SekMapMemory(Drv68KROM + 0x100000,	0x300000, 0x3fffff, MAP_ROM);
	SekMapMemory(DrvSpriteRam,		0x480000, 0x483fff, MAP_ROM);	// should be written through handler
	SekMapMemory(DrvPalRAM,			0x600000, 0x601fff, MAP_RAM);
	SekSetWriteWordHandler(0,		martchmp_main_write_word);
	SekSetWriteByteHandler(0,		martchmp_main_write_byte);
	SekSetReadWordHandler(0,		martchmp_main_read_word);
	SekSetReadByteHandler(0,		martchmp_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(mystwarr_sound_write);
	ZetSetReadHandler(mystwarr_sound_read);
	ZetClose();

	EEPROMInit(&mystwarr_eeprom_interface);

	K054539Init(0, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	K054539_set_gain(0, 0, 1.40);
	K054539_set_gain(0, 1, 1.40);
	K054539_set_gain(0, 2, 1.40);
	K054539_set_gain(0, 3, 1.40);
	K054539_set_gain(0, 4, 1.40);
	K054539_set_gain(0, 5, 1.40);
	K054539_set_gain(0, 6, 1.40);
	K054539_set_gain(0, 7, 1.40);

	K054539Init(1, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();

	return 0;
}

static void DrvGfxExpand(UINT8 *src, INT32 nLen)
{
	for (INT32 i = (nLen - 1) * 2; i >= 0; i-=2) {
		src[i+0] = src[(i/2)] >> 4;
		src[i+1] = src[(i/2)] & 0xf;
	}
}

// pre-draw the whole roz tilemap... needs a ton of ram!
static void GaiapolisRozTilemapdraw()
{
	UINT8 *dat1 = DrvGfxROM3;
	UINT8 *dat2 = DrvGfxROM3 + 0x20000;
	UINT8 *dat3 = DrvGfxROM3 + 0x60000;

	K053936_external_bitmap = pMystwarrRozBitmap;

	for (INT32 offs = 0; offs < 512 * 512; offs++)
	{
		INT32 sx = (offs & 0x1ff) * 16;
		INT32 sy = (offs / 0x200) * 16;

		INT32 code = dat3[offs] | ((dat2[offs]&0x3f)<<8);
		INT32 color = 0;

		if (offs & 1)
			color = dat1[offs>>1]&0xf;
		else
			color = dat1[offs>>1]>>4;

		if (dat2[offs] & 0x80) color |= 0x10;

		UINT8 *gfx = DrvGfxROM2 + (code * 0x100);
		color <<= 4;

		UINT16 *dst = pMystwarrRozBitmap + sy * 0x2000 + sx;

		for (INT32 y = 0; y < 16; y++) {
			for (INT32 x = 0; x < 16; x++, gfx++) {
				dst[x] = gfx[0] + color;
			}
			dst += 0x2000;
		}
	}
}

static INT32 GaiapolisInit()
{
	nGame = 5;

	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x200001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x200000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  5, 5, LD_GROUP(2) | LD_REVERSE)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  6, 5, LD_GROUP(2) | LD_REVERSE)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  7, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  8, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004,  9, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006, 10, 8, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x020000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x060000, 16, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 17, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x200000, 18, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 19, 1)) return 1;

		decode_gfx1(DrvGfxROM0, DrvGfxROMExp0, 0x500000);

		Metamrph_sprite_decode();
	}

	K055555Init();
	K054338Init();

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x400000, metamrph_tile_callback);
	K056832SetGlobalOffsets(32, 16);
	K056832SetLayerOffsets(0, -2, 0);
	K056832SetLayerOffsets(1,  0, 0);
	K056832SetLayerOffsets(2,  2, 0);
	K056832SetLayerOffsets(3,  2, 0);

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x7fffff, gaiapolis_sprite_callback, 1);
	K053247SetSpriteOffset(7+(-24-79), -16-24);

	konamigx_mixer_init(0);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x2fffff, MAP_ROM);
	SekMapMemory(DrvSpriteRam,		0x400000, 0x40ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x420000, 0x421fff, MAP_RAM);
	SekMapMemory(DrvK053936Ctrl,		0x460000, 0x46001f, MAP_RAM);
	SekMapMemory(DrvK053936RAM,		0x470000, 0x470fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0x600000, 0x60ffff, MAP_RAM);
	SekSetWriteWordHandler(0,		dadandrn_main_write_word);
	SekSetWriteByteHandler(0,		dadandrn_main_write_byte);
	SekSetReadWordHandler(0,		dadandrn_main_read_word);
	SekSetReadByteHandler(0,		dadandrn_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(mystwarr_sound_write);
	ZetSetReadHandler(mystwarr_sound_read);
	ZetClose();

	EEPROMInit(&gaiapolis_eeprom_interface);

	{
		DrvGfxExpand(DrvGfxROM2	, 0x180000);
		pMystwarrRozBitmap = (UINT16*)BurnMalloc(((512 * 16) * 2) * (512 * 16) * 2);
		GaiapolisRozTilemapdraw();

		m_k053936_0_ctrl = (UINT16*)DrvK053936Ctrl;
		m_k053936_0_linectrl = (UINT16*)DrvK053936RAM;
		K053936GP_set_offset(0, -44, -17);
	}

	K054539Init(0, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	K054539_set_gain(0, 0, 0.80);
	K054539_set_gain(0, 1, 0.80);
	K054539_set_gain(0, 2, 0.80);
	K054539_set_gain(0, 3, 0.80);
	K054539_set_gain(0, 4, 2.00);
	K054539_set_gain(0, 5, 2.00);
	K054539_set_gain(0, 6, 2.00);
	K054539_set_gain(0, 7, 2.00);

	K054539Init(1, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	K054539_set_gain(1, 0, 0.50);
	K054539_set_gain(1, 1, 0.50);
	K054539_set_gain(1, 2, 0.50);
	K054539_set_gain(1, 3, 0.50);
	K054539_set_gain(1, 4, 0.50);
	K054539_set_gain(1, 5, 0.50);
	K054539_set_gain(1, 6, 0.50);
	K054539_set_gain(1, 7, 0.50);

	DrvDoReset();

	return 0;
}

// pre-draw the whole roz tilemap... needs a ton of ram!
static void DadandrnRozTilemapdraw()
{
	UINT8 *dat1 = DrvGfxROM3;
	UINT8 *dat2 = DrvGfxROM3 + 0x40000;

	K053936_external_bitmap = pMystwarrRozBitmap;

	for (INT32 offs = 0; offs < 512 * 512; offs++)
	{
		INT32 sx = (offs & 0x1ff) * 16;
		INT32 sy = (offs / 0x200) * 16;

		INT32 code = dat2[offs] | ((dat1[offs]&0x1f)<<8);
		INT32 flipx = (dat1[offs] & 0x40) ? 0x0f : 0;

		UINT8 *gfx = DrvGfxROM2 + (code * 0x100);

		UINT16 *dst = pMystwarrRozBitmap + sy * 0x2000 + sx;

		for (INT32 y = 0; y < 16; y++) {
			for (INT32 x = 0; x < 16; x++, gfx++) {
				dst[x^flipx] = gfx[0];
			}
			dst += 0x2000;
		}
	}
}

static INT32 DadandrnInit()
{
	nGame = 6;

	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  5, 5, LD_GROUP(2) | LD_REVERSE)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  6, 5, LD_GROUP(2) | LD_REVERSE)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000004, 7, 5)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  8, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  9, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004, 10, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006, 11, 8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x800000, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x800001, 13, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100000, 16, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x040000, 18, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 19, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x200000, 20, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 21, 1)) return 1;

		decode_gfx1(DrvGfxROM0, DrvGfxROMExp0, 0x500000);

		DecodeSprites(DrvGfxROM1, DrvGfxROMExp1, 0xa00000);
	}

	K055555Init();
	K054338Init();

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x400000, game5bpp_tile_callback);
	K056832SetGlobalOffsets(24, 17);
	K056832SetLayerOffsets(0, -2+4, 0);
	K056832SetLayerOffsets(1,  0+4, 0);
	K056832SetLayerOffsets(2,  2+4, 0);
	K056832SetLayerOffsets(3,  3+4, 0);

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x7fffff, gaiapolis_sprite_callback, 1);
	K053247SetSpriteOffset(-24-42, -17-22);
	K053247SetBpp(5);

	konamigx_mixer_init(0);
	konamigx_mixer_primode(1);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(DrvSpriteRam,		0x400000, 0x40ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x420000, 0x421fff, MAP_RAM);
	SekMapMemory(DrvK053936Ctrl,		0x460000, 0x46001f, MAP_RAM);
	SekMapMemory(DrvK053936RAM,		0x470000, 0x470fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0x600000, 0x60ffff, MAP_RAM);
	SekSetWriteWordHandler(0,		dadandrn_main_write_word);
	SekSetWriteByteHandler(0,		dadandrn_main_write_byte);
	SekSetReadWordHandler(0,		dadandrn_main_read_word);
	SekSetReadByteHandler(0,		dadandrn_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(mystwarr_sound_write);
	ZetSetReadHandler(mystwarr_sound_read);
	ZetClose();

	EEPROMInit(&mystwarr_eeprom_interface);

	{
		pMystwarrRozBitmap = (UINT16*)BurnMalloc(((512 * 16) * 2) * (512 * 16) * 2);
		DadandrnRozTilemapdraw();

		m_k053936_0_ctrl = (UINT16*)DrvK053936Ctrl;
		m_k053936_0_linectrl = (UINT16*)DrvK053936RAM;
		K053936GP_set_offset(0, -24-8, -17);
	}

	K054539Init(0, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	K054539_set_gain(0, 0, 1.00);
	K054539_set_gain(0, 1, 1.00);
	K054539_set_gain(0, 2, 1.00);
	K054539_set_gain(0, 3, 1.00);
	K054539_set_gain(0, 4, 2.00);
	K054539_set_gain(0, 5, 2.00);
	K054539_set_gain(0, 6, 2.00);
	K054539_set_gain(0, 7, 2.00);

	K054539Init(1, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	K054539_set_gain(1, 0, 1.00);
	K054539_set_gain(1, 1, 1.00);
	K054539_set_gain(1, 2, 1.00);
	K054539_set_gain(1, 3, 1.00);
	K054539_set_gain(1, 4, 1.00);
	K054539_set_gain(1, 5, 1.00);
	K054539_set_gain(1, 6, 1.00);
	K054539_set_gain(1, 7, 1.00);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	KonamiICExit();

	konamigx_mixer_exit();

	SekExit();
	ZetExit();

	EEPROMExit();

	K054539Exit();

	BurnFree (AllMem);
	if (pMystwarrRozBitmap) {
		BurnFree (pMystwarrRozBitmap);
		pMystwarrRozBitmap = NULL;
	}
	return 0;
}

static void DrvPaletteRecalc()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x2000/2; i+=2)
	{
		INT32 r = pal[i+0] & 0xff;
		INT32 g = pal[i+1] >> 8;
		INT32 b = pal[i+1] & 0xff;

		DrvPalette[i/2] = (r << 16) + (g << 8) + b;
	}
}

static INT32 DrvDraw()
{
	DrvPaletteRecalc();

	KonamiClearBitmaps(0);

	for (INT32 i = 0; i < 4; i++) {
		layer_colorbase[i] = K055555GetPaletteIndex(i)<<4;
	}

	INT32 blendmode = 0, enable_sub = 0;

	if (nGame == 1) { // mystwarr
		blendmode = 0;
		cbparam = 0; // ?

		{ // "Superblend and the Survival of Alpha (in Mystwarr)"
			switch (Drv68KRAM[0x2335]) {
				case 0x0A:
				case 0x11:
				case 0x18: { // alpha on for sure, except endboss death scene (see: superblendoff)
					superblend = 0xfff;
					break;
				}

				case 0x09:
				case 0x10:
				case 0x12:
			    default: { // alpha off, but only if tilecount isn't rising
					if (superblend < oldsuperblend) {
						superblend = 0;
					}
					break;
				}
			}

			if ((superblend || oldsuperblend) && !superblendoff) {
				blendmode = (1 << 16 | GXMIX_BLEND_FORCE) << 2; // using "|| oldsuperblend" for 1 frame latency, to avoid flickers on the water level when he gets "flushed" into the boss part
			}

			if (DrvDips[1] & 1) // debug alpha
				bprintf(0, _T("%X %X (%X), "), superblend, oldsuperblend, Drv68KRAM[0x2335]);

			oldsuperblend = superblend;
			if (superblend) superblend = 1;

			superblendoff = 0; // frame-based.
		}

		sprite_colorbase = K055555GetPaletteIndex(4)<<5;
		konamigx_mixer(enable_sub, 0, 0, 0, blendmode, 0, 0);
		KonamiBlendCopy(DrvPalette);

		return 0;
	}

	if (nGame == 2 || nGame == 3) { // viostorm / metamrph
		blendmode = GXSUB_K053250 | GXSUB_4BPP;
		sprite_colorbase = K055555GetPaletteIndex(4)<<4;
	}

	if (nGame == 4) { // mtlchamp
		cbparam = K055555ReadRegister(K55_PRIINP_8);
		oinprion = K055555ReadRegister(K55_OINPRI_ON);

		blendmode = (oinprion==0xef && K054338_read_register(K338_REG_PBLEND)) ? ((1<<16|GXMIX_BLEND_FORCE)<<2) : 0;
		sprite_colorbase = K055555GetPaletteIndex(4)<<5;
	}

	if (nGame == 5) // gaiapolis
	{
		sprite_colorbase = (K055555GetPaletteIndex(4)<<4)&0x7f;
		sub1_colorbase = (K055555GetPaletteIndex(5)<<8)&0x700;
		blendmode = GXSUB_4BPP;
		K053936GP_set_colorbase(0, sub1_colorbase);
		enable_sub = 1;
	}

	if (nGame == 6) // dadandrn
	{
		sprite_colorbase = (K055555GetPaletteIndex(4)<<3)&0x7f;
		sub1_colorbase = (K055555GetPaletteIndex(5)<<8)&0x700;
		blendmode = GXSUB_8BPP;
		K053936GP_set_colorbase(0, sub1_colorbase);
		enable_sub = 1;
	}

	konamigx_mixer(enable_sub, blendmode, 0, 0, 0, 0, 0);

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 5 * sizeof(INT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}

		DrvInputs[1] = DrvDips[0] | (DrvInputs[1] & 0xff00) | 2;
	}

	SekNewFrame();
	ZetNewFrame();

	INT32 nInterleave = 60;
	INT32 nCyclesTotal[2] = { 16000000 / 60, 8000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nNext, nCyclesSegment;

		if (nGame == 1)
		{
			if (mw_irq_control & 1)
			{
				if (i == 0)
					SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

				if (i == ((nInterleave * (240+10))/256)) // +10 otherwise flickers on char.selection screen (mystwarr)
					SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
			}
		}

		if (nGame == 2 || nGame == 3)
		{
			if (i == 0) // otherwise service mode doesn't work!
				SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

			if (i == ((nInterleave *  24) / 256))
				SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);

			if (i == ((nInterleave * 248) / 256) && K053246_is_IRQ_enabled())
				SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		}

		if (nGame == 4) // martchmp
		{
			if (mw_irq_control & 2)
			{
				if (i == ((nInterleave *  23) / 256))
					SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);

				if (i == ((nInterleave *  47) / 256) && K053246_is_IRQ_enabled())
					SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
			}
		}

		if (nGame == 5 || nGame == 6)
		{
			if (i == (nInterleave - 1))
				SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		}

		nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[0];
		nCyclesSegment = SekRun(nCyclesSegment);
		nCyclesDone[0] += nCyclesSegment;


		nNext = (i + 1) * nCyclesTotal[1] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[1];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[1] += nCyclesSegment;

		if ((i % (nInterleave / 8)) == ((nInterleave / 8) - 1)) {// && sound_nmi_enable && sound_control) { // iq_132
			ZetNmi();
		}
	}

	if (pBurnSoundOut) {
		memset (pBurnSoundOut, 0, nBurnSoundLen * 2 * 2);
		K054539Update(0, pBurnSoundOut, nBurnSoundLen);
		K054539Update(1, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029732;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		ZetScan(nAction);

		K054539Scan(nAction);
		KonamiICScan(nAction);

		SCAN_VAR(sound_nmi_enable);
		SCAN_VAR(sound_control);
		SCAN_VAR(control_data);
		SCAN_VAR(mw_irq_control);
		SCAN_VAR(prot_data);
		SCAN_VAR(layer_colorbase);
		SCAN_VAR(sprite_colorbase);
		SCAN_VAR(sub1_colorbase);
		SCAN_VAR(cbparam);
		SCAN_VAR(oinprion);
		SCAN_VAR(z80_bank);
		SCAN_VAR(superblend);
		SCAN_VAR(oldsuperblend);
		SCAN_VAR(superblendoff);

		BurnRandomScan(nAction);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(z80_bank);
		ZetClose();
	}

	EEPROMScan(nAction, pnMin);

	return 0;
}


// Mystic Warriors (ver EAA)

static struct BurnRomInfo mystwarrRomDesc[] = {
	{ "128eaa01.20f",	0x040000, 0x508f249c, 1 }, //  0 maincpu
	{ "128eaa02.20g",	0x040000, 0xf8ffa352, 1 }, //  1
	{ "128a03.19f",		0x080000, 0xe98094f3, 1 }, //  2
	{ "128a04.19g",		0x080000, 0x88c6a3e4, 1 }, //  3

	{ "128a05.6b",		0x020000, 0x0e5194e0, 2 }, //  4 soundcpu

	{ "128a08.1h",		0x100000, 0x63d6cfa0, 3 }, //  5 gfx1
	{ "128a09.1k",		0x100000, 0x573a7725, 3 }, //  6
	{ "128a10.3h",		0x080000, 0x558e545a, 3 }, //  7

	{ "128a16.22k",		0x100000, 0x459b6407, 4 }, //  8 gfx2
	{ "128a15.20k",		0x100000, 0x6bbfedf4, 4 }, //  9
	{ "128a14.19k",		0x100000, 0xf7bd89dd, 4 }, // 10
	{ "128a13.17k",		0x100000, 0xe89b66a2, 4 }, // 11
	{ "128a12.12k",		0x080000, 0x63de93e2, 4 }, // 12
	{ "128a11.10k",		0x080000, 0x4eac941a, 4 }, // 13

	{ "128a06.2d",		0x200000, 0x88ed598c, 5 }, // 14 shared
	{ "128a07.1d",		0x200000, 0xdb79a66e, 5 }, // 15

	{ "mystwarr.nv",	0x000080, 0x28df2269, 6 }, // 16 eeprom
};

STD_ROM_PICK(mystwarr)
STD_ROM_FN(mystwarr)

struct BurnDriver BurnDrvMystwarr = {
	"mystwarr", NULL, NULL, NULL, "1993",
	"Mystic Warriors (ver EAA)\0", NULL, "Konami", "GX128",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, mystwarrRomInfo, mystwarrRomName, NULL, NULL, MystwarrInputInfo, MystwarrDIPInfo,
	MystwarrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x0800,
	288, 224, 4, 3
};


// Mystic Warriors (ver UAA)

static struct BurnRomInfo mystwarruRomDesc[] = {
	{ "128uaa01.20f",	0x040000, 0x3a89aafd, 1 }, //  0 maincpu
	{ "128uaa02.20g",	0x040000, 0xde07410f, 1 }, //  1
	{ "128a03.19f",		0x080000, 0xe98094f3, 1 }, //  2
	{ "128a04.19g",		0x080000, 0x88c6a3e4, 1 }, //  3

	{ "128a05.6b",		0x020000, 0x0e5194e0, 2 }, //  4 soundcpu

	{ "128a08.1h",		0x100000, 0x63d6cfa0, 3 }, //  5 gfx1
	{ "128a09.1k",		0x100000, 0x573a7725, 3 }, //  6
	{ "128a10.3h",		0x080000, 0x558e545a, 3 }, //  7

	{ "128a16.22k",		0x100000, 0x459b6407, 4 }, //  8 gfx2
	{ "128a15.20k",		0x100000, 0x6bbfedf4, 4 }, //  9
	{ "128a14.19k",		0x100000, 0xf7bd89dd, 4 }, // 10
	{ "128a13.17k",		0x100000, 0xe89b66a2, 4 }, // 11
	{ "128a12.12k",		0x080000, 0x63de93e2, 4 }, // 12
	{ "128a11.10k",		0x080000, 0x4eac941a, 4 }, // 13

	{ "128a06.2d",		0x200000, 0x88ed598c, 5 }, // 14 shared
	{ "128a07.1d",		0x200000, 0xdb79a66e, 5 }, // 15

	{ "mystwarru.nv",	0x000080, 0x1a2597c7, 6 }, // 16 eeprom
};

STD_ROM_PICK(mystwarru)
STD_ROM_FN(mystwarru)

struct BurnDriver BurnDrvMystwarru = {
	"mystwarru", "mystwarr", NULL, NULL, "1993",
	"Mystic Warriors (ver UAA)\0", NULL, "Konami", "GX128",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, mystwarruRomInfo, mystwarruRomName, NULL, NULL, MystwarrInputInfo, MystwarrDIPInfo,
	MystwarrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x0800,
	288, 224, 4, 3
};


// Mystic Warriors (ver JAA)

static struct BurnRomInfo mystwarrjRomDesc[] = {
	{ "128jaa01.20f",	0x040000, 0x49c37bfe, 1 }, //  0 maincpu
	{ "128jaa02.20g",	0x040000, 0xe39fb3bb, 1 }, //  1
	{ "128a03.19f",		0x080000, 0xe98094f3, 1 }, //  2
	{ "128a04.19g",		0x080000, 0x88c6a3e4, 1 }, //  3

	{ "128a05.6b",		0x020000, 0x0e5194e0, 2 }, //  4 soundcpu

	{ "128a08.1h",		0x100000, 0x63d6cfa0, 3 }, //  5 gfx1
	{ "128a09.1k",		0x100000, 0x573a7725, 3 }, //  6
	{ "128a10.3h",		0x080000, 0x558e545a, 3 }, //  7

	{ "128a16.22k",		0x100000, 0x459b6407, 4 }, //  8 gfx2
	{ "128a15.20k",		0x100000, 0x6bbfedf4, 4 }, //  9
	{ "128a14.19k",		0x100000, 0xf7bd89dd, 4 }, // 10
	{ "128a13.17k",		0x100000, 0xe89b66a2, 4 }, // 11
	{ "128a12.12k",		0x080000, 0x63de93e2, 4 }, // 12
	{ "128a11.10k",		0x080000, 0x4eac941a, 4 }, // 13

	{ "128a06.2d",		0x200000, 0x88ed598c, 5 }, // 14 shared
	{ "128a07.1d",		0x200000, 0xdb79a66e, 5 }, // 15

	{ "mystwarrj.nv",	0x000080, 0x8e259918, 6 }, // 16 eeprom
};

STD_ROM_PICK(mystwarrj)
STD_ROM_FN(mystwarrj)

struct BurnDriver BurnDrvMystwarrj = {
	"mystwarrj", "mystwarr", NULL, NULL, "1993",
	"Mystic Warriors (ver JAA)\0", NULL, "Konami", "GX128",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, mystwarrjRomInfo, mystwarrjRomName, NULL, NULL, MystwarrInputInfo, MystwarrDIPInfo,
	MystwarrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x0800,
	288, 224, 4, 3
};


// Mystic Warriors (ver AAB)

static struct BurnRomInfo mystwarraRomDesc[] = {
	{ "128aab01.20f",	0x040000, 0x3dc89153, 1 }, //  0 maincpu
	{ "128aab02.20g",	0x040000, 0x8fe92ad2, 1 }, //  1
	{ "128a03.19f",		0x080000, 0xe98094f3, 1 }, //  2
	{ "128a04.19g",		0x080000, 0x88c6a3e4, 1 }, //  3

	{ "128a05.6b",		0x020000, 0x0e5194e0, 2 }, //  4 soundcpu

	{ "128a08.1h",		0x100000, 0x63d6cfa0, 3 }, //  5 gfx1
	{ "128a09.1k",		0x100000, 0x573a7725, 3 }, //  6
	{ "128a10.3h",		0x080000, 0x558e545a, 3 }, //  7

	{ "128a16.22k",		0x100000, 0x459b6407, 4 }, //  8 gfx2
	{ "128a15.20k",		0x100000, 0x6bbfedf4, 4 }, //  9
	{ "128a14.19k",		0x100000, 0xf7bd89dd, 4 }, // 10
	{ "128a13.17k",		0x100000, 0xe89b66a2, 4 }, // 11
	{ "128a12.12k",		0x080000, 0x63de93e2, 4 }, // 12
	{ "128a11.10k",		0x080000, 0x4eac941a, 4 }, // 13

	{ "128a06.2d",		0x200000, 0x88ed598c, 5 }, // 14 shared
	{ "128a07.1d",		0x200000, 0xdb79a66e, 5 }, // 15

	{ "eeprom",			0x000080, 0xfd6a25b4, 6 }, // 16 eeprom
};

STD_ROM_PICK(mystwarra)
STD_ROM_FN(mystwarra)

struct BurnDriver BurnDrvMystwarra = {
	"mystwarra", "mystwarr", NULL, NULL, "1993",
	"Mystic Warriors (ver AAB)\0", NULL, "Konami", "GX128",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, mystwarraRomInfo, mystwarraRomName, NULL, NULL, MystwarrInputInfo, MystwarrDIPInfo,
	MystwarrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x0800,
	288, 224, 4, 3
};


// Mystic Warriors (ver AAA)

static struct BurnRomInfo mystwarraaRomDesc[] = {
	{ "128aaa01.20f",	0x040000, 0x633ead86, 1 }, //  0 maincpu
	{ "128aaa02.20g",	0x040000, 0x69ab81a2, 1 }, //  1
	{ "128a03.19f",		0x080000, 0xe98094f3, 1 }, //  2
	{ "128a04.19g",		0x080000, 0x88c6a3e4, 1 }, //  3

	{ "128a05.6b",		0x020000, 0x0e5194e0, 2 }, //  4 soundcpu

	{ "128a08.1h",		0x100000, 0x63d6cfa0, 3 }, //  5 gfx1
	{ "128a09.1k",		0x100000, 0x573a7725, 3 }, //  6
	{ "128a10.3h",		0x080000, 0x558e545a, 3 }, //  7

	{ "128a16.22k",		0x100000, 0x459b6407, 4 }, //  8 gfx2
	{ "128a15.20k",		0x100000, 0x6bbfedf4, 4 }, //  9
	{ "128a14.19k",		0x100000, 0xf7bd89dd, 4 }, // 10
	{ "128a13.17k",		0x100000, 0xe89b66a2, 4 }, // 11
	{ "128a12.12k",		0x080000, 0x63de93e2, 4 }, // 12
	{ "128a11.10k",		0x080000, 0x4eac941a, 4 }, // 13

	{ "128a06.2d",		0x200000, 0x88ed598c, 5 }, // 14 shared
	{ "128a07.1d",		0x200000, 0xdb79a66e, 5 }, // 15

	{ "mystwarra.nv",	0x000080, 0x38951263, 6 }, // 16 eeprom
};

STD_ROM_PICK(mystwarraa)
STD_ROM_FN(mystwarraa)

struct BurnDriver BurnDrvMystwarraa = {
	"mystwarraa", "mystwarr", NULL, NULL, "1993",
	"Mystic Warriors (ver AAA)\0", NULL, "Konami", "GX128",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, mystwarraaRomInfo, mystwarraaRomName, NULL, NULL, MystwarrInputInfo, MystwarrDIPInfo,
	MystwarrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x0800,
	288, 224, 4, 3
};


// Violent Storm (ver EAC)

static struct BurnRomInfo viostormRomDesc[] = {
	{ "168eac01.15h",	0x080000, 0x9f6b5c81, 1 }, //  0 maincpu
	{ "168eac02.15f",	0x080000, 0x126ecf03, 1 }, //  1

	{ "168a05.7c",		0x020000, 0x507fb3eb, 2 }, //  2 soundcpu

	{ "168a09.1h",		0x200000, 0x1b34a881, 3 }, //  3 gfx1
	{ "168a08.1k",		0x200000, 0xdb0ce743, 3 }, //  4

	{ "168a10.22k",		0x200000, 0xbd2bbdea, 4 }, //  5 gfx2
	{ "168a11.19k",		0x200000, 0x7a57c9e7, 4 }, //  6
	{ "168a12.20k",		0x200000, 0xb6b1c4ef, 4 }, //  7
	{ "168a13.17k",		0x200000, 0xcdec3650, 4 }, //  8

	{ "168a06.1c",		0x200000, 0x25404fd7, 5 }, //  9 shared
	{ "168a07.1e",		0x200000, 0xfdbbf8cc, 5 }, // 10

	{ "viostorm.nv",	0x000080, 0x3cb1c96c, 6 }, // 11 eeprom
};

STD_ROM_PICK(viostorm)
STD_ROM_FN(viostorm)

struct BurnDriver BurnDrvViostorm = {
	"viostorm", NULL, NULL, NULL, "1993",
	"Violent Storm (ver EAC)\0", NULL, "Konami", "GX224",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, viostormRomInfo, viostormRomName, NULL, NULL, ViostormInputInfo, ViostormDIPInfo,
	ViostormInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Violent Storm (ver EAB)

static struct BurnRomInfo viostormebRomDesc[] = {
	{ "168eab01.15h",	0x080000, 0x4eee6a8e, 1 }, //  0 maincpu
	{ "168eab02.15f",	0x080000, 0x8dd8aa4c, 1 }, //  1

	{ "168a05.7c",		0x020000, 0x507fb3eb, 2 }, //  2 soundcpu

	{ "168a09.1h",		0x200000, 0x1b34a881, 3 }, //  3 gfx1
	{ "168a08.1k",		0x200000, 0xdb0ce743, 3 }, //  4

	{ "168a10.22k",		0x200000, 0xbd2bbdea, 4 }, //  5 gfx2
	{ "168a11.19k",		0x200000, 0x7a57c9e7, 4 }, //  6
	{ "168a12.20k",		0x200000, 0xb6b1c4ef, 4 }, //  7
	{ "168a13.17k",		0x200000, 0xcdec3650, 4 }, //  8

	{ "168a06.1c",		0x200000, 0x25404fd7, 5 }, //  9 shared
	{ "168a07.1e",		0x200000, 0xfdbbf8cc, 5 }, // 10

	{ "viostormeb.nv",	0x000080, 0x28b5fe49, 6 }, // 11 eeprom
};

STD_ROM_PICK(viostormeb)
STD_ROM_FN(viostormeb)

struct BurnDriver BurnDrvViostormeb = {
	"viostormeb", "viostorm", NULL, NULL, "1993",
	"Violent Storm (ver EAB)\0", NULL, "Konami", "GX168",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, viostormebRomInfo, viostormebRomName, NULL, NULL, ViostormInputInfo, ViostormDIPInfo,
	ViostormInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Violent Storm (ver UAC)

static struct BurnRomInfo viostormuRomDesc[] = {
	{ "168uac01.15h",	0x080000, 0x49853530, 1 }, //  0 maincpu
	{ "168uac02.15f",	0x080000, 0x055ca6fe, 1 }, //  1

	{ "168a05.7c",		0x020000, 0x507fb3eb, 2 }, //  2 soundcpu

	{ "168a09.1h",		0x200000, 0x1b34a881, 3 }, //  3 gfx1
	{ "168a08.1k",		0x200000, 0xdb0ce743, 3 }, //  4

	{ "168a10.22k",		0x200000, 0xbd2bbdea, 4 }, //  5 gfx2
	{ "168a11.19k",		0x200000, 0x7a57c9e7, 4 }, //  6
	{ "168a12.20k",		0x200000, 0xb6b1c4ef, 4 }, //  7
	{ "168a13.17k",		0x200000, 0xcdec3650, 4 }, //  8

	{ "168a06.1c",		0x200000, 0x25404fd7, 5 }, //  9 shared
	{ "168a07.1e",		0x200000, 0xfdbbf8cc, 5 }, // 10

	{ "viostormu.nv",	0x000080, 0x797042a1, 6 }, // 11 eeprom
};

STD_ROM_PICK(viostormu)
STD_ROM_FN(viostormu)

struct BurnDriver BurnDrvViostormu = {
	"viostormu", "viostorm", NULL, NULL, "1993",
	"Violent Storm (ver UAC)\0", NULL, "Konami", "GX168",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, viostormuRomInfo, viostormuRomName, NULL, NULL, ViostormInputInfo, ViostormDIPInfo,
	ViostormInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Violent Storm (ver UAB)

static struct BurnRomInfo viostormubRomDesc[] = {
	{ "168uab01.15h",	0x080000, 0x2d6a9fa3, 1 }, //  0 maincpu
	{ "168uab02.15f",	0x080000, 0x0e75f7cc, 1 }, //  1

	{ "168a05.7c",		0x020000, 0x507fb3eb, 2 }, //  2 soundcpu

	{ "168a09.1h",		0x200000, 0x1b34a881, 3 }, //  3 gfx1
	{ "168a08.1k",		0x200000, 0xdb0ce743, 3 }, //  4

	{ "168a10.22k",		0x200000, 0xbd2bbdea, 4 }, //  5 gfx2
	{ "168a11.19k",		0x200000, 0x7a57c9e7, 4 }, //  6
	{ "168a12.20k",		0x200000, 0xb6b1c4ef, 4 }, //  7
	{ "168a13.17k",		0x200000, 0xcdec3650, 4 }, //  8

	{ "168a06.1c",		0x200000, 0x25404fd7, 5 }, //  9 shared
	{ "168a07.1e",		0x200000, 0xfdbbf8cc, 5 }, // 10

	{ "viostormub.nv",	0x000080, 0xb6937413, 6 }, // 11 eeprom
};

STD_ROM_PICK(viostormub)
STD_ROM_FN(viostormub)

struct BurnDriver BurnDrvViostormub = {
	"viostormub", "viostorm", NULL, NULL, "1993",
	"Violent Storm (ver UAB)\0", NULL, "Konami", "GX168",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, viostormubRomInfo, viostormubRomName, NULL, NULL, ViostormInputInfo, ViostormDIPInfo,
	ViostormInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Violent Storm (ver AAC)

static struct BurnRomInfo viostormaRomDesc[] = {
	{ "168aac01.15h",	0x080000, 0x3620635c, 1 }, //  0 maincpu
	{ "168aac02.15f",	0x080000, 0xdb679aec, 1 }, //  1

	{ "168a05.7c",		0x020000, 0x507fb3eb, 2 }, //  2 soundcpu

	{ "168a09.1h",		0x200000, 0x1b34a881, 3 }, //  3 gfx1
	{ "168a08.1k",		0x200000, 0xdb0ce743, 3 }, //  4

	{ "168a10.22k",		0x200000, 0xbd2bbdea, 4 }, //  5 gfx2
	{ "168a11.19k",		0x200000, 0x7a57c9e7, 4 }, //  6
	{ "168a12.20k",		0x200000, 0xb6b1c4ef, 4 }, //  7
	{ "168a13.17k",		0x200000, 0xcdec3650, 4 }, //  8

	{ "168a06.1c",		0x200000, 0x25404fd7, 5 }, //  9 shared
	{ "168a07.1e",		0x200000, 0xfdbbf8cc, 5 }, // 10

	{ "viostorma.nv",	0x000080, 0x2cfbf966, 6 }, // 11 eeprom
};

STD_ROM_PICK(viostorma)
STD_ROM_FN(viostorma)

struct BurnDriver BurnDrvViostorma = {
	"viostorma", "viostorm", NULL, NULL, "1993",
	"Violent Storm (ver AAC)\0", NULL, "Konami", "GX168",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, viostormaRomInfo, viostormaRomName, NULL, NULL, ViostormInputInfo, ViostormDIPInfo,
	ViostormInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Violent Storm (ver AAB)

static struct BurnRomInfo viostormabRomDesc[] = {
	{ "168aab01.15h",	0x080000, 0x14f78423, 1 }, //  0 maincpu
	{ "168aab02.15f",	0x080000, 0x3dd1cc83, 1 }, //  1

	{ "168a05.7c",		0x020000, 0x507fb3eb, 2 }, //  2 soundcpu

	{ "168a09.1h",		0x200000, 0x1b34a881, 3 }, //  3 gfx1
	{ "168a08.1k",		0x200000, 0xdb0ce743, 3 }, //  4

	{ "168a10.22k",		0x200000, 0xbd2bbdea, 4 }, //  5 gfx2
	{ "168a11.19k",		0x200000, 0x7a57c9e7, 4 }, //  6
	{ "168a12.20k",		0x200000, 0xb6b1c4ef, 4 }, //  7
	{ "168a13.17k",		0x200000, 0xcdec3650, 4 }, //  8

	{ "168a06.1c",		0x200000, 0x25404fd7, 5 }, //  9 shared
	{ "168a07.1e",		0x200000, 0xfdbbf8cc, 5 }, // 10

	{ "viostormab.nv",	0x000080, 0x38ffce43, 6 }, // 11 eeprom
};

STD_ROM_PICK(viostormab)
STD_ROM_FN(viostormab)

struct BurnDriver BurnDrvViostormab = {
	"viostormab", "viostorm", NULL, NULL, "1993",
	"Violent Storm (ver AAB)\0", NULL, "Konami", "GX168",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, viostormabRomInfo, viostormabRomName, NULL, NULL, ViostormInputInfo, ViostormDIPInfo,
	ViostormInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Violent Storm (ver JAC)

static struct BurnRomInfo viostormjRomDesc[] = {
	{ "168jac01.b01",	0x080000, 0xf8be1225, 1 }, //  0 maincpu
	{ "168jac02.b02",	0x080000, 0xf42fd1e5, 1 }, //  1

	{ "168a05.7c",		0x020000, 0x507fb3eb, 2 }, //  2 soundcpu

	{ "168a09.1h",		0x200000, 0x1b34a881, 3 }, //  3 gfx1
	{ "168a08.1k",		0x200000, 0xdb0ce743, 3 }, //  4

	{ "168a10.22k",		0x200000, 0xbd2bbdea, 4 }, //  5 gfx2
	{ "168a11.19k",		0x200000, 0x7a57c9e7, 4 }, //  6
	{ "168a12.20k",		0x200000, 0xb6b1c4ef, 4 }, //  7
	{ "168a13.17k",		0x200000, 0xcdec3650, 4 }, //  8

	{ "168a06.1c",		0x200000, 0x25404fd7, 5 }, //  9 shared
	{ "168a07.1e",		0x200000, 0xfdbbf8cc, 5 }, // 10

	{ "viostormj.nv",	0x000080, 0x32f5d8bc, 6 }, // 11 eeprom
};

STD_ROM_PICK(viostormj)
STD_ROM_FN(viostormj)

struct BurnDriver BurnDrvViostormj = {
	"viostormj", "viostorm", NULL, NULL, "1993",
	"Violent Storm (ver JAC)\0", NULL, "Konami", "GX168",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, viostormjRomInfo, viostormjRomName, NULL, NULL, ViostormInputInfo, ViostormDIPInfo,
	ViostormInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Metamorphic Force (ver EAA)

static struct BurnRomInfo metamrphRomDesc[] = {
	{ "224eaa01.15h",	0x040000, 0x30962c2b, 1 }, //  0 maincpu
	{ "224eaa02.15f",	0x040000, 0xe314330a, 1 }, //  1
	{ "224a03",			0x080000, 0xa5bedb01, 1 }, //  2
	{ "224a04",			0x080000, 0xada53ba4, 1 }, //  3

	{ "224a05",			0x040000, 0x4b4c985c, 2 }, //  4 soundcpu

	{ "224a09",			0x100000, 0x1931afce, 3 }, //  5 gfx1
	{ "224a08",			0x100000, 0xdc94d53a, 3 }, //  6

	{ "224a10",			0x200000, 0x161287f0, 4 }, //  7 gfx2
	{ "224a11",			0x200000, 0xdf5960e1, 4 }, //  8
	{ "224a12",			0x200000, 0xca72a4b3, 4 }, //  9
	{ "224a13",			0x200000, 0x86b58feb, 4 }, // 10

	{ "224a14",			0x040000, 0x3c79b404, 5 }, // 11 k053250_1

	{ "224a06",			0x200000, 0x972f6abe, 6 }, // 12 shared
	{ "224a07",			0x100000, 0x61b2f97a, 6 }, // 13

	{ "metamrph.nv",	0x000080, 0x2c51229a, 7 }, // 14 eeprom
};

STD_ROM_PICK(metamrph)
STD_ROM_FN(metamrph)

struct BurnDriver BurnDrvMetamrph = {
	"metamrph", NULL, NULL, NULL, "1993",
	"Metamorphic Force (ver EAA)\0", NULL, "Konami", "GX224",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, metamrphRomInfo, metamrphRomName, NULL, NULL, MetamrphInputInfo, MetamrphDIPInfo,
	MetamrphInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x0800,
	288, 224, 4, 3
};


// Metamorphic Force (ver EAA - alternate)
/* alternate set - possibly a bugfix version. Only 2 adjusted bytes causing a swap in commands */

static struct BurnRomInfo metamrpheRomDesc[] = {
	{ "3.15h",			0x040000, 0x8b9f1ba3, 1 }, //  0 maincpu
	{ "224eaa02.15f",	0x040000, 0xe314330a, 1 }, //  1
	{ "224a03",			0x080000, 0xa5bedb01, 1 }, //  2
	{ "224a04",			0x080000, 0xada53ba4, 1 }, //  3

	{ "224a05",			0x040000, 0x4b4c985c, 2 }, //  4 soundcpu

	{ "224a09",			0x100000, 0x1931afce, 3 }, //  5 gfx1
	{ "224a08",			0x100000, 0xdc94d53a, 3 }, //  6

	{ "224a10",			0x200000, 0x161287f0, 4 }, //  7 gfx2
	{ "224a11",			0x200000, 0xdf5960e1, 4 }, //  8
	{ "224a12",			0x200000, 0xca72a4b3, 4 }, //  9
	{ "224a13",			0x200000, 0x86b58feb, 4 }, // 10

	{ "224a14",			0x040000, 0x3c79b404, 5 }, // 11 k053250_1

	{ "224a06",			0x200000, 0x972f6abe, 6 }, // 12 shared
	{ "224a07",			0x100000, 0x61b2f97a, 6 }, // 13

	{ "metamrph.nv",	0x000080, 0x2c51229a, 7 }, // 14 eeprom
};

STD_ROM_PICK(metamrphe)
STD_ROM_FN(metamrphe)

struct BurnDriver BurnDrvMetamrphe = {
	"metamrphe", "metamrph", NULL, NULL, "1993",
	"Metamorphic Force (ver EAA - alternate)\0", NULL, "Konami", "GX224",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, metamrpheRomInfo, metamrpheRomName, NULL, NULL, MetamrphInputInfo, MetamrphDIPInfo,
	MetamrphInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x0800,
	288, 224, 4, 3
};


// Metamorphic Force (ver AAA)

static struct BurnRomInfo metamrphaRomDesc[] = {
	{ "224aaa01.15h",	0x040000, 0x12515518, 1 }, //  0 maincpu
	{ "224aaa02.15f",	0x040000, 0x04ed41df, 1 }, //  1
	{ "224a03",			0x080000, 0xa5bedb01, 1 }, //  2
	{ "224a04",			0x080000, 0xada53ba4, 1 }, //  3

	{ "224a05",			0x040000, 0x4b4c985c, 2 }, //  4 soundcpu

	{ "224a09",			0x100000, 0x1931afce, 3 }, //  5 gfx1
	{ "224a08",			0x100000, 0xdc94d53a, 3 }, //  6

	{ "224a10",			0x200000, 0x161287f0, 4 }, //  7 gfx2
	{ "224a11",			0x200000, 0xdf5960e1, 4 }, //  8
	{ "224a12",			0x200000, 0xca72a4b3, 4 }, //  9
	{ "224a13",			0x200000, 0x86b58feb, 4 }, // 10

	{ "224a14",			0x040000, 0x3c79b404, 5 }, // 11 k053250_1

	{ "224a06",			0x200000, 0x972f6abe, 6 }, // 12 shared
	{ "224a07",			0x100000, 0x61b2f97a, 6 }, // 13

	// default eeprom to prevent game booting upside down with error
	{ "metamrpha.nv",	0x000080, 0x6d34a4f2, 7 }, // 14 eeprom
};

STD_ROM_PICK(metamrpha)
STD_ROM_FN(metamrpha)

struct BurnDriver BurnDrvMetamrpha = {
	"metamrpha", "metamrph", NULL, NULL, "1993",
	"Metamorphic Force (ver AAA)\0", NULL, "Konami", "GX224",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, metamrphaRomInfo, metamrphaRomName, NULL, NULL, MetamrphInputInfo, MetamrphDIPInfo,
	MetamrphInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x0800,
	288, 224, 4, 3
};


// Metamorphic Force (ver UAA)

static struct BurnRomInfo metamrphuRomDesc[] = {
	{ "224uaa01.15h",	0x040000, 0xe1d9b516, 1 }, //  0 maincpu
	{ "224uaa02.15f",	0x040000, 0x289c926b, 1 }, //  1
	{ "224a03",			0x080000, 0xa5bedb01, 1 }, //  2
	{ "224a04",			0x080000, 0xada53ba4, 1 }, //  3

	{ "224a05",			0x040000, 0x4b4c985c, 2 }, //  4 soundcpu

	{ "224a09",			0x100000, 0x1931afce, 3 }, //  5 gfx1
	{ "224a08",			0x100000, 0xdc94d53a, 3 }, //  6

	{ "224a10",			0x200000, 0x161287f0, 4 }, //  7 gfx2
	{ "224a11",			0x200000, 0xdf5960e1, 4 }, //  8
	{ "224a12",			0x200000, 0xca72a4b3, 4 }, //  9
	{ "224a13",			0x200000, 0x86b58feb, 4 }, // 10

	{ "224a14",			0x040000, 0x3c79b404, 5 }, // 11 k053250_1

	{ "224a06",			0x200000, 0x972f6abe, 6 }, // 12 shared
	{ "224a07",			0x100000, 0x61b2f97a, 6 }, // 13

	{ "metamrphu.nv",	0x000080, 0x1af2f855, 7 }, // 14 eeprom
};

STD_ROM_PICK(metamrphu)
STD_ROM_FN(metamrphu)

struct BurnDriver BurnDrvMetamrphu = {
	"metamrphu", "metamrph", NULL, NULL, "1993",
	"Metamorphic Force (ver UAA)\0", NULL, "Konami", "GX224",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, metamrphuRomInfo, metamrphuRomName, NULL, NULL, MetamrphInputInfo, MetamrphDIPInfo,
	MetamrphInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x0800,
	288, 224, 4, 3
};


// Metamorphic Force (ver JAA)

static struct BurnRomInfo metamrphjRomDesc[] = {
	{ "224jaa01.15h",	0x040000, 0x558d2602, 1 }, //  0 maincpu
	{ "224jaa02.15f",	0x040000, 0x9b252ace, 1 }, //  1
	{ "224a03",			0x080000, 0xa5bedb01, 1 }, //  2
	{ "224a04",			0x080000, 0xada53ba4, 1 }, //  3

	{ "224a05",			0x040000, 0x4b4c985c, 2 }, //  4 soundcpu

	{ "224a09",			0x100000, 0x1931afce, 3 }, //  5 gfx1
	{ "224a08",			0x100000, 0xdc94d53a, 3 }, //  6

	{ "224a10",			0x200000, 0x161287f0, 4 }, //  7 gfx2
	{ "224a11",			0x200000, 0xdf5960e1, 4 }, //  8
	{ "224a12",			0x200000, 0xca72a4b3, 4 }, //  9
	{ "224a13",			0x200000, 0x86b58feb, 4 }, // 10

	{ "224a14",			0x040000, 0x3c79b404, 5 }, // 11 k053250_1

	{ "224a06",			0x200000, 0x972f6abe, 6 }, // 12 shared
	{ "224a07",			0x100000, 0x61b2f97a, 6 }, // 13

	{ "metamrphj.nv",	0x000080, 0x30497478, 7 }, // 14 eeprom
};

STD_ROM_PICK(metamrphj)
STD_ROM_FN(metamrphj)

struct BurnDriver BurnDrvMetamrphj = {
	"metamrphj", "metamrph", NULL, NULL, "1993",
	"Metamorphic Force (ver JAA)\0", NULL, "Konami", "GX224",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, metamrphjRomInfo, metamrphjRomName, NULL, NULL, MetamrphInputInfo, MetamrphDIPInfo,
	MetamrphInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x0800,
	288, 224, 4, 3
};


// Martial Champion (ver EAB)

static struct BurnRomInfo mtlchampRomDesc[] = {
	{ "234eab01.20f",	0x040000, 0x7c4d1e50, 1 }, //  0 maincpu
	{ "234eab02.20g",	0x040000, 0xd8bc85c9, 1 }, //  1
	{ "234_d03.19f",	0x080000, 0xabb577c6, 1 }, //  2
	{ "234_d04.19g",	0x080000, 0x030a1925, 1 }, //  3

	{ "234_d05.6b",		0x020000, 0xefb6bcaa, 2 }, //  4 soundcpu

	{ "234a08.1h",		0x100000, 0x27e94288, 3 }, //  5 gfx1
	{ "234a09.1k",		0x100000, 0x03aad28f, 3 }, //  6
	{ "234a10.3h",		0x080000, 0x51f50fe2, 3 }, //  7

	{ "234a16.22k",		0x200000, 0x14d909a5, 4 }, //  8 gfx2
	{ "234a15.20k",		0x200000, 0xa5028418, 4 }, //  9
	{ "234a14.19k",		0x200000, 0xd7921f47, 4 }, // 10
	{ "234a13.17k",		0x200000, 0x5974392e, 4 }, // 11
	{ "234a12.12k",		0x100000, 0xc7f2b099, 4 }, // 12
	{ "234a11.10k",		0x100000, 0x82923713, 4 }, // 13

	{ "234a06.2d",		0x200000, 0x12d32384, 5 }, // 14 shared
	{ "234a07.1d",		0x200000, 0x05ee239f, 5 }, // 15

	{ "mtlchamp.nv",	0x000080, 0xcd47858e, 6 }, // 16 eeprom
};

STD_ROM_PICK(mtlchamp)
STD_ROM_FN(mtlchamp)

struct BurnDriver BurnDrvMtlchamp = {
	"mtlchamp", NULL, NULL, NULL, "1993",
	"Martial Champion (ver EAB)\0", "Missing Graphics on Intro/Title screens", "Konami", "GX234",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_VSFIGHT, 0,
	NULL, mtlchampRomInfo, mtlchampRomName, NULL, NULL, MartchmpInputInfo, MartchmpDIPInfo,
	MartchmpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Martial Champion (ver EAA)

static struct BurnRomInfo mtlchamp1RomDesc[] = {
	{ "234eaa01.20f",	0x040000, 0x8fa731db, 1 }, //  0 maincpu
	{ "234eaa02.20g",	0x040000, 0xe7b50b54, 1 }, //  1
	{ "234_d03.19f",	0x080000, 0xabb577c6, 1 }, //  2
	{ "234_d04.19g",	0x080000, 0x030a1925, 1 }, //  3

	{ "234_d05.6b",		0x020000, 0xefb6bcaa, 2 }, //  4 soundcpu

	{ "234a08.1h",		0x100000, 0x27e94288, 3 }, //  5 gfx1
	{ "234a09.1k",		0x100000, 0x03aad28f, 3 }, //  6
	{ "234a10.3h",		0x080000, 0x51f50fe2, 3 }, //  7

	{ "234a16.22k",		0x200000, 0x14d909a5, 4 }, //  8 gfx2
	{ "234a15.20k",		0x200000, 0xa5028418, 4 }, //  9
	{ "234a14.19k",		0x200000, 0xd7921f47, 4 }, // 10
	{ "234a13.17k",		0x200000, 0x5974392e, 4 }, // 11
	{ "234a12.12k",		0x100000, 0xc7f2b099, 4 }, // 12
	{ "234a11.10k",		0x100000, 0x82923713, 4 }, // 13

	{ "234a06.2d",		0x200000, 0x12d32384, 5 }, // 14 shared
	{ "234a07.1d",		0x200000, 0x05ee239f, 5 }, // 15

	{ "mtlchamp1.nv",	0x000080, 0x202f6968, 6 }, // 16 eeprom
};

STD_ROM_PICK(mtlchamp1)
STD_ROM_FN(mtlchamp1)

struct BurnDriver BurnDrvMtlchamp1 = {
	"mtlchamp1", "mtlchamp", NULL, NULL, "1993",
	"Martial Champion (ver EAA)\0", "Missing Graphics on Intro/Title screens", "Konami", "GX234",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_VSFIGHT, 0,
	NULL, mtlchamp1RomInfo, mtlchamp1RomName, NULL, NULL, MartchmpInputInfo, MartchmpDIPInfo,
	MartchmpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Martial Champion (ver AAA)

static struct BurnRomInfo mtlchampaRomDesc[] = {
	{ "234aaa01.20f",	0x040000, 0x32c70e65, 1 }, //  0 maincpu
	{ "234aaa02.20g",	0x040000, 0x2f666d52, 1 }, //  1
	{ "234_d03.19f",	0x080000, 0xabb577c6, 1 }, //  2
	{ "234_d04.19g",	0x080000, 0x030a1925, 1 }, //  3

	{ "234_d05.6b",		0x020000, 0xefb6bcaa, 2 }, //  4 soundcpu

	{ "234a08.1h",		0x100000, 0x27e94288, 3 }, //  5 gfx1
	{ "234a09.1k",		0x100000, 0x03aad28f, 3 }, //  6
	{ "234a10.3h",		0x080000, 0x51f50fe2, 3 }, //  7

	{ "234a16.22k",		0x200000, 0x14d909a5, 4 }, //  8 gfx2
	{ "234a15.20k",		0x200000, 0xa5028418, 4 }, //  9
	{ "234a14.19k",		0x200000, 0xd7921f47, 4 }, // 10
	{ "234a13.17k",		0x200000, 0x5974392e, 4 }, // 11
	{ "234a12.12k",		0x100000, 0xc7f2b099, 4 }, // 12
	{ "234a11.10k",		0x100000, 0x82923713, 4 }, // 13

	{ "234a06.2d",		0x200000, 0x12d32384, 5 }, // 14 shared
	{ "234a07.1d",		0x200000, 0x05ee239f, 5 }, // 15

	{ "mtlchampa.nv",	0x000080, 0x79a6f420, 6 }, // 16 eeprom
};

STD_ROM_PICK(mtlchampa)
STD_ROM_FN(mtlchampa)

struct BurnDriver BurnDrvMtlchampa = {
	"mtlchampa", "mtlchamp", NULL, NULL, "1993",
	"Martial Champion (ver AAA)\0", "Missing Graphics on Intro/Title screens", "Konami", "GX234",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_VSFIGHT, 0,
	NULL, mtlchampaRomInfo, mtlchampaRomName, NULL, NULL, MartchmpInputInfo, MartchmpDIPInfo,
	MartchmpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Martial Champion (ver JAA)

static struct BurnRomInfo mtlchampjRomDesc[] = {
	{ "234jaa01.20f",	0x040000, 0x76c3c568, 1 }, //  0 maincpu
	{ "234jaa02.20g",	0x040000, 0x95eec0aa, 1 }, //  1
	{ "234_d03.19f",	0x080000, 0xabb577c6, 1 }, //  2
	{ "234_d04.19g",	0x080000, 0x030a1925, 1 }, //  3

	{ "234_d05.6b",		0x020000, 0xefb6bcaa, 2 }, //  4 soundcpu

	{ "234a08.1h",		0x100000, 0x27e94288, 3 }, //  5 gfx1
	{ "234a09.1k",		0x100000, 0x03aad28f, 3 }, //  6
	{ "234a10.3h",		0x080000, 0x51f50fe2, 3 }, //  7

	{ "234a16.22k",		0x200000, 0x14d909a5, 4 }, //  8 gfx2
	{ "234a15.20k",		0x200000, 0xa5028418, 4 }, //  9
	{ "234a14.19k",		0x200000, 0xd7921f47, 4 }, // 10
	{ "234a13.17k",		0x200000, 0x5974392e, 4 }, // 11
	{ "234a12.12k",		0x100000, 0xc7f2b099, 4 }, // 12
	{ "234a11.10k",		0x100000, 0x82923713, 4 }, // 13

	{ "234a06.2d",		0x200000, 0x12d32384, 5 }, // 14 shared
	{ "234a07.1d",		0x200000, 0x05ee239f, 5 }, // 15

	{ "mtlchampj.nv",	0x000080, 0xe311816f, 6 }, // 16 eeprom
};

STD_ROM_PICK(mtlchampj)
STD_ROM_FN(mtlchampj)

struct BurnDriver BurnDrvMtlchampj = {
	"mtlchampj", "mtlchamp", NULL, NULL, "1993",
	"Martial Champion (ver JAA)\0", "Missing Graphics on Intro/Title screens", "Konami", "GX234",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_VSFIGHT, 0,
	NULL, mtlchampjRomInfo, mtlchampjRomName, NULL, NULL, MartchmpInputInfo, MartchmpDIPInfo,
	MartchmpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Martial Champion (ver UAE)

static struct BurnRomInfo mtlchampuRomDesc[] = {
	{ "234uae01.20f",	0x040000, 0xacecfec9, 1 }, //  0 maincpu
	{ "234uae02.20g",	0x040000, 0xc54ccf65, 1 }, //  1
	{ "234_d03.19f",	0x080000, 0xabb577c6, 1 }, //  2
	{ "234_d04.19g",	0x080000, 0x030a1925, 1 }, //  3

	{ "234_d05.6b",		0x020000, 0xefb6bcaa, 2 }, //  4 soundcpu

	{ "234a08.1h",		0x100000, 0x27e94288, 3 }, //  5 gfx1
	{ "234a09.1k",		0x100000, 0x03aad28f, 3 }, //  6
	{ "234a10.3h",		0x080000, 0x51f50fe2, 3 }, //  7

	{ "234a16.22k",		0x200000, 0x14d909a5, 4 }, //  8 gfx2
	{ "234a15.20k",		0x200000, 0xa5028418, 4 }, //  9
	{ "234a14.19k",		0x200000, 0xd7921f47, 4 }, // 10
	{ "234a13.17k",		0x200000, 0x5974392e, 4 }, // 11
	{ "234a12.12k",		0x100000, 0xc7f2b099, 4 }, // 12
	{ "234a11.10k",		0x100000, 0x82923713, 4 }, // 13

	{ "234a06.2d",		0x200000, 0x12d32384, 5 }, // 14 shared
	{ "234a07.1d",		0x200000, 0x05ee239f, 5 }, // 15

	{ "mtlchampu.nv",	0x000080, 0x182f146a, 6 }, // 16 eeprom
};

STD_ROM_PICK(mtlchampu)
STD_ROM_FN(mtlchampu)

struct BurnDriver BurnDrvMtlchampu = {
	"mtlchampu", "mtlchamp", NULL, NULL, "1993",
	"Martial Champion (ver UAE)\0", "Missing Graphics on Intro/Title screens", "Konami", "GX234",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_VSFIGHT, 0,
	NULL, mtlchampuRomInfo, mtlchampuRomName, NULL, NULL, MartchmpInputInfo, MartchmpDIPInfo,
	MartchmpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Martial Champion (ver UAD)

static struct BurnRomInfo mtlchampu1RomDesc[] = {
	{ "234uad01.20f",	0x040000, 0x5f6c8d09, 1 }, //  0 maincpu
	{ "234uad02.20g",	0x040000, 0x15ca4fb2, 1 }, //  1
	{ "234_d03.19f",	0x080000, 0xabb577c6, 1 }, //  2
	{ "234_d04.19g",	0x080000, 0x030a1925, 1 }, //  3

	{ "234_d05.6b",		0x020000, 0xefb6bcaa, 2 }, //  4 soundcpu

	{ "234a08.1h",		0x100000, 0x27e94288, 3 }, //  5 gfx1
	{ "234a09.1k",		0x100000, 0x03aad28f, 3 }, //  6
	{ "234a10.3h",		0x080000, 0x51f50fe2, 3 }, //  7

	{ "234a16.22k",		0x200000, 0x14d909a5, 4 }, //  8 gfx2
	{ "234a15.20k",		0x200000, 0xa5028418, 4 }, //  9
	{ "234a14.19k",		0x200000, 0xd7921f47, 4 }, // 10
	{ "234a13.17k",		0x200000, 0x5974392e, 4 }, // 11
	{ "234a12.12k",		0x100000, 0xc7f2b099, 4 }, // 12
	{ "234a11.10k",		0x100000, 0x82923713, 4 }, // 13

	{ "234a06.2d",		0x200000, 0x12d32384, 5 }, // 14 shared
	{ "234a07.1d",		0x200000, 0x05ee239f, 5 }, // 15

	{ "mtlchampu1.nv",	0x000080, 0xf5d84df7, 6 }, // 16 eeprom
};

STD_ROM_PICK(mtlchampu1)
STD_ROM_FN(mtlchampu1)

struct BurnDriver BurnDrvMtlchampu1 = {
	"mtlchampu1", "mtlchamp", NULL, NULL, "1993",
	"Martial Champion (ver UAD)\0", "Missing Graphics on Intro/Title screens", "Konami", "GX234",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_VSFIGHT, 0,
	NULL, mtlchampu1RomInfo, mtlchampu1RomName, NULL, NULL, MartchmpInputInfo, MartchmpDIPInfo,
	MartchmpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Gaiapolis (ver EAF)

static struct BurnRomInfo gaiapolsRomDesc[] = {
	{ "123e07.24m",		0x100000, 0xf1a1db0f, 1 }, //  0 maincpu
	{ "123e09.19l",		0x100000, 0x4b3b57e7, 1 }, //  1
	{ "123eaf11.19p",	0x040000, 0x9c324ade, 1 }, //  2
	{ "123eaf12.17p",	0x040000, 0x1dfa14c5, 1 }, //  3

	{ "123e13.9c",		0x040000, 0xe772f822, 2 }, //  4 soundcpu

	{ "123e16.2t",		0x100000, 0xa3238200, 3 }, //  5 gfx1
	{ "123e17.2x",		0x100000, 0xbd0b9fb9, 3 }, //  6

	{ "123e19.34u",		0x200000, 0x219a7c26, 4 }, //  7 gfx2
	{ "123e21.34y",		0x200000, 0x1888947b, 4 }, //  8
	{ "123e18.36u",		0x200000, 0x3719b6d4, 4 }, //  9
	{ "123e20.36y",		0x200000, 0x490a6f64, 4 }, // 10

	{ "123e04.32n",		0x080000, 0x0d4d5b8b, 5 }, // 11 gfx3
	{ "123e05.29n",		0x080000, 0x7d123f3e, 5 }, // 12
	{ "123e06.26n",		0x080000, 0xfa50121e, 5 }, // 13

	{ "123e01.36j",		0x020000, 0x9dbc9678, 6 }, // 14 gfx4
	{ "123e02.34j",		0x040000, 0xb8e3f500, 6 }, // 15
	{ "123e03.36m",		0x040000, 0xfde4749f, 6 }, // 16

	{ "123e14.2g",		0x200000, 0x65dfd3ff, 7 }, // 17 shared
	{ "123e15.2m",		0x200000, 0x7017ff07, 7 }, // 18

	{ "gaiapols.nv",	0x000080, 0x44c78184, 8 }, // 19 eeprom
};

STD_ROM_PICK(gaiapols)
STD_ROM_FN(gaiapols)

struct BurnDriver BurnDrvGaiapols = {
	"gaiapols", NULL, NULL, NULL, "1993",
	"Gaiapolis (ver EAF)\0", NULL, "Konami", "GX123",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, gaiapolsRomInfo, gaiapolsRomName, NULL, NULL, DadandrnInputInfo, DadandrnDIPInfo,
	GaiapolisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 376, 3, 4
};


// Gaiapolis (ver UAF)

static struct BurnRomInfo gaiapolsuRomDesc[] = {
	{ "123e07.24m",		0x100000, 0xf1a1db0f, 1 }, //  0 maincpu
	{ "123e09.19l",		0x100000, 0x4b3b57e7, 1 }, //  1
	{ "123uaf11.19p",	0x040000, 0x39dc1298, 1 }, //  2
	{ "123uaf12.17p",	0x040000, 0xc633cf52, 1 }, //  3

	{ "123e13.9c",		0x040000, 0xe772f822, 2 }, //  4 soundcpu

	{ "123e16.2t",		0x100000, 0xa3238200, 3 }, //  5 gfx1
	{ "123e17.2x",		0x100000, 0xbd0b9fb9, 3 }, //  6

	{ "123e19.34u",		0x200000, 0x219a7c26, 4 }, //  7 gfx2
	{ "123e21.34y",		0x200000, 0x1888947b, 4 }, //  8
	{ "123e18.36u",		0x200000, 0x3719b6d4, 4 }, //  9
	{ "123e20.36y",		0x200000, 0x490a6f64, 4 }, // 10

	{ "123e04.32n",		0x080000, 0x0d4d5b8b, 5 }, // 11 gfx3
	{ "123e05.29n",		0x080000, 0x7d123f3e, 5 }, // 12
	{ "123e06.26n",		0x080000, 0xfa50121e, 5 }, // 13

	{ "123e01.36j",		0x020000, 0x9dbc9678, 6 }, // 14 gfx4
	{ "123e02.34j",		0x040000, 0xb8e3f500, 6 }, // 15
	{ "123e03.36m",		0x040000, 0xfde4749f, 6 }, // 16

	{ "123e14.2g",		0x200000, 0x65dfd3ff, 7 }, // 17 shared
	{ "123e15.2m",		0x200000, 0x7017ff07, 7 }, // 18

	{ "gaiapolsu.nv",	0x000080, 0x7ece27b6, 8 }, // 19 eeprom
};

STD_ROM_PICK(gaiapolsu)
STD_ROM_FN(gaiapolsu)

struct BurnDriver BurnDrvGaiapolsu = {
	"gaiapolsu", "gaiapols", NULL, NULL, "1993",
	"Gaiapolis (ver UAF)\0", NULL, "Konami", "GX123",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, gaiapolsuRomInfo, gaiapolsuRomName, NULL, NULL, DadandrnInputInfo, DadandrnDIPInfo,
	GaiapolisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 376, 3, 4
};


// Gaiapolis (ver JAF)

static struct BurnRomInfo gaiapolsjRomDesc[] = {
	{ "123e07.24m",		0x100000, 0xf1a1db0f, 1 }, //  0 maincpu
	{ "123e09.19l",		0x100000, 0x4b3b57e7, 1 }, //  1
	{ "123jaf11.19p",	0x040000, 0x19919571, 1 }, //  2
	{ "123jaf12.17p",	0x040000, 0x4246e595, 1 }, //  3

	{ "123e13.9c",		0x040000, 0xe772f822, 2 }, //  4 soundcpu

	{ "123e16.2t",		0x100000, 0xa3238200, 3 }, //  5 gfx1
	{ "123e17.2x",		0x100000, 0xbd0b9fb9, 3 }, //  6

	{ "123e19.34u",		0x200000, 0x219a7c26, 4 }, //  7 gfx2
	{ "123e21.34y",		0x200000, 0x1888947b, 4 }, //  8
	{ "123e18.36u",		0x200000, 0x3719b6d4, 4 }, //  9
	{ "123e20.36y",		0x200000, 0x490a6f64, 4 }, // 10

	{ "123e04.32n",		0x080000, 0x0d4d5b8b, 5 }, // 11 gfx3
	{ "123e05.29n",		0x080000, 0x7d123f3e, 5 }, // 12
	{ "123e06.26n",		0x080000, 0xfa50121e, 5 }, // 13

	{ "123e01.36j",		0x020000, 0x9dbc9678, 6 }, // 14 gfx4
	{ "123e02.34j",		0x040000, 0xb8e3f500, 6 }, // 15
	{ "123e03.36m",		0x040000, 0xfde4749f, 6 }, // 16

	{ "123e14.2g",		0x200000, 0x65dfd3ff, 7 }, // 17 shared
	{ "123e15.2m",		0x200000, 0x7017ff07, 7 }, // 18

	{ "gaiapolsj.nv",	0x000080, 0xc4b970df, 8 }, // 19 eeprom
};

STD_ROM_PICK(gaiapolsj)
STD_ROM_FN(gaiapolsj)

struct BurnDriver BurnDrvGaiapolsj = {
	"gaiapolsj", "gaiapols", NULL, NULL, "1993",
	"Gaiapolis (ver JAF)\0", NULL, "Konami", "GX123",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, gaiapolsjRomInfo, gaiapolsjRomName, NULL, NULL, DadandrnInputInfo, DadandrnDIPInfo,
	GaiapolisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 376, 3, 4
};


// Monster Maulers (ver EAA)

static struct BurnRomInfo mmaulersRomDesc[] = {
	{ "170eaa07.24m",	0x080000, 0x5458bd93, 1 }, //  0 maincpu
	{ "170eaa09.19l",	0x080000, 0x99c95c7b, 1 }, //  1
	{ "170a08.21m",		0x040000, 0x03c59ba2, 1 }, //  2
	{ "170a10.17l",		0x040000, 0x8a340909, 1 }, //  3

	{ "170a13.9c",		0x040000, 0x2ebf4d1c, 2 }, //  4 soundcpu

	{ "170a16.2t",		0x100000, 0x41fee912, 3 }, //  5 gfx1
	{ "170a17.2x",		0x100000, 0x96957c91, 3 }, //  6
	{ "170a24.5r",		0x080000, 0x562ad4bd, 3 }, //  7

	{ "170a19.34u",		0x200000, 0xbe835141, 4 }, //  8 gfx2
	{ "170a21.34y",		0x200000, 0xbcb68136, 4 }, //  9
	{ "170a18.36u",		0x200000, 0xe1e3c8d2, 4 }, // 10
	{ "170a20.36y",		0x200000, 0xccb4d88c, 4 }, // 11
	{ "170a23.29y",		0x100000, 0x6b5390e4, 4 }, // 12
	{ "170a22.32y",		0x100000, 0x21628106, 4 }, // 13

	{ "170a04.33n",		0x080000, 0x64b9a73b, 5 }, // 14 gfx3
	{ "170a05.30n",		0x080000, 0xf2c101d0, 5 }, // 15
	{ "170a06.27n",		0x080000, 0xb032e59b, 5 }, // 16

	{ "170a02.34j",		0x040000, 0xb040cebf, 6 }, // 17 gfx4
	{ "170a03.36m",		0x040000, 0x7fb412b2, 6 }, // 18

	{ "170a14.2g",		0x200000, 0x83317cda, 7 }, // 19 shared
	{ "170a15.2m",		0x200000, 0xd4113ae9, 7 }, // 20

	{ "mmaulers.nv",	0x000080, 0x8324f517, 8 }, // 21 eeprom
};

STD_ROM_PICK(mmaulers)
STD_ROM_FN(mmaulers)

struct BurnDriver BurnDrvMmaulers = {
	"mmaulers", NULL, NULL, NULL, "1993",
	"Monster Maulers (ver EAA)\0", NULL, "Konami", "GX170",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, mmaulersRomInfo, mmaulersRomName, NULL, NULL, DadandrnInputInfo, DadandrnDIPInfo,
	DadandrnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// Kyukyoku Sentai Dadandarn (ver JAA)

static struct BurnRomInfo dadandrnRomDesc[] = {
	{ "170jaa07.24m",	0x080000, 0x6a55e828, 1 }, //  0 maincpu
	{ "170jaa09.19l",	0x080000, 0x9e821cd8, 1 }, //  1
	{ "170a08.21m",		0x040000, 0x03c59ba2, 1 }, //  2
	{ "170a10.17l",		0x040000, 0x8a340909, 1 }, //  3

	{ "170a13.9c",		0x040000, 0x2ebf4d1c, 2 }, //  4 soundcpu

	{ "170a16.2t",		0x100000, 0x41fee912, 3 }, //  5 gfx1
	{ "170a17.2x",		0x100000, 0x96957c91, 3 }, //  6
	{ "170a24.5r",		0x080000, 0x562ad4bd, 3 }, //  7

	{ "170a19.34u",		0x200000, 0xbe835141, 4 }, //  8 gfx2
	{ "170a21.34y",		0x200000, 0xbcb68136, 4 }, //  9
	{ "170a18.36u",		0x200000, 0xe1e3c8d2, 4 }, // 10
	{ "170a20.36y",		0x200000, 0xccb4d88c, 4 }, // 11
	{ "170a23.29y",		0x100000, 0x6b5390e4, 4 }, // 12
	{ "170a22.32y",		0x100000, 0x21628106, 4 }, // 13

	{ "170a04.33n",		0x080000, 0x64b9a73b, 5 }, // 14 gfx3
	{ "170a05.30n",		0x080000, 0xf2c101d0, 5 }, // 15
	{ "170a06.27n",		0x080000, 0xb032e59b, 5 }, // 16

	{ "170a02.34j",		0x040000, 0xb040cebf, 6 }, // 17 gfx4
	{ "170a03.36m",		0x040000, 0x7fb412b2, 6 }, // 18

	{ "170a14.2g",		0x200000, 0x83317cda, 7 }, // 19 shared
	{ "170a15.2m",		0x200000, 0xd4113ae9, 7 }, // 20

	{ "dadandrn.nv",	0x000080, 0x346ae0cf, 8 }, // 21 eeprom
};

STD_ROM_PICK(dadandrn)
STD_ROM_FN(dadandrn)

struct BurnDriver BurnDrvDadandrn = {
	"dadandrn", "mmaulers", NULL, NULL, "1993",
	"Kyukyoku Sentai Dadandarn (ver JAA)\0", NULL, "Konami", "GX170",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, dadandrnRomInfo, dadandrnRomName, NULL, NULL, DadandrnInputInfo, DadandrnDIPInfo,
	DadandrnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};
