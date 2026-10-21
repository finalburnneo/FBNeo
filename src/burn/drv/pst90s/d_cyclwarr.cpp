// Final Burn Neo Cycle Warriors and Tatsumi Big Fight - Big Trouble In The Atlantic Ocean driver module
// Based on MAME driver by Bryan McPhail, Angelo Salese

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "cxd1095.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM[2];
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[7];
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM[3];
static UINT8 *DrvVidRAM[2];
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprCtrlRAM;
static UINT8 *DrvZ80RAM;

static UINT8 soundlatch;
static UINT8 control_data;
static UINT8 last_control;
static UINT16 video_config[4];
static UINT16 tile_banks[4];
static UINT16 mixing_control;

static INT32 road_color_bank;
static INT32 game_select = 0;
static INT32 tile_transpen = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 2"		},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 3"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 2"		},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 3"		},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p3 coin"		},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy4 + 0,	"p3 start"		},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy4 + 7,	"p3 up"			},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy4 + 6,	"p3 down"		},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy4 + 5,	"p3 left"		},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy4 + 4,	"p3 right"		},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"p3 fire 1"		},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy4 + 2,	"p3 fire 2"		},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy4 + 1,	"p3 fire 3"		},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p4 coin"		},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy5 + 0,	"p4 start"		},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy5 + 7,	"p4 up"			},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy5 + 6,	"p4 down"		},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy5 + 5,	"p4 left"		},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy5 + 4,	"p4 right"		},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy5 + 3,	"p4 fire 1"		},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy5 + 2,	"p4 fire 2"		},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy5 + 1,	"p4 fire 3"		},

	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"service"		},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"service2"		},
	{"Service 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"service3"		},
	{"Service 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"service4"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dips A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dips B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dips C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo CyclwarrDIPList[]=
{
	DIP_OFFSET(0x29)
	{0x00, 0xff, 0xff, 0x0d, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x6e, NULL					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Player Select"		},
	{0x00, 0x01, 0x02, 0x02, "Coin Slot"			},
	{0x00, 0x01, 0x02, 0x00, "Select SW"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x04, 0x04, "Off"					},
	{0x00, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Hardware Test Mode"	},
	{0x00, 0x01, 0x08, 0x08, "Off"					},
	{0x00, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x01, 0x01, 0x0f, 0x00, "5 Coins/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x02, "4 Coins/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x05, "3 Coins/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x08, "2 Coins/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x04, "3 Coins/2 Credits"	},
	{0x01, 0x01, 0x0f, 0x01, "4 Coins/3 Credits"	},
	{0x01, 0x01, 0x0f, 0x0f, "1 Coin/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x03, "3 Coins/4 Credits"	},
	{0x01, 0x01, 0x0f, 0x07, "2 Coins/3 Credits"	},
	{0x01, 0x01, 0x0f, 0x0e, "1 Coin/2 Credits"		},
	{0x01, 0x01, 0x0f, 0x06, "2 Coins/5 Credits"	},
	{0x01, 0x01, 0x0f, 0x0d, "1 Coin/3 Credits"		},
	{0x01, 0x01, 0x0f, 0x0c, "1 Coin/4 Credits"		},
	{0x01, 0x01, 0x0f, 0x0b, "1 Coin/5 Credits"		},
	{0x01, 0x01, 0x0f, 0x0a, "1 Coin/6 Credits"		},
	{0x01, 0x01, 0x0f, 0x09, "1 Coin/7 Credits"		},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x01, 0x01, 0xf0, 0x00, "6 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x10, "5 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x20, "4 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x50, "3 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x80, "2 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x40, "3 Coins/2 Credits"	},
	{0x01, 0x01, 0xf0, 0xf0, "1 Coin/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x30, "3 Coins/4 Credits"	},
	{0x01, 0x01, 0xf0, 0x70, "2 Coins/3 Credits"	},
	{0x01, 0x01, 0xf0, 0xe0, "1 Coin/2 Credits"		},
	{0x01, 0x01, 0xf0, 0x60, "2 Coins/5 Credits"	},
	{0x01, 0x01, 0xf0, 0xd0, "1 Coin/3 Credits"		},
	{0x01, 0x01, 0xf0, 0xc0, "1 Coin/4 Credits"		},
	{0x01, 0x01, 0xf0, 0xb0, "1 Coin/5 Credits"		},
	{0x01, 0x01, 0xf0, 0xa0, "1 Coin/6 Credits"		},
	{0x01, 0x01, 0xf0, 0x90, "1 Coin/7 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x02, 0x01, 0x03, 0x00, "5"					},
	{0x02, 0x01, 0x03, 0x01, "3"					},
	{0x02, 0x01, 0x03, 0x02, "2"					},
	{0x02, 0x01, 0x03, 0x03, "1"					},

	{0   , 0xfe, 0   ,    2, "Ticket Dispenser"		},
	{0x02, 0x01, 0x04, 0x04, "10000"				},
	{0x02, 0x01, 0x04, 0x00, "15000"				},

	{0   , 0xfe, 0   ,    3, "Machine Type"			},
	{0x02, 0x01, 0x18, 0x08, "2 Players"			},
	{0x02, 0x01, 0x18, 0x10, "3 Players"			},
	{0x02, 0x01, 0x18, 0x18, "4 Players"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x02, 0x01, 0x60, 0x00, "Very Hard"			},
	{0x02, 0x01, 0x60, 0x20, "Hard"					},
	{0x02, 0x01, 0x60, 0x40, "Easy"					},
	{0x02, 0x01, 0x60, 0x60, "Normal"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x02, 0x01, 0x80, 0x80, "Off"					},
	{0x02, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Cyclwarr)

static struct BurnDIPInfo CyclwarbDIPList[]=
{
	DIP_OFFSET(0x29)
	{0x00, 0xff, 0xff, 0x0d, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x6e, NULL					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Player Select"		},
	{0x00, 0x01, 0x02, 0x02, "Coin Slot"			},
	{0x00, 0x01, 0x02, 0x00, "Select SW"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x04, 0x04, "Off"					},
	{0x00, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x00, 0x01, 0x08, 0x08, "Off"					},
	{0x00, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x01, 0x01, 0x0f, 0x00, "5 Coins/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x02, "4 Coins/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x05, "3 Coins/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x08, "2 Coins/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x04, "3 Coins/2 Credits"	},
	{0x01, 0x01, 0x0f, 0x01, "4 Coins/3 Credits"	},
	{0x01, 0x01, 0x0f, 0x0f, "1 Coin/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x03, "3 Coins/4 Credits"	},
	{0x01, 0x01, 0x0f, 0x07, "2 Coins/3 Credits"	},
	{0x01, 0x01, 0x0f, 0x0e, "1 Coin/2 Credits"		},
	{0x01, 0x01, 0x0f, 0x06, "2 Coins/5 Credits"	},
	{0x01, 0x01, 0x0f, 0x0d, "1 Coin/3 Credits"		},
	{0x01, 0x01, 0x0f, 0x0c, "1 Coin/4 Credits"		},
	{0x01, 0x01, 0x0f, 0x0b, "1 Coin/5 Credits"		},
	{0x01, 0x01, 0x0f, 0x0a, "1 Coin/6 Credits"		},
	{0x01, 0x01, 0x0f, 0x09, "1 Coin/7 Credits"		},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x01, 0x01, 0xf0, 0x00, "6 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x10, "5 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x20, "4 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x50, "3 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x80, "2 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x40, "3 Coins/2 Credits"	},
	{0x01, 0x01, 0xf0, 0xf0, "1 Coin/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x30, "3 Coins/4 Credits"	},
	{0x01, 0x01, 0xf0, 0x70, "2 Coins/3 Credits"	},
	{0x01, 0x01, 0xf0, 0xe0, "1 Coin/2 Credits"		},
	{0x01, 0x01, 0xf0, 0x60, "2 Coins/5 Credits"	},
	{0x01, 0x01, 0xf0, 0xd0, "1 Coin/3 Credits"		},
	{0x01, 0x01, 0xf0, 0xc0, "1 Coin/4 Credits"		},
	{0x01, 0x01, 0xf0, 0xb0, "1 Coin/5 Credits"		},
	{0x01, 0x01, 0xf0, 0xa0, "1 Coin/6 Credits"		},
	{0x01, 0x01, 0xf0, 0x90, "1 Coin/7 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x02, 0x01, 0x03, 0x00, "5"					},
	{0x02, 0x01, 0x03, 0x01, "3"					},
	{0x02, 0x01, 0x03, 0x02, "2"					},
	{0x02, 0x01, 0x03, 0x03, "1"					},

	{0   , 0xfe, 0   ,    2, "Ticket Dispenser"		},
	{0x02, 0x01, 0x04, 0x04, "10000"				},
	{0x02, 0x01, 0x04, 0x00, "15000"				},

	{0   , 0xfe, 0   ,    3, "Machine Type"			},
	{0x02, 0x01, 0x18, 0x08, "2 Players"			},
	{0x02, 0x01, 0x18, 0x10, "3 Players"			},
	{0x02, 0x01, 0x18, 0x18, "4 Players"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x02, 0x01, 0x60, 0x00, "Very Hard"			},
	{0x02, 0x01, 0x60, 0x20, "Hard"					},
	{0x02, 0x01, 0x60, 0x40, "Easy"					},
	{0x02, 0x01, 0x60, 0x60, "Normal"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x02, 0x01, 0x80, 0x80, "Off"					},
	{0x02, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Cyclwarb)

static struct BurnDIPInfo BigfightDIPList[]=
{
	DIP_OFFSET(0x29)
	{0x00, 0xff, 0xff, 0x0d, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x7f, NULL					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Player Select"		},
	{0x00, 0x01, 0x02, 0x02, "Coin Slot"			},
	{0x00, 0x01, 0x02, 0x00, "Select SW"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x04, 0x04, "Off"					},
	{0x00, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Hardware Test Mode"	},
	{0x00, 0x01, 0x08, 0x08, "Off"					},
	{0x00, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x01, 0x01, 0x0f, 0x00, "5 Coins/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x02, "4 Coins/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x05, "3 Coins/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x08, "2 Coins/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x04, "3 Coins/2 Credits"	},
	{0x01, 0x01, 0x0f, 0x01, "4 Coins/3 Credits"	},
	{0x01, 0x01, 0x0f, 0x0f, "1 Coin/1 Credit"		},
	{0x01, 0x01, 0x0f, 0x03, "3 Coins/4 Credits"	},
	{0x01, 0x01, 0x0f, 0x07, "2 Coins/3 Credits"	},
	{0x01, 0x01, 0x0f, 0x0e, "1 Coin/2 Credits"		},
	{0x01, 0x01, 0x0f, 0x06, "2 Coins/5 Credits"	},
	{0x01, 0x01, 0x0f, 0x0d, "1 Coin/3 Credits"		},
	{0x01, 0x01, 0x0f, 0x0c, "1 Coin/4 Credits"		},
	{0x01, 0x01, 0x0f, 0x0b, "1 Coin/5 Credits"		},
	{0x01, 0x01, 0x0f, 0x0a, "1 Coin/6 Credits"		},
	{0x01, 0x01, 0x0f, 0x09, "1 Coin/7 Credits"		},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x01, 0x01, 0xf0, 0x00, "6 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x10, "5 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x20, "4 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x50, "3 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x80, "2 Coins/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x40, "3 Coins/2 Credits"	},
	{0x01, 0x01, 0xf0, 0xf0, "1 Coin/1 Credit"		},
	{0x01, 0x01, 0xf0, 0x30, "3 Coins/4 Credits"	},
	{0x01, 0x01, 0xf0, 0x70, "2 Coins/3 Credits"	},
	{0x01, 0x01, 0xf0, 0xe0, "1 Coin/2 Credits"		},
	{0x01, 0x01, 0xf0, 0x60, "2 Coins/5 Credits"	},
	{0x01, 0x01, 0xf0, 0xd0, "1 Coin/3 Credits"		},
	{0x01, 0x01, 0xf0, 0xc0, "1 Coin/4 Credits"		},
	{0x01, 0x01, 0xf0, 0xb0, "1 Coin/5 Credits"		},
	{0x01, 0x01, 0xf0, 0xa0, "1 Coin/6 Credits"		},
	{0x01, 0x01, 0xf0, 0x90, "1 Coin/7 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x02, 0x01, 0x03, 0x00, "1"					},
	{0x02, 0x01, 0x03, 0x03, "2"					},
	{0x02, 0x01, 0x03, 0x02, "3"					},
	{0x02, 0x01, 0x03, 0x01, "4"					},

	{0   , 0xfe, 0   ,    2, "Ticket Dispenser"		},
	{0x02, 0x01, 0x04, 0x04, "100000"				},
	{0x02, 0x01, 0x04, 0x00, "150000"				},

	{0   , 0xfe, 0   ,    2, "Continue Coin"		},
	{0x02, 0x01, 0x08, 0x08, "Off"					},
	{0x02, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Extend"				},
	{0x02, 0x01, 0x10, 0x10, "100000"				},
	{0x02, 0x01, 0x10, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x02, 0x01, 0x60, 0x00, "Very Hard"			},
	{0x02, 0x01, 0x60, 0x20, "Hard"					},
	{0x02, 0x01, 0x60, 0x40, "Easy"					},
	{0x02, 0x01, 0x60, 0x60, "Normal"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x02, 0x01, 0x80, 0x80, "Off"					},
	{0x02, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Bigfight)

static void __fastcall cyclwarr_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff0) == 0xb9000) {
		cxd1095_write(0, (address / 2) & 7, data);
		return;
	}

	if ((address & 0xffff0) == 0xba000) {
		cxd1095_write(1, (address / 2) & 7, data);
		return;
	}

	switch (address)
	{
		case 0xa2000:
		case 0xa2001:
		case 0xa2002:
		case 0xa2003:
		case 0xa2004:
		case 0xa2005:
		case 0xa2006:
		case 0xa2007:
			video_config[(address / 2) & 3] = data;
		return;

		case 0xa4000:
		case 0xa4001:
			tile_banks[0] = ((data & 0x000f) >>  0) << 10;
			tile_banks[1] = ((data & 0x00f0) >>  4) << 10;
			tile_banks[2] = ((data & 0x0f00) >>  8) << 10;
			tile_banks[3] = ((data & 0xf000) >> 12) << 10;
		return;

		case 0xa6000:
		case 0xa6001:
			mixing_control = data;
		return;

		case 0xbc000:
		case 0xbc001:
			// coin counters
		return;
	}
}

static void __fastcall cyclwarr_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffff1) == 0xb9001) {
		cxd1095_write(0, (address / 2) & 7, data);
		return;
	}

	if ((address & 0xffff1) == 0xba001) {
		cxd1095_write(1, (address / 2) & 7, data);
		return;
	}

	switch (address)
	{
		case 0xa6001:
			mixing_control = data;
		return;

		case 0xac000:
		case 0xac001:
		case 0xac002:
		case 0xac003:
			// hd6445 crt
		return;

		case 0xb8000:
			soundlatch = data;
			ZetNmi();
		return;
	}
}

static UINT16 __fastcall cyclwarr_main_read_word(UINT32 address)
{
	if ((address & 0xffff0) == 0xb9000) {
		return cxd1095_read(0, (address / 2) & 7);
	}

	if ((address & 0xffff0) == 0xba000) {
		return cxd1095_read(1, (address / 2) & 7);
	}

	return 0;
}

static UINT8 __fastcall cyclwarr_main_read_byte(UINT32 address)
{
	if ((address & 0xffff1) == 0xb9001) {
		return cxd1095_read(0, (address / 2) & 7);
	}

	if ((address & 0xffff1) == 0xba001) {
		return cxd1095_read(1, (address / 2) & 7);
	}

	return 0;
}

static void __fastcall cyclwarr_sound_write(UINT16 address, UINT8 data)
{
	if (address >= 0xe000) {
		DrvZ80RAM[address & 0x1fff] = data;
	}

	switch (address)
	{
		case 0xfff0:
		case 0xfff1:
			BurnYM2151Write(address & 1, data);
		return;

		case 0xfff4:
			MSM6295Write(0, data);
		return;
	}
}

// The ym2151 core in FBNeo does not support the YM2151 status bit
// Implement hack to bypass this (dink)
static bool check_ym2151_pc()
{
	const UINT16 pcs[] = { 0x1c65/*bigfight*/, 0x1b96/*bigfight*/, 0x2aca/*cyclwarr*/, 0 };

	// uncomment below, when soundcpu hangs, add last PC printed on debug
	// screen to the list above.
	// bprintf(0, _T("pc %x\n"), ZetGetPC(-1));

	for (int count = 0; pcs[count] != 0; count++) {
		if (pcs[count] == ZetGetPC(-1)) {
			return true;
		}
	}

	return false;
}

static UINT8 __fastcall cyclwarr_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xfff0:
			return 0xff;

		case 0xfff1:
			return check_ym2151_pc() ? 0x80 : BurnYM2151Read();

		case 0xfff4:
			return MSM6295Read(0) ^ 0xff;

		case 0xfffc:
			return soundlatch;
	}

	if (address >= 0xe000) {
		return DrvZ80RAM[address & 0x1fff];
	}

	return 0;
}

static void cxd1095_1_write(UINT8 offset, UINT8 data)
{
	if (offset == 4) // e
	{
		if ((data & 4) != (last_control & 4))
		{
			SekSetHALT(1, (data & 4) ? 1 : 0);
		}

		last_control = data;
	}
}

static UINT8 cxd1095_0_read(UINT8 offset)
{
	switch (offset)
	{
		case 1: // b - service
			return DrvInputs[0];

		case 2: // c - p1
			return DrvInputs[1];

		case 3: // d - p2
			return DrvInputs[2];

		case 4: // e - dsw3 (player select / service / test)
			return DrvDips[0];
	}

	return 0xff;
}

static UINT8 cxd1095_1_read(UINT8 offset)
{
	switch (offset)
	{
		case 0: // a dsw1 (coin a / coin b)
			return DrvDips[1];

		case 1: // b dsw2 (lives, tickets, machine type, difficulty, demo sounds)
			return DrvDips[2];

		case 2: // c - p3
			return DrvInputs[3];

		case 3: // d - p4
			return DrvInputs[4];
	}

	return 0xff;
}

static tilemap_callback( layer0 )
{
	INT32 attr = *((UINT16*)(DrvVidRAM[0] + offs * 2));

	INT32 code = (attr & 0x3ff) | tile_banks[(attr >> 10) & 3];

	TILE_SET_INFO(0, code, attr >> 12, ((attr & 0x4000) ? TILE_OPAQUE : 0) | TILE_GROUP(attr >> 15));
}

static tilemap_callback( layer1 )
{
	INT32 attr = *((UINT16*)(DrvVidRAM[0] + offs * 2));

	INT32 code = (attr & 0x3ff) | tile_banks[(attr >> 10) & 3];

	TILE_SET_INFO(1, code, (attr >> 12) | road_color_bank, ((attr & 0x4000) ? TILE_OPAQUE : 0) | TILE_GROUP(attr >> 15));
}

static tilemap_callback( layer2 )
{
	INT32 attr = *((UINT16*)(DrvVidRAM[1] + offs * 2));

	INT32 code = (attr & 0x3ff) | tile_banks[(attr >> 10) & 3];

	TILE_SET_INFO(1, code, attr >> 12, ((attr & 0x4000) ? TILE_OPAQUE : 0) | TILE_GROUP(attr >> 15));
}

static tilemap_callback( layer3 )
{
	INT32 attr = *((UINT16*)(DrvVidRAM[1] + offs * 2));

	INT32 code = (attr & 0x3ff) | tile_banks[(attr >> 10) & 3];

	TILE_SET_INFO(1, code, attr >> 12, ((attr & 0x4000) ? TILE_OPAQUE : 0) | TILE_GROUP(attr >> 15));
}

static void DrvYM2151IrqHandler(INT32 nState)
{
	ZetSetIRQLine(0, (nState) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	memcpy (Drv68KRAM[0], Drv68KROM[0], 0x100); // vectors
	memcpy (Drv68KRAM[1], Drv68KROM[1], 0x100); // vectors

	SekOpen(0);
	SekReset();
	SekClose();

	SekOpen(1);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	cxd1095Reset();

	MSM6295Reset(0);
	BurnYM2151Reset();

	video_config[0] = 0;
	video_config[1] = 0;
	video_config[2] = 0;
	video_config[3] = 0;
	control_data = 0;
	last_control = 0;
	tile_banks[0] = 0;
	tile_banks[0] = 1;
	tile_banks[0] = 2;
	tile_banks[0] = 3;
	mixing_control = 0;
	soundlatch = 0;
	road_color_bank = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM[0]	= Next; Next += 0x200000;
	Drv68KROM[1]	= Next; Next += 0x100000;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM[0]	= Next; Next += 0x200000;
	DrvGfxROM[1]	= Next; Next += 0x200000;
	DrvGfxROM[2]	= Next; Next += 0x020000;
	DrvGfxROM[3]	= Next; Next += 0x100000;
	DrvGfxROM[4]	= Next; Next += 0x400000;
	DrvGfxROM[5]	= Next; Next += 0x400000;
	DrvGfxROM[6]	= Next; Next += 0x2000;

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x040000;

	BurnPalette		= (UINT32*)Next; Next += 0x2000 * 4 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM[0]	= Next; Next += 0x010000;
	Drv68KRAM[1]	= Next; Next += 0x010000;
	Drv68KRAM[2]	= Next; Next += 0x001000;
	BurnPalRAM		= Next; Next += 0x004000;
	DrvVidRAM[0]	= Next; Next += 0x010000;
	DrvVidRAM[1]	= Next; Next += 0x010000;
	DrvSprRAM		= Next; Next += 0x004000;
	DrvSprCtrlRAM	= Next; Next += 0x000400;

	DrvZ80RAM		= Next; Next += 0x002000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void tile_expand()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	UINT8 *dest = tmp;

	for (INT32 c = 0; c < 0x4000; c++)
	{
		const UINT8 *c0base = DrvGfxROM[3] + c * 8 * 8;

		for (INT32 y = 0; y < 8; y++)
		{
			const UINT8 *c0 = c0base;

			for (INT32 x = 0; x < 8; x++)
			{
				UINT8 pix = (*c0++ & 7);
				UINT8 respix = DrvGfxROM[2][(c << 3) | pix];
				INT32 transp = ((pix&0x7)!=0 || ((pix&0x7)==0 && (respix&0x7)!=0)) ? (0x80 >> (x & 7)) : 0;
				if (transp == 0) respix = tile_transpen; // hack - color 0x1b is unused in bigfight, use it as a transparent color
				*dest++ = respix;
			}

			c0base += 8;
		}
	}

	memcpy (DrvGfxROM[3], tmp, 0x100000);
	BurnFree (tmp);
}

static void DrvGfxDecode()
{
	INT32 Planes[3] = { 0x20000 * 8 * 2, 0x20000 * 8 * 1, 0x20000 * 8 * 0 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };
	INT32 SprPlanes[4] = { STEP4(0,1) };
	INT32 SprXOffs[8] = { 8,12,0,4, 24, 28, 16,20 };
	INT32 SprYOffs[8] = { STEP8(0,32) };

	UINT8 *tmp = (UINT8 *)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM[0], 0x200000);

	GfxDecode(0x10000, 4, 8, 8, SprPlanes, SprXOffs, SprYOffs, 0x100, tmp, DrvGfxROM[4]);

	memcpy (tmp, DrvGfxROM[1], 0x200000);

	GfxDecode(0x10000, 4, 8, 8, SprPlanes, SprXOffs, SprYOffs, 0x100, tmp, DrvGfxROM[5]);

	memcpy (tmp, DrvGfxROM[3], 0x60000);

	GfxDecode(0x04000, 3, 8, 8, Planes, XOffs, YOffs, 0x040, tmp, DrvGfxROM[3]);

	BurnFree (tmp);
}

static void init68k(INT32 cpu)
{
	SekInit(cpu, 0x68000);
	SekOpen(cpu);
	SekMapMemory(Drv68KRAM[cpu],		0x000000, 0x00ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM[2],			0x03e000, 0x03efff, MAP_RAM);
	SekMapMemory(Drv68KRAM[cpu ^ 1],	0x040000, 0x04ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM[1],			0x080000, 0x08ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM[0],			0x090000, 0x09ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM,				0x0c0000, 0x0c3fff, MAP_RAM);
	SekMapMemory(DrvSprCtrlRAM,			0x0ca000, 0x0ca1ff | 0x3ff, MAP_RAM);
	SekMapMemory(BurnPalRAM,			0x0d0000, 0x0d3fff, MAP_RAM);
	SekMapMemory(Drv68KROM[1],			0x100000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KROM[0],			0x200000, 0x3fffff, MAP_ROM);
	SekSetWriteWordHandler(0,			cyclwarr_main_write_word);
	SekSetWriteByteHandler(0,			cyclwarr_main_write_byte);
	SekSetReadWordHandler(0,			cyclwarr_main_read_word);
	SekSetReadByteHandler(0,			cyclwarr_main_read_byte);
	SekClose();
}

static INT32 DrvInit(INT32 whichgame, INT32 (*pRomCallback)())
{
	BurnSetRefreshRate(57.44);
	game_select = whichgame;
	tile_transpen = game_select ? 0x1b : 0x3b; // transparent color hack

	BurnAllocMemIndex();

	if (pRomCallback) {
		if (pRomCallback()) return 1;

		DrvGfxDecode();
		tile_expand();
	}

	init68k(0);
	init68k(1);

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0xdfff, MAP_ROM);
//	ZetMapMemory(DrvZ80RAM,				0xe000, 0xffff, MAP_RAM); // handler
	ZetSetWriteHandler(cyclwarr_sound_write);
	ZetSetReadHandler(cyclwarr_sound_read);
	ZetClose();

	cxd1095Init(0, NULL, cxd1095_0_read);
	cxd1095Init(1, cxd1095_1_write, cxd1095_1_read);

	MSM6295Init(0, 2000000 / ((whichgame == 1) ? 2 : 1) / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.65, BURN_SND_ROUTE_BOTH);

	BurnYM2151InitBuffered(4000000, 1, NULL, 0);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetAllRoutes(0.50, BURN_SND_ROUTE_BOTH);
	BurnTimerAttachZet(4000000);

	GenericTilesInit();
	GenericTilesSetGfx(0, DrvGfxROM[3], 8, 8, 8, 0x100000, 0x000, 0x3);
	GenericTilesSetGfx(1, DrvGfxROM[3], 8, 8, 8, 0x100000, 0x400, 0x3);
	if (game_select == 0)
	{
		GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 8, 8,  64, 512);
		GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 8, 8, 128, 256);
		GenericTilemapInit(2, TILEMAP_SCAN_ROWS, layer2_map_callback, 8, 8,  64, 512);
		GenericTilemapInit(3, TILEMAP_SCAN_ROWS, layer3_map_callback, 8, 8,  64, 512);
	}
	else
	{
		GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 8, 8, 128, 256);
		GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 8, 8, 128, 256);
		GenericTilemapInit(2, TILEMAP_SCAN_ROWS, layer2_map_callback, 8, 8, 128, 256);
		GenericTilemapInit(3, TILEMAP_SCAN_ROWS, layer3_map_callback, 8, 8, 128, 256);
	}
	GenericTilemapSetTransparent(0, tile_transpen);
	GenericTilemapSetTransparent(1, tile_transpen);
	GenericTilemapSetTransparent(2, tile_transpen);
	GenericTilemapSetTransparent(3, tile_transpen);

	DrvDoReset();

	return 0;
}

static INT32 CyclwarrLoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM[0] + 0x000001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[0] + 0x000000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[0] + 0x100001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[0] + 0x100000, k++, 2)) return 1;
	memcpy (Drv68KROM[0] + 0x040000, Drv68KROM[0] + 0x000000, 0x040000);
	memcpy (Drv68KROM[0] + 0x080000, Drv68KROM[0] + 0x000000, 0x040000);
	memcpy (Drv68KROM[0] + 0x0c0000, Drv68KROM[0] + 0x000000, 0x040000);
	memcpy (Drv68KROM[0] + 0x140000, Drv68KROM[0] + 0x100000, 0x040000);
	memcpy (Drv68KROM[0] + 0x180000, Drv68KROM[0] + 0x100000, 0x040000);
	memcpy (Drv68KROM[0] + 0x1c0000, Drv68KROM[0] + 0x100000, 0x040000);

	if (BurnLoadRom(Drv68KROM[1] + 0x000001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[1] + 0x000000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[1] + 0x080001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[1] + 0x080000, k++, 2)) return 1;
	memcpy (Drv68KROM[1] + 0x040000, Drv68KROM[1] + 0x000000, 0x040000);
	memcpy (Drv68KROM[1] + 0x0c0000, Drv68KROM[1] + 0x080000, 0x040000);

	if (BurnLoadRom(DrvZ80ROM    + 0x000000, k++, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x000001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x000002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x000003, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x080000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x080001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x080002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x080003, k++, 4)) return 1;
	memcpy (DrvGfxROM[0] + 0x100000, DrvGfxROM[0] + 0x000000, 0x100000);

	if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x000001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x000002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x000003, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x080000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x080001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x080002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x080003, k++, 4)) return 1;
	memcpy (DrvGfxROM[1] + 0x100000, DrvGfxROM[1] + 0x000000, 0x100000);

	if (BurnLoadRom(DrvGfxROM[2] + 0x000000, k++, 1)) return 1;
		
	if (BurnLoadRom(DrvGfxROM[3] + 0x000000, k++, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[3] + 0x020000, k++, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[3] + 0x040000, k++, 1)) return 1;

	if (BurnLoadRom(DrvSndROM    + 0x000000, k++, 1)) return 1;
	if (BurnLoadRom(DrvSndROM    + 0x020000, k++, 1)) return 1;

	return 0;
}

