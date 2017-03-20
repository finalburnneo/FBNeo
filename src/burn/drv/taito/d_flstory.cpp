// FB Alpha Fairyland story driver module
// Based on MAME driver by Nicola Salmoria

// Todo: victnine

#include "tiles_generic.h"
#include "taito_m68705.h"
#include "z80_intf.h"
#include "driver.h"
#include "dac.h"
#include "msm5232.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvMcuROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvMcuRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[3];

static UINT8 snd_data;
static UINT8 snd_flag;
static INT32 nmi_enable;
static INT32 pending_nmi;
static INT32 char_bank = 0;
static UINT8 m_gfxctrl;
static INT32 mcu_select;

static UINT8  DrvJoy1[8];
static UINT8  DrvJoy2[8];
static UINT8  DrvJoy3[8];
static UINT8  DrvJoy4[8];
static UINT8  DrvJoy5[8];
static UINT8  DrvDips[3];
static UINT8  DrvInputs[5];
static UINT8  DrvReset;

static UINT8 *flipscreen;
static UINT8 *soundlatch;

static INT32 select_game;

static INT32 m_vol_ctrl[16];
static UINT8 m_snd_ctrl0;
static UINT8 m_snd_ctrl1;
static UINT8 m_snd_ctrl2;
static UINT8 m_mcu_cmd;
static UINT8 m_mcu_counter;
static UINT8 m_mcu_b4_cmd;
static UINT8 m_mcu_param;
static UINT8 m_mcu_b2_res;
static UINT8 m_mcu_b1_res;
static UINT8 m_mcu_bb_res;
static UINT8 m_mcu_b5_res;
static UINT8 m_mcu_b6_res;

static struct BurnInputInfo FlstoryInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Flstory)

static struct BurnInputInfo RumbaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Rumba)

