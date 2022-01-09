// FB Alpha Fuuki FG-3 driver module
// Based on MAME driver by Paul Priest and David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ymf278b.h"
#include "burn_ymf262.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf0;
static UINT8 *DrvSprBuf1;
static UINT8 *DrvFgRAM1;
static UINT8 *DrvFgRAM2;
static UINT8 *DrvBgRAM1;
static UINT8 *DrvBgRAM2;
static UINT8 *DrvVidRegs;
static UINT8 *DrvShareRAM;
static UINT8 *DrvZ80RAM;
static UINT32 *DrvScrollBuf;
static UINT8 *DrvTransTab1;
static UINT8 *DrvTransTab2;
static UINT8 *DrvTransTab3;

static UINT16 *DrvRasterPos;
static UINT8  *nDrvZ80Bank;
static UINT16 *tilebank;
static UINT16 *tilebank_buf;
static UINT8  *priority;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 DrvInputs[4];
static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[4];
static UINT8 DrvReset;

static INT32 nEnableRaster[3];

static INT32 asurablade = 0;
static INT32 is_usa = 0;

static struct BurnInputInfo AsurabldInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},
	{"P2 Button 3",	BIT_DIGITAL,	DrvJoy2 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 8,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Asurabld)

static struct BurnInputInfo AsurabusaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},
	{"P1 Button 4",	BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 4"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},
	{"P2 Button 3",	BIT_DIGITAL,	DrvJoy2 + 14,	"p2 fire 3"	},
	{"P2 Button 4",	BIT_DIGITAL,	DrvJoy2 + 15,	"p2 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 8,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Asurabusa)

static struct BurnDIPInfo AsurabldDIPList[]=
{
	DIP_OFFSET(0x14)

	{0x00, 0xff, 0xff, 0x3f, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Blood Colour"			},
	{0x00, 0x01, 0x02, 0x02, "Red"					},
	{0x00, 0x01, 0x02, 0x00, "Green"				},

	{0   , 0xfe, 0   ,    4, "Demo Sounds & Music"	},
	{0x00, 0x01, 0x0c, 0x0c, "Both On"				},
	{0x00, 0x01, 0x0c, 0x08, "Music Off"			},
	{0x00, 0x01, 0x0c, 0x04, "Both Off"				},
	{0x00, 0x01, 0x0c, 0x00, "Both Off"				},  /* Duplicate setting */

	{0   , 0xfe, 0   ,    4, "Timer"				},
	{0x00, 0x01, 0x30, 0x00, "Slow"					},
	{0x00, 0x01, 0x30, 0x30, "Medium"				},
	{0x00, 0x01, 0x30, 0x10, "Fast"					},
	{0x00, 0x01, 0x30, 0x20, "Very Fast"			},

	{0   , 0xfe, 0   ,    2, "Coinage Mode"			},
	{0x00, 0x01, 0xc0, 0xc0, "Split"				},
	{0x00, 0x01, 0xc0, 0x00, "Joint"				},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x01, 0x01, 0x01, 0x01, "Off"					},
//	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x01, 0x01, 0x0e, 0x00, "Easiest"				},  // Level 1
	{0x01, 0x01, 0x0e, 0x08, "Very_Easy"			},  // Level 2
	{0x01, 0x01, 0x0e, 0x04, "Easier"				},  // Level 3
	{0x01, 0x01, 0x0e, 0x0c, "Easy"					},  // Level 4
	{0x01, 0x01, 0x0e, 0x0e, "Normal"				},  // Level 5
	{0x01, 0x01, 0x0e, 0x02, "Hard"					},  // Level 6
	{0x01, 0x01, 0x0e, 0x0a, "Very_Hard"			},  // Level 7
	{0x01, 0x01, 0x0e, 0x06, "Hardest"				},  // Level 8

	{0   , 0xfe, 0   ,    4, "Damage"				},
	{0x01, 0x01, 0x30, 0x20, "75%"					},
	{0x01, 0x01, 0x30, 0x30, "100%"					},
	{0x01, 0x01, 0x30, 0x10, "125%"					},
	{0x01, 0x01, 0x30, 0x00, "150%"					},

	{0   , 0xfe, 0   ,    3, "Max Rounds"			},
	{0x01, 0x01, 0xc0, 0x00, "1"					},
	{0x01, 0x01, 0xc0, 0xc0, "3"					},
	{0x01, 0x01, 0xc0, 0x80, "5"					},

	{0   , 0xfe, 0   ,   14, "Coin B"				},
	{0x02, 0x01, 0x0f, 0x08, "8 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x09, "7 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0a, "6 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0b, "5 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0c, "4 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0d, "3 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0e, "2 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x02, 0x01, 0x0f, 0x06, "1 Coin  2 Credits"	},
	{0x02, 0x01, 0x0f, 0x05, "1 Coin  3 Credits"	},
	{0x02, 0x01, 0x0f, 0x04, "1 Coin  4 Credits"	},
	{0x02, 0x01, 0x0f, 0x03, "1 Coin  5 Credits"	},
	{0x02, 0x01, 0x0f, 0x02, "2 Coins Start / 1 Credit Continue"},
	{0x02, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   14, "Coin A"				},
	{0x02, 0x01, 0xf0, 0x80, "8 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0x90, "7 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xa0, "6 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xb0, "5 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xc0, "4 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xd0, "3 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xe0, "2 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x02, 0x01, 0xf0, 0x60, "1 Coin  2 Credits"	},
	{0x02, 0x01, 0xf0, 0x50, "1 Coin  3 Credits"	},
	{0x02, 0x01, 0xf0, 0x40, "1 Coin  4 Credits"	},
	{0x02, 0x01, 0xf0, 0x30, "1 Coin  5 Credits"	},
	{0x02, 0x01, 0xf0, 0x20, "2 Coins Start / 1 Credit Continue"},
	{0x02, 0x01, 0xf0, 0x00, "Free Play"			},
};

STDDIPINFO(Asurabld)

static struct BurnDIPInfo AsurabusDIPList[]=
{
	DIP_OFFSET(0x14)

	{0x00, 0xff, 0xff, 0x3f, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Blood Colour"			},
	{0x00, 0x01, 0x02, 0x02, "Red"					},
	{0x00, 0x01, 0x02, 0x00, "Green"				},

	{0   , 0xfe, 0   ,    4, "Demo Sounds & Music"	},
	{0x00, 0x01, 0x0c, 0x0c, "Both On"				},
	{0x00, 0x01, 0x0c, 0x08, "Sounds Off"			},
	{0x00, 0x01, 0x0c, 0x04, "Music Off"			},
	{0x00, 0x01, 0x0c, 0x00, "Both Off"				},

	{0   , 0xfe, 0   ,    4, "Timer"				},
	{0x00, 0x01, 0x30, 0x00, "Slow"					},
	{0x00, 0x01, 0x30, 0x30, "Medium"				},
	{0x00, 0x01, 0x30, 0x10, "Fast"					},
	{0x00, 0x01, 0x30, 0x20, "Very Fast"			},

	{0   , 0xfe, 0   ,    2, "Coinage Mode"			},
	{0x00, 0x01, 0xc0, 0xc0, "Split"				},
	{0x00, 0x01, 0xc0, 0x00, "Joint"				},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x01, 0x01, 0x01, 0x01, "Off"					},
//	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x01, 0x01, 0x0e, 0x00, "Easiest"				},  // Level 1
	{0x01, 0x01, 0x0e, 0x08, "Very_Easy"			},  // Level 2
	{0x01, 0x01, 0x0e, 0x04, "Easier"				},  // Level 3
	{0x01, 0x01, 0x0e, 0x0c, "Easy"					},  // Level 4
	{0x01, 0x01, 0x0e, 0x0e, "Normal"				},  // Level 5
	{0x01, 0x01, 0x0e, 0x02, "Hard"					},  // Level 6
	{0x01, 0x01, 0x0e, 0x0a, "Very_Hard"			},  // Level 7
	{0x01, 0x01, 0x0e, 0x06, "Hardest"				},  // Level 8

	{0   , 0xfe, 0   ,    4, "Damage"				},
	{0x01, 0x01, 0x30, 0x20, "75%"					},
	{0x01, 0x01, 0x30, 0x30, "100%"					},
	{0x01, 0x01, 0x30, 0x10, "125%"					},
	{0x01, 0x01, 0x30, 0x00, "150%"					},

	{0   , 0xfe, 0   ,    3, "Max Rounds"			},
	{0x01, 0x01, 0xc0, 0x00, "1"					},
	{0x01, 0x01, 0xc0, 0xc0, "3"					},
	{0x01, 0x01, 0xc0, 0x80, "5"					},

	{0   , 0xfe, 0   ,   14, "Coin B"				},
	{0x02, 0x01, 0x0f, 0x08, "8 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x09, "7 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0a, "6 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0b, "5 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0c, "4 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0d, "3 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0e, "2 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x02, 0x01, 0x0f, 0x06, "1 Coin  2 Credits"	},
	{0x02, 0x01, 0x0f, 0x05, "1 Coin  3 Credits"	},
	{0x02, 0x01, 0x0f, 0x04, "1 Coin  4 Credits"	},
	{0x02, 0x01, 0x0f, 0x03, "1 Coin  5 Credits"	},
	{0x02, 0x01, 0x0f, 0x02, "2 Coins Start / 1 Credit Continue"},
	{0x02, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   14, "Coin A"				},
	{0x02, 0x01, 0xf0, 0x80, "8 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0x90, "7 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xa0, "6 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xb0, "5 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xc0, "4 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xd0, "3 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xe0, "2 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x02, 0x01, 0xf0, 0x60, "1 Coin  2 Credits"	},
	{0x02, 0x01, 0xf0, 0x50, "1 Coin  3 Credits"	},
	{0x02, 0x01, 0xf0, 0x40, "1 Coin  4 Credits"	},
	{0x02, 0x01, 0xf0, 0x30, "1 Coin  5 Credits"	},
	{0x02, 0x01, 0xf0, 0x20, "2 Coins Start / 1 Credit Continue"},
	{0x02, 0x01, 0xf0, 0x00, "Free Play"			},
};

STDDIPINFO(Asurabus)

static struct BurnDIPInfo AsurabusaDIPList[]=
{
	DIP_OFFSET(0x16)

	{0x00, 0xff, 0xff, 0x3f, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Blood Colour"			},
	{0x00, 0x01, 0x02, 0x02, "Red"					},
	{0x00, 0x01, 0x02, 0x00, "Green"				},

	{0   , 0xfe, 0   ,    4, "Demo Sounds & Music"	},
	{0x00, 0x01, 0x0c, 0x0c, "Both On"				},
	{0x00, 0x01, 0x0c, 0x08, "Sounds Off"			},
	{0x00, 0x01, 0x0c, 0x04, "Music Off"			},
	{0x00, 0x01, 0x0c, 0x00, "Both Off"				},

	{0   , 0xfe, 0   ,    4, "Timer"				},
	{0x00, 0x01, 0x30, 0x00, "Slow"					},
	{0x00, 0x01, 0x30, 0x30, "Medium"				},
	{0x00, 0x01, 0x30, 0x10, "Fast"					},
	{0x00, 0x01, 0x30, 0x20, "Very Fast"			},

	{0   , 0xfe, 0   ,    2, "Coinage Mode"			},
	{0x00, 0x01, 0xc0, 0xc0, "Split"				},
	{0x00, 0x01, 0xc0, 0x00, "Joint"				},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x01, 0x01, 0x01, 0x01, "Off"					},
//	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x01, 0x01, 0x0e, 0x00, "Easiest"				},  // Level 1
	{0x01, 0x01, 0x0e, 0x08, "Very_Easy"			},  // Level 2
	{0x01, 0x01, 0x0e, 0x04, "Easier"				},  // Level 3
	{0x01, 0x01, 0x0e, 0x0c, "Easy"					},  // Level 4
	{0x01, 0x01, 0x0e, 0x0e, "Normal"				},  // Level 5
	{0x01, 0x01, 0x0e, 0x02, "Hard"					},  // Level 6
	{0x01, 0x01, 0x0e, 0x0a, "Very_Hard"			},  // Level 7
	{0x01, 0x01, 0x0e, 0x06, "Hardest"				},  // Level 8

	{0   , 0xfe, 0   ,    4, "Damage"				},
	{0x01, 0x01, 0x30, 0x20, "75%"					},
	{0x01, 0x01, 0x30, 0x30, "100%"					},
	{0x01, 0x01, 0x30, 0x10, "125%"					},
	{0x01, 0x01, 0x30, 0x00, "150%"					},

	{0   , 0xfe, 0   ,    3, "Max Rounds"			},
	{0x01, 0x01, 0xc0, 0x00, "1"					},
	{0x01, 0x01, 0xc0, 0xc0, "3"					},
	{0x01, 0x01, 0xc0, 0x80, "5"					},

	{0   , 0xfe, 0   ,   14, "Coin B"				},
	{0x02, 0x01, 0x0f, 0x08, "8 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x09, "7 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0a, "6 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0b, "5 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0c, "4 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0d, "3 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0e, "2 Coins 1 Credit"		},
	{0x02, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x02, 0x01, 0x0f, 0x06, "1 Coin  2 Credits"	},
	{0x02, 0x01, 0x0f, 0x05, "1 Coin  3 Credits"	},
	{0x02, 0x01, 0x0f, 0x04, "1 Coin  4 Credits"	},
	{0x02, 0x01, 0x0f, 0x03, "1 Coin  5 Credits"	},
	{0x02, 0x01, 0x0f, 0x02, "2 Coins Start / 1 Credit Continue"},
	{0x02, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   14, "Coin A"				},
	{0x02, 0x01, 0xf0, 0x80, "8 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0x90, "7 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xa0, "6 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xb0, "5 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xc0, "4 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xd0, "3 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xe0, "2 Coins 1 Credit"		},
	{0x02, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x02, 0x01, 0xf0, 0x60, "1 Coin  2 Credits"	},
	{0x02, 0x01, 0xf0, 0x50, "1 Coin  3 Credits"	},
	{0x02, 0x01, 0xf0, 0x40, "1 Coin  4 Credits"	},
	{0x02, 0x01, 0xf0, 0x30, "1 Coin  5 Credits"	},
	{0x02, 0x01, 0xf0, 0x20, "2 Coins Start / 1 Credit Continue"},
	{0x02, 0x01, 0xf0, 0x00, "Free Play"			},
};

STDDIPINFO(Asurabusa)

static inline void cpu_sync() // sync z80 & 68k
{
	INT32 t = ((SekTotalCycles() * 3) / 10) - ZetTotalCycles();

	if (t > 0) {
		BurnTimerUpdate(t);
	}
}

static void __fastcall fuuki32_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffe0) == 0x8c0000) {

		if (address == 0x8c001c) {
			DrvRasterPos[0] = data & 0xff;
		}

		*((UINT16*)(DrvVidRegs + (address & 0x1e))) = BURN_ENDIAN_SWAP_INT16(data);

		return;
	}

	if ((address & 0xffffe0) == 0x903fe0) {
		cpu_sync();

		DrvShareRAM[(address & 0x1f) >> 1] = data & 0xff;
		return;
	}

	switch (address)
	{
		case 0x8e0000:
			priority[0] = data & 0x000f;
		return;

		case 0xa00000:
			tilebank[0] = data;
		return;
	}
}

static void __fastcall fuuki32_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffffe0) == 0x903fe0) {
		cpu_sync();

		DrvShareRAM[(address & 0x1f) >> 1] = data;
		return;
	}
}

