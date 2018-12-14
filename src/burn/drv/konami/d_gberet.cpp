// FB Alpha Green Beret driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprRAM1;
static UINT8 *DrvSprRAM2;
static UINT8 *DrvScrollRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 sprite_bank;
static INT32 z80_bank;
static INT32 irq_mask;
static INT32 irq_timer;
static INT32 flipscreen;
static INT32 soundlatch;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo GberetInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Gberet)

static struct BurnInputInfo GberetbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Gberetb)

static struct BurnDIPInfo GberetDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL					},
	{0x13, 0xff, 0xff, 0x4a, NULL					},
	{0x14, 0xff, 0xff, 0x0f, NULL					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "No Coin B"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x13, 0x01, 0x03, 0x03, "2"					},
	{0x13, 0x01, 0x03, 0x02, "3"					},
	{0x13, 0x01, 0x03, 0x01, "5"					},
	{0x13, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x13, 0x01, 0x04, 0x00, "Upright"				},
	{0x13, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x13, 0x01, 0x18, 0x18, "30K, 70K, Every 70K"	},
	{0x13, 0x01, 0x18, 0x10, "40K, 80K, Every 80K"	},
	{0x13, 0x01, 0x18, 0x08, "50K, 100K, Every 100K"},
	{0x13, 0x01, 0x18, 0x00, "50K, 200K, Every 200K"},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x60, 0x60, "Easy"					},
	{0x13, 0x01, 0x60, 0x40, "Normal"				},
	{0x13, 0x01, 0x60, 0x20, "Difficult"			},
	{0x13, 0x01, 0x60, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x80, 0x80, "Off"					},
	{0x13, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x01, 0x01, "Off"					},
	{0x14, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Upright Controls"		},
	{0x14, 0x01, 0x02, 0x02, "Single"				},
	{0x14, 0x01, 0x02, 0x00, "Dual"					},
};

STDDIPINFO(Gberet)

static struct BurnDIPInfo GberetbDIPList[]=
{
	{0x09, 0xff, 0xff, 0xff, NULL					},
	{0x0a, 0xff, 0xff, 0x4a, NULL					},
	{0x0b, 0xff, 0xff, 0x0f, NULL					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x09, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x09, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x09, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x09, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x09, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x09, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x09, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x09, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x09, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x09, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x09, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x09, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x09, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x09, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x09, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x09, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x09, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x09, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x09, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x09, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x09, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x09, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x09, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x09, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x09, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x09, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x09, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x09, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x09, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x09, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x09, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x09, 0x01, 0xf0, 0x00, "No Coin B"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0a, 0x01, 0x03, 0x03, "2"					},
	{0x0a, 0x01, 0x03, 0x02, "3"					},
	{0x0a, 0x01, 0x03, 0x01, "5"					},
	{0x0a, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0a, 0x01, 0x04, 0x00, "Upright"				},
	{0x0a, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x0a, 0x01, 0x18, 0x18, "30K, 70K, Every 70K"	},
	{0x0a, 0x01, 0x18, 0x10, "40K, 80K, Every 80K"	},
	{0x0a, 0x01, 0x18, 0x08, "50K, 100K, Every 100K"},
	{0x0a, 0x01, 0x18, 0x00, "50K, 200K, Every 200K"},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0a, 0x01, 0x60, 0x60, "Easy"					},
	{0x0a, 0x01, 0x60, 0x40, "Normal"				},
	{0x0a, 0x01, 0x60, 0x20, "Difficult"			},
	{0x0a, 0x01, 0x60, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0a, 0x01, 0x80, 0x80, "Off"					},
	{0x0a, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Gberetb)

static struct BurnDIPInfo MrgoemonDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL					},
	{0x13, 0xff, 0xff, 0x4a, NULL					},
	{0x14, 0xff, 0xff, 0x0f, NULL					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "No Coin B"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x13, 0x01, 0x03, 0x03, "2"					},
	{0x13, 0x01, 0x03, 0x02, "3"					},
	{0x13, 0x01, 0x03, 0x01, "5"					},
	{0x13, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x13, 0x01, 0x04, 0x00, "Upright"				},
	{0x13, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x13, 0x01, 0x18, 0x18, "20K, Every 60K"		},
	{0x13, 0x01, 0x18, 0x10, "30K, Every 70K"		},
	{0x13, 0x01, 0x18, 0x08, "40K, Every 80K"		},
	{0x13, 0x01, 0x18, 0x00, "50K, Every 90K"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x60, 0x60, "Easy"					},
	{0x13, 0x01, 0x60, 0x40, "Normal"				},
	{0x13, 0x01, 0x60, 0x20, "Difficult"			},
	{0x13, 0x01, 0x60, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x80, 0x80, "Off"					},
	{0x13, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x01, 0x01, "Off"					},
	{0x14, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Upright Controls"		},
	{0x14, 0x01, 0x02, 0x02, "Single"				},
	{0x14, 0x01, 0x02, 0x00, "Dual"					},
};