static INT32 CyclwarrInit()
{
	return DrvInit(0, CyclwarrLoadRoms);
}

static INT32 BigfightLoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM[0] + 0x000001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[0] + 0x000000, k++, 2)) return 1;
	memcpy (Drv68KROM[0] + 0x080000, Drv68KROM[0] + 0x000000, 0x080000);
	memcpy (Drv68KROM[0] + 0x180000, Drv68KROM[0] + 0x000000, 0x080000);
	memcpy (Drv68KROM[0] + 0x200000, Drv68KROM[0] + 0x000000, 0x080000);
	memcpy (Drv68KROM[0] + 0x280000, Drv68KROM[0] + 0x000000, 0x080000);

	if (BurnLoadRom(Drv68KROM[1] + 0x000001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[1] + 0x000000, k++, 2)) return 1;
	memcpy (Drv68KROM[1] + 0x080000, Drv68KROM[1] + 0x000000, 0x080000);

	if (BurnLoadRom(DrvZ80ROM    + 0x000000, k++, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x000001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x000002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x000003, k++, 4)) return 1;

	if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x000001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x000002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x000003, k++, 4)) return 1;

	if (BurnLoadRom(DrvGfxROM[2] + 0x000000, k++, 1)) return 1;
		
	if (BurnLoadRom(DrvGfxROM[3] + 0x000000, k++, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[3] + 0x020000, k++, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[3] + 0x040000, k++, 1)) return 1;

	if (BurnLoadRom(DrvSndROM    + 0x000000, k++, 1)) return 1;

	return 0;
}