static UINT16 __fastcall fuuki32_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x800000:
			return DrvInputs[0];

		case 0x810000:
			return DrvInputs[1];

		case 0x880000:
			return DrvInputs[2];

		case 0x890000:
			return DrvInputs[3];

		case 0x8c001e:
			return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRegs + 0x1e)));
	}
	bprintf(0, _T("rw %x\n"), address);
	return 0;
}

static UINT8 __fastcall fuuki32_read_byte(UINT32 address)
{
	if ((address & 0xffffe0) == 0x903fe0) {
		cpu_sync();
		return DrvShareRAM[(address & 0x1f) >> 1];
	}
	bprintf(0, _T("rb %x\n"), address);

	return 0;
}

static void bankswitch(INT32 data)
{
	nDrvZ80Bank[0] = data;

	INT32 nBank = (data & 0x0f) * 0x8000;

	ZetMapMemory(DrvZ80ROM + nBank, 0x8000, 0xffff, MAP_ROM);
}

static void __fastcall fuuki32_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x7ff0) {
		DrvShareRAM[address & 0x0f] = data;
		return;
	}
}

static UINT8 __fastcall fuuki32_sound_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x7ff0) {
		return DrvShareRAM[address & 0x0f];
	}

	return 0;
}

static void __fastcall fuuki32_sound_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			bankswitch(data);
		return;

		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
			BurnYMF262Write(port & 3, data);
		return;

		case 0x44:
			BurnYMF278BSelectRegister((port >> 1) & 3, data);
		return;

		case 0x45:
			BurnYMF278BWriteRegister((port >> 1) & 3, data);
		return;
	}
}