STDDIPINFO(Mrgoemon)

static void __fastcall gberet_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffc0) == 0xe000) {
		DrvScrollRAM[address & 0x3f] = data;
		return;
	}

	switch (address)
	{
		case 0xe040:
		case 0xe041:
		case 0xe042:
		return;	// nopw

		case 0xe043:
			sprite_bank = data;
		return;

		case 0xe044:
		{
			INT32 ack_mask = ~data & irq_mask;

			if (ack_mask & 1) ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			if (ack_mask & 6) ZetSetIRQLine(0x00, CPU_IRQSTATUS_NONE);

			irq_mask   = data & 0x07;
			flipscreen = data & 0x08;
		}
		return;

		case 0xf000:
			// coin counter
		return;

		case 0xf200:
			soundlatch = data;
		return;

		case 0xf400:
			SN76496Write(0, soundlatch);
		return;

		case 0xf600:
			BurnWatchdogWrite();
		return;
	}
}

static UINT8 __fastcall gberet_read(UINT16 address)
{
	switch (address)
	{
		case 0xf200:
			return DrvDips[1]; // dsw2

		case 0xf400:
			return DrvDips[2]; // dsw3

		case 0xf600:
			return DrvDips[0]; // dsw1

		case 0xf601:
			return DrvInputs[1]; // p2

		case 0xf602:
			return DrvInputs[0]; // p1

		case 0xf603:
			return DrvInputs[2]; // system
	}

	return 0;
}

static void __fastcall gberetb_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe044:
			flipscreen = data & 0x08;
		return;

		case 0xf000:
		return; // coin counter

		case 0xf400:
			SN76496Write(0, data);
		return;

		case 0xf800:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xf900:
		case 0xf901:
			DrvScrollRAM[0x80] = data;
			DrvScrollRAM[0x81] = address & 1;
		return;
	}
}

static UINT8 __fastcall gberetb_read(UINT16 address)
{
	switch (address)
	{
		case 0xf200:
			return DrvDips[1];

		case 0xf600:
			return DrvInputs[1]; // p2

		case 0xf601:
			return DrvDips[0];

		case 0xf602:
			return DrvInputs[0]; // p1

		case 0xf603:
			return DrvInputs[2];

		case 0xf800:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return 0xff;
	}

	return 0;
}

static void bankswitch(INT32 data)
{
	z80_bank = data | 0x80;

	ZetMapMemory(DrvZ80ROM + 0xc000 + (data & 7) * 0x800, 0xf800, 0xffff, MAP_ROM);
}

static void __fastcall mrgoemon_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf000:
			bankswitch(data >> 5);
		break; // break, not return!
	}

	gberet_write(address, data);
}

