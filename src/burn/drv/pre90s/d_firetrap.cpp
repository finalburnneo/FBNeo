// FB Alpha Fire Trap driver module
// Based on MAME driver by Nicola Salmoria and Stephane Humbert


#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6502_intf.h"
#include "burn_ym3526.h"
#include "msm5205.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvBg0RAM;
static UINT8 *DrvBg1RAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvM6502RAM;
static UINT8 *scroll;
static UINT8 *banks;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static UINT8 flipscreen;
static UINT8 soundlatch;
static UINT8 nmi_enable;
static UINT8 sound_irq_enable;
static UINT8 msm5205next;
static UINT8 MSM5205Last = 0;
static UINT8 adpcm_toggle;

static UINT8 coin_command_pending; // bootleg
static UINT8 i8751_current_command;
static UINT8 i8751_return;
static INT32 i8751_init_ptr;

static struct BurnInputInfo FiretrapInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 start"	},
	{"P1 Left Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Left Down",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left Left",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Left Right",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Right Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"		},
	{"P1 Right Down",	BIT_DIGITAL,	DrvJoy1 + 5,	"p2 down"	},
	{"P1 Right Left",	BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"	},
	{"P1 Right Right",	BIT_DIGITAL,	DrvJoy1 + 7,	"p2 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Left Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p3 up"		},
	{"P2 Left Down",	BIT_DIGITAL,	DrvJoy2 + 1,	"p3 down"	},
	{"P2 Left Left",	BIT_DIGITAL,	DrvJoy2 + 2,	"p3 left"	},
	{"P2 Left Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p3 right"	},
	{"P2 Right Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p4 up"		},
	{"P2 Right Down",	BIT_DIGITAL,	DrvJoy2 + 5,	"p4 down"	},
	{"P2 Right Left",	BIT_DIGITAL,	DrvJoy2 + 6,	"p4 left"	},
	{"P2 Right Right",	BIT_DIGITAL,	DrvJoy2 + 7,	"p4 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Firetrap)

static struct BurnInputInfo FiretrapblInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 start"	},
	{"P1 Left Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Left Down",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left Left",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Left Right",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Right Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"		},
	{"P1 Right Down",	BIT_DIGITAL,	DrvJoy1 + 5,	"p2 down"	},
	{"P1 Right Left",	BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"	},
	{"P1 Right Right",	BIT_DIGITAL,	DrvJoy1 + 7,	"p2 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Left Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p3 up"		},
	{"P2 Left Down",	BIT_DIGITAL,	DrvJoy2 + 1,	"p3 down"	},
	{"P2 Left Left",	BIT_DIGITAL,	DrvJoy2 + 2,	"p3 left"	},
	{"P2 Left Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p3 right"	},
	{"P2 Right Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p4 up"		},
	{"P2 Right Down",	BIT_DIGITAL,	DrvJoy2 + 5,	"p4 down"	},
	{"P2 Right Left",	BIT_DIGITAL,	DrvJoy2 + 6,	"p4 left"	},
	{"P2 Right Right",	BIT_DIGITAL,	DrvJoy2 + 7,	"p4 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Firetrapbl)

static struct BurnDIPInfo FiretrapDIPList[]=
{
	{0x18, 0xff, 0xff, 0xdf, NULL			},
	{0x19, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    5, "Coin A"		},
	{0x18, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x18, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x18, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x18, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x18, 0x01, 0x07, 0x04, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x18, 0x01, 0x18, 0x00, "4 Coins 1 Credits"	},
	{0x18, 0x01, 0x18, 0x08, "3 Coins 1 Credits"	},
	{0x18, 0x01, 0x18, 0x10, "2 Coins 1 Credits"	},
	{0x18, 0x01, 0x18, 0x18, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x18, 0x01, 0x20, 0x00, "Upright"		},
	{0x18, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x18, 0x01, 0x40, 0x00, "Off"			},
	{0x18, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x18, 0x01, 0x80, 0x80, "Off"			},
	{0x18, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x19, 0x01, 0x03, 0x02, "Easy"			},
	{0x19, 0x01, 0x03, 0x03, "Normal"		},
	{0x19, 0x01, 0x03, 0x01, "Hard"			},
	{0x19, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x19, 0x01, 0x0c, 0x00, "2"			},
	{0x19, 0x01, 0x0c, 0x0c, "3"			},
	{0x19, 0x01, 0x0c, 0x08, "4"			},
	{0x19, 0x01, 0x0c, 0x04, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x19, 0x01, 0x30, 0x10, "30k and 70k"		},
	{0x19, 0x01, 0x30, 0x00, "50k and 100k"		},
	{0x19, 0x01, 0x30, 0x30, "30k only"		},
	{0x19, 0x01, 0x30, 0x20, "50k only"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x19, 0x01, 0x40, 0x00, "No"			},
	{0x19, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x19, 0x01, 0x80, 0x80, "Off"			},
	{0x19, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Firetrap)

static struct BurnDIPInfo FiretrapjDIPList[]=
{
	{0x18, 0xff, 0xff, 0xdf, NULL			},
	{0x19, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    5, "Coin A"		},
	{0x18, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x18, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x18, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x18, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x18, 0x01, 0x07, 0x04, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x18, 0x01, 0x18, 0x00, "4 Coins 1 Credits"	},
	{0x18, 0x01, 0x18, 0x08, "3 Coins 1 Credits"	},
	{0x18, 0x01, 0x18, 0x10, "2 Coins 1 Credits"	},
	{0x18, 0x01, 0x18, 0x18, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x18, 0x01, 0x20, 0x00, "Upright"		},
	{0x18, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x18, 0x01, 0x40, 0x00, "Off"			},
	{0x18, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x18, 0x01, 0x80, 0x80, "Off"			},
	{0x18, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x19, 0x01, 0x03, 0x02, "Easy"			},
	{0x19, 0x01, 0x03, 0x03, "Normal"		},
	{0x19, 0x01, 0x03, 0x01, "Hard"			},
	{0x19, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x19, 0x01, 0x0c, 0x00, "2"			},
	{0x19, 0x01, 0x0c, 0x0c, "3"			},
	{0x19, 0x01, 0x0c, 0x08, "4"			},
	{0x19, 0x01, 0x0c, 0x04, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x19, 0x01, 0x30, 0x30, "50k & Every 70k"	},
	{0x19, 0x01, 0x30, 0x20, "60k & Every 80k"	},
	{0x19, 0x01, 0x30, 0x10, "80k & Every 100k"	},
	{0x19, 0x01, 0x30, 0x00, "50k only"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x19, 0x01, 0x40, 0x00, "No"			},
	{0x19, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x19, 0x01, 0x80, 0x80, "Off"			},
	{0x19, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Firetrapj)

static struct BurnDIPInfo FiretrapblDIPList[]=
{
	{0x18, 0xff, 0xff, 0xdf, NULL			},
	{0x19, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    5, "Coin A"		},
	{0x18, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x18, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x18, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x18, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x18, 0x01, 0x07, 0x04, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x18, 0x01, 0x18, 0x00, "4 Coins 1 Credits"	},
	{0x18, 0x01, 0x18, 0x08, "3 Coins 1 Credits"	},
	{0x18, 0x01, 0x18, 0x10, "2 Coins 1 Credits"	},
	{0x18, 0x01, 0x18, 0x18, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x18, 0x01, 0x20, 0x00, "Upright"		},
	{0x18, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x18, 0x01, 0x40, 0x00, "Off"			},
	{0x18, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x18, 0x01, 0x80, 0x80, "Off"			},
	{0x18, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x19, 0x01, 0x03, 0x02, "Easy"			},
	{0x19, 0x01, 0x03, 0x03, "Normal"		},
	{0x19, 0x01, 0x03, 0x01, "Hard"			},
	{0x19, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x19, 0x01, 0x0c, 0x00, "2"			},
	{0x19, 0x01, 0x0c, 0x0c, "3"			},
	{0x19, 0x01, 0x0c, 0x08, "4"			},
	{0x19, 0x01, 0x0c, 0x04, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x19, 0x01, 0x30, 0x10, "30k and 70k"		},
	{0x19, 0x01, 0x30, 0x00, "50k and 100k"		},
	{0x19, 0x01, 0x30, 0x30, "30k only"		},
	{0x19, 0x01, 0x30, 0x20, "50k only"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x19, 0x01, 0x40, 0x00, "No"			},
	{0x19, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x19, 0x01, 0x80, 0x80, "Off"			},
	{0x19, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Firetrapbl)

static void firetrap_8751_write(UINT8 data)
{
	static const UINT8 i8751_init_data[0x100] = {
		0xf5,0xd5,0xdd,0x21,0x05,0xc1,0x87,0x5f,0x87,0x83,0x5f,0x16,0x00,0xdd,0x19,0xd1,
		0xf1,0xc9,0xf5,0xd5,0xfd,0x21,0x2f,0xc1,0x87,0x5f,0x16,0x00,0xfd,0x19,0xd1,0xf1,
		0xc9,0xe3,0xd5,0xc5,0xf5,0xdd,0xe5,0xfd,0xe5,0xe9,0xe1,0xfd,0xe1,0xdd,0xe1,0xf1,
		0xc1,0xd1,0xe3,0xc9,0xf5,0xc5,0xe5,0xdd,0xe5,0xc5,0x78,0xe6,0x0f,0x47,0x79,0x48,
		0x06,0x00,0xdd,0x21,0x00,0xd0,0xdd,0x09,0xe6,0x0f,0x6f,0x26,0x00,0x29,0x29,0x29,
		0x29,0xeb,0xdd,0x19,0xc1,0x78,0xe6,0xf0,0x28,0x05,0x11,0x00,0x02,0xdd,0x19,0x79,
		0xe6,0xf0,0x28,0x05,0x11,0x00,0x04,0xdd,0x19,0xdd,0x5e,0x00,0x01,0x00,0x01,0xdd,
		0x09,0xdd,0x56,0x00,0xdd,0xe1,0xe1,0xc1,0xf1,0xc9,0xf5,0x3e,0x01,0x32,0x04,0xf0,
		0xf1,0xc9,0xf5,0x3e,0x00,0x32,0x04,0xf0,0xf1,0xc9,0xf5,0xd5,0xdd,0x21,0x05,0xc1,
		0x87,0x5f,0x87,0x83,0x5f,0x16,0x00,0xdd,0x19,0xd1,0xf1,0xc9,0xf5,0xd5,0xfd,0x21,
		0x2f,0xc1,0x87,0x5f,0x16,0x00,0xfd,0x19,0xd1,0xf1,0xc9,0xe3,0xd5,0xc5,0xf5,0xdd,
		0xe5,0xfd,0xe5,0xe9,0xe1,0xfd,0xe1,0xdd,0xe1,0xf1,0xc1,0xd1,0xe3,0xc9,0xf5,0xc5,
		0xe5,0xdd,0xe5,0xc5,0x78,0xe6,0x0f,0x47,0x79,0x48,0x06,0x00,0xdd,0x21,0x00,0xd0,
		0xdd,0x09,0xe6,0x0f,0x6f,0x26,0x00,0x29,0x29,0x29,0x29,0xeb,0xdd,0x19,0xc1,0x78,
		0xe6,0xf0,0x28,0x05,0x11,0x00,0x02,0xdd,0x19,0x79,0xe6,0xf0,0x28,0x05,0x11,0x00,
		0x04,0xdd,0x19,0xdd,0x5e,0x00,0x01,0x00,0x01,0xdd,0x09,0xdd,0x56,0x00,0xdd,0x00
	};

	static const int i8751_coin_data[2] = { 0x00, 0xb7 };
	static const int i8751_36_data[2] = { 0x00, 0xbc };

	if (BurnDrvGetFlags() & BDF_BOOTLEG) return;

	if (data == 0x26)
	{
		data = 0;
		i8751_return = 0xff;
	}
	else if ((data == 0x13) || (data == 0xf5))
	{
		if (!i8751_current_command) i8751_init_ptr = 0;
		i8751_return = i8751_init_data[i8751_init_ptr++];
	}
	else if (data == 0xbd)
	{
		if (!i8751_current_command) i8751_init_ptr = 0;
		i8751_return = i8751_coin_data[i8751_init_ptr++];
	}
	else if (data == 0x36)
	{
		if (!i8751_current_command) i8751_init_ptr = 0;
		i8751_return = i8751_36_data[i8751_init_ptr++];
	}
	else if (data == 0x14) i8751_return = 1;
	else if (data == 0x02) i8751_return = 0;
	else if (data == 0x72) i8751_return = 3;
	else if (data == 0x69) i8751_return = 2;
	else if (data == 0xcb) i8751_return = 0;
	else if (data == 0x49) i8751_return = 1;
	else if (data == 0x17) i8751_return = 2;
	else if (data == 0x88) i8751_return = 3;
	else                   i8751_return = 0xff;

	ZetSetVector(0xff);
	ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
	i8751_current_command = data;
}

static void bankswitch1(INT32 data)
{
	banks[0] = data;

	INT32 bank = (data & 3) * 0x4000;

	ZetMapMemory(DrvZ80ROM + 0x10000 + bank, 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall firetrap_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf000: // nop / IRQ ACK
		return;

		case 0xf001:
			soundlatch = data;
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		return;

		case 0xf002:
			bankswitch1(data);
		return;

		case 0xf003:
			flipscreen = data;
		return;

		case 0xf004:
			nmi_enable = ~data & 1;
		return;

		case 0xf005:
			firetrap_8751_write(data);
		return;

		case 0xf008:
		case 0xf009:
		case 0xf00a:
		case 0xf00b:
		case 0xf00c:
		case 0xf00d:
		case 0xf00e:
		case 0xf00f:
			scroll[address & 7] = data;
		return;
	}
}

static UINT8 firetrap_8751_bootleg_read()
{
	UINT8 coin = 0;
	UINT8 port = DrvInputs[2] & 0x70;

	if (ZetGetPC(-1) == 0x1188) return ~coin_command_pending;

	if (port != 0x70)
	{
		if (!(port & 0x20)) coin = 1;
		if (!(port & 0x40)) coin = 2;
		if (!(port & 0x10)) coin = 3;

		coin_command_pending = coin;

		return 0xff;
	}

	return 0;
}

static UINT8 __fastcall firetrap_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xf010:
		case 0xf011:
		case 0xf012:
			return DrvInputs[address & 3];

		case 0xf013:
		case 0xf014:
			return DrvDips[address - 0xf013];

		case 0xf016:
			if (BurnDrvGetFlags() & BDF_BOOTLEG)
				return firetrap_8751_bootleg_read();
			return i8751_return;
	}

	return 0;
}

static void bankswitch2(INT32 data)
{
	banks[1] = data;

	INT32 bank = (data & 1) * 0x4000;

	M6502MapMemory(DrvM6502ROM + 0x10000 + bank, 0x4000, 0x7fff, MAP_ROM);
}

static void firetrap_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x1000:
		case 0x1001:
			BurnYM3526Write(address & 1, data);
		return;

		case 0x2000:
			msm5205next = data;
			if (MSM5205Last == 0x8 && msm5205next == 0x8) { // clears up hissing & clicking noise
				MSM5205ResetWrite(0, 1);
			} else MSM5205ResetWrite(0, 0);
			MSM5205Last = data;
		return;

		case 0x2400:
			MSM5205ResetWrite(0, ~data & 0x01);
			sound_irq_enable = data & 0x02;
		return;

		case 0x2800:
			bankswitch2(data);
		return;
	}
}

static UINT8 firetrap_sound_read(UINT16 address)
{
	if (address >= 0x4000 && address <= 0x7fff) {
		return DrvM6502ROM[(address&0x3fff) + (banks[1] & 1) * 0x4000];
	}

	switch (address)
	{
		case 0x3400:
			return soundlatch;
	}

	return 0;
}

static void firetrap_adpcm_interrupt()
{
	MSM5205DataWrite(0, msm5205next >> 4);
	msm5205next <<= 4;

	adpcm_toggle ^= 1;
	if (sound_irq_enable && adpcm_toggle) {
		M6502SetIRQLine(0, CPU_IRQSTATUS_AUTO);
	}
}

static INT32 SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)M6502TotalCycles() * nSoundRate / 1500000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	M6502Open(0);
	M6502Reset();
	BurnYM3526Reset();
	MSM5205Reset();
	M6502Close();

	flipscreen = 0;
	soundlatch = 0;
	nmi_enable = 0;
	sound_irq_enable = 0;
	msm5205next = 0xff;
	adpcm_toggle = 0;

	coin_command_pending = 0;
	i8751_current_command = 0;
	i8751_return = 0;
	i8751_init_ptr = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x020000;
	DrvM6502ROM		= Next; Next += 0x018000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x040000;
	DrvGfxROM2		= Next; Next += 0x040000;
	DrvGfxROM3		= Next; Next += 0x040000;

	DrvColPROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x01000 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x0010000;
	DrvBg0RAM		= Next; Next += 0x0008000;
	DrvBg1RAM		= Next; Next += 0x0008000;
	DrvFgRAM		= Next; Next += 0x0008000;
	DrvSprRAM		= Next; Next += 0x0002000;
	DrvM6502RAM		= Next; Next += 0x0008000;

	scroll			= Next; Next += 0x000008;
	banks			= Next; Next += 0x000002;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Planes0[2] = { 0, 4 };
	INT32 Planes1[4] = { 0, 4, (0x10000*8)+0, (0x10000*8)+4 };
	INT32 Planes2[4] = { (0x8000*8)*0, (0x8000*8)*1, (0x8000*8)*2, (0x8000*8)*3 };
	INT32 XOffs0[16] = { STEP4(3,-1), STEP4((0x1000*8)+3,-1) };
	INT32 XOffs1[16] = { STEP4(3,-1), STEP4((0x8000*8+3), -1), STEP4(0x80+3, -1), STEP4((0x8000*8+0x80+3), -1) };
	INT32 XOffs2[16] = { STEP8(7,-1), STEP8(0x80+7,-1) };
	INT32 YOffs0[8]  = { STEP8(7*8,-8) };
	INT32 YOffs1[16] = { STEP16(15*8,-8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x200, 2,  8,  8, Planes0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

	for (INT32 i = 0; i < 0x20000; i++) {
		tmp[((i & 0x2000) << 2) | ((i & 0xc000) >> 1) | (i & 0x11fff)] = DrvGfxROM1[i];
	}

	GfxDecode(0x400, 4, 16, 16, Planes1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	for (INT32 i = 0; i < 0x20000; i++) {
		tmp[((i & 0x2000) << 2) | ((i & 0xc000) >> 1) | (i & 0x11fff)] = DrvGfxROM2[i];
	}

	GfxDecode(0x400, 4, 16, 16, Planes1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x20000);

	GfxDecode(0x400, 4, 16, 16, Planes2, XOffs2, YOffs1, 0x100, tmp, DrvGfxROM3);

	BurnFree(tmp);
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x10000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x18000,  2, 1)) return 1;

		if (BurnDrvGetFlags() & BDF_BOOTLEG)
			BurnLoadRom(DrvZ80ROM   + 0x08000,  5, 1); // bootleg

		if (BurnLoadRom(DrvM6502ROM + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x10000,  4, 1)) return 1;

	//	if (BurnLoadRom(DrvI8751ROM + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x08000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x10000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x18000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2  + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x08000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x10000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x18000, 14, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3  + 0x00000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3  + 0x08000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3  + 0x10000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3  + 0x18000, 18, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 19, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00100, 20, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvBg0RAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBg1RAM,		0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe800, 0xe9ff, MAP_RAM); // 0-180
	ZetMapMemory(DrvZ80ROM + 0xf800,0xf800, 0xf8ff, MAP_ROM); // bootleg only
	ZetSetWriteHandler(firetrap_main_write);
	ZetSetReadHandler(firetrap_main_read);
	ZetClose();

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,	0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(firetrap_sound_write);
	M6502SetReadHandler(firetrap_sound_read);
	M6502SetReadOpArgHandler(firetrap_sound_read);
	M6502SetReadOpHandler(firetrap_sound_read);
	M6502Close();

	BurnYM3526Init(3000000, NULL, &SynchroniseStream, 0);
	BurnTimerAttachM6502YM3526(1500000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, SynchroniseStream, 375000, firetrap_adpcm_interrupt, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();
	ZetExit();

	BurnYM3526Exit();
	MSM5205Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i] >> 4) & 0x01;
		bit1 = (DrvColPROM[i] >> 5) & 0x01;
		bit2 = (DrvColPROM[i] >> 6) & 0x01;
		bit3 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static inline void draw_16x16_tile(UINT8 *gfx, INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 transparent)
{
	transparent = (transparent) ? 0 : 0xff;

	if (flipy) {
		if (flipx) {
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 8, color, 4, transparent, 0, gfx);
		} else {
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 8, color, 4, transparent, 0, gfx);
		}
	} else {
		if (flipx) {
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 8, color, 4, transparent, 0, gfx);
		} else {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 8, color, 4, transparent, 0, gfx);
		}
	}
}

