// FB Alpha Deco32 driver module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "arm_intf.h"
#include "h6280_intf.h"
#include "z80_intf.h"
#include "deco16ic.h"
#include "deco146.h"
#include "msm6295.h"
#include "burn_ym2151.h"
#include "burn_gun.h"
#include "eeprom.h"
#include "decobsmt.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvARMROM;
static UINT8 *DrvHucROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvSndROM2;
static UINT8 *DrvTMSROM;
static UINT8 *DrvDVIROM; // dragngun
static UINT8 *DrvSysRAM;
static UINT8 *DrvHucRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvSprRAM2;
static UINT8 *DrvSprBuf2;
static UINT8 *DrvPalRAM;
static UINT8 *DrvPalBuf; // active palette
static UINT8 *DrvAceRAM;
static UINT8 *DrvJackRAM;
static UINT8 *DrvTMSRAM;

static UINT8 *DrvDVIRAM0; // dragngun
static UINT8 *DrvDVIRAM1;
static UINT32 *pTempSprite;

static UINT16 *pTempDraw[4];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 DrvOkiBank;
static INT32 global_priority;
static UINT32 sprite_ctrl;
static UINT32 lightgun_port;

static UINT16 color_base[3];

static INT32 nExtraCycles[3];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoyFS[4];
// fake start buttons for symmetrical 4p inputs (nslasher: 3p coin & captaven 3p, 4p start)
// captaven: not hooked up (p1,p2 start buttons are non-functional when system is set to 4players!)
// therefore, the p3, p4 start buttons are only there to keep symmetry between the inputs for netgames to work
// nslasher: p3 coin is mirrored to p1 coin
static UINT8 DrvDips[5];
static UINT8 DrvReset;
static UINT16 DrvInputs[3];

static INT32 uses_gun = 0;
static INT16 DrvGun0 = 0;
static INT16 DrvGun1 = 0;
static INT16 DrvGun2 = 0;
static INT16 DrvGun3 = 0;

static INT32 game_select = 0; // 0 capt, 1 fhist, 2 nslasher, 3 tattass, 4 dragngun
static INT32 has_ace = 0;
static INT32 use_z80 = 0;
static INT32 use_bsmt = 0;
static INT32 speedhack_address = 0;

static INT32 (*pStartDraw)() = NULL;
static INT32 (*pDrawScanline)(INT32) = NULL;