static tilemap_callback( bg )
{
	INT32 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] + ((attr & 0x40) << 2);
	INT32 color = attr & 0x0f;
	INT32 flags = TILE_FLIPYX((attr & 0x30) >> 4) | TILE_GROUP((attr >> 7)&1);

	TILE_SET_INFO(0, code, color, flags);
	*category = color;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnWatchdogReset();

	HiscoreReset();

	irq_mask = 0;
	irq_timer = 0;
	sprite_bank = 0;
	z80_bank = 0;
	flipscreen = 0;
	soundlatch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000220;

	DrvPalette		= (UINT32*)Next; Next += 0x201 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvColRAM		= Next; Next += 0x000800;
	DrvSprRAM0		= Next; Next += 0x000100;
	DrvSprRAM1		= Next; Next += 0x000100;
	DrvSprRAM2		= Next; Next += 0x000200;
	DrvScrollRAM	= Next; Next += 0x000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { STEP4(0,1) };
	INT32 XOffs[16] = { STEP8(0,4), STEP8(256,4) };
	INT32 YOffs[16] = { STEP8(0,32), STEP8(512,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x04000);

	GfxDecode(0x0200, 4,  8,  8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x10000);

	GfxDecode(0x0200, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 BootGfxDecode()
{
	INT32 Plane0[4]  = { STEP4(0,1) };
	INT32 XOffs0[8]  = { 6*4, 7*4, 0*4, 1*4, 2*4, 3*4, 4*4, 5*4 };
	INT32 YOffs0[8]  = { STEP8(0,32) };
	INT32 Plane1[4]  = { STEP4(0,0x4000*8) };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs1[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x04000);

	GfxDecode(0x0200, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x10000);

	GfxDecode(0x0200, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (game_select == 0) // gberet
	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x04000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x08000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00120,  k++, 1)) return 1;

		DrvGfxDecode();
	}
	else if (game_select == 1) // mrgoemon
	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x08000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00120,  k++, 1)) return 1;

		DrvGfxDecode();
	}
	else if (game_select == 2) // gberetb
	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x08000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00120,  k++, 1)) return 1;

		BootGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvColRAM,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM1,	0xd000, 0xd0ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM0,	0xd100, 0xd1ff, MAP_RAM); // 100-1ff
	ZetMapMemory(DrvZ80RAM,		0xd200, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvScrollRAM,	0xe000, 0xe0ff, MAP_ROM); // handler
	ZetMapMemory(DrvSprRAM2,	0xe800, 0xe9ff, MAP_RAM); // 100-1ff (bootleg)
	ZetSetWriteHandler((game_select == 1) ? mrgoemon_write : ((game_select == 2) ? gberetb_write : gberet_write));
	ZetSetReadHandler((game_select == 2) ? gberetb_read : gberet_read);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	SN76489AInit(0, 18432000 / 12, 0);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x08000, 0, 0xf);
	GenericTilemapSetScrollRows(0, 32);
	GenericTilemapCategoryConfig(0, 0x10);
	for (INT32 i = 0; i < 0x100; i++) {
		GenericTilemapSetCategoryEntry(0, (i / 16), i & 15, ((DrvColPROM[i+0x20]&0xf) == 0xf) ? 0 : 1);
	}
	GenericTilemapSetOffsets(0, -8, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	SN76496Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 tab[0x20];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		tab[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		DrvPalette[i] = tab[(DrvColPROM[0x020 + i] & 0xf) | ((~i & 0x100) >> 4)];
	}

	DrvPalette[0x200] = BurnHighCol(0xff,0,0xff, 0);
}

static void draw_sprites()
{
	UINT8 *ram = (sprite_bank & 8) ? DrvSprRAM1 : DrvSprRAM0;

	for (INT32 offs = 0; offs < 0xc0; offs += 4)
	{
		if (ram[offs + 3])
		{
			INT32 attr  = ram[offs + 1];
			INT32 code  = ram[offs + 0] + ((attr & 0x40) << 2);
			INT32 sx    = ram[offs + 2] - ((attr & 0x80) << 1);
			INT32 sy    = ram[offs + 3];
			INT32 color = attr & 0x0f;
			INT32 flipx = attr & 0x10;
			INT32 flipy = attr & 0x20;

			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			RenderTileTranstabOffset(pTransDraw, DrvGfxROM1, code, color*16, 0, sx - 8, sy - 16, flipx, flipy, 16, 16, DrvColPROM + 0x120, 0x100);
		}
	}
}

static void gberetb_draw_sprites()
{
	UINT8 *ram = DrvSprRAM2 + 0x100;

	for (INT32 offs = 0x100 - 4; offs >= 0; offs -= 4)
	{
		if (ram[offs + 1])
		{
			INT32 attr  = ram[offs + 3];
			INT32 code  = ram[offs + 0] + ((attr & 0x40) << 2);
			INT32 sx    = ram[offs + 2] - ((attr & 0x80) << 1);
			INT32 sy    = ram[offs + 1];
			INT32 color = attr & 0x0f;
			INT32 flipx = attr & 0x10;
			INT32 flipy = attr & 0x20;

			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			RenderTileTranstabOffset(pTransDraw, DrvGfxROM1, code, color*16, 0, sx - 8, (240 - sy) - 16, flipx, flipy, 16, 16, DrvColPROM + 0x120, 0x100);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	for (INT32 i = 0; i < 32; i++) {
		GenericTilemapSetScrollRow(0, i, DrvScrollRAM[i] | (DrvScrollRAM[i + 32] * 256));
	}

	if (nBurnLayer != 0xff) BurnTransferClear(0x200);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_FORCEOPAQUE);

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(0));

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 BootDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	UINT16 scrollx = DrvScrollRAM[0x80] + (DrvScrollRAM[0x81] * 256) + 56;

	for (INT32 i = 6; i < 29; i++) {
		GenericTilemapSetScrollRow(0, i, scrollx);
	}

	if (nBurnLayer != 0xff) BurnTransferClear(0x200);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_FORCEOPAQUE);

	if (nSpriteEnable & 1) gberetb_draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(0));

	BurnTransferCopy(DrvPalette);

	return 0;
}