static void draw_16x16_layer(UINT8 *ram, UINT8 *gfx, UINT8 *scr, INT32 color_base, INT32 transparent)
{
	INT32 scrollx = ((scr[0] + (scr[1] << 8)) & 0x1ff);
	INT32 scrolly = ((scr[2] + (scr[3] << 8)) & 0x1ff);

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 row = (offs / 0x20);
		INT32 col = (offs & 0x1f);

		INT32 sx = col * 16;
		INT32 sy = row * 16;

		sx -= scrollx;
		if (sx < -15) sx += 512;
		sy += scrolly;
		if (sy >= 256) sy -= 512;

		INT32 ofst = ((row & 0x0f) ^ 0x0f) | ((col & 0x0f) << 4) | ((row & 0x10) << 5) | ((col & 0x10) << 6);

		INT32 attr  = ram[ofst + 0x100];
		INT32 code  = ram[ofst] | ((attr & 0x03) << 8);
		INT32 color = (attr & 0x30) >> 4;

		INT32 flipx = attr & 0x08;
		INT32 flipy = attr & 0x04;

		draw_16x16_tile(gfx, code, color+(color_base>>4), sx, sy, flipx, flipy, transparent);
	}
}

static void draw_8x8_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs / 0x20) * 8;
		INT32 sy = (offs % 0x20) * 8;

		INT32 attr  = DrvFgRAM[offs + 0x400];
		INT32 code  = DrvFgRAM[offs] | ((attr & 0x01) << 8);
		INT32 color = attr >> 4;

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, (sy ^ 0xf8) - 8, color, 2, 0, 0, DrvGfxROM0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x180; offs += 4)
	{
		INT32 sy    = DrvSprRAM[offs];
		INT32 sx    = DrvSprRAM[offs + 2];
		INT32 code  = DrvSprRAM[offs + 3] + ((DrvSprRAM[offs + 1] & 0xc0) << 2);
		INT32 color = ((DrvSprRAM[offs + 1] & 0x08) >> 2) | (DrvSprRAM[offs + 1] & 0x01);
		INT32 flipx = DrvSprRAM[offs + 1] & 0x04;
		INT32 flipy = DrvSprRAM[offs + 1] & 0x02;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (DrvSprRAM[offs + 1] & 0x10)    /* double width */
		{
			if (flipscreen) sy -= 16;

			draw_16x16_tile(DrvGfxROM3, code & ~1, color+0x4, sx,       flipy ? sy : sy + 16, flipx, flipy, 1);
			draw_16x16_tile(DrvGfxROM3, code |  1, color+0x4, sx,       flipy ? sy + 16 : sy, flipx, flipy, 1);
			draw_16x16_tile(DrvGfxROM3, code & ~1, color+0x4, sx - 256, flipy ? sy : sy + 16, flipx, flipy, 1);
			draw_16x16_tile(DrvGfxROM3, code |  1, color+0x4, sx - 256, flipy ? sy + 16 : sy, flipx, flipy, 1);
		}
		else
		{
			draw_16x16_tile(DrvGfxROM3, code, color+0x4, sx, sy, flipx, flipy, 1);
			draw_16x16_tile(DrvGfxROM3, code, color+0x4, sx-256, sy, flipx, flipy, 1);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_16x16_layer(DrvBg1RAM, DrvGfxROM2, scroll + 4, 0xc0, 0); // bg2
	draw_16x16_layer(DrvBg0RAM, DrvGfxROM1, scroll + 0, 0x80, 1); // bg1
	draw_sprites();
	draw_8x8_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		DrvInputs[3] = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		{ // coin derp
			static UINT8 lastcoin = 0;
			if (DrvInputs[3] && lastcoin != DrvInputs[3]) {
				if (!i8751_current_command) {
					i8751_return = (DrvInputs[3]&1) ? 1 : 2;
					ZetOpen(0);
					ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
					ZetClose();
				}
			}
			lastcoin = DrvInputs[3];
		}
	}

	INT32 nInterleave = MSM5205CalcInterleave(0, 1500000);
	INT32 nCyclesTotal[2] = { 6000000 / 60, 1500000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetOpen(0);
	M6502Open(0);
	DrvInputs[2] &= 0x7f; // vblank

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (nmi_enable && (i == nInterleave - 1)) ZetNmi();

		BurnTimerUpdateYM3526((i + 1) * (nCyclesTotal[1] / nInterleave));

		if (i == (nInterleave - 2)) DrvInputs[2] |= 0x80; // vblank

		MSM5205Update();
	}

	BurnTimerEndFrameYM3526(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	M6502Close();
	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		M6502Scan(nAction);

		M6502Open(0);
		BurnYM3526Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);
		M6502Close();

		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(sound_irq_enable);
		SCAN_VAR(msm5205next);
		SCAN_VAR(adpcm_toggle);

		SCAN_VAR(i8751_current_command);
		SCAN_VAR(i8751_return);
		SCAN_VAR(i8751_init_ptr);

		SCAN_VAR(coin_command_pending);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch1(banks[0]);
		ZetClose();

		M6502Open(0);
		bankswitch2(banks[1]);
		M6502Close();
	}

	return 0;
}