static struct BurnDIPInfo RumbaDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfc, NULL		},
	{0x14, 0xff, 0xff, 0x00, NULL		},
	{0x15, 0xff, 0xff, 0xe3, NULL		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x03, 0x00, "20000 50000"		},
	{0x13, 0x01, 0x03, 0x01, "10000 60000"		},
	{0x13, 0x01, 0x03, 0x02, "10000 40000"		},
	{0x13, 0x01, 0x03, 0x03, "10000 20000"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x13, 0x01, 0x04, 0x04, "Off"		},
	{0x13, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x18, 0x18, "3"		},
	{0x13, 0x01, 0x18, 0x10, "4"		},
	{0x13, 0x01, 0x18, 0x08, "5"		},
	{0x13, 0x01, 0x18, 0x00, "6"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x13, 0x01, 0x20, 0x20, "Off"		},
	{0x13, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x13, 0x01, 0x40, 0x40, "Off"		},
	{0x13, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x80, 0x00, "Upright"		},
	{0x13, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x14, 0x01, 0x0f, 0x0f, "9 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x0e, "8 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x0d, "7 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x0c, "6 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x0b, "5 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x0a, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x09, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"		},
	{0x14, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"		},
	{0x14, 0x01, 0x0f, 0x07, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x14, 0x01, 0xf0, 0xf0, "9 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0xe0, "8 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0xd0, "7 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0xc0, "6 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0xa0, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x90, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"		},
	{0x14, 0x01, 0xf0, 0x60, "1 Coin  7 Credits"		},
	{0x14, 0x01, 0xf0, 0x70, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    2, "Training Stage"		},
	{0x15, 0x01, 0x01, 0x00, "Off"		},
	{0x15, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x15, 0x01, 0x02, 0x02, "Off"		},
	{0x15, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x15, 0x01, 0x04, 0x04, "Japanese"		},
	{0x15, 0x01, 0x04, 0x00, "English"		},

	{0   , 0xfe, 0   ,    2, "Attract Sound"		},
	{0x15, 0x01, 0x08, 0x08, "Off"		},
	{0x15, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Coinage Display"		},
	{0x15, 0x01, 0x10, 0x10, "Off"		},
	{0x15, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Copyright String"		},
	{0x15, 0x01, 0x20, 0x20, "Taito Corp. MCMLXXXIV"		},
	{0x15, 0x01, 0x20, 0x00, "Taito Corporation"		},

	{0   , 0xfe, 0   ,    2, "Infinite Lives"		},
	{0x15, 0x01, 0x40, 0x40, "Off"		},
	{0x15, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x15, 0x01, 0x80, 0x80, "Off"		},
	{0x15, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Rumba)

static struct BurnDIPInfo FlstoryDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xec, NULL				},
	{0x10, 0xff, 0xff, 0x00, NULL				},
	{0x11, 0xff, 0xff, 0xf8, NULL				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x0f, 0x01, 0x03, 0x00, "30000 100000"			},
	{0x0f, 0x01, 0x03, 0x01, "30000 150000"			},
	{0x0f, 0x01, 0x03, 0x02, "50000 150000"			},
	{0x0f, 0x01, 0x03, 0x03, "70000 150000"			},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x0f, 0x01, 0x04, 0x04, "Off"				},
	{0x0f, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0f, 0x01, 0x18, 0x08, "3"				},
	{0x0f, 0x01, 0x18, 0x10, "4"				},
	{0x0f, 0x01, 0x18, 0x18, "5"				},
	{0x0f, 0x01, 0x18, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Debug Mode"			},
	{0x0f, 0x01, 0x20, 0x20, "Off"				},
	{0x0f, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0f, 0x01, 0x40, 0x40, "Off"				},
	{0x0f, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0f, 0x01, 0x80, 0x80, "Upright"			},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,   16, "Coin A"			},
	{0x10, 0x01, 0x0f, 0x0f, "9 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x0e, "8 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x0d, "7 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x0c, "6 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x0b, "5 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x0a, "4 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x09, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x10, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"		},
	{0x10, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"		},
	{0x10, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"		},
	{0x10, 0x01, 0x0f, 0x07, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,   16, "Coin B"			},
	{0x10, 0x01, 0xf0, 0xf0, "9 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0xe0, "8 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0xd0, "7 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0xc0, "6 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0xa0, "4 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0x90, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x10, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
	{0x10, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"		},
	{0x10, 0x01, 0xf0, 0x60, "1 Coin  7 Credits"		},
	{0x10, 0x01, 0xf0, 0x70, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x11, 0x01, 0x08, 0x00, "No"				},
	{0x11, 0x01, 0x08, 0x08, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Attract Animation"		},
	{0x11, 0x01, 0x10, 0x00, "Off"				},
	{0x11, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Leave Off"			},
	{0x11, 0x01, 0x20, 0x20, "Off"				},
	{0x11, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x11, 0x01, 0x40, 0x40, "Off"				},
	{0x11, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x11, 0x01, 0x80, 0x00, "1"				},
	{0x11, 0x01, 0x80, 0x80, "2"				},
};

STDDIPINFO(Flstory)

static struct BurnInputInfo Onna34roInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Onna34ro)

static struct BurnDIPInfo Onna34roDIPList[]=
{
	{0x13, 0xff, 0xff, 0xc0, NULL		},
	{0x14, 0xff, 0xff, 0x00, NULL		},
	{0x15, 0xff, 0xff, 0x80, NULL		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x03, 0x00, "200000 200000"		},
	{0x13, 0x01, 0x03, 0x01, "200000 300000"		},
	{0x13, 0x01, 0x03, 0x02, "100000 200000"		},
	{0x13, 0x01, 0x03, 0x03, "200000 100000"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x13, 0x01, 0x04, 0x00, "Off"		},
	{0x13, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x18, 0x10, "1"		},
	{0x13, 0x01, 0x18, 0x08, "2"		},
	{0x13, 0x01, 0x18, 0x00, "3"		},
	{0x13, 0x01, 0x18, 0x18, "Endless (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x13, 0x01, 0x20, 0x20, "Off"		},
	{0x13, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x40, 0x40, "Off"		},
	{0x13, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x80, 0x80, "Upright"		},
	{0x13, 0x01, 0x80, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x14, 0x01, 0x0f, 0x0f, "9 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x0e, "8 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x0d, "7 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x0c, "6 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x0b, "5 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x0a, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x09, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"		},
	{0x14, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"		},
	{0x14, 0x01, 0x0f, 0x07, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x14, 0x01, 0xf0, 0xf0, "9 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0xe0, "8 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0xd0, "7 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0xc0, "6 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0xa0, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x90, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"		},
	{0x14, 0x01, 0xf0, 0x60, "1 Coin  7 Credits"		},
	{0x14, 0x01, 0xf0, 0x70, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"		},
	{0x15, 0x01, 0x01, 0x00, "Off"		},
	{0x15, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    2, "Rack Test"		},
	{0x15, 0x01, 0x02, 0x00, "Off"		},
	{0x15, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x15, 0x01, 0x04, 0x04, "Off"		},
	{0x15, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x15, 0x01, 0x08, 0x00, "Off"		},
	{0x15, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    2, "Coinage Display"		},
	{0x15, 0x01, 0x10, 0x10, "Off"		},
	{0x15, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x60, 0x20, "Easy"		},
	{0x15, 0x01, 0x60, 0x00, "Normal"		},
	{0x15, 0x01, 0x60, 0x40, "Difficult"		},
	{0x15, 0x01, 0x60, 0x60, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x15, 0x01, 0x80, 0x80, "A and B"		},
	{0x15, 0x01, 0x80, 0x00, "A only"		},
};

STDDIPINFO(Onna34ro)

static struct BurnInputInfo VictnineInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy4 + 5,	"p1 fire 6"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy5 + 2,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy5 + 3,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy5 + 4,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 fire 6"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Victnine)

static struct BurnDIPInfo VictnineDIPList[]=
{
	{0x1b, 0xff, 0xff, 0x67, NULL			},
	{0x1c, 0xff, 0xff, 0x00, NULL			},
	{0x1d, 0xff, 0xff, 0x7f, NULL			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x1b, 0x01, 0x04, 0x04, "Off"			},
	{0x1b, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x1b, 0x01, 0x40, 0x40, "Off"			},
	{0x1b, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    3, "Cabinet"		},
	{0x1b, 0x01, 0xa0, 0x20, "Upright"		},
	{0x1b, 0x01, 0xa0, 0xa0, "Cocktail"		},
	{0x1b, 0x01, 0xa0, 0x00, "MA / MB"		},

	{0   , 0xfe, 0   ,   16, "Coin A"		},
	{0x1c, 0x01, 0x0f, 0x0f, "9 Coins 1 Credits"	},
	{0x1c, 0x01, 0x0f, 0x0e, "8 Coins 1 Credits"	},
	{0x1c, 0x01, 0x0f, 0x0d, "7 Coins 1 Credits"	},
	{0x1c, 0x01, 0x0f, 0x0c, "6 Coins 1 Credits"	},
	{0x1c, 0x01, 0x0f, 0x0b, "5 Coins 1 Credits"	},
	{0x1c, 0x01, 0x0f, 0x0a, "4 Coins 1 Credits"	},
	{0x1c, 0x01, 0x0f, 0x09, "3 Coins 1 Credits"	},
	{0x1c, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x1c, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x1c, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x1c, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x1c, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x1c, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x1c, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x1c, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"	},
	{0x1c, 0x01, 0x0f, 0x07, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,   16, "Coin B"		},
	{0x1c, 0x01, 0xf0, 0xf0, "9 Coins 1 Credits"	},
	{0x1c, 0x01, 0xf0, 0xe0, "8 Coins 1 Credits"	},
	{0x1c, 0x01, 0xf0, 0xd0, "7 Coins 1 Credits"	},
	{0x1c, 0x01, 0xf0, 0xc0, "6 Coins 1 Credits"	},
	{0x1c, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"	},
	{0x1c, 0x01, 0xf0, 0xa0, "4 Coins 1 Credits"	},
	{0x1c, 0x01, 0xf0, 0x90, "3 Coins 1 Credits"	},
	{0x1c, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x1c, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"	},
	{0x1c, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"	},
	{0x1c, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"	},
	{0x1c, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},
	{0x1c, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"	},
	{0x1c, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x1c, 0x01, 0xf0, 0x60, "1 Coin  7 Credits"	},
	{0x1c, 0x01, 0xf0, 0x70, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    2, "Coinage Display"	},
	{0x1d, 0x01, 0x10, 0x00, "Off"			},
	{0x1d, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Show Year"		},
	{0x1d, 0x01, 0x20, 0x00, "Off"			},
	{0x1d, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "No hit"		},
	{0x1d, 0x01, 0x40, 0x40, "Off"			},
	{0x1d, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x1d, 0x01, 0x80, 0x80, "A and B"		},
	{0x1d, 0x01, 0x80, 0x00, "A only"		},
};

STDDIPINFO(Victnine)

static void gfxctrl_write(INT32 data)
{
	m_gfxctrl = data;

	INT32 bank = (data & 0x20) << 3;

	ZetMapArea(0xdd00, 0xddff, 0, DrvPalRAM + 0x000 + bank);
	ZetMapArea(0xdd00, 0xddff, 1, DrvPalRAM + 0x000 + bank);
	ZetMapArea(0xde00, 0xdeff, 0, DrvPalRAM + 0x200 + bank);
	ZetMapArea(0xde00, 0xdeff, 1, DrvPalRAM + 0x200 + bank);

	if (select_game == 3) {
		char_bank = 0;
		return;
	}

	char_bank = (data & 0x10) >> 4;

	if (data & 4) *flipscreen = (~data & 0x01);
}

static UINT8 rumba_mcu_read()
{
	if((m_mcu_cmd & 0xf0) == 0x00) // end packet cmd, value returned is meaningless (probably used for main <-> mcu comms syncronization)
		return 0;

	switch(m_mcu_cmd)
	{
		case 0x73: return 0xa4; //initial MCU check
		case 0x33: return m_mcu_b2_res; //0xb2 result
		case 0x31: return m_mcu_b1_res; //0xb1 result

		case 0x35: m_mcu_b5_res = 1; m_mcu_b6_res = 1; return 0;
		case 0x36: return m_mcu_b4_cmd; //0xb4 command, extra protection for lives (first play only), otherwise game gives one extra life at start-up (!)
		case 0x37: return m_mcu_b5_res; //0xb4 / 0xb5 / 0xb6 result y value
		case 0x38: return m_mcu_b6_res; //x value

		case 0x3b: return m_mcu_bb_res; //0xbb result
		case 0x40: return 0;
		case 0x41: return 0;
		case 0x42:
		{
			return 0;
		}
	}

	return 0;
}

static void rumba_mcu_write(UINT8 data)
{
	if(m_mcu_param)
	{
		m_mcu_param = 0; // clear param

		switch(m_mcu_cmd)
		{
			case 0xb0: // counter, used by command 0xb1 (and something else?
			{
				m_mcu_counter = data;
				break;
			}
			case 0xb1: // player death sequence, controls X position
			{
				m_mcu_b1_res = data;

				/* TODO: this is pretty hard to simulate ... */
				if(m_mcu_counter >= 0x10)
					m_mcu_b1_res++; // left
				else if(m_mcu_counter >= 0x08)
					m_mcu_b1_res--; // right
				else
					m_mcu_b1_res++; // left again

				break;
			}
			case 0xb2: // player sprite hook-up param when he throws the wheel
			{
				switch(data)
				{
					case 1: m_mcu_b2_res = 0xaa; break; //left
					case 2: m_mcu_b2_res = 0xaa; break; //right
					case 4: m_mcu_b2_res = 0xab; break; //down
					case 8: m_mcu_b2_res = 0xa9; break; //up
				}
				break;
			}
			case 0xbb: // when you start a level, lives
			{
				m_mcu_bb_res = data;
				break;
			}
			case 0xb4: // when the bird touches the top / bottom / left / right of the screen, for correct repositioning
			{
				m_mcu_b4_cmd = data;
				break;
			}
			case 0xb5: // bird X coord
			{
				m_mcu_b5_res = data;

				if(m_mcu_b4_cmd == 3) // from right to left
					m_mcu_b5_res = 0x0d;

				if(m_mcu_b4_cmd == 2) // from left to right
					m_mcu_b5_res = 0xe4;

				break;
			}
			case 0xb6: // bird Y coord
			{
				m_mcu_b6_res = data;

				if(m_mcu_b4_cmd == 1) // from up to down
					m_mcu_b6_res = 0x04;

				if(m_mcu_b4_cmd == 4) // from down to up
					m_mcu_b6_res = 0xdc;

				break;
			}
		}

		return;
	}

	m_mcu_cmd = data;

	if(((data & 0xf0) == 0xb0 || (data & 0xf0) == 0xc0) && m_mcu_param == 0)
		m_mcu_param = 1;
}

static void onna34ro_mcu_write(INT32 data)
{
	INT32 score_adr = (ZetReadByte(0xe29e) << 8) + ZetReadByte(0xe29d);

	mcu_sent = 1;

	switch (data)
	{
		case 0x0e:
			from_mcu = 0xff;
			break;

		case 0x01:
			from_mcu = 0x6a;
			break;

		case 0x40: if(score_adr >= 0xe000 && score_adr < 0xe800) from_mcu = ZetReadByte(score_adr);
			break;

		case 0x41: if(score_adr >= 0xe000 && score_adr < 0xe800) from_mcu = ZetReadByte(score_adr+1);
			break;

		case 0x42: if(score_adr >= 0xe000 && score_adr < 0xe800) from_mcu = ZetReadByte(score_adr+2) & 0x0f;
			break;

		default:
			from_mcu = 0x80;
	}
}

static const UINT8 victnine_mcu_data[0x100] =
{
	0x3e, 0x08, 0xdd, 0x29, 0xcb, 0x14, 0xfd, 0x29,
	0xcb, 0x15, 0xd9, 0x29, 0xd9, 0x30, 0x0d, 0xd9,
	0x19, 0xd9, 0xdd, 0x09, 0x30, 0x01, 0x24, 0xfd,
	0x19, 0x30, 0x01, 0x2c, 0x3d, 0x20, 0xe3, 0xc9,
	0x11, 0x14, 0x00, 0x19, 0x7e, 0x32, 0xed, 0xe4,
	0x2a, 0x52, 0xe5, 0x22, 0xe9, 0xe4, 0x2a, 0x54,
	0xe5, 0x22, 0xeb, 0xe4, 0x21, 0x2a, 0xe6, 0xfe,
	0x06, 0x38, 0x02, 0xcb, 0xc6, 0xcb, 0xce, 0xc9,
	0x06, 0x00, 0x3a, 0xaa, 0xe4, 0x07, 0x07, 0x07,
	0xb0, 0x47, 0x3a, 0xab, 0xe4, 0x07, 0x07, 0xb0,
	0x47, 0x3a, 0xac, 0xe4, 0xe6, 0x03, 0xb0, 0x21,
	0xe3, 0xe6, 0xc9, 0x38, 0xe1, 0x29, 0x07, 0xc9,
	0x23, 0x7e, 0x47, 0xe6, 0x1f, 0x32, 0x0c, 0xe6,
	0x78, 0xe6, 0xe0, 0x07, 0x07, 0x47, 0xe6, 0x03,
	0x28, 0x06, 0xcb, 0x7a, 0x28, 0x02, 0xc6, 0x02,
	0x32, 0x0a, 0xe6, 0x78, 0xe6, 0x80, 0xc9, 0x3a,
	0x21, 0x29, 0xe5, 0x7e, 0xe6, 0xf8, 0xf6, 0x01,
	0x77, 0x23, 0x3a, 0x0a, 0xe6, 0x77, 0x21, 0x08,
	0xe6, 0xcb, 0xc6, 0xcb, 0x8e, 0x3a, 0x2b, 0xe5,
	0x21, 0xff, 0xe5, 0xfe, 0x02, 0xc9, 0x1f, 0xc6,
	0x47, 0x3a, 0xaa, 0xe4, 0xa7, 0x21, 0x00, 0xe5,
	0x28, 0x03, 0x21, 0x1b, 0xe5, 0x70, 0x3a, 0xaa,
	0xe4, 0xee, 0x01, 0x32, 0xaa, 0xe4, 0x21, 0xb0,
	0xe4, 0x34, 0x23, 0x36, 0x00, 0xc9, 0x2b, 0xb2,
	0xaf, 0x77, 0x12, 0x23, 0x13, 0x3c, 0xfe, 0x09,
	0x20, 0xf7, 0x3e, 0x01, 0x32, 0xad, 0xe4, 0x21,
	0x48, 0xe5, 0xcb, 0xfe, 0xc9, 0x32, 0xe5, 0xaa,
	0x21, 0x00, 0x13, 0xe4, 0x47, 0x1b, 0xa1, 0xc9,
	0x00, 0x08, 0x04, 0x0c, 0x05, 0x0d, 0x06, 0x0e,
	0x22, 0x66, 0xaa, 0x22, 0x33, 0x01, 0x11, 0x88,
	0x06, 0x05, 0x03, 0x04, 0x08, 0x01, 0x03, 0x02,
	0x06, 0x07, 0x02, 0x03, 0x15, 0x17, 0x11, 0x13
};

static void victnine_mcu_write(INT32 data)
{
	INT32 seed = ZetReadByte(0xe685);

	if (!seed && (data & 0x37) == 0x37)
	{
		from_mcu = 0xa6;
	}
	else
	{
		data += seed;

		if ((data & ~0x1f) == 0xa0)
		{
			mcu_select = data & 0x1f;
		}
		else if (data < 0x20)
		{
			INT32 offset = mcu_select * 8 + data;
			from_mcu = victnine_mcu_data[offset];
		}
		else if (data >= 0x38 && data <= 0x3a)
		{
			from_mcu = ZetReadByte(0xe691 - 0x38 + data);
		}
	}
}

void __fastcall flstory_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff00) == 0xdc00) {
		DrvSprRAM[address & 0xff] = data;

		if (((select_game == 2) || (select_game == 3)) && address == 0xdce0) {
			gfxctrl_write(data);
		}

		return;
	}

	switch (address)
	{
		case 0xd000:
			if (select_game == 3) {
				rumba_mcu_write(data);
			} else if (select_game == 2) {
				victnine_mcu_write(data);
			} else if (select_game == 1) {
				onna34ro_mcu_write(data);
			} else {
				standard_taito_mcu_write(data);
			}
		return;

		case 0xd400:
		{
			*soundlatch = data;

			if (nmi_enable) {
				ZetClose();
				ZetOpen(1);
				ZetNmi();
				ZetClose();
				ZetOpen(0);
			} else {
				pending_nmi = 1;
			}
		}
		return;

		case 0xdf03:
			if ((select_game != 2) && (select_game != 3)) gfxctrl_write(data | 0x04);
		return;
	}
}

UINT8 __fastcall flstory_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xd000:
			if (select_game == 1) {
				return from_mcu; }
			else if (select_game == 3) {
				return rumba_mcu_read();
			} else if (select_game == 2) {
				return from_mcu - ZetReadByte(0xe685);
			} else {
				return standard_taito_mcu_read();
			}

		case 0xd400:
			snd_flag = 0;
			return snd_data;

		case 0xd401:
			return snd_flag | 0xfd;

		case 0xd800:
		case 0xd801:
		case 0xd802:
			return DrvDips[address & 3];

		case 0xd803: if (select_game != 3) {
			return DrvInputs[0] & 0x3f;
		} else {
			return DrvInputs[0] ^ 0x30;
		}

		case 0xd804:
			return DrvInputs[1];

		case 0xd805:
		{
			INT32 res = 0;
			if (!main_sent) res |= 0x01;
			if (mcu_sent) res |= 0x02;

			if (select_game == 2) res |= DrvInputs[3];
			if (select_game == 3 || select_game == 1) res = 0x03; // rumba and onna always returns 3
			return res;
		}

		case 0xd806:
			return DrvInputs[2];

		case 0xd807:
			return DrvInputs[4];
		case 0xdce0:
			return m_gfxctrl;
	}

	return 0;
}

static void ta7630_init()
{
	int i;

	double db           = 0.0;
	double db_step      = 1.50; /* 1.50 dB step (at least, maybe more) */
	double db_step_inc  = 0.125;
	for (i = 0; i < 16; i++)
	{
		double max = 100.0 / pow(10.0, db/20.0 );
		m_vol_ctrl[15 - i] = (INT32)max;

		db += db_step;
		db_step += db_step_inc;
	}
/*
  channels 0-2 AY#0
  channels 3,4 MSM5232 group1,group2
*/
}

static void sound_control_0_w(UINT8 data)
{
	double vol;
	m_snd_ctrl0 = data & 0xff;

	vol = m_vol_ctrl[(m_snd_ctrl0 >> 4) & 15] / 100.0;
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_0);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_1);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_2);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_3);
}

static void sound_control_1_w(UINT8 data)
{
	double vol;
	m_snd_ctrl1 = data & 0xff;

	vol = m_vol_ctrl[(m_snd_ctrl1 >> 4) & 15] / 100.0;
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_4);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_5);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_6);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_7);
}

static void AY_ayportA_write(UINT32 /*addr*/, UINT32 data)
{
	if (data == 0xff) return; // ignore ay-init
	m_snd_ctrl2 = data & 0xff;

	AY8910SetAllRoutes(0, m_vol_ctrl[(m_snd_ctrl2 >> 4) & 15] / ((select_game == 3) ? 1600.0 : 2000.0), BURN_SND_ROUTE_BOTH);
}

void __fastcall flstory_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc800:
		case 0xc801:
			AY8910Write(0, address & 1, data);
		return;

		case 0xca00:
		case 0xca01:
		case 0xca02:
		case 0xca03:
		case 0xca04:
		case 0xca05:
		case 0xca06:
		case 0xca07:
		case 0xca08:
		case 0xca09:
		case 0xca0a:
		case 0xca0b:
		case 0xca0c:
		case 0xca0d:
			MSM5232Write(address, data);
		return;

		case 0xcc00:
			sound_control_0_w(data);
			break;

		case 0xce00:
			sound_control_1_w(data);
			break;

		case 0xd800:
			snd_data = data;
			snd_flag = 2;
		return;

		case 0xda00:
			nmi_enable = 1;
			if (pending_nmi)
			{
				ZetNmi();
				pending_nmi = 0;
			}
		return;

		case 0xdc00:
			nmi_enable = 0;
		return;

		case 0xde00:
			DACSignedWrite(0, data);
		return;
	}
}

UINT8 __fastcall flstory_sound_read(UINT16 address)
{
	if (address == 0xd800) {
		return *soundlatch;
	}

	return 0;
}

static INT32 flstoryDACSync()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (4000000.000 / (nBurnFPS / 100.000))));
}