static struct BurnInputInfo CaptavenInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoyFS + 0,	"p3 start"	}, // see notes for DrvJoyFS[]
	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p3 fire 2"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoyFS + 1,	"p4 start"	}, // ""
	{"P4 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Captaven)

static struct BurnDIPInfo CaptavenDIPList[]=
{
	DIP_OFFSET(0x22)
	{0x00, 0xff, 0xff, 0xff, NULL			},
	{0x01, 0xff, 0xff, 0x7f, NULL			},
	{0x02, 0xff, 0xff, 0xff, NULL			},
	{0x03, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x00, 0x01, 0x07, 0x00, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x01, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x07, 0x02, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x00, 0x01, 0x38, 0x00, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x38, 0x08, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x38, 0x18, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x38, 0x10, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x00, 0x01, 0x40, 0x40, "Off"			},
	{0x00, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Continue Coin"	},
	{0x00, 0x01, 0x80, 0x80, "1 Start/1 Continue"	},
	{0x00, 0x01, 0x80, 0x00, "2 Start/1 Continue"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x01, 0x01, 0x03, 0x01, "1"			},
	{0x01, 0x01, 0x03, 0x00, "2"			},
	{0x01, 0x01, 0x03, 0x03, "3"			},
	{0x01, 0x01, 0x03, 0x02, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x01, 0x01, 0x0c, 0x08, "Easy"			},
	{0x01, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x01, 0x01, 0x0c, 0x04, "Hard"			},
	{0x01, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Coin Slots"		},
	{0x01, 0x01, 0x10, 0x10, "Common"		},
	{0x01, 0x01, 0x10, 0x00, "Individual"		},

	{0   , 0xfe, 0   ,    2, "Play Mode"		},
	{0x01, 0x01, 0x20, 0x20, "2 Player"		},
	{0x01, 0x01, 0x20, 0x00, "4 Player"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x01, 0x01, 0x40, 0x00, "Off"			},
	{0x01, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x01, 0x01, 0x80, 0x80, "Off"			},
	{0x01, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Reset"		},
	{0x02, 0x01, 0x10, 0x10, "Off"			},
	{0x02, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x02, 0x01, 0x20, 0x20, "Off"			},
	{0x02, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Stage Select"		},
	{0x02, 0x01, 0x40, 0x40, "Off"			},
	{0x02, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Debug Mode"		},
	{0x02, 0x01, 0x80, 0x80, "Off"			},
	{0x02, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"		},
	{0x03, 0x01, 0x01, 0x00, "Off"			},
	{0x03, 0x01, 0x01, 0x01, "On"			},
};

STDDIPINFO(Captaven)

static struct BurnInputInfo FghthistInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Weak Punch",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Medium Punch",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Strong Punch",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Weak Kick",	BIT_DIGITAL,	DrvJoy2 + 8,	"p1 fire 4"	},
	{"P1 Medium Kick",	BIT_DIGITAL,	DrvJoy2 + 9,	"p1 fire 5"	},
	{"P1 Strong Kick",	BIT_DIGITAL,	DrvJoy2 + 10,	"p1 fire 6"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Weak Punch",	BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Medium Punch",	BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Strong Punch",	BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},
	{"P2 Weak Kick",	BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 4"	},
	{"P2 Medium Kick",	BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 5"	},
	{"P2 Strong Kick",	BIT_DIGITAL,	DrvJoy2 + 14,	"p2 fire 6"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		}, // +3!
};

STDINPUTINFO(Fghthist)

static struct BurnDIPInfo FghthistDIPList[]=
{
	{0x1a, 0xff, 0xff, 0xef, NULL				},
	{0x1b, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x1a, 0x01, 0x08, 0x08, "Off"				},
	{0x1a, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"			},
	{0x1b, 0x01, 0x01, 0x00, "Off"				},
	{0x1b, 0x01, 0x01, 0x01, "On"				},
};

STDDIPINFO(Fghthist)

static struct BurnInputInfo NslasherInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoyFS + 3,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy2 + 15,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p3 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		}, // + 3!
};

STDINPUTINFO(Nslasher)

static struct BurnDIPInfo NslasherDIPList[]=
{
	DIP_OFFSET(0x1d)
	{0x00, 0xff, 0xff, 0x08, NULL			},
	{0x01, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode "	},
	{0x00, 0x01, 0x08, 0x08, "Off"			},
	{0x00, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"			},
	{0x01, 0x01, 0x01, 0x00, "Off"				},
	{0x01, 0x01, 0x01, 0x01, "On"				},
};

STDDIPINFO(Nslasher)

static struct BurnInputInfo TattassInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy2 + 8,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy2 + 9,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy2 + 10,	"p1 fire 6"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	DrvJoy2 + 14,	"p2 fire 6"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 3,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		}, // + 3!
};

STDINPUTINFO(Tattass)

static struct BurnDIPInfo TattassDIPList[]=
{
	{0x1b, 0xff, 0xff, 0x00, NULL			},
	{0x1c, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"			},
	{0x1c, 0x01, 0x01, 0x00, "Off"				},
	{0x1c, 0x01, 0x01, 0x01, "On"				},
};

STDDIPINFO(Tattass)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo DragngunInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &DrvGun0,    "mouse x-axis"	),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &DrvGun1,    "mouse y-axis"	),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &DrvGun2,    "p2 x-axis"	),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &DrvGun3,    "p2 y-axis"	),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		}, // + 3!
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
};

STDINPUTINFO(Dragngun)
#undef A

static struct BurnDIPInfo DragngunDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xfe, NULL			},
	{0x0f, 0xff, 0xff, 0x00, NULL			},
	{0x10, 0xff, 0xff, 0x04, NULL			},

	{0   , 0xfe, 0   ,    2, "Reset"		},
	{0x0e, 0x01, 0x01, 0x00, "Off"			},
	{0x0e, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Stage Select"		},
	{0x0e, 0x01, 0x40, 0x40, "Off"			},
	{0x0e, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Debug Mode"		},
	{0x0e, 0x01, 0x80, 0x80, "Off"			},
	{0x0e, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"			},
	{0x0f, 0x01, 0x01, 0x00, "Off"				},
	{0x0f, 0x01, 0x01, 0x01, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x04, 0x04, "Off"			},
	{0x10, 0x01, 0x04, 0x00, "On"			},
};

STDDIPINFO(Dragngun)

static void DrvYM2151WritePort(UINT32, UINT32 data)
{
	MSM6295SetBank(0, DrvSndROM0 + (data & 0x01) * 0x40000, 0, 0x3ffff);
	MSM6295SetBank(1, DrvSndROM1 + (data & 0x02) * 0x20000, 0, 0x3ffff);
	DrvOkiBank = data;
}

static UINT8 deco32_sound_irq;
static UINT8 *deco32_sound_rom;

static void deco32_soundlatch_write(UINT16 data)
{
	deco16_soundlatch = data & 0xff;
	deco32_sound_irq |= 0x02;

	if (use_z80) {
		ZetSetIRQLine(0, (deco32_sound_irq != 0) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	} else {
		h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
	}
}

static void deco32_z80_YM2151_irq_handler(INT32 state)
{
	if (state) {
		deco32_sound_irq |= 0x01;
	} else {
		deco32_sound_irq &= ~0x01;
	}

	ZetSetIRQLine(0, (deco32_sound_irq != 0) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void __fastcall deco32_z80_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			BurnYM2151SelectRegister(data);
		return;

		case 0xa001:
			BurnYM2151WriteRegister(data);
		return;

		case 0xb000:
			MSM6295Write(0, data);
		return;

		case 0xc000:
			MSM6295Write(1, data);
		return;
	}
}

static UINT8 __fastcall deco32_z80_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
			return BurnYM2151Read();

		case 0xb000:
			return MSM6295Read(0);

		case 0xc000:
			return MSM6295Read(1);

		case 0xd000:
			deco32_sound_irq &= ~0x02;
			ZetSetIRQLine(0, (deco32_sound_irq != 0) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			return deco16_soundlatch;
	}

	return 0;
}

static UINT8 __fastcall deco32_z80_sound_read_port(UINT16 port)
{
	return deco32_sound_rom[port & 0xffff];
}

void deco32_z80_sound_reset()
{
	ZetOpen(0);
	ZetReset();
	BurnYM2151Reset();
	ZetClose();

	MSM6295Reset();

	deco32_sound_irq = 0;
	deco16_soundlatch = 0xff;
}

void deco32_z80_sound_init(UINT8 *rom, UINT8 *ram)
{
	use_z80 = 1;
//	INT32 z80_clock = 32220000/8;
	deco32_sound_rom = rom;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(rom,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(ram,	0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(deco32_z80_sound_write);
	ZetSetReadHandler(deco32_z80_sound_read);
	ZetSetInHandler(deco32_z80_sound_read_port);
	ZetClose();

	BurnYM2151InitBuffered(3580000, 1, NULL, 0);
	BurnYM2151SetAllRoutes(0.40, BURN_SND_ROUTE_BOTH);
	BurnYM2151SetIrqHandler(&deco32_z80_YM2151_irq_handler);
	BurnYM2151SetPortHandler(DrvYM2151WritePort);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.40, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.40, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachZet(3580000);

	MSM6295Init(0, (32220000 / 32) / MSM6295_PIN7_HIGH, 1);
	MSM6295Init(1, (32220000 / 16) / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.25, BURN_SND_ROUTE_BOTH);
}

void deco32_z80_sound_exit()
{
	ZetExit();

	BurnYM2151Exit();
	MSM6295Exit();

	MSM6295ROM = NULL;
	use_z80 = 0;
}

void deco32_z80_sound_update(INT16 *buf, INT32 len)
{
	BurnYM2151Render(buf, len);
	MSM6295Render(buf, len);
}

void deco32_z80_sound_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(deco16_soundlatch);
		SCAN_VAR(deco32_sound_irq);
	}
}

static void (*raster1_irq_cb)(INT32 param) = NULL;
static void (*raster2_irq_cb)(INT32 param) = NULL;
static void (*vblank_irq_cb)(INT32 param) = NULL;
static void (*lightgun_irq_cb)(INT32 param) = NULL;

static UINT8 raster_irq_target = 0;
static UINT8 raster_irq_masked = 0;
static UINT8 raster_irq = 0;
static UINT8 vblank_irq = 0;
static UINT8 lightgun_irq = 0;
static UINT8 raster_irq_scanline = 0;
static INT32 lightgun_latch = 0;

static void irq_callback(INT32 param)
{
	ArmSetIRQLine(ARM_IRQ_LINE, (param) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static UINT8 deco_irq_read(INT32 offset)
{
	switch (offset)
	{
		case 1:
			return raster_irq_scanline;

		case 2:
			raster_irq = 0;
			if(raster1_irq_cb)raster1_irq_cb(CLEAR_LINE);
			if(raster2_irq_cb)raster2_irq_cb(CLEAR_LINE);
			return 0xff;

		case 3:
			// ((totalcycles/fps)/scanlines)
			INT32 hblank = ((ArmGetTotalCycles() / 256) > 145833) ? 1 : 0;
			UINT8 data = 0;
			data |= 1 << 7;
			data |= (lightgun_irq ? 1 : 0) << 6;
			data |= (raster_irq ? 1 : 0) << 5;
			data |= (vblank_irq ? 1 : 0) << 4;
			data |= deco16_vblank << 1;
			data |= hblank << 0; // iq_132
			return data;
	}

	return 0xff;
}

static void deco_irq_write(INT32 offset, UINT8 data)
{
	switch (offset)
	{
		case 0:
		{
			raster_irq_target = (data & 0x10);
			raster_irq_masked = (data & 0x02);

			if (raster_irq_masked)
			{
				raster_irq = 0;
				if (raster1_irq_cb) raster1_irq_cb(CLEAR_LINE);
				if (raster2_irq_cb) raster2_irq_cb(CLEAR_LINE);
			}
		}
		return;

		case 1:
			raster_irq_scanline = data;
		return;

		case 2:
			vblank_irq = 0;
			if (vblank_irq_cb) vblank_irq_cb(CLEAR_LINE);
		return;
	}
}

static void deco_irq_scanline_callback(INT32 y)
{
	if (raster_irq_scanline > 0 && raster_irq_scanline < 240 && y == (raster_irq_scanline - 1))
	{
		if (!raster_irq_masked)
		{
			raster_irq = 1;

			switch (raster_irq_target)
			{
				case 1/*RASTER1_IRQ*/: if (raster1_irq_cb) raster1_irq_cb(ASSERT_LINE); break;
				case 0/*RASTER2_IRQ*/: if (raster2_irq_cb) raster2_irq_cb(ASSERT_LINE); break;
			}
		}
	}

	if (lightgun_latch >= 8 && lightgun_latch < (nScreenHeight+8) && y == lightgun_latch)
	{
		lightgun_irq = 1;
		if (lightgun_irq_cb) lightgun_irq_cb(ASSERT_LINE);
	}

	if (y == (nScreenHeight+8))
	{
		vblank_irq = 1;
		if (vblank_irq_cb) vblank_irq_cb(ASSERT_LINE);
	}
}

static inline void palette_write(UINT32 offs) // for captaven, only game to not use buffered palette
{
	offs = (offs & 0x1fff) / 4;

	UINT32 *p = (UINT32*)DrvPalBuf;

	UINT8 r = (p[offs] >>  0) & 0xff;
	UINT8 g = (p[offs] >>  8) & 0xff;
	UINT8 b = (p[offs] >> 16) & 0xff;

	DrvPalette[offs] = BurnHighCol(r,g,b,0);
}


static void captaven_write_byte(UINT32 address, UINT8 data)
{
	address &= 0xffffff;

	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static void captaven_write_long(UINT32 address, UINT32 data)
{
	address &= 0xffffff;

//	bprintf (0, _T("WL: %5.5x, %8.8x\n"), address, data);

	if (address >= 0x130000 && address <= 0x131fff) {
		UINT32 *p = (UINT32*)DrvPalBuf;
		p[(address & 0x1fff) / 4] = data;
		palette_write(address);
	}

	if (address >= 0x128000 && address <= 0x12ffff) {
		deco146_104_prot_ww(0, (address & 0x7ffc) >> 1, data);
		return;
	}

	Write16Long(DrvSprRAM,				0x110000, 0x111fff) // 16-bit
	Write16Long(((UINT8*)deco16_pf_control[0]),	0x180000, 0x18001f) // 16-bit
	Write16Long(deco16_pf_ram[0],			0x190000, 0x191fff) // 16-bit
	Write16Long(deco16_pf_ram[0],			0x192000, 0x193fff) // 16-bit (mirror)
	Write16Long(deco16_pf_ram[1],			0x194000, 0x195fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[0],		0x1a0000, 0x1a3fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[1],		0x1a4000, 0x1a5fff) // 16-bit
	Write16Long(((UINT8*)deco16_pf_control[1]),	0x1c0000, 0x1c001f) // 16-bit
	Write16Long(deco16_pf_ram[2],			0x1d0000, 0x1d1fff) // 16-bit
	Write16Long(deco16_pf_ram[3],			0x1d4000, 0x1d5fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[2],		0x1e0000, 0x1e3fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[3],		0x1e4000, 0x1e5fff) // 16-bit

	switch (address & ~3)
	{
		case 0x100000:
			memcpy (DrvSprBuf, DrvSprRAM, 0x1000);
		return;

		case 0x108000:
			// nop
		return;

		case 0x148000:
		case 0x148004:
		case 0x148008:
		case 0x14800c:
			deco_irq_write((address / 4) & 3, data & 0xff);
		return;

		case 0x178000:
			global_priority = data & 3;
		return;
	}
}

static UINT8 captaven_read_byte(UINT32 address)
{
	switch (address & 0xffffff)
	{
		case 0x168000:
		case 0x168001:
		case 0x168002:
			return DrvDips[address & 3];

		case 0x168003:
			return 0xff; // sound cpu
	}

	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static UINT32 captaven_read_long(UINT32 address)
{
	address &= 0xffffff;

//	bprintf (0, _T("RW: %5.5x\n"), address);
	if (address >= 0x130000 && address <= 0x131fff) {
		UINT32 *p = (UINT32*)DrvPalBuf;
		return p[(address & 0x1fff) / 4];
	}

	if (address >= 0x128000 && address <= 0x12ffff) {
		UINT16 ret = deco146_104_prot_rw(0, (address & 0x7ffc) >> 1);
		return ret + (ret << 16);
	}

	Read16Long(DrvSprRAM,				0x110000, 0x111fff) // 16-bit
	Read16Long(((UINT8*)deco16_pf_control[0]),	0x180000, 0x18001f) // 16-bit
	Read16Long(deco16_pf_ram[0],			0x190000, 0x191fff) // 16-bit
	Read16Long(deco16_pf_ram[0],			0x192000, 0x193fff) // 16-bit (mirror)
	Read16Long(deco16_pf_ram[1],			0x194000, 0x195fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[0],		0x1a0000, 0x1a3fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[1],		0x1a4000, 0x1a5fff) // 16-bit
	Read16Long(((UINT8*)deco16_pf_control[1]),	0x1c0000, 0x1c001f) // 16-bit
	Read16Long(deco16_pf_ram[2],			0x1d0000, 0x1d1fff) // 16-bit
	Read16Long(deco16_pf_ram[3],			0x1d4000, 0x1d5fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[2],		0x1e0000, 0x1e3fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[3],		0x1e4000, 0x1e5fff) // 16-bit

	switch (address & ~3)
	{
		case 0x100000:
		case 0x100004: // 71_r
			return 0xffffffff;

		case 0x148000:
		case 0x148004:
		case 0x148008:
		case 0x14800c:
			return deco_irq_read((address / 4) & 3);

		case 0x168000:
			return (DrvDips[0]) + (DrvDips[1] << 8) + (DrvDips[2] << 16) + (0xff << 24);
	}

	return 0;
}

static INT32 m_bufPtr = 0;
static INT32 m_pendingCommand = 0;
static INT32 m_readBitCount = 0;
static INT32 m_byteAddr = 0;
static INT32 m_lastClock = 0;
static UINT8 m_buffer[0x20];
static UINT8 m_eeprom[0x400];
static UINT8 m_tattass_eprom_bit = 0;

static void tattass_eeprom_scan()
{
	SCAN_VAR(m_bufPtr);
	SCAN_VAR(m_pendingCommand);
	SCAN_VAR(m_readBitCount);
	SCAN_VAR(m_byteAddr);
	SCAN_VAR(m_lastClock);
	SCAN_VAR(m_buffer);
	SCAN_VAR(m_eeprom);
	SCAN_VAR(m_tattass_eprom_bit);
}

static void tattass_control_write(UINT32 data)
{
	/* Eprom in low byte */
	{
		if ((data&0x40)==0) {
			m_bufPtr=0;
			m_pendingCommand=0;
			m_readBitCount=0;
		}

		/* Eprom has been clocked */
		if (m_lastClock==0 && data&0x20 && data&0x40) {
			/* Handle pending read */
			if (m_pendingCommand==1) {
				INT32 d=m_readBitCount/8;
				INT32 m=7-(m_readBitCount%8);
				INT32 a=(m_byteAddr+d)%1024;
				INT32 b=m_eeprom[a];

				m_tattass_eprom_bit=(b>>m)&1;

				m_readBitCount++;
				m_lastClock=data&0x20;
				return;
			}

			/* Handle pending write */
			if (m_pendingCommand==2) {
				m_buffer[m_bufPtr++]=(data&0x10)>>4;

				if (m_bufPtr==32) {
					INT32 b=(m_buffer[24]<<7)|(m_buffer[25]<<6)|(m_buffer[26]<<5)|(m_buffer[27]<<4)
						|(m_buffer[28]<<3)|(m_buffer[29]<<2)|(m_buffer[30]<<1)|(m_buffer[31]<<0);

					m_eeprom[m_byteAddr] = b;
				}
				m_lastClock=data&0x20;
				return;
			}

			m_buffer[m_bufPtr++]=(data&0x10)>>4;
			if (m_bufPtr==24) {
				/* Decode addr */
				m_byteAddr=(m_buffer[3]<<9)|(m_buffer[4]<<8)
						|(m_buffer[16]<<7)|(m_buffer[17]<<6)|(m_buffer[18]<<5)|(m_buffer[19]<<4)
						|(m_buffer[20]<<3)|(m_buffer[21]<<2)|(m_buffer[22]<<1)|(m_buffer[23]<<0);

				/* Check for read command */
				if (m_buffer[0] && m_buffer[1]) {
					m_tattass_eprom_bit=(m_eeprom[m_byteAddr]>>7)&1;
					m_readBitCount=1;
					m_pendingCommand=1;
				}

				/* Check for write command */
				else if (m_buffer[0]==0x0 && m_buffer[1]==0x0) {
					m_pendingCommand=2;
				}

			}

		} else {
			if (!(data&0x40)) {
				m_bufPtr=0;
			}
		}

		m_lastClock=data&0x20;
	}

	if (data & 0x80) {
		bsmt_in_reset = 0;
	} else {
		M6809Open(0);
		decobsmt_reset_line(1);
		M6809Close();
		bsmt_in_reset = 1;
	}
}

static UINT16 tattass_read_B()
{
	return m_tattass_eprom_bit;
}


static void fghthist_write_byte(UINT32 address, UINT8 data)
{
	address &= 0xffffff;

	if (address >= 0x200000 && address <= 0x207fff) {
		deco146_104_prot_wb(0, ((address & 0x7ffc) >> 1) | (address & 1), data);
		return;
	}

	switch (address)
	{
		case 0x1201fc:
			deco32_soundlatch_write(data);
		return;

		case 0x150000:
			if (game_select == 3) {
				tattass_control_write(data);
			}
		return;

		case 0x120000:
		case 0x120001:
		case 0x120002:
		case 0x120003: // tattass nop's
		case 0x150001: // tattass vol (unused)
		return;
	}

	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static void fghthist_write_long(UINT32 address, UINT32 data)
{
	address &= 0xffffff;

	if (address >= 0x200000 && address <= 0x207fff) {
		deco146_104_prot_ww(0, (address & 0x7ffc) >> 1, data >> 16);
		return;
	}

	if (game_select == 3 && address >= 0xf8000 && address <= 0xfffff) {
		return; // nop;
	}

	Write16Long(DrvSprRAM2,				0x170000, 0x171fff) // 16-bit - nslasher

	Write16Long(DrvSprRAM,				0x178000, 0x179fff) // 16-bit
	Write16Long(deco16_pf_ram[0],			0x182000, 0x183fff) // 16-bit
	Write16Long(deco16_pf_ram[1],			0x184000, 0x185fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[0],		0x192000, 0x193fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[1],		0x194000, 0x195fff) // 16-bit
	Write16Long(((UINT8*)deco16_pf_control[0]),	0x1a0000, 0x1a001f) // 16-bit
	Write16Long(deco16_pf_ram[2],			0x1c2000, 0x1c3fff) // 16-bit
	Write16Long(deco16_pf_ram[3],			0x1c4000, 0x1c5fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[2],		0x1d2000, 0x1d3fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[3],		0x1d4000, 0x1d5fff) // 16-bit
	Write16Long(((UINT8*)deco16_pf_control[1]),	0x1e0000, 0x1e001f) // 16-bit

	switch (address & ~3)
	{
		case 0x1201fc: deco32_soundlatch_write(data); return;
		case 0x12002c: if (game_select != 1) return; // fghthist only (ignore compiler warning -dink)
		case 0x150000: // fghthist / nslasher / tattass
			if (game_select == 3) {
				tattass_control_write(data);
				global_priority = (data & 3) | (~data & 4); // 3rd bit is inverted in tattass
			} else {
				EEPROMWrite(data & 0x20, data & 0x40, data & 0x10);
				global_priority = data & 7;
			}
		return;

		case 0x140000:
			ArmSetIRQLine(ARM_IRQ_LINE, CPU_IRQSTATUS_NONE);
		return;

		case 0x164000: // tmap
		case 0x164004: // spr0
		case 0x164008: // spr1
			color_base[(address & 0xf) / 4] = (data & 7) << 8;
		return;

		case 0x130000:
		case 0x148000:
		case 0x16400c:
		case 0x16c000:
		case 0x16c00c:
		case 0x174000:
		case 0x17c000:
		case 0x17c018:
		case 0x17a000:
		case 0x17a004:
		case 0x17a008:
		case 0x17a00c:
		case 0x20c800:
			return; // nops (tattass, fghthist, nslasher, etc)

		case 0x16c008:
			memcpy (DrvPalBuf, DrvPalRAM, 0x2000);
		return;

		case 0x174010: // nslasher2
			memcpy (DrvSprBuf2, DrvSprRAM2, 0x1000);
		return;

		case 0x17c010:
			memcpy (DrvSprBuf, DrvSprRAM, 0x1000);
		return;

		case 0x208800:
			// nop
		return;
	}

	bprintf (0, _T("WL: %5.5x, %8.8x\n"), address, data);
}

static UINT8 fghthist_read_byte(UINT32 address)
{
	if (address >= 0x200000 && address <= 0x207fff) {
		return deco146_104_prot_rb(0, ((address & 0x7ffc) >> 1) | (address & 1));
	}

	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static UINT32 fghthist_read_long(UINT32 address)
{
//	bprintf (0, _T("RL: %5.5x\n"), address);

	if (address >= 0x200000 && address <= 0x207fff) {
		return (deco146_104_prot_rw(0, (address & 0x7ffc) >> 1) << 16) | 0xffff;
	}

	Read16Long(DrvSprRAM2,				0x170000, 0x171fff) // 16-bit - nslasher

	Read16Long(DrvSprRAM,				0x178000, 0x179fff) // 16-bit
	Read16Long(deco16_pf_ram[0],			0x182000, 0x183fff) // 16-bit
	Read16Long(deco16_pf_ram[1],			0x184000, 0x185fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[0],		0x192000, 0x193fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[1],		0x194000, 0x195fff) // 16-bit
	Read16Long(((UINT8*)deco16_pf_control[0]),	0x1a0000, 0x1a001f) // 16-bit
	Read16Long(deco16_pf_ram[2],			0x1c2000, 0x1c3fff) // 16-bit
	Read16Long(deco16_pf_ram[3],			0x1c4000, 0x1c5fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[2],		0x1d2000, 0x1d3fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[3],		0x1d4000, 0x1d5fff) // 16-bit
	Read16Long(((UINT8*)deco16_pf_control[1]),	0x1e0000, 0x1e001f) // 16-bit

	switch (address & ~3)
	{
		case 0x120020:
			return DrvInputs[0];

		case 0x120024:
			return (DrvInputs[1] & ~0x10) | ((deco16_vblank) ? 0x10 : 0);

		case 0x120028:
			return (EEPROMRead() & 1) ? 0xff : 0xfe;

		case 0x16c000:
		case 0x17c000:
			return 0; // nops
	}

	return 0;
}



#if 0

static void lockload_write_byte(UINT32 address, UINT8 data)
{
	address &= 0xffffff;

	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static void lockload_write_long(UINT32 address, UINT32 data)
{
	address &= 0xffffff;

	if (address >= 0x120000  && address <= 0x127fff) {
		deco146_104_prot_ww(0, (address & 0x7ffc) >> 1, data >> 16);
		return;
	}

	Write16Long(((UINT8*)deco16_pf_control[0]),	0x180000, 0x18001f) // 16-bit
	Write16Long(deco16_pf_ram[0],			0x190000, 0x191fff) // 16-bit
	Write16Long(deco16_pf_ram[1],			0x194000, 0x195fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[0],		0x1a0000, 0x1a3fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[1],		0x1a4000, 0x1a5fff) // 16-bit
	Write16Long(((UINT8*)deco16_pf_control[1]),	0x1c0000, 0x1c001f) // 16-bit
	Write16Long(deco16_pf_ram[2],			0x1d0000, 0x1d1fff) // 16-bit
	Write16Long(deco16_pf_ram[3],			0x1d4000, 0x1d5fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[2],		0x1e0000, 0x1e3fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[3],		0x1e4000, 0x1e5fff) // 16-bit

	switch (address & ~3)
	{
		case 0x128000:
		case 0x128004:
		case 0x128008:
		case 0x12800c:
			deco_irq_write((address / 4) & 3, data & 0xff);
		return;

		case 0x138008:
			memcpy (DrvPalBuf, DrvPalRAM, 0x2000);
		return;

		case 0x230000:
			memcpy (DrvSprBuf, DrvSprRAM, 0x2000);
		return;

		case 0x410000:
			// volume
		return;

		case 0x420000:
			EEPROMWrite(data & 0x2, data & 0x4, data & 0x1);
		return;

		case 0x500000:
			sprite_ctrl = data;
		return;

		case 0x140000:
		//	ArmSetIRQLine(ARM_IRQ_LINE, CPU_IRQSTATUS_NONE); // 
//	AM_RANGE(0x178008, 0x17800f) AM_WRITE(gun_irq_ack_w) /* Gun read ACK's */
		return;

	}

	bprintf (0, _T("WL: %5.5x, %8.8x\n"), address, data);
}

static UINT8 lockload_read_byte(UINT32 address)
{
	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static UINT32 lockload_read_long(UINT32 address)
{
//	bprintf (0, _T("RW: %5.5x\n"), address);

	if (address >= 0x120000  && address <= 0x127fff) {
		return (deco146_104_prot_rw(0, (address & 0x7ffc) >> 1) << 16) | 0xffff;
	}

	Read16Long(((UINT8*)deco16_pf_control[0]),	0x180000, 0x18001f) // 16-bit
	Read16Long(deco16_pf_ram[0],			0x190000, 0x191fff) // 16-bit
	Read16Long(deco16_pf_ram[1],			0x194000, 0x195fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[0],		0x1a0000, 0x1a3fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[1],		0x1a4000, 0x1a5fff) // 16-bit
	Read16Long(((UINT8*)deco16_pf_control[1]),	0x1c0000, 0x1c001f) // 16-bit
	Read16Long(deco16_pf_ram[2],			0x1d0000, 0x1d1fff) // 16-bit
	Read16Long(deco16_pf_ram[3],			0x1d4000, 0x1d5fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[2],		0x1e0000, 0x1e3fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[3],		0x1e4000, 0x1e5fff) // 16-bit

	switch (address & ~3)
	{
		case 0x128000:
		case 0x128004:
		case 0x128008:
		case 0x12800c:
			return deco_irq_read((address / 4) & 3);

		case 0x420000:
			return (EEPROMRead() & 1) ? 0xff : 0xfe;

		case 0x440000:
			return 0xe | (deco16_vblank ? 1 : 0);


		case 0x170000:
		case 0x170004:
			return ~0;
	}

	return 0;
}

#endif


static void dragngun_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0x1000000 && address <= 0x1001000) {
		*((UINT32*)(DrvDVIRAM0 + ((address & 0xfff) ^ 3))) = data;
		return;
	}

	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static void dragngun_write_long(UINT32 address, UINT32 data)
{
	if (address >= 0x120000 && address <= 0x127fff) {
		deco146_104_prot_ww(0, (address & 0x7ffc) >> 1, data);
		return;
	}

	Write16Long(((UINT8*)deco16_pf_control[0]),	0x180000, 0x18001f) // 16-bit
	Write16Long(deco16_pf_ram[0],			0x190000, 0x191fff) // 16-bit
	Write16Long(deco16_pf_ram[1],			0x194000, 0x195fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[0],		0x1a0000, 0x1a3fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[1],		0x1a4000, 0x1a5fff) // 16-bit
	Write16Long(((UINT8*)deco16_pf_control[1]),	0x1c0000, 0x1c001f) // 16-bit
	Write16Long(deco16_pf_ram[2],			0x1d0000, 0x1d1fff) // 16-bit
	Write16Long(deco16_pf_ram[3],			0x1d4000, 0x1d5fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[2],		0x1e0000, 0x1e3fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[3],		0x1e4000, 0x1e5fff) // 16-bit

	if (address >= 0x1000000 && address <= 0x1001000) {
		*((UINT32*)(DrvDVIRAM0 + ((address & 0xfff) / 4))) = data;
		return;
	}

	switch (address & ~3)
	{
		case 0x128000:
		case 0x128004:
		case 0x128008:
		case 0x12800c:
			deco_irq_write((address / 4) & 3, data & 0xff);
		return;

		case 0x138000:
		case 0x13800c:
		case 0x140200:
		case 0x140400:
		case 0x140800:
		case 0x140a00:
		case 0x140c00:
		case 0x150000:
		case 0x158000:
		case 0x160000:
		case 0x280000:
		case 0x280004:
		case 0x280008:
		case 0x28000c:
		case 0x234000:
		case 0x408000:
		return; // nop

		case 0x138008:
			memcpy (DrvPalBuf, DrvPalRAM, 0x2000);
		return;

		case 0x230000:
			memcpy (DrvSprBuf, DrvSprRAM + 0x20000, 0x2000);
			memset (DrvSprRAM + 0x20000, 0, 0x2000);
		return;

		case 0x400000:
			MSM6295Write(2, data & 0xff);
		return;

		case 0x410000:
			// volume_w
		return;

		case 0x418000:
			// speaker_switch_w
		return;

		case 0x420000:
			EEPROMWrite(data & 0x02, data & 0x04, data & 0x01);
		return;

		case 0x430000:
		case 0x430004:
		case 0x430008:
		case 0x43000c:
		case 0x430010:
		case 0x430014:
		case 0x430018:
		case 0x43001c:
			lightgun_port = (address / 4) & 7;
		return;

		case 0x500000:
			sprite_ctrl = data;
		return;
	}
	if ((address & 0xfff0000) == 0x170000) return; // more nops

	bprintf (0, _T("WL: %5.5x, %8.8x\n"), address, data);
}

static UINT8 dragngun_read_byte(UINT32 address)
{
	if (address >= 0x120000 && address <= 0x127fff) {
		return deco146_104_prot_rb(0, ((address & 0x7ffc) >> 1) | (address & 1));
	}

	switch (address)
	{
		case 0x440000:
			return (deco16_vblank ? 0xfb : 0xfa) | (DrvDips[4] & 0x04); // service & 4

		case 0x438000:
			switch (lightgun_port) {
				case 4: return BurnGunReturnX(0);
				case 5: return BurnGunReturnX(1);
				case 6: return BurnGunReturnY(0);
				case 7: return BurnGunReturnY(1);
			}

			return 0;
	}

	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static UINT32 dragngun_read_long(UINT32 address)
{
	if (address >= 0x120000 && address <= 0x127fff) {
		return deco146_104_prot_rw(0, (address & 0x7ffc) >> 1);
	}

	Read16Long(((UINT8*)deco16_pf_control[0]),	0x180000, 0x18001f) // 16-bit
	Read16Long(deco16_pf_ram[0],			0x190000, 0x191fff) // 16-bit
	Read16Long(deco16_pf_ram[1],			0x194000, 0x195fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[0],		0x1a0000, 0x1a3fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[1],		0x1a4000, 0x1a5fff) // 16-bit
	Read16Long(((UINT8*)deco16_pf_control[1]),	0x1c0000, 0x1c001f) // 16-bit
	Read16Long(deco16_pf_ram[2],			0x1d0000, 0x1d1fff) // 16-bit
	Read16Long(deco16_pf_ram[3],			0x1d4000, 0x1d5fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[2],		0x1e0000, 0x1e3fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[3],		0x1e4000, 0x1e5fff) // 16-bit

	if (address >= 0x1000008 && address <= 0x1001000) {
		return *((UINT32*)(DrvDVIRAM0 + (address & 0xfff)));
	}

	switch (address & ~3)
	{
		case 0x128000:
		case 0x128004:
		case 0x128008:
		case 0x12800c:
			return deco_irq_read((address / 4) & 3);

		case 0x138000: // nop
			return 0;

		case 0x420000:
			return 0xfffffffe | (EEPROMRead() & 1);

		case 0x400000:
			return MSM6295Read(2);

		case 0x438000:
			switch (lightgun_port) {
				case 4: return BurnGunReturnX(0);
				case 5: return BurnGunReturnX(1);
				case 6: return BurnGunReturnY(0);
				case 7: return BurnGunReturnY(1);
			}

			return 0;

		case 0x440000:
			return (deco16_vblank ? 0xfb : 0xfa) | (DrvDips[4] & 0x04); // service & 4

		case 0x1000000:
		case 0x1000004:
			return BurnRandom();

	}
	bprintf (0, _T("RL: %5.5x\n"), address);
	return 0;
}

static void pCommonSpeedhackCallback()
{
	ArmIdleCycles(1120); // git r dun!
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ArmOpen(0);
	ArmReset();
	if (DrvDips[3] & 1) {
		bprintf(0, _T("Speedhack Enabled for 0x%x.\n"), speedhack_address);
		ArmSetSpeedHack(speedhack_address ? speedhack_address : ~0, pCommonSpeedhackCallback);
	} else {
		bprintf(0, _T("Speedhack Disabled.\n"));
		ArmSetSpeedHack(~0, NULL);
	}

	ArmClose();

	if (use_bsmt)
	{
		bsmt_in_reset = 0;
		decobsmt_reset();
		M6809Open(0);
		M6809Reset();
		M6809Close();
	}
	else if (use_z80)
	{
		deco32_z80_sound_reset();
	}
	else
	{
		deco16SoundReset();
	}

	if (game_select != 3) DrvYM2151WritePort(0, 0);

	EEPROMReset();

	deco16Reset();

	global_priority = 0;
	sprite_ctrl = 0;
	lightgun_port = 0;

	raster_irq_target = 0;
	raster_irq_masked = 0;
	raster_irq = 0;
	vblank_irq = 0;
	lightgun_irq = 0;
	raster_irq_scanline = 0;
	lightgun_latch = 0;

	nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = 0;

	HiscoreReset();

	return 0;
}

static INT32 gfxlen[5];
static INT32 sndlen[3];

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvARMROM	= Next; Next += 0x100000;
	if (game_select == 4) Next += 0x100000 /*drgngun*/;
	DrvHucROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += gfxlen[0];
	DrvGfxROM1	= Next; Next += gfxlen[1];
	DrvGfxROM2	= Next; Next += gfxlen[2];
	DrvGfxROM3	= Next; Next += gfxlen[3];
	DrvGfxROM4	= Next; Next += gfxlen[4];

	DrvSndROM0	= Next; Next += sndlen[0];
	DrvSndROM1	= Next; Next += sndlen[1];
	DrvSndROM2	= Next; Next += sndlen[2];

	DrvTMSROM	= Next; Next += 0x002000;

	if (game_select == 4)
	{
		DrvDVIROM   = Next; Next += 0x1000000;
	}

	DrvPalette	= (UINT32*)Next; Next += 0x801 * 2 * sizeof(UINT32);

	AllRam		= Next;

	DrvSysRAM	= Next; Next += 0x020000;
	DrvHucRAM	= Next; Next += 0x002000;
	DrvSprRAM	= Next; Next += 0x002000 + 0x26400 /*drgngun*/;
	DrvSprBuf	= Next; Next += 0x002000;
	DrvPalRAM	= Next; Next += 0x002000;
	DrvPalBuf	= Next; Next += 0x002000;

	DrvAceRAM	= Next; Next += 0x000400; // 0xa0
	DrvSprRAM2	= Next; Next += 0x001000;
	DrvSprBuf2	= Next; Next += 0x001000;
	DrvTMSRAM	= Next; Next += 0x000200; // 0x100 words!
	DrvJackRAM	= Next; Next += 0x001000;
	DrvDVIRAM0  = Next; Next += 0x008000;
	DrvDVIRAM1  = Next; Next += 0x000200;

	RamEnd		= Next;

	if (game_select == 2 || game_select == 3) // nslasher, tattass
	{
		pTempDraw[0]	= (UINT16*)Next; Next += nScreenWidth * nScreenHeight * sizeof(INT16);
		pTempDraw[1]	= (UINT16*)Next; Next += nScreenWidth * nScreenHeight * sizeof(INT16);
		pTempDraw[2]	= (UINT16*)Next; Next += nScreenWidth * nScreenHeight * sizeof(INT16);
		pTempDraw[3]	= (UINT16*)Next; Next += nScreenWidth * nScreenHeight * sizeof(INT16);
	}

	if (game_select == 4) // dragngun
	{
		pTempSprite    = (UINT32*)Next; Next += nScreenWidth * nScreenHeight * sizeof(UINT32);
	}

	MemEnd		= Next;

	return 0;
}

static UINT16 fghthist_read_A()
{
	return DrvInputs[0];
}

static UINT16 fghthist_read_B()
{
	return EEPROMRead() ? 0xff : 0xfe;
}

static UINT16 fghthist_read_C()
{
	return (DrvInputs[1] & ~0x10) | (deco16_vblank ? 0x10 : 0);
}

static INT32 fghthist_bank_callback(const INT32 data)
{
	INT32 bank = ((data & 0x10) >> 4) | ((data & 0x40) >> 5) | ((data & 0x20) >> 3);

	return bank * 0x1000;
}

static INT32 FghthistCommonInit(INT32 z80_sound, UINT32 speedhack)
{
	game_select = 1;
	speedhack_address = speedhack;

	BurnSetRefreshRate(57.79965);

	gfxlen[0] = 0x200000;
	gfxlen[1] = 0x200000;
	gfxlen[2] = 0x200000;
	gfxlen[3] = 0x1000000;
	gfxlen[4] = 0;
	sndlen[0] = 0x080000;
	sndlen[1] = 0x080000;
	sndlen[2] = 0;

	BurnAllocMemIndex();

	{
		if (BurnLoadRomExt(DrvARMROM + 0x000000,  0, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvARMROM + 0x000002,  1, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvHucROM,		  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1,		  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2,		  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000,	  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x000001,	  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x400000,	  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x400001,	  8, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000,	  9, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000,	 10, 1)) return 1;
	
		deco56_decrypt_gfx(DrvGfxROM1, 0x100000);
		deco74_decrypt_gfx(DrvGfxROM2, 0x100000);

		deco16_tile_decode(DrvGfxROM1, DrvGfxROM0, 0x0100000, 1);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM1, 0x0100000, 0);
		deco16_tile_decode(DrvGfxROM2, DrvGfxROM2, 0x0100000, 0);
		deco16_sprite_decode(DrvGfxROM3, 0x800000);
	}

	ArmInit(0);
	ArmOpen(0);	
	ArmMapMemory(DrvARMROM,		0x000000, 0x0fffff, MAP_ROM);
	ArmMapMemory(DrvSysRAM,		0x100000, 0x11ffff, MAP_RAM); // 32-bit
	ArmMapMemory(DrvPalRAM,		0x168000, 0x169fff, MAP_RAM); // 32-bit
	ArmSetWriteByteHandler(fghthist_write_byte);
	ArmSetWriteLongHandler(fghthist_write_long);
	ArmSetReadByteHandler(fghthist_read_byte);
	ArmSetReadLongHandler(fghthist_read_long);
	ArmClose();

	EEPROMInit(&eeprom_interface_93C46);

	deco_146_init();
	deco_146_104_set_port_a_cb(fghthist_read_A); // inputs 0
	deco_146_104_set_port_b_cb(fghthist_read_B); // eeprom
	deco_146_104_set_port_c_cb(fghthist_read_C); // inputs 1
	deco_146_104_set_soundlatch_cb(deco32_soundlatch_write);

	deco_146_104_set_interface_scramble_interleave();
	deco_146_104_set_use_magic_read_address_xor(1);

	deco16Init(0, 0, 1);
	deco16_set_graphics(DrvGfxROM0, 0x100000 * 2, DrvGfxROM1, 0x100000 * 2, DrvGfxROM2, 0x100000 * 2);
	deco16_set_color_base(0, 0x000);
	deco16_set_color_base(1, 0x100);
	deco16_set_color_base(2, 0x200);
	deco16_set_color_base(3, 0x300);
	deco16_set_global_offsets(0, 8);
	deco16_set_bank_callback(0, fghthist_bank_callback);
	deco16_set_bank_callback(1, fghthist_bank_callback);
	deco16_set_bank_callback(2, fghthist_bank_callback);
	deco16_set_bank_callback(3, fghthist_bank_callback);

	if (z80_sound)
	{
		deco32_z80_sound_init(DrvHucROM, DrvHucRAM); // re-use these for z80
	}
	else
	{
		use_z80 = 0;
		deco16SoundInit(DrvHucROM, DrvHucRAM, 4027500, 0, DrvYM2151WritePort, 0.42, 1006875, 1.00, 2013750, 0.35);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.80, BURN_SND_ROUTE_LEFT);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.80, BURN_SND_ROUTE_RIGHT);
	}

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static UINT16 captaven_read_A()
{
	return DrvInputs[0];
}

static UINT16 captaven_read_B()
{
	return DrvInputs[2] & 0xf;
}

static UINT16 captaven_read_C()
{
	return DrvInputs[1];
}

static INT32 captaven_bank_callback(const INT32 data)
{
	return (data & 0x20) << 9;
}

static void decode_8bpp_tiles(UINT8 *dst, INT32 len)
{
	INT32 frac = (len / 4) * 8;

	INT32 Plane[8]  = { (frac*3)+8, frac*3, (frac*2)+8, frac*2, frac+8, frac, 8, 0 };
	INT32 XOffs[16] = { STEP8(256, 1), STEP8(0,1) };
	INT32 YOffs[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);

	memcpy (tmp, dst, len);

	GfxDecode(len / (16 * 16), 8, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, dst);

	BurnFree(tmp);
}

static INT32 CaptavenCommonInit(INT32 has_z80, UINT32 speedhack)
{
	game_select = 0;
	speedhack_address = speedhack;

	BurnSetRefreshRate(57.79965);

	gfxlen[0] = 0x100000;
	gfxlen[1] = 0x100000;
	gfxlen[2] = 0x500000;
	gfxlen[3] = 0x800000;
	gfxlen[4] = 0;
	sndlen[0] = 0x080000;
	sndlen[1] = 0x080000;
	sndlen[2] = 0;

	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvARMROM + 0x000000,     0, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x000001,     1, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x000002,     2, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x000003,     3, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x080000,     4, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x080001,     5, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x080002,     6, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x080003,     7, 4)) return 1;

		if (BurnLoadRom(DrvHucROM,		  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1,		  9, 1)) return 1;

		for (INT32 i = 0; i < 5; i++)
		{
			if (BurnLoadRom(DrvGfxROM0, 10 + i, 1)) return 1;
	
			memcpy (DrvGfxROM2 + (i * 0x40000) + 0x000000, DrvGfxROM0 + 0x000000, 0x040000);
			memcpy (DrvGfxROM2 + (i * 0x40000) + 0x140000, DrvGfxROM0 + 0x040000, 0x040000);
			memcpy (DrvGfxROM2 + (i * 0x40000) + 0x280000, DrvGfxROM0 + 0x080000, 0x040000);
			memcpy (DrvGfxROM2 + (i * 0x40000) + 0x3c0000, DrvGfxROM0 + 0x0c0000, 0x040000);
		}

		if (BurnLoadRom(DrvGfxROM3 + 0x000001,	 15, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x000000,	 16, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x200001,	 17, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x200000,	 18, 2)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000,	 19, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000,	 20, 1)) return 1;

		deco56_decrypt_gfx(DrvGfxROM1, 0x080000);
		deco56_decrypt_gfx(DrvGfxROM2, 0x500000);

		deco16_tile_decode(DrvGfxROM1, DrvGfxROM0, 0x080000, 1);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM1, 0x080000, 0);
		decode_8bpp_tiles(DrvGfxROM2, 0x500000);
		deco16_sprite_decode(DrvGfxROM3, 0x400000);
	}

	ArmInit(0);
	ArmOpen(0);	
	ArmMapMemory(DrvARMROM,			0x000000, 0x0fffff, MAP_ROM);
	ArmMapMemory(DrvSysRAM,			0x120000, 0x127fff, MAP_RAM); // 32-bit
//	ArmMapMemory(DrvPalBuf,			0x130000, 0x131fff, MAP_RAM); // 32-bit (point to buffer as this isn't buffered)
	ArmMapMemory(DrvSysRAM + 0x8000,	0x160000, 0x167fff, MAP_RAM); // 32-bit
	ArmSetWriteByteHandler(captaven_write_byte);
	ArmSetWriteLongHandler(captaven_write_long);
	ArmSetReadByteHandler(captaven_read_byte);
	ArmSetReadLongHandler(captaven_read_long);
	ArmClose();

	vblank_irq_cb = irq_callback;
	raster2_irq_cb = irq_callback;

	EEPROMInit(&eeprom_interface_93C46); // not used on this pcb

	deco_146_init();
	deco_146_104_set_port_a_cb(captaven_read_A); // inputs
	deco_146_104_set_port_b_cb(captaven_read_B); // system
	deco_146_104_set_port_c_cb(captaven_read_C); // dsw
	deco_146_104_set_soundlatch_cb(deco32_soundlatch_write);

	deco16Init(0, 0, 1|2);
	deco16_set_graphics(DrvGfxROM0, 0x080000 * 2, DrvGfxROM1, 0x080000 * 2, DrvGfxROM2, 0x500000 * 1);
	deco16_set_color_base(0, 0x200);
	deco16_set_color_base(1, 0x300);
	deco16_set_color_base(2, 0x100 << 4);
	deco16_set_color_base(3, 0x000);
	deco16_set_color_mask(2, 0xf);
	deco16_set_color_mask(3, 0);
	deco16_set_global_offsets(0, 8);
	deco16_set_bank_callback(2, captaven_bank_callback);

	deco16SoundInit(DrvHucROM, DrvHucRAM, 2685000, 0, DrvYM2151WritePort, 0.80, 1006875, 1.40, 2013750, 0.30);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.80, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.80, BURN_SND_ROUTE_RIGHT);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static void nslasher_sprite_decode(UINT8 *src, UINT8 *dst, INT32 len, INT32 top)
{
	for (INT32 i = 0; i < len * 8; i++)
	{
		INT32 shift = 0;
		INT32 addr = (~i & 7) | ((i & 0x1e0) >> 1) | ((~i & 0x200) >> 6) | ((i & ~0x3ff) >> 2);

		if (top)
		{
			shift = ((i >> 4) & 1) | ((~i & 8) >> 2);
		}
		else
		{
			shift = 4;
			if (i & 0x18) { i |= 0x1f; continue; } // these bytes empty
		}

		dst[addr] |= ((src[i/8] >> (i & 7)) & 1) << shift;
	}
}

static INT32 tattass_bank_callback(const INT32 data)
{
	return (data & 0xf0) * 0x100;
}

static INT32 NslasherCommonInit(INT32 has_z80, UINT32 speedhack)
{
	game_select = 2;
	has_ace = 1;
	speedhack_address = speedhack;

	BurnSetRefreshRate(58.464346);

	GenericTilesInit(); // for allocating memory for pTempDraw;

	gfxlen[0] = 0x400000;
	gfxlen[1] = 0x400000;
	gfxlen[2] = 0x400000;
	gfxlen[3] = 0x1000000;
	gfxlen[4] = 0x200000;
	sndlen[0] = 0x080000;
	sndlen[1] = 0x080000;
	sndlen[2] = 0;

	BurnAllocMemIndex();

	{
		if (BurnLoadRomExt(DrvARMROM + 0x000000,  0, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvARMROM + 0x000002,  1, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvHucROM,		  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1,		  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2,		  4, 1)) return 1;

		{
			UINT8 *tmp = DrvGfxROM0;
			memcpy (tmp + 0x000000, DrvGfxROM1 + 0x080000, 0x080000);
			memcpy (DrvGfxROM1 + 0x080000, DrvGfxROM1 + 0x100000, 0x080000);
			memcpy (DrvGfxROM1 + 0x100000, tmp + 0x000000, 0x080000);

			memcpy (tmp + 0x000000, DrvGfxROM2 + 0x080000, 0x080000);
			memcpy (DrvGfxROM2 + 0x080000, DrvGfxROM2 + 0x100000, 0x080000);
			memcpy (DrvGfxROM2 + 0x100000, tmp + 0x000000, 0x080000);
		}

		{
			if (BurnLoadRom(DrvGfxROM0 + 0x000001,	  5, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x000000,	  6, 2)) return 1;

			nslasher_sprite_decode(DrvGfxROM0, DrvGfxROM3 + 0x000000, 0x400000, 1);

			if (BurnLoadRom(DrvGfxROM0 + 0x000001,	  7, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x000000,	  8, 2)) return 1;

			nslasher_sprite_decode(DrvGfxROM0, DrvGfxROM3 + 0x800000, 0x100000, 1);

			memset (DrvGfxROM0, 0, 0x400000);

			if (BurnLoadRom(DrvGfxROM0 + 0x000000,	  9, 4)) return 1;

			nslasher_sprite_decode(DrvGfxROM0, DrvGfxROM3 + 0x000000, 0x400000, 0);

			memset (DrvGfxROM0, 0, 0x400000);

			if (BurnLoadRom(DrvGfxROM0 + 0x000000,	 10, 4)) return 1;

			nslasher_sprite_decode(DrvGfxROM0, DrvGfxROM3 + 0x800000, 0x100000, 0);
		}

		if (BurnLoadRom(DrvGfxROM4 + 0x000000,	 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x000001,	 12, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000,	 13, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000,	 14, 1)) return 1;
		
		deco156_decrypt(DrvARMROM, 0x100000);
		deco56_decrypt_gfx(DrvGfxROM1, 0x200000);
		deco74_decrypt_gfx(DrvGfxROM2, 0x200000);

		deco16_tile_decode(DrvGfxROM1, DrvGfxROM0, 0x0200000, 1);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM1, 0x0200000, 0);
		deco16_tile_decode(DrvGfxROM2, DrvGfxROM2, 0x0200000, 0);
		deco16_sprite_decode(DrvGfxROM4, 0x100000);
	}

	ArmInit(0);
	ArmOpen(0);	
	ArmMapMemory(DrvARMROM,		0x000000, 0x0fffff, MAP_ROM);
	ArmMapMemory(DrvSysRAM,		0x100000, 0x11ffff, MAP_RAM); // 32-bit
	ArmMapMemory(DrvAceRAM,		0x163000, 0x1633ff, MAP_RAM); // 32-bit
	ArmMapMemory(DrvPalRAM,		0x168000, 0x169fff, MAP_RAM); // 32-bit
	ArmSetWriteByteHandler(fghthist_write_byte);
	ArmSetWriteLongHandler(fghthist_write_long);
	ArmSetReadByteHandler(fghthist_read_byte);
	ArmSetReadLongHandler(fghthist_read_long);
	ArmClose();

	EEPROMInit(&eeprom_interface_93C46);

	deco_104_init();
	deco_146_104_set_port_a_cb(fghthist_read_A); // inputs 0
	deco_146_104_set_port_b_cb(fghthist_read_B); // eeprom
	deco_146_104_set_port_c_cb(fghthist_read_C); // inputs 1
	deco_146_104_set_soundlatch_cb(deco32_soundlatch_write);
	deco_146_104_set_interface_scramble_interleave();

	deco16Init(0, 0, 1);
	deco16_set_graphics(DrvGfxROM0, 0x200000 * 2, DrvGfxROM1, 0x200000 * 2, DrvGfxROM2, 0x200000 * 2);
	deco16_set_color_base(0, 0x000);
	deco16_set_color_base(1, 0x100);
	deco16_set_color_base(2, 0x200);
	deco16_set_color_base(3, 0x300);
	deco16_set_global_offsets(0, 8);
	deco16_set_bank_callback(0, tattass_bank_callback);
	deco16_set_bank_callback(1, tattass_bank_callback);
	deco16_set_bank_callback(2, tattass_bank_callback);
	deco16_set_bank_callback(3, tattass_bank_callback);

	if (has_z80)
	{
		deco32_z80_sound_init(DrvHucROM, DrvHucRAM); // re-use these for z80
	}
	else
	{
		deco16SoundInit(DrvHucROM, DrvHucRAM, 4027500, 0, DrvYM2151WritePort, 0.42, 1006875, 1.00, 2013750, 0.25);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.80, BURN_SND_ROUTE_LEFT);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.80, BURN_SND_ROUTE_RIGHT);
	}

	DrvDoReset();

	return 0;
}

