// FB Alpha Alpha68k II & V driver module
// Based on MAME driver by Pierpaolo Prazzoli, Bryan McPhail, Stephane Humbert

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "burn_ym2413.h"
#include "dac.h"
#include "burn_pal.h"

// Todo:
//   fix goldmedl color issue(s)

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvShareRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bank_base;
static UINT8 buffer_28;
static UINT8 buffer_60;
static UINT8 buffer_68;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 sound_nmi_enable;
static UINT8 sound_nmi_previous;
static UINT8 bankdata;

#define ALPHA68K_BTLFIELDB	0xff

// these are configured (static)
static INT32 invert_controls = 0;
static UINT16 game_id; // 5 denotes alpha68k sys. V
static UINT16 coin_id;
static UINT16 microcontroller_id;

// these are variable
static UINT16 credits;
static UINT16 coinvalue;
static UINT16 deposits2;
static UINT16 deposits1;
static UINT16 coin_latch;
static UINT16 microcontroller_data;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[2];
static UINT8 DrvSrv[1];
static UINT8 DrvInputs[6];
static UINT8 DrvReset;

// Rotation stuff! -dink
static UINT8  DrvFakeInput[6]       = {0, 0, 0, 0, 0, 0};
static UINT8  nRotateHoldInput[2]   = {0, 0};
static INT32  nRotate[2]            = {0, 0};
static INT32  nRotateTarget[2]      = {0, 0};
static INT32  nRotateTry[2]         = {0, 0};
static UINT32 nRotateTime[2]        = {0, 0};
static UINT8  game_rotates = 0;

static struct BurnInputInfo TimesoldInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3 (rotate)" , BIT_DIGITAL  , DrvFakeInput + 4,  "p1 fire 3" },

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3 (rotate)" , BIT_DIGITAL  , DrvFakeInput + 5,  "p2 fire 3" },

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,    DrvSrv + 0,     "diag"      },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Timesold)

static struct BurnInputInfo BtlfieldInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3 (rotate)" , BIT_DIGITAL  , DrvFakeInput + 4,  "p1 fire 3" },

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3 (rotate)" , BIT_DIGITAL  , DrvFakeInput + 5,  "p2 fire 3" },

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,    DrvSrv + 0,     "diag"      },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Btlfield)

static struct BurnInputInfo BtlfieldbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,    DrvSrv + 0,     "diag"      },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Btlfieldb)

static struct BurnInputInfo GoldmedlInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 3"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p3 start"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p3 fire 3"	},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p4 start"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p4 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,    DrvSrv + 0,     "diag"      },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Goldmedl)

static struct BurnInputInfo SkysoldrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,    DrvSrv + 0,     "diag"      },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Skysoldr)

static struct BurnInputInfo GangwarsInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,    DrvSrv + 0,     "diag"      },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Gangwars)

static struct BurnInputInfo SbasebalInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,    DrvSrv + 0,     "diag"      },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sbasebal)

static struct BurnDIPInfo TimesoldDIPList[]=
{
	{0x15, 0xff, 0xff, 0xdc, NULL				},
	{0x16, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x15, 0x01, 0x04, 0x04, "Off"				},
	{0x15, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    3, "Difficulty"			},
	{0x15, 0x01, 0x18, 0x00, "Easy"				},
	{0x15, 0x01, 0x18, 0x18, "Normal"			},
	{0x15, 0x01, 0x18, 0x10, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x15, 0x01, 0x20, 0x00, "English"			},
	{0x15, 0x01, 0x20, 0x20, "Japanese"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x15, 0x01, 0x80, 0x80, "Off"				},
	{0x15, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x08, 0x08, "Off"				},
	{0x16, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x16, 0x01, 0x07, 0x07, "A 1C/1C B 1C/1C"		},
	{0x16, 0x01, 0x07, 0x06, "A 1C/2C B 2C/1C"		},
	{0x16, 0x01, 0x07, 0x05, "A 1C/3C B 3C/1C"		},
	{0x16, 0x01, 0x07, 0x04, "A 1C/4C B 4C/1C"		},
	{0x16, 0x01, 0x07, 0x03, "A 1C/5C B 5C/1C"		},
	{0x16, 0x01, 0x07, 0x02, "A 1C/6C B 6C/1C"		},
	{0x16, 0x01, 0x07, 0x01, "A 2C/3C B 7C/1C"		},
	{0x16, 0x01, 0x07, 0x00, "A 3C/2C B 8C/1C"		},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x16, 0x01, 0x30, 0x30, "3"				},
	{0x16, 0x01, 0x30, 0x20, "4"				},
	{0x16, 0x01, 0x30, 0x10, "5"				},
	{0x16, 0x01, 0x30, 0x00, "6"				},
};

STDDIPINFO(Timesold)

static struct BurnDIPInfo BtlfieldDIPList[]=
{
	{0x15, 0xff, 0xff, 0xfd, NULL				},
	{0x16, 0xff, 0xff, 0xf7, NULL				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x15, 0x01, 0x04, 0x04, "Off"				},
	{0x15, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    3, "Difficulty"			},
	{0x15, 0x01, 0x18, 0x00, "Easy"				},
	{0x15, 0x01, 0x18, 0x18, "Normal"			},
	{0x15, 0x01, 0x18, 0x10, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x15, 0x01, 0x20, 0x00, "English"			},
	{0x15, 0x01, 0x20, 0x20, "Japanese"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x15, 0x01, 0x80, 0x80, "Off"				},
	{0x15, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x16, 0x01, 0x07, 0x07, "A 1C/1C B 1C/1C"		},
	{0x16, 0x01, 0x07, 0x06, "A 1C/2C B 2C/1C"		},
	{0x16, 0x01, 0x07, 0x05, "A 1C/3C B 3C/1C"		},
	{0x16, 0x01, 0x07, 0x04, "A 1C/4C B 4C/1C"		},
	{0x16, 0x01, 0x07, 0x03, "A 1C/5C B 5C/1C"		},
	{0x16, 0x01, 0x07, 0x02, "A 1C/6C B 6C/1C"		},
	{0x16, 0x01, 0x07, 0x01, "A 2C/3C B 7C/1C"		},
	{0x16, 0x01, 0x07, 0x00, "A 3C/2C B 8C/1C"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x08, 0x08, "Off"				},
	{0x16, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x16, 0x01, 0x30, 0x30, "3"				},
	{0x16, 0x01, 0x30, 0x20, "4"				},
	{0x16, 0x01, 0x30, 0x10, "5"				},
	{0x16, 0x01, 0x30, 0x00, "6"				},
};

STDDIPINFO(Btlfield)

static struct BurnDIPInfo BtlfieldbDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfd, NULL				},
	{0x14, 0xff, 0xff, 0xf8, NULL				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x04, 0x04, "Off"				},
	{0x13, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    3, "Difficulty"			},
	{0x13, 0x01, 0x18, 0x00, "Easy"				},
	{0x13, 0x01, 0x18, 0x18, "Normal"			},
	{0x13, 0x01, 0x18, 0x10, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x13, 0x01, 0x20, 0x00, "English"			},
	{0x13, 0x01, 0x20, 0x20, "Japanese"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x14, 0x01, 0x07, 0x00, "A 1C/1C B 1C/1C"		},
	{0x14, 0x01, 0x07, 0x01, "A 1C/2C B 2C/1C"		},
	{0x14, 0x01, 0x07, 0x02, "A 1C/3C B 3C/1C"		},
	{0x14, 0x01, 0x07, 0x03, "A 1C/4C B 4C/1C"		},
	{0x14, 0x01, 0x07, 0x04, "A 1C/5C B 5C/1C"		},
	{0x14, 0x01, 0x07, 0x05, "A 1C/6C B 6C/1C"		},
	{0x14, 0x01, 0x07, 0x06, "A 2C/3C B 7C/1C"		},
	{0x14, 0x01, 0x07, 0x07, "A 3C/2C B 8C/1C"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x08, 0x00, "Off"				},
	{0x14, 0x01, 0x08, 0x08, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x14, 0x01, 0x30, 0x30, "3"				},
	{0x14, 0x01, 0x30, 0x20, "4"				},
	{0x14, 0x01, 0x30, 0x10, "5"				},
	{0x14, 0x01, 0x30, 0x00, "6"				},
};

STDDIPINFO(Btlfieldb)

static struct BurnDIPInfo GoldmedlDIPList[]=
{
	{0x15, 0xff, 0xff, 0xfd, NULL				},
	{0x16, 0xff, 0xff, 0xdf, NULL				},

	{0   , 0xfe, 0   ,    2, "Event Select"			},
	{0x15, 0x01, 0x04, 0x04, "Off"				},
	{0x15, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    3, "Cabinet"			},
	{0x15, 0x01, 0x88, 0x00, "Upright 2 Players"		},
	{0x15, 0x01, 0x88, 0x80, "Upright 4 Players"		},
	{0x15, 0x01, 0x88, 0x88, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Speed For 100M Dash"		},
	{0x15, 0x01, 0x20, 0x00, "10 Beats For Max Speed"	},
	{0x15, 0x01, 0x20, 0x20, "14 Beats For Max Speed"	},

	{0   , 0xfe, 0   ,    2, "Computer Demonstration"	},
	{0x15, 0x01, 0x40, 0x00, "Off"				},
	{0x15, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x16, 0x01, 0x03, 0x03, "Easy"				},
	{0x16, 0x01, 0x03, 0x02, "Normal"			},
	{0x16, 0x01, 0x03, 0x01, "Hard"				},
	{0x16, 0x01, 0x03, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x16, 0x01, 0x1c, 0x1c, "A 1C/1C B 1C/1C"		},
	{0x16, 0x01, 0x1c, 0x18, "A 1C/2C B 2C/1C"		},
	{0x16, 0x01, 0x1c, 0x14, "A 1C/3C B 3C/1C"		},
	{0x16, 0x01, 0x1c, 0x10, "A 1C/4C B 4C/1C"		},
	{0x16, 0x01, 0x1c, 0x0c, "A 1C/5C B 5C/1C"		},
	{0x16, 0x01, 0x1c, 0x08, "A 1C/6C B 6C/1C"		},
	{0x16, 0x01, 0x1c, 0x04, "A 2C/3C B 7C/1C"		},
	{0x16, 0x01, 0x1c, 0x00, "A 3C/2C B 8C/1C"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x20, 0x20, "Off"				},
	{0x16, 0x01, 0x20, 0x00, "On"				},
};

STDDIPINFO(Goldmedl)

static struct BurnDIPInfo SkysoldrDIPList[]=
{
	{0x13, 0xff, 0xff, 0xd4, NULL				},
	{0x14, 0xff, 0xff, 0xf7, NULL				},
	
	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x04, 0x04, "Off"				},
	{0x13, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x18, 0x08, "Easy"				},
	{0x13, 0x01, 0x18, 0x10, "Normal"			},
	{0x13, 0x01, 0x18, 0x18, "Hard"				},
	{0x13, 0x01, 0x18, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x13, 0x01, 0x20, 0x00, "English"			},
	{0x13, 0x01, 0x20, 0x20, "Japanese"			},

	{0   , 0xfe, 0   ,    2, "Manufacturer"			},
	{0x13, 0x01, 0x40, 0x40, "SNK"				},
	{0x13, 0x01, 0x40, 0x00, "Romstar"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x14, 0x01, 0x07, 0x07, "A 1C/1C B 1C/1C"		},
	{0x14, 0x01, 0x07, 0x06, "A 1C/2C B 2C/1C"		},
	{0x14, 0x01, 0x07, 0x05, "A 1C/3C B 3C/1C"		},
	{0x14, 0x01, 0x07, 0x04, "A 1C/4C B 4C/1C"		},
	{0x14, 0x01, 0x07, 0x03, "A 1C/5C B 5C/1C"		},
	{0x14, 0x01, 0x07, 0x02, "A 1C/6C B 6C/1C"		},
	{0x14, 0x01, 0x07, 0x01, "A 2C/3C B 7C/1C"		},
	{0x14, 0x01, 0x07, 0x00, "A 3C/2C B 8C/1C"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x08, 0x08, "Off"				},
	{0x14, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x14, 0x01, 0x30, 0x30, "3"				},
	{0x14, 0x01, 0x30, 0x20, "4"				},
	{0x14, 0x01, 0x30, 0x10, "5"				},
	{0x14, 0x01, 0x30, 0x00, "6"				},
};

STDDIPINFO(Skysoldr)


static struct BurnDIPInfo SkyadvntDIPList[]=
{
	{0x15, 0xff, 0xff, 0xfc, NULL				},
	{0x16, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x15, 0x01, 0x0c, 0x08, "2"				},
	{0x15, 0x01, 0x0c, 0x0c, "3"				},
	{0x15, 0x01, 0x0c, 0x04, "4"				},
	{0x15, 0x01, 0x0c, 0x00, "5"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x60, 0x40, "Easy"				},
	{0x15, 0x01, 0x60, 0x60, "Normal"			},
	{0x15, 0x01, 0x60, 0x20, "Hard"				},
	{0x15, 0x01, 0x60, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x80, 0x00, "Off"				},
	{0x15, 0x01, 0x80, 0x80, "On"				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x16, 0x01, 0x0e, 0x0e, "A 1C/1C B 1C/1C"		},
	{0x16, 0x01, 0x0e, 0x06, "A 1C/2C B 2C/1C"		},
	{0x16, 0x01, 0x0e, 0x0a, "A 1C/3C B 3C/1C"		},
	{0x16, 0x01, 0x0e, 0x02, "A 1C/4C B 4C/1C"		},
	{0x16, 0x01, 0x0e, 0x0c, "A 1C/5C B 5C/1C"		},
	{0x16, 0x01, 0x0e, 0x04, "A 1C/6C B 6C/1C"		},
	{0x16, 0x01, 0x0e, 0x08, "A 2C/3C B 7C/1C"		},
	{0x16, 0x01, 0x0e, 0x00, "A 3C/2C B 8C/1C"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x16, 0x01, 0x10, 0x10, "Off"				},
	{0x16, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x16, 0x01, 0x20, 0x20, "Off"				},
	{0x16, 0x01, 0x20, 0x00, "On"				},
};