static UINT8 __fastcall fuuki32_sound_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x40:
			return BurnYMF262Read(0);
	}

	return 0;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 6000000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYMF278BReset();
	BurnYMF262Reset();
	ZetClose();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x200000;
	DrvZ80ROM	= Next; Next += 0x080000;

	DrvTransTab1= Next; Next += 0x008000;
	DrvTransTab2= Next; Next += 0x008000;
	DrvTransTab3= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x4000000;
	DrvGfxROM1	= Next; Next += 0x800000;
	DrvGfxROM2	= Next; Next += 0x800000;
	DrvGfxROM3	= Next; Next += 0x400000;

	DrvSndROM	= Next; Next += 0x400000;

	DrvPalette	= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);
	DrvScrollBuf= (UINT32*)Next; Next += 256 * 4 * sizeof(UINT32);
	DrvRasterPos= (UINT16*)Next; Next += 0x0001 * sizeof(UINT32);

	AllRam		= Next;

	DrvVidRegs	= Next; Next += 0x000400;
	DrvShareRAM	= Next; Next += 0x000010;
	DrvZ80RAM	= Next; Next += 0x001000;

	Drv68KRAM	= Next; Next += 0x020000;
	DrvPalRAM	= Next; Next += 0x004000;
	DrvFgRAM1	= Next; Next += 0x002000;
	DrvFgRAM2	= Next; Next += 0x020000;
	DrvBgRAM1	= Next; Next += 0x002000;
	DrvBgRAM2	= Next; Next += 0x002000;
	DrvSprRAM	= Next; Next += 0x002000;
	DrvSprBuf0	= Next; Next += 0x002000;
	DrvSprBuf1	= Next; Next += 0x002000;

	priority	= Next; Next += 0x000001 * sizeof(UINT32); // (sizeof(UINT32) for alignment)

	tilebank	= (UINT16*)Next; Next += 0x0001 * sizeof(UINT32);
	tilebank_buf= (UINT16*)Next; Next += 0x0002 * sizeof(UINT32);

	nDrvZ80Bank	= Next; Next += 0x000001 * sizeof(UINT32);

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[ 8]  = { 0x0000000, 0x0000001, 0x0000002, 0x0000003,
			   0x2000000, 0x2000001, 0x2000002, 0x2000003 };
	INT32 XOffs[16]  = { 0x008, 0x00c, 0x000, 0x004, 0x018, 0x01c, 0x010, 0x014,
			   0x028, 0x02c, 0x020, 0x024, 0x038, 0x03c, 0x030, 0x034 };
	INT32 YOffs0[ 8] = { 0x000, 0x020, 0x040, 0x060, 0x080, 0x0a0, 0x0c0, 0x0e0 };
	INT32 YOffs1[16] = { 0x000, 0x040, 0x080, 0x0c0, 0x100, 0x140, 0x180, 0x1c0,
			   0x200, 0x240, 0x280, 0x2c0, 0x300, 0x340, 0x380, 0x3c0 };

	UINT8 *tmp = DrvGfxROM0 + 0x2000000;

	memcpy (tmp, DrvGfxROM1, 0x800000);

	GfxDecode(0x08000, 8, 16, 16, Plane, XOffs, YOffs1, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x800000);

	GfxDecode(0x08000, 8, 16, 16, Plane, XOffs, YOffs1, 0x400, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x200000);

	GfxDecode(0x10000, 4,  8,  8, Plane, XOffs, YOffs0, 0x100, tmp, DrvGfxROM3);

	return 0;
}