static void sprite_decode_5bpp_alt(UINT8 *gfx, INT32 len)
{
	INT32 Plane[5] = { 0x800000*8, 0x600000*8, 0x400000*8, 0x200000*8, 0 };
	INT32 XOffs[16] = { STEP8(128,1), STEP8(0,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len*2);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, gfx, len);

	GfxDecode(((len * 8) / 5) / (16 * 16), 5, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, gfx);

	BurnFree (tmp);
}

static INT32 TattassCommonInit(INT32 has_z80, UINT32 speedhack)
{
	game_select = 3;
	has_ace = 1;
	speedhack_address = speedhack;

	BurnSetRefreshRate(58.0); // 58hz for bsmt

	GenericTilesInit(); // for allocating memory for pTempDraw;

	gfxlen[0] = 0x400000;
	gfxlen[1] = 0x400000;
	gfxlen[2] = 0x400000;
	gfxlen[3] = 0x1000000;
	gfxlen[4] = 0x1000000;
	sndlen[0] = 0x200000;
	sndlen[1] = 0;
	sndlen[2] = 0;

	BurnAllocMemIndex();

	{
		if (BurnLoadRomExt(DrvARMROM + 0x000000,  0, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvARMROM + 0x000002,  1, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvHucROM,		  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,	  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001,	  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,	  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100001,	  6, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,	  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001,	  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100000,	  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100001,	 10, 2)) return 1;

		{
			UINT8 *tmp = DrvGfxROM0;
			memcpy (tmp + 0x000000, DrvGfxROM1 + 0x080000, 0x080000);
			memcpy (DrvGfxROM1 + 0x080000, DrvGfxROM1 + 0x100000, 0x080000);
			memcpy (DrvGfxROM1 + 0x100000, tmp + 0x000000, 0x080000);

			memcpy (tmp + 0x000000, DrvGfxROM2 + 0x080000, 0x080000);
			memcpy (DrvGfxROM2 + 0x080000, DrvGfxROM2 + 0x100000, 0x080000);
			memcpy (DrvGfxROM2 + 0x100000, tmp + 0x000000, 0x080000);
		}

		if (BurnLoadRom(DrvGfxROM3 + 0x000000,	 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x200000,	 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x400000,	 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x600000,	 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x800000,	 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x080000,	 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x280000,	 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x480000,	 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x680000,	 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x880000,	 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x100000,	 21, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x300000,	 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x500000,	 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x700000,	 24, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x900000,	 25, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x180000,	 26, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x380000,	 27, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x580000,	 28, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x780000,	 29, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x980000,	 30, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x000001,	 31, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x000003,	 32, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x000000,	 33, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x000002,	 34, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x200001,	 35, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x200003,	 36, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x200000,	 37, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x200002,	 38, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x400001,	 39, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x400003,	 40, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x400000,	 41, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x400002,	 42, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x600001,	 43, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x600003,	 44, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x600000,	 45, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x600002,	 46, 4)) return 1;

		BurnByteswap(DrvGfxROM4, 0x800000);

		if (BurnLoadRom(DrvSndROM0 + 0x000000,	 47, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0 + 0x080000,	 48, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0 + 0x100000,	 49, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0 + 0x180000,	 50, 1)) return 1;

		if (BurnLoadRom(m_eeprom + 0x000000,	 51, 1)) return 1;

		if (BurnLoadRom(DrvTMSROM  + 0x000000,   52, 1)) return 1;

		deco56_decrypt_gfx(DrvGfxROM1, 0x200000);
		deco56_decrypt_gfx(DrvGfxROM2, 0x200000);

		deco16_tile_decode(DrvGfxROM1, DrvGfxROM0, 0x0200000, 1);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM1, 0x0200000, 0);
		deco16_tile_decode(DrvGfxROM2, DrvGfxROM2, 0x0200000, 0);
		sprite_decode_5bpp_alt(DrvGfxROM3, 0xa00000);
		deco16_sprite_decode(DrvGfxROM4, 0x800000);
	}

	ArmInit(0);
	ArmOpen(0);	
	ArmMapMemory(DrvARMROM,		0x000000, 0x0fffff, MAP_ROM);
	ArmMapMemory(DrvSysRAM,		0x100000, 0x11ffff, MAP_RAM); // 32-bit
	ArmMapMemory(DrvJackRAM,	0x162000, 0x162fff, MAP_RAM); // 32-bit
	ArmMapMemory(DrvAceRAM,		0x163000, 0x1633ff, MAP_RAM); // 32-bit
	ArmMapMemory(DrvPalRAM,		0x168000, 0x169fff, MAP_RAM); // 32-bit
	ArmSetWriteByteHandler(fghthist_write_byte);
	ArmSetWriteLongHandler(fghthist_write_long);
	ArmSetReadByteHandler(fghthist_read_byte);
	ArmSetReadLongHandler(fghthist_read_long);
	ArmClose();

	EEPROMInit(&eeprom_interface_93C46);

	deco_104_init();
	deco_146_104_set_port_a_cb(fghthist_read_A); // inputs 0
	deco_146_104_set_port_b_cb(tattass_read_B); // eeprom
	deco_146_104_set_port_c_cb(fghthist_read_C); // inputs 1
	deco_146_104_set_soundlatch_cb(tattass_sound_cb);
	deco_146_104_set_interface_scramble_interleave();

	deco16Init(0, 0, 1);
	deco16_set_graphics(DrvGfxROM0, 0x200000 * 2, DrvGfxROM1, 0x200000 * 2, DrvGfxROM2, 0x200000 * 2);
	deco16_set_color_base(0, 0x000);
	deco16_set_color_base(1, 0x100);
	deco16_set_color_base(2, 0x200);
	deco16_set_color_base(3, 0x300);
	deco16_set_global_offsets(0, 8);
	deco16_set_bank_callback(0, tattass_bank_callback);
	deco16_set_bank_callback(1, tattass_bank_callback);
	deco16_set_bank_callback(2, tattass_bank_callback);
	deco16_set_bank_callback(3, tattass_bank_callback);

	use_bsmt = 1;
	decobsmt_init(DrvHucROM, DrvHucRAM, DrvTMSROM, DrvTMSRAM, DrvSndROM0, 0x200000);

	DrvDoReset();

	return 0;
}



static INT32 dragngun_bank_callback(const INT32 data)
{
	return (data & 0xe0) << 7;
}

static UINT16 dragngun_read_A()
{
	return DrvInputs[0];
}

static UINT16 dragngun_read_B()
{
	return (DrvInputs[1] & 0x7) | (deco16_vblank ? 8 : 0);
}

static UINT16 dragngun_read_C()
{
	return (DrvDips[1] << 8) | 0xff;
}

static INT32 DragngunCommonInit(INT32 has_z80, UINT32 speedhack)
{
	game_select = 4;
	speedhack_address = speedhack;

	BurnSetRefreshRate(57.79965);

	GenericTilesInit(); // for allocating memory for pTempSprite

	gfxlen[0] = 0x40000;
	gfxlen[1] = 0x400000;
	gfxlen[2] = 0x400000;
	gfxlen[3] = 0x800000;
	sndlen[0] = 0x080000;
	sndlen[1] = 0x080000;
	sndlen[2] = 0x080000;

	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvARMROM + 0x000000,     0, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x000001,     1, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x000002,     2, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x000003,     3, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x100000,     4, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x100001,     5, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x100002,     6, 4)) return 1;
		if (BurnLoadRom(DrvARMROM + 0x100003,     7, 4)) return 1;

		if (BurnLoadRom(DrvHucROM,		  8, 1)) return 1;

		for (INT32 i = 0; i < 4; i++) {
			if (BurnLoadRom(DrvGfxROM0 + 0x000000,	 13 + i, 1)) return 1;

			for (INT32 j = 0; j < 4; j++) {
				memcpy (DrvGfxROM2 + (0x100000 * j) + (0x40000 * i), DrvGfxROM0 + (0x40000 * j), 0x40000);
			}
		}

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,	  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,	 10, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,	 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x090000,	 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000003,	 17, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x400003,	 18, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x000002,	 19, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x400002,	 20, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x000001,	 21, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x400001,	 22, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x000000,	 23, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x400000,	 24, 4)) return 1;

		if (BurnLoadRom(DrvDVIROM + (0x000000^3),    25, 4)) return 1;
		if (BurnLoadRom(DrvDVIROM + (0x000001^3),    26, 4)) return 1;
		if (BurnLoadRom(DrvDVIROM + (0x000002^3),    27, 4)) return 1;
		if (BurnLoadRom(DrvDVIROM + (0x000003^3),    28, 4)) return 1;
		if (BurnLoadRom(DrvDVIROM + (0x400000^3),    29, 4)) return 1;
		if (BurnLoadRom(DrvDVIROM + (0x400001^3),    30, 4)) return 1;
		if (BurnLoadRom(DrvDVIROM + (0x400002^3),    31, 4)) return 1;
		if (BurnLoadRom(DrvDVIROM + (0x400003^3),    32, 4)) return 1;
		if (BurnLoadRom(DrvDVIROM + (0x800000^3),    33, 4)) return 1;
		if (BurnLoadRom(DrvDVIROM + (0x800001^3),    34, 4)) return 1;
		if (BurnLoadRom(DrvDVIROM + (0x800002^3),    35, 4)) return 1;
		if (BurnLoadRom(DrvDVIROM + (0x800003^3),    36, 4)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000,	 37, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000,	 38, 1)) return 1;

		if (BurnLoadRom(DrvSndROM2 + 0x000000,	 39, 1)) return 1;

		deco74_decrypt_gfx(DrvGfxROM0, 0x020000);
		deco74_decrypt_gfx(DrvGfxROM1, 0x120000);
		deco74_decrypt_gfx(DrvGfxROM2, 0x400000);

		memcpy (DrvGfxROM1+0x080000,DrvGfxROM0+0x00000,0x10000);
		memcpy (DrvGfxROM1+0x110000,DrvGfxROM0+0x10000,0x10000);

		deco16_tile_decode(DrvGfxROM0, DrvGfxROM0, 0x0020000, 1);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM1, 0x0120000, 0);
		decode_8bpp_tiles(DrvGfxROM2, 0x400000);
	}

	ArmInit(0);
	ArmOpen(0);	
	ArmMapMemory(DrvARMROM,			    0x000000, 0x0fffff, MAP_ROM);
	ArmMapMemory(DrvSysRAM,			    0x100000, 0x11ffff, MAP_RAM); // 32-bit
	ArmMapMemory(DrvPalRAM,			    0x130000, 0x131fff, MAP_RAM); // 32-bit
	ArmMapMemory(DrvSprRAM,			    0x200000, 0x2283ff, MAP_RAM);
	ArmMapMemory(DrvAceRAM,			    0x0204800, 0x0204fff, MAP_RAM);

	ArmMapMemory(DrvDVIROM,    	        0x1400000, 0x1ffffff, MAP_ROM); // DVI needed to beat the final boss
	ArmMapMemory(DrvDVIRAM0 + 0x1000,   0x1001000, 0x1007fff, MAP_RAM);
	ArmMapMemory(DrvDVIRAM1,            0x10b0000, 0x10b01ff, MAP_RAM);

	ArmMapMemory(DrvARMROM + 0x100000,	0x300000, 0x3fffff, MAP_ROM);
	ArmSetWriteByteHandler(dragngun_write_byte);
	ArmSetWriteLongHandler(dragngun_write_long);
	ArmSetReadByteHandler(dragngun_read_byte);
	ArmSetReadLongHandler(dragngun_read_long);
	ArmClose();

	vblank_irq_cb = irq_callback;
	raster2_irq_cb = irq_callback;

	EEPROMInit(&eeprom_interface_93C46);

	deco_146_init();
	deco_146_104_set_port_a_cb(dragngun_read_A); // inputs 0
	deco_146_104_set_port_b_cb(dragngun_read_B); // system
	deco_146_104_set_port_c_cb(dragngun_read_C); // dips
	deco_146_104_set_soundlatch_cb(deco32_soundlatch_write);
	deco_146_104_set_interface_scramble_reverse();

	deco16Init(0, 0, 1);
	deco16_dragngun_kludge = 1; // st.3 boss blank tile fix
	deco16_set_graphics(DrvGfxROM0, 0x020000 * 2, DrvGfxROM1, 0x120000 * 2, DrvGfxROM2, 0x400000 * 1);
	deco16_set_color_base(0, 0x200); // >> 4 internally (bpp shifts)
	deco16_set_color_base(1, 0x300);
	deco16_set_color_base(2, 0x400); // >> 8 ""
	deco16_set_color_base(3, 0x400);
	deco16_set_color_mask(2, 0x003);
	deco16_set_color_mask(3, 0x003);
	deco16_set_global_offsets(0, 8);
	deco16_set_bank_callback(0, tattass_bank_callback);
	deco16_set_bank_callback(1, tattass_bank_callback);
	deco16_set_bank_callback(2, dragngun_bank_callback);
	deco16_set_bank_callback(3, dragngun_bank_callback);

	use_z80 = 0;
	deco16SoundInit(DrvHucROM, DrvHucRAM, 4027500, 0, DrvYM2151WritePort, 0.42, 1006875, 0.50, 2013750, 0.18);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.40, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.40, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(2, (32220000/32) / 132, 1);
	MSM6295SetBank(2, DrvSndROM2 + ((DrvARMROM[0] == 0x5f) ? 0x00000 : 0x40000), 0, 0x3ffff);

	MSM6295SetRoute(2, 0.50, BURN_SND_ROUTE_BOTH);

	{	// disable service mode lockout (what is this??)
		if (DrvARMROM[0] == 0x5f) { // japan
			*((UINT32*)(DrvARMROM + 0x1a1b4)) = 0xe1a00000;
		} else {
			*((UINT32*)(DrvARMROM + 0x1b32c)) = 0xe1a00000;
		}
	}

	BurnGunInit(2, false);
	uses_gun = 1;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	deco16Exit();

	if (use_bsmt)
	{
		use_bsmt = 0;
		decobsmt_exit();
	}
	else if (use_z80)
	{
		use_z80 = 0;
		deco32_z80_sound_exit();
	}
	else
	{
		deco16SoundExit();
	}

	EEPROMExit();

	ArmExit();

	if (uses_gun) {
		BurnGunExit();
		uses_gun = 0;
	}

	BurnFreeMemIndex();

	raster1_irq_cb = NULL;
	raster2_irq_cb = NULL;
	vblank_irq_cb = NULL;
	lightgun_irq_cb = NULL;
	has_ace = 0;
	speedhack_address = 0;

	pStartDraw = NULL;
	pDrawScanline = NULL;

	return 0;
}

#ifndef MIN
#define MIN(x,y)			((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x,y)			((x) > (y) ? (x) : (y))
#endif