static INT32 DrvDoReset()
{
	DrvReset = 0;
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	m67805_taito_reset();

	ta7630_init();
	AY8910Reset(0);
	MSM5232Reset();

	DACReset();

	snd_data = 0;
	snd_flag = 0;
	nmi_enable = 0;
	pending_nmi = 0;
	char_bank = 0;
	mcu_select = 0;

	// below for Rumba mcu sim
	m_mcu_cmd = 0;
	m_mcu_counter = 0;
	m_mcu_b4_cmd = 0;
	m_mcu_param = 0;
	m_mcu_b2_res = 0;
	m_mcu_b1_res = 0;
	m_mcu_bb_res = 0;
	m_mcu_b5_res = 0;
	m_mcu_b6_res = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x010000;
	DrvZ80ROM1		= Next; Next += 0x010000;

	DrvMcuROM		= Next; Next += 0x000800;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x040000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x001000;
	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000100;

	DrvMcuRAM		= Next; Next += 0x000080;

	soundlatch		= Next; Next += 0x000001;
	flipscreen		= Next; Next += 0x000001;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { RGN_FRAC(((select_game == 3) ? 0x8000 : 0x20000), 1, 2)+0, RGN_FRAC(((select_game == 3) ? 0x8000 : 0x20000), 1, 2)+4, 0x00000, 0x00004 };
	INT32 XOffs[16] = { 3, 2, 1, 0, 8+3, 8+2, 8+1, 8+0, 16*8+3, 16*8+2, 16*8+1, 16*8+0, 16*8+8+3, 16*8+8+2, 16*8+8+1, 16*8+8+0 };
	INT32 YOffs[16] = { 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16, 16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x20000; i++) tmp[i] = DrvGfxROM0[i] ^ 0xff;

	GfxDecode(0x1000, 4,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);
	GfxDecode(0x0400, 4, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
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
		if (select_game == 0) {
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  4, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x04000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x08000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0c000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x10000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x14000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x18000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1c000, 12, 1)) return 1;
	
			if (BurnLoadRom(DrvMcuROM  + 0x00000, 13, 1)) return 1;
		} else if (select_game == 1) {
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  4, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  5, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x06000,  6, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x08000,  7, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x00000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x04000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x08000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0c000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x10000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x14000, 13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x18000, 14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1c000, 15, 1)) return 1;
		} else if (select_game == 2) {
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  4, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x0a000,  5, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  6, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  7, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  8, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x06000,  9, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x08000, 10, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x0a000, 11, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x00000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x02000, 13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x04000, 14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x06000, 15, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x10000, 16, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x12000, 17, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x14000, 18, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x16000, 19, 1)) return 1;
		} else if (select_game == 3) {
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  4, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  5, 1)) return 1;
			// 6 == undumped mcu
			if (BurnLoadRom(DrvGfxROM0 + 0x02000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x00000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x06000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x04000, 10, 1)) return 1;
		}

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xbfff, 0, DrvZ80ROM0);
	ZetMapArea(0x0000, 0xbfff, 2, DrvZ80ROM0);
	ZetMapArea(0xc000, 0xcfff, 0, DrvVidRAM);
	ZetMapArea(0xc000, 0xcfff, 1, DrvVidRAM);
	ZetMapArea(0xc000, 0xcfff, 2, DrvVidRAM);
	ZetMapArea(0xdc00, 0xdcff, 0, DrvSprRAM);