static inline void irq_timer_update()
{
	INT32 timer_mask = ~irq_timer & (irq_timer + 1);
	irq_timer++;

	if (timer_mask & irq_mask & 0x01)
		ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);

	if (timer_mask & (irq_mask << 2) & 0x18)
		ZetSetIRQLine(0x00, CPU_IRQSTATUS_ACK);
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 bootleg = (BurnDrvGetFlags() & BDF_BOOTLEG) ? 1 : 0;

	INT32 nInterleave = bootleg ? 10 : 16;
	INT32 nCyclesTotal = bootleg ? (5000000 / 60) : (3072000 / 60);
	INT32 nCyclesDone = 0;
	INT32 nSoundBufferPos = 0;

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone += ZetRun(nCyclesTotal / nInterleave);

		if (bootleg) {
			if (i == 9) ZetSetIRQLine(0x00, CPU_IRQSTATUS_ACK);
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		}
		else
			irq_timer_update();

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	ZetClose();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			SN76496Update(0, pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
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

		ZetScan(nAction);
		BurnWatchdogScan(nAction);
		SN76496Scan(nAction, pnMin);

		SCAN_VAR(sprite_bank);
		SCAN_VAR(z80_bank);
		SCAN_VAR(irq_mask);
		SCAN_VAR(irq_timer);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
	}

	if (nAction & ACB_WRITE)
	{
		if (z80_bank & 0x80)
		{
			ZetOpen(0);
			bankswitch(z80_bank);
			ZetClose();
		}
	}

	return 0;
}


// Green Beret

static struct BurnRomInfo gberetRomDesc[] = {
	{ "577l03.10c",			0x4000, 0xae29e4ff, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "577l02.8c",			0x4000, 0x240836a5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "577l01.7c",			0x4000, 0x41fa3e1f, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "577l07.3f",			0x4000, 0x4da7bd1b, 2 | BRF_GRA },           //  3 Background Tiles

	{ "577l06.5e",			0x4000, 0x0f1cb0ca, 3 | BRF_GRA },           //  4 Sprites
	{ "577l05.4e",			0x4000, 0x523a8b66, 3 | BRF_GRA },           //  5
	{ "577l08.4f",			0x4000, 0x883933a4, 3 | BRF_GRA },           //  6
	{ "577l04.3e",			0x4000, 0xccecda4c, 3 | BRF_GRA },           //  7