static INT32 BigfightInit()
{
	return DrvInit(1, BigfightLoadRoms);
}

static INT32 BigfightjLoadRoms()
{
	INT32 k = 0;
	if (BurnLoadRom(Drv68KROM[0] + 0x000001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[0] + 0x000000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[0] + 0x100001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[0] + 0x100000, k++, 2)) return 1;
	memcpy (Drv68KROM[0] + 0x040000, Drv68KROM[0] + 0x000000, 0x040000);
	memcpy (Drv68KROM[0] + 0x080000, Drv68KROM[0] + 0x000000, 0x040000);
	memcpy (Drv68KROM[0] + 0x0c0000, Drv68KROM[0] + 0x000000, 0x040000);
	memcpy (Drv68KROM[0] + 0x140000, Drv68KROM[0] + 0x100000, 0x040000);
	memcpy (Drv68KROM[0] + 0x180000, Drv68KROM[0] + 0x100000, 0x040000);
	memcpy (Drv68KROM[0] + 0x1c0000, Drv68KROM[0] + 0x100000, 0x040000);

	if (BurnLoadRom(Drv68KROM[1] + 0x000001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[1] + 0x000000, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[1] + 0x080001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[1] + 0x080000, k++, 2)) return 1;
	memcpy (Drv68KROM[1] + 0x040000, Drv68KROM[1] + 0x000000, 0x040000);
	memcpy (Drv68KROM[1] + 0x0c0000, Drv68KROM[1] + 0x080000, 0x040000);

	if (BurnLoadRom(DrvZ80ROM    + 0x000000, k++, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x000001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x000002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x000003, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x100000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x100001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x100002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + 0x100003, k++, 4)) return 1;
	memcpy (DrvGfxROM[0] + 0x180000, DrvGfxROM[0] + 0x100000, 0x080000);

	if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x000001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x000002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x000003, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x100000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x100001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x100002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + 0x100003, k++, 4)) return 1;
	memcpy (DrvGfxROM[1] + 0x180000, DrvGfxROM[1] + 0x100000, 0x080000);

	if (BurnLoadRom(DrvGfxROM[2] + 0x000000, k++, 1)) return 1;
		
	if (BurnLoadRom(DrvGfxROM[3] + 0x000000, k++, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[3] + 0x020000, k++, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[3] + 0x040000, k++, 1)) return 1;

	if (BurnLoadRom(DrvSndROM    + 0x000000, k++, 1)) return 1;
	if (BurnLoadRom(DrvSndROM    + 0x020000, k++, 1)) return 1;

	return 0;
}