static void DrvPaletteUpdate()
{
	UINT32 *p = (UINT32*)DrvPalBuf;
	UINT32 *s = (UINT32*)DrvAceRAM;

	UINT8 fadeptr = s[0x20] & 0xff;
	UINT8 fadeptg = s[0x21] & 0xff;
	UINT8 fadeptb = s[0x22] & 0xff;
	UINT8 fadepsr = s[0x23] & 0xff;
	UINT8 fadepsg = s[0x24] & 0xff;
	UINT8 fadepsb = s[0x25] & 0xff;
	UINT16 mode = s[0x26] & 0xffff;

	for (INT32 i = 0; i < 0x2000/4; i++)
	{
		UINT8 r = (BURN_ENDIAN_SWAP_INT32(p[i]) >> 0) & 0xff;
		UINT8 g = (BURN_ENDIAN_SWAP_INT32(p[i]) >> 8) & 0xff;
		UINT8 b = (BURN_ENDIAN_SWAP_INT32(p[i]) >> 16) & 0xff;

		DrvPalette[i + 0x800] = BurnHighCol(r,g,b,0);

		if (has_ace == 1)
		{
			switch (mode)
			{
				default:
				case 0x1100: // multiplicative fade
					b = MAX(0, MIN(255, UINT8((b + (((fadeptb - b) * fadepsb) / 255)))));
					g = MAX(0, MIN(255, UINT8((g + (((fadeptg - g) * fadepsg) / 255)))));
					r = MAX(0, MIN(255, UINT8((r + (((fadeptr - r) * fadepsr) / 255)))));
					break;
				case 0x1000: // additive fade, correct?
					b = MIN(b + fadepsb, 0xff);
					g = MIN(g + fadepsg, 0xff);
					r = MIN(r + fadepsr, 0xff);
					break;
			}

		}

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 default_col_cb(INT32 col)
{
	return (col >> 9) & 0x1f;
}

static INT32 (*m_pri_cb)(INT32, INT32) = NULL;
static INT32 (*m_col_cb)(INT32) = NULL;

static void draw_sprites_common(UINT16 *bitmap, UINT8* ram, UINT8 *gfx, INT32 colbase, INT32 /*transp*/, INT32 sizewords, bool invert_flip, INT32 m_raw_shift, INT32 m_alt_format, INT32 layerID )
{
	INT32 m_y_offset = 0;
	INT32 m_x_offset = 0;

	INT32 offs, end, incr;

	INT32 bitmap_is_null = 0;

	if (bitmap) {
		memset (bitmap, 0, nScreenWidth * nScreenHeight * sizeof(UINT16));
	} else {
		bitmap_is_null = 1;
		bitmap = pTransDraw;
	}

	UINT16 *spriteram = (UINT16*)ram;

	bool flipscreen = false;

	if (invert_flip)
		flipscreen = !flipscreen;

	if (m_pri_cb)
	{
		offs = sizewords-4;
		end = -4;
		incr = -4;
	}
	else
	{
		offs = 0;
		end = sizewords;
		incr = 4;
	}

	while (offs!=end)
	{
		INT32 x, y, sprite, colour, multi, mult2, fx, fy, inc, flash, mult, w, pri;

		if (!m_alt_format)
		{
			sprite = BURN_ENDIAN_SWAP_INT16(spriteram[offs + 1]);
			y = BURN_ENDIAN_SWAP_INT16(spriteram[offs]);

			flash = y & 0x1000;

			w = y & 0x0800;

			if (!(flash && (nCurrentFrame & 1)))
			{
				x = BURN_ENDIAN_SWAP_INT16(spriteram[offs + 2]);

				if (bitmap_is_null)
				{
					colour = m_col_cb(x);
				}
				else
				{
					colour = (x >> 9) & 0x7f;
					if (y&0x8000) colour |= 0x80; // fghthist uses this to mark priority
				}

				if (m_pri_cb)
					pri = m_pri_cb(x, y);
				else
					pri = 0;

				fx = y & 0x2000;
				fy = y & 0x4000;

				INT32 tempwidth = (y & 0x0600) >> 9;

				multi = (1 << (tempwidth)) - 1; /* 1x, 2x, 4x, 8x height */

				if (flipscreen) x = ((x&0x1ff) - m_x_offset)&0x1ff;
				else x = ((x&0x1ff) + m_x_offset)&0x1ff;
				y = ((y&0x1ff) + m_y_offset)&0x1ff;

				if (nScreenWidth>256)
				{
					x = x & 0x01ff;
					y = y & 0x01ff;
					if (x >= 320) x -= 512;
					if (y >= 256) y -= 512;
					y = 240 - y;
					x = 304 - x;
				}
				else
				{
					x = x & 0x01ff;
					y = y & 0x01ff;
					if (x >= 256) x -= 512;
					if (y >= 256) y -= 512;
					y = 240 - y;
					x = 240 - x;
				}

				{
					sprite &= ~multi;

					if (fy)
						inc = -1;
					else
					{
						sprite += multi;
						inc = 1;
					}

					if (flipscreen)
					{
						y = 240 - y;
						if (fy) fy = 0; else fy = 1;
						mult = 16;
					}
					else
						mult = -16;

					if (flipscreen)
					{
						if (nScreenWidth>256)
							x = 304 - x;
						else
							x = 240 - x;

						if (fx) fx = 0; else fx = 1;
					}

					mult2 = multi + 1;

					y -= 8;

					while (multi >= 0)
					{
						INT32 ypos;
						ypos = y + mult * multi;
						if ((ypos < nScreenHeight) && (ypos>=0-16))
						{
							{
								{
									if (m_pri_cb)
										deco16_draw_prio_sprite(bitmap, gfx, (sprite - multi * inc)&0xffff, (colour<<m_raw_shift)+colbase, x,ypos, fx, fy, pri);
									else
										if (fy) {
											if (fx) {
												Render16x16Tile_Mask_FlipXY_Clip(bitmap, (sprite - multi * inc)&0xffff, x,ypos, colour, m_raw_shift, 0, colbase, gfx);
											} else {
												Render16x16Tile_Mask_FlipY_Clip(bitmap, (sprite - multi * inc)&0xffff, x,ypos, colour, m_raw_shift, 0, colbase, gfx);
											}
										} else{
											if (fx) {
												Render16x16Tile_Mask_FlipX_Clip(bitmap, (sprite - multi * inc)&0xffff, x,ypos, colour, m_raw_shift, 0, colbase, gfx);
											} else {
												Render16x16Tile_Mask_Clip(bitmap, (sprite - multi * inc)&0xffff, x,ypos, colour, m_raw_shift, 0, colbase, gfx);
											}
										}
								}

								// double wing uses this flag
								if (w)
								{
									if (m_pri_cb)
										deco16_draw_prio_sprite(bitmap, gfx, ((sprite - multi * inc)-mult2)&0xffff, (colour<<m_raw_shift)+colbase, !flipscreen ? x-16 : x+16,ypos, fx, fy, pri);
									else
										if (fy) {
											if (fx) {
												Render16x16Tile_Mask_FlipXY_Clip(bitmap, ((sprite - multi * inc)-mult2)&0xffff, !flipscreen ? x-16 : x+16,ypos, colour, m_raw_shift, 0, colbase, gfx);
											} else {
												Render16x16Tile_Mask_FlipY_Clip(bitmap, ((sprite - multi * inc)-mult2)&0xffff, !flipscreen ? x-16 : x+16,ypos, colour, m_raw_shift, 0, colbase, gfx);
											}
										} else{
											if (fx) {
												Render16x16Tile_Mask_FlipX_Clip(bitmap, ((sprite - multi * inc)-mult2)&0xffff, !flipscreen ? x-16 : x+16,ypos, colour, m_raw_shift, 0, colbase, gfx);
											} else {
												Render16x16Tile_Mask_Clip(bitmap, ((sprite - multi * inc)-mult2)&0xffff, !flipscreen ? x-16 : x+16,ypos, colour, m_raw_shift, 0, colbase, gfx);
											}
										}
								}
							}
						}

						multi--;
					}
				}
			}
		}
		else // m_alt_format
		{
			INT32 h=0;
			y = BURN_ENDIAN_SWAP_INT16(spriteram[offs+0]);
			sprite = BURN_ENDIAN_SWAP_INT16(spriteram[offs+3]) & 0xffff;

			if (m_pri_cb)
				pri = m_pri_cb(BURN_ENDIAN_SWAP_INT16(spriteram[offs+2])&0x00ff, 0);
			else
				pri = 0;

			x = BURN_ENDIAN_SWAP_INT16(spriteram[offs+1]);

			if (!((y&0x2000) && (nCurrentFrame & 1)))
			{
				colour = (BURN_ENDIAN_SWAP_INT16(spriteram[offs+2]) >>0) & 0x1f;

				h = (BURN_ENDIAN_SWAP_INT16(spriteram[offs+2])&0xf000)>>12;
				w = (BURN_ENDIAN_SWAP_INT16(spriteram[offs+2])&0x0f00)>> 8;
				fx = !(BURN_ENDIAN_SWAP_INT16(spriteram[offs+0])&0x4000);
				fy = !(BURN_ENDIAN_SWAP_INT16(spriteram[offs+0])&0x8000);

				if (!flipscreen) {
					x = x & 0x01ff;
					y = y & 0x01ff;
					if (x>0x180) x=-(0x200 - x);
					if (y>0x180) y=-(0x200 - y);

					if (fx) { mult=-16; x+=16*w; } else { mult=16; x-=16; }
					if (fy) { mult2=-16; y+=16*h; } else { mult2=16; y-=16; }
				} else {
					if (fx) fx=0; else fx=1;
					if (fy) fy=0; else fy=1;

					x = x & 0x01ff;
					y = y & 0x01ff;
					if (x&0x100) x=-(0x100 - (x&0xff));
					if (y&0x100) y=-(0x100 - (y&0xff));
					x = 304 - x;
					y = 240 - y;
					if (x >= 432) x -= 512;
					if (y >= 384) y -= 512;
					if (fx) { mult=-16; x+=16; } else { mult=16; x-=16*w; }
					if (fy) { mult2=-16; y+=16; } else { mult2=16; y-=16*h; }
				}
				INT32 ypos;

				for (INT32 xx=0; xx<w; xx++)
				{
					for (INT32 yy=0; yy<h; yy++)
					{
						{
							if (m_pri_cb)
							{
								ypos = y + mult2 * (h-yy);

								if ((ypos<nScreenHeight+16) && (ypos>=0-16))
								{
									deco16_draw_prio_sprite(bitmap, gfx, sprite + yy + h * xx, (colour<<m_raw_shift)+colbase, x + mult * (w-xx),ypos, fx, fy, pri);
								}

								ypos -= 512; // wrap-around y

								if ((ypos<nScreenHeight+16) && (ypos>=(0-16)))
								{
									deco16_draw_prio_sprite(bitmap, gfx, sprite + yy + h * xx, (colour<<m_raw_shift)+colbase, x + mult * (w-xx),ypos, fx, fy, pri);
								}

							}
							else
							{
								ypos = y + mult2 * (h-yy);

								if ((ypos<nScreenHeight) && (ypos>=(0-16))) // if it's not rendering the last couple lines somewhere, remove this.
								{
									if (fy) {
										if (fx) {
											Render16x16Tile_Mask_FlipXY_Clip(bitmap, sprite + yy + h * xx, x + mult * (w-xx),ypos, colour, m_raw_shift, 0, colbase, gfx);
										} else {
											Render16x16Tile_Mask_FlipY_Clip(bitmap, sprite + yy + h * xx, x + mult * (w-xx),ypos, colour, m_raw_shift, 0, colbase, gfx);
										}
									} else{
										if (fx) {
											Render16x16Tile_Mask_FlipX_Clip(bitmap, sprite + yy + h * xx, x + mult * (w-xx),ypos, colour, m_raw_shift, 0, colbase, gfx);
										} else {
											Render16x16Tile_Mask_Clip(bitmap, sprite + yy + h * xx, x + mult * (w-xx),ypos, colour, m_raw_shift, 0, colbase, gfx);
										}
									}
								}

								ypos -= 512; // wrap-around y

								if ((ypos<nScreenHeight) && (ypos>=(0-16)))
								{
									if (fy) {
										if (fx) {
											Render16x16Tile_Mask_FlipXY_Clip(bitmap, sprite + yy + h * xx, x + mult * (w-xx),ypos, colour, m_raw_shift, 0, colbase, gfx);
										} else {
											Render16x16Tile_Mask_FlipY_Clip(bitmap, sprite + yy + h * xx, x + mult * (w-xx),ypos, colour, m_raw_shift, 0, colbase, gfx);
										}
									} else{
										if (fx) {
											Render16x16Tile_Mask_FlipX_Clip(bitmap, sprite + yy + h * xx, x + mult * (w-xx),ypos, colour, m_raw_shift, 0, colbase, gfx);
										} else {
											Render16x16Tile_Mask_Clip(bitmap, sprite + yy + h * xx, x + mult * (w-xx),ypos, colour, m_raw_shift, 0, colbase, gfx);
										}
									}
								}
							}
						}
					}
				}
			}
		}

		offs+=incr;
	}
}

static INT32 captaven_pri_callback(INT32 pri, INT32)
{
	switch (pri & 0x60)
	{
		case 0x00:
			return 0;	// above everything

		case 0x20:
			return 0xfff0; // above 2nd playfield

		case 0x40:
			return 0xfffc; // above 1st playfield

		case 0x60:
		default:
			return 0xfffe; // under everything
	}
}

static INT32 lastline;

static INT32 CaptavenStartDraw()
{
	m_pri_cb = captaven_pri_callback;
	m_col_cb = default_col_cb;

	lastline = 0;

	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	deco16_clear_prio_map();

	BurnTransferClear();

	return 0;
}

static INT32 CaptavenDrawScanline(INT32 line)
{
	if (line > nScreenHeight) return 0;

	deco16_pf12_update();
	deco16_pf34_update();

	if (global_priority & 1)
	{
		if (nBurnLayer & 2) deco16_draw_layer_by_line(lastline, line, 1, pTransDraw, 1);
		if (nBurnLayer & 4) deco16_draw_layer_by_line(lastline, line, 2, pTransDraw, 2 | DECO16_LAYER_CAPTAVEN | DECO16_LAYER_8BITSPERPIXEL);
	}
	else
	{
		if (nBurnLayer & 4) deco16_draw_layer_by_line(lastline, line, 2, pTransDraw, 1 | DECO16_LAYER_CAPTAVEN | DECO16_LAYER_8BITSPERPIXEL);
		if (nBurnLayer & 2) deco16_draw_layer_by_line(lastline, line, 1, pTransDraw, 2);
	}

	if (nBurnLayer & 1) deco16_draw_layer_by_line(lastline, line, 0, pTransDraw, 4);

	lastline = line;

	return 0;
}

static INT32 CaptavenDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	if (nSpriteEnable & 1) draw_sprites_common(NULL, DrvSprBuf, DrvGfxROM3, 0, 0, 0x400, false, 4, 1, 0 );

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 fghthist_pri_callback(INT32, INT32 pri)
{
	return (pri & 0x8000) ? 0x110 : 0x100;
}

static INT32 fghthist_col_cb(INT32 col)
{
	return (col >> 9) & 0x7f;
}

static INT32 FghthistDraw()
{
	m_col_cb = fghthist_col_cb;
	m_pri_cb = fghthist_pri_callback;

	DrvPaletteUpdate();

	deco16_pf12_update();
	deco16_pf34_update();
	deco16_clear_prio_map();

	BurnTransferClear(0x300);

	if (nBurnLayer & 8) deco16_draw_layer(3, pTransDraw, 1);

	if (global_priority & 1)
	{
		if (nBurnLayer & 2) deco16_draw_layer(1, pTransDraw, 2);
		if (nBurnLayer & 4) deco16_draw_layer(2, pTransDraw, 4);
	}
	else
	{
		if (nBurnLayer & 4) deco16_draw_layer(2, pTransDraw, 2);
		if (nBurnLayer & 2) deco16_draw_layer(1, pTransDraw, 4);
	}

	if (nBurnLayer & 1) deco16_draw_layer(0, pTransDraw, 8);

	if (nSpriteEnable & 1) draw_sprites_common(NULL, DrvSprBuf, DrvGfxROM3, 0x400, 0xf, 0x800, true, 4, 0, 0 );

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void draw_combined_playfield(INT32 color, INT32 priority) // opaque
{
	UINT16 *src0 = pTempDraw[2];
	UINT16 *src1 = pTempDraw[3];
	UINT16 *dest = pTransDraw;
	UINT8 *prio = deco16_prio_map;

	UINT8 *tptr = deco16_pf_rowscroll[3];
	deco16_pf_rowscroll[3] = deco16_pf_rowscroll[2];

	deco16_draw_layer(2, pTempDraw[2], DECO16_LAYER_OPAQUE);
	deco16_draw_layer(3, pTempDraw[3], DECO16_LAYER_OPAQUE);

	deco16_pf_rowscroll[3] = tptr;

	if (game_select == 2)
	{
		for (INT32 y = 0; y < nScreenHeight; y++) {
			for (INT32 x = 0; x < nScreenWidth; x++) {
				INT32 pxl = (src0[x] & 0x0f) | ((src1[x] & 0x0f) << 4);
				dest[x] = (pxl) ? color | pxl : 0x300; // color at 0x300 is better for bg fill @ 8bpp mode. fixes green background on dataeast screen and ending scenes.
				prio[x] = priority;
			}
			src0 += nScreenWidth;
			src1 += nScreenWidth;
			dest += nScreenWidth;
			prio += 512;
		}
	} else {
		for (INT32 y = 0; y < nScreenHeight; y++) {
			for (INT32 x = 0; x < nScreenWidth; x++) {
				INT32 pxl = (src0[x] & 0x0f) | ((src1[x] & 0x0f) << 4) | ((src0[x] & 0x30) << 4);
				dest[x] = 0x200 + pxl;
				prio[x] = priority;
			}
			src0 += nScreenWidth;
			src1 += nScreenWidth;
			dest += nScreenWidth;
			prio += 512;
		}
	}
}

static inline UINT32 alphablend32(UINT32 d, UINT32 s, UINT32 p)
{
	INT32 a = 255 - p;

	return (((((s & 0xff00ff) * p) + ((d & 0xff00ff) * a)) & 0xff00ff00) +
			((((s & 0x00ff00) * p) + ((d & 0x00ff00) * a)) & 0x00ff0000)) >> 8;
}

static inline UINT32 alphablend16(UINT32 d, UINT32 s, UINT32 p)
{
	p = (p + 2) >> 2;
	UINT8 a = 64 - p;

	return (((((s & 0x00f81f) * p) + ((d & 0x00f81f) * a)) & 0x003e07c0) +
			((((s & 0x0007e0) * p) + ((d & 0x0007e0) * a)) & 0x0001f800)) >> 6;
}

static inline UINT32 alphablend15(UINT32 d, UINT32 s, UINT32 p)
{
	p = (p + 4) >> 3;
	UINT8 a = 32 - p;

	return (((((s & 0x007c1f) * p) + ((d & 0x007c1f) * a)) & 0x00f83e0) +
			((((s & 0x0003e0) * p) + ((d & 0x0003e0) * a)) & 0x0007c00)) >> 5;
}

static UINT8 ace_get_alpha(UINT8 val)
{
	val &= 0x1f;

	UINT32 *m_ace_ram = (UINT32*)DrvAceRAM;

	int alpha = BURN_ENDIAN_SWAP_INT32(m_ace_ram[val]) & 0xff;
	if (alpha > 0x20) {
		return 0x80;
	} else {
		alpha = 255 - (alpha << 3);
		if (alpha < 0) {
			alpha = 0;
		}

		return alpha & 0xff;
	}
}

static void mixDualAlphaSprites(INT32 mixAlphaTilemap, INT32 drawAlphaTilemap)
{
	UINT32 *pal0 = DrvPalette + color_base[1];
	UINT32 *pal1 = DrvPalette + color_base[2];
	UINT32 *pal2 = DrvPalette;

	INT32 granularity0 = 1<<5;
	INT32 granularity1 = 1<<4;

	INT32 depth = BurnHighCol(0,0xff,0,0);

	switch (depth)
	{
		case 0x00ff00:
			depth = nBurnBpp * 8;
		break;

		case 0x007e0:
			depth = 16;
		break;

		case 0x003e0:
			depth = 15;
		break;
	}

	/* Mix sprites into main bitmap, based on priority & alpha */
	for (INT32 y=0; y<nScreenHeight; y++) {
		UINT16* alphaTilemap=pTempDraw[2] + y * nScreenWidth;
		UINT8* tilemapPri=deco16_prio_map + (y * 512);
		UINT16* sprite0=pTempDraw[0] + (y * nScreenWidth);
		UINT16* sprite1=pTempDraw[1] + (y * nScreenWidth);
		UINT32 *destLine32 = (UINT32*)pBurnDraw;
		UINT16 *destLine16 = (UINT16*)pBurnDraw;
		destLine32 += y * nScreenWidth;
		destLine16 += y * nScreenWidth;

		for (INT32 x=0; x<nScreenWidth; x++) {
			if (tilemapPri[x] == 8) { // text layer, always on top!
				continue;
			}

			UINT16 priColAlphaPal0=sprite0[x];
			UINT16 priColAlphaPal1=sprite1[x];
			UINT16 pri0=(priColAlphaPal0&0x6000)>>13;
			UINT16 pri1=(priColAlphaPal1&0x6000)>>13;
			UINT16 col0=((priColAlphaPal0&0x1f00)>>8);
			UINT16 col1=((priColAlphaPal1&0x0f00)>>8);
			if (game_select == 3) { // tattass
				col0=((priColAlphaPal0&0x0f00)>>8); // diff. masks
				col1=((priColAlphaPal1&0x3f00)>>8);
			}
			bool alpha1=(priColAlphaPal1&0x8000);
			bool alpha2=(!(priColAlphaPal1&0x1000));

			// Apply sprite bitmap 0 according to priority rules
			UINT16 coloffs = ((global_priority & 4) == 0) ? 0x800 : 0;
			bool sprite1_drawn = false;
			if ((priColAlphaPal0&0xff)!=0)
			{
				if ((pri0&0x3)==0 || (pri0&0x3)==1 || ((pri0&0x3)==2 && mixAlphaTilemap))
				{
					if (depth == 32)
						destLine32[x]=pal0[coloffs | ((priColAlphaPal0&0xff) + (granularity0 * col0))];
					else if (depth < 24)
						destLine16[x]=pal0[coloffs | ((priColAlphaPal0&0xff) + (granularity0 * col0))];
					sprite1_drawn = true;
				}
				else if ((pri0&0x3)==2) // Spri0 under top playfield
				{
					if (tilemapPri[x]<4) {
						if (depth == 32)
							destLine32[x]=pal0[coloffs | ((priColAlphaPal0&0xff) + (granularity0 * col0))];
						else if (depth < 24)
							destLine16[x]=pal0[coloffs | ((priColAlphaPal0&0xff) + (granularity0 * col0))];
						sprite1_drawn = true;
					}
				}
				else // Spri0 under top & middle playfields
				{
					if (tilemapPri[x]<2) {
						if (depth == 32)
							destLine32[x]=pal0[coloffs | ((priColAlphaPal0&0xff) + (granularity0 * col0))];
						else if (depth < 24)
							destLine16[x]=pal0[coloffs | ((priColAlphaPal0&0xff) + (granularity0 * col0))];
						sprite1_drawn = true;
					}
				}
			}

			coloffs = (((global_priority & 4) == 0) && sprite1_drawn) ? 0x800 : 0;
			int alpha = ((!alpha1) || alpha2) ? ace_get_alpha((col1 & 0x8) ? (0x4 + ((col1 & 0x3) / 2)) : ((col1 & 0x7) / 2)) : 0xff;

			if (game_select == 3) {
				alpha = ace_get_alpha(col1 / 8);
				col1 &= 0xf;
			}

			// Apply sprite bitmap 1 according to priority rules
			if (priColAlphaPal1&0xff)
			{
				if (alpha1)
				{
					if (pri1==0 && (((priColAlphaPal0&0xff)==0 || ((pri0&0x3)!=0 && (pri0&0x3)!=1 && (pri0&0x3)!=2))))
					{
						if ((global_priority&1)==0 ||
							((global_priority&1)==1 && tilemapPri[x]<4) ||
							((global_priority&1)==1 && (mixAlphaTilemap && ((alphaTilemap[x] & 0xf) == 0))))
							{
							// usually "shadows" under characters (nslasher)
							if (depth == 32)
								destLine32[x]=alphablend32(destLine32[x], pal1[coloffs | ((priColAlphaPal1&0xff) + (granularity1 * col1))], alpha);
							else if (depth == 16)
								destLine16[x]=alphablend16(destLine16[x], pal1[coloffs | ((priColAlphaPal1&0xff) + (granularity1 * col1))], alpha);
							else if (depth == 15)
								destLine16[x]=alphablend15(destLine16[x], pal1[coloffs | ((priColAlphaPal1&0xff) + (granularity1 * col1))], alpha);

						}
					}
					else if ((pri1>=2) || (pri1 == 1 && ((global_priority & 1) == 0 || tilemapPri[x] < 4)
										   && ((priColAlphaPal0 & 0xff) == 0 || ((pri0 & 0x3) != 0 && (pri0 & 0x3) != 1 && ((global_priority & 1) == 0 || (pri0 & 0x3) != 2))))) {
						if (depth == 32)
							destLine32[x]=alphablend32(destLine32[x], pal1[coloffs | ((priColAlphaPal1&0xff) + (granularity1 * col1))], alpha);
						else if (depth == 16)
							destLine16[x]=alphablend16(destLine16[x], pal1[coloffs | ((priColAlphaPal1&0xff) + (granularity1 * col1))], alpha);
						else if (depth == 15)
							destLine16[x]=alphablend15(destLine16[x], pal1[coloffs | ((priColAlphaPal1&0xff) + (granularity1 * col1))], alpha);
					}
				}
				else
				{
					if ((pri1==0 && ((priColAlphaPal0&0xff)==0 || ((pri0&0x3)!=0))) || (pri1>=1)) {
						if (depth == 32)
							destLine32[x]=alphablend32(destLine32[x], pal1[coloffs | ((priColAlphaPal1&0xff) + (granularity1 * col1))], alpha);
						else if (depth == 16)
							destLine16[x]=alphablend16(destLine16[x], pal1[coloffs | ((priColAlphaPal1&0xff) + (granularity1 * col1))], alpha);
						else if (depth == 15)
							destLine16[x]=alphablend15(destLine16[x], pal1[coloffs | ((priColAlphaPal1&0xff) + (granularity1 * col1))], alpha);
					}
				}
			}

			if (mixAlphaTilemap && drawAlphaTilemap)
			{
				UINT16 p=alphaTilemap[x];
				if (p&0xf)
				{
					/* Alpha tilemap under top two sprite 0 priorities */

					if (((priColAlphaPal0&0xff)==0 || (pri0&0x3)==2 || (pri0&0x3)==3)
						&& ((priColAlphaPal1&0xff)==0 || (pri1&0x3)==2 || (pri1&0x3)==3 || alpha1))
					{
						/* Alpha values are tied to ACE ram */
						INT32 alpha_ = ace_get_alpha(0x17 + (((p & 0xf0) >> 4) / 2));

						if (depth == 32)
							destLine32[x]=alphablend32(destLine32[x], pal2[coloffs | p], alpha_);
						else if (depth == 16)
							destLine16[x]=alphablend16(destLine16[x], pal2[coloffs | p], alpha_);
						else if (depth == 15)
							destLine16[x]=alphablend15(destLine16[x], pal2[coloffs | p], alpha_);

					}
				}
			}
		}
	}
}

static INT32 NslasherDraw()
{
	DrvPaletteUpdate();

	deco16_pf12_update();
	deco16_pf34_update();
	deco16_clear_prio_map();

	BurnTransferClear(0x300);

	UINT32 *ace = (UINT32*)DrvAceRAM;

	INT32 has_alpha = (ace[0x17] && global_priority&3) ? 1 : 0;

	if (global_priority & 2)
	{
		draw_combined_playfield(0x200, 1);
		if (nBurnLayer & 2) deco16_draw_layer(1, pTransDraw, 4);
	}
	else
	{
		if (nBurnLayer & 8) deco16_draw_layer(3, pTransDraw, 1);

		if (global_priority & 1)
		{
			if (nBurnLayer & 2) deco16_draw_layer(1, pTransDraw, 2);
			if (nBurnLayer & 4) deco16_draw_layer(2, (has_alpha) ? pTempDraw[2] : pTransDraw, 4 + (has_alpha ? DECO16_LAYER_OPAQUE : 0));
		}
		else
		{
			if (nBurnLayer & 4) deco16_draw_layer(2, pTransDraw, 2);
			if (nBurnLayer & 2) deco16_draw_layer(1, (has_alpha) ? pTempDraw[2] : pTransDraw, 4 + (has_alpha ? DECO16_LAYER_OPAQUE : 0));
		}
	}

	if ((nSpriteEnable & 1) == 0) memset (pTempDraw[0], 0, nScreenWidth * nScreenHeight*2);
	if ((nSpriteEnable & 2) == 0) memset (pTempDraw[1], 0, nScreenWidth * nScreenHeight*2);

	m_col_cb = default_col_cb;
	m_pri_cb = NULL;

	if (nSpriteEnable & 1) draw_sprites_common(pTempDraw[0], DrvSprBuf2, DrvGfxROM3, 0, 0, 0x800, true, 8, 0, 0);
	if (nSpriteEnable & 2) draw_sprites_common(pTempDraw[1], DrvSprBuf,  DrvGfxROM4, 0, 0, 0x800, true, 8, 0, 1);

	if (nBurnLayer & 1) deco16_draw_layer(0, pTransDraw, 8);

	BurnTransferCopy(DrvPalette);

	mixDualAlphaSprites(has_alpha, has_alpha);

	return 0;
}

static void dragngun_drawgfxzoom(UINT32 code, UINT32 color,INT32 flipx,INT32 flipy,INT32 sx,INT32 sy,
		INT32 scalex, INT32 scaley, INT32 sprite_screen_width, INT32 sprite_screen_height, UINT8 alpha, INT32 priority, INT32 depth)
{
	if (!scalex || !scaley) return;

	INT32 shift = (code & 0x8000) ? 0 : 4;

	color = (color & 0x1f) * 16;
	UINT32 *pal = DrvPalette + color;

	const UINT8 *code_base = DrvGfxROM3 + ((code & 0x7fff) * 0x100);

	if (sprite_screen_width && sprite_screen_height)
	{
		INT32 dx = 0x100000/sprite_screen_width;
		INT32 dy = 0x100000/sprite_screen_height;

		INT32 ex = sx+sprite_screen_width;
		INT32 ey = sy+sprite_screen_height;

		INT32 x_index_base;
		INT32 y_index;

		if( flipx )
		{
			x_index_base = (sprite_screen_width-1)*dx;
			dx = -dx;
		}
		else
		{
			x_index_base = 0;
		}

		if( flipy )
		{
			y_index = (sprite_screen_height-1)*dy;
			dy = -dy;
		}
		else
		{
			y_index = 0;
		}

		if( sx < 0)
		{
			INT32 pixels = 0-sx;
			sx += pixels;
			x_index_base += pixels*dx;
		}

		if( sy <0 )
		{
			INT32 pixels = 0-sy;
			sy += pixels;
			y_index += pixels*dy;
		}

		if( ex > nScreenWidth )
		{
			INT32 pixels = ex-nScreenWidth;
			ex -= pixels;
		}

		if( ey > nScreenHeight )
		{
			INT32 pixels = ey-nScreenHeight;
			ey -= pixels;
		}

		if (ex > sx)
		{
			for (INT32 y = sy; y < ey; y++)
			{
				if (depth == 32)
				{
					const UINT8 *source = code_base + (y_index >> 16) * 16;
					UINT32 *dest_tmap = ((UINT32*)pBurnDraw) + (y * nScreenWidth);
					UINT32 *dest = pTempSprite + (y * nScreenWidth);
					UINT8 *pri = deco16_prio_map + (y * 512);

					INT32 x_index = x_index_base;
					for (INT32 x = sx; x < ex; x++)
					{
						INT32 c = (source[x_index >> 16] >> shift) & 0xf;
						if (c != 0xf)
						{
							if (priority >= pri[x])
							{
								if (alpha == 0xff) { // no alpha
									dest[x] = pal[c];
									dest[x] |= 0xff000000;
								} else { // alpha
									if ((dest[x] & 0xff000000) == 0x00000000)
										dest[x] = alphablend32(dest_tmap[x] & 0x00ffffff, pal[c] & 0x00ffffff, alpha);
									else
										dest[x] = alphablend32(dest[x] & 0x00ffffff, pal[c] & 0x00ffffff, alpha);

									dest[x] |= 0xff000000;
								}
							} else {
								dest[x] = 0x00000000;
							}
						}

						x_index += dx;
					}
				}
				else if (depth == 16)
				{
					const UINT8 *source = code_base + (y_index >> 16) * 16;
					UINT16 *dest_tmap = ((UINT16*)pBurnDraw) + (y * nScreenWidth);
					UINT32 *dest = pTempSprite + (y * nScreenWidth);
					UINT8 *pri = deco16_prio_map + (y * 512);

					INT32 x_index = x_index_base;
					for (INT32 x = sx; x < ex; x++)
					{
						INT32 c = (source[x_index >> 16] >> shift) & 0xf;
						if (c != 0xf)
						{
							if (priority >= pri[x])
							{
								if (alpha == 0xff) { // no alpha
									dest[x] = pal[c];
									dest[x] |= 0xff000000;
								} else { // alpha
									if ((dest[x] & 0xff000000) == 0x00000000)
										dest[x] = alphablend16(dest_tmap[x] & 0x00ffffff, pal[c] & 0x00ffffff, alpha);
									else
										dest[x] = alphablend16(dest[x] & 0x00ffffff, pal[c] & 0x00ffffff, alpha);

									dest[x] |= 0xff000000;
								}
							} else {
								dest[x] = 0x00000000;
							}
						}

						x_index += dx;
					}
				}
				else if (depth == 15)
				{
					const UINT8 *source = code_base + (y_index >> 16) * 16;
					UINT16 *dest_tmap = ((UINT16*)pBurnDraw) + (y * nScreenWidth);
					UINT32 *dest = pTempSprite + (y * nScreenWidth);
					UINT8 *pri = deco16_prio_map + (y * 512);

					INT32 x_index = x_index_base;
					for (INT32 x = sx; x < ex; x++)
					{
						INT32 c = (source[x_index >> 16] >> shift) & 0xf;
						if (c != 0xf)
						{
							if (priority >= pri[x])
							{
								if (alpha == 0xff) { // no alpha
									dest[x] = pal[c];
									dest[x] |= 0xff000000;
								} else { // alpha
									if ((dest[x] & 0xff000000) == 0x00000000)
										dest[x] = alphablend15(dest_tmap[x] & 0x00ffffff, pal[c] & 0x00ffffff, alpha);
									else
										dest[x] = alphablend15(dest[x] & 0x00ffffff, pal[c] & 0x00ffffff, alpha);

									dest[x] |= 0xff000000;
								}
							} else {
								dest[x] = 0x00000000;
							}
						}

						x_index += dx;
					}
				}

				y_index += dy;
			}
		}
	}
}

static void dragngun_draw_sprites()
{
	const UINT32 *spritedata = (UINT32*)DrvSprBuf;
	const UINT32 *dragngun_sprite_layout_0_ram = (UINT32*)(DrvSprRAM + 0x08000);
	const UINT32 *dragngun_sprite_layout_1_ram = (UINT32*)(DrvSprRAM + 0x0c000);
	const UINT32 *dragngun_sprite_lookup_0_ram = (UINT32*)(DrvSprRAM + 0x10000);
	const UINT32 *dragngun_sprite_lookup_1_ram = (UINT32*)(DrvSprRAM + 0x18000);
	const UINT32 *layout_ram;
	const UINT32 *lookup_ram;

	memset(pTempSprite, 0x00, nScreenWidth * nScreenHeight * sizeof(UINT32));

	INT32 depth = BurnHighCol(0,0xff,0,0);

	switch (depth)
	{
		case 0x00ff00:
			depth = nBurnBpp * 8;
		break;

		case 0x007e0:
			depth = 16;
		break;

		case 0x003e0:
			depth = 15;
		break;
	}

	for (INT32 offs = 0;offs < 0x800;offs += 8)
	{
		INT32 xpos,ypos;

		INT32 scalex = BURN_ENDIAN_SWAP_INT32(spritedata[offs+4]) & 0x3ff;
		INT32 scaley = BURN_ENDIAN_SWAP_INT32(spritedata[offs+5]) & 0x3ff;
		if (!scalex || !scaley)
			continue;

		INT32 layoutram_offset = (BURN_ENDIAN_SWAP_INT32(spritedata[offs + 0]) & 0x1ff) * 4;

		if (BURN_ENDIAN_SWAP_INT32(spritedata[offs + 0]) & 0x400)
			layout_ram = dragngun_sprite_layout_1_ram;
		else
			layout_ram = dragngun_sprite_layout_0_ram;
		INT32 h = (BURN_ENDIAN_SWAP_INT32(layout_ram[layoutram_offset + 1])>>0)&0xf;
		INT32 w = (BURN_ENDIAN_SWAP_INT32(layout_ram[layoutram_offset + 1])>>4)&0xf;
		if (!h || !w)
			continue;

		INT32 sx = BURN_ENDIAN_SWAP_INT32(spritedata[offs+2]) & 0x3ff;
		INT32 sy = BURN_ENDIAN_SWAP_INT32(spritedata[offs+3]) & 0x3ff;
		INT32 bx = BURN_ENDIAN_SWAP_INT32(layout_ram[layoutram_offset + 2]) & 0x1ff;
		INT32 by = BURN_ENDIAN_SWAP_INT32(layout_ram[layoutram_offset + 3]) & 0x1ff;
		if (bx&0x100) bx=1-(bx&0xff);
		if (by&0x100) by=1-(by&0xff);
		if (sx >= 512) sx -= 1024;
		if (sy >= 512) sy -= 1024;

		INT32 color = BURN_ENDIAN_SWAP_INT32(spritedata[offs+6])&0x1f;

		INT32 priority = (BURN_ENDIAN_SWAP_INT32(spritedata[offs + 6]) & 0x60) >> 5;
		INT32 priority_orig = priority;
		// pri hacking: follow priority_orig
		priority = 7;

		INT32 alpha = (BURN_ENDIAN_SWAP_INT32(spritedata[offs+6]) & 0x80) ? 0x80 : 0xff;

		INT32 fx = BURN_ENDIAN_SWAP_INT32(spritedata[offs+4]) & 0x8000;
		INT32 fy = BURN_ENDIAN_SWAP_INT32(spritedata[offs+5]) & 0x8000;

		INT32 lookupram_offset = BURN_ENDIAN_SWAP_INT32(layout_ram[layoutram_offset + 0]) & 0x1fff;

		if (BURN_ENDIAN_SWAP_INT32(layout_ram[layoutram_offset + 0]) & 0x2000)
			lookup_ram = dragngun_sprite_lookup_1_ram;
		else
			lookup_ram = dragngun_sprite_lookup_0_ram;

		INT32 zoomx=scalex * 0x10000 / (w*16);
		INT32 zoomy=scaley * 0x10000 / (h*16);

		if (!fy)
			ypos=(sy<<16) - (by*zoomy);
		else
			ypos=(sy<<16) + (by*zoomy) - (16*zoomy);

		for (INT32 y=0; y<h; y++) {
			if (!fx)
				xpos=(sx<<16) - (bx*zoomx); 
			else
				xpos=(sx<<16) + (bx*zoomx) - (16*zoomx);

			for (INT32 x=0; x<w; x++) {
				INT32 sprite = BURN_ENDIAN_SWAP_INT32(lookup_ram[lookupram_offset]) & 0x3fff;

				lookupram_offset++;

				sprite = (sprite & 0xfff) | ((sprite_ctrl >> ((sprite & 0x3000) >> 10)) << 12);

				if ((sprite & 0xc000) == 0x0000 || (sprite & 0xc000) == 0xc000)
					sprite ^= 0xc000;
				//if (counter) bprintf(0, _T("%X, "), sprite);

				// hack for sprite mask in intro
				if (sprite >= 0x3e44 && sprite <= 0x3f03 && priority_orig == 1) // dragon-fire masking effect for titlescreen
					priority = 1; else priority = 7;

				if (sprite >= 0xfe4f55 && sprite <= 0xfe4fcc)
					zoomx=(scalex+1) * 0x10000 / (w*16);

				dragngun_drawgfxzoom(sprite,color,fx,fy,xpos>>16,(ypos>>16)-8,zoomx,zoomy,
						((xpos+(zoomx<<4))>>16) - (xpos>>16), ((ypos+(zoomy<<4))>>16) - (ypos>>16), alpha, priority, depth);

				if (fx)
					xpos-=zoomx<<4;
				else
					xpos+=zoomx<<4;
			}
			if (fy)
				ypos-=zoomy<<4;
			else
				ypos+=zoomy<<4;
		}
	}
	//if (counter) bprintf(0, _T("\n-------\n"));

	switch (depth) {
		case 32: {
			for (INT32 y = 0; y < nScreenHeight; y++)
			{
				UINT32 *src = pTempSprite + (y * nScreenWidth);
				UINT32 *dst = ((UINT32*)pBurnDraw) + (y * nScreenWidth);

				for (INT32 x = 0; x < nScreenWidth; x++)
				{
					UINT32 srcpix = src[x];

					if ((srcpix & 0xff000000) == 0xff000000)
					{
						dst[x] = srcpix & 0x00ffffff;
					}
				}

			}
			break;
		}
		case 15:
		case 16: {
			for (INT32 y = 0; y < nScreenHeight; y++)
			{
				UINT32 *src = pTempSprite + (y * nScreenWidth);
				UINT16 *dst = ((UINT16*)pBurnDraw) + (y * nScreenWidth);

				for (INT32 x = 0; x < nScreenWidth; x++)
				{
					UINT32 srcpix = src[x];

					if ((srcpix & 0xff000000) == 0xff000000)
					{
						dst[x] = srcpix & 0x0000ffff;
					}
				}

			}
			break;
		}
	}
}

static INT32 DragngunStartDraw()
{
	DrvPaletteUpdate();

	lastline = 0;

	deco16_clear_prio_map();

	BurnTransferClear(0x400);

	return 0;
}

static INT32 DragngunDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	BurnTransferCopy(DrvPalette);

	if (nSpriteEnable & 1) dragngun_draw_sprites();

	return 0;
}

static INT32 DragngunDrawScanline(INT32 line)
{
	if (line > nScreenHeight) return 0;

	deco16_pf12_update();
	deco16_pf34_update();

	if (nBurnLayer & 1) deco16_draw_layer_by_line(lastline, line, 3, pTransDraw, 1 | DECO16_LAYER_8BITSPERPIXEL);
	if (nBurnLayer & 2) deco16_draw_layer_by_line(lastline, line, 2, pTransDraw, 2 | DECO16_LAYER_8BITSPERPIXEL);
	if (nBurnLayer & 4) deco16_draw_layer_by_line(lastline, line, 1, pTransDraw, 4);
	if (nBurnLayer & 8) deco16_draw_layer_by_line(lastline, line, 0, pTransDraw, 8);

	lastline = line;

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ArmNewFrame();
	h6280NewFrame();

	{
		memset (DrvInputs, 0xff, 3 * sizeof(INT16));

		if (game_select == 1 || game_select == 2 || game_select == 3) {
			DrvInputs[1] = (DrvInputs[1] & ~0x18) | (DrvDips[0] & 8);
		}

		if (game_select == 2) { // nslasher p3 fake coin button
			DrvJoy2[0] |= DrvJoyFS[3];
		}

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (uses_gun) {
			BurnGunMakeInputs(0, DrvGun0, DrvGun1);
			BurnGunMakeInputs(1, DrvGun2, DrvGun3);
		}
	}

	INT32 nInterleave = 274;
	INT32 nCyclesTotal[2] = { (INT32)((double)7000000 / 57.799650), (INT32)((double)deco16_sound_cpuclock / 57.799650) };
	if (game_select == 2) { // nslasher
		nCyclesTotal[0] = (INT32)((double)7080500 / 58.464346);
		nCyclesTotal[1] = (INT32)((double)deco16_sound_cpuclock / 58.464346);
	}
	INT32 nCyclesDone[2] = { nExtraCycles[0], nExtraCycles[1] };

	ArmOpen(0);
	h6280Open(0);

	deco16_vblank = 1;

	if (pStartDraw != NULL)
		pStartDraw();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Arm);
		CPU_RUN_TIMER(1);

		deco_irq_scanline_callback(i); // iq_132 - ok?

		if (pDrawScanline != NULL && i>=7 && raster_irq) pDrawScanline(i-7);

		if (i == 8) deco16_vblank = 0;

		if (i == 248) {
			if (pDrawScanline != NULL) {
				pDrawScanline(i-8);

				if (pBurnDraw) {
					BurnDrvRedraw();
				}
			}
			if (game_select == 1 || game_select == 2) irq_callback(1);
			deco16_vblank = 1;
		}
	}

	if (pBurnSoundOut) {
		deco16SoundUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	h6280Close();
	ArmClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnDraw && pDrawScanline == NULL) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvZ80Frame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ArmNewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3 * sizeof(INT16));

		if (game_select == 1 || game_select == 2 || game_select == 3) {
			DrvInputs[1] = (DrvInputs[1] & ~0x18) | (DrvDips[0] & 8);
		}

		if (game_select == 2) { // nslasher p3 fake coin button
			DrvJoy2[0] |= DrvJoyFS[3];
		}

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 274;
	INT32 nCyclesTotal[2] = { (INT32)((double)7000000 / 57.799650), (INT32)((double)3580000 / 57.799650) };
	if (game_select == 2) { // nslasher
		nCyclesTotal[0] = (INT32)((double)7080500 / 58.464346);
		nCyclesTotal[1] = (INT32)((double)3580000 / 58.464346);
	}
	INT32 nCyclesDone[2] = { nExtraCycles[0], nExtraCycles[1] };

	ArmOpen(0);
	ZetOpen(0);

	deco16_vblank = 1;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Arm);
		CPU_RUN_TIMER(1);

		deco_irq_scanline_callback(i); // iq_132 - ok?

		if (i == 8) deco16_vblank = 0;

		if (i == 248) {
			if (game_select == 1 || game_select == 2 || game_select == 3) irq_callback(1);
			deco16_vblank = 1;
		}
	}

	ZetClose();
	ArmClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		deco32_z80_sound_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvBSMTFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ArmNewFrame();
	decobsmt_new_frame();
	{
		memset (DrvInputs, 0xff, 3 * sizeof(INT16));

		DrvInputs[1] = (DrvInputs[1] & ~0x10);

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 274;
	INT32 nCyclesTotal[3] = { 7000000 / 58, 1789790 / 58, 24000000/4 / 58 };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2] };

	ArmOpen(0);
	deco16_vblank = 1;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Arm);
		if (bsmt_in_reset == 0) {
			M6809Open(0);
			CPU_RUN(1, M6809);

			if (nCurrentFrame&1) { // needs 8.43 firq's per frame, to simplify, we'll do an extra firq every other frame.
				if ((i%34) == 33) decobsmt_firq_interrupt(); // 8 (per frame)
			} else {
				if ((i%30) == 29) decobsmt_firq_interrupt(); // 9
			}

			CPU_RUN(2, tms32010);
			M6809Close();
		}

		deco_irq_scanline_callback(i); // iq_132 - ok?

		if (i == 8) deco16_vblank = 0;

		if (i == 248) {
			if (game_select == 1 || game_select == 2 || game_select == 3) irq_callback(1);
			deco16_vblank = 1;
		}
	}

	if (pBurnSoundOut) {
		decobsmt_update();
	}

	ArmClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029722;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ArmScan(nAction);

		(use_z80) ? deco32_z80_sound_scan(nAction, pnMin) : deco16SoundScan(nAction, pnMin);

		deco16Scan();

		if (game_select == 3) {
			tattass_eeprom_scan();
			decobsmt_scan(nAction, pnMin);
		} else {
			EEPROMScan(nAction, pnMin);
		}

		if (game_select == 4) {
			BurnRandomScan(nAction);
		}

		if (uses_gun) {
			BurnGunScan();
		}

		SCAN_VAR(DrvOkiBank);
		SCAN_VAR(global_priority);
		SCAN_VAR(raster_irq_target);
		SCAN_VAR(raster_irq_masked);
		SCAN_VAR(raster_irq);
		SCAN_VAR(vblank_irq);
		SCAN_VAR(lightgun_irq);
		SCAN_VAR(raster_irq_scanline);
		SCAN_VAR(lightgun_latch);
		SCAN_VAR(sprite_ctrl);
		SCAN_VAR(lightgun_port);
		SCAN_VAR(color_base);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		if (game_select != 3) {
			DrvYM2151WritePort(0, DrvOkiBank);
		}
	}

	if (nAction & ACB_NVRAM) {
		if (game_select == 3) {
			SCAN_VAR(m_eeprom);
		}
	}

	return 0;
}