//	ZetMapArea(0xdc00, 0xdcff, 1, DrvSprRAM);
	ZetMapArea(0xdc00, 0xdcff, 2, DrvSprRAM);
	ZetMapArea(0xdd00, 0xddff, 0, DrvPalRAM);
	ZetMapArea(0xdd00, 0xddff, 1, DrvPalRAM);
	ZetMapArea(0xdd00, 0xddff, 2, DrvPalRAM);
	ZetMapArea(0xde00, 0xdeff, 0, DrvPalRAM + 0x200);
	ZetMapArea(0xde00, 0xdeff, 1, DrvPalRAM + 0x200);
	ZetMapArea(0xde00, 0xdeff, 2, DrvPalRAM + 0x200);
	ZetMapArea(0xe000, 0xe7ff, 0, DrvZ80RAM0);
	ZetMapArea(0xe000, 0xe7ff, 1, DrvZ80RAM0);
	ZetMapArea(0xe000, 0xe7ff, 2, DrvZ80RAM0);
	ZetSetWriteHandler(flstory_main_write);
	ZetSetReadHandler(flstory_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapArea(0x0000, 0xbfff, 0, DrvZ80ROM1);
	ZetMapArea(0x0000, 0xbfff, 2, DrvZ80ROM1);
	ZetMapArea(0xc000, 0xc7ff, 0, DrvZ80RAM1);
	ZetMapArea(0xc000, 0xc7ff, 1, DrvZ80RAM1);
	ZetMapArea(0xc000, 0xc7ff, 2, DrvZ80RAM1);
	ZetMapArea(0xe000, 0xefff, 0, DrvZ80ROM1 + 0xe000);
	ZetMapArea(0xe000, 0xefff, 2, DrvZ80ROM1 + 0xe000);
	ZetSetWriteHandler(flstory_sound_write);
	ZetSetReadHandler(flstory_sound_read);
	ZetClose();

	m67805_taito_init(DrvMcuROM, DrvMcuRAM, &standard_m68705_interface);

	AY8910Init(0, 2000000, nBurnSoundRate, NULL, NULL, AY_ayportA_write, NULL);
	AY8910SetAllRoutes(0, 0.05, BURN_SND_ROUTE_BOTH);
	if (select_game == 3) {
		AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);
	}

	MSM5232Init(2000000, 1);
	MSM5232SetCapacitors(1.0e-6, 1.0e-6, 1.0e-6, 1.0e-6, 1.0e-6, 1.0e-6, 1.0e-6, 1.0e-6);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_0);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_1);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_2);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_3);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_4);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_5);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_6);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_7);

	DACInit(0, 0, 1, flstoryDACSync);
	DACSetRoute(0, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		m68705_taito_scan(nAction);
		AY8910Scan(nAction, pnMin);
		MSM5232Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(snd_data);
		SCAN_VAR(snd_flag);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(pending_nmi);
		SCAN_VAR(char_bank);
		SCAN_VAR(mcu_select);
		SCAN_VAR(m_snd_ctrl0);
		SCAN_VAR(m_snd_ctrl1);
		SCAN_VAR(m_snd_ctrl2);
		// below for Rumba mcu sim
		SCAN_VAR(m_mcu_cmd);
		SCAN_VAR(m_mcu_counter);
		SCAN_VAR(m_mcu_b4_cmd);
		SCAN_VAR(m_mcu_param);
		SCAN_VAR(m_mcu_b2_res);
		SCAN_VAR(m_mcu_b1_res);
		SCAN_VAR(m_mcu_bb_res);
		SCAN_VAR(m_mcu_b5_res);
		SCAN_VAR(m_mcu_b6_res);

		DrvRecalc = 1;
	}

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	m67805_taito_exit();

	AY8910Exit(0);
	MSM5232Exit();
	DACExit();

	BurnFree (AllMem);

	return 0;
}