static INT32 BigfightjInit()
{
	return DrvInit(1, BigfightjLoadRoms);
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM2151Exit();
	MSM6295Exit(0);
	SekExit();
	ZetExit();

	BurnFreeMemIndex();

	MSM6295ROM = NULL;
	game_select = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)BurnPalRAM;

	for (INT32 i = 0; i < 0x2000; i++)
	{
		INT32 r = pal5bit((p[i] >> 10) & 0x1f);
		INT32 g = pal5bit((p[i] >>  5) & 0x1f);
		INT32 b = pal5bit((p[i] >>  0) & 0x1f);

		BurnPalette[i] = BurnHighCol(r,g,b,0);
		BurnPalette[i + 0x2000] = BurnHighCol(r >> 1, g >> 1, b >> 1, 0);
	}

	UINT8 *bank1 = DrvGfxROM[0] + 0x200000 - 0x1000;
	UINT8 *bank2 = DrvGfxROM[1] + 0x200000 - 0x1000;

	for (int i = 0; i < 0x2000; i+=8)
	{
		BurnPalette[0x4000 + i + 0] = BurnPalette[0x1000 + bank1[1]];
		BurnPalette[0x4000 + i + 1] = BurnPalette[0x1000 + bank1[0]];
		BurnPalette[0x4000 + i + 2] = BurnPalette[0x1000 + bank1[3]];
		BurnPalette[0x4000 + i + 3] = BurnPalette[0x1000 + bank1[2]];
		BurnPalette[0x4000 + i + 4] = BurnPalette[0x1000 + bank2[1]];
		BurnPalette[0x4000 + i + 5] = BurnPalette[0x1000 + bank2[0]];
		BurnPalette[0x4000 + i + 6] = BurnPalette[0x1000 + bank2[3]];
		BurnPalette[0x4000 + i + 7] = BurnPalette[0x1000 + bank2[2]];
		BurnPalette[0x6000 + i + 0] = BurnPalette[0x3000 + bank1[1]];
		BurnPalette[0x6000 + i + 1] = BurnPalette[0x3000 + bank1[0]];
		BurnPalette[0x6000 + i + 2] = BurnPalette[0x3000 + bank1[3]];
		BurnPalette[0x6000 + i + 3] = BurnPalette[0x3000 + bank1[2]];
		BurnPalette[0x6000 + i + 4] = BurnPalette[0x3000 + bank2[1]];
		BurnPalette[0x6000 + i + 5] = BurnPalette[0x3000 + bank2[0]];
		BurnPalette[0x6000 + i + 6] = BurnPalette[0x3000 + bank2[3]];
		BurnPalette[0x6000 + i + 7] = BurnPalette[0x3000 + bank2[2]];

		DrvGfxROM[6][i+0] = (bank1[1] == 0xff);
		DrvGfxROM[6][i+1] = (bank1[0] == 0xff);
		DrvGfxROM[6][i+2] = (bank1[3] == 0xff);
		DrvGfxROM[6][i+3] = (bank1[2] == 0xff);
		DrvGfxROM[6][i+4] = (bank2[1] == 0xff);
		DrvGfxROM[6][i+5] = (bank2[0] == 0xff);
		DrvGfxROM[6][i+6] = (bank2[3] == 0xff);
		DrvGfxROM[6][i+7] = (bank2[2] == 0xff);

		bank1+=4;
		bank2+=4;
	}
}