// Captain America and The Avengers (Asia Rev 1.4)

static struct BurnRomInfo captavenRomDesc[] = {
	{ "hn_00-4.1e",		0x020000, 0x147fb094, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "hn_01-4.1h",		0x020000, 0x11ecdb95, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hn_02-4.1k",		0x020000, 0x35d2681f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hn_03-4.1m",		0x020000, 0x3b59ba05, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "man-12.3e",		0x020000, 0xd6261e98, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "man-13.3h",		0x020000, 0x40f0764d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "man-14.3k",		0x020000, 0x7cb9a4bd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "man-15.3m",		0x020000, 0xc7854fe8, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "hj_08.17k",		0x010000, 0x361fbd16, 2 | BRF_PRG | BRF_ESS }, //  8 HUC6280 Code

	{ "man-00.8a",		0x080000, 0x7855a607, 3 | BRF_GRA },           //  9 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "man-05.16a",		0x100000, 0xd44d1995, 4 | BRF_GRA },           // 10 Tilemap 2&3 Tiles (Encrypted)
	{ "man-04.14a",		0x100000, 0x541492a1, 4 | BRF_GRA },           // 11
	{ "man-03.12a",		0x100000, 0x2d9c52b2, 4 | BRF_GRA },           // 12
	{ "man-02.11a",		0x100000, 0x07674c05, 4 | BRF_GRA },           // 13
	{ "man-01.10a",		0x100000, 0xae714ada, 4 | BRF_GRA },           // 14

	{ "man-06.17a",		0x100000, 0xa9a64297, 5 | BRF_GRA },           // 15 Sprites (chip #0)
	{ "man-07.18a",		0x100000, 0xb1db200c, 5 | BRF_GRA },           // 16
	{ "man-08.17c",		0x100000, 0x28e98e66, 5 | BRF_GRA },           // 17
	{ "man-09.21c",		0x100000, 0x1921245d, 5 | BRF_GRA },           // 18

	{ "man-10.14k",		0x080000, 0x0132c578, 6 | BRF_SND },           // 19 MSM6295 #1 Samples

	{ "man-11.16k",		0x080000, 0x0dc60a4c, 7 | BRF_SND },           // 20 MSM6295 #0 Samples

	{ "ts-00.4h",		0x000117, 0xebc2908e, 8 | BRF_OPT },           // 21 PLDs
	{ "ts-01.5h",		0x000117, 0xc776a980, 8 | BRF_OPT },           // 22
	{ "ts-02.12l",		0x0001bf, 0x6f26528c, 8 | BRF_OPT },           // 23
};

STD_ROM_PICK(captaven)
STD_ROM_FN(captaven)

static INT32 CaptavenInit()
{
	pStartDraw = CaptavenStartDraw;
	pDrawScanline = CaptavenDrawScanline;

	return CaptavenCommonInit(0, 0x39e8);
}

struct BurnDriver BurnDrvCaptaven = {
	"captaven", NULL, NULL, NULL, "1991",
	"Captain America and The Avengers (Asia Rev 1.4)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, captavenRomInfo, captavenRomName, NULL, NULL, NULL, NULL, CaptavenInputInfo, CaptavenDIPInfo,
	CaptavenInit, DrvExit, DrvFrame, CaptavenDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Captain America and The Avengers (Asia Rev 1.0)

static struct BurnRomInfo captavenaRomDesc[] = {
	{ "hn_00.e1",		0x020000, 0x12dd0c71, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "hn_01.h1",		0x020000, 0xac5ea492, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hn_02.k1",		0x020000, 0x0c5e13f6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hn_03.l1",		0x020000, 0xbc050740, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "man-12.3e",		0x020000, 0xd6261e98, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "man-13.3h",		0x020000, 0x40f0764d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "man-14.3k",		0x020000, 0x7cb9a4bd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "man-15.3m",		0x020000, 0xc7854fe8, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "hj_08.17k",		0x010000, 0x361fbd16, 2 | BRF_PRG | BRF_ESS }, //  8 HUC6280 Code

	{ "man-00.8a",		0x080000, 0x7855a607, 3 | BRF_GRA },           //  9 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "man-05.16a",		0x100000, 0xd44d1995, 4 | BRF_GRA },           // 10 Tilemap 2&3 Tiles (Encrypted)
	{ "man-04.14a",		0x100000, 0x541492a1, 4 | BRF_GRA },           // 11
	{ "man-03.12a",		0x100000, 0x2d9c52b2, 4 | BRF_GRA },           // 12
	{ "man-02.11a",		0x100000, 0x07674c05, 4 | BRF_GRA },           // 13
	{ "man-01.10a",		0x100000, 0xae714ada, 4 | BRF_GRA },           // 14

	{ "man-06.17a",		0x100000, 0xa9a64297, 5 | BRF_GRA },           // 15 Sprites (chip #0)
	{ "man-07.18a",		0x100000, 0xb1db200c, 5 | BRF_GRA },           // 16
	{ "man-08.17c",		0x100000, 0x28e98e66, 5 | BRF_GRA },           // 17
	{ "man-09.21c",		0x100000, 0x1921245d, 5 | BRF_GRA },           // 18

	{ "man-10.14k",		0x080000, 0x0132c578, 6 | BRF_SND },           // 19 MSM6295 #1 Samples

	{ "man-11.16k",		0x080000, 0x0dc60a4c, 7 | BRF_SND },           // 20 MSM6295 #0 Samples