static void DrvSpriteExpand()
{
	BurnByteswap(DrvGfxROM0, 0x4000000);
	UINT8 *tmp;

	for (INT32 i = 0x2000000 - 0x80; i >= 0; i -= 0x80)
	{
		tmp = DrvGfxROM0 + i;

		for (INT32 j = 0x7f; j >= 0; j--) {
			INT32 t = tmp[j];
			DrvGfxROM0[((i + j) << 1) + 0] = t >> 4;
			DrvGfxROM0[((i + j) << 1) + 1] = t & 0x0f;
		}
	}
}

static void DrvCalculateTransTab(UINT8 *src, UINT8 *dst, INT32 t, INT32 w, INT32 len)
{
	UINT8 *dptr = dst;
	for (INT32 i = 0; i < len; i+= w)
	{
		INT32 a = 0, b = 0;
		for (INT32 j = 0; j < w; j++) {
			a|=src[i+j]^t;

			if (src[i+j] != t) {
				b++;
			}
		}

		dptr[0] = a ? 0 : 2;
		if (b == w) dptr[0] |= 1;

		dptr++;
	}
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x0000001,	 0, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000000,	 1, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000003,	 2, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000002,	 3, 4)) return 1;

		if (BurnLoadRom(DrvZ80ROM,		 4, 1)) return 1;

		if (BurnLoadRom(DrvSndROM,		 5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000000,	 6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0400000,	 7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000000,	 8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0400000,	 9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x0000000,	10, 1)) return 1;

		if (strcmp(BurnDrvGetTextA(DRV_NAME), "asurabld") == 0) {
			if (BurnLoadRom(DrvGfxROM0 + 0x0400000,	11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0800000,	12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0c00000,	13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1000000,	14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1400000,	15, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1800000,	16, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvGfxROM0 + 0x0000000,	11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0400000,	12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0800000,	13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0c00000,	14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1000000,	15, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1400000,	16, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1800000,	17, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1c00000,	18, 1)) return 1;
		}

		DrvGfxDecode();
		DrvSpriteExpand();

		DrvCalculateTransTab(DrvGfxROM1, DrvTransTab1, 0xff, 0x100, 0x800000);
		DrvCalculateTransTab(DrvGfxROM2, DrvTransTab2, 0xff, 0x100, 0x800000);
		DrvCalculateTransTab(DrvGfxROM3, DrvTransTab3, 0x0f, 0x040, 0x400000);
	}

	SekInit(0, 0x68ec020);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x400000, 0x40ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x410000, 0x41ffff, MAP_RAM);
	SekMapMemory(DrvBgRAM1,		0x500000, 0x501fff, MAP_RAM);
	SekMapMemory(DrvBgRAM2,		0x502000, 0x503fff, MAP_RAM);
	SekMapMemory(DrvFgRAM1,		0x504000, 0x505fff, MAP_RAM);
	SekMapMemory(DrvFgRAM2,		0x506000, 0x507fff, MAP_RAM);
	SekMapMemory(DrvFgRAM2 + 0x2000,0x508000, 0x517fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x600000, 0x601fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x700000, 0x703fff, MAP_RAM);
	SekSetWriteWordHandler(0,	fuuki32_write_word);
	SekSetWriteByteHandler(0,	fuuki32_write_byte);
	SekSetReadWordHandler(0,	fuuki32_read_word);
	SekSetReadByteHandler(0,	fuuki32_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM, 0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM, 0x6000, 0x6fff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0x8000, 0x8000, 0xffff, MAP_ROM);
	ZetSetWriteHandler(fuuki32_sound_write);
	ZetSetReadHandler(fuuki32_sound_read);
	ZetSetOutHandler(fuuki32_sound_out);
	ZetSetInHandler(fuuki32_sound_in);
	ZetClose();

	BurnYMF278BInit(0, DrvSndROM, 0x400000, NULL, DrvSynchroniseStream);
	BurnYMF278BSetRoute(BURN_SND_YMF278B_YMF278B_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYMF278BSetRoute(BURN_SND_YMF278B_YMF278B_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	BurnYMF262Init(14318180, &DrvFMIRQHandler, DrvSynchroniseStream, 1);
	BurnYMF262SetRoute(BURN_SND_YMF262_YMF262_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYMF262SetRoute(BURN_SND_YMF262_YMF262_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachZet(6000000);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYMF262Exit();
	BurnYMF278BExit();
	SekExit();
	ZetExit();

	BurnFreeMemIndex();

	asurablade = 0;
	is_usa = 0;

	return 0;
}

static void draw_sprites()
{
	UINT16 *spriteram16 = (UINT16*)DrvSprBuf1;

	for (INT32 offs = (0x2000 - 8) / 2; offs >=0; offs -= 8 / 2)
	{
		INT32 x, y, xstart, ystart, xend, yend, xinc, yinc;
		INT32 xnum, ynum, xzoom, yzoom, flipx, flipy;
		INT32 pri_mask;

		INT32 sx = BURN_ENDIAN_SWAP_INT16(spriteram16[offs + 0]);
		INT32 sy = BURN_ENDIAN_SWAP_INT16(spriteram16[offs + 1]);
		INT32 attr = BURN_ENDIAN_SWAP_INT16(spriteram16[offs + 2]);
		INT32 code = BURN_ENDIAN_SWAP_INT16(spriteram16[offs + 3]);

		if (sx & 0x400)
			continue;

		INT32 bank = (tilebank_buf[1] >> ((code >> 12) & 0x0c)) & 0xf;

		code = (code & 0x3fff) | (bank << 14);
		INT32 color = ((attr & 0x3f) << 4) + 0x800; // 4bpp, color offset 0x800

		flipx = sx & 0x0800;
		flipy = sy & 0x0800;

		xnum = ((sx >> 12) & 0xf) + 1;
		ynum = ((sy >> 12) & 0xf) + 1;

		xzoom = 16 * 8 - (8 * ((attr >> 12) & 0xf)) / 2;
		yzoom = 16 * 8 - (8 * ((attr >>  8) & 0xf)) / 2;

		switch ((attr >> 6) & 3)
		{
			case 3: pri_mask = 0xf0 | 0xcc | 0xaa;  break;  // behind all layers
			case 2: pri_mask = 0xf0 | 0xcc;         break;  // behind fg + middle layer
			case 1: pri_mask = 0xf0;                break;  // behind fg layer
			case 0:
			default:pri_mask = 0;                           // above all
		}

		sx = (sx & 0x1ff) - (sx & 0x200);
		sy = (sy & 0x1ff) - (sy & 0x200);

		if (flipx)  { xstart = xnum-1;  xend = -1;    xinc = -1; }
		else        { xstart = 0;       xend = xnum;  xinc = +1; }

		if (flipy)  { ystart = ynum-1;  yend = -1;    yinc = -1; }
		else        { ystart = 0;       yend = ynum;  yinc = +1; }

		for (y = ystart; y != yend; y += yinc)
		{
			for (x = xstart; x != xend; x += xinc)
			{
				if (xzoom == (16*8) && yzoom == (16*8)) {
					RenderPrioSprite(pTransDraw, DrvGfxROM0, code++, color, 0xf, sx + x * 16, sy + y * 16, flipx, flipy, 16, 16, pri_mask);
				} else {
					RenderZoomedPrioSprite(pTransDraw, DrvGfxROM0, code++, color, 0xf, sx + (x * xzoom) / 8, sy + (y * yzoom) / 8, flipx, flipy, 16, 16, (0x10000/0x10/8) * (xzoom + 8),(0x10000/0x10/8) * (yzoom + 8), pri_mask);
				}
			}
		}
	}
}

static void draw_background_layer(UINT8 *ram, UINT8 *gfx, UINT8 *tab, INT32 coloff, INT32 soff, INT32 prio)
{
	INT32 yoff    = (DrvScrollBuf[0x300] & 0xffff) - 0x1f3;
	INT32 xoff    = (DrvScrollBuf[0x300] >> 16) - 0x3f6;
	INT32 scrolly = ((DrvScrollBuf[soff*256] & 0xffff) + yoff) & 0x1ff;
	INT32 scrollx = ((DrvScrollBuf[soff*256] >> 16) + xoff) & 0x3ff;

	UINT16 *vram = (UINT16*)ram;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) << 4;
		INT32 sy = (offs >> 6) << 4;

		sx -= scrollx;
		if (sx < -15) sx += 1024;
		sy -= scrolly;
		if (sy < -15) sy +=  512;

		INT32 code = BURN_ENDIAN_SWAP_INT16(vram[offs*2]) & 0x7fff;
		if (tab[code] == 2) continue;
		INT32 attr = BURN_ENDIAN_SWAP_INT16(vram[offs*2 + 1]);

		INT32 color = (attr & 0x30) >> 4;
		INT32 flipx = (attr >> 6) & 1;
		INT32 flipy = (attr >> 7) & 1;

		if (tab[code]) {
			Draw16x16PrioTile(pTransDraw, code, sx, sy, flipx, flipy, color, 8, coloff, prio, gfx);
		} else {
			Draw16x16PrioMaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 8, 0xff, coloff, prio, gfx);
		}
	}
}