static inline void sprite_unrotate(INT32 slope, INT64 shrink, INT64 px, INT64 py, INT64 *ox, INT64 *oy)
{
	// screen position relative to origin (16 fractional bits) -> position in scaled sprite (40 fractional bits)
	*ox = (px + ((slope * py) >> 8)) * shrink;
	*oy = (py - ((slope * px) >> 8)) * shrink;
}

static void draw_sprite(INT32 index, INT32 color, INT32 x, INT32 y, INT32 scale, INT32 rotate, INT32 flip_x, INT32 flip_y, INT32 write_priority_only)
{
	const UINT8 *src1 = DrvGfxROM[0] + (index * 4);
	const UINT8 *src2 = DrvGfxROM[1] + (index * 4);
	const INT32 repeat = (src1[3] & 0x80) ? 1 : 0;

	const INT32 top = src1[0] & 0xf8;
	const INT32 lines = src1[2] | ((src1[3] & 0x01) << 8);
	const INT32 rows = (lines - top + 7) / 8;
	if (!scale || rows <= 0)
		return;
	const INT32 bottom = top + rows * 8;

	// one run of tiles per row of 8 lines, taken from alternate banks
	INT32 run_left[64], run_right[64], run_base[64];
	INT32 width = 0;
	for (INT32 h = 0; h < rows; h++)
	{
		const UINT8 *entry = (h & 1) ? (src1 + 4 + (h / 2) * 4) : (src2 + (h / 2) * 4);
		run_left[h] = entry[1] * 8;
		run_right[h] = run_left[h] + (entry[0] + 1) * 8;
		run_base[h] = (entry[2] | (entry[3] << 8)) * 2;
		if (run_right[h] > width) width = run_right[h];
	}

	// slope t in 8 fractional bits, and 1 / (1 + t^2) in 24 fractional bits
	INT32 slope = (rotate & 0x100) ? (rotate - 0x200) : rotate; // sign-extend 9 bits
	if (flip_x) slope = -slope;
	const INT64 shrink = ((INT64)1 << 40) / (65536 + slope * slope);

	// size of the scaled sprite. scale 0x80 is 1:1
	const INT32 scaled_top = (top * scale) >> 7;
	const INT32 scaled_bottom = (bottom * scale + 127) >> 7;
	const INT32 scaled_left = 0;
	INT32 scaled_right = (width * scale + 127) >> 7;

	if (repeat)
	{
		// repeated lines run on to the edge of the screen, so find how far that is in the sprite
		const INT32 cxs[2] = { 0, nScreenWidth };
		const INT32 cys[2] = { 0, nScreenHeight };
		for (INT32 i = 0; i < 2; i++)
		{
			for (INT32 j = 0; j < 2; j++)
			{
				INT64 zx, zy;
				sprite_unrotate(slope, shrink, (INT64)(cxs[i] - x) << 16, (INT64)(cys[j] - y) << 16, &zx, &zy);
				INT32 r = (INT32)(-((flip_x ? zx : -zx) >> 40)) + 1;
				if (r > scaled_right) scaled_right = r;
			}
		}
	}

	// screen bounds: rotate the corners of the scaled sprite (8 fractional bits)
	INT64 minx = 0, maxx = 0, miny = 0, maxy = 0;
	for (INT32 corner = 0; corner < 4; corner++)
	{
		const INT32 cx = (corner & 1) ? scaled_right : scaled_left;
		const INT32 cy = (corner & 2) ? scaled_bottom : scaled_top;
		const INT32 lx = flip_x ? -cx : cx;
		const INT32 ly = flip_y ? -cy : cy;
		const INT64 rx = ((INT64)lx << 8) - (INT64)slope * ly;
		const INT64 ry = (INT64)slope * lx + ((INT64)ly << 8);
		if (corner == 0 || rx < minx) minx = rx;
		if (corner == 0 || rx > maxx) maxx = rx;
		if (corner == 0 || ry < miny) miny = ry;
		if (corner == 0 || ry > maxy) maxy = ry;
	}

	INT32 left   = x + (INT32)(minx >> 8) - 1;
	INT32 right  = x + (INT32)(-((-maxx) >> 8)) + 1;
	INT32 topy   = y + (INT32)(miny >> 8) - 1;
	INT32 bottomy= y + (INT32)(-((-maxy) >> 8)) + 1;

	if (left < 0) left = 0;
	if (topy < 0) topy = 0;
	if (right > nScreenWidth - 1) right = nScreenWidth - 1;
	if (bottomy > nScreenHeight - 1) bottomy = nScreenHeight - 1;
	if (left > right || topy > bottomy)
		return;

	const UINT16 pal = 0x4000 + ((color & 0x1ff) * 16);
	const UINT16 shadowpal = 0x2000 + ((color & 0x1ff) * 16); // ?
	const UINT8 *shadowpens = DrvGfxROM[6] + ((color & 0x1ff) * 16);

	// moving one pixel along a scanline moves the same amount through the scaled sprite each time
	const INT64 step_x = shrink << 16;
	const INT64 step_y = -((INT64)slope << 8) * shrink;

	for (INT32 py = topy; py <= bottomy; py++)
	{
		// pixel centres
		INT64 sprite_x, sprite_y;
		sprite_unrotate(slope, shrink, ((INT64)(left - x) << 16) + 0x8000, ((INT64)(py - y) << 16) + 0x8000, &sprite_x, &sprite_y);

		UINT16 *dest = pTransDraw + py * nScreenWidth;
		INT32 last_dist_y = -1, line = 0, h = 0, line_ok = 0;

		for (INT32 px = left; px <= right; px++, sprite_x += step_x, sprite_y += step_y)
		{
			// distance from the origin in the scaled sprite, which grows away from it whichever way it is flipped
			const INT32 dist_x = flip_x ? (INT32)(-(sprite_x >> 40) - 1) : (INT32)(sprite_x >> 40);
			const INT32 dist_y = flip_y ? (INT32)(-(sprite_y >> 40) - 1) : (INT32)(sprite_y >> 40);
			if (dist_x < 0 || dist_y < 0)
				continue;

			// back through the scaling to the tile pixel
			if (dist_y != last_dist_y)
			{
				last_dist_y = dist_y;
				line = (dist_y << 7) / scale;
				line_ok = (line >= top && line < bottom);
				h = (line - top) >> 3;
			}
			if (!line_ok)
				continue;

			INT32 col = (dist_x << 7) / scale;
			if (col < run_left[h])
				continue;
			if (col >= run_right[h])
			{
				if (!repeat)
					continue;
				col = run_left[h] + (col - run_left[h]) % (run_right[h] - run_left[h]);
			}

			const INT32 tile = run_base[h] + ((col - run_left[h]) >> 3);
			const UINT8 *gfx = DrvGfxROM[4 + (tile & 1)] + (((tile >> 1) & 0xffff) * 8 * 8);
			const INT32 c = gfx[((line & 7) * 8) + (col & 7)];

			if (c)
			{
				// only draw shadow pens if writing priority buffer
				if (write_priority_only)
					dest[px] = shadowpal | c;
				else if (!shadowpens[c])
					dest[px] = pal | c;
				else
					dest[px] |= 0x2000;
			}
		}
	}
}