STDDIPINFO(Skyadvnt)

static struct BurnDIPInfo SkyadvntuDIPList[]=
{
	{0x15, 0xff, 0xff, 0xfd, NULL				},
	{0x16, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x15, 0x01, 0x0c, 0x08, "2"				},
	{0x15, 0x01, 0x0c, 0x0c, "3"				},
	{0x15, 0x01, 0x0c, 0x04, "4"				},
	{0x15, 0x01, 0x0c, 0x00, "5"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x60, 0x40, "Easy"				},
	{0x15, 0x01, 0x60, 0x60, "Normal"			},
	{0x15, 0x01, 0x60, 0x20, "Hard"				},
	{0x15, 0x01, 0x60, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x80, 0x00, "Off"				},
	{0x15, 0x01, 0x80, 0x80, "On"				},

	{0   , 0xfe, 0   ,    2, "Price to Continue"		},
	{0x16, 0x01, 0x01, 0x01, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0x01, 0x00, "Same as Start"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x16, 0x01, 0x02, 0x00, "No"				},
	{0x16, 0x01, 0x02, 0x02, "Yes"				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x16, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"		},
	{0x16, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},
	{0x16, 0x01, 0x0c, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x16, 0x01, 0x10, 0x10, "Off"				},
	{0x16, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x16, 0x01, 0x20, 0x20, "Off"				},
	{0x16, 0x01, 0x20, 0x00, "On"				},
};

STDDIPINFO(Skyadvntu)

static struct BurnDIPInfo GangwarsuDIPList[]=
{
	{0x15, 0xff, 0xff, 0xed, NULL				},
	{0x16, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x15, 0x01, 0x0c, 0x08, "2"				},
	{0x15, 0x01, 0x0c, 0x0c, "3"				},
	{0x15, 0x01, 0x0c, 0x04, "4"				},
	{0x15, 0x01, 0x0c, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Timer Speed"			},
	{0x15, 0x01, 0x10, 0x00, "Slow"				},
	{0x15, 0x01, 0x10, 0x10, "Normal"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x60, 0x40, "Easy"				},
	{0x15, 0x01, 0x60, 0x60, "Normal"			},
	{0x15, 0x01, 0x60, 0x20, "Hard"				},
	{0x15, 0x01, 0x60, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x80, 0x00, "Off"				},
	{0x15, 0x01, 0x80, 0x80, "On"				},

	{0   , 0xfe, 0   ,    2, "Price to Continue"		},
	{0x16, 0x01, 0x01, 0x01, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0x01, 0x00, "Same as Start"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x16, 0x01, 0x02, 0x00, "No"				},
	{0x16, 0x01, 0x02, 0x02, "Yes"				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x16, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"		},
	{0x16, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},
	{0x16, 0x01, 0x0c, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x16, 0x01, 0x10, 0x10, "Off"				},
	{0x16, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x16, 0x01, 0x20, 0x20, "Off"				},
	{0x16, 0x01, 0x20, 0x00, "On"				},
};

STDDIPINFO(Gangwarsu)

static struct BurnDIPInfo GangwarsDIPList[]=
{
	{0x15, 0xff, 0xff, 0xed, NULL				},
	{0x16, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x15, 0x01, 0x08, 0x08, "3"				},
	{0x15, 0x01, 0x08, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Timer Speed"			},
	{0x15, 0x01, 0x10, 0x00, "Slow"				},
	{0x15, 0x01, 0x10, 0x10, "Normal"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x60, 0x40, "Easy"				},
	{0x15, 0x01, 0x60, 0x60, "Normal"			},
	{0x15, 0x01, 0x60, 0x20, "Hard"				},
	{0x15, 0x01, 0x60, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x80, 0x00, "Off"				},
	{0x15, 0x01, 0x80, 0x80, "On"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x16, 0x01, 0x01, 0x00, "1"				},
	{0x16, 0x01, 0x01, 0x01, "2"				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x16, 0x01, 0x0e, 0x00, "5 Coins 1 Credits"		},
	{0x16, 0x01, 0x0e, 0x02, "3 Coins 1 Credits"		},
	{0x16, 0x01, 0x0e, 0x04, "2 Coins 1 Credits"		},
	{0x16, 0x01, 0x0e, 0x0e, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0x0e, 0x0c, "1 Coin  2 Credits"		},
	{0x16, 0x01, 0x0e, 0x0a, "1 Coin  3 Credits"		},
	{0x16, 0x01, 0x0e, 0x08, "1 Coin  4 Credits"		},
	{0x16, 0x01, 0x0e, 0x06, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x16, 0x01, 0x10, 0x10, "Off"				},
	{0x16, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x16, 0x01, 0x20, 0x20, "Off"				},
	{0x16, 0x01, 0x20, 0x00, "On"				},
};

STDDIPINFO(Gangwars)

static struct BurnDIPInfo SbasebalDIPList[]=
{
	{0x14, 0xff, 0xff, 0xfd, NULL				},
	{0x15, 0xff, 0xff, 0xcf, NULL				},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x14, 0x01, 0x04, 0x04, "Off"				},
	{0x14, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x08, 0x08, "Off"				},
	{0x14, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Game Time"			},
	{0x14, 0x01, 0xc0, 0x00, "3:30"				},
	{0x14, 0x01, 0xc0, 0x80, "3:00"				},
	{0x14, 0x01, 0xc0, 0x40, "2:30"				},
	{0x14, 0x01, 0xc0, 0xc0, "2:00"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x03, 0x02, "Easy"				},
	{0x15, 0x01, 0x03, 0x03, "Normal"			},
	{0x15, 0x01, 0x03, 0x01, "Hard"				},
	{0x15, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x15, 0x01, 0x0c, 0x08, "2 Coins 1 Credit"		},
	{0x15, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"		},
	{0x15, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x0c, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Price to Continue"		},
	{0x15, 0x01, 0x10, 0x10, "1 Coin  1 Credit"		},
	{0x15, 0x01, 0x10, 0x00, "Same as Start"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x20, 0x20, "Off"				},
	{0x15, 0x01, 0x20, 0x00, "On"				},
};

STDDIPINFO(Sbasebal)

// Rotation-handler code

static void RotateReset() {
	for (INT32 playernum = 0; playernum < 2; playernum++) {
		nRotate[playernum] = 0; // start out pointing straight up (0=up)
		nRotateTarget[playernum] = -1;
		nRotateTime[playernum] = 0;
		nRotateHoldInput[0] = nRotateHoldInput[1] = 0;
	}
}

static void RotateStateload() {
	for (INT32 playernum = 0; playernum < 2; playernum++) {
		nRotateTarget[playernum] = -1;
	}
}

static UINT32 RotationTimer(void) {
    return nCurrentFrame;
}

static void RotateRight(INT32 *v) {
    (*v)--;
    if (*v < 0) *v = 11;
}

static void RotateLeft(INT32 *v) {
    (*v)++;
    if (*v > 11) *v = 0;
}

static UINT8 Joy2Rotate(UINT8 *joy) { // ugly code, but the effect is awesome. -dink
	if (joy[0] && joy[2]) return 11;   // up left
	if (joy[0] && joy[3]) return 1;    // up right

	if (joy[1] && joy[2]) return 7;    // down left
	if (joy[1] && joy[3]) return 4;    // down right

	if (joy[0]) return 0;    // up
	if (joy[1]) return 6;    // down
	if (joy[2]) return 9;    // left
	if (joy[3]) return 3;    // right

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
        //bprintf(PRINT_NORMAL, _T("Player %d Rotate Left => %06X\n"), playernum+1, nRotate[playernum]);
		nRotateTime[playernum] = RotationTimer();
		nRotateTarget[playernum] = -1;
    }

	if (player[1] && (player[1] != lastplayer[playernum][1] || (RotationTimer() > nRotateTime[playernum]+0xf))) {
        RotateRight(&nRotate[playernum]);
        //bprintf(PRINT_NORMAL, _T("Player %d Rotate Right => %06X\n"), playernum+1, nRotate[playernum]);
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
// game     p1           p2           clockwise value in memory  multiplier
// timesold 0x40017      0x40016      0 1 2 3 4 5 6 7 8 9 a b

static void RotateSetGunPosRAM(UINT8 *p1, UINT8 *p2, UINT8 multiplier) {
	rotate_gunpos[0] = p1;
	rotate_gunpos[1] = p2;
	rotate_gunpos_multiplier = multiplier;
}

static INT32 get_distance(INT32 from, INT32 to) {
// this function finds the easiest way to get from "from" to "to", wrapping at 0 and 7
	INT32 countA = 0;
	INT32 countB = 0;
	INT32 fromtmp = from / rotate_gunpos_multiplier;
	INT32 totmp = to / rotate_gunpos_multiplier;

	while (1) {
		fromtmp++;
		countA++;
		if (fromtmp > 0xb) fromtmp = 0;
		if (fromtmp == totmp || countA > 32) break;
	}

	fromtmp = from / rotate_gunpos_multiplier;
	totmp = to / rotate_gunpos_multiplier;

	while (1) {
		fromtmp--;
		countB++;
		if (fromtmp < 0) fromtmp = 0xb;
		if (fromtmp == totmp || countB > 32) break;
	}

	if (countA > countB) {
		return 1; // go negative
	} else {
		return 0; // go positive
	}
}

static void RotateDoTick() {
	// since the game only allows for 1 rotation every other frame, we have to
	// do this.
	if (nCurrentFrame&1) return;

	for (INT32 i = 0; i < 2; i++) {
		if (rotate_gunpos[i] && (nRotateTarget[i] != -1) && (nRotateTarget[i] != (*rotate_gunpos[i] & 0xff))) {
			if (get_distance(nRotateTarget[i], *rotate_gunpos[i] & 0xff)) {
				RotateRight(&nRotate[i]); // --
			} else {
				RotateLeft(&nRotate[i]);  // ++
			}
			bprintf(0, _T("p%X target %X mempos %X nRotate %X.\n"), i, nRotateTarget[i], *rotate_gunpos[i] & 0xff, nRotate[i]);
			nRotateTry[i]++;
			if (nRotateTry[i] > 10) nRotateTarget[i] = -1; // don't get stuck in a loop if something goes horribly wrong here.
		} else {
			nRotateTarget[i] = -1;
		}
	}
}

static void SuperJoy2Rotate() {
	for (INT32 i = 0; i < 2; i++) { // p1 = 0, p2 = 1
		if (DrvFakeInput[4 + i]) { //  rotate-button had been pressed
			UINT8 rot = Joy2Rotate(((!i) ? &DrvJoy1[0] : &DrvJoy2[0]));
			if (rot != 0xff) {
				nRotateTarget[i] = rot * rotate_gunpos_multiplier;
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

static void alpha68k_II_video_bank_write(UINT8 offset)
{
	switch (offset)
	{
		case 0x10: // Reset
			bank_base = buffer_28 = buffer_60 = buffer_68 = 0;
		return;

		case 0x14:
			if (buffer_60) bank_base=1; else bank_base=0;
			buffer_28 = 1;
		return;

		case 0x18:
			if (buffer_68) { if (buffer_60) bank_base = 3; else bank_base = 2; }
			if (buffer_28) { if (buffer_60) bank_base = 1; else bank_base = 0; }
		return;

		case 0x30:
			buffer_28 = buffer_68 = 0; bank_base = 1;
			buffer_60 = 1;
		return;

		case 0x34:
			if (buffer_60) bank_base = 3; else bank_base = 2;
			buffer_68 = 1;
		return;

		case 0x38:
			if (buffer_68) { if (buffer_60) bank_base = 7; else bank_base = 6; }
			if (buffer_28) { if (buffer_60) bank_base = 5; else bank_base = 4; }
		return;
	}
}

static UINT16 alpha_II_trigger_r(INT32 offset)
{
	offset = (offset / 2) & 0xff;
	UINT16 *m_shared_ram = (UINT16*)DrvShareRAM;
	static const UINT8 coinage1[8][2] = {{1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, {2,3}, {3,2}};
	static const UINT8 coinage2[8][2] = {{1,1}, {2,1}, {3,1}, {4,1}, {5,1}, {6,1}, {7,1}, {8,1}};
	UINT16 source = m_shared_ram[offset];

	switch (offset)
	{
		case 0: /* Dipswitch 2 */
			m_shared_ram[0] = (source & 0xff00) | DrvDips[1];
			return 0;

		case 0x22: /* Coin value */
			m_shared_ram[0x22] = (source & 0xff00) | (credits & 0x00ff);
			return 0;

		case 0x29: /* Query microcontroller for coin insert */
			if ((DrvInputs[2] & 0x3) == 3)
				coin_latch = 0;
			if ((DrvInputs[2] & 0x1) == 0 && !coin_latch)
			{
				m_shared_ram[0x29] = (source & 0xff00) | (coin_id & 0xff);    // coinA
				m_shared_ram[0x22] = (source & 0xff00) | 0x0;
				coin_latch = 1;

				if ((coin_id & 0xff) == 0x22)
				{
					if (game_id == ALPHA68K_BTLFIELDB)
						coinvalue = (DrvDips[1] >> 0) & 7;
					else
						coinvalue = (~DrvDips[1] >> 0) & 7;

					deposits1++;
					if (deposits1 == coinage1[coinvalue][0])
					{
						credits = coinage1[coinvalue][1];
						deposits1 = 0;
					}
					else
						credits = 0;
				}
			}
			else if ((DrvInputs[2] & 0x2) == 0 && !coin_latch)
			{
				m_shared_ram[0x29] = (source & 0xff00) | (coin_id >> 8);  // coinB
				m_shared_ram[0x22] = (source & 0xff00) | 0x0;
				coin_latch = 1;

				if ((coin_id >> 8) == 0x22)
				{
					if (game_id == ALPHA68K_BTLFIELDB)
						coinvalue = (DrvDips[1] >> 0) & 7;
					else
						coinvalue = (~DrvDips[1] >> 0) & 7;

					deposits2++;
					if (deposits2 == coinage2[coinvalue][0])
					{
						credits = coinage2[coinvalue][1];
						deposits2 = 0;
					}
					else
						credits = 0;
				}
			}
			else
			{
				if (microcontroller_id == 0x8803)     /* Gold Medalist */
					microcontroller_data = 0x21;              // timer
				else
					microcontroller_data = 0x00;
				m_shared_ram[0x29] = (source & 0xff00) | microcontroller_data;
			}
			return 0;

		case 0xfe:  /* Custom ID check, same for all games */
			m_shared_ram[0xfe] = (source & 0xff00) | 0x87;
			break;

		case 0xff:  /* Custom ID check, same for all games */
			m_shared_ram[0xff] = (source & 0xff00) | 0x13;
			break;
	}

	return 0; /* Values returned don't matter */
}

static INT32 alpha_V_trigger_r(UINT16 offset)
{
	offset = (offset / 2) & 0x1fff;
	UINT16 *m_shared_ram = (UINT16*)DrvShareRAM;

	static const UINT8 coinage1[8][2] = {{1,1}, {1,5}, {1,3}, {2,3}, {1,2}, {1,6}, {1,4}, {3,2}};
	static const UINT8 coinage2[8][2] = {{1,1}, {5,1}, {3,1}, {7,1}, {2,1}, {6,1}, {4,1}, {8,1}};
	int source = m_shared_ram[offset];

	switch (offset)
	{
		case 0: /* Dipswitch 1 */
			m_shared_ram[0] = (source & 0xff00) | DrvDips[1];
			return 0;
		case 0x22: /* Coin value */
			m_shared_ram[0x22] = (source & 0xff00) | (credits & 0x00ff);
			return 0;
		case 0x29: /* Query microcontroller for coin insert */
			if ((DrvInputs[2] & 0x3) == 3)
				coin_latch = 0;
			if ((DrvInputs[2] & 0x1) == 0 && !coin_latch)
			{
				m_shared_ram[0x29] = (source & 0xff00) | (coin_id & 0xff);    // coinA
				m_shared_ram[0x22] = (source & 0xff00) | 0x0;
				coin_latch = 1;

				if ((coin_id & 0xff) == 0x22)
				{
					coinvalue = (~DrvDips[1] >> 1) & 7;
					deposits1++;
					if (deposits1 == coinage1[coinvalue][0])
					{
						credits = coinage1[coinvalue][1];
						deposits1 = 0;
					}
					else
						credits = 0;
				}
			}
			else if ((DrvInputs[2] & 0x2) == 0 && !coin_latch)
			{
				m_shared_ram[0x29] = (source & 0xff00) | (coin_id>>8);    // coinB
				m_shared_ram[0x22] = (source & 0xff00) | 0x0;
				coin_latch = 1;

				if ((coin_id >> 8) == 0x22)
				{
					coinvalue = (~DrvDips[1] >> 1) & 7;
					deposits2++;
					if (deposits2 == coinage2[coinvalue][0])
					{
						credits = coinage2[coinvalue][1];
						deposits2 = 0;
					}
					else
						credits = 0;
				}
			}
			else
			{
				microcontroller_data = 0x00;
				m_shared_ram[0x29] = (source & 0xff00) | microcontroller_data;
			}

			return 0;
		case 0xfe:  /* Custom ID check */
			m_shared_ram[0xfe] = (source & 0xff00) | (microcontroller_id >> 8);
			break;
		case 0xff:  /* Custom ID check */
			m_shared_ram[0xff] = (source & 0xff00) | (microcontroller_id & 0xff);
			break;

		case 0x1f00: /* Dipswitch 1 */
			m_shared_ram[0x1f00] = (source & 0xff00) | DrvDips[1];
			return 0;
		case 0x1f29: /* Query microcontroller for coin insert */
			if ((DrvInputs[2] & 0x3) == 3)
				coin_latch = 0;
			if ((DrvInputs[2] & 0x1) == 0 && !coin_latch)
			{
				m_shared_ram[0x1f29] = (source & 0xff00) | (coin_id & 0xff);  // coinA
				m_shared_ram[0x1f22] = (source & 0xff00) | 0x0;
				coin_latch = 1;

				if ((coin_id & 0xff) == 0x22)
				{
					coinvalue = (~DrvDips[1] >> 1) & 7;
					deposits1++;
					if (deposits1 == coinage1[coinvalue][0])
					{
						credits = coinage1[coinvalue][1];
						deposits1 = 0;
					}
					else
						credits = 0;
				}
			}
			else if ((DrvInputs[2] & 0x2) == 0 && !coin_latch)
			{
				m_shared_ram[0x1f29] = (source & 0xff00) | (coin_id >> 8);    // coinB
				m_shared_ram[0x1f22] = (source & 0xff00) | 0x0;
				coin_latch = 1;

				if ((coin_id >> 8) == 0x22)
				{
					coinvalue = (~DrvDips[1] >> 1) & 7;
					deposits2++;
					if (deposits2 == coinage2[coinvalue][0])
					{
						credits = coinage2[coinvalue][1];
						deposits2 = 0;
					}
					else
						credits = 0;
				}
			}
			else
			{
				microcontroller_data = 0x00;
				m_shared_ram[0x1f29] = (source & 0xff00) | microcontroller_data;
			}

			source = m_shared_ram[0x0163];
			m_shared_ram[0x0163] = (source & 0x00ff) | (DrvDips[1] << 8);

			return 0;
		case 0x1ffe:  /* Custom ID check */
			m_shared_ram[0x1ffe] = (source & 0xff00) | (microcontroller_id >> 8);
			break;
		case 0x1fff:  /* Custom ID check */
			m_shared_ram[0x1fff] = (source & 0xff00) | (microcontroller_id & 0xff);
			break;
	}

	return 0;
}

static void __fastcall alpha68k_ii_write_word(UINT32 address, UINT16 data)
{
//	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);

	// video ram is written byte-wise
	if ((address & 0xfff000) == 0x100000) {
		DrvVidRAM[(address / 2) & 0x7ff] = data;
		return;
	}

	if ((address & 0xfffe00) == 0x300000) {
		// microcontroller_w
		if (address == 0x30005a) flipscreen = data & 1;
		return;
	}

	if ((address & 0xffff00) == 0x0c0000) {
		alpha68k_II_video_bank_write(address / 2);
		return;
	}

	switch (address)
	{
		case 0x080000:
			soundlatch = data;
		return;
	}
}

static void __fastcall alpha68k_ii_write_byte(UINT32 address, UINT8 data)
{
	// video ram is written byte-wise
	if ((address & 0xfff000) == 0x100000) {
		DrvVidRAM[(address / 2) & 0x7ff] = data;
		return;
	}

	if ((address & 0xfffe00) == 0x300000) {
		// microcontroller_w
		if (address == 0x30005a) flipscreen = data & 1;
		return;
	}

	if ((address & 0xffff00) == 0x0c0000) {
		alpha68k_II_video_bank_write(address / 2);
		return;
	}

	switch (address)
	{
		case 0x080001:
			soundlatch = data;
		return;
	}
}

static UINT16 __fastcall alpha68k_ii_read_word(UINT32 address)
{
	if ((address & 0xfff000) == 0x100000) {
		return DrvVidRAM[(address / 2) & 0x7ff];
	}

	if ((address & 0xfffe00) == 0x300000) {
		alpha_II_trigger_r(address);
		return 0;
	}

	switch (address)
	{
		case 0x080000:
			return (DrvInputs[0] + (DrvInputs[1] * 256));

		case 0x0c0000: {
			UINT16 ret = ((~(1 << dialRotation(0))) << 8);
			if (invert_controls) ret = ~ret & 0xff00;
			return (DrvInputs[3] + ret);
		}

		case 0x0c8000: {
			UINT16 ret = (((~(1 << dialRotation(1))) << 8) & 0xff00);
			if (invert_controls) ret = ~ret;
			return ret;
		}

		case 0x0d0000: {
			UINT16 ret = ((((~(1 << dialRotation(1))) << 4) & 0xf000)
						+ (((~(1 << dialRotation(0)))) & 0x0f00));
			if (invert_controls) ret = ~ret;
			return ret;
		}

		case 0x0d8000:
		case 0x0e0000:
		case 0x0e8000:
			return 0;
	}

	return 0;
}

static UINT8 __fastcall alpha68k_ii_read_byte(UINT32 address)
{
	if ((address & 0xfff000) == 0x100000) {
		return DrvVidRAM[(address / 2) & 0x7ff];
	}

	if ((address & 0xfffe00) == 0x300000) {
		alpha_II_trigger_r(address);
		return 0;
	}

	switch (address)
	{
		case 0x080000:
		case 0x080001:
		case 0x0c0000:
		case 0x0c0001:
		case 0x0c8000:
		case 0x0c8001:
		case 0x0d0000:
		case 0x0d0001: {
			UINT16 ret = alpha68k_ii_read_word(address & ~1);

			return (address & 1) ? (ret & 0xff) : ((ret >> 8) & 0xff);
		}

		case 0x0d8000:
		case 0x0d8001:
		case 0x0e0000:
		case 0x0e0001:
		case 0x0e8001:
		case 0x0e8000:
			return 0;
	}

	return 0;
}


static void __fastcall alpha68k_v_write_word(UINT32 address, UINT16 data)
{
	// video ram is written byte-wise
	if ((address & 0xfff000) == 0x100000) {
		DrvVidRAM[(address / 2) & 0x7ff] = data;
		return;
	}

	if ((address & 0xffc000) == 0x300000) {
		address &= 0x1ff;
		// microcontroller_w
		if (address == 0x00005a) flipscreen = data & 1;
		return;
	}

	if ((address & 0xffff00) == 0x0c0000) {
		// not used
		return;
	}

	switch (address)
	{
		case 0x080000:
			bank_base = data >> 8;
			soundlatch = data & 0xff;
		return;
	}
}

static void __fastcall alpha68k_v_write_byte(UINT32 address, UINT8 data)
{
	// video ram is written byte-wise
	if ((address & 0xfff000) == 0x100000) {
		DrvVidRAM[(address / 2) & 0x7ff] = data;
		return;
	}

	if ((address & 0xffc000) == 0x300000) {
		address &= 0x1ff;
		// microcontroller_w
		if (address == 0x00005a) flipscreen = data & 1;
		return;
	}

	if ((address & 0xffff00) == 0x0c0000) {
		// not used
		return;
	}

	switch (address)
	{
		case 0x080000:
			bank_base = data;
		return;

		case 0x080001:
			soundlatch = data;
		return;

	}
}

static UINT16 __fastcall alpha68k_v_read_word(UINT32 address)
{
	if ((address & 0xfff000) == 0x100000) {
		return DrvVidRAM[(address / 2) & 0x7ff];
	}

	if ((address & 0xffc000) == 0x300000) {
		alpha_V_trigger_r(address);
		return 0;
	}

	switch (address)
	{
		case 0x080000:
			return (DrvInputs[0] + (DrvInputs[1] * 256));

		case 0x0c0000:
			return DrvInputs[3];

		case 0x0d8000:
		case 0x0e0000:
		case 0x0e8000:
			return 0;
	}

	return 0;
}

static UINT8 __fastcall alpha68k_v_read_byte(UINT32 address)
{
	if ((address & 0xfff000) == 0x100000) {
		return DrvVidRAM[(address / 2) & 0x7ff];
	}

	if ((address & 0xffc000) == 0x300000) {
		alpha_V_trigger_r(address);
		return 0;
	}

	switch (address)
	{
		case 0x080000:
			return DrvInputs[1];

		case 0x080001:
			return DrvInputs[0];

		case 0x0c0000:
			return 0;

		case 0x0c0001:
			return DrvInputs[3];

		case 0x0d8000:
		case 0x0d8001:
		case 0x0e0000:
		case 0x0e0001:
		case 0x0e8001:
		case 0x0e8000:
			return 0;
	}

	return 0;
}

static void bankswitch(INT32 data)
{
	if ((data & 0x1f) >= 28) return;

	bankdata = data & 0x1f;

	ZetMapMemory(DrvZ80ROM + (0x10000 + (bankdata * 0x4000)), 0xc000, 0xffff, MAP_ROM);
}

static void __fastcall alpha68k_ii_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			soundlatch = 0;
		return;

		case 0x08:
			DACSignedWrite(0, data);
		return;

		case 0x0a:
		case 0x0b:
			BurnYM2413Write(port & 1, data);
		return;

		case 0x0c:
		case 0x0d:
			BurnYM2203Write(0, port & 1, data);
		return;

		case 0x0e:
			bankswitch(data);
		return;
	}
}

static UINT8 __fastcall alpha68k_ii_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( character )
{
	INT32 tile  = DrvVidRAM[offs * 2 + 0] | (bank_base * 256);
	INT32 color = DrvVidRAM[offs * 2 + 1] & 0x0f;

	TILE_SET_INFO(0, tile, color, 0);
}

static UINT8 DrvYM2203ReadPortA(UINT32)
{
	return soundlatch;
}

static void DrvYM2203WritePortA(UINT32, UINT32 data)
{
	if (data == 0xff) return;

	if (data == 0 && sound_nmi_previous != 0)
		sound_nmi_enable = 1;

	if (data != 0 && sound_nmi_previous == 0)
		sound_nmi_enable = 0;

	sound_nmi_previous = data & 1;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM2413Reset();
	BurnYM2203Reset();
	DACReset();
	ZetClose();

	soundlatch = 0;
	flipscreen = 0;
	sound_nmi_enable = 0;
	sound_nmi_previous = 0;
	bank_base = 0;
	buffer_28 = 0;
	buffer_60 = 0;
	buffer_68 = 0;
	credits = 0;
	coinvalue = 0;
	deposits2 = 0;
	deposits1 = 0;
	coin_latch = 0;
	microcontroller_data = 0;

	RotateReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;
	DrvZ80ROM		= Next; Next += 0x080000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x800000;

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam			= Next;

	DrvShareRAM		= Next; Next += 0x004000;
	DrvPalRAM		= Next; Next += 0x002000;
	DrvVidRAM		= Next; Next += 0x001000; // 800
	DrvSprRAM		= Next; Next += 0x008000;
	DrvZ80RAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 chartype, INT32 len)
{
	INT32 Plane0[4]  = { STEP4(0,4) };
	INT32 XOffs0[8]  = { 8*16+3, 8*16+2, 8*16+1, 8*16+0, 3, 2, 1, 0 };
	INT32 YOffs0[8]  = { STEP8(0,16) };
	INT32 Plane1[4]  = { ((len)/4)*8*0, ((len)/4)*8*1, ((len)/4)*8*2, ((len)/4)*8*3 };
	INT32 XOffs1[16] = { STEP8(128+7,-1), STEP8(7,-1) };
	INT32 YOffs1[16] = { STEP16(0,8) };
	INT32 Plane2[4]  = { STEP4(0,1) };
	INT32 XOffs2[8]  = { 16*8+4, 16*8+0, 24*8+4, 24*8+0, 4, 0, 8*8+4, 8*8+0 };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x10000);

	if (chartype == 0)
	{
		GfxDecode(((0x010000/4)*8)/(8 * 8), 4,  8,  8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);
	}
	else
	{
		GfxDecode(((0x010000/4)*8)/(8 * 8), 4,  8,  8, Plane2, XOffs2, YOffs1, 0x100, tmp, DrvGfxROM0);
	}

	memcpy (tmp, DrvGfxROM1, len);

	GfxDecode((((len)/4)*8)/(16*16), 4, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 Drv2Init(INT32 (*pLoadCb)(), UINT8 invert, UINT16 mc_id, UINT16 coin, UINT8 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (pLoadCb) {
			if (pLoadCb()) return 1;
		}

		DrvGfxDecode(0, 0x200000);
	}

	invert_controls = invert;
	microcontroller_id = mc_id;
	coin_id = coin;
	game_id = game;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM,		0x040000, 0x040fff, MAP_RAM);
//	SekMapMemory(DrvVidRAM,			0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x200000, 0x207fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x400000, 0x400fff, MAP_RAM);
	SekMapMemory(Drv68KROM + 0x40000,	0x800000, 0x83ffff, MAP_ROM); // data rom
	SekSetWriteWordHandler(0,		alpha68k_ii_write_word);
	SekSetWriteByteHandler(0,		alpha68k_ii_write_byte);
	SekSetReadWordHandler(0,		alpha68k_ii_read_word);
	SekSetReadByteHandler(0,		alpha68k_ii_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM,		0xc000, 0xffff, MAP_ROM);
	ZetSetOutHandler(alpha68k_ii_sound_write_port);
	ZetSetInHandler(alpha68k_ii_sound_read_port);
	ZetClose();

	BurnYM2413Init(3579545);
	BurnYM2413SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	BurnYM2203Init(1, 3000000, NULL, 1);
	BurnYM2203SetPorts(0, &DrvYM2203ReadPortA, NULL, &DrvYM2203WritePortA, NULL);
	BurnTimerAttachZet(3579545*2);
	BurnYM2203SetAllRoutes(0, 0.65, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.10);

	DACInit(0, 0, 1, ZetTotalCycles, 3579545*2);
	DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, character_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x20000, 0, 0xf);
	GenericTilemapSetTransparent(0,0);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 Drv5Init(INT32 (*pLoadCb)(), UINT8 invert, UINT16 mc_id, UINT16 coin, UINT8 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (pLoadCb) {
			if (pLoadCb()) return 1;
		}

		DrvGfxDecode(1, 0x400000);
	}

	invert_controls = invert;
	microcontroller_id = mc_id;
	coin_id = coin;
	game_id = game;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM,		0x040000, 0x043fff, MAP_RAM);
//	SekMapMemory(DrvVidRAM,			0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x200000, 0x207fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x400000, 0x401fff, MAP_RAM);
	SekMapMemory(Drv68KROM + 0x40000,	0x800000, 0x83ffff, MAP_ROM); // data rom
	SekSetWriteWordHandler(0,		alpha68k_v_write_word);
	SekSetWriteByteHandler(0,		alpha68k_v_write_byte);
	SekSetReadWordHandler(0,		alpha68k_v_read_word);
	SekSetReadByteHandler(0,		alpha68k_v_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM,		0xc000, 0xffff, MAP_ROM);
	ZetSetOutHandler(alpha68k_ii_sound_write_port);
	ZetSetInHandler(alpha68k_ii_sound_read_port);
	ZetClose();

	BurnYM2413Init(3579545);
	BurnYM2413SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	BurnYM2203Init(1, 3000000, NULL, 1);
	BurnYM2203SetPorts(0, &DrvYM2203ReadPortA, NULL, &DrvYM2203WritePortA, NULL);
	BurnTimerAttachZet(3579545*2);
	BurnYM2203SetAllRoutes(0, 0.65, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.10);

	DACInit(0, 0, 1, ZetTotalCycles, 3579545*2);
	DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, character_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x20000, 0, 0xf);
	GenericTilemapSetTransparent(0,0);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset();

	return 0;
}


static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM2203Exit();
	BurnYM2413Exit();
	SekExit();
	ZetExit();
	DACExit();

	BurnFree (AllMem);

	game_rotates = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		UINT16 p = pal[i];
		UINT8 r = pal5bit(((p >> 7) & 0x1e) | ((p >> 14) & 0x01));
		UINT8 g = pal5bit(((p >> 3) & 0x1e) | ((p >> 13) & 0x01));
		UINT8 b = pal5bit(((p << 1) & 0x1e) | ((p >> 12) & 0x01));

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites(INT32 j, INT32 s, INT32 e, INT32 fx_mask, INT32 fy_mask, INT32 sprite_mask, INT32 color_mask)
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	for (INT32 offs = s; offs < e; offs += 0x40)
	{
		INT32 my = spriteram[offs + 3 + (j << 1)];
		INT32 mx =(spriteram[offs + 2 + (j << 1)] << 1) | (my >> 15);
		my = -my & 0x1ff;
		mx = ((mx + 0x100) & 0x1ff) - 0x100;
		if (j == 0 && s == 0x7c0)
			my++;

		if (flipscreen)
		{
			mx = 240 - mx;
			my = 240 - my;
		}

		for (INT32 i = 0; i < 0x40; i += 2)
		{
			INT32 tile = spriteram[offs + 1 + i + (0x800 * j) + 0x800];
			INT32 color = spriteram[offs + i + (0x800 * j) + 0x800] & color_mask;

			INT32 fx = tile & fx_mask;
			INT32 fy = tile & fy_mask;
			tile &= sprite_mask;

			if (tile > 0x4fff) continue;

			if (flipscreen)
			{
				if (fx) fx = 0; else fx = 1;
				if (fy) fy = 0; else fy = 1;
			}

			if (color)
			{
				Draw16x16MaskTile(pTransDraw, tile, mx, my - 16, fx, fy, color, 4, 0, 0, DrvGfxROM1);
			}

			if (flipscreen)
				my = (my - 16) & 0x1ff;
			else
				my = (my + 16) & 0x1ff;
		}
	}
}

static INT32 Alpha68KIIDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear(BurnDrvGetPaletteEntries() - 1);

	GenericTilemapSetFlip(0, flipscreen ? TMAP_FLIPXY : 0);

	if (nBurnLayer & 1)
	{
		draw_sprites(0, 0x07c0, 0x0800, 0x4000, 0x8000, 0x3fff, 0x7f);
		draw_sprites(1, 0x0000, 0x0800, 0x4000, 0x8000, 0x3fff, 0x7f);
		draw_sprites(2, 0x0000, 0x0800, 0x4000, 0x8000, 0x3fff, 0x7f);
		draw_sprites(0, 0x0000, 0x07c0, 0x4000, 0x8000, 0x3fff, 0x7f);
	}

	if (nBurnLayer & 2)
	{
		GenericTilemapDraw(0, pTransDraw, 0);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 GangwarsDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear(BurnDrvGetPaletteEntries() - 1);

	GenericTilemapSetFlip(0, flipscreen ? TMAP_FLIPXY : 0);

	if (nBurnLayer & 2)
	{
		draw_sprites(0, 0x07c0, 0x0800, 0x8000, 0, 0x7fff, 0xff);
		draw_sprites(1, 0x0000, 0x0800, 0x8000, 0, 0x7fff, 0xff);
		draw_sprites(2, 0x0000, 0x0800, 0x8000, 0, 0x7fff, 0xff);
		draw_sprites(0, 0x0000, 0x07c0, 0x8000, 0, 0x7fff, 0xff);
	}

	if (nBurnLayer & 2)
	{
		GenericTilemapDraw(0, pTransDraw, 0);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 SkyadvntDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear(BurnDrvGetPaletteEntries() - 1);

	GenericTilemapSetFlip(0, flipscreen ? TMAP_FLIPXY : 0);

	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	if (nBurnLayer & 1)
	{
		draw_sprites(0, 0x07c0, 0x0800, 0, 0x8000, 0x7fff, 0xff);
		draw_sprites(1, 0x0000, 0x0800, 0, 0x8000, 0x7fff, 0xff);

		if (spriteram[0x1bde] == 0x24 && (spriteram[0x1bdf] >> 8) == 0x3b)
		{
			draw_sprites(2, 0x03c0, 0x0800, 0, 0x8000, 0x7fff, 0xff);
			draw_sprites(2, 0x0000, 0x03c0, 0, 0x8000, 0x7fff, 0xff);
		}
		else
		{
			draw_sprites(2, 0x0000, 0x0800, 0, 0x8000, 0x7fff, 0xff);
		}

		draw_sprites(0, 0x0000, 0x07c0, 0, 0x8000, 0x7fff, 0xff);
	}

	if (nBurnLayer & 2)
	{
		GenericTilemapDraw(0, pTransDraw, 0);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 SbasebalDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear(BurnDrvGetPaletteEntries() - 1);

	GenericTilemapSetFlip(0, flipscreen ? TMAP_FLIPXY : 0);

	if (nBurnLayer & 1)
	{
		draw_sprites(0, 0x07c0, 0x0800, 0x4000, 0x8000, 0x3fff, 0xff);
		draw_sprites(1, 0x0000, 0x0800, 0x4000, 0x8000, 0x3fff, 0xff);
		draw_sprites(2, 0x0000, 0x0800, 0x4000, 0x8000, 0x3fff, 0xff);
		draw_sprites(0, 0x0000, 0x07c0, 0x4000, 0x8000, 0x3fff, 0xff);
	}

	if (nBurnLayer & 2)
	{
		GenericTilemapDraw(0, pTransDraw, 0);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		// 0 p1 inputs
		// 1 p2 inputs
		// 2 coins -> mcu
		// 3 dips 0
		// 4 dips 1

		DrvInputs[0] = (invert_controls) ? 0 : 0xff;
		DrvInputs[1] = (invert_controls) ? 0 : 0xff;
		DrvInputs[2] = 0x03; // read by mcu-sim, no inversion
		DrvInputs[3] =((DrvSrv[0] ? 0x00 : 0x02) | 0x01 | (DrvDips[0] & 0xfc)) ^ ((invert_controls) ? 0xff : 0x00);
		DrvInputs[4] = DrvDips[1]; // read by mcu-sim, no inversion

		for (INT32 i = 0; i < 8; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (game_rotates) {
			SuperJoy2Rotate();
		}
	}

	INT32 nInterleave = (game_id == 5) ? 141 : 125; // sound nmi
	INT32 nCyclesTotal[2] = { ((game_id == 5) ? 10000000 : 8000000) / 60, (3579545 * 2) / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (microcontroller_id == 0x8803) { // goldmedl

			if (i == (nInterleave - 1)) {
				SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
			}

			if (i == 41 || i == 83 || i == 123) {
				SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
			}
		} else {
			if (i == nInterleave - 1) {
				SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
			}
		}	

		BurnTimerUpdate((i + 1) * nCyclesTotal[1] / nInterleave);
		if (sound_nmi_enable) ZetNmi();
	}

	BurnTimerEndFrame(nCyclesTotal[1]);
	
	if (pBurnSoundOut) {
		BurnYM2413Render(pBurnSoundOut, nBurnSoundLen);
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029703;
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
		ZetScan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		BurnYM2413Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(bankdata);
		SCAN_VAR(bank_base);
		SCAN_VAR(buffer_28);
		SCAN_VAR(buffer_60);
		SCAN_VAR(buffer_68);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(sound_nmi_enable);
		SCAN_VAR(sound_nmi_previous);
		SCAN_VAR(bankdata);
		SCAN_VAR(credits);
		SCAN_VAR(coinvalue);
		SCAN_VAR(deposits2);
		SCAN_VAR(deposits1);
		SCAN_VAR(coin_latch);
		SCAN_VAR(microcontroller_data);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();

		RotateStateload();
	}

	return 0;
}



// Time Soldiers (US Rev 3)

static struct BurnRomInfo timesoldRomDesc[] = {
	{ "bf.3",		0x10000, 0xa491e533, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "bf.4",		0x10000, 0x34ebaccc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bf.1",		0x10000, 0x158f4cb3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bf.2",		0x10000, 0xaf01a718, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bf.7",		0x10000, 0xf8b293b5, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "bf.8",		0x10000, 0x8a43497b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bf.9",		0x10000, 0x1408416f, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "alpha.mcu",		0x01000, 0x00000000, 3 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  7 MCU Code

	{ "bf.6",		0x08000, 0x086a364d, 4 | BRF_GRA },           //  8 Characters
	{ "bf.5",		0x08000, 0x3cec2f55, 4 | BRF_GRA },           //  9

	{ "bf.10",		0x20000, 0x613313ba, 5 | BRF_GRA },           // 10 Sprites
	{ "bf.14",		0x20000, 0xefda5c45, 5 | BRF_GRA },           // 11
	{ "bf.18",		0x20000, 0xe886146a, 5 | BRF_GRA },           // 12
	{ "bf.11",		0x20000, 0x92b42eba, 5 | BRF_GRA },           // 13
	{ "bf.15",		0x20000, 0xba3b9f5a, 5 | BRF_GRA },           // 14
	{ "bf.19",		0x20000, 0x8994bf10, 5 | BRF_GRA },           // 15
	{ "bf.12",		0x20000, 0x7ca8bb32, 5 | BRF_GRA },           // 16
	{ "bf.16",		0x20000, 0x2aa74125, 5 | BRF_GRA },           // 17
	{ "bf.20",		0x20000, 0xbab6a7c5, 5 | BRF_GRA },           // 18
	{ "bf.13",		0x20000, 0x56a3a26a, 5 | BRF_GRA },           // 19
	{ "bf.17",		0x20000, 0x6b37d048, 5 | BRF_GRA },           // 20
	{ "bf.21",		0x20000, 0xbc3b3944, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(timesold)
STD_ROM_FN(timesold)

static INT32 TimesoldRomCb()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x020001,  2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x020000,  3, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x08000);
	if (BurnLoadRom(DrvZ80ROM  + 0x030000,  5, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x050000,  6, 1)) return 1;

//	if (BurnLoadRom(DrvMCUROM  + 0x000000,  7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000001,  8, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  9, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x020000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x040000, 12, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000, 13, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0a0000, 14, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0c0000, 15, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000, 16, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x120000, 17, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x140000, 18, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x180000, 19, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1a0000, 20, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1c0000, 21, 1)) return 1;

	return 0;
}

static INT32 TimesoldInit()
{
	INT32 rc = Drv2Init(TimesoldRomCb, 0, 0, 0x2222, 0);

	if (!rc) {
		game_rotates = 1;
		RotateSetGunPosRAM(DrvShareRAM + (0x17), DrvShareRAM + (0x16), 1);
	}

	return rc;
}

struct BurnDriver BurnDrvTimesold = {
	"timesold", NULL, NULL, NULL, "1987",
	"Time Soldiers (US Rev 3)\0", NULL, "Alpha Denshi Co. (SNK/Romstar license)", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, timesoldRomInfo, timesoldRomName, NULL, NULL, NULL, NULL, TimesoldInputInfo, TimesoldDIPInfo,
	TimesoldInit, DrvExit, DrvFrame, Alpha68KIIDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};


// Time Soldiers (US Rev 1)

static struct BurnRomInfo timesold1RomDesc[] = {
	{ "3",			0x10000, 0xbc069a29, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "4",			0x10000, 0xac7dca56, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bf.1",		0x10000, 0x158f4cb3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bf.2",		0x10000, 0xaf01a718, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bf.7",		0x10000, 0xf8b293b5, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "bf.8",		0x10000, 0x8a43497b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bf.9",		0x10000, 0x1408416f, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "alpha.mcu",		0x01000, 0x00000000, 3 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  7 MCU Code

	{ "bf.6",		0x08000, 0x086a364d, 4 | BRF_GRA },           //  8 Characters
	{ "bf.5",		0x08000, 0x3cec2f55, 4 | BRF_GRA },           //  9

	{ "bf.10",		0x20000, 0x613313ba, 5 | BRF_GRA },           // 10 Sprites
	{ "bf.14",		0x20000, 0xefda5c45, 5 | BRF_GRA },           // 11
	{ "bf.18",		0x20000, 0xe886146a, 5 | BRF_GRA },           // 12
	{ "bf.11",		0x20000, 0x92b42eba, 5 | BRF_GRA },           // 13
	{ "bf.15",		0x20000, 0xba3b9f5a, 5 | BRF_GRA },           // 14
	{ "bf.19",		0x20000, 0x8994bf10, 5 | BRF_GRA },           // 15
	{ "bf.12",		0x20000, 0x7ca8bb32, 5 | BRF_GRA },           // 16
	{ "bf.16",		0x20000, 0x2aa74125, 5 | BRF_GRA },           // 17
	{ "bf.20",		0x20000, 0xbab6a7c5, 5 | BRF_GRA },           // 18
	{ "bf.13",		0x20000, 0x56a3a26a, 5 | BRF_GRA },           // 19
	{ "bf.17",		0x20000, 0x6b37d048, 5 | BRF_GRA },           // 20
	{ "bf.21",		0x20000, 0xbc3b3944, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(timesold1)
STD_ROM_FN(timesold1)

static INT32 Timesold1Init()
{
	INT32 rc = Drv2Init(TimesoldRomCb, 1, 0, 0x2222, 0);

	if (!rc) {
		game_rotates = 1;
		RotateSetGunPosRAM(DrvShareRAM + (0x17), DrvShareRAM + (0x16), 1);
	}

	return rc;
}

struct BurnDriver BurnDrvTimesold1 = {
	"timesold1", "timesold", NULL, NULL, "1987",
	"Time Soldiers (US Rev 1)\0", NULL, "Alpha Denshi Co. (SNK/Romstar license)", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_FLIPPED | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, timesold1RomInfo, timesold1RomName, NULL, NULL, NULL, NULL, TimesoldInputInfo, TimesoldDIPInfo,
	Timesold1Init, DrvExit, DrvFrame, Alpha68KIIDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};


// Battle Field (Japan)

static struct BurnRomInfo btlfieldRomDesc[] = {
	{ "bfv1_03.bin",	0x10000, 0x8720af0d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "bfv1_04.bin",	0x10000, 0x7dcccbe6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bf.1",		0x10000, 0x158f4cb3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bf.2",		0x10000, 0xaf01a718, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bf.7",		0x10000, 0xf8b293b5, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "bf.8",		0x10000, 0x8a43497b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bf.9",		0x10000, 0x1408416f, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "bfv1_06.bin",	0x08000, 0x022b9de9, 3 | BRF_GRA },           //  7 Characters
	{ "bfv1_05.bin",	0x08000, 0xbe269dbf, 3 | BRF_GRA },           //  8

	{ "bf.10",		0x20000, 0x613313ba, 4 | BRF_GRA },           //  9 Sprites
	{ "bf.14",		0x20000, 0xefda5c45, 4 | BRF_GRA },           // 10
	{ "bf.18",		0x20000, 0xe886146a, 4 | BRF_GRA },           // 11
	{ "bf.11",		0x20000, 0x92b42eba, 4 | BRF_GRA },           // 12
	{ "bf.15",		0x20000, 0xba3b9f5a, 4 | BRF_GRA },           // 13
	{ "bf.19",		0x20000, 0x8994bf10, 4 | BRF_GRA },           // 14
	{ "bf.12",		0x20000, 0x7ca8bb32, 4 | BRF_GRA },           // 15
	{ "bf.16",		0x20000, 0x2aa74125, 4 | BRF_GRA },           // 16
	{ "bf.20",		0x20000, 0xbab6a7c5, 4 | BRF_GRA },           // 17
	{ "bf.13",		0x20000, 0x56a3a26a, 4 | BRF_GRA },           // 18
	{ "bf.17",		0x20000, 0x6b37d048, 4 | BRF_GRA },           // 19
	{ "bf.21",		0x20000, 0xbc3b3944, 4 | BRF_GRA },           // 20
};

STD_ROM_PICK(btlfield)
STD_ROM_FN(btlfield)

static INT32 BtlfieldRomCb()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x020001,  2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x020000,  3, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x08000);
	if (BurnLoadRom(DrvZ80ROM  + 0x030000,  5, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x050000,  6, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000001,  7, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  8, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x020000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x040000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000, 12, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0a0000, 13, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0c0000, 14, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000, 15, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x120000, 16, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x140000, 17, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x180000, 18, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1a0000, 19, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1c0000, 20, 1)) return 1;

	return 0;
}

static INT32 BtlfieldInit()
{
	INT32 rc = Drv2Init(BtlfieldRomCb, 1, 0, 0x2222, 0);

	if (!rc) {
		game_rotates = 1;
		RotateSetGunPosRAM(DrvShareRAM + (0x17), DrvShareRAM + (0x16), 1);
	}

	return rc;
}

struct BurnDriver BurnDrvBtlfield = {
	"btlfield", "timesold", NULL, NULL, "1987",
	"Battle Field (Japan)\0", NULL, "Alpha Denshi Co. (SNK license)", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_FLIPPED | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, btlfieldRomInfo, btlfieldRomName, NULL, NULL, NULL, NULL, BtlfieldInputInfo, BtlfieldDIPInfo,
	BtlfieldInit, DrvExit, DrvFrame, Alpha68KIIDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};


// Battle Field (bootleg)

static struct BurnRomInfo btlfieldbRomDesc[] = {
	{ "3.bin",		0x10000, 0x141f10ca, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "1.bin",		0x10000, 0xcaa09adf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bf.1",		0x10000, 0x158f4cb3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bf.2",		0x10000, 0xaf01a718, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bf.7",		0x10000, 0xf8b293b5, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "bf.8",		0x10000, 0x8a43497b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bf.9",		0x10000, 0x1408416f, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "alpha.mcu",		0x01000, 0x00000000, 3 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  7 MCU Code

	{ "bfv1_06.bin",	0x08000, 0x022b9de9, 4 | BRF_GRA },           //  8 Character
	{ "bfv1_05.bin",	0x08000, 0xbe269dbf, 4 | BRF_GRA },           //  9

	{ "12.bin",		0x10000, 0x8cab60f2, 5 | BRF_GRA },           // 10 Sprites
	{ "11.bin",		0x10000, 0xe296581e, 5 | BRF_GRA },           // 11
	{ "8.bin",		0x10000, 0x54462294, 5 | BRF_GRA },           // 12
	{ "7.bin",		0x10000, 0x03b18a1d, 5 | BRF_GRA },           // 13
	{ "5.bin",		0x10000, 0xd6f3d746, 5 | BRF_GRA },           // 14
	{ "6.bin",		0x10000, 0x07714f39, 5 | BRF_GRA },           // 15
	{ "16.bin",		0x10000, 0x6166553a, 5 | BRF_GRA },           // 16
	{ "15.bin",		0x10000, 0x805439d7, 5 | BRF_GRA },           // 17
	{ "13.bin",		0x10000, 0x5622cb93, 5 | BRF_GRA },           // 18
	{ "14.bin",		0x10000, 0x67860390, 5 | BRF_GRA },           // 19
	{ "9.bin",		0x10000, 0x02f73dc5, 5 | BRF_GRA },           // 20
	{ "10.bin",		0x10000, 0xe5de7eb8, 5 | BRF_GRA },           // 21
	{ "21.bin",		0x10000, 0x81b75cdc, 5 | BRF_GRA },           // 22
	{ "22.bin",		0x10000, 0x5231e4df, 5 | BRF_GRA },           // 23
	{ "20.bin",		0x10000, 0x5a944f3e, 5 | BRF_GRA },           // 24
	{ "19.bin",		0x10000, 0xdb1dcd5e, 5 | BRF_GRA },           // 25
	{ "17.bin",		0x10000, 0x69210ee6, 5 | BRF_GRA },           // 26
	{ "18.bin",		0x10000, 0x40bf0b3d, 5 | BRF_GRA },           // 27
	{ "28.bin",		0x10000, 0x3399e86e, 5 | BRF_GRA },           // 28
	{ "27.bin",		0x10000, 0x86529c8a, 5 | BRF_GRA },           // 29
	{ "25.bin",		0x10000, 0x6764d81b, 5 | BRF_GRA },           // 30
	{ "26.bin",		0x10000, 0x335b7b50, 5 | BRF_GRA },           // 31
	{ "24.bin",		0x10000, 0x2e78684a, 5 | BRF_GRA },           // 32
	{ "23.bin",		0x10000, 0x500e0dbc, 5 | BRF_GRA },           // 33
};

STD_ROM_PICK(btlfieldb)
STD_ROM_FN(btlfieldb)

static INT32 BtlfieldbRomCb()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x020001,  2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x020000,  3, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x08000);
	if (BurnLoadRom(DrvZ80ROM  + 0x030000,  5, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x050000,  6, 1)) return 1;

//	if (BurnLoadRom(DrvMCUROM  + 0x000000,  7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000001,  8, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  9, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x010000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x020000, 12, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x030000, 13, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x040000, 14, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x050000, 15, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000, 16, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x090000, 17, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0a0000, 18, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0b0000, 19, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0c0000, 20, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0d0000, 21, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000, 22, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x110000, 23, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x120000, 24, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x130000, 25, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x140000, 26, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x150000, 27, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x180000, 28, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x190000, 29, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1a0000, 30, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1b0000, 31, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1c0000, 32, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1d0000, 33, 1)) return 1;

	return 0;
}

static INT32 BtlfieldbInit()
{
	return Drv2Init(BtlfieldbRomCb, 1, 0, 0x2222, ALPHA68K_BTLFIELDB);
}

struct BurnDriver BurnDrvBtlfieldb = {
	"btlfieldb", "timesold", NULL, NULL, "1987",
	"Battle Field (bootleg)\0", "no-rotation joystick ver", "bootleg", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_FLIPPED | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_RUNGUN, 0,
	NULL, btlfieldbRomInfo, btlfieldbRomName, NULL, NULL, NULL, NULL, BtlfieldbInputInfo, BtlfieldbDIPInfo,
	BtlfieldbInit, DrvExit, DrvFrame, Alpha68KIIDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};


// Sky Soldiers (US)

static struct BurnRomInfo skysoldrRomDesc[] = {
	{ "ss.3",		0x20000, 0x7b88aa2e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ss.4",		0x20000, 0xf0283d43, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ss.1",		0x20000, 0x20e9dbc7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ss.2",		0x20000, 0x486f3432, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ss.7",		0x10000, 0xb711fad4, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "ss.8",		0x10000, 0xe5cf7b37, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "ss.9",		0x10000, 0x76124ca2, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "alpha.mcu",		0x01000, 0x00000000, 3 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  7 MCU Code

	{ "ss.6",		0x08000, 0x93b30b55, 4 | BRF_GRA },           //  8 Characters
	{ "ss.5",		0x08000, 0x928ba287, 4 | BRF_GRA },           //  9

	{ "ss.10",		0x20000, 0xe48c1623, 5 | BRF_GRA },           // 10 Sprites
	{ "ss.14",		0x20000, 0x190c8704, 5 | BRF_GRA },           // 11
	{ "ss.18",		0x20000, 0xcb6ff33a, 5 | BRF_GRA },           // 12
	{ "ss.22",		0x20000, 0xe69b4485, 5 | BRF_GRA },           // 13
	{ "ss.11",		0x20000, 0x6c63e9c5, 5 | BRF_GRA },           // 14
	{ "ss.15",		0x20000, 0x55f71ab1, 5 | BRF_GRA },           // 15
	{ "ss.19",		0x20000, 0x312a21f5, 5 | BRF_GRA },           // 16
	{ "ss.23",		0x20000, 0x923c19c2, 5 | BRF_GRA },           // 17
	{ "ss.12",		0x20000, 0x63bb4e89, 5 | BRF_GRA },           // 18
	{ "ss.16",		0x20000, 0x138179f7, 5 | BRF_GRA },           // 19
	{ "ss.20",		0x20000, 0x268cc7b4, 5 | BRF_GRA },           // 20
	{ "ss.24",		0x20000, 0xf63b8417, 5 | BRF_GRA },           // 21
	{ "ss.13",		0x20000, 0x3506c06b, 5 | BRF_GRA },           // 22
	{ "ss.17",		0x20000, 0xa7f524e0, 5 | BRF_GRA },           // 23
	{ "ss.21",		0x20000, 0xcb7bf5fe, 5 | BRF_GRA },           // 24
	{ "ss.25",		0x20000, 0x65138016, 5 | BRF_GRA },           // 25
};

STD_ROM_PICK(skysoldr)
STD_ROM_FN(skysoldr)

static INT32 SkysoldrRomCb()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x040001,  2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x040000,  3, 2)) return 1;
	memcpy (DrvZ80ROM, Drv68KROM + 0x20000, 0x20000);
	memcpy (Drv68KROM + 0x20000, Drv68KROM + 0x40000, 0x20000);
	memcpy (Drv68KROM + 0x40000, DrvZ80ROM, 0x20000);
	memset (DrvZ80ROM, 0, 0x20000);

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x08000);
	if (BurnLoadRom(DrvZ80ROM  + 0x030000,  5, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x050000,  6, 1)) return 1;

//	if (BurnLoadRom(DrvMCUROM  + 0x000000,  7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000001,  8, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  9, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x020000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x040000, 12, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x060000, 13, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000, 14, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0a0000, 15, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0c0000, 16, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0e0000, 17, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000, 18, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x120000, 19, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x140000, 20, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x160000, 21, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x180000, 22, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1a0000, 23, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1c0000, 24, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1e0000, 25, 1)) return 1;

	return 0;
}

static INT32 SkysoldrInit()
{
	return Drv2Init(SkysoldrRomCb, 0, 0, 0x2222, 0);
}

struct BurnDriver BurnDrvSkysoldr = {
	"skysoldr", NULL, NULL, NULL, "1988",
	"Sky Soldiers (US)\0", NULL, "Alpha Denshi Co. (SNK of America/Romstar license)", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, skysoldrRomInfo, skysoldrRomName, NULL, NULL, NULL, NULL, SkysoldrInputInfo, SkysoldrDIPInfo,
	SkysoldrInit, DrvExit, DrvFrame, Alpha68KIIDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};


// Sky Soldiers (bootleg)

static struct BurnRomInfo skysoldrblRomDesc[] = {
	{ "g.bin",		0x10000, 0x4d3273e9, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "c.bin",		0x10000, 0x86c7af62, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "e.bin",		0x10000, 0x03115b75, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a.bin",		0x10000, 0x7aa103c7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "h-gtop.bin",		0x10000, 0xf41dfeab, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "d-ctop.bin",		0x10000, 0x56560a3c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "f-etop.bin",		0x10000, 0x60a52583, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "b-atop.bin",		0x10000, 0x028fd31b, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "33.ic11",		0x10000, 0xb711fad4, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 Code
	{ "34.ic12",		0x10000, 0xe5cf7b37, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "35.ic13",		0x10000, 0x76124ca2, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "68705r3p.mcu",	0x01000, 0x00000000, 3 | BRF_NODUMP | BRF_OPT }, // 11 MCU Code

	{ "xx.ic1",		0x08000, 0x93b30b55, 4 | BRF_GRA },           // 12 Characters
	{ "xx.ic2",		0x08000, 0x928ba287, 4 | BRF_GRA },           // 13

	{ "26.ica9",		0x10000, 0x2aad8c4d, 5 | BRF_GRA },           // 14 Sprites
	{ "25.ica8",		0x10000, 0x7bca633e, 5 | BRF_GRA },           // 15
	{ "28.ic41",		0x10000, 0xda94809d, 5 | BRF_GRA },           // 16
	{ "27.ica10",		0x10000, 0xdd1e56c0, 5 | BRF_GRA },           // 17
	{ "30.ic43",		0x10000, 0x9eb10d3d, 5 | BRF_GRA },           // 18
	{ "31.ic44",		0x10000, 0x6b6c4e56, 5 | BRF_GRA },           // 19
	{ "32.ic45",		0x10000, 0xfdf55eca, 5 | BRF_GRA },           // 20
	{ "29.ic42",		0x10000, 0xcf888369, 5 | BRF_GRA },           // 21
	{ "18.ica1",		0x10000, 0x08419273, 5 | BRF_GRA },           // 22
	{ "17.ic30",		0x10000, 0x6258a61b, 5 | BRF_GRA },           // 23
	{ "20.ica3",		0x10000, 0x5e716c62, 5 | BRF_GRA },           // 24
	{ "19.ica2",		0x10000, 0xf3922f1e, 5 | BRF_GRA },           // 25
	{ "22.ica5",		0x10000, 0x1a7c2f20, 5 | BRF_GRA },           // 26
	{ "23.ica6",		0x10000, 0x3155aca2, 5 | BRF_GRA },           // 27
	{ "24.ica7",		0x10000, 0x8fc95590, 5 | BRF_GRA },           // 28
	{ "21.ica4",		0x10000, 0xf7ea25b0, 5 | BRF_GRA },           // 29
	{ "10.ic23",		0x10000, 0x8e67a39e, 5 | BRF_GRA },           // 30
	{ "9.ic22",		0x10000, 0x6f6d2593, 5 | BRF_GRA },           // 31
	{ "12.ic25",		0x10000, 0x549182ba, 5 | BRF_GRA },           // 32
	{ "11.ic24",		0x10000, 0xb5b06e28, 5 | BRF_GRA },           // 33
	{ "14.ic27",		0x10000, 0x1498a515, 5 | BRF_GRA },           // 34
	{ "15.ic28",		0x10000, 0x34545c01, 5 | BRF_GRA },           // 35
	{ "16.ic29",		0x10000, 0xea5c20a1, 5 | BRF_GRA },           // 36
	{ "13.ic26",		0x10000, 0x962a3e28, 5 | BRF_GRA },           // 37
	{ "2.ic15",		0x10000, 0x022bcdc1, 5 | BRF_GRA },           // 38
	{ "1.ic14",		0x10000, 0x129a58b5, 5 | BRF_GRA },           // 39
	{ "4.ic17",		0x10000, 0xccaf1968, 5 | BRF_GRA },           // 40
	{ "3.ic16",		0x10000, 0x796999ba, 5 | BRF_GRA },           // 41
	{ "6.ic19",		0x10000, 0x45b1ab8a, 5 | BRF_GRA },           // 42
	{ "7.ic20",		0x10000, 0x052247d1, 5 | BRF_GRA },           // 43
	{ "8.ic21",		0x10000, 0xf670ce4b, 5 | BRF_GRA },           // 44
	{ "5.ic18",		0x10000, 0xfe34cd89, 5 | BRF_GRA },           // 45
};

STD_ROM_PICK(skysoldrbl)
STD_ROM_FN(skysoldrbl)

static INT32 SkysoldrblRomCb()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x020001,  2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x020000,  3, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x040001,  4, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x040000,  5, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x060001,  6, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x060000,  7, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  8, 1)) return 1;
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x08000);
	if (BurnLoadRom(DrvZ80ROM  + 0x030000,  9, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x050000, 10, 1)) return 1;

//	if (BurnLoadRom(DrvMCUROM  + 0x000000, 11, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000001, 12, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000000, 13, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000, 14, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x010000, 15, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x020000, 16, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x030000, 17, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x040000, 18, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x050000, 19, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x060000, 20, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x070000, 21, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000, 22, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x090000, 23, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0a0000, 24, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0b0000, 25, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0c0000, 26, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0d0000, 27, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0e0000, 28, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x0f0000, 29, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000, 30, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x110000, 31, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x120000, 32, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x130000, 33, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x140000, 34, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x150000, 35, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x160000, 36, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x170000, 37, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x180000, 38, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x190000, 39, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1a0000, 40, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1b0000, 41, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1c0000, 42, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1d0000, 43, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1e0000, 44, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x1f0000, 45, 1)) return 1;

	return 0;
}

static INT32 SkysoldrblInit()
{
	return Drv2Init(SkysoldrblRomCb, 0, 0, 0x2222, 0);
}

struct BurnDriver BurnDrvSkysoldrbl = {
	"skysoldrbl", "skysoldr", NULL, NULL, "1988",
	"Sky Soldiers (bootleg)\0", NULL, "bootleg", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_FLIPPED | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, skysoldrblRomInfo, skysoldrblRomName, NULL, NULL, NULL, NULL, SkysoldrInputInfo, SkysoldrDIPInfo,
	SkysoldrblInit, DrvExit, DrvFrame, Alpha68KIIDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};


// Gold Medalist (set 1)

static struct BurnRomInfo goldmedlRomDesc[] = {
	{ "gm.3",		0x10000, 0xddf0113c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gm.4",		0x10000, 0x16db4326, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gm.1",		0x10000, 0x54a11e28, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gm.2",		0x10000, 0x4b6a13e4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "38.bin",		0x10000, 0x4bf251b8, 2 | BRF_PRG | BRF_ESS }, //  4 Z080 Code
	{ "39.bin",		0x10000, 0x1d92be86, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "40.bin",		0x10000, 0x8dafc4e8, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "1.bin",		0x10000, 0x1e78062c, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "alpha.mcu",		0x01000, 0x00000000, 3 | BRF_NODUMP | BRF_OPT }, //  8 MCU Code

	{ "gm.6",		0x08000, 0x56020b13, 4 | BRF_GRA },           //  9 Characters
	{ "gm.5",		0x08000, 0x667f33f1, 4 | BRF_GRA },           // 10

	{ "goldchr3.c46",	0x80000, 0x6faaa07a, 5 | BRF_GRA },           // 11 Sprites
	{ "goldchr2.c45",	0x80000, 0xe6b0aa2c, 5 | BRF_GRA },           // 12
	{ "goldchr1.c44",	0x80000, 0x55db41cd, 5 | BRF_GRA },           // 13
	{ "goldchr0.c43",	0x80000, 0x76572c3f, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(goldmedl)
STD_ROM_FN(goldmedl)

static INT32 GoldmedlRomCb()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x020001,  2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x020000,  3, 2)) return 1;

	// goldmedla
	memcpy (Drv68KROM + 0x40000, Drv68KROM + 0x20000, 0x20000);
	memcpy (Drv68KROM + 0x60000, Drv68KROM + 0x20000, 0x20000);

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x08000);
	if (BurnLoadRom(DrvZ80ROM  + 0x020000,  5, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x030000,  6, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x040000,  7, 1)) return 1;

//	if (BurnLoadRom(DrvMCUROM  + 0x000000,  8, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000001,  9, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000000, 10, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000, 12, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000, 13, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x180000, 14, 1)) return 1;

	return 0;
}

static INT32 GoldmedlInit()
{
	return Drv2Init(GoldmedlRomCb, 0, 0x8803, 0x2423, 0);
}

struct BurnDriver BurnDrvGoldmedl = {
	"goldmedl", NULL, NULL, NULL, "1988",
	"Gold Medalist (set 1)\0", NULL, "SNK", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, goldmedlRomInfo, goldmedlRomName, NULL, NULL, NULL, NULL, GoldmedlInputInfo, GoldmedlDIPInfo,
	GoldmedlInit, DrvExit, DrvFrame, Alpha68KIIDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Gold Medalist (set 2)

static struct BurnRomInfo goldmedlaRomDesc[] = {
	{ "gm3-7.bin",		0x10000, 0x11a63f4c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gm4-7.bin",		0x10000, 0xe19966af, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gm1-7.bin",		0x10000, 0x6d87b8a6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gm2-7.bin",		0x10000, 0x8d579505, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "38.bin",		0x10000, 0x4bf251b8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "39.bin",		0x10000, 0x1d92be86, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "40.bin",		0x10000, 0x8dafc4e8, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "1.bin",		0x10000, 0x1e78062c, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "alpha.mcu",		0x01000, 0x00000000, 3 | BRF_NODUMP | BRF_OPT }, //  8 MCU Code