static void draw_background_layer(INT32 type, INT32 priority)
{                    //fg      bg      fg      bg
	INT32 masks[4] = { 0x3fff, 0xc000, 0x8000, 0x7fff };
	INT32 mask = masks[type];

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		sy -= DrvSprRAM[0xa0 + (offs & 0x1f)] + 16;
		if (sy < -7) sy += 256;
		if (sy >= nScreenHeight) continue;

		INT32 attr  = DrvVidRAM[offs * 2 + 1];
		INT32 code  = DrvVidRAM[offs * 2 + 0] | ((attr & 0xc0) << 2) | 0x400 | (char_bank * 0x800);

		INT32 flipy =  attr & 0x10;
		INT32 flipx =  attr & 0x08;
		if (select_game == 3) {
			flipx = 0; flipy = 0; // no flipping in rumba
			code &= 0x3ff; // mask code to max_rumba_chars-1
		}

		INT32 color = (attr & 0x0f) << 4;
		INT32 prio  = (attr & 0x20) >> 5;
		if (priority && !prio) continue;

		if (type == 1) { // first background layer
			if (flipy) {
				if (flipx) {
					Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color >> 4, 4, 0, DrvGfxROM0);
				} else {
					Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color >> 4, 4, 0, DrvGfxROM0);
				}
			} else {
				if (flipx) {
					Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color >> 4, 4, 0, DrvGfxROM0);
				} else {
					Render8x8Tile_Clip(pTransDraw, code, sx, sy, color >> 4, 4, 0, DrvGfxROM0);
				}
			}
		}
		else if (type == 6) { // not used (for now)
			if (flipy) {
				if (flipx) {
					Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color >> 4, 4, 15, 0, DrvGfxROM0);
				} else {
					Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color >> 4, 4, 15, 0, DrvGfxROM0);
				}
			} else {
				if (flipx) {
					Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color >> 4, 4, 15, 0, DrvGfxROM0);
				} else {
					Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color >> 4, 4, 15, 0, DrvGfxROM0);
				}
			}
		}
		else
		{
			if (flipy) flipy  = 0x38;
			if (flipx) flipy |= 0x07;
			UINT8 *src = DrvGfxROM0 + (code * 8 * 8);
			UINT16 *dst;

			for (INT32 y = 0; y < 8; y++, sy++) {
				if (sy < 0 || sy >= nScreenHeight) continue;
				dst = pTransDraw + sy * nScreenWidth;

				for (INT32 x = 0; x < 8; x++, sx++) {
					if (sx < 0 || sx >= nScreenWidth) continue;

					INT32 pxl = src[((y << 3) | x) ^ flipy];
					if (mask & (1 << pxl)) continue;

					dst[sx] = pxl | color;
				}

				sx -= 8;
			}
		}
	}
}