static void draw_sprites(INT32 write_priority_only)
{
	INT32 rambank = *((UINT16*)(DrvSprCtrlRAM + 0x1c0)) & 0x1000;
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	// sprite data is double buffered
	for (INT32 offs = rambank; offs < rambank + 0x800; offs += 6)
	{
		INT32 index  = spriteram[offs];

		// end of sprite list marker
		if (index == 0xffff || spriteram[offs + 4] == 0xffff) // todo
			return;

		if (index >= 0x4000)
			continue;

		INT32 y      = (INT16)spriteram[offs+3];
		INT32 x      = (INT16)spriteram[offs+2];
		INT32 scale  = spriteram[offs+4] & 0x1ff;
		INT32 color  = spriteram[offs+1] >> 3 & 0x1ff;
		INT32 flip_x = spriteram[offs+1] & 0x8000;
		INT32 flip_y = spriteram[offs+1] & 0x4000;
		INT32 rotate = spriteram[offs+5] & 0x1ff;

		draw_sprite(index, color, x, y, scale, rotate, flip_x, flip_y, write_priority_only);
	}
}

static void apply_shadow_bitmap()
{
	if (mixing_control & 0x01)
	{
		UINT8 *pri = BurnBitmapGetPriomap(0);

		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++)
		{
			if (pri[i] ^ 0x01)
			{
				pTransDraw[i] |= 0x2000;
			}
		}
	}
}

static void draw_bg(INT32 src, UINT16* scrollx, UINT16* scrolly, INT32 hi_priority)
{
	INT32 rowscroll_enable = (~scrollx[0] & 0x1000);
	INT32 colscroll_enable = (~scrollx[0] & 0x2000);

	UINT16 page_select = scrolly[0xff];

	const INT32 scrollx_adjustments[2][8] = { // and page sizes
		{ 0x100, 0x200, 0x200, 0x200, -16, -8, -8, -8 }, // cyclwarr
		{ 0x200, 0x200, 0x200, 0x200, -16, -8, -8, -8 }  // bigfight
	};

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		INT32 y_base = rowscroll_enable ? y : 0;
		INT32 x_base = colscroll_enable ? y : 0;
		INT32 src_y = (scrolly[y_base] & 0x7ff);
		INT32 src_x = (scrollx[x_base] & 0x7ff);

		INT32 page_disable = scrolly[y_base] & 0x800;
		INT32 cur_page = src_y + y;

		road_color_bank = 0;
		if (scrolly[y_base] & 0x8000)
		{
			road_color_bank = (scrolly[y_base] >> 13) & 3;
			page_disable = 1;
		}

		if ((cur_page - page_select) >= scrollx_adjustments[game_select][src] && page_disable == 0)
			src_y -= scrollx_adjustments[game_select][src];

		GenericTilesSetScanline(y);
		GenericTilemapSetScrollX(src, src_x - scrollx_adjustments[game_select][4 + src]);
		GenericTilemapSetScrollY(src, src_y);
		GenericTilemapDraw(src, 0, TMAP_SET_GROUP(hi_priority));
		GenericTilesClearClip();
	}
}

static void draw_bg_layers(INT32 hi_priority)
{
	UINT16 *vidram[2] = { (UINT16*)DrvVidRAM[0], (UINT16*)DrvVidRAM[1] };

	if (nBurnLayer & 8) draw_bg(3, &vidram[1][0x000], &vidram[1][0x100], hi_priority);
	if (nBurnLayer & 4) draw_bg(2, &vidram[1][0x200], &vidram[1][0x300], hi_priority);
	if (nBurnLayer & 2) draw_bg(1, &vidram[0][0x000], &vidram[0][0x100], hi_priority);
	if (nBurnLayer & 1) draw_bg(0, &vidram[0][0x200], &vidram[0][0x300], hi_priority);
}

static INT32 DrvDraw()
{
	DrvPaletteUpdate(); //BurnPaletteUpdate_xRRRRRGGGGGBBBBB();

	if (BurnRecalc) {

		BurnRecalc = 0;
	}

	BurnTransferClear(0);

	draw_sprites(1); // alpha pass only
	draw_bg_layers(0);
	apply_shadow_bitmap();
	draw_sprites(0);
	draw_bg_layers(1);
	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}
	}

	INT32 nInterleave = 272*1;
	INT32 nCyclesTotal[3] = { (12500000 * 100) / 5744, (12500000 * 100) / 5744, (4000000 * 100) / 5744 }; // 57.444852941176470588235294117647 hz
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	SekNewFrame();
	ZetNewFrame();

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		CPU_RUN(0, Sek);
		if (i == 239*1) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		SekClose();

		SekOpen(1);
		CPU_RUN(1, Sek);
		if (i == 239*1) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		SekClose();

		CPU_RUN_TIMER(2);
	}

	ZetClose();

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(AllRam, RamEnd-AllRam, "All Ram");
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		cxd1095Scan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(control_data);
		SCAN_VAR(last_control);
		SCAN_VAR(video_config);
		SCAN_VAR(tile_banks);
		SCAN_VAR(mixing_control);
		SCAN_VAR(soundlatch);
	}

	return 0;
}


// Cycle Warriors (Rev. C)

static struct BurnRomInfo cyclwarrRomDesc[] = {
	{ "cw16c.ic77",		0x020000, 0x4d88892b,  1 | BRF_PRG | BRF_ESS },	//  0 68K #0 Code
	{ "cw18c.ic98",		0x020000, 0x4ff56209,  1 | BRF_PRG | BRF_ESS },	//  1 
	{ "cw17b.ic78",		0x020000, 0xda998afc,  1 | BRF_PRG | BRF_ESS },	//  2 
	{ "cw19b.ic99",		0x020000, 0xc15a8413,  1 | BRF_PRG | BRF_ESS },	//  3 

	{ "cw20b.ic100",	0x020000, 0x4d75292a,  2 | BRF_PRG | BRF_ESS },	//  4 68K #1 Code
	{ "cw22b.ic102",	0x020000, 0x0aec0ba4,  2 | BRF_PRG | BRF_ESS },	//  5 
	{ "cw21.ic101",		0x020000, 0xed90d956,  2 | BRF_PRG | BRF_ESS },	//  6 
	{ "cw23.ic103",		0x020000, 0x009cdc78,  2 | BRF_PRG | BRF_ESS },	//  7 

	{ "cw26a.ic91",		0x010000, 0xf7a70e3a,  3 | BRF_PRG | BRF_ESS },	//  8 Z80 Code

	{ "cw00a.ic26",		0x020000, 0x058a77f1,  4 | BRF_GRA },			//  9 Sprites (Low)
	{ "cw08a.ic45",		0x020000, 0xf53993e7,  4 | BRF_GRA },			// 10 
	{ "cw02a.ic28",		0x020000, 0x4dadf3cb,  4 | BRF_GRA },			// 11 
	{ "cw10a.ic47",		0x020000, 0x3b7cd251,  4 | BRF_GRA },			// 12 
	{ "cw01a.ic27",		0x020000, 0x7c639948,  4 | BRF_GRA },			// 13 
	{ "cw09a.ic46",		0x020000, 0x4ba24af5,  4 | BRF_GRA },			// 14 
	{ "cw03a.ic29",		0x020000, 0x3ca6f98e,  4 | BRF_GRA },			// 15 
	{ "cw11a.ic48",		0x020000, 0x5d760392,  4 | BRF_GRA },			// 16 

	{ "cw04a.ic30",		0x020000, 0xf05f594d,  5 | BRF_GRA },			// 17 Sprites (High)
	{ "cw12a.ic49",		0x020000, 0x4ac07e8b,  5 | BRF_GRA },			// 18 
	{ "cw06a.ic32",		0x020000, 0xf628edc9,  5 | BRF_GRA },			// 19 
	{ "cw14a.ic51",		0x020000, 0xa9131f5f,  5 | BRF_GRA },			// 20 
	{ "cw05a.ic31",		0x020000, 0xc8f5faa9,  5 | BRF_GRA },			// 21 
	{ "cw13a.ic50",		0x020000, 0x8091d381,  5 | BRF_GRA },			// 22 
	{ "cw07a.ic33",		0x020000, 0x314579b5,  5 | BRF_GRA },			// 23 
	{ "cw15a.ic52",		0x020000, 0x7ed4b721,  5 | BRF_GRA },			// 24 