	{ "gm.6",		0x08000, 0x56020b13, 4 | BRF_GRA },           //  9 Characters
	{ "gm.5",		0x08000, 0x667f33f1, 4 | BRF_GRA },           // 10

	{ "goldchr3.c46",	0x80000, 0x6faaa07a, 5 | BRF_GRA },           // 11 Sprites
	{ "goldchr2.c45",	0x80000, 0xe6b0aa2c, 5 | BRF_GRA },           // 12
	{ "goldchr1.c44",	0x80000, 0x55db41cd, 5 | BRF_GRA },           // 13
	{ "goldchr0.c43",	0x80000, 0x76572c3f, 5 | BRF_GRA },           // 14

	{ "gm5-1.bin",		0x10000, 0x77c601a3, 0 | BRF_OPT },           // 15 Unknown?
};

STD_ROM_PICK(goldmedla)
STD_ROM_FN(goldmedla)

static INT32 GoldmedlaRomCb()
{
	memset (Drv68KROM, 0xff, 0x80000);
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x040001,  2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x040000,  3, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x08000);
	if (BurnLoadRom(DrvZ80ROM  + 0x020000,  5, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x030000,  6, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x040000,  7, 1)) return 1;

//	if (BurnLoadRom(DrvMCUROM  + 0x000000,  8, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000001,  9, 2)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x000000, 10, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000, 12, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000, 13, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x180000, 14, 1)) return 1;