static void victnine_draw_background_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		sy -= DrvSprRAM[0xa0 + (offs & 0x1f)] + 16;
		if (sy < -7) sy += 256;
		if (sy >= nScreenHeight) continue;

		INT32 attr  = DrvVidRAM[offs * 2 + 1];
		INT32 code  = DrvVidRAM[offs * 2 + 0] | ((attr & 0x38) << 5);

		INT32 flipy = attr & 0x80;
		INT32 flipx = attr & 0x40;
		INT32 color = attr & 0x07;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			}
		}

	}
}

static void draw_sprites(INT32 pri, INT32 type)
{
	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 pr = DrvSprRAM[0xa0-1 -i];
		INT32 offs = (pr & 0x1f) * 4;

		if ((pr & 0x80) == pri || type)
		{
			INT32 sy    = DrvSprRAM[offs + 0];
			INT32 attr  = DrvSprRAM[offs + 1];
			INT32 code  = DrvSprRAM[offs + 2];
			if (type) {
				code += (attr & 0x20) << 3;
			} else {
				code += ((attr & 0x30) << 4);
			}

			INT32 sx    = DrvSprRAM[offs + 3];
			INT32 flipx = attr & 0x40;
			INT32 flipy = attr & 0x80;

			if (*flipscreen)
			{
				sx = (240 - sx) & 0xff;
				sy = sy - 1;

				flipx ^= 0x40;
				flipx ^= 0x80;
			}
			else
				sy = 240 - sy - 1;

			sy -= 16;

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, attr & 0x0f, 4, 15, 0x100, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, attr & 0x0f, 4, 15, 0x100, DrvGfxROM1);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, attr & 0x0f, 4, 15, 0x100, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, attr & 0x0f, 4, 15, 0x100, DrvGfxROM1);
				}
			}

			if (sx > 240) {
				if (flipy) {
					if (flipx) {
						Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx - 256, sy, attr & 0x0f, 4, 15, 0x100, DrvGfxROM1);
					} else {
						Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx - 256, sy, attr & 0x0f, 4, 15, 0x100, DrvGfxROM1);
					}
				} else {
					if (flipx) {
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx - 256, sy, attr & 0x0f, 4, 15, 0x100, DrvGfxROM1);
					} else {
						Render16x16Tile_Mask_Clip(pTransDraw, code, sx - 256, sy, attr & 0x0f, 4, 15, 0x100, DrvGfxROM1);
					}
				}
			}
		}
	}
}