	{ "cw27.ic128",		0x020000, 0x2db48a9e,  6 | BRF_GRA },			// 25 Tile Color Lookup Table

	{ "cw30.ic73",		0x020000, 0x331d0711,  7 | BRF_GRA },			// 26 Tiles
	{ "cw29.ic72",		0x020000, 0x64dd519c,  7 | BRF_GRA },			// 27 
	{ "cw28.ic71",		0x020000, 0x3fc568ed,  7 | BRF_GRA },			// 28 

	{ "cw24a.ic39",		0x020000, 0x22600cba,  8 | BRF_SND },			// 29 Samples
	{ "cw25a.ic40",		0x020000, 0x372c6bc8,  8 | BRF_SND },			// 30 
};

STD_ROM_PICK(cyclwarr)
STD_ROM_FN(cyclwarr)

struct BurnDriver BurnDrvCyclwarr = {
	"cyclwarr", NULL, NULL, NULL, "1991",
	"Cycle Warriors (Rev. C)\0", NULL, "Tatsumi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_RACING | GBF_SCRFIGHT, 0,
	NULL, cyclwarrRomInfo, cyclwarrRomName, NULL, NULL, NULL, NULL, DrvInputInfo, CyclwarrDIPInfo,
	CyclwarrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x8000,
	320, 240, 4, 3
};


// Cycle Warriors (Rev. B)

static struct BurnRomInfo cyclwarraRomDesc[] = {
	{ "cw16b.ic77",		0x020000, 0xcb1a737a,  1 | BRF_PRG | BRF_ESS },	//  0 68K #0 Code
	{ "cw18b.ic98",		0x020000, 0x0633ddcb,  1 | BRF_PRG | BRF_ESS },	//  1 
	{ "cw17a.ic78",		0x020000, 0x2ad6f836,  1 | BRF_PRG | BRF_ESS },	//  2 
	{ "cw19a.ic99",		0x020000, 0xd3853658,  1 | BRF_PRG | BRF_ESS },	//  3 

	{ "cw20a.ic100",	0x020000, 0xc3578ac1,  2 | BRF_PRG | BRF_ESS },	//  4 68K #1 Code
	{ "cw22a.ic102",	0x020000, 0x5339ed24,  2 | BRF_PRG | BRF_ESS },	//  5 
	{ "cw21.ic101",		0x020000, 0xed90d956,  2 | BRF_PRG | BRF_ESS },	//  6 
	{ "cw23.ic103",		0x020000, 0x009cdc78,  2 | BRF_PRG | BRF_ESS },	//  7 

	{ "cw26a.ic91",		0x010000, 0xf7a70e3a,  3 | BRF_PRG | BRF_ESS },	//  8 Z80 Code

	{ "cw00a.ic26",		0x020000, 0x058a77f1,  4 | BRF_GRA },			//  9 Sprites (Low)
	{ "cw08a.ic45",		0x020000, 0xf53993e7,  4 | BRF_GRA },			// 10 
	{ "cw02a.ic28",		0x020000, 0x4dadf3cb,  4 | BRF_GRA },			// 11 
	{ "cw10a.ic47",		0x020000, 0x3b7cd251,  4 | BRF_GRA },			// 12 
	{ "cw01a.ic27",		0x020000, 0x7c639948,  4 | BRF_GRA },			// 13 
	{ "cw09a.ic46",		0x020000, 0x4ba24af5,  4 | BRF_GRA },			// 14 
	{ "cw03a.ic29",		0x020000, 0x3ca6f98e,  4 | BRF_GRA },			// 15 
	{ "cw11a.ic48",		0x020000, 0x5d760392,  4 | BRF_GRA },			// 16 

	{ "cw04a.ic30",		0x020000, 0xf05f594d,  5 | BRF_GRA },			// 17 Sprites (High)
	{ "cw12a.ic49",		0x020000, 0x4ac07e8b,  5 | BRF_GRA },			// 18 
	{ "cw06a.ic32",		0x020000, 0xf628edc9,  5 | BRF_GRA },			// 19 
	{ "cw14a.ic51",		0x020000, 0xa9131f5f,  5 | BRF_GRA },			// 20 
	{ "cw05a.ic31",		0x020000, 0xc8f5faa9,  5 | BRF_GRA },			// 21 
	{ "cw13a.ic50",		0x020000, 0x8091d381,  5 | BRF_GRA },			// 22 
	{ "cw07a.ic33",		0x020000, 0x314579b5,  5 | BRF_GRA },			// 23 
	{ "cw15a.ic52",		0x020000, 0x7ed4b721,  5 | BRF_GRA },			// 24 

	{ "cw27.ic128",		0x020000, 0x2db48a9e,  6 | BRF_GRA },			// 25 Tile Color Lookup Table

	{ "cw30.ic73",		0x020000, 0x331d0711,  7 | BRF_GRA },			// 26 Tiles
	{ "cw29.ic72",		0x020000, 0x64dd519c,  7 | BRF_GRA },			// 27 
	{ "cw28.ic71",		0x020000, 0x3fc568ed,  7 | BRF_GRA },			// 28 

	{ "cw24a.ic39",		0x020000, 0x22600cba,  8 | BRF_SND },			// 29 Samples
	{ "cw25a.ic40",		0x020000, 0x372c6bc8,  8 | BRF_SND },			// 30 
};

STD_ROM_PICK(cyclwarra)
STD_ROM_FN(cyclwarra)

struct BurnDriver BurnDrvCyclwarra = {
	"cyclwarra", "cyclwarr", NULL, NULL, "1991",
	"Cycle Warriors (Rev. B)\0", NULL, "Tatsumi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_RACING | GBF_SCRFIGHT, 0,
	NULL, cyclwarraRomInfo, cyclwarraRomName, NULL, NULL, NULL, NULL, DrvInputInfo, CyclwarbDIPInfo,
	CyclwarrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x8000,
	320, 240, 4, 3
};


// Cycle Warriors

static struct BurnRomInfo cyclwarrbRomDesc[] = {
	{ "cw16.ic77",		0x020000, 0x47d57cf9,  1 | BRF_PRG | BRF_ESS },	//  0 68K #0 Code
	{ "cw18.ic98",		0x020000, 0x7d541f9e,  1 | BRF_PRG | BRF_ESS },	//  1 
	{ "cw17.ic78",		0x020000, 0x008bdf09,  1 | BRF_PRG | BRF_ESS },	//  2 
	{ "cw19.ic99",		0x020000, 0xe82244e0,  1 | BRF_PRG | BRF_ESS },	//  3 

	{ "cw20.ic100",		0x020000, 0xc7a6fa85,  2 | BRF_PRG | BRF_ESS },	//  4 68K #1 Code
	{ "cw22.ic102",		0x020000, 0x917c1a2a,  2 | BRF_PRG | BRF_ESS },	//  5 
	{ "cw21.ic101",		0x020000, 0xed90d956,  2 | BRF_PRG | BRF_ESS },	//  6 
	{ "cw23.ic103",		0x020000, 0x009cdc78,  2 | BRF_PRG | BRF_ESS },	//  7 

	{ "cw26.ic91",		0x010000, 0xa6485a3a,  3 | BRF_PRG | BRF_ESS },	//  8 Z80 Code

	{ "cw00.ic26",		0x020000, 0xba00c582,  4 | BRF_GRA },			//  9 Sprites (Low)
	{ "cw08.ic45",		0x020000, 0x1583e576,  4 | BRF_GRA },			// 10 
	{ "cw02.ic28",		0x020000, 0x8376a744,  4 | BRF_GRA },			// 11 
	{ "cw10.ic47",		0x020000, 0x901d849d,  4 | BRF_GRA },			// 12 
	{ "cw01.ic27",		0x020000, 0x35979022,  4 | BRF_GRA },			// 13 
	{ "cw09.ic46",		0x020000, 0x8114be09,  4 | BRF_GRA },			// 14 
	{ "cw03.ic29",		0x020000, 0x951ed812,  4 | BRF_GRA },			// 15 
	{ "cw11.ic48",		0x020000, 0xa7e5bf0b,  4 | BRF_GRA },			// 16 

	{ "cw04.ic30",		0x020000, 0x890ea7b1,  5 | BRF_GRA },			// 17 Sprites (High)
	{ "cw12.ic49",		0x020000, 0x1587e96d,  5 | BRF_GRA },			// 18 
	{ "cw06.ic32",		0x020000, 0x47decb23,  5 | BRF_GRA },			// 19 
	{ "cw14.ic51",		0x020000, 0xa75072a1,  5 | BRF_GRA },			// 20 
	{ "cw05.ic31",		0x020000, 0x4e49fcc5,  5 | BRF_GRA },			// 21 
	{ "cw13.ic50",		0x020000, 0x51aee710,  5 | BRF_GRA },			// 22 
	{ "cw07.ic33",		0x020000, 0x4f6b3c72,  5 | BRF_GRA },			// 23 
	{ "cw15.ic52",		0x020000, 0x9cfc3b14,  5 | BRF_GRA },			// 24 