	{ "ts-00.4h",		0x000117, 0xebc2908e, 8 | BRF_OPT },           // 21 PLDs
	{ "ts-01.5h",		0x000117, 0xc776a980, 8 | BRF_OPT },           // 22
	{ "ts-02.12l",		0x0001bf, 0x6f26528c, 8 | BRF_OPT },           // 23
};

STD_ROM_PICK(captavena)
STD_ROM_FN(captavena)

struct BurnDriver BurnDrvCaptavena = {
	"captavena", "captaven", NULL, NULL, "1991",
	"Captain America and The Avengers (Asia Rev 1.0)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, captavenaRomInfo, captavenaRomName, NULL, NULL, NULL, NULL, CaptavenInputInfo, CaptavenDIPInfo,
	CaptavenInit, DrvExit, DrvFrame, CaptavenDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Captain America and The Avengers (UK Rev 1.4)

static struct BurnRomInfo captaveneRomDesc[] = {
	{ "hg_00-4.1e",		0x020000, 0x7008d43c, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "hg_01-4.1h",		0x020000, 0x53dc1042, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hg_02-4.1k",		0x020000, 0x9e3f9ee2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hg_03-4.1m",		0x020000, 0xbc050740, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "man-12.3e",		0x020000, 0xd6261e98, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "man-13.3h",		0x020000, 0x40f0764d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "man-14.3k",		0x020000, 0x7cb9a4bd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "man-15.3m",		0x020000, 0xc7854fe8, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "hj_08.17k",		0x010000, 0x361fbd16, 2 | BRF_PRG | BRF_ESS }, //  8 HUC6280 Code

	{ "man-00.8a",		0x080000, 0x7855a607, 3 | BRF_GRA },           //  9 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "man-05.16a",		0x100000, 0xd44d1995, 4 | BRF_GRA },           // 10 Tilemap 2&3 Tiles (Encrypted)
	{ "man-04.14a",		0x100000, 0x541492a1, 4 | BRF_GRA },           // 11
	{ "man-03.12a",		0x100000, 0x2d9c52b2, 4 | BRF_GRA },           // 12
	{ "man-02.11a",		0x100000, 0x07674c05, 4 | BRF_GRA },           // 13
	{ "man-01.10a",		0x100000, 0xae714ada, 4 | BRF_GRA },           // 14

	{ "man-06.17a",		0x100000, 0xa9a64297, 5 | BRF_GRA },           // 15 Sprites (chip #0)
	{ "man-07.18a",		0x100000, 0xb1db200c, 5 | BRF_GRA },           // 16
	{ "man-08.17c",		0x100000, 0x28e98e66, 5 | BRF_GRA },           // 17
	{ "man-09.21c",		0x100000, 0x1921245d, 5 | BRF_GRA },           // 18

	{ "man-10.14k",		0x080000, 0x0132c578, 6 | BRF_SND },           // 19 MSM6295 #1 Samples

	{ "man-11.16k",		0x080000, 0x0dc60a4c, 7 | BRF_SND },           // 20 MSM6295 #0 Samples

	{ "ts-00.4h",		0x000117, 0xebc2908e, 8 | BRF_OPT },           // 21 PLDs
	{ "ts-01.5h",		0x000117, 0xc776a980, 8 | BRF_OPT },           // 22
	{ "ts-02.12l",		0x0001bf, 0x6f26528c, 8 | BRF_OPT },           // 23
	{ "pal16r8b.14c",	0x000104, 0x00000000, 8 | BRF_NODUMP | BRF_OPT }, // 24
};

STD_ROM_PICK(captavene)
STD_ROM_FN(captavene)

struct BurnDriver BurnDrvCaptavene = {
	"captavene", "captaven", NULL, NULL, "1991",
	"Captain America and The Avengers (UK Rev 1.4)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, captaveneRomInfo, captaveneRomName, NULL, NULL, NULL, NULL, CaptavenInputInfo, CaptavenDIPInfo,
	CaptavenInit, DrvExit, DrvFrame, CaptavenDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Captain America and The Avengers (US Rev 1.9)

static struct BurnRomInfo captavenuRomDesc[] = {
	{ "hh_00-19.1e",	0x020000, 0x08b870e0, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "hh_01-19.1h",	0x020000, 0x0dc0feca, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hh_02-19.1k",	0x020000, 0x26ef94c0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hn_03-4.1m",		0x020000, 0x3b59ba05, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "man-12.3e",		0x020000, 0xd6261e98, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "man-13.3h",		0x020000, 0x40f0764d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "man-14.3k",		0x020000, 0x7cb9a4bd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "man-15.3m",		0x020000, 0xc7854fe8, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "hj_08.17k",		0x010000, 0x361fbd16, 2 | BRF_PRG | BRF_ESS }, //  8 HUC6280 Code

	{ "man-00.8a",		0x080000, 0x7855a607, 3 | BRF_GRA },           //  9 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "man-05.16a",		0x100000, 0xd44d1995, 4 | BRF_GRA },           // 10 Tilemap 2&3 Tiles (Encrypted)
	{ "man-04.14a",		0x100000, 0x541492a1, 4 | BRF_GRA },           // 11
	{ "man-03.12a",		0x100000, 0x2d9c52b2, 4 | BRF_GRA },           // 12
	{ "man-02.11a",		0x100000, 0x07674c05, 4 | BRF_GRA },           // 13
	{ "man-01.10a",		0x100000, 0xae714ada, 4 | BRF_GRA },           // 14

	{ "man-06.17a",		0x100000, 0xa9a64297, 5 | BRF_GRA },           // 15 Sprites (chip #0)
	{ "man-07.18a",		0x100000, 0xb1db200c, 5 | BRF_GRA },           // 16
	{ "man-08.17c",		0x100000, 0x28e98e66, 5 | BRF_GRA },           // 17
	{ "man-09.21c",		0x100000, 0x1921245d, 5 | BRF_GRA },           // 18

	{ "man-10.14k",		0x080000, 0x0132c578, 6 | BRF_SND },           // 19 MSM6295 #1 Samples

	{ "man-11.16k",		0x080000, 0x0dc60a4c, 7 | BRF_SND },           // 20 MSM6295 #0 Samples

	{ "ts-00.4h",		0x000117, 0xebc2908e, 8 | BRF_OPT },           // 21 PLDs
	{ "ts-01.5h",		0x000117, 0xc776a980, 8 | BRF_OPT },           // 22
	{ "ts-02.12l",		0x0001bf, 0x6f26528c, 8 | BRF_OPT },           // 23
};

STD_ROM_PICK(captavenu)
STD_ROM_FN(captavenu)

struct BurnDriver BurnDrvCaptavenu = {
	"captavenu", "captaven", NULL, NULL, "1991",
	"Captain America and The Avengers (US Rev 1.9)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, captavenuRomInfo, captavenuRomName, NULL, NULL, NULL, NULL, CaptavenInputInfo, CaptavenDIPInfo,
	CaptavenInit, DrvExit, DrvFrame, CaptavenDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Captain America and The Avengers (US Rev 1.6)

static struct BurnRomInfo captavenuuRomDesc[] = {
	{ "hh-00.1e",		0x020000, 0xc34da654, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "hh-01.1h",		0x020000, 0x55abe63f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hh-02.1k",		0x020000, 0x6096a9fb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hh-03.1m",		0x020000, 0x93631ded, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "man-12.3e",		0x020000, 0xd6261e98, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "man-13.3h",		0x020000, 0x40f0764d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "man-14.3k",		0x020000, 0x7cb9a4bd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "man-15.3m",		0x020000, 0xc7854fe8, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "hj_08.17k",		0x010000, 0x361fbd16, 2 | BRF_PRG | BRF_ESS }, //  8 HUC6280 Code

	{ "man-00.8a",		0x080000, 0x7855a607, 3 | BRF_GRA },           //  9 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "man-05.16a",		0x100000, 0xd44d1995, 4 | BRF_GRA },           // 10 Tilemap 2&3 Tiles (Encrypted)
	{ "man-04.14a",		0x100000, 0x541492a1, 4 | BRF_GRA },           // 11
	{ "man-03.12a",		0x100000, 0x2d9c52b2, 4 | BRF_GRA },           // 12
	{ "man-02.11a",		0x100000, 0x07674c05, 4 | BRF_GRA },           // 13
	{ "man-01.10a",		0x100000, 0xae714ada, 4 | BRF_GRA },           // 14

	{ "man-06.17a",		0x100000, 0xa9a64297, 5 | BRF_GRA },           // 15 Sprites (chip #0)
	{ "man-07.18a",		0x100000, 0xb1db200c, 5 | BRF_GRA },           // 16
	{ "man-08.17c",		0x100000, 0x28e98e66, 5 | BRF_GRA },           // 17
	{ "man-09.21c",		0x100000, 0x1921245d, 5 | BRF_GRA },           // 18

	{ "man-10.14k",		0x080000, 0x0132c578, 6 | BRF_SND },           // 19 MSM6295 #1 Samples

	{ "man-11.16k",		0x080000, 0x0dc60a4c, 7 | BRF_SND },           // 20 MSM6295 #0 Samples

	{ "ts-00.4h",		0x000117, 0xebc2908e, 8 | BRF_OPT },           // 21 PLDs
	{ "ts-01.5h",		0x000117, 0xc776a980, 8 | BRF_OPT },           // 22
	{ "ts-02.12l",		0x0001bf, 0x6f26528c, 8 | BRF_OPT },           // 23
};

STD_ROM_PICK(captavenuu)
STD_ROM_FN(captavenuu)

struct BurnDriver BurnDrvCaptavenuu = {
	"captavenuu", "captaven", NULL, NULL, "1991",
	"Captain America and The Avengers (US Rev 1.6)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, captavenuuRomInfo, captavenuuRomName, NULL, NULL, NULL, NULL, CaptavenInputInfo, CaptavenDIPInfo,
	CaptavenInit, DrvExit, DrvFrame, CaptavenDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Captain America and The Avengers (US Rev 1.4)

static struct BurnRomInfo captavenuaRomDesc[] = {
	{ "hh_00-4.2e",		0x020000, 0x0e1acc05, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "hh_01-4.2h",		0x020000, 0x4ff0351d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hh_02-4.2k",		0x020000, 0xe84c0665, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hh_03-4.2m",		0x020000, 0xbc050740, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "man-12.3e",		0x020000, 0xd6261e98, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "man-13.3h",		0x020000, 0x40f0764d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "man-14.3k",		0x020000, 0x7cb9a4bd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "man-15.3m",		0x020000, 0xc7854fe8, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "hj_08.17k",		0x010000, 0x361fbd16, 2 | BRF_PRG | BRF_ESS }, //  8 HUC6280 Code

	{ "man-00.8a",		0x080000, 0x7855a607, 3 | BRF_GRA },           //  9 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "man-05.16a",		0x100000, 0xd44d1995, 4 | BRF_GRA },           // 10 Tilemap 2&3 Tiles (Encrypted)
	{ "man-04.14a",		0x100000, 0x541492a1, 4 | BRF_GRA },           // 11
	{ "man-03.12a",		0x100000, 0x2d9c52b2, 4 | BRF_GRA },           // 12
	{ "man-02.11a",		0x100000, 0x07674c05, 4 | BRF_GRA },           // 13
	{ "man-01.10a",		0x100000, 0xae714ada, 4 | BRF_GRA },           // 14

	{ "man-06.17a",		0x100000, 0xa9a64297, 5 | BRF_GRA },           // 15 Sprites (chip #0)
	{ "man-07.18a",		0x100000, 0xb1db200c, 5 | BRF_GRA },           // 16
	{ "man-08.17c",		0x100000, 0x28e98e66, 5 | BRF_GRA },           // 17
	{ "man-09.21c",		0x100000, 0x1921245d, 5 | BRF_GRA },           // 18

	{ "man-10.14k",		0x080000, 0x0132c578, 6 | BRF_SND },           // 19 MSM6295 #1 Samples

	{ "man-11.16k",		0x080000, 0x0dc60a4c, 7 | BRF_SND },           // 20 MSM6295 #0 Samples

	{ "ts-00.4h",		0x000117, 0xebc2908e, 0 | BRF_OPT },           // 21 PLDs
	{ "ts-01.5h",		0x000117, 0xc776a980, 0 | BRF_OPT },           // 22
	{ "ts-02.12l",		0x0001bf, 0x6f26528c, 0 | BRF_OPT },           // 23
};

STD_ROM_PICK(captavenua)
STD_ROM_FN(captavenua)

struct BurnDriver BurnDrvCaptavenua = {
	"captavenua", "captaven", NULL, NULL, "1991",
	"Captain America and The Avengers (US Rev 1.4)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, captavenuaRomInfo, captavenuaRomName, NULL, NULL, NULL, NULL, CaptavenInputInfo, CaptavenDIPInfo,
	CaptavenInit, DrvExit, DrvFrame, CaptavenDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Captain America and The Avengers (Japan Rev 0.2)

static struct BurnRomInfo captavenjRomDesc[] = {
	{ "hj_00-2.1e",		0x020000, 0x10b1faaf, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "hj_01-2.1h",		0x020000, 0x62c59f27, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hj_02-2.1k",		0x020000, 0xce946cad, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hj_03-2.1m",		0x020000, 0x140cf9ce, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "man-12.3e",		0x020000, 0xd6261e98, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "man-13.3h",		0x020000, 0x40f0764d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "man-14.3k",		0x020000, 0x7cb9a4bd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "man-15.3m",		0x020000, 0xc7854fe8, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "hj_08.17k",		0x010000, 0x361fbd16, 2 | BRF_PRG | BRF_ESS }, //  8 HUC6280 Code

	{ "man-00.8a",		0x080000, 0x7855a607, 3 | BRF_GRA },           //  9 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "man-05.16a",		0x100000, 0xd44d1995, 4 | BRF_GRA },           // 10 Tilemap 2&3 Tiles (Encrypted)
	{ "man-04.14a",		0x100000, 0x541492a1, 4 | BRF_GRA },           // 11
	{ "man-03.12a",		0x100000, 0x2d9c52b2, 4 | BRF_GRA },           // 12
	{ "man-02.11a",		0x100000, 0x07674c05, 4 | BRF_GRA },           // 13
	{ "man-01.10a",		0x100000, 0xae714ada, 4 | BRF_GRA },           // 14

	{ "man-06.17a",		0x100000, 0xa9a64297, 5 | BRF_GRA },           // 15 Sprites (chip #0)
	{ "man-07.18a",		0x100000, 0xb1db200c, 5 | BRF_GRA },           // 16
	{ "man-08.17c",		0x100000, 0x28e98e66, 5 | BRF_GRA },           // 17
	{ "man-09.21c",		0x100000, 0x1921245d, 5 | BRF_GRA },           // 18

	{ "man-10.14k",		0x080000, 0x0132c578, 6 | BRF_SND },           // 19 MSM6295 #1 Samples

	{ "man-11.16k",		0x080000, 0x0dc60a4c, 7 | BRF_SND },           // 20 MSM6295 #0 Samples

	{ "ts-00.4h",		0x000117, 0xebc2908e, 0 | BRF_OPT },           // 21 PLDs
	{ "ts-01.5h",		0x000117, 0xc776a980, 0 | BRF_OPT },           // 22
	{ "ts-02.12l",		0x0001bf, 0x6f26528c, 0 | BRF_OPT },           // 23
};

STD_ROM_PICK(captavenj)
STD_ROM_FN(captavenj)

struct BurnDriver BurnDrvCaptavenj = {
	"captavenj", "captaven", NULL, NULL, "1991",
	"Captain America and The Avengers (Japan Rev 0.2)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, captavenjRomInfo, captavenjRomName, NULL, NULL, NULL, NULL, CaptavenInputInfo, CaptavenDIPInfo,
	CaptavenInit, DrvExit, DrvFrame, CaptavenDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Fighter's History (World ver 43-09, DE-0395-1 PCB)

static struct BurnRomInfo fghthistRomDesc[] = {
	{ "lc00-1.1f",		0x080000, 0x61a76a16, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "lc01-1.2f",		0x080000, 0x6f2740d1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "lc02-1.18k",		0x010000, 0x5fd2309c, 2 | BRF_PRG | BRF_ESS }, //  2 HUC6280 Code

	{ "mbf00-8.8a",		0x100000, 0xd3e9b580, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbf01-8.9a",		0x100000, 0x0c6ed2eb, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbf02-16.16d",	0x200000, 0xc19c5953, 5 | BRF_GRA },           //  5 Sprites (chip #0)
	{ "mbf04-16.18d",	0x200000, 0xf6a23fd7, 5 | BRF_GRA },           //  6
	{ "mbf03-16.17d",	0x200000, 0x37d25c75, 5 | BRF_GRA },           //  7
	{ "mbf05-16.19d",	0x200000, 0x137be66d, 5 | BRF_GRA },           //  8

	{ "mbf06.15k",		0x080000, 0xfb513903, 6 | BRF_SND },           //  9 MSM6295 #0 Samples

	{ "mbf07.16k",		0x080000, 0x51d4adc7, 7 | BRF_SND },           // 10 MSM6295 #1 Samples

	{ "kt-00.8j",		0x000200, 0x7294354b, 0 | BRF_OPT },           // 11 Unused PROM