	return 0;
}

static INT32 GoldmedlaInit()
{
	return Drv2Init(GoldmedlaRomCb, 0, 0x8803, 0x2423, 0);
}

struct BurnDriver BurnDrvGoldmedla = {
	"goldmedla", "goldmedl", NULL, NULL, "1988",
	"Gold Medalist (set 2)\0", NULL, "SNK", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, goldmedlaRomInfo, goldmedlaRomName, NULL, NULL, NULL, NULL, GoldmedlInputInfo, GoldmedlDIPInfo,
	GoldmedlaInit, DrvExit, DrvFrame, Alpha68KIIDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Gold Medalist (bootleg)

static struct BurnRomInfo goldmedlbRomDesc[] = {
	{ "l_3.bin",		0x10000, 0x5e106bcf, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "l_4.bin",		0x10000, 0xe19966af, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "l_1.bin",		0x08000, 0x7eec7ee5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "l_2.bin",		0x08000, 0xbf59e4f9, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "l_5.bin",		0x10000, 0x77c601a3, 0 | BRF_OPT },           //  4 Unknown?

	{ "38.bin",		0x10000, 0x4bf251b8, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	{ "39.bin",		0x10000, 0x1d92be86, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "40.bin",		0x10000, 0x8dafc4e8, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "1.bin",		0x10000, 0x1e78062c, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "gm.6",		0x08000, 0x56020b13, 3 | BRF_GRA },           //  9 Characters
	{ "gm.5",		0x08000, 0x667f33f1, 3 | BRF_GRA },           // 10

	{ "goldchr3.c46",	0x80000, 0x6faaa07a, 4 | BRF_GRA },           // 11 Sprites
	{ "goldchr2.c45",	0x80000, 0xe6b0aa2c, 4 | BRF_GRA },           // 12
	{ "goldchr1.c44",	0x80000, 0x55db41cd, 4 | BRF_GRA },           // 13
	{ "goldchr0.c43",	0x80000, 0x76572c3f, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(goldmedlb)
STD_ROM_FN(goldmedlb)

struct BurnDriver BurnDrvGoldmedlb = {
	"goldmedlb", "goldmedl", NULL, NULL, "1988",
	"Gold Medalist (bootleg)\0", NULL, "bootleg", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, goldmedlbRomInfo, goldmedlbRomName, NULL, NULL, NULL, NULL, GoldmedlInputInfo, GoldmedlDIPInfo,
	GoldmedlaInit, DrvExit, DrvFrame, Alpha68KIIDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Sky Adventure (World)

static struct BurnRomInfo skyadvntRomDesc[] = {
	{ "sa1.bin",		0x20000, 0xc2b23080, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sa2.bin",		0x20000, 0x06074e72, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sa.3",		0x10000, 0x3d0b32e0, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code
	{ "sa.4",		0x10000, 0xc2e3c30c, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "sa.5",		0x10000, 0x11cdb868, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "sa.6",		0x08000, 0x237d93fd, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "alpha.mcu",		0x01000, 0x00000000, 3 | BRF_NODUMP | BRF_OPT }, //  6 MCU Code

	{ "sa.7",		0x08000, 0xea26e9c5, 4 | BRF_GRA },           //  7 Characters

	{ "sachr3",		0x80000, 0xa986b8d5, 5 | BRF_GRA },           //  8 Sprites
	{ "sachr2",		0x80000, 0x504b07ae, 5 | BRF_GRA },           //  9
	{ "sachr1",		0x80000, 0xe734dccd, 5 | BRF_GRA },           // 10
	{ "sachr0",		0x80000, 0xe281b204, 5 | BRF_GRA },           // 11
};

STD_ROM_PICK(skyadvnt)
STD_ROM_FN(skyadvnt)

static INT32 SkyadvntRomCb()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x08000);
	if (BurnLoadRom(DrvZ80ROM  + 0x030000,  3, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x050000,  4, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x070000,  5, 1)) return 1;

//	if (BurnLoadRom(DrvMCUROM  + 0x000000,  6, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x300000, 11, 1)) return 1;

	memcpy (DrvGfxROM1 + 0x080000, DrvGfxROM1 + 0x000000, 0x080000);
	memcpy (DrvGfxROM1 + 0x180000, DrvGfxROM1 + 0x100000, 0x080000);
	memcpy (DrvGfxROM1 + 0x280000, DrvGfxROM1 + 0x200000, 0x080000);
	memcpy (DrvGfxROM1 + 0x380000, DrvGfxROM1 + 0x300000, 0x080000);

	return 0;
}

static INT32 SkyadvntInit()
{
	return Drv5Init(SkyadvntRomCb, 0, 0x8814, 0x2222, 5);
}

struct BurnDriver BurnDrvSkyadvnt = {
	"skyadvnt", NULL, NULL, NULL, "1989",
	"Sky Adventure (World)\0", NULL, "Alpha Denshi Co.", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, skyadvntRomInfo, skyadvntRomName, NULL, NULL, NULL, NULL, GangwarsInputInfo, SkyadvntDIPInfo,
	SkyadvntInit, DrvExit, DrvFrame, SkyadvntDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};


// Sky Adventure (US)

static struct BurnRomInfo skyadvntuRomDesc[] = {
	{ "sa_v3.1",		0x20000, 0x862393b5, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sa_v3.2",		0x20000, 0xfa7a14d1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sa.3",		0x10000, 0x3d0b32e0, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code
	{ "sa.4",		0x10000, 0xc2e3c30c, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "sa.5",		0x10000, 0x11cdb868, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "sa.6",		0x08000, 0x237d93fd, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "alpha.mcu",		0x01000, 0x00000000, 3 | BRF_NODUMP | BRF_OPT }, //  6 MCU Code

	{ "sa.7",		0x08000, 0xea26e9c5, 4 | BRF_GRA },           //  7 Characters

	{ "sachr3",		0x80000, 0xa986b8d5, 5 | BRF_GRA },           //  8 Sprites
	{ "sachr2",		0x80000, 0x504b07ae, 5 | BRF_GRA },           //  9
	{ "sachr1",		0x80000, 0xe734dccd, 5 | BRF_GRA },           // 10
	{ "sachr0",		0x80000, 0xe281b204, 5 | BRF_GRA },           // 11
};

STD_ROM_PICK(skyadvntu)
STD_ROM_FN(skyadvntu)

static INT32 SkyadvntuInit()
{
	return Drv5Init(SkyadvntRomCb, 0, 0x8814, 0x2423, 5);
}

struct BurnDriver BurnDrvSkyadvntu = {
	"skyadvntu", "skyadvnt", NULL, NULL, "1989",
	"Sky Adventure (US)\0", NULL, "Alpha Denshi Co. (SNK of America license)", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, skyadvntuRomInfo, skyadvntuRomName, NULL, NULL, NULL, NULL, GangwarsInputInfo, SkyadvntuDIPInfo,
	SkyadvntuInit, DrvExit, DrvFrame, SkyadvntDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};


// Sky Adventure (Japan)

static struct BurnRomInfo skyadvntjRomDesc[] = {
	{ "saj1.c19",		0x20000, 0x662cb4b8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "saj2.e19",		0x20000, 0x06d6130a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sa.3",		0x10000, 0x3d0b32e0, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code
	{ "sa.4",		0x10000, 0xc2e3c30c, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "sa.5",		0x10000, 0x11cdb868, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "sa.6",		0x08000, 0x237d93fd, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "alpha.mcu",		0x01000, 0x00000000, 3 | BRF_NODUMP | BRF_OPT }, //  6 MCU Code

	{ "sa.7",		0x08000, 0xea26e9c5, 4 | BRF_GRA },           //  7 Characters

	{ "sachr3",		0x80000, 0xa986b8d5, 5 | BRF_GRA },           //  8 Sprites
	{ "sachr2",		0x80000, 0x504b07ae, 5 | BRF_GRA },           //  9
	{ "sachr1",		0x80000, 0xe734dccd, 5 | BRF_GRA },           // 10
	{ "sachr0",		0x80000, 0xe281b204, 5 | BRF_GRA },           // 11
};

STD_ROM_PICK(skyadvntj)
STD_ROM_FN(skyadvntj)

struct BurnDriver BurnDrvSkyadvntj = {
	"skyadvntj", "skyadvnt", NULL, NULL, "1989",
	"Sky Adventure (Japan)\0", NULL, "Alpha Denshi Co.", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, skyadvntjRomInfo, skyadvntjRomName, NULL, NULL, NULL, NULL, GangwarsInputInfo, SkyadvntDIPInfo,
	SkyadvntInit, DrvExit, DrvFrame, SkyadvntDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};


// Gang Wars

static struct BurnRomInfo gangwarsRomDesc[] = {
	{ "gw-ver1-e1.19c",	0x20000, 0x7752478e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gw-ver1-e2.19d",	0x20000, 0xc2f3b85e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gw-ver1-e3.18c",	0x20000, 0x2a5fe86e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gw-ver1-e4.18d",	0x20000, 0xc8b60c53, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gw-12.10f",		0x10000, 0xe6d6c9cf, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "gw-11.11f",		0x10000, 0x7b9f2608, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "gw-10.13f",		0x10000, 0xeb305d42, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "gw-9.15f",		0x10000, 0x84e5c946, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "alpha.mcu",		0x01000, 0x00000000, 3 | BRF_NODUMP | BRF_OPT }, //  8 MCU Code

	{ "gw-13.4l",		0x10000, 0xb75bf1d0, 4 | BRF_GRA },           //  9 Characters

	{ "guernica-c3.17h",	0x80000, 0x281a4138, 5 | BRF_GRA },           // 10 Sprites
	{ "gw-5.21f",		0x20000, 0x9ef36031, 5 | BRF_GRA },           // 11
	{ "guernica-c2.18h",	0x80000, 0x2fcbea97, 5 | BRF_GRA },           // 12
	{ "gw-6.20f",		0x20000, 0xddbbcda7, 5 | BRF_GRA },           // 13
	{ "guernica-c1.20h",	0x80000, 0xb0fd1c23, 5 | BRF_GRA },           // 14
	{ "gw-7.18f",		0x20000, 0x4656d377, 5 | BRF_GRA },           // 15
	{ "guernica-c0.21h",	0x80000, 0xe60c9882, 5 | BRF_GRA },           // 16
	{ "gw-8.17f",		0x20000, 0x798ed82a, 5 | BRF_GRA },           // 17
};

STD_ROM_PICK(gangwars)
STD_ROM_FN(gangwars)

static INT32 GangwarsRomCb()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x040001,  2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x040000,  3, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x08000);
	if (BurnLoadRom(DrvZ80ROM  + 0x030000,  5, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x050000,  6, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x070000,  7, 1)) return 1;

//	if (BurnLoadRom(DrvMCUROM  + 0x000000,  8, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  9, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000, 12, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x180000, 13, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000, 14, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x280000, 15, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x300000, 16, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x380000, 17, 1)) return 1;

	return 0;
}

static INT32 GangwarsInit()
{
	return Drv5Init(GangwarsRomCb, 0, 0x8512, 0x2423, 5);
}

struct BurnDriver BurnDrvGangwars = {
	"gangwars", NULL, NULL, NULL, "1989",
	"Gang Wars\0", NULL, "Alpha Denshi Co.", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, gangwarsRomInfo, gangwarsRomName, NULL, NULL, NULL, NULL, GangwarsInputInfo, GangwarsDIPInfo,
	GangwarsInit, DrvExit, DrvFrame, GangwarsDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Gang Wars (bootleg)

static struct BurnRomInfo gangwarsbRomDesc[] = {
	{ "gwb_ic.m15",		0x20000, 0x7752478e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gwb_ic.m16",		0x20000, 0xc2f3b85e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gwb_ic.m17",		0x20000, 0x2a5fe86e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gwb_ic.m18",		0x20000, 0xc8b60c53, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gwb_ic.380",		0x10000, 0xe6d6c9cf, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "gwb_ic.421",		0x10000, 0x7b9f2608, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "gwb_ic.420",		0x10000, 0xeb305d42, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "gwb_ic.419",		0x10000, 0x84e5c946, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "68705.mcu",		0x01000, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, //  8 MCU Code

	{ "gwb_ic.m19",		0x10000, 0xb75bf1d0, 3 | BRF_GRA },           //  9 Characters

	{ "gwb_ic.308",		0x10000, 0x321a2fdd, 4 | BRF_GRA },           // 10 Sprites
	{ "gwb_ic.309",		0x10000, 0x4d908f65, 4 | BRF_GRA },           // 11
	{ "gwb_ic.310",		0x10000, 0xfc888541, 4 | BRF_GRA },           // 12
	{ "gwb_ic.311",		0x10000, 0x181b128b, 4 | BRF_GRA },           // 13
	{ "gwb_ic.312",		0x10000, 0x930665f3, 4 | BRF_GRA },           // 14
	{ "gwb_ic.313",		0x10000, 0xc18f4ca8, 4 | BRF_GRA },           // 15
	{ "gwb_ic.314",		0x10000, 0xdfc44b60, 4 | BRF_GRA },           // 16
	{ "gwb_ic.307",		0x10000, 0x28082a7f, 4 | BRF_GRA },           // 17
	{ "gwb_ic.280",		0x10000, 0x222b3dcd, 4 | BRF_GRA },           // 18
	{ "gwb_ic.321",		0x10000, 0x6b421c7b, 4 | BRF_GRA },           // 19
	{ "gwb_ic.300",		0x10000, 0xf3fa0877, 4 | BRF_GRA },           // 20
	{ "gwb_ic.301",		0x10000, 0xf8c866de, 4 | BRF_GRA },           // 21
	{ "gwb_ic.302",		0x10000, 0x5b0d587d, 4 | BRF_GRA },           // 22
	{ "gwb_ic.303",		0x10000, 0xd8c0e102, 4 | BRF_GRA },           // 23
	{ "gwb_ic.304",		0x10000, 0xb02bc9d8, 4 | BRF_GRA },           // 24
	{ "gwb_ic.305",		0x10000, 0x5e04a9aa, 4 | BRF_GRA },           // 25
	{ "gwb_ic.306",		0x10000, 0xe2172955, 4 | BRF_GRA },           // 26
	{ "gwb_ic.299",		0x10000, 0xe39f5599, 4 | BRF_GRA },           // 27
	{ "gwb_ic.320",		0x10000, 0x9a7b51d8, 4 | BRF_GRA },           // 28
	{ "gwb_ic.319",		0x10000, 0xc5b862b7, 4 | BRF_GRA },           // 29
	{ "gwb_ic.292",		0x10000, 0xc125f7be, 4 | BRF_GRA },           // 30
	{ "gwb_ic.293",		0x10000, 0xc04fce8e, 4 | BRF_GRA },           // 31
	{ "gwb_ic.294",		0x10000, 0x4eda3df5, 4 | BRF_GRA },           // 32
	{ "gwb_ic.295",		0x10000, 0x6e60c475, 4 | BRF_GRA },           // 33
	{ "gwb_ic.296",		0x10000, 0x99b2a557, 4 | BRF_GRA },           // 34
	{ "gwb_ic.297",		0x10000, 0x10373f63, 4 | BRF_GRA },           // 35
	{ "gwb_ic.298",		0x10000, 0xdf37ec4d, 4 | BRF_GRA },           // 36
	{ "gwb_ic.291",		0x10000, 0xbeb07a2e, 4 | BRF_GRA },           // 37
	{ "gwb_ic.318",		0x10000, 0x9aeaddf9, 4 | BRF_GRA },           // 38
	{ "gwb_ic.317",		0x10000, 0x1622fadd, 4 | BRF_GRA },           // 39
	{ "gwb_ic.284",		0x10000, 0x4aa95d66, 4 | BRF_GRA },           // 40
	{ "gwb_ic.285",		0x10000, 0x3a1f3ce0, 4 | BRF_GRA },           // 41
	{ "gwb_ic.286",		0x10000, 0x886e298b, 4 | BRF_GRA },           // 42
	{ "gwb_ic.287",		0x10000, 0xb9542e6a, 4 | BRF_GRA },           // 43
	{ "gwb_ic.288",		0x10000, 0x8e620056, 4 | BRF_GRA },           // 44
	{ "gwb_ic.289",		0x10000, 0xc754d69f, 4 | BRF_GRA },           // 45
	{ "gwb_ic.290",		0x10000, 0x306d1963, 4 | BRF_GRA },           // 46
	{ "gwb_ic.283",		0x10000, 0xb46e5761, 4 | BRF_GRA },           // 47
	{ "gwb_ic.316",		0x10000, 0x655b1518, 4 | BRF_GRA },           // 48
	{ "gwb_ic.315",		0x10000, 0xe7c9b103, 4 | BRF_GRA },           // 49
};

STD_ROM_PICK(gangwarsb)
STD_ROM_FN(gangwarsb)

static INT32 GangwarsbRomCb()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x040001,  2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x040000,  3, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x08000);
	if (BurnLoadRom(DrvZ80ROM  + 0x030000,  5, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x050000,  6, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x070000,  7, 1)) return 1;

//	if (BurnLoadRom(DrvMCUROM  + 0x000000,  8, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  9, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x010000, 11, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x020000, 12, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x030000, 13, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x040000, 14, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x050000, 15, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x060000, 16, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x070000, 17, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000, 18, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x090000, 19, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000, 20, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x110000, 21, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x120000, 22, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x130000, 23, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x140000, 24, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x150000, 25, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x160000, 26, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x170000, 27, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x180000, 28, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x190000, 29, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000, 30, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x210000, 31, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x220000, 32, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x230000, 33, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x240000, 34, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x250000, 35, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x260000, 36, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x270000, 37, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x280000, 38, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x290000, 39, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x300000, 40, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x310000, 41, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x320000, 42, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x330000, 43, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x340000, 44, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x350000, 45, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x360000, 46, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x370000, 47, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x380000, 48, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x390000, 49, 1)) return 1;
	return 0;
}

static INT32 GangwarsbInit()
{
	return Drv5Init(GangwarsbRomCb, 0, 0x8512, 0x2423, 5);
}

struct BurnDriver BurnDrvGangwarsb = {
	"gangwarsb", "gangwars", NULL, NULL, "1989",
	"Gang Wars (bootleg)\0", NULL, "bootleg", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, gangwarsbRomInfo, gangwarsbRomName, NULL, NULL, NULL, NULL, GangwarsInputInfo, GangwarsDIPInfo,
	GangwarsbInit, DrvExit, DrvFrame, GangwarsDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Gang Wars (Japan)

static struct BurnRomInfo gangwarsjRomDesc[] = {
	{ "gw-ver1-j1.19c",	0x20000, 0x98bf9668, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gw-ver1-j2.19d",	0x20000, 0x41868606, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gw-ver1-j3.18c",	0x20000, 0x6e6b7e1f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gw-ver1-j4.18d",	0x20000, 0x1f13eb20, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gw-12.10f",		0x10000, 0xe6d6c9cf, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "gw-11.11f",		0x10000, 0x7b9f2608, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "gw-10.13f",		0x10000, 0xeb305d42, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "gw-9.15f",		0x10000, 0x84e5c946, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "alpha.mcu",		0x01000, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, //  8 MCU Code

	{ "gw-13.4l",		0x10000, 0xb75bf1d0, 3 | BRF_GRA },           //  9 Characters

	{ "guernica-c3.17h",	0x80000, 0x281a4138, 4 | BRF_GRA },           // 10 Sprites
	{ "gw-5.21f",		0x20000, 0x9ef36031, 4 | BRF_GRA },           // 11
	{ "guernica-c2.18h",	0x80000, 0x2fcbea97, 4 | BRF_GRA },           // 12
	{ "gw-6.20f",		0x20000, 0xddbbcda7, 4 | BRF_GRA },           // 13
	{ "guernica-c1.20h",	0x80000, 0xb0fd1c23, 4 | BRF_GRA },           // 14
	{ "gw-7.18f",		0x20000, 0x4656d377, 4 | BRF_GRA },           // 15
	{ "guernica-c0.21h",	0x80000, 0xe60c9882, 4 | BRF_GRA },           // 16
	{ "gw-8.17f",		0x20000, 0x798ed82a, 4 | BRF_GRA },           // 17
};

STD_ROM_PICK(gangwarsj)
STD_ROM_FN(gangwarsj)

struct BurnDriver BurnDrvGangwarsj = {
	"gangwarsj", "gangwars", NULL, NULL, "1989",
	"Gang Wars (Japan)\0", NULL, "Alpha Denshi Co.", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, gangwarsjRomInfo, gangwarsjRomName, NULL, NULL, NULL, NULL, GangwarsInputInfo, GangwarsDIPInfo,
	GangwarsInit, DrvExit, DrvFrame, GangwarsDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Gang Wars (US)

static struct BurnRomInfo gangwarsuRomDesc[] = {
	{ "u1",			0x20000, 0x11433507, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "u2",			0x20000, 0x44cc375f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u3",			0x20000, 0xde6fd3c0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "u4",			0x20000, 0x43f7f5d3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "u12",		0x10000, 0x2620caa1, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "u11",		0x10000, 0x2218ceb9, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "u10",		0x10000, 0x636978ae, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "u9",			0x10000, 0x9136745e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "alpha.mcu",		0x01000, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, //  8 MCU Code

	{ "gw-13.4l",		0x10000, 0xb75bf1d0, 3 | BRF_GRA },           //  9 Characters

	{ "guernica-c3.17h",	0x80000, 0x281a4138, 4 | BRF_GRA },           // 10 Sprites
	{ "u5",			0x20000, 0x94612190, 4 | BRF_GRA },           // 11
	{ "guernica-c2.18h",	0x80000, 0x2fcbea97, 4 | BRF_GRA },           // 12
	{ "u6",			0x20000, 0x5a4ea0f0, 4 | BRF_GRA },           // 13
	{ "guernica-c1.20h",	0x80000, 0xb0fd1c23, 4 | BRF_GRA },           // 14
	{ "u7",			0x20000, 0x33f324cb, 4 | BRF_GRA },           // 15
	{ "guernica-c0.21h",	0x80000, 0xe60c9882, 4 | BRF_GRA },           // 16
	{ "u8",			0x20000, 0xc1995c2c, 4 | BRF_GRA },           // 17
};

STD_ROM_PICK(gangwarsu)
STD_ROM_FN(gangwarsu)

struct BurnDriver BurnDrvGangwarsu = {
	"gangwarsu", "gangwars", NULL, NULL, "1989",
	"Gang Wars (US)\0", NULL, "Alpha Denshi Co.", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, gangwarsuRomInfo, gangwarsuRomName, NULL, NULL, NULL, NULL, GangwarsInputInfo, GangwarsuDIPInfo,
	GangwarsInit, DrvExit, DrvFrame, GangwarsDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Super Champion Baseball (US)

static struct BurnRomInfo sbasebalRomDesc[] = {
	{ "snksb1.c19",		0x20000, 0x304fef2d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "snksb2.e19",		0x20000, 0x35821339, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sb-3.g9",		0x10000, 0x89e12f25, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code
	{ "sb-4.g11",		0x10000, 0xcca2555d, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "sb-5.g13",		0x10000, 0xf45ee36f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "sb-6.g15",		0x10000, 0x651c9472, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "alpha.mcu",		0x01000, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, //  6 MCU Code

	{ "sb-7.l3",		0x10000, 0x8f3c2e25, 3 | BRF_GRA },           //  7 Characters

	{ "kcb-chr3.h21",	0x80000, 0x719071c7, 4 | BRF_GRA },           //  8 Sprites
	{ "kcb-chr2.h19",	0x80000, 0x014f0f90, 4 | BRF_GRA },           //  9
	{ "kcb-chr1.h18",	0x80000, 0xa5ce1e10, 4 | BRF_GRA },           // 10
	{ "kcb-chr0.h16",	0x80000, 0xb8a1a088, 4 | BRF_GRA },           // 11
};

STD_ROM_PICK(sbasebal)
STD_ROM_FN(sbasebal)

static INT32 SbasebalRomCb()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;
	memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x08000);
	if (BurnLoadRom(DrvZ80ROM  + 0x030000,  3, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x050000,  4, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM  + 0x070000,  5, 1)) return 1;

//	if (BurnLoadRom(DrvMCUROM  + 0x000000,  6, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  7, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x300000, 11, 1)) return 1;

	return 0;
}

static INT32 SbasebalInit()
{
	INT32 rc = Drv5Init(SbasebalRomCb, 0, 0x8512, 0x2423, 5);
	if (!rc) {
		UINT16 *rom = (UINT16*)Drv68KROM;
		rom[0xb672/2] = 0x4e71;
		rom[0x44e/2] = 0x4e71;
		rom[0x450/2] = 0x4e71;
		rom[0x458/2] = 0x4e71;
		rom[0x45a/2] = 0x4e71;
	}
	return rc;
}

struct BurnDriver BurnDrvSbasebal = {
	"sbasebal", NULL, NULL, NULL, "1989",
	"Super Champion Baseball (US)\0", NULL, "Alpha Denshi Co. (SNK of America license)", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, sbasebalRomInfo, sbasebalRomName, NULL, NULL, NULL, NULL, SbasebalInputInfo, SbasebalDIPInfo,
	SbasebalInit, DrvExit, DrvFrame, SbasebalDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Super Champion Baseball (Japan)

static struct BurnRomInfo sbasebaljRomDesc[] = {
	{ "sb-j-1.c19",		0x20000, 0xc46a3c03, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sb-j-2.e19",		0x20000, 0xa8ec2287, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sb-3.g9",		0x10000, 0x89e12f25, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code
	{ "sb-4.g11",		0x10000, 0xcca2555d, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "sb-5.g13",		0x10000, 0xf45ee36f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "sb-6.g15",		0x10000, 0x651c9472, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "alpha.mcu",		0x01000, 0x00000000, 0 | BRF_NODUMP | BRF_OPT }, //  6 MCU Code

	{ "sb-7.l3",		0x10000, 0x8f3c2e25, 3 | BRF_GRA },           //  7 Characters

	{ "kcb-chr3.h21",	0x80000, 0x719071c7, 4 | BRF_GRA },           //  8 Sprites
	{ "kcb-chr2.h19",	0x80000, 0x014f0f90, 4 | BRF_GRA },           //  9
	{ "kcb-chr1.h18",	0x80000, 0xa5ce1e10, 4 | BRF_GRA },           // 10
	{ "kcb-chr0.h16",	0x80000, 0xb8a1a088, 4 | BRF_GRA },           // 11
};

STD_ROM_PICK(sbasebalj)
STD_ROM_FN(sbasebalj)

static INT32 SbasebaljInit()
{
	return Drv5Init(SbasebalRomCb, 0, 0x8512, 0x2423, 5);
}

struct BurnDriver BurnDrvSbasebalj = {
	"sbasebalj", "sbasebal", NULL, NULL, "1989",
	"Super Champion Baseball (Japan)\0", NULL, "Alpha Denshi Co.", "Alpha 68k",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, sbasebaljRomInfo, sbasebaljRomName, NULL, NULL, NULL, NULL, SbasebalInputInfo, SbasebalDIPInfo /*wrong*/,
	SbasebaljInit, DrvExit, DrvFrame, SbasebalDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};