	{ "cw27.ic128",		0x020000, 0x2db48a9e,  6 | BRF_GRA },			// 25 Tile Color Lookup Table

	{ "cw30.ic73",		0x020000, 0x331d0711,  7 | BRF_GRA },			// 26 Tiles
	{ "cw29.ic72",		0x020000, 0x64dd519c,  7 | BRF_GRA },			// 27 
	{ "cw28.ic71",		0x020000, 0x3fc568ed,  7 | BRF_GRA },			// 28 

	{ "cw24.ic39",		0x020000, 0x08656756,  8 | BRF_SND },			// 29 Samples
	{ "cw25.ic40",		0x020000, 0x36c0b8a6,  8 | BRF_SND },			// 30 
};

STD_ROM_PICK(cyclwarrb)
STD_ROM_FN(cyclwarrb)

struct BurnDriver BurnDrvCyclwarrb = {
	"cyclwarrb", "cyclwarr", NULL, NULL, "1991",
	"Cycle Warriors\0", NULL, "Tatsumi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_RACING | GBF_SCRFIGHT, 0,
	NULL, cyclwarrbRomInfo, cyclwarrbRomName, NULL, NULL, NULL, NULL, DrvInputInfo, CyclwarbDIPInfo,
	CyclwarrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x8000,
	320, 240, 4, 3
};


// Big Fight: Big Trouble In The Atlantic Ocean

static struct BurnRomInfo bigfightRomDesc[] = {
	{ "rom16.ic77",		0x040000, 0xe7304ec8,  1 | BRF_PRG | BRF_ESS },	//  0 68K #0 Code
	{ "rom17.ic98",		0x040000, 0x4cf090f6,  1 | BRF_PRG | BRF_ESS },	//  1 

	{ "rom18.ic100",	0x040000, 0x49df6207,  2 | BRF_PRG | BRF_ESS },	//  2 68K #1 Code
	{ "rom19.ic102",	0x040000, 0xc12aa9e9,  2 | BRF_PRG | BRF_ESS },	//  3 

	{ "rom20.ic91",		0x010000, 0xb3add091,  3 | BRF_PRG | BRF_ESS },	//  4 Z80 Code

	{ "rom0.ic26",		0x080000, 0xa4a3c8d6,  4 | BRF_GRA },			//  5 Sprites (Low)
	{ "rom8.ic45",		0x080000, 0x220956ed,  4 | BRF_GRA },			//  6 
	{ "rom2.ic28",		0x080000, 0xc4f6d243,  4 | BRF_GRA },			//  7 
	{ "rom10.ic47",		0x080000, 0x0212d472,  4 | BRF_GRA },			//  8 

	{ "rom4.ic30",		0x080000, 0x999ff7e9,  5 | BRF_GRA },			//  9 Sprites (High)
	{ "rom12.ic49",		0x080000, 0xcb4c1f0b,  5 | BRF_GRA },			// 10 
	{ "rom6.ic32",		0x080000, 0xf70e2d47,  5 | BRF_GRA },			// 11 
	{ "rom14.ic51",		0x080000, 0x77430bc9,  5 | BRF_GRA },			// 12 

	{ "rom21.ic128",	0x020000, 0xda027dcf,  6 | BRF_GRA },			// 13 Tile Color Lookup Table

	{ "rom24.ic73",		0x020000, 0xc564185d,  7 | BRF_GRA },			// 14 Tiles
	{ "rom23.ic72",		0x020000, 0xf8bb340b,  7 | BRF_GRA },			// 15 
	{ "rom22.ic71",		0x020000, 0xfb505074,  7 | BRF_GRA },			// 16 

	{ "rom15.ic39",		0x040000, 0x58d136e8,  8 | BRF_SND },			// 17 Samples
};

STD_ROM_PICK(bigfight)
STD_ROM_FN(bigfight)

struct BurnDriver BurnDrvBigfight = {
	"bigfight", NULL, NULL, NULL, "1992",
	"Big Fight: Big Trouble In The Atlantic Ocean\0", NULL, "Tatsumi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, bigfightRomInfo, bigfightRomName, NULL, NULL, NULL, NULL, DrvInputInfo, BigfightDIPInfo,
	BigfightInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x8000,
	320, 240, 4, 3
};


// Big Fight: Big Trouble In The Atlantic Ocean (Japan, Rev. F)

static struct BurnRomInfo bigfightjRomDesc[] = {
	{ "bfj16f.ic77",	0x020000, 0x9141a488,  1 | BRF_PRG | BRF_ESS },	//  0 68K #0 Code
	{ "bf18f.ic98",		0x020000, 0xf23a4935,  1 | BRF_PRG | BRF_ESS },	//  1 
	{ "bf17e.ic78",		0x020000, 0x5e5d023d,  1 | BRF_PRG | BRF_ESS },	//  2 
	{ "bf19e.ic99",		0x020000, 0x5329e151,  1 | BRF_PRG | BRF_ESS },	//  3 

	{ "bf20.ic100",		0x020000, 0x5bd44e11,  2 | BRF_PRG | BRF_ESS },	//  4 68K #1 Code
	{ "bf22.ic102",		0x020000, 0xe52c29ab,  2 | BRF_PRG | BRF_ESS },	//  5 
	{ "bf21d.ic101",	0x020000, 0xc4c2f969,  2 | BRF_PRG | BRF_ESS },	//  6 
	{ "bf23d.ic103",	0x020000, 0xc05ae1fe,  2 | BRF_PRG | BRF_ESS },	//  7 

	{ "bf36b.ic91",		0x010000, 0xb3add091,  3 | BRF_PRG | BRF_ESS },	//  8 Z80 Code

	{ "bf00d.ic26",		0x040000, 0xf506d508,  4 | BRF_GRA },			//  9 Sprites (Low)
	{ "bf08d.ic45",		0x040000, 0x4bf948b9,  4 | BRF_GRA },			// 10 
	{ "bf02d.ic28",		0x040000, 0xaf30acf7,  4 | BRF_GRA },			// 11 
	{ "bf10d.ic47",		0x040000, 0xffddb7f8,  4 | BRF_GRA },			// 12 
	{ "bf01d.ic27",		0x020000, 0xec3f5f17,  4 | BRF_GRA },			// 13 
	{ "bf09d.ic46",		0x020000, 0x284837ed,  4 | BRF_GRA },			// 14 
	{ "bf03d.ic29",		0x020000, 0x2ba0398e,  4 | BRF_GRA },			// 15 
	{ "bf11d.ic48",		0x020000, 0x3f2fa72f,  4 | BRF_GRA },			// 16 

	{ "bf04d.ic30",		0x040000, 0x6203d320,  5 | BRF_GRA },			// 17 Sprites (High)
	{ "bf12d.ic49",		0x040000, 0xd261dfa7,  5 | BRF_GRA },			// 18 
	{ "bf06d.ic32",		0x040000, 0xbe187c3c,  5 | BRF_GRA },			// 19 
	{ "bf14d.ic51",		0x040000, 0x60f2ab3d,  5 | BRF_GRA },			// 20 
	{ "bf05d.ic31",		0x020000, 0x2229acbc,  5 | BRF_GRA },			// 21 
	{ "bf13d.ic50",		0x020000, 0x1e46cd79,  5 | BRF_GRA },			// 22 
	{ "bf07d.ic33",		0x020000, 0x4940b0bb,  5 | BRF_GRA },			// 23 
	{ "bf15d.ic52",		0x020000, 0xdab0c80a,  5 | BRF_GRA },			// 24 

	{ "bf27.ic128",		0x020000, 0xda027dcf,  6 | BRF_GRA },			// 25 Tile Color Lookup Table

	{ "bf30.ic73",		0x020000, 0xc564185d,  7 | BRF_GRA },			// 26 Tiles
	{ "bf29.ic72",		0x020000, 0xf8bb340b,  7 | BRF_GRA },			// 27 
	{ "bf28.ic71",		0x020000, 0xfb505074,  7 | BRF_GRA },			// 28 

	{ "bf24.ic39",		0x020000, 0x9db80c8a,  8 | BRF_SND },			// 29 Samples
	{ "bf25.ic40",		0x020000, 0x630154c4,  8 | BRF_SND },			// 30 
};

STD_ROM_PICK(bigfightj)
STD_ROM_FN(bigfightj)

struct BurnDriver BurnDrvBigfightj = {
	"bigfightj", "bigfight", NULL, NULL, "1992",
	"Big Fight: Big Trouble In The Atlantic Ocean (Japan, Rev. F)\0", NULL, "Tatsumi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, bigfightjRomInfo, bigfightjRomName, NULL, NULL, NULL, NULL, DrvInputInfo, BigfightDIPInfo,
	BigfightjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x8000,
	320, 240, 4, 3
};