	{ "577h09.2f",			0x0020, 0xc15e7c80, 4 | BRF_GRA },           //  8 Color Data
	{ "577h11.6f",			0x0100, 0x2a1a992b, 4 | BRF_GRA },           //  9
	{ "577h10.5f",			0x0100, 0xe9de1e53, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(gberet)
STD_ROM_FN(gberet)

static INT32 GberetInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvGberet = {
	"gberet", NULL, NULL, NULL, "1985",
	"Green Beret\0", NULL, "Konami", "GX577",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, gberetRomInfo, gberetRomName, NULL, NULL, NULL, NULL, GberetInputInfo, GberetDIPInfo,
	GberetInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 224, 4, 3
};


// Rush'n Attack (US)

static struct BurnRomInfo rushatckRomDesc[] = {
	{ "577h03.10c",			0x4000, 0x4d276b52, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "577h02.8c",			0x4000, 0xb5802806, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "577h01.7c",			0x4000, 0xda7c8f3d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "577h07.3f",			0x4000, 0x03f9815f, 2 | BRF_GRA },           //  3 Background Tiles

	{ "577l06.5e",			0x4000, 0x0f1cb0ca, 3 | BRF_GRA },           //  4 Sprites
	{ "577h05.4e",			0x4000, 0x9d028e8f, 3 | BRF_GRA },           //  5
	{ "577l08.4f",			0x4000, 0x883933a4, 3 | BRF_GRA },           //  6
	{ "577l04.3e",			0x4000, 0xccecda4c, 3 | BRF_GRA },           //  7

	{ "577h09.2f",			0x0020, 0xc15e7c80, 4 | BRF_GRA },           //  8 Color Data
	{ "577h11.6f",			0x0100, 0x2a1a992b, 4 | BRF_GRA },           //  9
	{ "577h10.5f",			0x0100, 0xe9de1e53, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(rushatck)
STD_ROM_FN(rushatck)

struct BurnDriver BurnDrvRushatck = {
	"rushatck", "gberet", NULL, NULL, "1985",
	"Rush'n Attack (US)\0", NULL, "Konami", "GX577",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, rushatckRomInfo, rushatckRomName, NULL, NULL, NULL, NULL, GberetInputInfo, GberetDIPInfo,
	GberetInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 224, 4, 3
};


// Green Beret (bootleg)

static struct BurnRomInfo gberetbRomDesc[] = {
	{ "2-ic82.10g",			0x8000, 0x6d6fb494, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "3-ic81.10f",			0x4000, 0xf1520a0a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1-ic92.12c",			0x4000, 0xb0189c87, 2 | BRF_GRA },           //  2 Background Tiles

	{ "7-1c8.2b",			0x4000, 0x86334522, 3 | BRF_GRA },           //  3 Sprites
	{ "6-ic9.2c",			0x4000, 0xbda50d3e, 3 | BRF_GRA },           //  4
	{ "5-ic10.2d",			0x4000, 0x6a7b3881, 3 | BRF_GRA },           //  5
	{ "4-ic11.2e",			0x4000, 0x3fb186c9, 3 | BRF_GRA },           //  6

	{ "577h09",				0x0020, 0xc15e7c80, 4 | BRF_GRA },           //  7 Color Data
	{ "577h11.6f",			0x0100, 0x2a1a992b, 4 | BRF_GRA },           //  8
	{ "577h10.5f",			0x0100, 0xe9de1e53, 4 | BRF_GRA },           //  9

	{ "pal16r6_ic35.5h",	0x0104, 0xbd76fb53, 0 | BRF_OPT },           // 10 PLDs
};

STD_ROM_PICK(gberetb)
STD_ROM_FN(gberetb)

static INT32 GberetbInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvGberetb = {
	"gberetb", "gberet", NULL, NULL, "1985",
	"Green Beret (bootleg)\0", NULL, "bootleg", "GX577",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, gberetbRomInfo, gberetbRomName, NULL, NULL, NULL, NULL, GberetbInputInfo, GberetbDIPInfo,
	GberetbInit, DrvExit, DrvFrame, BootDraw, DrvScan, &DrvRecalc, 0x200,
	240, 224, 4, 3
};


// Mr. Goemon (Japan)

static struct BurnRomInfo mrgoemonRomDesc[] = {
	{ "621d01.10c",			0x8000, 0xb2219c56, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "621d02.12c",			0x8000, 0xc3337a97, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "621a05.6d",			0x4000, 0xf0a6dfc5, 2 | BRF_GRA },           //  2 Background Tiles

	{ "621d03.4d",			0x8000, 0x66f2b973, 3 | BRF_GRA },           //  3 Sprites
	{ "621d04.5d",			0x8000, 0x47df6301, 3 | BRF_GRA },           //  4

	{ "621a06.5f",			0x0020, 0x7c90de5f, 4 | BRF_GRA },           //  5 Color Data
	{ "621a08.7f",			0x0100, 0x2fb244dd, 4 | BRF_GRA },           //  6
	{ "621a07.6f",			0x0100, 0x3980acdc, 4 | BRF_GRA },           //  7
};

STD_ROM_PICK(mrgoemon)
STD_ROM_FN(mrgoemon)

static INT32 MrgoemonInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvMrgoemon = {
	"mrgoemon", NULL, NULL, NULL, "1986",
	"Mr. Goemon (Japan)\0", NULL, "Konami", "GX621",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, mrgoemonRomInfo, mrgoemonRomName, NULL, NULL, NULL, NULL, GberetInputInfo, MrgoemonDIPInfo,
	MrgoemonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 224, 4, 3
};