static inline void DrvRecalcPalette()
{
	UINT8 r,g,b;
	for (INT32 i = 0; i < 0x200; i++) {
		INT32 d = DrvPalRAM[i] | (DrvPalRAM[i + 0x200] << 8);
		b = (d >> 8) & 0x0f;
		g = (d >> 4) & 0x0f;
		r = d & 0x0f;
		DrvPalette[i] = BurnHighCol((r << 4) | r, (g << 4) | g, (b << 4) | b, 0);
	}
}
static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
	}
	BurnTransferClear();

	if (nBurnLayer & 1) draw_background_layer(1, 0);
	if (nBurnLayer & 2) draw_background_layer(3, 1);
	if (nSpriteEnable & 1) draw_sprites(0x00, (select_game == 3));
	if (nBurnLayer & 4) draw_background_layer(0, 0);
	if (nSpriteEnable & 2) draw_sprites(0x80, (select_game == 3));
	if (nBurnLayer & 8) draw_background_layer(2, 1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 victnineDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
	}

	victnine_draw_background_layer();
	draw_sprites(0, 1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 5);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[3] = { 5366500 / 60, 4000000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[3]  = { 0, 0, 0 };

	if (select_game == 2 || select_game == 3)
		nCyclesTotal[0] = 4000000 / 60;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		ZetOpen(1);
		nSegment = nCyclesTotal[1] / nInterleave;
		nCyclesDone[1] += ZetRun(nSegment);
		if (i == (nInterleave / 2) - 1) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		if (i == (nInterleave / 1) - 1) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		if (select_game == 0) {
			m6805Open(0);
			nSegment = nCyclesTotal[2] / nInterleave;
			nCyclesDone[2] += m6805Run(nSegment);
			m6805Close();
		}
	}

	ZetOpen(1);

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
		MSM5232Update(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}


// The FairyLand Story

static struct BurnRomInfo flstoryRomDesc[] = {
	{ "cpu-a45.15",		0x4000, 0xf03fc969, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "cpu-a45.16",		0x4000, 0x311aa82e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cpu-a45.17",		0x4000, 0xa2b5d17d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "snd.22",		    0x2000, 0xd58b201d, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu
	{ "snd.23",		    0x2000, 0x25e7fd9d, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "vid-a45.18",		0x4000, 0x6f08f69e, 3 | BRF_GRA }, //  5 gfx1
	{ "vid-a45.06",		0x4000, 0xdc856a75, 3 | BRF_GRA }, //  6
	{ "vid-a45.08",		0x4000, 0xd0b028ca, 3 | BRF_GRA }, //  7
	{ "vid-a45.20",		0x4000, 0x1b0edf34, 3 | BRF_GRA }, //  8
	{ "vid-a45.19",		0x4000, 0x2b572dc9, 3 | BRF_GRA }, //  9
	{ "vid-a45.07",		0x4000, 0xaa4b0762, 3 | BRF_GRA }, // 10
	{ "vid-a45.09",		0x4000, 0x8336be58, 3 | BRF_GRA }, // 11
	{ "vid-a45.21",		0x4000, 0xfc382bd1, 3 | BRF_GRA }, // 12

	{ "a45.mcu",		0x0800, 0x5378253c, 4 | BRF_PRG | BRF_ESS }, // 13 mcu
};

STD_ROM_PICK(flstory)
STD_ROM_FN(flstory)

static INT32 flstoryInit()
{
	select_game = 0;

	return DrvInit();
}

struct BurnDriver BurnDrvFlstory = {
	"flstory", NULL, NULL, NULL, "1985",
	"The FairyLand Story\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, flstoryRomInfo, flstoryRomName, NULL, NULL, FlstoryInputInfo, FlstoryDIPInfo,
	flstoryInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// The FairyLand Story (Japan)

static struct BurnRomInfo flstoryjRomDesc[] = {
	{ "cpu-a45.15",		0x4000, 0xf03fc969, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "cpu-a45.16",		0x4000, 0x311aa82e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cpu-a45.17",		0x4000, 0xa2b5d17d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a45_12.8",		0x2000, 0xd6f593fb, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu
	{ "a45_13.9",		0x2000, 0x451f92f9, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "vid-a45.18",		0x4000, 0x6f08f69e, 3 | BRF_GRA }, //  5 gfx1
	{ "vid-a45.06",		0x4000, 0xdc856a75, 3 | BRF_GRA }, //  6
	{ "vid-a45.08",		0x4000, 0xd0b028ca, 3 | BRF_GRA }, //  7
	{ "vid-a45.20",		0x4000, 0x1b0edf34, 3 | BRF_GRA }, //  8
	{ "vid-a45.19",		0x4000, 0x2b572dc9, 3 | BRF_GRA }, //  9
	{ "vid-a45.07",		0x4000, 0xaa4b0762, 3 | BRF_GRA }, // 10
	{ "vid-a45.09",		0x4000, 0x8336be58, 3 | BRF_GRA }, // 11
	{ "vid-a45.21",		0x4000, 0xfc382bd1, 3 | BRF_GRA }, // 12

	{ "a45.mcu",		0x0800, 0x5378253c, 4 | BRF_PRG | BRF_ESS }, // 13 mcu
};

STD_ROM_PICK(flstoryj)
STD_ROM_FN(flstoryj)

struct BurnDriver BurnDrvFlstoryj = {
	"flstoryj", "flstory", NULL, NULL, "1985",
	"The FairyLand Story (Japan)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, flstoryjRomInfo, flstoryjRomName, NULL, NULL, FlstoryInputInfo, FlstoryDIPInfo,
	flstoryInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Onna Sansirou - Typhoon Gal (set 1)

static struct BurnRomInfo onna34roRomDesc[] = {
	{ "a52-01-1.40c",	0x4000, 0xffddcb02, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "a52-02-1.41c",	0x4000, 0xda97150d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a52-03-1.42c",	0x4000, 0xb9749a53, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a52-12.08s",		0x2000, 0x28f48096, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu
	{ "a52-13.09s",		0x2000, 0x4d3b16f3, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "a52-14.10s",		0x2000, 0x90a6f4e8, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "a52-15.37s",		0x2000, 0x5afc21d0, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "a52-16.38s",		0x2000, 0xccf42aee, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "a52-04.11v",		0x4000, 0x5b126294, 3 | BRF_GRA }, //  8 gfx1
	{ "a52-06.10v",		0x4000, 0x78114721, 3 | BRF_GRA }, //  9
	{ "a52-08.09v",		0x4000, 0x4a293745, 3 | BRF_GRA }, // 10
	{ "a52-10.08v",		0x4000, 0x8be7b4db, 3 | BRF_GRA }, // 11
	{ "a52-05.35v",		0x4000, 0xa1a99588, 3 | BRF_GRA }, // 12
	{ "a52-07.34v",		0x4000, 0x0bf420f2, 3 | BRF_GRA }, // 13
	{ "a52-09.33v",		0x4000, 0x39c543b5, 3 | BRF_GRA }, // 14
	{ "a52-11.32v",		0x4000, 0xd1dda6b3, 3 | BRF_GRA }, // 15