// Fire Trap (US, set 1)

static struct BurnRomInfo firetrapRomDesc[] = {
	{ "di-02.4a",		0x8000, 0x3d1e4bf7, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code
	{ "di-01.3a",		0x8000, 0x9bbae38b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "di-00.2a",		0x8000, 0xd0dad7de, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "di-17.10j",		0x8000, 0x8605f6b9, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU Code
	{ "di-18.12j",		0x8000, 0x49508c93, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "di-12.16h",		0x1000, 0x00000000, 3 | BRF_NODUMP },        //  5 MCU (undumped)

	{ "di-03.17c",		0x2000, 0x46721930, 4 | BRF_GRA },           //  6 Characters

	{ "di-06.3e",		0x8000, 0x441d9154, 5 | BRF_GRA },           //  7 Background Tiles
	{ "di-04.2e",		0x8000, 0x8e6e7eec, 5 | BRF_GRA },           //  8
	{ "di-07.6e",		0x8000, 0xef0a7e23, 5 | BRF_GRA },           //  9
	{ "di-05.4e",		0x8000, 0xec080082, 5 | BRF_GRA },           // 10

	{ "di-09.3j",		0x8000, 0xd11e28e8, 6 | BRF_GRA },           // 11 Background Tiles
	{ "di-08.2j",		0x8000, 0xc32a21d8, 6 | BRF_GRA },           // 12
	{ "di-11.6j",		0x8000, 0x6424d5c3, 6 | BRF_GRA },           // 13
	{ "di-10.4j",		0x8000, 0x9b89300a, 6 | BRF_GRA },           // 14

	{ "di-16.17h",		0x8000, 0x0de055d7, 7 | BRF_GRA },           // 15 Sprites
	{ "di-13.13h",		0x8000, 0x869219da, 7 | BRF_GRA },           // 16
	{ "di-14.14h",		0x8000, 0x6b65812e, 7 | BRF_GRA },           // 17
	{ "di-15.15h",		0x8000, 0x3e27f77d, 7 | BRF_GRA },           // 18

	{ "firetrap.3b",	0x0100, 0x8bb45337, 8 | BRF_GRA },           // 19 Color data
	{ "firetrap.4b",	0x0100, 0xd5abfc64, 8 | BRF_GRA },           // 20

	{ "firetrap.1a",	0x0100, 0xd67f3514, 0 | BRF_OPT },           // 21
};

STD_ROM_PICK(firetrap)
STD_ROM_FN(firetrap)

struct BurnDriver BurnDrvFiretrap = {
	"firetrap", NULL, NULL, NULL, "1986",
	"Fire Trap (US, set 1)\0", NULL, "Wood Place Inc. (Data East USA license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 4, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, firetrapRomInfo, firetrapRomName, NULL, NULL, FiretrapInputInfo, FiretrapDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 256, 3, 4
};


// Fire Trap (US, set 2)

static struct BurnRomInfo firetrapaRomDesc[] = {
	{ "di-02.4a",		0x8000, 0x3d1e4bf7, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code
	{ "di-01.3a",		0x8000, 0x9bbae38b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "di-00-a.2a.bin",	0x8000, 0xf39e2cf4, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "di-17.10j",		0x8000, 0x8605f6b9, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU Code
	{ "di-18.12j",		0x8000, 0x49508c93, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "di-12.16h",		0x1000, 0x00000000, 3 | BRF_NODUMP },        //  5 MCU (undumped)

	{ "di-03.17c",		0x2000, 0x46721930, 4 | BRF_GRA },           //  6 Characters

	{ "di-06.3e",		0x8000, 0x441d9154, 5 | BRF_GRA },           //  7 Background Tiles
	{ "di-04.2e",		0x8000, 0x8e6e7eec, 5 | BRF_GRA },           //  8
	{ "di-07.6e",		0x8000, 0xef0a7e23, 5 | BRF_GRA },           //  9
	{ "di-05.4e",		0x8000, 0xec080082, 5 | BRF_GRA },           // 10

	{ "di-09.3j",		0x8000, 0xd11e28e8, 6 | BRF_GRA },           // 11 Background Tiles
	{ "di-08.2j",		0x8000, 0xc32a21d8, 6 | BRF_GRA },           // 12
	{ "di-11.6j",		0x8000, 0x6424d5c3, 6 | BRF_GRA },           // 13
	{ "di-10.4j",		0x8000, 0x9b89300a, 6 | BRF_GRA },           // 14

	{ "di-16.17h",		0x8000, 0x0de055d7, 7 | BRF_GRA },           // 15 Sprites
	{ "di-13.13h",		0x8000, 0x869219da, 7 | BRF_GRA },           // 16
	{ "di-14.14h",		0x8000, 0x6b65812e, 7 | BRF_GRA },           // 17
	{ "di-15.15h",		0x8000, 0x3e27f77d, 7 | BRF_GRA },           // 18

	{ "firetrap.3b",	0x0100, 0x8bb45337, 8 | BRF_GRA },           // 19 Color data
	{ "firetrap.4b",	0x0100, 0xd5abfc64, 8 | BRF_GRA },           // 20

	{ "firetrap.1a",	0x0100, 0xd67f3514, 0 | BRF_OPT },           // 21
};

STD_ROM_PICK(firetrapa)
STD_ROM_FN(firetrapa)

struct BurnDriver BurnDrvFiretrapa = {
	"firetrapa", "firetrap", NULL, NULL, "1986",
	"Fire Trap (US, set 2)\0", NULL, "Wood Place Inc. (Data East USA license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 4, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, firetrapaRomInfo, firetrapaRomName, NULL, NULL, FiretrapInputInfo, FiretrapDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 256, 3, 4
};


// Fire Trap (Japan)

static struct BurnRomInfo firetrapjRomDesc[] = {
	{ "fi-03.4a",		0x8000, 0x20b2a4ff, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code
	{ "fi-02.3a",		0x8000, 0x5c8a0562, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fi-01.2a",		0x8000, 0xf2412fe8, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "fi-18.10j",		0x8000, 0x8605f6b9, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU Code
	{ "fi-19.12j",		0x8000, 0x49508c93, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "fi-13.16h",		0x1000, 0x00000000, 3 | BRF_NODUMP },        //  5 MCU (undumped)

	{ "fi-04.17c",		0x2000, 0xa584fc16, 4 | BRF_GRA },           //  6 Characters

	{ "fi-06.3e",		0x8000, 0x441d9154, 5 | BRF_GRA },           //  7 Background Tiles
	{ "fi-05.2e",		0x8000, 0x8e6e7eec, 5 | BRF_GRA },           //  8
	{ "fi-08.6e",		0x8000, 0xef0a7e23, 5 | BRF_GRA },           //  9
	{ "fi-07.4e",		0x8000, 0xec080082, 5 | BRF_GRA },           // 10

	{ "fi-10.3j",		0x8000, 0xd11e28e8, 6 | BRF_GRA },           // 11 Background Tiles
	{ "fi-09.2j",		0x8000, 0xc32a21d8, 6 | BRF_GRA },           // 12
	{ "fi-12.6j",		0x8000, 0x6424d5c3, 6 | BRF_GRA },           // 13
	{ "fi-11.4j",		0x8000, 0x9b89300a, 6 | BRF_GRA },           // 14

	{ "fi-17.17h",		0x8000, 0x0de055d7, 7 | BRF_GRA },           // 15 Sprites
	{ "fi-14.13h",		0x8000, 0xdbcdd3df, 7 | BRF_GRA },           // 16
	{ "fi-15.14h",		0x8000, 0x6b65812e, 7 | BRF_GRA },           // 17
	{ "fi-16.15h",		0x8000, 0x3e27f77d, 7 | BRF_GRA },           // 18

	{ "fi-2.3b",		0x0100, 0x8bb45337, 8 | BRF_GRA },           // 19 Color data
	{ "fi-3.4b",		0x0100, 0xd5abfc64, 8 | BRF_GRA },           // 20

	{ "fi-1.1a",		0x0100, 0xd67f3514, 0 | BRF_OPT },           // 21
};

STD_ROM_PICK(firetrapj)
STD_ROM_FN(firetrapj)

struct BurnDriver BurnDrvFiretrapj = {
	"firetrapj", "firetrap", NULL, NULL, "1986",
	"Fire Trap (Japan)\0", NULL, "Wood Place Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 4, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, firetrapjRomInfo, firetrapjRomName, NULL, NULL, FiretrapInputInfo, FiretrapjDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 256, 3, 4
};


// Fire Trap (Japan bootleg)

static struct BurnRomInfo firetrapblRomDesc[] = {
	{ "ft0d.bin",		0x8000, 0x793ef849, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code
	{ "fi-02.3a",		0x8000, 0x5c8a0562, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fi-01.2a",		0x8000, 0xf2412fe8, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "fi-18.10j",		0x8000, 0x8605f6b9, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU Code
	{ "fi-19.12j",		0x8000, 0x49508c93, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "ft0a.bin",		0x8000, 0x613313ee, 1 | BRF_PRG | BRF_ESS }, //  5 Main CPU Extra

	{ "fi-04.17c",		0x2000, 0xa584fc16, 4 | BRF_GRA },           //  6 Characters

	{ "fi-06.3e",		0x8000, 0x441d9154, 5 | BRF_GRA },           //  7 Background Tiles
	{ "fi-05.2e",		0x8000, 0x8e6e7eec, 5 | BRF_GRA },           //  8
	{ "fi-08.6e",		0x8000, 0xef0a7e23, 5 | BRF_GRA },           //  9
	{ "fi-07.4e",		0x8000, 0xec080082, 5 | BRF_GRA },           // 10

	{ "fi-10.3j",		0x8000, 0xd11e28e8, 6 | BRF_GRA },           // 11 Background Tiles
	{ "fi-09.2j",		0x8000, 0xc32a21d8, 6 | BRF_GRA },           // 12
	{ "fi-12.6j",		0x8000, 0x6424d5c3, 6 | BRF_GRA },           // 13
	{ "fi-11.4j",		0x8000, 0x9b89300a, 6 | BRF_GRA },           // 14

	{ "fi-17.17h",		0x8000, 0x0de055d7, 7 | BRF_GRA },           // 15 Sprites
	{ "fi-14.13h",		0x8000, 0x869219da, 7 | BRF_GRA },           // 16
	{ "fi-15.14h",		0x8000, 0x6b65812e, 7 | BRF_GRA },           // 17
	{ "fi-16.15h",		0x8000, 0x3e27f77d, 7 | BRF_GRA },           // 18

	{ "fi-2.3b",		0x0100, 0x8bb45337, 8 | BRF_GRA },           // 19 Color data
	{ "fi-3.4b",		0x0100, 0xd5abfc64, 8 | BRF_GRA },           // 20

	{ "fi-1.1a",		0x0100, 0xd67f3514, 0 | BRF_OPT },           // 21
};

STD_ROM_PICK(firetrapbl)
STD_ROM_FN(firetrapbl)

struct BurnDriver BurnDrvFiretrapbl = {
	"firetrapbl", "firetrap", NULL, NULL, "1986",
	"Fire Trap (Japan bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG, 4, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, firetrapblRomInfo, firetrapblRomName, NULL, NULL, FiretrapblInputInfo, FiretrapblDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 256, 3, 4
};