	{ "ve-00.3d",		0x000117, 0x384d316c, 0 | BRF_OPT },           // 12 PLDs
	{ "ve-01a.4d",		0x000117, 0x109613c8, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(fghthist)
STD_ROM_FN(fghthist)

static INT32 FghthistInit()
{
	return FghthistCommonInit(0, 0x9cf8);
}

struct BurnDriver BurnDrvFghthist = {
	"fghthist", NULL, NULL, NULL, "1993",
	"Fighter's History (World ver 43-09, DE-0395-1 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, fghthistRomInfo, fghthistRomName, NULL, NULL, NULL, NULL, FghthistInputInfo, FghthistDIPInfo,
	FghthistInit, DrvExit, DrvFrame, FghthistDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Fighter's History (World ver 43-07, DE-0380-2 PCB)

static struct BurnRomInfo fghthistaRomDesc[] = {
	{ "kx00-3.1f",		0x080000, 0xfe5eaba1, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "kx01-3.2f",		0x080000, 0x3fb8d738, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "kx02.18k",		0x010000, 0x5fd2309c, 2 | BRF_PRG | BRF_ESS }, //  2 HUC6280 Code

	{ "mbf00-8.8a",		0x100000, 0xd3e9b580, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbf01-8.9a",		0x100000, 0x0c6ed2eb, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbf02-16.16d",	0x200000, 0xc19c5953, 5 | BRF_GRA },           //  5 Sprites (chip #0)
	{ "mbf04-16.18d",	0x200000, 0xf6a23fd7, 5 | BRF_GRA },           //  6
	{ "mbf03-16.17d",	0x200000, 0x37d25c75, 5 | BRF_GRA },           //  7
	{ "mbf05-16.19d",	0x200000, 0x137be66d, 5 | BRF_GRA },           //  8

	{ "mbf06.15k",		0x080000, 0xfb513903, 6 | BRF_SND },           //  9 MSM6295 #0 Samples

	{ "mbf07.16k",		0x080000, 0x51d4adc7, 7 | BRF_SND },           // 10 MSM6295 #1 Samples

	{ "kt-00.8j",		0x000200, 0x7294354b, 0 | BRF_OPT },           // 11 Unused PROM

	{ "ve-00.3d",		0x000117, 0x384d316c, 0 | BRF_OPT },           // 12 PLDs
	{ "ve-01.4d",		0x000117, 0x4ba7e6a9, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(fghthista)
STD_ROM_FN(fghthista)

static INT32 FghthistaInit()
{
	return FghthistCommonInit(0, 0x9ca8);
}

struct BurnDriver BurnDrvFghthista = {
	"fghthista", "fghthist", NULL, NULL, "1993",
	"Fighter's History (World ver 43-07, DE-0380-2 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, fghthistaRomInfo, fghthistaRomName, NULL, NULL, NULL, NULL, FghthistInputInfo, FghthistDIPInfo,
	FghthistaInit, DrvExit, DrvFrame, FghthistDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Fighter's History (World ver 43-05, DE-0380-2 PCB)

static struct BurnRomInfo fghthistbRomDesc[] = {
	{ "kx00-2.1f",		0x080000, 0xa7c36bbd, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "kx01-2.2f",		0x080000, 0xbdc60bb1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "kx02.18k",		0x010000, 0x5fd2309c, 2 | BRF_PRG | BRF_ESS }, //  2 HUC6280 Code

	{ "mbf00-8.8a",		0x100000, 0xd3e9b580, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbf01-8.9a",		0x100000, 0x0c6ed2eb, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbf02-16.16d",	0x200000, 0xc19c5953, 5 | BRF_GRA },           //  5 Sprites (chip #0)
	{ "mbf04-16.18d",	0x200000, 0xf6a23fd7, 5 | BRF_GRA },           //  6
	{ "mbf03-16.17d",	0x200000, 0x37d25c75, 5 | BRF_GRA },           //  7
	{ "mbf05-16.19d",	0x200000, 0x137be66d, 5 | BRF_GRA },           //  8

	{ "mbf06.15k",		0x080000, 0xfb513903, 6 | BRF_SND },           //  9 MSM6295 #0 Samples

	{ "mbf07.16k",		0x080000, 0x51d4adc7, 7 | BRF_SND },           // 10 MSM6295 #1 Samples

	{ "kt-00.8j",		0x000200, 0x7294354b, 0 | BRF_OPT },           // 11 Unused PROM

	{ "ve-00.3d",		0x000117, 0x384d316c, 0 | BRF_OPT },           // 12 PLDs
	{ "ve-01.4d",		0x000117, 0x4ba7e6a9, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(fghthistb)
STD_ROM_FN(fghthistb)

static INT32 FghthistbInit()
{
	return FghthistCommonInit(0, 0x9c84);
}

struct BurnDriver BurnDrvFghthistb = {
	"fghthistb", "fghthist", NULL, NULL, "1993",
	"Fighter's History (World ver 43-05, DE-0380-2 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, fghthistbRomInfo, fghthistbRomName, NULL, NULL, NULL, NULL, FghthistInputInfo, FghthistDIPInfo,
	FghthistbInit, DrvExit, DrvFrame, FghthistDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Fighter's History (US ver 42-09, DE-0396-0 PCB)

static struct BurnRomInfo fghthistuRomDesc[] = {
	{ "lj00-3.1f",		0x080000, 0x17543d60, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "lj01-3.2f",		0x080000, 0xe255d48f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "lj02-.17k",		0x010000, 0x146a1063, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "mbf00-8.8a",		0x100000, 0xd3e9b580, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbf01-8.9a",		0x100000, 0x0c6ed2eb, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbf02-16.16d",	0x200000, 0xc19c5953, 5 | BRF_GRA },           //  5 Sprites (chip #0)
	{ "mbf04-16.18d",	0x200000, 0xf6a23fd7, 5 | BRF_GRA },           //  6
	{ "mbf03-16.17d",	0x200000, 0x37d25c75, 5 | BRF_GRA },           //  7
	{ "mbf05-16.19d",	0x200000, 0x137be66d, 5 | BRF_GRA },           //  8

	{ "mbf06.15k",		0x080000, 0xfb513903, 6 | BRF_SND },           //  9 MSM6295 #0 Samples

	{ "mbf07.16k",		0x080000, 0x51d4adc7, 7 | BRF_SND },           // 10 MSM6295 #1 Samples

	{ "kt-00.8j",		0x000200, 0x7294354b, 0 | BRF_OPT },           // 11 Unused PROM

	{ "ve-00.3d",		0x000117, 0x384d316c, 0 | BRF_OPT },           // 12 PLDs
	{ "ve-01.4d",		0x000117, 0x4ba7e6a9, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(fghthistu)
STD_ROM_FN(fghthistu)

static INT32 FghthistuInit()
{
	return FghthistCommonInit(1, 0x9cf8);
}

struct BurnDriver BurnDrvFghthistu = {
	"fghthistu", "fghthist", NULL, NULL, "1993",
	"Fighter's History (US ver 42-09, DE-0396-0 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, fghthistuRomInfo, fghthistuRomName, NULL, NULL, NULL, NULL, FghthistInputInfo, FghthistDIPInfo,
	FghthistuInit, DrvExit, DrvZ80Frame, FghthistDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Fighter's History (US ver 42-06, DE-0395-1 PCB)

static struct BurnRomInfo fghthistuaRomDesc[] = {
	{ "le00-1.1f",		0x080000, 0xfccacafb, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "le01-1.2f",		0x080000, 0x06a3c326, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "le02.18k",		0x010000, 0x5fd2309c, 2 | BRF_PRG | BRF_ESS }, //  2 HUC6280 Code

	{ "mbf00-8.8a",		0x100000, 0xd3e9b580, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbf01-8.9a",		0x100000, 0x0c6ed2eb, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbf02-16.16d",	0x200000, 0xc19c5953, 5 | BRF_GRA },           //  5 Sprites (chip #0)
	{ "mbf04-16.18d",	0x200000, 0xf6a23fd7, 5 | BRF_GRA },           //  6
	{ "mbf03-16.17d",	0x200000, 0x37d25c75, 5 | BRF_GRA },           //  7
	{ "mbf05-16.19d",	0x200000, 0x137be66d, 5 | BRF_GRA },           //  8

	{ "mbf06.15k",		0x080000, 0xfb513903, 6 | BRF_SND },           //  9 MSM6295 #0 Samples

	{ "mbf07.16k",		0x080000, 0x51d4adc7, 7 | BRF_SND },           // 10 MSM6295 #1 Samples

	{ "kt-00.8j",		0x000200, 0x7294354b, 0 | BRF_OPT },           // 11 Unused PROM

	{ "ve-00.3d",		0x000117, 0x384d316c, 0 | BRF_OPT },           // 12 PLDs
	{ "ve-01a.4d",		0x000117, 0x109613c8, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(fghthistua)
STD_ROM_FN(fghthistua)

static INT32 FghthistuaInit()
{
	return FghthistCommonInit(0, 0x9ce8);
}

struct BurnDriver BurnDrvFghthistua = {
	"fghthistua", "fghthist", NULL, NULL, "1993",
	"Fighter's History (US ver 42-06, DE-0395-1 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, fghthistuaRomInfo, fghthistuaRomName, NULL, NULL, NULL, NULL, FghthistInputInfo, FghthistDIPInfo,
	FghthistuaInit, DrvExit, DrvFrame, FghthistDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Fighter's History (US ver 42-05, DE-0395-1 PCB)

static struct BurnRomInfo fghthistubRomDesc[] = {
	{ "le00.1f",		0x080000, 0xa5c410eb, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "le01.2f",		0x080000, 0x7e148aa2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "le02.18k",		0x010000, 0x5fd2309c, 2 | BRF_PRG | BRF_ESS }, //  2 HUC6280 Code

	{ "mbf00-8.8a",		0x100000, 0xd3e9b580, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbf01-8.9a",		0x100000, 0x0c6ed2eb, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbf02-16.16d",	0x200000, 0xc19c5953, 5 | BRF_GRA },           //  5 Sprites (chip #0)
	{ "mbf04-16.18d",	0x200000, 0xf6a23fd7, 5 | BRF_GRA },           //  6
	{ "mbf03-16.17d",	0x200000, 0x37d25c75, 5 | BRF_GRA },           //  7
	{ "mbf05-16.19d",	0x200000, 0x137be66d, 5 | BRF_GRA },           //  8

	{ "mbf06.15k",		0x080000, 0xfb513903, 6 | BRF_SND },           //  9 MSM6295 #0 Samples

	{ "mbf07.16k",		0x080000, 0x51d4adc7, 7 | BRF_SND },           // 10 MSM6295 #1 Samples

	{ "kt-00.8j",		0x000200, 0x7294354b, 0 | BRF_OPT },           // 11 Unused PROM

	{ "ve-00.3d",		0x000117, 0x384d316c, 0 | BRF_OPT },           // 12 PLDs
	{ "ve-01a.4d",		0x000117, 0x109613c8, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(fghthistub)
STD_ROM_FN(fghthistub)

static INT32 FghthistubInit()
{
	return FghthistCommonInit(0, 0x9cf4);
}

struct BurnDriver BurnDrvFghthistub = {
	"fghthistub", "fghthist", NULL, NULL, "1993",
	"Fighter's History (US ver 42-05, DE-0395-1 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, fghthistubRomInfo, fghthistubRomName, NULL, NULL, NULL, NULL, FghthistInputInfo, FghthistDIPInfo,
	FghthistubInit, DrvExit, DrvFrame, FghthistDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Fighter's History (US ver 42-03, DE-0380-2 PCB)

static struct BurnRomInfo fghthistucRomDesc[] = {
	{ "kz00-1.1f",		0x080000, 0x3a3dd15c, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "kz01-1.2f",		0x080000, 0x86796cd6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "kz02.18k",		0x010000, 0x5fd2309c, 2 | BRF_PRG | BRF_ESS }, //  2 HUC6280 Code

	{ "mbf00-8.8a",		0x100000, 0xd3e9b580, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbf01-8.9a",		0x100000, 0x0c6ed2eb, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbf02-16.16d",	0x200000, 0xc19c5953, 5 | BRF_GRA },           //  5 Sprites (chip #0)
	{ "mbf04-16.18d",	0x200000, 0xf6a23fd7, 5 | BRF_GRA },           //  6
	{ "mbf03-16.17d",	0x200000, 0x37d25c75, 5 | BRF_GRA },           //  7
	{ "mbf05-16.19d",	0x200000, 0x137be66d, 5 | BRF_GRA },           //  8

	{ "mbf06.15k",		0x080000, 0xfb513903, 6 | BRF_SND },           //  9 MSM6295 #0 Samples

	{ "mbf07.16k",		0x080000, 0x51d4adc7, 7 | BRF_SND },           // 10 MSM6295 #1 Samples

	{ "kt-00.8j",		0x000200, 0x7294354b, 0 | BRF_OPT },           // 11 Unused PROM

	{ "ve-00.3d",		0x000117, 0x384d316c, 0 | BRF_OPT },           // 12 PLDs
	{ "ve-01.4d",		0x000117, 0x4ba7e6a9, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(fghthistuc)
STD_ROM_FN(fghthistuc)

static INT32 FghthistucInit()
{
	return FghthistCommonInit(0, 0x9ca8);
}

struct BurnDriver BurnDrvFghthistuc = {
	"fghthistuc", "fghthist", NULL, NULL, "1993",
	"Fighter's History (US ver 42-03, DE-0380-2 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, fghthistucRomInfo, fghthistucRomName, NULL, NULL, NULL, NULL, FghthistInputInfo, FghthistDIPInfo,
	FghthistucInit, DrvExit, DrvFrame, FghthistDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Fighter's History (Japan ver 41-07, DE-0395-1 PCB)

static struct BurnRomInfo fghthistjRomDesc[] = {
	{ "lb00.1f",		0x080000, 0x321099ad, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "lb01.2f",		0x080000, 0x22f45755, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "lb02.18k",		0x010000, 0x5fd2309c, 2 | BRF_PRG | BRF_ESS }, //  2 HUC6280 Code

	{ "mbf00-8.8a",		0x100000, 0xd3e9b580, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbf01-8.9a",		0x100000, 0x0c6ed2eb, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbf02-16.16d",	0x200000, 0xc19c5953, 5 | BRF_GRA },           //  5 Sprites (chip #0)
	{ "mbf04-16.18d",	0x200000, 0xf6a23fd7, 5 | BRF_GRA },           //  6
	{ "mbf03-16.17d",	0x200000, 0x37d25c75, 5 | BRF_GRA },           //  7
	{ "mbf05-16.19d",	0x200000, 0x137be66d, 5 | BRF_GRA },           //  8

	{ "mbf06.15k",		0x080000, 0xfb513903, 6 | BRF_SND },           //  9 MSM6295 #0 Samples

	{ "mbf07.16k",		0x080000, 0x51d4adc7, 7 | BRF_SND },           // 10 MSM6295 #1 Samples

	{ "kt-00.8j",		0x000200, 0x7294354b, 0 | BRF_OPT },           // 11 Unused PROM

	{ "ve-00.3d",		0x000117, 0x384d316c, 0 | BRF_OPT },           // 12 PLDs
	{ "ve-01a.4d",		0x000117, 0x109613c8, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(fghthistj)
STD_ROM_FN(fghthistj)

struct BurnDriver BurnDrvFghthistj = {
	"fghthistj", "fghthist", NULL, NULL, "1993",
	"Fighter's History (Japan ver 41-07, DE-0395-1 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, fghthistjRomInfo, fghthistjRomName, NULL, NULL, NULL, NULL, FghthistInputInfo, FghthistDIPInfo,
	FghthistInit, DrvExit, DrvFrame, FghthistDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Fighter's History (Japan ver 41-05, DE-0380-2 PCB)

static struct BurnRomInfo fghthistjaRomDesc[] = {
	{ "kw00-3.1f",		0x080000, 0xade9581a, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "kw01-3.2f",		0x080000, 0x63580acf, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "kw02-.18k",		0x010000, 0x5fd2309c, 2 | BRF_PRG | BRF_ESS }, //  2 HUC6280 Code

	{ "mbf00-8.8a",		0x100000, 0xd3e9b580, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbf01-8.9a",		0x100000, 0x0c6ed2eb, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbf02-16.16d",	0x200000, 0xc19c5953, 5 | BRF_GRA },           //  5 Sprites (chip #0)
	{ "mbf04-16.18d",	0x200000, 0xf6a23fd7, 5 | BRF_GRA },           //  6
	{ "mbf03-16.17d",	0x200000, 0x37d25c75, 5 | BRF_GRA },           //  7
	{ "mbf05-16.19d",	0x200000, 0x137be66d, 5 | BRF_GRA },           //  8

	{ "mbf06.15k",		0x080000, 0xfb513903, 6 | BRF_SND },           //  9 MSM6295 #0 Samples

	{ "mbf07.16k",		0x080000, 0x51d4adc7, 7 | BRF_SND },           // 10 MSM6295 #1 Samples

	{ "kt-00.8j",		0x000200, 0x7294354b, 8 | BRF_GRA },           // 11 Unused PROM

	{ "ve-00.3d",		0x000117, 0x384d316c, 0 | BRF_OPT },           // 12 PLDs
	{ "ve-01.4d",		0x000117, 0x4ba7e6a9, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(fghthistja)
STD_ROM_FN(fghthistja)

static INT32 FghthistjaInit()
{
	return FghthistCommonInit(0, 0x9ca8);
}

struct BurnDriver BurnDrvFghthistja = {
	"fghthistja", "fghthist", NULL, NULL, "1993",
	"Fighter's History (Japan ver 41-05, DE-0380-2 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, fghthistjaRomInfo, fghthistjaRomName, NULL, NULL, NULL, NULL, FghthistInputInfo, FghthistDIPInfo,
	FghthistjaInit, DrvExit, DrvFrame, FghthistDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Fighter's History (Japan ver 41-04, DE-0380-1 PCB)

static struct BurnRomInfo fghthistjbRomDesc[] = {
	{ "kw00-2.1f",		0x080000, 0xf4749806, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "kw01-2.2f",		0x080000, 0x7e0ee66a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "kw02-.18k",		0x010000, 0x5fd2309c, 2 | BRF_PRG | BRF_ESS }, //  2 HUC6280 Code

	{ "mbf00-8.8a",		0x100000, 0xd3e9b580, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbf01-8.9a",		0x100000, 0x0c6ed2eb, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbf02-16.16d",	0x200000, 0xc19c5953, 5 | BRF_GRA },           //  5 Sprites (chip #0)
	{ "mbf04-16.18d",	0x200000, 0xf6a23fd7, 5 | BRF_GRA },           //  6
	{ "mbf03-16.17d",	0x200000, 0x37d25c75, 5 | BRF_GRA },           //  7
	{ "mbf05-16.19d",	0x200000, 0x137be66d, 5 | BRF_GRA },           //  8

	{ "mbf06.15k",		0x080000, 0xfb513903, 6 | BRF_SND },           //  9 MSM6295 #0 Samples

	{ "mbf07.16k",		0x080000, 0x51d4adc7, 7 | BRF_SND },           // 10 MSM6295 #1 Samples

	{ "kt-00.8j",		0x000200, 0x7294354b, 8 | BRF_GRA },           // 11 Unused PROM

	{ "ve-00.3d",		0x000117, 0x384d316c, 0 | BRF_OPT },           // 12 PLDs
	{ "ve-01.4d",		0x000117, 0x4ba7e6a9, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(fghthistjb)
STD_ROM_FN(fghthistjb)

static INT32 FghthistjbInit()
{
	return FghthistCommonInit(0, 0x9c84);
}

struct BurnDriver BurnDrvFghthistjb = {
	"fghthistjb", "fghthist", NULL, NULL, "1993",
	"Fighter's History (Japan ver 41-04, DE-0380-1 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, fghthistjbRomInfo, fghthistjbRomName, NULL, NULL, NULL, NULL, FghthistInputInfo, FghthistDIPInfo,
	FghthistjbInit, DrvExit, DrvFrame, FghthistDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Night Slashers (Korea Rev 1.3, DE-0397-0 PCB)

static struct BurnRomInfo nslasherRomDesc[] = {
	{ "mainprg.1f",		0x080000, 0x507acbae, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code (Encrypted)
	{ "mainprg.2f",		0x080000, 0x931fc7ee, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sndprg.17l",		0x010000, 0x18939e92, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "mbh-00.8c",		0x200000, 0xa877f8a3, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbh-01.9c",		0x200000, 0x1853dafc, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbh-02.14c",		0x200000, 0xb2f158a1, 5 | BRF_GRA },           //  5 Sprites (chip #1)
	{ "mbh-04.16c",		0x200000, 0xeecfe06d, 5 | BRF_GRA },           //  6
	{ "mbh-03.15c",		0x080000, 0x787787e3, 5 | BRF_GRA },           //  7
	{ "mbh-05.17c",		0x080000, 0x1d2b7c17, 5 | BRF_GRA },           //  8
	{ "mbh-06.18c",		0x100000, 0x038c2127, 5 | BRF_GRA },           //  9
	{ "mbh-07.19c",		0x040000, 0xbbd22323, 5 | BRF_GRA },           // 10

	{ "mbh-08.16e",		0x080000, 0xcdd7f8cb, 6 | BRF_GRA },           // 11 Sprites (chip #0)
	{ "mbh-09.18e",		0x080000, 0x33fa2121, 6 | BRF_GRA },           // 12

	{ "mbh-10.14l",		0x080000, 0xc4d6b116, 7 | BRF_SND },           // 13 MSM6295 #0 Samples

	{ "mbh-11.16l",		0x080000, 0x0ec40b6b, 8 | BRF_SND },           // 14 MSM6295 #1 Samples

	{ "ln-00.j7",		0x000200, 0x5e83eaf3, 0 | BRF_OPT },           // 15 Unused PROM

	{ "vm-00.3d",		0x000117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 16 PLDs
	{ "vm-01.4d",		0x000117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 17
	{ "vm-02.8j",		0x000117, 0x53692426, 0 | BRF_OPT },           // 18
};

STD_ROM_PICK(nslasher)
STD_ROM_FN(nslasher)

static INT32 NslasherInit()
{
	return NslasherCommonInit(1, 0x9c8);
}

struct BurnDriver BurnDrvNslasher = {
	"nslasher", NULL, NULL, NULL, "1994",
	"Night Slashers (Korea Rev 1.3, DE-0397-0 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 3, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, nslasherRomInfo, nslasherRomName, NULL, NULL, NULL, NULL, NslasherInputInfo, NslasherDIPInfo,
	NslasherInit, DrvExit, DrvZ80Frame, NslasherDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Night Slashers (Japan Rev 1.2, DE-0397-0 PCB)

static struct BurnRomInfo nslasherjRomDesc[] = {
	{ "lx-00.1f",		0x080000, 0x6ed5fb88, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code (Encrypted)
	{ "lx-01.2f",		0x080000, 0xa6df2152, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sndprg.17l",		0x010000, 0x18939e92, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "mbh-00.8c",		0x200000, 0xa877f8a3, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbh-01.9c",		0x200000, 0x1853dafc, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbh-02.14c",		0x200000, 0xb2f158a1, 5 | BRF_GRA },           //  5 Sprites (chip #1)
	{ "mbh-04.16c",		0x200000, 0xeecfe06d, 5 | BRF_GRA },           //  6
	{ "mbh-03.15c",		0x080000, 0x787787e3, 5 | BRF_GRA },           //  7
	{ "mbh-05.17c",		0x080000, 0x1d2b7c17, 5 | BRF_GRA },           //  8
	{ "mbh-06.18c",		0x100000, 0x038c2127, 5 | BRF_GRA },           //  9
	{ "mbh-07.19c",		0x040000, 0xbbd22323, 5 | BRF_GRA },           // 10

	{ "mbh-08.16e",		0x080000, 0xcdd7f8cb, 6 | BRF_GRA },           // 11 Sprites (chip #0)
	{ "mbh-09.18e",		0x080000, 0x33fa2121, 6 | BRF_GRA },           // 12

	{ "mbh-10.14l",		0x080000, 0xc4d6b116, 7 | BRF_SND },           // 13 MSM6295 #0 Samples

	{ "mbh-11.16l",		0x080000, 0x0ec40b6b, 8 | BRF_SND },           // 14 MSM6295 #1 Samples

	{ "ln-00.j7",		0x000200, 0x5e83eaf3, 0 | BRF_OPT },           // 15 Unused PROM

	{ "vm-00.3d",		0x000117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 16 PLDs
	{ "vm-01.4d",		0x000117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 17
	{ "vm-02.8j",		0x000117, 0x53692426, 0 | BRF_OPT },           // 18
};

STD_ROM_PICK(nslasherj)
STD_ROM_FN(nslasherj)

static INT32 NslasherjInit()
{
	return NslasherCommonInit(1, 0xa84);
}


struct BurnDriver BurnDrvNslasherj = {
	"nslasherj", "nslasher", NULL, NULL, "1994",
	"Night Slashers (Japan Rev 1.2, DE-0397-0 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 3, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, nslasherjRomInfo, nslasherjRomName, NULL, NULL, NULL, NULL, NslasherInputInfo, NslasherDIPInfo,
	NslasherjInit, DrvExit, DrvZ80Frame, NslasherDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Night Slashers (Over Sea Rev 1.2, DE-0397-0 PCB)

static struct BurnRomInfo nslashersRomDesc[] = {
	{ "ly-00.1f",		0x080000, 0xfa0646f9, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code (Encrypted)
	{ "ly-01.2f",		0x080000, 0xae508149, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sndprg.17l",		0x010000, 0x18939e92, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "mbh-00.8c",		0x200000, 0xa877f8a3, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbh-01.9c",		0x200000, 0x1853dafc, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbh-02.14c",		0x200000, 0xb2f158a1, 5 | BRF_GRA },           //  5 Sprites (chip #1)
	{ "mbh-04.16c",		0x200000, 0xeecfe06d, 5 | BRF_GRA },           //  6
	{ "mbh-03.15c",		0x080000, 0x787787e3, 5 | BRF_GRA },           //  7
	{ "mbh-05.17c",		0x080000, 0x1d2b7c17, 5 | BRF_GRA },           //  8
	{ "mbh-06.18c",		0x100000, 0x038c2127, 5 | BRF_GRA },           //  9
	{ "mbh-07.19c",		0x040000, 0xbbd22323, 5 | BRF_GRA },           // 10

	{ "mbh-08.16e",		0x080000, 0xcdd7f8cb, 6 | BRF_GRA },           // 11 Sprites (chip #0)
	{ "mbh-09.18e",		0x080000, 0x33fa2121, 6 | BRF_GRA },           // 12

	{ "mbh-10.14l",		0x080000, 0xc4d6b116, 7 | BRF_SND },           // 13 MSM6295 #0 Samples

	{ "mbh-11.16l",		0x080000, 0x0ec40b6b, 8 | BRF_SND },           // 14 MSM6295 #1 Samples

	{ "ln-00.j7",		0x000200, 0x5e83eaf3, 0 | BRF_OPT },           // 15 Unused PROM

	{ "vm-00.3d",		0x000117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 16 PLDs
	{ "vm-01.4d",		0x000117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, // 17
	{ "vm-02.8j",		0x000117, 0x53692426, 0 | BRF_OPT },           // 18
};

STD_ROM_PICK(nslashers)
STD_ROM_FN(nslashers)

struct BurnDriver BurnDrvNslashers = {
	"nslashers", "nslasher", NULL, NULL, "1994",
	"Night Slashers (Over Sea Rev 1.2, DE-0397-0 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 3, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, nslashersRomInfo, nslashersRomName, NULL, NULL, NULL, NULL, NslasherInputInfo, NslasherDIPInfo,
	NslasherInit, DrvExit, DrvZ80Frame, NslasherDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Night Slashers (US Rev 1.2, DE-0395-1 PCB)

static struct BurnRomInfo nslasheruRomDesc[] = {
	{ "00.f1",		0x080000, 0x944f3329, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code (Encrypted)
	{ "01.f2",		0x080000, 0xac12d18a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "02.l18",		0x010000, 0x5e63bd91, 2 | BRF_PRG | BRF_ESS }, //  2 HUC6280 Code

	{ "mbh-00.8c",		0x200000, 0xa877f8a3, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)

	{ "mbh-01.9c",		0x200000, 0x1853dafc, 4 | BRF_GRA },           //  4 Tilemap 2&3 Tiles (Encrypted)

	{ "mbh-02.14c",		0x200000, 0xb2f158a1, 5 | BRF_GRA },           //  5 Sprites (chip #1)
	{ "mbh-04.16c",		0x200000, 0xeecfe06d, 5 | BRF_GRA },           //  6
	{ "mbh-03.15c",		0x080000, 0x787787e3, 5 | BRF_GRA },           //  7
	{ "mbh-05.17c",		0x080000, 0x1d2b7c17, 5 | BRF_GRA },           //  8
	{ "mbh-06.18c",		0x100000, 0x038c2127, 5 | BRF_GRA },           //  9
	{ "mbh-07.19c",		0x040000, 0xbbd22323, 5 | BRF_GRA },           // 10

	{ "mbh-08.16e",		0x080000, 0xcdd7f8cb, 6 | BRF_GRA },           // 11 Sprites (chip #0)
	{ "mbh-09.18e",		0x080000, 0x33fa2121, 6 | BRF_GRA },           // 12

	{ "mbh-10.14l",		0x080000, 0xc4d6b116, 7 | BRF_SND },           // 13 MSM6295 #0 Samples

	{ "mbh-11.16l",		0x080000, 0x0ec40b6b, 8 | BRF_SND },           // 14 MSM6295 #1 Samples

	{ "ln-00.j7",		0x000200, 0x5e83eaf3, 0 | BRF_OPT },           // 15 Unused PROM

	{ "ve-00.3d",		0x000117, 0x384d316c, 0 | BRF_OPT },           // 16 PLDs
	{ "ve-01a.4d",		0x000117, 0x109613c8, 0 | BRF_OPT },           // 17
	{ "vm-02.8j",		0x000117, 0x53692426, 0 | BRF_OPT },           // 18
};

STD_ROM_PICK(nslasheru)
STD_ROM_FN(nslasheru)

static INT32 NslasheruInit()
{
	return NslasherCommonInit(0, 0x9e0);
}

struct BurnDriver BurnDrvNslasheru = {
	"nslasheru", "nslasher", NULL, NULL, "1994",
	"Night Slashers (US Rev 1.2, DE-0395-1 PCB)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 3, HARDWARE_PREFIX_DATAEAST, GBF_SCRFIGHT, 0,
	NULL, nslasheruRomInfo, nslasheruRomName, NULL, NULL, NULL, NULL, NslasherInputInfo, NslasherDIPInfo,
	NslasheruInit, DrvExit, DrvFrame, NslasherDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Tattoo Assassins (US prototype, Mar 14 1995)

static struct BurnRomInfo tattassRomDesc[] = {
	{ "pp44.cpu",		0x80000, 0xc3ca5b49, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "pp45.cpu",		0x80000, 0xd3f30de0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u7.snd",			0x10000, 0xa7228077, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "abak_b01.s02",	0x80000, 0xbc805680, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)
	{ "abak_b01.s13",	0x80000, 0x350effcd, 3 | BRF_GRA },           //  4
	{ "abak_b23.s02",	0x80000, 0x91abdc21, 3 | BRF_GRA },           //  5
	{ "abak_b23.s13",	0x80000, 0x80eb50fe, 3 | BRF_GRA },           //  6

	{ "bbak_b01.s02",	0x80000, 0x611be9a6, 4 | BRF_GRA },           //  7 Tilemap 2&3 Tiles (Encrypted)
	{ "bbak_b01.s13",	0x80000, 0x097e0604, 4 | BRF_GRA },           //  8
	{ "bbak_b23.s02",	0x80000, 0x3836531a, 4 | BRF_GRA },           //  9
	{ "bbak_b23.s13",	0x80000, 0x1210485a, 4 | BRF_GRA },           // 10

	{ "ob1_c0.b0",		0x80000, 0x053fecca, 5 | BRF_GRA },           // 11 Sprites bank 0
	{ "ob1_c1.b0",		0x80000, 0xe183e6bc, 5 | BRF_GRA },           // 12
	{ "ob1_c2.b0",		0x80000, 0x1314f828, 5 | BRF_GRA },           // 13
	{ "ob1_c3.b0",		0x80000, 0xc63866df, 5 | BRF_GRA },           // 14
	{ "ob1_c4.b0",		0x80000, 0xf71cdd1b, 5 | BRF_GRA },           // 15
	{ "ob1_c0.b1",		0x80000, 0x385434b0, 5 | BRF_GRA },           // 16
	{ "ob1_c1.b1",		0x80000, 0x0a3ec489, 5 | BRF_GRA },           // 17
	{ "ob1_c2.b1",		0x80000, 0x52f06081, 5 | BRF_GRA },           // 18
	{ "ob1_c3.b1",		0x80000, 0xa8a5cfbe, 5 | BRF_GRA },           // 19
	{ "ob1_c4.b1",		0x80000, 0x09d0acd6, 5 | BRF_GRA },           // 20
	{ "ob1_c0.b2",		0x80000, 0x946e9f59, 5 | BRF_GRA },           // 21
	{ "ob1_c1.b2",		0x80000, 0x9f66ad54, 5 | BRF_GRA },           // 22
	{ "ob1_c2.b2",		0x80000, 0xa8df60eb, 5 | BRF_GRA },           // 23
	{ "ob1_c3.b2",		0x80000, 0xa1a753be, 5 | BRF_GRA },           // 24
	{ "ob1_c4.b2",		0x80000, 0xb65b3c4b, 5 | BRF_GRA },           // 25
	{ "ob1_c0.b3",		0x80000, 0xcbbbc696, 5 | BRF_GRA },           // 26
	{ "ob1_c1.b3",		0x80000, 0xf7b1bdee, 5 | BRF_GRA },           // 27
	{ "ob1_c2.b3",		0x80000, 0x97815619, 5 | BRF_GRA },           // 28
	{ "ob1_c3.b3",		0x80000, 0xfc3ccb7a, 5 | BRF_GRA },           // 29
	{ "ob1_c4.b3",		0x80000, 0xdfdfd0ff, 5 | BRF_GRA },           // 30

	{ "ob2_c0.b0",		0x80000, 0x9080ebe4, 6 | BRF_GRA },           // 31 Sprites bank 1
	{ "ob2_c1.b0",		0x80000, 0xc0464970, 6 | BRF_GRA },           // 32
	{ "ob2_c2.b0",		0x80000, 0x35a2e621, 6 | BRF_GRA },           // 33
	{ "ob2_c3.b0",		0x80000, 0x99c7cc2d, 6 | BRF_GRA },           // 34
	{ "ob2_c0.b1",		0x80000, 0x2c2c15c9, 6 | BRF_GRA },           // 35
	{ "ob2_c1.b1",		0x80000, 0xd2c49a14, 6 | BRF_GRA },           // 36
	{ "ob2_c2.b1",		0x80000, 0xfbe957e8, 6 | BRF_GRA },           // 37
	{ "ob2_c3.b1",		0x80000, 0xd7238829, 6 | BRF_GRA },           // 38
	{ "ob2_c0.b2",		0x80000, 0xaefa1b01, 6 | BRF_GRA },           // 39
	{ "ob2_c1.b2",		0x80000, 0x4af620ca, 6 | BRF_GRA },           // 40
	{ "ob2_c2.b2",		0x80000, 0x8e58be07, 6 | BRF_GRA },           // 41
	{ "ob2_c3.b2",		0x80000, 0x1b5188c5, 6 | BRF_GRA },           // 42
	{ "ob2_c0.b3",		0x80000, 0xa2a5dafd, 6 | BRF_GRA },           // 43
	{ "ob2_c1.b3",		0x80000, 0x6f0afd05, 6 | BRF_GRA },           // 44
	{ "ob2_c2.b3",		0x80000, 0x90fe5f4f, 6 | BRF_GRA },           // 45
	{ "ob2_c3.b3",		0x80000, 0xe3517e6e, 6 | BRF_GRA },           // 46

	{ "u17.snd",		0x80000, 0x49303539, 7 | BRF_GRA },           // 47 BSMT Samples
	{ "u21.snd",		0x80000, 0x0ad8bc18, 7 | BRF_GRA },           // 48
	{ "u36.snd",		0x80000, 0xf558947b, 7 | BRF_GRA },           // 49
	{ "u37.snd",		0x80000, 0x7a3190bc, 7 | BRF_GRA },           // 50

	{ "eeprom-tattass.bin",	0x00400, 0x7140f40c, 8 | BRF_PRG | BRF_ESS }, // 51 Default Settings

#if !defined ROM_VERIFY
	{ "bsmt2000.bin",	0x02000, 0xc2a265af, 9 | BRF_PRG | BRF_ESS }, // 52 DSP Code
#endif
};

STD_ROM_PICK(tattass)
STD_ROM_FN(tattass)

static INT32 TattassInit()
{
	return TattassCommonInit(0, 0x1c5ec);
}

struct BurnDriver BurnDrvTattass = {
	"tattass", NULL, NULL, NULL, "1994",
	"Tattoo Assassins (US prototype, Mar 14 1995)\0", NULL, "Data East Pinball", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, tattassRomInfo, tattassRomName, NULL, NULL, NULL, NULL, TattassInputInfo, TattassDIPInfo,
	TattassInit, DrvExit, DrvBSMTFrame, NslasherDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Tattoo Assassins (US prototype, Mar 14 1995, older sound)

static struct BurnRomInfo tattassoRomDesc[] = {
	{ "pp44.cpu",		0x80000, 0xc3ca5b49, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "pp45.cpu",		0x80000, 0xd3f30de0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u7.snd",			0x10000, 0x6947be8a, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "abak_b01.s02",	0x80000, 0xbc805680, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)
	{ "abak_b01.s13",	0x80000, 0x350effcd, 3 | BRF_GRA },           //  4
	{ "abak_b23.s02",	0x80000, 0x91abdc21, 3 | BRF_GRA },           //  5
	{ "abak_b23.s13",	0x80000, 0x80eb50fe, 3 | BRF_GRA },           //  6

	{ "bbak_b01.s02",	0x80000, 0x611be9a6, 4 | BRF_GRA },           //  7 Tilemap 2&3 Tiles (Encrypted)
	{ "bbak_b01.s13",	0x80000, 0x097e0604, 4 | BRF_GRA },           //  8
	{ "bbak_b23.s02",	0x80000, 0x3836531a, 4 | BRF_GRA },           //  9
	{ "bbak_b23.s13",	0x80000, 0x1210485a, 4 | BRF_GRA },           // 10

	{ "ob1_c0.b0",		0x80000, 0x053fecca, 5 | BRF_GRA },           // 11 Sprites bank 0
	{ "ob1_c1.b0",		0x80000, 0xe183e6bc, 5 | BRF_GRA },           // 12
	{ "ob1_c2.b0",		0x80000, 0x1314f828, 5 | BRF_GRA },           // 13
	{ "ob1_c3.b0",		0x80000, 0xc63866df, 5 | BRF_GRA },           // 14
	{ "ob1_c4.b0",		0x80000, 0xf71cdd1b, 5 | BRF_GRA },           // 15
	{ "ob1_c0.b1",		0x80000, 0x385434b0, 5 | BRF_GRA },           // 16
	{ "ob1_c1.b1",		0x80000, 0x0a3ec489, 5 | BRF_GRA },           // 17
	{ "ob1_c2.b1",		0x80000, 0x52f06081, 5 | BRF_GRA },           // 18
	{ "ob1_c3.b1",		0x80000, 0xa8a5cfbe, 5 | BRF_GRA },           // 19
	{ "ob1_c4.b1",		0x80000, 0x09d0acd6, 5 | BRF_GRA },           // 20
	{ "ob1_c0.b2",		0x80000, 0x946e9f59, 5 | BRF_GRA },           // 21
	{ "ob1_c1.b2",		0x80000, 0x9f66ad54, 5 | BRF_GRA },           // 22
	{ "ob1_c2.b2",		0x80000, 0xa8df60eb, 5 | BRF_GRA },           // 23
	{ "ob1_c3.b2",		0x80000, 0xa1a753be, 5 | BRF_GRA },           // 24
	{ "ob1_c4.b2",		0x80000, 0xb65b3c4b, 5 | BRF_GRA },           // 25
	{ "ob1_c0.b3",		0x80000, 0xcbbbc696, 5 | BRF_GRA },           // 26
	{ "ob1_c1.b3",		0x80000, 0xf7b1bdee, 5 | BRF_GRA },           // 27
	{ "ob1_c2.b3",		0x80000, 0x97815619, 5 | BRF_GRA },           // 28
	{ "ob1_c3.b3",		0x80000, 0xfc3ccb7a, 5 | BRF_GRA },           // 29
	{ "ob1_c4.b3",		0x80000, 0xdfdfd0ff, 5 | BRF_GRA },           // 30

	{ "ob2_c0.b0",		0x80000, 0x9080ebe4, 6 | BRF_GRA },           // 31 Sprites bank 1
	{ "ob2_c1.b0",		0x80000, 0xc0464970, 6 | BRF_GRA },           // 32
	{ "ob2_c2.b0",		0x80000, 0x35a2e621, 6 | BRF_GRA },           // 33
	{ "ob2_c3.b0",		0x80000, 0x99c7cc2d, 6 | BRF_GRA },           // 34
	{ "ob2_c0.b1",		0x80000, 0x2c2c15c9, 6 | BRF_GRA },           // 35
	{ "ob2_c1.b1",		0x80000, 0xd2c49a14, 6 | BRF_GRA },           // 36
	{ "ob2_c2.b1",		0x80000, 0xfbe957e8, 6 | BRF_GRA },           // 37
	{ "ob2_c3.b1",		0x80000, 0xd7238829, 6 | BRF_GRA },           // 38
	{ "ob2_c0.b2",		0x80000, 0xaefa1b01, 6 | BRF_GRA },           // 39
	{ "ob2_c1.b2",		0x80000, 0x4af620ca, 6 | BRF_GRA },           // 40
	{ "ob2_c2.b2",		0x80000, 0x8e58be07, 6 | BRF_GRA },           // 41
	{ "ob2_c3.b2",		0x80000, 0x1b5188c5, 6 | BRF_GRA },           // 42
	{ "ob2_c0.b3",		0x80000, 0xa2a5dafd, 6 | BRF_GRA },           // 43
	{ "ob2_c1.b3",		0x80000, 0x6f0afd05, 6 | BRF_GRA },           // 44
	{ "ob2_c2.b3",		0x80000, 0x90fe5f4f, 6 | BRF_GRA },           // 45
	{ "ob2_c3.b3",		0x80000, 0xe3517e6e, 6 | BRF_GRA },           // 46

	{ "u17.snd",		0x80000, 0xb945c18d, 7 | BRF_GRA },           // 47 BSMT Samples
	{ "u21.snd",		0x80000, 0x10b2110c, 7 | BRF_GRA },           // 48
	{ "u36.snd",		0x80000, 0x3b73abe2, 7 | BRF_GRA },           // 49
	{ "u37.snd",		0x80000, 0x986066b5, 7 | BRF_GRA },           // 50

	{ "eeprom-tattass.bin",	0x00400, 0x7140f40c, 8 | BRF_PRG | BRF_ESS }, // 51 Default Settings

#if !defined ROM_VERIFY
	{ "bsmt2000.bin",	0x02000, 0xc2a265af, 9 | BRF_PRG | BRF_ESS }, // 52 DSP Code
#endif
};

STD_ROM_PICK(tattasso)
STD_ROM_FN(tattasso)

struct BurnDriver BurnDrvTattasso = {
	"tattasso", "tattass", NULL, NULL, "1994",
	"Tattoo Assassins (US prototype, Mar 14 1995, older sound)\0", NULL, "Data East Pinball", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, tattassoRomInfo, tattassoRomName, NULL, NULL, NULL, NULL, TattassInputInfo, TattassDIPInfo,
	TattassInit, DrvExit, DrvBSMTFrame, NslasherDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Tattoo Assassins (Asia prototype, Mar 14 1995)

static struct BurnRomInfo tattassaRomDesc[] = {
	{ "rev232a.000",	0x80000, 0x1a357112, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "rev232a.001",	0x80000, 0x550245d4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u7.snd",			0x10000, 0xa7228077, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "abak_b01.s02",	0x80000, 0xbc805680, 3 | BRF_GRA },           //  3 Tilemap 0&1 Tiles & Characters (Encrypted)
	{ "abak_b01.s13",	0x80000, 0x350effcd, 3 | BRF_GRA },           //  4
	{ "abak_b23.s02",	0x80000, 0x91abdc21, 3 | BRF_GRA },           //  5
	{ "abak_b23.s13",	0x80000, 0x80eb50fe, 3 | BRF_GRA },           //  6

	{ "bbak_b01.s02",	0x80000, 0x611be9a6, 4 | BRF_GRA },           //  7 Tilemap 2&3 Tiles (Encrypted)
	{ "bbak_b01.s13",	0x80000, 0x097e0604, 4 | BRF_GRA },           //  8
	{ "bbak_b23.s02",	0x80000, 0x3836531a, 4 | BRF_GRA },           //  9
	{ "bbak_b23.s13",	0x80000, 0x1210485a, 4 | BRF_GRA },           // 10

	{ "ob1_c0.b0",		0x80000, 0x053fecca, 5 | BRF_GRA },           // 11 Sprites bank 0
	{ "ob1_c1.b0",		0x80000, 0xe183e6bc, 5 | BRF_GRA },           // 12
	{ "ob1_c2.b0",		0x80000, 0x1314f828, 5 | BRF_GRA },           // 13
	{ "ob1_c3.b0",		0x80000, 0xc63866df, 5 | BRF_GRA },           // 14
	{ "ob1_c4.b0",		0x80000, 0xf71cdd1b, 5 | BRF_GRA },           // 15
	{ "ob1_c0.b1",		0x80000, 0x385434b0, 5 | BRF_GRA },           // 16
	{ "ob1_c1.b1",		0x80000, 0x0a3ec489, 5 | BRF_GRA },           // 17
	{ "ob1_c2.b1",		0x80000, 0x52f06081, 5 | BRF_GRA },           // 18
	{ "ob1_c3.b1",		0x80000, 0xa8a5cfbe, 5 | BRF_GRA },           // 19
	{ "ob1_c4.b1",		0x80000, 0x09d0acd6, 5 | BRF_GRA },           // 20
	{ "ob1_c0.b2",		0x80000, 0x946e9f59, 5 | BRF_GRA },           // 21
	{ "ob1_c1.b2",		0x80000, 0x9f66ad54, 5 | BRF_GRA },           // 22
	{ "ob1_c2.b2",		0x80000, 0xa8df60eb, 5 | BRF_GRA },           // 23
	{ "ob1_c3.b2",		0x80000, 0xa1a753be, 5 | BRF_GRA },           // 24
	{ "ob1_c4.b2",		0x80000, 0xb65b3c4b, 5 | BRF_GRA },           // 25
	{ "ob1_c0.b3",		0x80000, 0xcbbbc696, 5 | BRF_GRA },           // 26
	{ "ob1_c1.b3",		0x80000, 0xf7b1bdee, 5 | BRF_GRA },           // 27
	{ "ob1_c2.b3",		0x80000, 0x97815619, 5 | BRF_GRA },           // 28
	{ "ob1_c3.b3",		0x80000, 0xfc3ccb7a, 5 | BRF_GRA },           // 29
	{ "ob1_c4.b3",		0x80000, 0xdfdfd0ff, 5 | BRF_GRA },           // 30

	{ "ob2_c0.b0",		0x80000, 0x9080ebe4, 6 | BRF_GRA },           // 31 Sprites bank 1
	{ "ob2_c1.b0",		0x80000, 0xc0464970, 6 | BRF_GRA },           // 32
	{ "ob2_c2.b0",		0x80000, 0x35a2e621, 6 | BRF_GRA },           // 33
	{ "ob2_c3.b0",		0x80000, 0x99c7cc2d, 6 | BRF_GRA },           // 34
	{ "ob2_c0.b1",		0x80000, 0x2c2c15c9, 6 | BRF_GRA },           // 35
	{ "ob2_c1.b1",		0x80000, 0xd2c49a14, 6 | BRF_GRA },           // 36
	{ "ob2_c2.b1",		0x80000, 0xfbe957e8, 6 | BRF_GRA },           // 37
	{ "ob2_c3.b1",		0x80000, 0xd7238829, 6 | BRF_GRA },           // 38
	{ "ob2_c0.b2",		0x80000, 0xaefa1b01, 6 | BRF_GRA },           // 39
	{ "ob2_c1.b2",		0x80000, 0x4af620ca, 6 | BRF_GRA },           // 40
	{ "ob2_c2.b2",		0x80000, 0x8e58be07, 6 | BRF_GRA },           // 41
	{ "ob2_c3.b2",		0x80000, 0x1b5188c5, 6 | BRF_GRA },           // 42
	{ "ob2_c0.b3",		0x80000, 0xa2a5dafd, 6 | BRF_GRA },           // 43
	{ "ob2_c1.b3",		0x80000, 0x6f0afd05, 6 | BRF_GRA },           // 44
	{ "ob2_c2.b3",		0x80000, 0x90fe5f4f, 6 | BRF_GRA },           // 45
	{ "ob2_c3.b3",		0x80000, 0xe3517e6e, 6 | BRF_GRA },           // 46

	{ "u17.snd",		0x80000, 0x49303539, 7 | BRF_GRA },           // 47 BSMT Samples
	{ "u21.snd",		0x80000, 0x0ad8bc18, 7 | BRF_GRA },           // 48
	{ "u36.snd",		0x80000, 0xf558947b, 7 | BRF_GRA },           // 49
	{ "u37.snd",		0x80000, 0x7a3190bc, 7 | BRF_GRA },           // 50

	{ "eeprom-tattass.bin",	0x00400, 0x7140f40c, 8 | BRF_PRG | BRF_ESS }, // 51 Default Settings

#if !defined ROM_VERIFY
	{ "bsmt2000.bin",	0x02000, 0xc2a265af, 9 | BRF_PRG | BRF_ESS }, // 52 DSP Code
#endif
};

STD_ROM_PICK(tattassa)
STD_ROM_FN(tattassa)

struct BurnDriver BurnDrvTattassa = {
	"tattassa", "tattass", NULL, NULL, "1994",
	"Tattoo Assassins (Asia prototype, Mar 14 1995)\0", NULL, "Data East Pinball", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VSFIGHT, 0,
	NULL, tattassaRomInfo, tattassaRomName, NULL, NULL, NULL, NULL, TattassInputInfo, TattassDIPInfo,
	TattassInit, DrvExit, DrvBSMTFrame, NslasherDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Dragon Gun (US)

static struct BurnRomInfo dragngunRomDesc[] = {
	{ "kb02.a9",		0x040000, 0x4fb9cfea, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "kb06.c9",		0x040000, 0x2395efec, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kb00.a5",		0x040000, 0x1539ff35, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kb04.c5",		0x040000, 0x5b5c1ec2, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "kb03.a10",		0x040000, 0x6c6a4f42, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "kb07.c10",		0x040000, 0x2637e8a1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "kb01.a7",		0x040000, 0xd780ba8d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "kb05.c7",		0x040000, 0xfbad737b, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "kb10.n25",		0x010000, 0xec56f560, 2 | BRF_PRG | BRF_ESS }, //  8 HUC6280 Code

	{ "kb08.a15",		0x010000, 0x8fe4e5f5, 3 | BRF_GRA },           //  9 Tilemap 0&1 Characters (Encrypted)
	{ "kb09.a17",		0x010000, 0xe9dcac3f, 3 | BRF_GRA },           // 10

	{ "mar-00.bin",		0x080000, 0xd0491a37, 4 | BRF_GRA },           // 11 Tilemap 0&1 Tiles (Encrypted)
	{ "mar-01.bin",		0x080000, 0xd5970365, 4 | BRF_GRA },           // 12

	{ "mar-02.bin",		0x100000, 0xc6cd4baf, 5 | BRF_GRA },           // 13 Tilemap 2&3 Tiles (Encrypted)
	{ "mar-03.bin",		0x100000, 0x793006d7, 5 | BRF_GRA },           // 14
	{ "mar-04.bin",		0x100000, 0x56631a2b, 5 | BRF_GRA },           // 15
	{ "mar-05.bin",		0x100000, 0xac16e7ae, 5 | BRF_GRA },           // 16

	{ "mar-09.bin",		0x100000, 0x18fec9e1, 6 | BRF_GRA },           // 17 Sprites
	{ "mar-10.bin",		0x100000, 0x73126fbc, 6 | BRF_GRA },           // 18
	{ "mar-11.bin",		0x100000, 0x1fc638a4, 6 | BRF_GRA },           // 19
	{ "mar-12.bin",		0x100000, 0x4c412512, 6 | BRF_GRA },           // 20
	{ "mar-13.bin",		0x100000, 0xd675821c, 6 | BRF_GRA },           // 21
	{ "mar-14.bin",		0x100000, 0x22d38c71, 6 | BRF_GRA },           // 22
	{ "mar-15.bin",		0x100000, 0xec976b20, 6 | BRF_GRA },           // 23
	{ "mar-16.bin",		0x100000, 0x8b329bc8, 6 | BRF_GRA },           // 24

	{ "mar-17.bin",		0x100000, 0x7799ed23, 7 | BRF_GRA },           // 25 DVI Video (not used)
	{ "mar-20.bin",		0x100000, 0xfa0462f0, 7 | BRF_GRA },           // 26
	{ "mar-28.bin",		0x100000, 0x5a2ec71d, 7 | BRF_GRA },           // 27
	{ "mar-25.bin",		0x100000, 0xd65d895c, 7 | BRF_GRA },           // 28
	{ "mar-18.bin",		0x100000, 0xded66da9, 7 | BRF_GRA },           // 29
	{ "mar-21.bin",		0x100000, 0x2d0a28ae, 7 | BRF_GRA },           // 30
	{ "mar-27.bin",		0x100000, 0x3fcbd10f, 7 | BRF_GRA },           // 31
	{ "mar-24.bin",		0x100000, 0x5cec45c8, 7 | BRF_GRA },           // 32
	{ "mar-19.bin",		0x100000, 0xbdd1ed20, 7 | BRF_GRA },           // 33
	{ "mar-22.bin",		0x100000, 0xc85f3559, 7 | BRF_GRA },           // 34
	{ "mar-26.bin",		0x100000, 0x246a06c5, 7 | BRF_GRA },           // 35
	{ "mar-23.bin",		0x100000, 0xba907d6a, 7 | BRF_GRA },           // 36

	{ "mar-06.n17",		0x080000, 0x3e006c6e, 8 | BRF_SND },           // 37 MSM6295 #0 Samples

	{ "mar-08.n21",		0x080000, 0xb9281dfd, 9 | BRF_SND },           // 38 MSM6295 #1 Samples

	{ "mar-07.n19",		0x080000, 0x40287d62, 10 | BRF_SND },          // 39 MSM6295 #2 Samples
};

STD_ROM_PICK(dragngun)
STD_ROM_FN(dragngun)

static INT32 DragngunInit()
{
	pStartDraw = DragngunStartDraw;
	pDrawScanline = DragngunDrawScanline;

	return DragngunCommonInit(0, 0x628c);
}

struct BurnDriver BurnDrvDragngun = {
	"dragngun", NULL, NULL, NULL, "1993",
	"Dragon Gun (US)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SHOOT, 0,
	NULL, dragngunRomInfo, dragngunRomName, NULL, NULL, NULL, NULL, DragngunInputInfo, DragngunDIPInfo,
	DragngunInit, DrvExit, DrvFrame, DragngunDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Dragon Gun (Japan)

static struct BurnRomInfo dragngunjRomDesc[] = {
	{ "ka-02.a9",		0x040000, 0x402a03f9, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "ka-06.c9",		0x040000, 0x26822853, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ka-00.a5",		0x040000, 0xcc9e321b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ka-04.c5",		0x040000, 0x5fd9d935, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ka-03.a10",		0x040000, 0xe213c859, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ka-07.c10",		0x040000, 0xf34c54eb, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ka-01.a7",		0x040000, 0x1b52364c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ka-05.c7",		0x040000, 0x4c975f52, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ka-10.n25",		0x010000, 0xec56f560, 2 | BRF_PRG | BRF_ESS }, //  8 HUC6280 Code

	{ "ka-08.a15",		0x010000, 0x8fe4e5f5, 3 | BRF_GRA },           //  9 Tilemap 0&1 Characters (Encrypted)
	{ "ka-09.a17",		0x010000, 0xe9dcac3f, 3 | BRF_GRA },           // 10

	{ "mar-00.bin",		0x080000, 0xd0491a37, 4 | BRF_GRA },           // 11 Tilemap 0&1 Tiles (Encrypted)
	{ "mar-01.bin",		0x080000, 0xd5970365, 4 | BRF_GRA },           // 12

	{ "mar-02.bin",		0x100000, 0xc6cd4baf, 5 | BRF_GRA },           // 13 Tilemap 2&3 Tiles (Encrypted)
	{ "mar-03.bin",		0x100000, 0x793006d7, 5 | BRF_GRA },           // 14
	{ "mar-04.bin",		0x100000, 0x56631a2b, 5 | BRF_GRA },           // 15
	{ "mar-05.bin",		0x100000, 0xac16e7ae, 5 | BRF_GRA },           // 16

	{ "mar-09.bin",		0x100000, 0x18fec9e1, 6 | BRF_GRA },           // 17 Sprites
	{ "mar-10.bin",		0x100000, 0x73126fbc, 6 | BRF_GRA },           // 18
	{ "mar-11.bin",		0x100000, 0x1fc638a4, 6 | BRF_GRA },           // 19
	{ "mar-12.bin",		0x100000, 0x4c412512, 6 | BRF_GRA },           // 20
	{ "mar-13.bin",		0x100000, 0xd675821c, 6 | BRF_GRA },           // 21
	{ "mar-14.bin",		0x100000, 0x22d38c71, 6 | BRF_GRA },           // 22
	{ "mar-15.bin",		0x100000, 0xec976b20, 6 | BRF_GRA },           // 23
	{ "mar-16.bin",		0x100000, 0x8b329bc8, 6 | BRF_GRA },           // 24

	{ "mar-17.bin",		0x100000, 0x7799ed23, 7 | BRF_GRA },           // 25 DVI Video (not used)
	{ "mar-20.bin",		0x100000, 0xfa0462f0, 7 | BRF_GRA },           // 26
	{ "mar-28.bin",		0x100000, 0x5a2ec71d, 7 | BRF_GRA },           // 27
	{ "mar-25.bin",		0x100000, 0xd65d895c, 7 | BRF_GRA },           // 28
	{ "mar-18.bin",		0x100000, 0xded66da9, 7 | BRF_GRA },           // 29
	{ "mar-21.bin",		0x100000, 0x2d0a28ae, 7 | BRF_GRA },           // 30
	{ "mar-27.bin",		0x100000, 0x3fcbd10f, 7 | BRF_GRA },           // 31
	{ "mar-24.bin",		0x100000, 0x5cec45c8, 7 | BRF_GRA },           // 32
	{ "mar-19.bin",		0x100000, 0xbdd1ed20, 7 | BRF_GRA },           // 33
	{ "mar-22.bin",		0x100000, 0xc85f3559, 7 | BRF_GRA },           // 34
	{ "mar-26.bin",		0x100000, 0x246a06c5, 7 | BRF_GRA },           // 35
	{ "mar-23.bin",		0x100000, 0xba907d6a, 7 | BRF_GRA },           // 36

	{ "mar-06.n17",		0x080000, 0x3e006c6e, 8 | BRF_SND },           // 37 MSM6295 #0 Samples

	{ "mar-08.n21",		0x080000, 0xb9281dfd, 9 | BRF_SND },           // 38 MSM6295 #1 Samples

	{ "mar-07.n19",		0x080000, 0x40287d62, 10 | BRF_SND },          // 39 MSM6295 #2 Samples
};

STD_ROM_PICK(dragngunj)
STD_ROM_FN(dragngunj)

struct BurnDriver BurnDrvDragngunj = {
	"dragngunj", "dragngun", NULL, NULL, "1993",
	"Dragon Gun (Japan)\0", NULL, "Data East Corporation", "DECO 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SHOOT, 0,
	NULL, dragngunjRomInfo, dragngunjRomName, NULL, NULL, NULL, NULL, DragngunInputInfo, DragngunDIPInfo,
	DragngunInit, DrvExit, DrvFrame, DragngunDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};