static void draw_background_layer_byline(UINT8 *ram, UINT8 *gfx, UINT8 *tab, INT32 coloff, INT32 soff, INT32 prio)
{
	UINT16 *vram = (UINT16*)ram;
	UINT16 *dst = pTransDraw;
	UINT8  *pri = pPrioDraw;

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		INT32 yoff    = (DrvScrollBuf[0x300 + y] & 0xffff) - 0x1f3;
		INT32 xoff    = (DrvScrollBuf[0x300 + y] >> 16) - 0x3f6;
		INT32 scrolly = ((DrvScrollBuf[(soff*256) + y] & 0xffff) + yoff + y) & 0x1ff;
		INT32 scrollx = ((DrvScrollBuf[(soff*256) + y] >> 16) + xoff) & 0x3ff;

		INT32 yy = scrolly & 0x1f0;
		INT32 yo = (scrolly & 0x0f) << 4;
		INT32 xo =  scrollx & 0x0f;

		for (INT32 x = 0; x < nScreenWidth + 16; x+=16)
		{
			INT32 xx = ((scrollx + x) >> 3) & 0x7e;

			INT32 ofst = (yy << 3) | xx;

			INT32 code = BURN_ENDIAN_SWAP_INT16(vram[ofst]) & 0x7fff;
			if (tab[code] == 2) continue;
			INT32 attr = BURN_ENDIAN_SWAP_INT16(vram[ofst + 1]);

			INT32 color = (attr & 0x30) << 4;
			INT32 flipx = ((attr >> 6) & 1) * 0x0f;
			INT32 flipy = ((attr >> 7) & 1) * 0xf0;

			color |= coloff;

			UINT8 *src = gfx + (code << 8) + (yo ^ flipy);

			INT32 xxx = x - xo;

			if (tab[code]) {
				if (xxx >= 0 && xxx < (nScreenWidth - 15)) {
					pri[xxx] |= prio;
					dst[xxx++] = src[ 0 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[ 1 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[ 2 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[ 3 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[ 4 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[ 5 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[ 6 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[ 7 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[ 8 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[ 9 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[10 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[11 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[12 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[13 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[14 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx  ] = src[15 ^ flipx] | color;
				} else {
					for (INT32 sx = 0; sx < 16; sx++, xxx++)
					{
						if (xxx >= 0 && xxx < nScreenWidth) {
							dst[xxx] = src[sx ^ flipx] | color;
							pri[xxx] |= prio;
						}
					}
				}
			} else {
				if (xxx >= 0 && xxx < (nScreenWidth - 15)) {
					for (INT32 sx = 0; sx < 16; sx++, xxx++) {
						INT32 pxl = src[sx ^ flipx];

						if (pxl != 0xff) {
							dst[xxx] = pxl | color;
							pri[xxx] |= prio;
						}
					}
				} else {
					for (INT32 sx = 0; sx < 16; sx++, xxx++) {
						INT32 pxl = src[sx ^ flipx];

						if (xxx >= 0 && xxx < nScreenWidth && pxl != 0xff) {
							dst[xxx] = pxl | color;
							pri[xxx] |= prio;
						}
					}
				}
			}
		}

		dst += nScreenWidth;
		pri += nScreenWidth;
	}
}

static void draw_foreground_layer(UINT8 *ram, INT32 prio)
{
	UINT16 *vram = (UINT16*)ram;

	INT32 scrolly =  (DrvScrollBuf[0x200] + (is_usa ? 8 : 0)) & 0xff;
	INT32 scrollx = ((DrvScrollBuf[0x200] >> 16) + (is_usa ? 16 : 0)) & 0x1ff;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) << 3;
		INT32 sy = (offs >> 6) << 3;

		sx -= scrollx;
		if (sx < -7) sx += 512;
		sy -= scrolly;
		if (sy < -7) sy += 256;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 code = BURN_ENDIAN_SWAP_INT16(vram[offs*2]);
		if (DrvTransTab3[code] == 2) continue;
		INT32 attr = BURN_ENDIAN_SWAP_INT16(vram[offs*2+1]);

		INT32 color = attr & 0x3f;
		INT32 flipx = (attr >> 6) & 1;
		INT32 flipy = (attr >> 7) & 1;

		if (DrvTransTab3[code]) {
			Draw8x8PrioTile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 0xc00, prio, DrvGfxROM3);
		} else {
			Draw8x8PrioMaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 0x0f, 0xc00, prio, DrvGfxROM3);
		}
	}
}

static void draw_foreground_layer_byline(UINT8 *ram, INT32 prio)
{
	UINT16 *vram = (UINT16*)ram;
	UINT16 *dst = pTransDraw;
	UINT8  *pri = pPrioDraw;

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		INT32 scrolly = ((DrvScrollBuf[512 + y] & 0xffff) + y + (is_usa ? 8 : 0)) & 0x0ff;
		INT32 scrollx = ((DrvScrollBuf[512 + y] >> 16) + (is_usa ? 16 : 0)) & 0x1ff;

		INT32 yy = scrolly & 0xf8;
		INT32 yo = (scrolly & 0x07) << 3;
		INT32 xo = scrollx & 0x07;

		for (INT32 x = 0; x < nScreenWidth + 8; x+=8)
		{
			INT32 xx = ((scrollx + x) >> 2) & 0x7e;

			INT32 ofst = (yy << 4) | xx;

			INT32 code = BURN_ENDIAN_SWAP_INT16(vram[ofst]);
			if (DrvTransTab3[code] == 2) continue;
			INT32 attr = BURN_ENDIAN_SWAP_INT16(vram[ofst + 1]);

			INT32 color = attr & 0x3f;
			INT32 flipx = ((attr >> 6) & 1) * 0x07;
			INT32 flipy = ((attr >> 7) & 1) * 0x38;

			color <<= 4;
			color |= 0xc00;

			UINT8 *src = DrvGfxROM3 + (code << 6) + (yo ^ flipy);

			INT32 xxx = x - xo;

			if (DrvTransTab3[code]) {
				if (xxx >= 0 && xxx < (nScreenWidth - 7)) {
					pri[xxx] |= prio;
					dst[xxx++] = src[0 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[1 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[2 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[3 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[4 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[5 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[6 ^ flipx] | color;
					pri[xxx] |= prio;
					dst[xxx++] = src[7 ^ flipx] | color;
				} else {
					for (INT32 sx = 0; sx < 8; sx++, xxx++)
					{
						if (xxx >= 0 && xxx < nScreenWidth) {
							dst[xxx] = src[sx ^ flipx] | color;
							pri[xxx] |= prio;
						}
					}
				}
			} else {
				if (xxx >= 0 && xxx < (nScreenWidth - 7)) {
					for (INT32 sx = 0; sx < 8; sx++, xxx++) {
						INT32 pxl = src[sx ^ flipx];
						if (pxl != 0x0f) {
							dst[xxx] = pxl | color;
							pri[xxx] |= prio;
						}
					}
				} else {
					for (INT32 sx = 0; sx < 8; sx++, xxx++) {
						INT32 pxl = src[sx ^ flipx];
						if (xxx >= 0 && xxx < nScreenWidth && pxl != 0x0f) {
							dst[xxx] = pxl | color;
							pri[xxx] |= prio;
						}
					}
				}
			}
		}

		dst += nScreenWidth;
		pri += nScreenWidth;
	}
}

static void fuuki32_draw_layer(INT32 layer, INT32 buffer, INT32 prio)
{
	switch (layer)
	{
		case 0:
			if (nEnableRaster[0]) {
				draw_background_layer_byline(DrvBgRAM1, DrvGfxROM1, DrvTransTab1, 0x000, 0, prio);
			} else {
				draw_background_layer(DrvBgRAM1, DrvGfxROM1, DrvTransTab1, 0x000, 0, prio);
			}
		return;

		case 1:
			if (nEnableRaster[1]) {
				draw_background_layer_byline(DrvBgRAM2, DrvGfxROM2, DrvTransTab2, 0x400, 1, prio);
			} else {
				draw_background_layer(DrvBgRAM2, DrvGfxROM2, DrvTransTab2, 0x400, 1, prio);
			}
		return;

		case 2:
			if (buffer) {
				if (nEnableRaster[2]) {
					draw_foreground_layer_byline(DrvFgRAM2, prio);
				} else {
					draw_foreground_layer(DrvFgRAM2, prio);
				}
			} else {
				if (nEnableRaster[2]) {
					draw_foreground_layer_byline(DrvFgRAM1, prio);
				} else {
					draw_foreground_layer(DrvFgRAM1, prio);
				}
			}
		return;
	}
}

static void enable_rasters()
{
	nEnableRaster[0] = nEnableRaster[1] = nEnableRaster[2] = 0;

	for (INT32 i = 0; i < nScreenHeight; i++) {
		if (DrvScrollBuf[0x000] != DrvScrollBuf[0x000 + i]) {
			nEnableRaster[0] |= 1;
		}

		if (DrvScrollBuf[0x100] != DrvScrollBuf[0x100 + i]) {
			nEnableRaster[1] |= 1;
		}

		if (DrvScrollBuf[0x200] != DrvScrollBuf[0x200 + i]) {
			nEnableRaster[2] |= 1;
		}

		if (DrvScrollBuf[0x300] != DrvScrollBuf[0x300 + i]) {
			nEnableRaster[0] |= 1;
			nEnableRaster[1] |= 1;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		UINT8 r,g,b;
		UINT16 *p = (UINT16*)DrvPalRAM;
		for (INT32 i = 0; i < 0x2000 / 2; i++) {
			r = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 10) & 0x1f;
			g = (BURN_ENDIAN_SWAP_INT16(p[i]) >>  5) & 0x1f;
			b = (BURN_ENDIAN_SWAP_INT16(p[i]) >>  0) & 0x1f;

			r = (r << 3) | (r >> 2);
			g = (g << 3) | (g >> 2);
			b = (b << 3) | (b >> 2);

			DrvPalette[i] = BurnHighCol(r, g, b, 0);
		}
	}

	UINT32 *vregs = (UINT32*)DrvVidRegs;

	static const INT32 pri_table[6][3] = {
		{ 0, 1, 2 },
		{ 0, 2, 1 },
		{ 1, 0, 2 },
		{ 1, 2, 0 },	// ?
		{ 2, 0, 1 },
		{ 2, 1, 0 }
	};

	INT32 tm_front  = pri_table[ priority[0] ][0];
	INT32 tm_middle = pri_table[ priority[0] ][1];
	INT32 tm_back   = pri_table[ priority[0] ][2];
	INT32 buffer = BURN_ENDIAN_SWAP_INT32(vregs[0x1e/4]) & 0x40;

	BurnTransferClear(0x1fff);

	enable_rasters();

	if (nBurnLayer & 1) fuuki32_draw_layer(tm_back,   buffer, 1);
	if (nBurnLayer & 2) fuuki32_draw_layer(tm_middle, buffer, 2);
	if (nBurnLayer & 4) fuuki32_draw_layer(tm_front,  buffer, 4);

	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xffff;
		DrvInputs[1] = 0xffff;
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
		DrvInputs[2] = 0xff00 | DrvDips[0];
		DrvInputs[3] = (DrvDips[2] << 8) | DrvDips[1];
	}

	SekNewFrame();
	ZetNewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 20000000 / 60, 6000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	UINT32 *vregs = (UINT32*)DrvVidRegs;

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (i == DrvRasterPos[0]) {
			SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
			DrvRasterPos[0] = 0x1000;
		}

		if (i == 248) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO); // level 1
		if (i == 240) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO); // vblank

		CPU_RUN_TIMER(1);

		// hack -- save scroll/offset registers so the
		// lines can be drawn in one pass -- should save
		// quite a few cycles...
		DrvScrollBuf[i + 0x000] = BURN_ENDIAN_SWAP_INT32(vregs[0]);
		DrvScrollBuf[i + 0x100] = BURN_ENDIAN_SWAP_INT32(vregs[1]);
		DrvScrollBuf[i + 0x200] = BURN_ENDIAN_SWAP_INT32(vregs[2]);
		DrvScrollBuf[i + 0x300] = BURN_ENDIAN_SWAP_INT32(vregs[3]);

		if (i == 240) { // Draw @ VBlank
			if (pBurnDraw) DrvDraw();

			memcpy (DrvSprBuf1, DrvSprBuf0, 0x2000);
			memcpy (DrvSprBuf0, DrvSprRAM,  0x2000);
			tilebank_buf[1] = tilebank_buf[0];
			tilebank_buf[0] = tilebank[0];
		}
	}

	if (pBurnSoundOut) {
		BurnYMF278BUpdate(nBurnSoundLen);
		BurnYMF262Update(nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029704;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		BurnYMF278BScan(nAction, pnMin);
		BurnYMF262Scan(nAction, pnMin);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(nDrvZ80Bank[0]);
		ZetClose();
	}

	return 0;
}


// Asura Blade - Sword of Dynasty (Japan)

static struct BurnRomInfo asurabldRomDesc[] = {
	{ "pgm3.u1",		0x080000, 0x053e9758, 1 | BRF_PRG | BRF_ESS }, //  0 68ec020 Code
	{ "pgm2.u2",		0x080000, 0x16b656ca, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pgm1.u3",		0x080000, 0x35104452, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pgm0.u4",		0x080000, 0x68615497, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "srom.u7",		0x080000, 0xbb1deb89, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "pcm.u6",			0x400000, 0xac72225a, 3 | BRF_SND },           //  5 Samples

	{ "bg1113.u23",		0x400000, 0x94338267, 4 | BRF_GRA },           //  6 Background Tiles 0
	{ "bg1012.u22",		0x400000, 0xd717a0a1, 4 | BRF_GRA },           //  7

	{ "bg2123.u24",		0x400000, 0x4acfc469, 5 | BRF_GRA },           //  8 Background Tiles 1
	{ "bg2022.u25",		0x400000, 0xee312cd3, 5 | BRF_GRA },           //  9

	{ "map.u5",			0x200000, 0xe681155e, 6 | BRF_GRA },           // 10 Character Tiles

	{ "sp23.u14",		0x400000, 0x7df492eb, 7 | BRF_GRA },           // 11 Sprite Tiles
	{ "sp45.u15",		0x400000, 0x1890f42a, 7 | BRF_GRA },           // 12
	{ "sp67.u16",		0x400000, 0xa48f1ef0, 7 | BRF_GRA },           // 13
	{ "sp89.u17",		0x400000, 0x6b024362, 7 | BRF_GRA },           // 14
	{ "spab.u18",		0x400000, 0x803d2d8c, 7 | BRF_GRA },           // 15
	{ "spcd.u19",		0x400000, 0x42e5c26e, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(asurabld)
STD_ROM_FN(asurabld)

static INT32 BladeDrvInit()
{
	asurablade = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvAsurabld = {
	"asurabld", NULL, NULL, NULL, "1998",
	"Asura Blade - Sword of Dynasty (Japan)\0", NULL, "Fuuki", "FG-3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, asurabldRomInfo, asurabldRomName, NULL, NULL, NULL, NULL, AsurabldInputInfo, AsurabldDIPInfo,
	BladeDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 240, 4, 3
};

static INT32 USAInit()
{
	is_usa = 1;

	return DrvInit();
}

// Asura Buster - Eternal Warriors (USA)

static struct BurnRomInfo asurabusRomDesc[] = {
	{ "uspgm3.u1",		0x080000, 0xe152cec9, 1 | BRF_PRG | BRF_ESS }, //  0 68ec020 Code
	{ "uspgm2.u2",		0x080000, 0xb19787db, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "uspgm1.u3",		0x080000, 0x6588e51a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "uspgm0.u4",		0x080000, 0x981e6ff1, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "srom.u7",		0x080000, 0x368da389, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "opm.u6",			0x400000, 0x31b05be4, 3 | BRF_SND },           //  5 Samples

	{ "bg1113.u23",		0x400000, 0x5f8657e6, 4 | BRF_GRA },           //  6 Background Tiles 0
	{ "bg1012.u22",		0x400000, 0xe3fb9af0, 4 | BRF_GRA },           //  7

	{ "bg2123.u24",		0x400000, 0xc4ebb86b, 5 | BRF_GRA },           //  8 Background Tiles 1
	{ "bg2022.u25",		0x400000, 0xf46eda52, 5 | BRF_GRA },           //  9

	{ "map.u5",			0x200000, 0xbd179dc5, 6 | BRF_GRA },           // 10 Character Tiles

	{ "sp01.u13",		0x400000, 0x5edea463, 7 | BRF_GRA },           // 11 Sprite Tiles
	{ "sp23.u14",		0x400000, 0x91b1b0de, 7 | BRF_GRA },           // 12
	{ "sp45.u15",		0x400000, 0x96c69aac, 7 | BRF_GRA },           // 13
	{ "sp67.u16",		0x400000, 0x7c3d83bf, 7 | BRF_GRA },           // 14
	{ "sp89.u17",		0x400000, 0xcb1e14f8, 7 | BRF_GRA },           // 15
	{ "spab.u18",		0x400000, 0xe5a4608d, 7 | BRF_GRA },           // 16
	{ "spcd.u19",		0x400000, 0x99bfbe32, 7 | BRF_GRA },           // 17
	{ "spef.u20",		0x400000, 0xc9c799cc, 7 | BRF_GRA },           // 18
};

STD_ROM_PICK(asurabus)
STD_ROM_FN(asurabus)

struct BurnDriver BurnDrvAsurabus = {
	"asurabus", NULL, NULL, NULL, "2000",
	"Asura Buster - Eternal Warriors (USA)\0", NULL, "Fuuki", "FG-3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, asurabusRomInfo, asurabusRomName, NULL, NULL, NULL, NULL, AsurabldInputInfo, AsurabusDIPInfo,
	USAInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 240, 4, 3
};


// Asura Buster - Eternal Warriors (Japan, set 1)

static struct BurnRomInfo asurabusjRomDesc[] = {
	{ "pgm3.u1",		0x080000, 0x2c6b5271, 1 | BRF_PRG | BRF_ESS }, //  0 68ec020 Code
	{ "pgm2.u2",		0x080000, 0x8f8694ec, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pgm1.u3",		0x080000, 0x0a040f0f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pgm0.u4",		0x080000, 0x9b71e9d8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "srom.u7",		0x080000, 0x368da389, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "opm.u6",			0x400000, 0x31b05be4, 3 | BRF_SND },           //  5 Samples

	{ "bg1113.u23",		0x400000, 0x5f8657e6, 4 | BRF_GRA },           //  6 Background Tiles 0
	{ "bg1012.u22",		0x400000, 0xe3fb9af0, 4 | BRF_GRA },           //  7

	{ "bg2123.u24",		0x400000, 0xc4ebb86b, 5 | BRF_GRA },           //  8 Background Tiles 1
	{ "bg2022.u25",		0x400000, 0xf46eda52, 5 | BRF_GRA },           //  9

	{ "map.u5",			0x200000, 0xbd179dc5, 6 | BRF_GRA },           // 10 Character Tiles

	{ "sp01.u13",		0x400000, 0x5edea463, 7 | BRF_GRA },           // 11 Sprite Tiles
	{ "sp23.u14",		0x400000, 0x91b1b0de, 7 | BRF_GRA },           // 12
	{ "sp45.u15",		0x400000, 0x96c69aac, 7 | BRF_GRA },           // 13
	{ "sp67.u16",		0x400000, 0x7c3d83bf, 7 | BRF_GRA },           // 14
	{ "sp89.u17",		0x400000, 0xcb1e14f8, 7 | BRF_GRA },           // 15
	{ "spab.u18",		0x400000, 0xe5a4608d, 7 | BRF_GRA },           // 16
	{ "spcd.u19",		0x400000, 0x99bfbe32, 7 | BRF_GRA },           // 17
	{ "spef.u20",		0x400000, 0xc9c799cc, 7 | BRF_GRA },           // 18
};

STD_ROM_PICK(asurabusj)
STD_ROM_FN(asurabusj)

struct BurnDriver BurnDrvAsurabusj = {
	"asurabusj", "asurabus", NULL, NULL, "2000",
	"Asura Buster - Eternal Warriors (Japan, set 1)\0", NULL, "Fuuki", "FG-3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, asurabusjRomInfo, asurabusjRomName, NULL, NULL, NULL, NULL, AsurabldInputInfo, AsurabusDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 240, 4, 3
};


// Asura Buster - Eternal Warriors (Japan, set 2)

static struct BurnRomInfo asurabusjaRomDesc[] = {
	{ "pgm3_583a.u1",	0x080000, 0x46ab3b0e, 1 | BRF_PRG | BRF_ESS }, //  0 68ec020 Code
	{ "pgm2_0ff4.u2",	0x080000, 0xfa7aa289, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pgm1_bac7.u3",	0x080000, 0x67364e19, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pgm0_193a.u4",	0x080000, 0x94d39c64, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "srom.u7",		0x080000, 0x368da389, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "opm.u6",			0x400000, 0x31b05be4, 3 | BRF_SND },           //  5 Samples

	{ "bg1113.u23",		0x400000, 0x5f8657e6, 4 | BRF_GRA },           //  6 Background Tiles 0
	{ "bg1012.u22",		0x400000, 0xe3fb9af0, 4 | BRF_GRA },           //  7

	{ "bg2123.u24",		0x400000, 0xc4ebb86b, 5 | BRF_GRA },           //  8 Background Tiles 1
	{ "bg2022.u25",		0x400000, 0xf46eda52, 5 | BRF_GRA },           //  9

	{ "map.u5",			0x200000, 0xbd179dc5, 6 | BRF_GRA },           // 10 Character Tiles

	{ "sp01.u13",		0x400000, 0x5edea463, 7 | BRF_GRA },           // 11 Sprite Tiles
	{ "sp23.u14",		0x400000, 0x91b1b0de, 7 | BRF_GRA },           // 12
	{ "sp45.u15",		0x400000, 0x96c69aac, 7 | BRF_GRA },           // 13
	{ "sp67.u16",		0x400000, 0x7c3d83bf, 7 | BRF_GRA },           // 14
	{ "sp89.u17",		0x400000, 0xcb1e14f8, 7 | BRF_GRA },           // 15
	{ "spab.u18",		0x400000, 0xe5a4608d, 7 | BRF_GRA },           // 16
	{ "spcd.u19",		0x400000, 0x99bfbe32, 7 | BRF_GRA },           // 17
	{ "spef.u20",		0x400000, 0xc9c799cc, 7 | BRF_GRA },           // 18
};

STD_ROM_PICK(asurabusja)
STD_ROM_FN(asurabusja)

struct BurnDriver BurnDrvAsurabusja = {
	"asurabusja", "asurabus", NULL, NULL, "2000",
	"Asura Buster - Eternal Warriors (Japan, set 2)\0", NULL, "Fuuki", "FG-3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, asurabusjaRomInfo, asurabusjaRomName, NULL, NULL, NULL, NULL, AsurabldInputInfo, AsurabusDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 240, 4, 3
};


// Asura Buster - Eternal Warriors (Japan) (ARCADIA review build)

static struct BurnRomInfo asurabusjrRomDesc[] = {
	{ "24-31.pgm3",	0x080000, 0xcfcb9c75, 1 | BRF_PRG | BRF_ESS }, //  0 68ec020 Code
	{ "16-23.pgm2",	0x080000, 0xe4d07738, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "8-15.pgm1",	0x080000, 0x1dd67fe7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "0-7.pgm0",	0x080000, 0x3af08de3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "srom.u7",	0x080000, 0x368da389, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "opm.u6",		0x400000, 0x31b05be4, 3 | BRF_SND },           //  5 Samples

	{ "bg1113.u23",	0x400000, 0x5f8657e6, 4 | BRF_GRA },           //  6 Background Tiles 0
	{ "bg1012.u22",	0x400000, 0xe3fb9af0, 4 | BRF_GRA },           //  7

	{ "bg2123.u24",	0x400000, 0xc4ebb86b, 5 | BRF_GRA },           //  8 Background Tiles 1
	{ "bg2022.u25",	0x400000, 0xf46eda52, 5 | BRF_GRA },           //  9

	{ "map.u5",		0x200000, 0xbd179dc5, 6 | BRF_GRA },           // 10 Character Tiles

	{ "sp01.u13",	0x400000, 0x5edea463, 7 | BRF_GRA },           // 11 Sprite Tiles
	{ "sp23.u14",	0x400000, 0x91b1b0de, 7 | BRF_GRA },           // 12
	{ "sp45.u15",	0x400000, 0x96c69aac, 7 | BRF_GRA },           // 13
	{ "sp67.u16",	0x400000, 0x7c3d83bf, 7 | BRF_GRA },           // 14
	{ "sp89.u17",	0x400000, 0xcb1e14f8, 7 | BRF_GRA },           // 15
	{ "spab.u18",	0x400000, 0xe5a4608d, 7 | BRF_GRA },           // 16
	{ "spcd.u19",	0x400000, 0x99bfbe32, 7 | BRF_GRA },           // 17
	{ "spef.u20",	0x400000, 0xc9c799cc, 7 | BRF_GRA },           // 18
};

STD_ROM_PICK(asurabusjr)
STD_ROM_FN(asurabusjr)

struct BurnDriver BurnDrvAsurabusjr = {
	"asurabusjr", "asurabus", NULL, NULL, "2000",
	"Asura Buster - Eternal Warriors (Japan) (ARCADIA review build)\0", NULL, "Fuuki", "FG-3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, asurabusjrRomInfo, asurabusjrRomName, NULL, NULL, NULL, NULL, AsurabusaInputInfo, AsurabusaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 240, 4, 3
};