	{ "a52-17.54c",		0x0800, 0x00000000, 4 | BRF_NODUMP }, // 16 cpu2
};

STD_ROM_PICK(onna34ro)
STD_ROM_FN(onna34ro)

static INT32 onna34roInit()
{
	select_game = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvOnna34ro = {
	"onna34ro", NULL, NULL, NULL, "1985",
	"Onna Sansirou - Typhoon Gal (set 1)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, onna34roRomInfo, onna34roRomName, NULL, NULL, Onna34roInputInfo, Onna34roDIPInfo,
	onna34roInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Onna Sansirou - Typhoon Gal (set 2)

static struct BurnRomInfo onna34raRomDesc[] = {
	{ "ry-08.rom",		0x4000, 0xe4587b85, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ry-07.rom",		0x4000, 0x6ffda515, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ry-06.rom",		0x4000, 0x6fefcda8, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a52-12.08s",		0x2000, 0x28f48096, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu
	{ "a52-13.09s",		0x2000, 0x4d3b16f3, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "a52-14.10s",		0x2000, 0x90a6f4e8, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "a52-15.37s",		0x2000, 0x5afc21d0, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "a52-16.38s",		0x2000, 0xccf42aee, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "a52-04.11v",		0x4000, 0x5b126294, 3 | BRF_GRA }, //  8 gfx1
	{ "a52-06.10v",		0x4000, 0x78114721, 3 | BRF_GRA }, //  9
	{ "a52-08.09v",		0x4000, 0x4a293745, 3 | BRF_GRA }, // 10
	{ "a52-10.08v",		0x4000, 0x8be7b4db, 3 | BRF_GRA }, // 11
	{ "a52-05.35v",		0x4000, 0xa1a99588, 3 | BRF_GRA }, // 12
	{ "a52-07.34v",		0x4000, 0x0bf420f2, 3 | BRF_GRA }, // 13
	{ "a52-09.33v",		0x4000, 0x39c543b5, 3 | BRF_GRA }, // 14
	{ "a52-11.32v",		0x4000, 0xd1dda6b3, 3 | BRF_GRA }, // 15

	{ "a52-17.54c",		0x0800, 0x00000000, 4 | BRF_NODUMP }, // 16 cpu2
};

STD_ROM_PICK(onna34ra)
STD_ROM_FN(onna34ra)

struct BurnDriver BurnDrvOnna34ra = {
	"onna34roa", "onna34ro", NULL, NULL, "1985",
	"Onna Sansirou - Typhoon Gal (set 2)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, onna34raRomInfo, onna34raRomName, NULL, NULL, Onna34roInputInfo, Onna34roDIPInfo,
	onna34roInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Victorious Nine

static struct BurnRomInfo victnineRomDesc[] = {
	{ "a16-19.1",		0x2000, 0xdeb7c439, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "a16-20.2",		0x2000, 0x60cdb6ae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a16-21.3",		0x2000, 0x121bea03, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a16-22.4",		0x2000, 0xb20e3027, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a16-23.5",		0x2000, 0x95fe9cb7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "a16-24.6",		0x2000, 0x32b5c155, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "a16-12.8",		0x2000, 0x4b9bff43, 2 | BRF_PRG | BRF_ESS }, //  6 audiocpu
	{ "a16-13.9",		0x2000, 0x355121b9, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "a16-14.10",		0x2000, 0x0f33ef4d, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "a16-15.37",		0x2000, 0xf91d63dc, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "a16-16.38",		0x2000, 0x9395351b, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "a16-17.39",		0x2000, 0x872270b3, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "a16-06-1.7",		0x2000, 0xb708134d, 3 | BRF_GRA }, // 12 gfx1
	{ "a16-07-2.8",		0x2000, 0xcdaf7f83, 3 | BRF_GRA }, // 13
	{ "a16-10.90",		0x2000, 0xe8e42454, 3 | BRF_GRA }, // 14
	{ "a16-11-1.91",	0x2000, 0x1f766661, 3 | BRF_GRA }, // 15
	{ "a16-04.5",		0x2000, 0xb2fae99f, 3 | BRF_GRA }, // 16
	{ "a16-05-1.6",		0x2000, 0x85dfbb6e, 3 | BRF_GRA }, // 17
	{ "a16-08.88",		0x2000, 0x1ddb6466, 3 | BRF_GRA }, // 18
	{ "a16-09-1.89",	0x2000, 0x23d4c43c, 3 | BRF_GRA }, // 19

	{ "a16-18.mcu",		0x0800, 0x00000000, 4 | BRF_NODUMP }, // 20 cpu2
};

STD_ROM_PICK(victnine)
STD_ROM_FN(victnine)

static INT32 victnineInit()
{
	select_game = 2;

	INT32 nRet = DrvInit();
	
	AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
	
	return nRet;
}

struct BurnDriverD BurnDrvVictnine = {
	"victnine", NULL, NULL, NULL, "1984",
	"Victorious Nine\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_TAITO_MISC, GBF_SPORTSMISC, 0,
	NULL, victnineRomInfo, victnineRomName, NULL, NULL, VictnineInputInfo, VictnineDIPInfo,
	victnineInit, DrvExit, DrvFrame, victnineDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};

static INT32 rumbaInit()
{
	select_game = 3;

	INT32 nRet = DrvInit();

	return nRet;
}

// Rumba Lumber

static struct BurnRomInfo rumbaRomDesc[] = {
	{ "a23_01-1.bin",	0x4000, 0x4bea6e18, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "a23_02-1.bin",	0x4000, 0x08f98c6f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a23_03-1.bin",	0x4000, 0xab595427, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a23_08-1.bin",	0x2000, 0xa18eae00, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu
	{ "a23_09.bin",		0x2000, 0xd0a101d3, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "a23_10.bin",		0x2000, 0xf9447bd4, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "a23-11.mc68705p5s",	0x0800, 0x00000000, 3 | BRF_NODUMP }, //  6 mcu

	{ "a23_07.bin",		0x2000, 0xc98fbea6, 4 | BRF_GRA }, //  7 gfx1
	{ "a23_06.bin",		0x2000, 0xbf1e3a7f, 4 | BRF_GRA }, //  8
	{ "a23_05.bin",		0x2000, 0xb40db231, 4 | BRF_GRA }, //  9
	{ "a23_04.bin",		0x2000, 0x1d4f001f, 4 | BRF_GRA }, // 10
};

STD_ROM_PICK(rumba)
STD_ROM_FN(rumba)

struct BurnDriver BurnDrvRumba = {
	"rumba", NULL, NULL, NULL, "1984",
	"Rumba Lumber\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, rumbaRomInfo, rumbaRomName, NULL, NULL, RumbaInputInfo, RumbaDIPInfo,
	rumbaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
