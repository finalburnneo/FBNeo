// FB Alpha Jaleco Mega System 1 driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM0;
static UINT8 *Drv68KROM1;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[4];
static UINT8 *DrvTransTab[4];
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvPrioPROM;
static UINT8 *DrvSprBuf0;
static UINT8 *DrvObjBuf0;
static UINT8 *DrvSprBuf1;
static UINT8 *DrvObjBuf1;
static UINT8 *Drv68KRAM0;
static UINT8 *Drv68KRAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvObjRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvScrRAM[3];
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRegs;
static UINT8 *DrvPrioBitmap;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 scrollx[3];
static UINT16 scrolly[3];
static UINT16 scroll_flag[3];
static UINT16 m_active_layers;
static UINT16 sprite_flag;
static UINT16 sprite_bank;
static UINT16 screen_flag;

static UINT16 input_select;
static UINT16 protection_val;
static UINT8 oki_bank;

static UINT16 soundlatch;
static UINT16 soundlatch2;

static UINT16 mcu_ram[0x10];
static INT32  mcu_hs = 0;
static UINT16 *mcu_config;
static UINT32 mcu_write_address;
static UINT16 mcu_config_type1[3] = { 0xff, 0, 0x889e };
static UINT16 mcu_config_type2[3] = { 0, 0xff, 0x835d };

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[3];

static INT32 system_select = 0;
static INT32 sound_cpu_reset = 0;
static UINT8 input_select_values[5];
static INT32 ignore_oki_status_hack = 1;
static INT32 layer_color_config[4] = { 0, 0x100, 0x200, 0x300 };
static UINT32 m_layers_order[0x10];
static INT32 scroll_factor_8x8[3] = { 1, 1, 1 };
static INT32 tshingen = 0;
static INT32 monkelf = 0;
#ifdef BUILD_A68K
static INT32 oldasmemulation = 0;
#endif

static struct BurnInputInfo CommonInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Common)

static struct BurnInputInfo Common3ButtonInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"	},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"	},
};

STDINPUTINFO(Common3Button)

static struct BurnInputInfo Hayaosi1InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 5"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 5"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p3 start"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 4"	},
	{"P3 Button 5",		BIT_DIGITAL,	DrvJoy2 + 7,	"p3 fire 5"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Hayaosi1)

static struct BurnInputInfo PeekabooInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy1 + 9,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 right"	},
	{"P1 Fire",		    BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Left",		    BIT_DIGITAL,	DrvJoy1 + 11,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 right"	},
	{"P2 Fire",		    BIT_DIGITAL,	DrvJoy1 + 10,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Peekaboo)

static struct BurnDIPInfo P47DIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x03, 0x02, "2"				},
	{0x12, 0x01, 0x03, 0x03, "3"				},
	{0x12, 0x01, 0x03, 0x01, "4"				},
	{0x12, 0x01, 0x03, 0x00, "5"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x30, 0x00, "Easy"				},
	{0x12, 0x01, 0x30, 0x30, "Normal"			},
	{0x12, 0x01, 0x30, 0x20, "Hard"				},
	{0x12, 0x01, 0x30, 0x10, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

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

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x00, "Off"				},
	{0x13, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(P47)

static struct BurnDIPInfo KickoffDIPList[]=
{
	{0x12, 0xff, 0xff, 0xbf, NULL				},
	{0x13, 0xff, 0xff, 0x3f, NULL				},

	{0   , 0xfe, 0   ,    4, "Time"				},
	{0x12, 0x01, 0x03, 0x03, "3'"				},
	{0x12, 0x01, 0x03, 0x02, "4'"				},
	{0x12, 0x01, 0x03, 0x01, "5'"				},
	{0x12, 0x01, 0x03, 0x00, "6'"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x30, 0x30, "Normal"			},
	{0x12, 0x01, 0x30, 0x20, "Hard"				},
	{0x12, 0x01, 0x30, 0x10, "Harder"			},
	{0x12, 0x01, 0x30, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Controls"			},
	{0x12, 0x01, 0x40, 0x40, "Trackball"			},
	{0x12, 0x01, 0x40, 0x00, "Joystick"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x13, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Freeze Screen (Cheat)"	},
	{0x13, 0x01, 0x20, 0x20, "Off"				},
	{0x13, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Text"				},
	{0x13, 0x01, 0x80, 0x80, "Japanese"			},
	{0x13, 0x01, 0x80, 0x00, "English"			},
};

STDDIPINFO(Kickoff)

static struct BurnDIPInfo KazanDIPList[]=
{
	{0x12, 0xff, 0xff, 0xbf, NULL				},
	{0x13, 0xff, 0xff, 0xbf, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x03, 0x03, "2"				},
	{0x12, 0x01, 0x03, 0x01, "3"				},
	{0x12, 0x01, 0x03, 0x02, "4"				},
	{0x12, 0x01, 0x03, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x12, 0x01, 0x04, 0x04, "50k"				},
	{0x12, 0x01, 0x04, 0x00, "200k"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x12, 0x01, 0x08, 0x00, "Off"				},
	{0x12, 0x01, 0x08, 0x08, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x30, 0x30, "Normal"			},
	{0x12, 0x01, 0x30, 0x20, "Hard"				},
	{0x12, 0x01, 0x30, 0x10, "Harder"			},
	{0x12, 0x01, 0x30, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x12, 0x01, 0x40, 0x00, "Upright"			},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x07, 0x04, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x01, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  5 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x38, 0x00, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x08, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Freeze Screen (Cheat)"	},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Kazan)

static struct BurnDIPInfo TshingenDIPList[]=
{
	{0x14, 0xff, 0xff, 0xdd, NULL				},
	{0x15, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x14, 0x01, 0x03, 0x03, "2"				},
	{0x14, 0x01, 0x03, 0x01, "3"				},
	{0x14, 0x01, 0x03, 0x02, "4"				},
	{0x14, 0x01, 0x03, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x14, 0x01, 0x0c, 0x0c, "20k"				},
	{0x14, 0x01, 0x0c, 0x04, "30k"				},
	{0x14, 0x01, 0x0c, 0x08, "40k"				},
	{0x14, 0x01, 0x0c, 0x00, "50k"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x30, 0x30, "Easy"				},
	{0x14, 0x01, 0x30, 0x10, "Normal"			},
	{0x14, 0x01, 0x30, 0x20, "Hard"				},
	{0x14, 0x01, 0x30, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x40, 0x00, "Off"				},
	{0x14, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x80, 0x80, "Off"				},
	{0x14, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x15, 0x01, 0x07, 0x04, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x07, 0x01, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x07, 0x06, "1 Coin  5 Credits"		},
	{0x15, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x15, 0x01, 0x38, 0x00, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x38, 0x10, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x38, 0x08, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x38, 0x30, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x40, 0x00, "Off"				},
	{0x15, 0x01, 0x40, 0x40, "On"				},
};

STDDIPINFO(Tshingen)

static struct BurnDIPInfo AstyanaxDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL				},
	{0x15, 0xff, 0xff, 0xbf, NULL				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x14, 0x01, 0x04, 0x04, "30k 70k 110k then every 30k"	},
	{0x14, 0x01, 0x04, 0x00, "50k 100k then every 40k"	},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x14, 0x01, 0x18, 0x08, "2"				},
	{0x14, 0x01, 0x18, 0x18, "3"				},
	{0x14, 0x01, 0x18, 0x10, "4"				},
	{0x14, 0x01, 0x18, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x14, 0x01, 0x20, 0x20, "Normal"			},
	{0x14, 0x01, 0x20, 0x00, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Swap 1P/2P Controls"		},
	{0x14, 0x01, 0x40, 0x40, "Off"				},
	{0x14, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x80, 0x80, "Off"				},
	{0x14, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x15, 0x01, 0x07, 0x00, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x04, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x15, 0x01, 0x38, 0x00, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x38, 0x10, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x40, 0x40, "Off"				},
	{0x15, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Test Mode"			},
	{0x15, 0x01, 0x80, 0x80, "Off"				},
	{0x15, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Astyanax)

static struct BurnDIPInfo HachooDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x30, 0x30, "Normal"			},
	{0x12, 0x01, 0x30, 0x20, "Hard"				},
	{0x12, 0x01, 0x30, 0x10, "Harder"			},
	{0x12, 0x01, 0x30, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x07, 0x04, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x01, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  5 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x38, 0x00, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x08, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x00, "Off"				},
	{0x13, 0x01, 0x40, 0x40, "On"				},
};

STDDIPINFO(Hachoo)

static struct BurnDIPInfo JitsuproDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL				},
	{0x15, 0xff, 0xff, 0xbf, NULL				},

	{0   , 0xfe, 0   ,    2, "2 Player Innings per Credit"	},
	{0x14, 0x01, 0x01, 0x01, "2"				},
	{0x14, 0x01, 0x01, 0x00, "3"				},

	{0   , 0xfe, 0   ,    2, "Difficulty for Catching Ball"	},
	{0x14, 0x01, 0x02, 0x02, "Normal"			},
	{0x14, 0x01, 0x02, 0x00, "Hard"				},

	{0   , 0xfe, 0   ,   13, "Franchise (Increase Power)"	},
	{0x14, 0x01, 0x3c, 0x3c, "Normal"			},
	{0x14, 0x01, 0x3c, 0x38, "G"				},
	{0x14, 0x01, 0x3c, 0x34, "D"				},
	{0x14, 0x01, 0x3c, 0x30, "C"				},
	{0x14, 0x01, 0x3c, 0x2c, "S"				},
	{0x14, 0x01, 0x3c, 0x28, "W (B)"			},
	{0x14, 0x01, 0x3c, 0x24, "T"				},
	{0x14, 0x01, 0x3c, 0x20, "L"				},
	{0x14, 0x01, 0x3c, 0x1c, "Br (Bw)"			},
	{0x14, 0x01, 0x3c, 0x18, "F"				},
	{0x14, 0x01, 0x3c, 0x14, "H"				},
	{0x14, 0x01, 0x3c, 0x10, "O (M)"			},
	{0x14, 0x01, 0x3c, 0x0c, "Bu"				},

	{0   , 0xfe, 0   ,    2, "Scroll Is Based On"		},
	{0x14, 0x01, 0x40, 0x40, "Shadow of Baseball"		},
	{0x14, 0x01, 0x40, 0x00, "The Baseball Itself"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x80, 0x80, "Off"				},
	{0x14, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x15, 0x01, 0x07, 0x04, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x07, 0x01, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x07, 0x06, "1 Coin  5 Credits"		},
	{0x15, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x15, 0x01, 0x38, 0x00, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x38, 0x10, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x38, 0x08, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x38, 0x30, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x40, 0x40, "Off"				},
	{0x15, 0x01, 0x40, 0x00, "On"				},
};

STDDIPINFO(Jitsupro)

static struct BurnDIPInfo PlusalphDIPList[]=
{
	{0x12, 0xff, 0xff, 0xbf, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x03, 0x03, "3"				},
	{0x12, 0x01, 0x03, 0x02, "4"				},
	{0x12, 0x01, 0x03, 0x01, "5"				},
	{0x12, 0x01, 0x03, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Hyper Laser Beams"		},
	{0x12, 0x01, 0x04, 0x00, "2"				},
	{0x12, 0x01, 0x04, 0x04, "3"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x12, 0x01, 0x08, 0x08, "70k and every 200k"		},
	{0x12, 0x01, 0x08, 0x00, "100k and 300k Only"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x30, 0x00, "Easy"				},
	{0x12, 0x01, 0x30, 0x30, "Normal"			},
	{0x12, 0x01, 0x30, 0x10, "Hard"				},
	{0x12, 0x01, 0x30, 0x20, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x12, 0x01, 0x40, 0x00, "Upright"			},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x07, 0x04, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x01, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  5 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x38, 0x00, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x08, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x00, "Off"				},
	{0x13, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Freeze Screen (Cheat)"	},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Plusalph)

static struct BurnDIPInfo StdragonDIPList[]=
{
	{0x12, 0xff, 0xff, 0xbf, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x03, 0x02, "2"				},
	{0x12, 0x01, 0x03, 0x03, "3"				},
	{0x12, 0x01, 0x03, 0x01, "4"				},
	{0x12, 0x01, 0x03, 0x00, "5"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x30, 0x30, "Easy"				},
	{0x12, 0x01, 0x30, 0x20, "Normal"			},
	{0x12, 0x01, 0x30, 0x10, "Hard"				},
	{0x12, 0x01, 0x30, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x12, 0x01, 0x40, 0x00, "Upright"			},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

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

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x00, "Off"				},
	{0x13, 0x01, 0x40, 0x40, "On"				},
};

STDDIPINFO(Stdragon)

static struct BurnDIPInfo RodlandDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL				},
	{0x13, 0xff, 0xff, 0xbf, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x0c, 0x04, "2"				},
	{0x12, 0x01, 0x0c, 0x0c, "3"				},
	{0x12, 0x01, 0x0c, 0x08, "4"				},
	{0x12, 0x01, 0x0c, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Default episode"		},
	{0x12, 0x01, 0x10, 0x10, "1"				},
	{0x12, 0x01, 0x10, 0x00, "2"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x60, 0x00, "Easy"				},
	{0x12, 0x01, 0x60, 0x60, "Normal"			},
	{0x12, 0x01, 0x60, 0x20, "Hard"				},
	{0x12, 0x01, 0x60, 0x40, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x07, 0x04, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x01, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  5 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x38, 0x00, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x08, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Rodland)

static struct BurnDIPInfo SoldamDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x00, "Easy"				},
	{0x12, 0x01, 0x03, 0x03, "Normal"			},
	{0x12, 0x01, 0x03, 0x02, "Hard"				},
	{0x12, 0x01, 0x03, 0x01, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Games To Play (Vs)"		},
	{0x12, 0x01, 0x0c, 0x00, "1"				},
	{0x12, 0x01, 0x0c, 0x0c, "2"				},
	{0x12, 0x01, 0x0c, 0x08, "3"				},
	{0x12, 0x01, 0x0c, 0x04, "4"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x12, 0x01, 0x10, 0x00, "Off"				},
	{0x12, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Credits To Start (Vs)"	},
	{0x12, 0x01, 0x20, 0x20, "1"				},
	{0x12, 0x01, 0x20, 0x00, "2"				},

	{0   , 0xfe, 0   ,    2, "Credits To Continue (Vs)"	},
	{0x12, 0x01, 0x40, 0x40, "1"				},
	{0x12, 0x01, 0x40, 0x00, "2"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

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

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x00, "Off"				},
	{0x13, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Soldam)

static struct BurnDIPInfo AvspiritDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL				},
	{0x13, 0xff, 0xff, 0xfd, NULL				},

	{0   , 0xfe, 0   ,   11, "Coin A"			},
	{0x12, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   11, "Coin B"			},
	{0x12, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0xf0, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x01, 0x01, "Off"				},
	{0x13, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x02, 0x02, "Off"				},
	{0x13, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x13, 0x01, 0x04, 0x00, "Off"				},
	{0x13, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x18, 0x08, "Easy"				},
	{0x13, 0x01, 0x18, 0x18, "Normal"			},
	{0x13, 0x01, 0x18, 0x10, "Hard"				},
	{0x13, 0x01, 0x18, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x13, 0x01, 0x20, 0x20, "Upright"			},
	{0x13, 0x01, 0x20, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Test Mode"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Avspirit)

static struct BurnDIPInfo EdfDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x12, 0x01, 0x07, 0x04, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x07, 0x01, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x12, 0x01, 0x38, 0x00, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x10, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x38, 0x08, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x40, 0x00, "Off"				},
	{0x12, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "DSW-B bits 2-0"		},
	{0x13, 0x01, 0x07, 0x00, "0"				},
	{0x13, 0x01, 0x07, 0x01, "1"				},
	{0x13, 0x01, 0x07, 0x02, "2"				},
	{0x13, 0x01, 0x07, 0x03, "3"				},
	{0x13, 0x01, 0x07, 0x04, "4"				},
	{0x13, 0x01, 0x07, 0x05, "5"				},
	{0x13, 0x01, 0x07, 0x06, "6"				},
	{0x13, 0x01, 0x07, 0x07, "7"				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x13, 0x01, 0x08, 0x08, "3"				},
	{0x13, 0x01, 0x08, 0x00, "4"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x30, 0x00, "Easy"				},
	{0x13, 0x01, 0x30, 0x30, "Normal"			},
	{0x13, 0x01, 0x30, 0x10, "Hard"				},
	{0x13, 0x01, 0x30, 0x20, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Edf)

static struct BurnDIPInfo Hayaosi1DIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL				},
	{0x17, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,   11, "Coin A"			},
	{0x16, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x16, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x16, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x16, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x16, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x16, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x16, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x16, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x16, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x16, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   11, "Coin B"			},
	{0x16, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"		},
	{0x16, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x16, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x16, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x16, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x16, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x16, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x16, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x16, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x16, 0x01, 0xf0, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x17, 0x01, 0x01, 0x01, "Off"				},
	{0x17, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x17, 0x01, 0x02, 0x00, "Off"				},
	{0x17, 0x01, 0x02, 0x02, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x17, 0x01, 0x18, 0x10, "Easy"				},
	{0x17, 0x01, 0x18, 0x18, "Normal"			},
	{0x17, 0x01, 0x18, 0x08, "Hard"				},
	{0x17, 0x01, 0x18, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Points To Win"		},
	{0x17, 0x01, 0x20, 0x00, "10"				},
	{0x17, 0x01, 0x20, 0x20, "15"				},
};

STDDIPINFO(Hayaosi1)

static struct BurnDIPInfo Street64DIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL				},
	{0x13, 0xff, 0xff, 0xbd, NULL				},

	{0   , 0xfe, 0   ,   11, "Coin A"			},
	{0x12, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   11, "Coin B"			},
	{0x12, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0xf0, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x01, 0x01, "Off"				},
	{0x13, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x02, 0x02, "Off"				},
	{0x13, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x13, 0x01, 0x04, 0x00, "Off"				},
	{0x13, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x18, 0x10, "Easy"				},
	{0x13, 0x01, 0x18, 0x18, "Normal"			},
	{0x13, 0x01, 0x18, 0x08, "Hard"				},
	{0x13, 0x01, 0x18, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x13, 0x01, 0x60, 0x40, "1"				},
	{0x13, 0x01, 0x60, 0x60, "2"				},
	{0x13, 0x01, 0x60, 0x20, "3"				},
	{0x13, 0x01, 0x60, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Street64)

static struct BurnDIPInfo BigstrikDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL				},
	{0x15, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,   11, "Coin A"			},
	{0x14, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x14, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   11, "Coin B"			},
	{0x14, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x14, 0x01, 0xf0, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x15, 0x01, 0x01, 0x01, "Off"				},
	{0x15, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x06, 0x02, "Easy"				},
	{0x15, 0x01, 0x06, 0x06, "Normal"			},
	{0x15, 0x01, 0x06, 0x04, "Hard"				},
	{0x15, 0x01, 0x06, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Time"				},
	{0x15, 0x01, 0x18, 0x00, "Very Short"			},
	{0x15, 0x01, 0x18, 0x10, "Short"			},
	{0x15, 0x01, 0x18, 0x18, "Normal"			},
	{0x15, 0x01, 0x18, 0x08, "Long"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x20, 0x00, "Off"				},
	{0x15, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "1 Credit 2 Play"		},
	{0x15, 0x01, 0x40, 0x40, "Off"				},
	{0x15, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Bigstrik)

static struct BurnDIPInfo ChimerabDIPList[]=
{
	{0x12, 0xff, 0xff, 0xbd, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x01, 0x01, "Off"				},
	{0x12, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x02, 0x02, "Off"				},
	{0x12, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x12, 0x01, 0x04, 0x00, "Off"				},
	{0x12, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x18, 0x10, "Easy"				},
	{0x12, 0x01, 0x18, 0x18, "Normal"			},
	{0x12, 0x01, 0x18, 0x08, "Hard"				},
	{0x12, 0x01, 0x18, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x60, 0x40, "1"				},
	{0x12, 0x01, 0x60, 0x60, "2"				},
	{0x12, 0x01, 0x60, 0x20, "3"				},
	{0x12, 0x01, 0x60, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,   11, "Coin A"			},
	{0x13, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x13, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x13, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   11, "Coin B"			},
	{0x13, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x13, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x13, 0x01, 0xf0, 0x00, "Free Play"			},
};

STDDIPINFO(Chimerab)

static struct BurnDIPInfo CybattlrDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL				},
	{0x13, 0xff, 0xff, 0xbf, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x12, 0x01, 0x07, 0x00, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x01, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x07, 0x02, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x12, 0x01, 0x38, 0x00, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x08, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x38, 0x10, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x12, 0x01, 0x40, 0x40, "Off"				},
	{0x12, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x03, 0x02, "Easy"				},
	{0x13, 0x01, 0x03, 0x03, "Normal"			},
	{0x13, 0x01, 0x03, 0x01, "Hard"				},
	{0x13, 0x01, 0x03, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "Disable Instruction Screen"	},
	{0x13, 0x01, 0x04, 0x04, "Off"				},
	{0x13, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Allow Continue"		},
	{0x13, 0x01, 0x18, 0x00, "Off"				},
	{0x13, 0x01, 0x18, 0x10, "Up to Mission 4"		},
	{0x13, 0x01, 0x18, 0x08, "Not on Final Mission"		},
	{0x13, 0x01, 0x18, 0x18, "On"				},

	{0   , 0xfe, 0   ,    2, "Intro Music"			},
	{0x13, 0x01, 0x20, 0x00, "Off"				},
	{0x13, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Cybattlr)

static struct BurnDIPInfo LomakaiDIPList[]=
{
	{0x12, 0xff, 0xff, 0xbf, NULL				},
	{0x13, 0xff, 0xff, 0xbf, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x03, 0x00, "2"				},
	{0x12, 0x01, 0x03, 0x03, "3"				},
	{0x12, 0x01, 0x03, 0x02, "4"				},
	{0x12, 0x01, 0x03, 0x01, "5"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x30, 0x30, "Easy"				},
	{0x12, 0x01, 0x30, 0x20, "Normal"			},
	{0x12, 0x01, 0x30, 0x10, "Hard"				},
	{0x12, 0x01, 0x30, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x12, 0x01, 0x40, 0x00, "Upright"			},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

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

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Lomakai)

static struct BurnDIPInfo PhantasmDIPList[]=
{
	{0x12, 0xff, 0xff, 0xfd, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x01, 0x01, "Off"				},
	{0x12, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x02, 0x02, "Off"				},
	{0x12, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x12, 0x01, 0x04, 0x00, "Off"				},
	{0x12, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x18, 0x08, "Easy"				},
	{0x12, 0x01, 0x18, 0x18, "Normal"			},
	{0x12, 0x01, 0x18, 0x10, "Hard"				},
	{0x12, 0x01, 0x18, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x12, 0x01, 0x20, 0x20, "Upright"			},
	{0x12, 0x01, 0x20, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Test Mode"			},
	{0x12, 0x01, 0x40, 0x40, "Off"				},
	{0x12, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    11, "Coin A"			},
	{0x13, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x13, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x13, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    11, "Coin B"			},
	{0x13, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x13, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x13, 0x01, 0xf0, 0x00, "Free Play"			},
};

STDDIPINFO(Phantasm)

static struct BurnDIPInfo PeekabooDIPList[]=
{
	{0x0b, 0xff, 0xff, 0xf7, NULL				},
	{0x0c, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0b, 0x01, 0x03, 0x00, "Easy"				},
	{0x0b, 0x01, 0x03, 0x03, "Normal"			},
	{0x0b, 0x01, 0x03, 0x02, "Hard"				},
	{0x0b, 0x01, 0x03, 0x01, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0b, 0x01, 0x04, 0x04, "Off"				},
	{0x0b, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    1, "Movement"			},
	//{0x0a, 0x01, 0x08, 0x08, "Paddles"			},
	{0x0b, 0x01, 0x08, 0x00, "Buttons"			},

	{0   , 0xfe, 0   ,    4, "Nudity"			},
	{0x0b, 0x01, 0x30, 0x30, "Female and Male (Full)"	},
	{0x0b, 0x01, 0x30, 0x20, "Female (Full)"		},
	{0x0b, 0x01, 0x30, 0x10, "Female (Partial)"		},
	{0x0b, 0x01, 0x30, 0x00, "None"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0b, 0x01, 0x40, 0x40, "Upright"			},
	{0x0b, 0x01, 0x40, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Number of controllers"	},
	{0x0b, 0x01, 0x80, 0x80, "1"				},
	{0x0b, 0x01, 0x80, 0x00, "2"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x0c, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x0c, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x0c, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x0c, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x0c, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x0c, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x0c, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x0c, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x0c, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x0c, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x0c, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x0c, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x0c, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x0c, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x0c, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x0c, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0c, 0x01, 0x40, 0x00, "Off"				},
	{0x0c, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0c, 0x01, 0x80, 0x80, "Off"				},
	{0x0c, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Peekaboo)

static UINT8 __fastcall mcu_prot_read_byte(UINT32 address)
{
	return Drv68KROM0[(address & 0x3ffff) ^ 1];
}

static UINT16 __fastcall mcu_prot_read_word(UINT32 address)
{
	if ((UINT32)mcu_hs && (((UINT32)mcu_ram[4] << 6) & 0x3ffc0) == (address & 0x3ffc0))
	{
		return mcu_config[2];
	}

	return *((UINT16*)(Drv68KROM0 + (address & 0x3fffe)));
}

static void __fastcall mcu_prot_write_word(UINT32 address, UINT16 data)
{
	if (address >= mcu_write_address && address <= (mcu_write_address + 9)) {
		mcu_ram[(address & 0xe)/2] = data;

		if ((address & ~1) == (mcu_write_address+8)) {
			if (mcu_ram[0] == mcu_config[0] && mcu_ram[1] == 0x55 && mcu_ram[2] == 0xaa && mcu_ram[3] == mcu_config[1]) {
				mcu_hs = 1;
			} else {
				mcu_hs = 0;
			}
		}
	}
}

static void install_mcu_protection(UINT16 *config, UINT32 address)
{
	mcu_write_address = address;
	mcu_config = config;

	SekOpen(0);
	SekMapHandler(2,		0x00000, 0x3ffff, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(2,	mcu_prot_read_word);
	SekSetReadByteHandler(2,	mcu_prot_read_byte);
	SekSetWriteWordHandler(2,	mcu_prot_write_word);
	SekClose();
}

static inline void megasys_palette_write(INT32 offset)
{
	INT32 r,g,b,p;

	p = *((UINT16*)(DrvPalRAM + (offset & 0x7fe)));

	if (system_select == 0xD)	// system D
	{
		r = ((p >> 11) & 0x1f);
		g = ((p >>  6) & 0x1f);
		b = ((p >>  1) & 0x1f);
	
		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);
	}
	else				// system A, B, C, Z
	{
		r = ((p >> 11) & 0x1e) | ((p >> 3) & 0x01);
		g = ((p >>  7) & 0x1e) | ((p >> 2) & 0x01);
		b = ((p >>  3) & 0x1e) | ((p >> 1) & 0x01);
	
		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);
	}

	DrvPalette[(offset & 0x7fe)/2] = BurnHighCol(r,g,b,0);
}

#define RAM_WRITE_BYTE_MIRRORED(mask)				\
	{                                   			\
		Drv68KRAM0[(address & mask) | 0] = data;	\
		Drv68KRAM0[(address & mask) | 1] = data;	\
		return;						                \
	}							                    \

#define RAM_WRITE_WORD(mask)						\
	{					                            \
		*((UINT16*)(Drv68KRAM0 + (address & mask))) = BURN_ENDIAN_SWAP_INT16(data);	\
		return;								        \
	}									            \

static void __fastcall megasys_palette_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvPalRAM + (address & 0x7fe))) = data;
	megasys_palette_write(address);
}

static void __fastcall megasys_palette_write_byte(UINT32 address, UINT8 data)
{
	DrvPalRAM[(address & 0x7ff)^1] = data;
	megasys_palette_write(address);
}

static void update_video_regs(INT32 offset)
{
	offset &= 0x3fe;

	UINT16 data = *((UINT16*)(DrvVidRegs + offset));

	switch (offset)
	{
		case 0x000:
			m_active_layers = data;
		return;

		case 0x008:
			scrollx[2] = data;
		return;

		case 0x00a:
			scrolly[2] = data;
		return;

		case 0x00c:
			scroll_flag[2] = data;
		return;

		case 0x100:
			sprite_flag = data;
		return;

		case 0x200:
			if ((data & 0x0f) > 0x0d && monkelf) data -= 0x10;
			scrollx[0] = data;
		return;

		case 0x202:
			scrolly[0] = data;
		return;

		case 0x204:
			scroll_flag[0] = data;
		return;

		case 0x208:
			if ((data & 0x0f) > 0x0b && monkelf) data -= 0x10;
			scrollx[1] = data;
		return;

		case 0x20a:
			scrolly[1] = data;
		return;

		case 0x20c:
			scroll_flag[1] = data;
		return;

		case 0x300:
		{
			screen_flag = data;
			sound_cpu_reset = data & 0x10;

			if (sound_cpu_reset) {
				if (system_select == 0) { // system Z
					ZetReset();
				} else {

					SekClose();
					SekOpen(1);
					SekReset();
					SekClose();
					SekOpen(0);
				}
			}
		}
		return;
			
		case 0x308:
		{
			soundlatch = data;

			if (system_select == 0) {	// system Z
				ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
			} else {
				SekClose();
				SekOpen(1);
				SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
				SekClose();
				SekOpen(0);
			}
		}
		return;
	}	
}

static void update_video_regs2(INT32 offset)
{
	offset &= 0xfffe;

	UINT16 data = *((UINT16*)(DrvVidRegs + offset));

	switch (offset)
	{
		case 0x2000:
			scrollx[0] = data;
		return;

		case 0x2002:
			scrolly[0] = data;
		return;

		case 0x2004:
			scroll_flag[0] = data;
		return;

		case 0x2008:
			scrollx[1] = data;
		return;

		case 0x200a:
			scrolly[1] = data;
		return;

		case 0x200c:
			scroll_flag[1] = data;
		return;

		case 0x2100:
			scrollx[2] = data;
		return;

		case 0x2102:
			scrolly[2] = data;
		return;

		case 0x2104:
			scroll_flag[2] = data;
		return;

		case 0x2108:
			sprite_bank = data;
		return;

		case 0x2208:
			m_active_layers = data;
		return;

		case 0x2200:
			sprite_flag = data;
		return;

		case 0x2308:
		{
			screen_flag = data;

			sound_cpu_reset = data & 0x10;

			if (sound_cpu_reset) {
				SekClose();
				SekOpen(1);
				SekReset();
				SekClose();
				SekOpen(0);
			}
		}
		return;
			
		case 0x8000:
		{
			soundlatch = data;
			SekClose();
			SekOpen(1);
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO); // auto?
			SekClose();
			SekOpen(0);
		}
		return;
	}	
}

static UINT8 __fastcall megasys1A_main_read_byte(UINT32 address)
{
	if (address & 0xfff00000) {
		return SekReadByte(address & 0xfffff);
	}

	switch (address)
	{
		case 0x080000:
			return DrvInputs[0] >> 8;

		case 0x080001:
			return DrvInputs[0];

		case 0x080002:
			return DrvInputs[1] >> 8;

		case 0x080003:
			return DrvInputs[1];

		case 0x080004:
			return DrvInputs[2] >> 8;

		case 0x080005:
			return DrvInputs[2];

		case 0x080006:
			return DrvDips[1];

		case 0x080007:
			return DrvDips[0];

		case 0x080008:
			return soundlatch2 >> 8;

		case 0x080009:
			return soundlatch2;
	}

	return 0;
}

static UINT16 __fastcall megasys1A_main_read_word(UINT32 address)
{
	if (address & 0xfff00000) {
		return SekReadWord(address & 0xfffff);
	}

	switch (address)
	{
		case 0x080000:
			return DrvInputs[0];

		case 0x080002:
			return DrvInputs[1];

		case 0x080004:
			return DrvInputs[2];

		case 0x080006:
			return (DrvDips[1] << 8) | (DrvDips[0] << 0);

		case 0x080008:
			return soundlatch2;
	}

	return 0;
}

static void __fastcall megasys1A_main_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0xf0000 && address <= 0xfffff) {
		RAM_WRITE_BYTE_MIRRORED(0xffff)
		return;
	}

	if (address & 0xfff00000) {
		SekWriteByte(address & 0xfffff, data);
		return;
	}

	if ((address & 0xffc00) == 0x084000) {
		DrvVidRegs[(address & 0x3ff)^1] = data;
		update_video_regs(address);
		return;
	}
}

static void __fastcall megasys1A_main_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0xf0000 && address <= 0xfffff) {
		RAM_WRITE_WORD(0xffff)
		return;
	}

	if (address & 0xfff00000) {
		SekWriteWord(address & 0xfffff, data);
		return;
	}

	if ((address & 0xffc00) == 0x084000) {
		*((UINT16*)(DrvVidRegs + (address & 0x3fe))) = data;
		update_video_regs(address);
		return;
	}
}

static UINT16 input_protection_read()
{
	int i;

	if ((input_select & 0xf0) == 0xf0) return 0x000D;

	for (i = 0; i < 5; i++)
		if (input_select == input_select_values[i]) break;

	switch (i)
	{
		case 0:
		case 1:
		case 2: return DrvInputs[i];
		case 3:
		case 4: return DrvDips[i-3];
	}

	return 0x0006;
}

static UINT8 __fastcall megasys1B_main_read_byte(UINT32 address)
{
	if (address & 0xf00000) {
		return SekReadByte(address & 0xfffff);
	}

	switch (address)
	{
		case 0x0e0000:
			return input_protection_read() >> 8;

		case 0x0e0001:
			return input_protection_read();
	}

	return 0xff;
}

static UINT16 __fastcall megasys1B_main_read_word(UINT32 address)
{
	if (address & 0xf00000) {
		return SekReadWord(address & 0xfffff);
	}

	switch (address)
	{
		case 0x0e0000:
			return input_protection_read();
	}

	return 0xffff;
}

static void __fastcall megasys1B_main_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0x60000 && address <= 0x7ffff) {
		RAM_WRITE_BYTE_MIRRORED(0x1ffff)
		return;
	}

	if (address & 0xf00000) {
		SekWriteByte(address & 0xfffff, data);
		return;
	}

	if ((address & 0xffc00) == 0x044000) {
		DrvVidRegs[(address & 0x3ff)^1] = data;
		update_video_regs(address);
		return;
	}

	switch (address)
	{
		case 0x0e0000:
		case 0x0e0001:
			input_select = data;
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		return;

		case 0x0e000e: // edf bootleg..
		case 0x0e000f:
			MSM6295Command(0, data);
		return;
	}
}

static void __fastcall megasys1B_main_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x60000 && address <= 0x7ffff) {
		RAM_WRITE_WORD(0x1ffff)
		return;
	}

	if (address & 0xf00000) {
		SekWriteWord(address & 0xfffff, data);
		return;
	}

	if ((address & 0xffc00) == 0x044000) {
		*((UINT16*)(DrvVidRegs + (address & 0x3fe))) = data;
		update_video_regs(address);
		return;
	}

	switch (address)
	{
		case 0x0e0000:
			input_select = data;
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		return;

		case 0x0e000e: // edf bootleg..
		case 0x0e000f:
			MSM6295Command(0, data);
		return;
	}
}

static UINT8 __fastcall megasys1C_main_read_byte(UINT32 address)
{
	if (address & 0xffe00000) {
		return SekReadByte(address & 0x1fffff);
	}

	switch (address)
	{
		case 0x0d8000:
			return input_protection_read() >> 8;

		case 0x0d8001:
			return input_protection_read();
	}

	return 0;
}

static UINT16 __fastcall megasys1C_main_read_word(UINT32 address)
{
	if (address & 0xffe00000) {
		return SekReadWord(address & 0x1fffff);
	}

	switch (address)
	{
		case 0x0d8000:
			return input_protection_read();
	}

	return 0;
}

static void __fastcall megasys1C_main_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0x1c0000 && address <= 0x1fffff) {
		RAM_WRITE_BYTE_MIRRORED(0xffff)
		return;
	}

	if (address & 0xffe00000) {
		SekWriteByte(address & 0x1fffff, data);
		return;
	}

	if ((address & 0x1f0000) == 0x0c0000) {
		DrvVidRegs[(address & 0xffff)^1] = data;
		update_video_regs2(address);
		return;
	}

	switch (address)
	{
		case 0x0d8000:
		case 0x0d8001:
			input_select = data;
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		return;
	}
}

static void __fastcall megasys1C_main_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x1c0000 && address <= 0x1fffff) {
		RAM_WRITE_WORD(0xffff)
		return;
	}

	if (address & 0xffe00000) {
		SekWriteWord(address & 0x1fffff, data);
		return;
	}

	if ((address & 0x1f0000) == 0x0c0000) {
		*((UINT16*)(DrvVidRegs + (address & 0xfffe))) = data;
		update_video_regs2(address);
		return;
	}

	switch (address)
	{
		case 0x0d8000:
			input_select = data;
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		return;
	}
}

static UINT16 peekaboo_prot_read()
{
	switch (protection_val)
	{
		case 0x02:  return 0x03;
		case 0x51:  return DrvInputs[0];
		case 0x52:  return DrvInputs[1];
	}

	return protection_val;
}

static void peekaboo_prot_write(INT32 data)
{
	protection_val = data;

	if ((protection_val & 0x90) == 0x90)
	{
		INT32 bank = (protection_val + 1) & 0x07;
		if (oki_bank != bank) {
			oki_bank = bank;
			memcpy (DrvSndROM0 + 0x20000, DrvSndROM1 + bank * 0x20000, 0x20000);
		}
	}

	SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
//	hack - for some reason, setting irq line 4 isn't working, so simulate
//	what is suppose to be doing.
//	000520: 33FC FFFF 001F 000A        move.w  #$ffff, $1f000a.l
//	000528: 4E73                       rte
	*((UINT16*)(Drv68KRAM0 + 0x000a)) = 0xffff;
}

static void __fastcall megasys1D_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0x1f0000) == 0x0c0000) {
		*((UINT16*)(DrvVidRegs + (address & 0xfffe))) = data;
		update_video_regs2(address);
		return;
	}

	switch (address)
	{
		case 0x0f8000:
			MSM6295Command(0, data & 0xff);
		return;

		case 0x100000:
			peekaboo_prot_write(data);
		return;
	}
}

static void __fastcall megasys1D_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0x1f0000) == 0x0c0000) {
		DrvVidRegs[(address & 0xffff)^1] = data;
		update_video_regs2(address);
		return;
	}
}

static UINT16 __fastcall megasys1D_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x0e0000:
			return (DrvDips[1]<<8)|(DrvDips[0]);

		case 0x0f0000:
			return DrvInputs[0];

		case 0x0f8000:
			return MSM6295ReadStatus(0);

		case 0x100000:
			return peekaboo_prot_read();
	}

	return 0;
}

static UINT8 __fastcall megasys1D_main_read_byte(UINT32 /*address*/)
{
	return 0;
}

static UINT8 __fastcall megasys_sound_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x040000:
		case 0x060000:
			return soundlatch >> 8;

		case 0x060001:
		case 0x040001:
			return soundlatch;

		case 0x080000:
		case 0x080001:
		case 0x080002:
		case 0x080003:
			return BurnYM2151ReadStatus();

		case 0x0a0000:
		case 0x0a0001:
			return (ignore_oki_status_hack) ? 0 : MSM6295ReadStatus(0);

		case 0x0c0000:
		case 0x0c0001:
			return (ignore_oki_status_hack) ? 0 : MSM6295ReadStatus(1);
	}

	return 0;
}

static UINT16 __fastcall megasys_sound_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x040000:
		case 0x060000:
			return soundlatch;

		case 0x080002:
			return BurnYM2151ReadStatus();

		case 0x0a0000:
		case 0x0a0001:
			return (ignore_oki_status_hack) ? 0 : MSM6295ReadStatus(0);

		case 0x0c0000:
		case 0x0c0001:
			return (ignore_oki_status_hack) ? 0 : MSM6295ReadStatus(1);
	}

	return 0;
}

static void __fastcall megasys_sound_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x040000:
		case 0x060000:
		case 0x040001:
		case 0x060001:
			soundlatch2 = data;
		return;

		case 0x080000:
		case 0x080001:
			BurnYM2151SelectRegister(data);
		return;

		case 0x080002:
		case 0x080003:
			BurnYM2151WriteRegister(data);
		return;

		case 0x0a0000:
		case 0x0a0001:
		case 0x0a0002:
		case 0x0a0003:
			MSM6295Command(0, data);
		return;

		case 0x0c0000:
		case 0x0c0001:
		case 0x0c0002:
		case 0x0c0003:
			MSM6295Command(1, data);
		return;
	}
}

static void __fastcall megasys_sound_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x040000:
		case 0x060000:
			soundlatch2 = data;
			*((UINT16*)(DrvVidRegs + 0x8000)) = data;
		return;

		case 0x080000:
			BurnYM2151SelectRegister(data);
		return;

		case 0x080002:
			BurnYM2151WriteRegister(data);
		return;

		case 0x0a0000:
		case 0x0a0001:
		case 0x0a0002:
		case 0x0a0003:
			MSM6295Command(0, data);
		return;

		case 0x0c0000:
		case 0x0c0001:
		case 0x0c0002:
		case 0x0c0003:
			MSM6295Command(1, data);
		return;
	}
}

static void __fastcall megasys1z_sound_write(UINT16 /*address*/, UINT8 /*data*/)
{

}

static UINT8 __fastcall megasys1z_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			return soundlatch;
	}

	return 0;
}

static void __fastcall megasys1z_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM2203Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall megasys1z_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM2203Read(0, port & 1);
	}

	return 0;
}

static void DrvYM2151IrqHandler(INT32 nStatus)
{
	if (nStatus) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
}

static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 3000000;
}

inline static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 3000000.0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	if (system_select == 0) { // system Z
		ZetOpen(0);
		ZetReset();
		ZetClose();

		BurnYM2203Reset();
	} else {
		SekOpen(1);
		SekReset();
		SekClose();

		MSM6295Reset(0);
		MSM6295Reset(1);
		BurnYM2151Reset();
	}

	for (INT32 i = 0; i < 3; i++) {
		scrollx[i] = 0;
		scrolly[i] = 0;
		scroll_flag[i] = 0;
	}

	memset (mcu_ram, 0, sizeof(mcu_ram));
	mcu_hs = 0;

	m_active_layers = 0;
	sprite_flag = 0;
	sprite_bank = 0;
	screen_flag = 0;

	input_select = 0;
	protection_val = 0;

	soundlatch = 0;
	soundlatch2 = 0;

	oki_bank = 0xff;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM0	= Next; Next += 0x080000;
	DrvZ80ROM	= Next;
	Drv68KROM1	= Next; Next += 0x020000;
 
	DrvGfxROM[0]	= Next; Next += 0x100000;
	DrvGfxROM[1]	= Next; Next += 0x100000;
	DrvGfxROM[2]	= Next; Next += 0x100000;
	DrvGfxROM[3]	= Next; Next += 0x200000;

	DrvTransTab[0]	= Next; Next += 0x100000 / (8 * 8);
	DrvTransTab[1]	= Next; Next += 0x100000 / (8 * 8);
	DrvTransTab[2]	= Next; Next += 0x100000 / (8 * 8);
	DrvTransTab[3]	= Next; Next += 0x200000 / (16 * 16);

	MSM6295ROM	= Next;
	DrvSndROM0	= Next; Next += 0x100000;
	DrvSndROM1	= Next; Next += 0x100000;

	DrvPrioPROM	= Next; Next += 0x000200;

	DrvPrioBitmap	= Next; Next += 256 * 256;

	DrvSprBuf0	= Next; Next += 0x002000;
	DrvObjBuf0	= Next; Next += 0x002000;
	DrvSprBuf1	= Next; Next += 0x002000;
	DrvObjBuf1	= Next; Next += 0x002000;

	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM0	= Next; Next += 0x020000;
	Drv68KRAM1	= Next; Next += 0x020000;

	DrvZ80RAM	= Next; Next += 0x000800;

	DrvPalRAM	= Next; Next += 0x000800;
	DrvObjRAM	= Next; Next += 0x002000;

	DrvScrRAM[0]	= Next; Next += 0x004000;
	DrvScrRAM[1]	= Next; Next += 0x004000;
	DrvScrRAM[2]	= Next; Next += 0x004000;

	DrvVidRegs	= Next; Next += 0x010000;

	DrvSprRAM	= Drv68KRAM0 + 0x8000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 gfx, INT32 nLen, INT32 nType)
{
	INT32 Plane[4]  = { STEP4(0,1) };
	INT32 XOffs[16] = { STEP8(0,4), STEP8(512,4) };
	INT32 YOffs[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(nLen);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[gfx], nLen);

	if (nType == 0) {	// 8x8
		GfxDecode(((nLen*8)/4)/(8 * 8), 4,  8,  8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM[gfx]);
	} else {		// 16x16
		GfxDecode(((nLen*8)/4)/(16*16), 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM[gfx]);
	}

	BurnFree (tmp);

	INT32 size = (nType) ? 16 : 8;

	memset (DrvTransTab[gfx], 1, nLen / (size * size));

	for (INT32 i = 0; i < nLen; i++) {
		if (DrvGfxROM[gfx][i] != 0xff) {
			DrvTransTab[gfx][i/(size*size)] = 0;
			i = (i|((size*size)-1))+1;
		}
	}

	return 0;
}

static void DrvPriorityDecode()
{
	const UINT8 *color_prom = DrvPrioPROM;

	for (INT32 pri_code = 0; pri_code < 0x10 ; pri_code++)    // 16 priority codes
	{
		INT32 layers_order[2];    // 2 layers orders (split sprites on/off)

		for (INT32 offset = 0; offset < 2; offset ++)
		{
			INT32 enable_mask = 0xf;  // start with every layer enabled

			layers_order[offset] = 0xfffff;

			do
			{
				INT32 top = color_prom[pri_code * 0x20 + offset + enable_mask * 2] & 3;   // this must be the top layer
				INT32 top_mask = 1 << top;

				INT32 result = 0;     // result of the feasibility check for this layer

				for (INT32 i = 0; i < 0x10 ; i++) // every combination of opaque and transparent pens
				{
					int opacity =   i & enable_mask;    // only consider active layers
					int layer   =   color_prom[pri_code * 0x20 + offset + opacity * 2];

					if (opacity)
					{
						if (opacity & top_mask)
						{
							if (layer != top )  result |= 1;    // error: opaque pens aren't always opaque!
						}
						else
						{
							if (layer == top)   result |= 2;    // transparent pen is opaque
							else                result |= 4;    // transparent pen is transparent
						}
					}
				}

				layers_order[offset] = ( (layers_order[offset] << 4) | top ) & 0xfffff;
				enable_mask &= ~top_mask;

				if (result & 1)
				{
					layers_order[offset] = 0xfffff;
					break;
				}

				if  ((result & 6) == 6)
				{
					layers_order[offset] = 0xfffff;
					break;
				}

				if (result == 2) enable_mask = 0; // totally opaque top layer

			}   while (enable_mask);
		}

		INT32 order = 0xfffff;

		for (INT32 i = 5; i > 0 ; )   // 5 layers to write
		{
			INT32 layer;
			INT32 layer0 = layers_order[0] & 0x0f;
			INT32 layer1 = layers_order[1] & 0x0f;

			if (layer0 != 3)    // 0,1,2 or f
			{
				if (layer1 == 3)
				{
					layer = 4;
					layers_order[0] <<= 4;  // layer1 won't change next loop
				}
				else
				{
					layer = layer0;
					if (layer0 != layer1)
					{
						order = 0xfffff;
						break;
					}
				}
			}
			else    // layer0 = 3;
			{
				if (layer1 == 3)
				{
					layer = 0x43;           // 4 must always be present
					order <<= 4;
					i --;                   // 2 layers written at once
				}
				else
				{
					layer = 3;
					layers_order[1] <<= 4;  // layer1 won't change next loop
				}
			}

			order = (order << 4 ) | layer;

			i--;       // layer written

			layers_order[0] >>= 4;
			layers_order[1] >>= 4;
		}

		m_layers_order[pri_code] = order & 0xfffff;
	}
}

static void phantasm_rom_decode()
{
#define BITSWAP_0 BITSWAP16(x,0xd,0xe,0xf,0x0,0x1,0x8,0x9,0xa,0xb,0xc,0x5,0x6,0x7,0x2,0x3,0x4)
#define BITSWAP_1 BITSWAP16(x,0xf,0xd,0xb,0x9,0x7,0x5,0x3,0x1,0xe,0xc,0xa,0x8,0x6,0x4,0x2,0x0)
#define BITSWAP_2 BITSWAP16(x,0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0xb,0xa,0x9,0x8,0xf,0xe,0xd,0xc)

	UINT16 *prg = (UINT16*)Drv68KROM0;

	for (INT32 i = 0 ; i < 0x40000 / 2; i++)
	{
		UINT16 x,y;

		x = prg[i];

		if      (i < 0x08000/2) { if ((i | (0x248/2)) != i) { y = BITSWAP_0; } else { y = BITSWAP_1; } }
		else if (i < 0x10000/2) { y = BITSWAP_2; }
		else if (i < 0x18000/2) { if ((i | (0x248/2)) != i) { y = BITSWAP_0; } else { y = BITSWAP_1; } }
		else if (i < 0x20000/2) { y = BITSWAP_1; }
		else                    { y = BITSWAP_2; }

		prg[i] = y;
	}

#undef BITSWAP_0
#undef BITSWAP_1
#undef BITSWAP_2
}

static void astyanax_rom_decode()
{
#define BITSWAP_0 BITSWAP16(x,0xd,0xe,0xf,0x0,0xa,0x9,0x8,0x1,0x6,0x5,0xc,0xb,0x7,0x2,0x3,0x4)
#define BITSWAP_1 BITSWAP16(x,0xf,0xd,0xb,0x9,0x7,0x5,0x3,0x1,0x8,0xa,0xc,0xe,0x0,0x2,0x4,0x6)
#define BITSWAP_2 BITSWAP16(x,0x4,0x5,0x6,0x7,0x0,0x1,0x2,0x3,0xb,0xa,0x9,0x8,0xf,0xe,0xd,0xc)

	UINT16 *prg = (UINT16*)Drv68KROM0;

	for (INT32 i = 0 ; i < 0x40000 / 2; i++)
	{
		UINT16 x,y;

		x = prg[i];

		if      (i < 0x08000/2) { if ((i | (0x248/2)) != i) {y = BITSWAP_0;} else {y = BITSWAP_1;} }
		else if (i < 0x10000/2) { y = BITSWAP_2; }
		else if (i < 0x18000/2) { if ((i | (0x248/2)) != i) {y = BITSWAP_0;} else {y = BITSWAP_1;} }
		else if (i < 0x20000/2) { y = BITSWAP_1; }
		else                    { y = BITSWAP_2; }

		prg[i] = y;
	}

#undef BITSWAP_0
#undef BITSWAP_1
#undef BITSWAP_2
}

static void rodland_rom_decode()
{
#define BITSWAP_0 BITSWAP16(x,0xd,0x0,0xa,0x9,0x6,0xe,0xb,0xf,0x5,0xc,0x7,0x2,0x3,0x8,0x1,0x4);
#define BITSWAP_1 BITSWAP16(x,0x4,0x5,0x6,0x7,0x0,0x1,0x2,0x3,0xb,0xa,0x9,0x8,0xf,0xe,0xd,0xc);
#define BITSWAP_2 BITSWAP16(x,0xf,0xd,0xb,0x9,0xc,0xe,0x0,0x7,0x5,0x3,0x1,0x8,0xa,0x2,0x4,0x6);
#define BITSWAP_3 BITSWAP16(x,0x4,0x5,0x1,0x2,0xe,0xd,0x3,0xb,0xa,0x9,0x6,0x7,0x0,0x8,0xf,0xc);

	UINT16 *prg = (UINT16*)Drv68KROM0;

	for (INT32 i = 0 ; i < 0x40000 / 2; i++)
	{
		UINT16 x,y;

		x = prg[i];

		if      (i < 0x08000/2) { if ((i | (0x248/2)) != i) { y = BITSWAP_0; } else { y = BITSWAP_1; } }
		else if (i < 0x10000/2) { if ((i | (0x248/2)) != i) { y = BITSWAP_2; } else { y = BITSWAP_3; } }
		else if (i < 0x18000/2) { if ((i | (0x248/2)) != i) { y = BITSWAP_0; } else { y = BITSWAP_1; } }
		else if (i < 0x20000/2) { y = BITSWAP_1; }
		else                    { y = BITSWAP_3; }

		prg[i] = y;
	}

#undef BITSWAP_0
#undef BITSWAP_1
#undef BITSWAP_2
#undef BITSWAP_3
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *Prg0Load = Drv68KROM0;
	UINT8 *Prg1Load = Drv68KROM1;
	UINT8 *Gfx0Load = DrvGfxROM[0];
	UINT8 *Gfx1Load = DrvGfxROM[1];
	UINT8 *Gfx2Load = DrvGfxROM[2];
	UINT8 *Gfx3Load = DrvGfxROM[3];
	UINT8 *Snd0Load = DrvSndROM0;
	UINT8 *Snd1Load = DrvSndROM1;
	UINT8 *PromLoad = DrvPrioPROM;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if (ri.nType & BRF_NODUMP) continue;

		if ((ri.nType & 0x0f) == 1) {
			if (BurnLoadRom(Prg0Load + 1, i + 0, 2)) return 1;
			if (BurnLoadRom(Prg0Load + 0, i + 1, 2)) return 1;
			Prg0Load += ri.nLen * 2; i++;
		}

		if ((ri.nType & 0x0f) == 2) {
			if (BurnLoadRom(Prg1Load + 1, i + 0, 2)) return 1;
			if (BurnLoadRom(Prg1Load + 0, i + 1, 2)) return 1;
			Prg1Load += ri.nLen * 2; i++;
		}

		if ((ri.nType & 0x0f) == 3) {
			if (BurnLoadRom(Gfx0Load, i, 1)) return 1;
			Gfx0Load += ri.nLen;
		}

		if ((ri.nType & 0x0f) == 4) {
			if (BurnLoadRom(Gfx1Load, i, 1)) return 1;
			Gfx1Load += ri.nLen;
		}

		if ((ri.nType & 0x0f) == 5) {
			if (BurnLoadRom(Gfx2Load, i, 1)) return 1;
			Gfx2Load += ri.nLen;
		}

		if ((ri.nType & 0x0f) == 6) {
			if (BurnLoadRom(Gfx3Load, i, 1)) return 1;
			Gfx3Load += ri.nLen;
		}

		if ((ri.nType & 0x0f) == 7) {
			if (BurnLoadRom(Snd0Load, i, 1)) return 1;
			Snd0Load += ri.nLen;
		}

		if ((ri.nType & 0x0f) == 8) {
			if (BurnLoadRom(Snd1Load, i, 1)) return 1;
			Snd1Load += ri.nLen;
		}

		if ((ri.nType & 0x0f) == 9) {
			if (BurnLoadRom(PromLoad, i, 1)) return 1;
			PromLoad += ri.nLen;
		}

		if ((ri.nType & 0x0f) == 10) {
			if (BurnLoadRom(Prg1Load, i, 1)) return 1;
			Prg1Load += ri.nLen;
		}
	}

	if ((PromLoad - DrvPrioPROM) != 0) {
		DrvPriorityDecode();
	}

	return 0;
}

static INT32 SystemInit(INT32 nSystem, void (*pRomLoadCallback)())
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (DrvLoadRoms()) return 1;

		if (pRomLoadCallback) {
			pRomLoadCallback();
		}

		DrvGfxDecode(0, 0x080000, 0);
		DrvGfxDecode(1, 0x080000, 0);
		DrvGfxDecode(2, 0x080000, 0);
		DrvGfxDecode(3, 0x100000, 1);
	}

	system_select = nSystem;

	switch (system_select)
	{
		case 0x0: // system Z
		{
			SekInit(0, 0x68000);
			SekOpen(0);
			SekMapMemory(Drv68KROM0,		0x000000, 0x05ffff, MAP_ROM);
			SekMapMemory(DrvVidRegs,		0x084000, 0x0843ff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(DrvPalRAM,			0x088000, 0x0887ff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(DrvObjRAM,			0x08e000, 0x08ffff, MAP_RAM);
			SekMapMemory(DrvScrRAM[0],		0x090000, 0x093fff, MAP_RAM);
			SekMapMemory(DrvScrRAM[1],		0x094000, 0x097fff, MAP_RAM);
			SekMapMemory(DrvScrRAM[2],		0x098000, 0x09bfff, MAP_RAM);
			SekMapMemory(Drv68KRAM0,		0x0f0000, 0x0fffff, MAP_RAM);
			SekSetReadWordHandler(0,		megasys1A_main_read_word);
			SekSetReadByteHandler(0,		megasys1A_main_read_byte);
			SekSetWriteWordHandler(0,		megasys1A_main_write_word);
			SekSetWriteByteHandler(0,		megasys1A_main_write_byte);

			SekMapHandler(1,			0x088000, 0x0887ff, MAP_WRITE);
			SekSetWriteWordHandler(1,		megasys_palette_write_word);
			SekSetWriteByteHandler(1,		megasys_palette_write_byte);
			SekClose();
		}
		break;

		case 0xA: // system A
		{
			SekInit(0, 0x68000);
			SekOpen(0);
			SekMapMemory(Drv68KROM0,		0x000000, 0x05ffff, MAP_ROM);
			SekMapMemory(DrvVidRegs,		0x084000, 0x0843ff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(DrvPalRAM,			0x088000, 0x0887ff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(DrvObjRAM,			0x08e000, 0x08ffff, MAP_RAM);
			SekMapMemory(DrvScrRAM[0],		0x090000, 0x093fff, MAP_RAM);
			SekMapMemory(DrvScrRAM[1],		0x094000, 0x097fff, MAP_RAM);
			SekMapMemory(DrvScrRAM[2],		0x098000, 0x09bfff, MAP_RAM);
			SekMapMemory(Drv68KRAM0,		0x0f0000, 0x0fffff, MAP_ROM); // writes handled in write handlers (mirrored bytes)
			SekSetReadWordHandler(0,		megasys1A_main_read_word);
			SekSetReadByteHandler(0,		megasys1A_main_read_byte);
			SekSetWriteWordHandler(0,		megasys1A_main_write_word);
			SekSetWriteByteHandler(0,		megasys1A_main_write_byte);

			SekMapHandler(1,			0x088000, 0x0887ff, MAP_WRITE);
			SekSetWriteWordHandler(1,		megasys_palette_write_word);
			SekSetWriteByteHandler(1,		megasys_palette_write_byte);
			SekClose();
		}
		break;

		case 0xB: // system B
		{
			SekInit(0, 0x68000);
			SekOpen(0);
			SekMapMemory(Drv68KROM0,		0x000000, 0x03ffff, MAP_ROM);
			SekMapMemory(DrvVidRegs,		0x044000, 0x0443ff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(DrvPalRAM,			0x048000, 0x0487ff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(DrvObjRAM,			0x04e000, 0x04ffff, MAP_RAM);
			SekMapMemory(DrvScrRAM[0],		0x050000, 0x053fff, MAP_RAM);
			SekMapMemory(DrvScrRAM[1],		0x054000, 0x057fff, MAP_RAM);
			SekMapMemory(DrvScrRAM[2],		0x058000, 0x05bfff, MAP_RAM);
			SekMapMemory(Drv68KRAM0,		0x060000, 0x07ffff, MAP_RAM); // mirrored bytes breaks EDF, use regular writes for system B
			SekMapMemory(Drv68KROM0 + 0x40000,	0x080000, 0x0bffff, MAP_ROM);
			SekSetReadWordHandler(0,		megasys1B_main_read_word);
			SekSetReadByteHandler(0,		megasys1B_main_read_byte);
			SekSetWriteWordHandler(0,		megasys1B_main_write_word);
			SekSetWriteByteHandler(0,		megasys1B_main_write_byte);

			SekMapHandler(1,			0x048000, 0x0487ff, MAP_WRITE);
			SekSetWriteWordHandler(1,		megasys_palette_write_word);
			SekSetWriteByteHandler(1,		megasys_palette_write_byte);

			SekClose();
		}
		break;

		case 0xC: // system C
		{
			SekInit(0, 0x68000);
			SekOpen(0);
			SekMapMemory(Drv68KROM0,		0x000000, 0x07ffff, MAP_ROM);
			SekMapMemory(DrvVidRegs,		0x0c0000, 0x0cffff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(DrvObjRAM,			0x0d2000, 0x0d3fff, MAP_RAM);
			SekMapMemory(DrvScrRAM[0],		0x0e0000, 0x0e3fff, MAP_RAM);
			SekMapMemory(DrvScrRAM[0],		0x0e4000, 0x0e7fff, MAP_RAM); // mirror
			SekMapMemory(DrvScrRAM[1],		0x0e8000, 0x0ebfff, MAP_RAM);
			SekMapMemory(DrvScrRAM[1],		0x0ec000, 0x0effff, MAP_RAM); // mirror
			SekMapMemory(DrvScrRAM[2],		0x0f0000, 0x0f3fff, MAP_RAM);
			SekMapMemory(DrvScrRAM[2],		0x0f4000, 0x0f7fff, MAP_RAM); // mirror
			SekMapMemory(DrvPalRAM,			0x0f8000, 0x0f87ff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(Drv68KRAM0,		0x1c0000, 0x1cffff, MAP_ROM); // writes handled in write handlers (mirrored bytes)
			SekMapMemory(Drv68KRAM0,		0x1d0000, 0x1dffff, MAP_ROM);
			SekMapMemory(Drv68KRAM0,		0x1e0000, 0x1effff, MAP_ROM);
			SekMapMemory(Drv68KRAM0,		0x1f0000, 0x1fffff, MAP_ROM);
			SekSetReadWordHandler(0,		megasys1C_main_read_word);
			SekSetReadByteHandler(0,		megasys1C_main_read_byte);
			SekSetWriteWordHandler(0,		megasys1C_main_write_word);
			SekSetWriteByteHandler(0,		megasys1C_main_write_byte);

			SekMapHandler(1,			0x0f8000, 0x0f87ff, MAP_WRITE);
			SekSetWriteWordHandler(1,		megasys_palette_write_word);
			SekSetWriteByteHandler(1,		megasys_palette_write_byte);
			SekClose();
		}
		break;

		case 0xD: // system D
		{
			SekInit(0, 0x68000);
			SekOpen(0);
			SekMapMemory(Drv68KROM0,		0x000000, 0x03ffff, MAP_ROM);
			SekMapMemory(DrvVidRegs,		0x0c0000, 0x0c9fff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(DrvObjRAM,			0x0ca000, 0x0cbfff, MAP_RAM);
			SekMapMemory(DrvScrRAM[1],		0x0d0000, 0x0d3fff, MAP_RAM);
			SekMapMemory(DrvScrRAM[2],		0x0d4000, 0x0d7fff, MAP_RAM);
			SekMapMemory(DrvPalRAM,			0x0d8000, 0x0d87ff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(DrvPalRAM,			0x0d9000, 0x0d97ff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(DrvPalRAM,			0x0da000, 0x0da7ff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(DrvPalRAM,			0x0db000, 0x0db7ff, MAP_ROM /*MAP_WRITE*/);
			SekMapMemory(DrvScrRAM[0],		0x0e8000, 0x0ebfff, MAP_RAM);
			SekMapMemory(Drv68KRAM0,		0x1f0000, 0x1fffff, MAP_RAM);
			SekSetReadWordHandler(0,		megasys1D_main_read_word);
			SekSetReadByteHandler(0,		megasys1D_main_read_byte);
			SekSetWriteWordHandler(0,		megasys1D_main_write_word);
			SekSetWriteByteHandler(0,		megasys1D_main_write_byte);

			SekMapHandler(1,			0x0d8000, 0x0db7ff, MAP_WRITE);
			SekSetWriteWordHandler(1,		megasys_palette_write_word);
			SekSetWriteByteHandler(1,		megasys_palette_write_byte);
			SekClose();
		}
		break;
	}

	if (system_select == 0) // system Z
	{
		ZetInit(0);
		ZetOpen(0);
		ZetMapMemory(DrvZ80ROM,		0x0000, 0x3fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM,		0xc000, 0xc7ff, MAP_RAM);
		ZetSetWriteHandler(megasys1z_sound_write);
		ZetSetReadHandler(megasys1z_sound_read);
		ZetSetOutHandler(megasys1z_sound_write_port);
		ZetSetInHandler(megasys1z_sound_read_port);
		ZetClose();

		BurnYM2203Init(2, 1500000, &DrvYM2203IRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
		BurnTimerAttachZet(3000000);
		BurnYM2203SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
		BurnYM2203SetAllRoutes(1, 0.50, BURN_SND_ROUTE_BOTH);

		layer_color_config[0] = 0;
		layer_color_config[1] = 0x200;
		layer_color_config[2] = 0;	// layer doesn't exist
		layer_color_config[3] = 0x100;	// sprites
	}
	else
	{
		// not in system D
		SekInit(1, 0x68000);
		SekOpen(1);
		SekMapMemory(Drv68KROM1,	0x000000, 0x01ffff, MAP_ROM);
		SekMapMemory(Drv68KRAM1,	0x0e0000, 0x0fffff, MAP_RAM);
		SekSetReadWordHandler(0,	megasys_sound_read_word);
		SekSetReadByteHandler(0,	megasys_sound_read_byte);
		SekSetWriteWordHandler(0,	megasys_sound_write_word);
		SekSetWriteByteHandler(0,	megasys_sound_write_byte);
		SekClose();

		// not in system D
		BurnYM2151Init(3500000);
		BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.80, BURN_SND_ROUTE_LEFT);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.80, BURN_SND_ROUTE_RIGHT);

		MSM6295Init(0, ((system_select == 0xD) ? 2000000 : 4000000) / 132, 1);
		MSM6295SetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);

		// not in system D
		MSM6295Init(1, 4000000 / 132, 1);
		MSM6295SetRoute(1, 0.30, BURN_SND_ROUTE_BOTH);
	}

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	if (system_select == 0) { // system Z
		ZetExit();
		BurnYM2203Exit();
	} else {
		BurnYM2151Exit();
		MSM6295Exit(0);
		MSM6295Exit(1);
	}

	SekExit();

#ifdef BUILD_A68K
	if (tshingen) {
		if (!oldasmemulation)
			bprintf(0, _T("Switching back to Musashi 68K core\n"));
		bBurnUseASMCPUEmulation = oldasmemulation;
		oldasmemulation = 0;
	}
#endif

	ignore_oki_status_hack = 1;
	system_select = 0;

	scroll_factor_8x8[1] = 1;

	layer_color_config[0] = 0;
	layer_color_config[1] = 0x100;
	layer_color_config[2] = 0x200;
	layer_color_config[3] = 0x300;

	monkelf = 0;
	tshingen = 0;

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static inline void draw_16x16_priority_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, UINT8 mosaic, UINT8 mosaicsol, INT32 priority)
{
	if (sy >= nScreenHeight || sy < -15 || sx >= nScreenWidth || sx < -15) return;

	UINT8 *gfx = DrvGfxROM[3] + (code * 0x100);

	flipy = (flipy) ? 0x0f : 0;
	flipx = (flipx) ? 0x0f : 0;

	color = (color * 16) + layer_color_config[3];

	UINT16 *dest = pTransDraw + sy * nScreenWidth + sx;
	UINT8 *prio = DrvPrioBitmap + sy * nScreenWidth + sx;

	for (INT32 y = 0; y < 16; y++, sy++, sx-=16)
	{
		for (INT32 x = 0; x < 16; x++, sx++)
		{
			if (sx < 0 || sy < 0 || sx >= nScreenWidth || sy >= nScreenHeight) continue;	

			INT32 pxl;

			if (mosaicsol) {
				pxl = gfx[(((y ^ flipy) |  mosaic) * 16) + ((x ^ flipx) |  mosaic)];
			} else {
				pxl = gfx[(((y ^ flipy) & ~mosaic) * 16) + ((x ^ flipx) & ~mosaic)];
			}

			if (pxl != 0x0f) {
				if ((priority & (1 << (prio[x] & 0x1f))) == 0 && prio[x] < 0x80) {
					dest[x] = pxl + color;
					prio[x] |= 0x80;
				}
			}
		}
		dest += nScreenWidth;
		prio += nScreenWidth;
	}
}

static void System1A_draw_sprites()
{
	INT32 color_mask = (sprite_flag & 0x100) ? 0x07 : 0x0f;

	UINT16 *objectram = (UINT16*)DrvObjBuf1;
	UINT16 *spriteram = (UINT16*)DrvSprBuf1;

	for (INT32 offs = (0x800-8)/2; offs >= 0; offs -= 4)
	{
		for (INT32 sprite = 0; sprite < 4 ; sprite ++)
		{
			UINT16 *objectdata = &objectram[offs + (0x800/2) * sprite];
			UINT16 *spritedata = &spriteram[(objectdata[0] & 0x7f) * 8];

			INT32 attr = spritedata[4];
			if (((attr & 0xc0) >> 6) != sprite) continue;

			INT32 sx = (spritedata[5] + objectdata[1]) & 0x1ff;
			INT32 sy = (spritedata[6] + objectdata[2]) & 0x1ff;

			if (sx > 255) sx -= 512;
			if (sy > 255) sy -= 512;

			INT32 code  = spritedata[7] + objectdata[3];
			INT32 color = attr & color_mask;

			INT32 flipx = attr & 0x40;
			INT32 flipy = attr & 0x80;
			INT32 pri  = (attr & 0x08) ? 0x0c : 0x0a;
			INT32 mosaic = (attr & 0x0f00)>>8;
			INT32 mossol = (attr & 0x1000)>>8; //not yet

			code = (code & 0xfff) + ((sprite_bank & 1) << 12);
			if (DrvTransTab[3][code]) continue;

			if (screen_flag & 1)
			{
				flipx = !flipx;
				flipy = !flipy;
				sx = 240 - sx;
				sy = 240 - sy;
			}

			draw_16x16_priority_sprite(code, color, sx, sy - 16, flipx, flipy, mosaic, mossol, pri);
		}
	}
}

static void System1Z_draw_sprites()
{
	UINT16 *spriteram16 = (UINT16*)DrvSprRAM;

	for (INT32 sprite = 0x80-1; sprite >= 0; sprite--)
	{
		UINT16 *spritedata = &spriteram16[sprite * 8];

		INT32 attr = spritedata[4];

		INT32 sx = spritedata[5] & 0x1ff;
		INT32 sy = spritedata[6] & 0x1ff;

		if (sx > 255) sx -= 512;
		if (sy > 255) sy -= 512;

		INT32 code  = spritedata[7] & 0x3ff;
		if (DrvTransTab[3][code]) continue;

		INT32 color = attr & 0x0f;

		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;
		INT32 pri  = (attr & 0x08) ? 0x0c : 0x0a;

		if (screen_flag & 1)
		{
			flipx = !flipx;
			flipy = !flipy;
			sx = 240 - sx;
			sy = 240 - sy;
		}

		draw_16x16_priority_sprite(code, color, sx, sy - 16, flipx, flipy, 0, 0, pri);
	}
}

static void draw_layer(INT32 tmap, INT32 flags, INT32 priority)
{
	INT32 layer = scroll_flag[tmap] & 0x03;
	INT32 size  =(scroll_flag[tmap] & 0x10) >> 4; // 1 - 8x8, 0 - 16x16

	INT32 type[2][4][2] = { { { 16, 2 }, { 8, 4 }, { 4, 8 }, { 2, 16 } }, { { 8, 1 }, { 4, 2 }, { 4, 2 }, { 2, 4 } } };
	INT32 trans_mask = (flags) ? 0xff : 0x0f;
	INT32 color_base = layer_color_config[tmap];

	UINT8 *gfxbase = DrvGfxROM[tmap];
	UINT16 *vidram = (UINT16*)DrvScrRAM[tmap];

	INT32 columns = type[size][layer][0];
	INT32 rows    = type[size][layer][1];
	INT32 width   = columns * 32;
	INT32 height  = rows * 32;

	INT32 xscroll = scrollx[tmap] & ((width * 8)-1);
	INT32 yscroll = (scrolly[tmap] + 16) & ((height * 8)-1);

	for (INT32 row = 0; row < height; row++)
	{
		for (INT32 col = 0; col < width; col++)
		{
			INT32 ofst, code;
			INT32 sx = (col * 8) - xscroll;
			INT32 sy = (row * 8) - yscroll;

			if (sx < -7) sx += width * 8;
			if (sy < -7) sy += height * 8;

			if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

			if (size) {
				ofst = (col * 32) + (row / 32) * 1024 * columns + (row & 0x1f);
				code = (vidram[ofst] & 0x0fff) * scroll_factor_8x8[tmap];
			} else {
				ofst = (((col / 2) * 16) + (row / 32) * 256 * columns + ((row / 2) & 0x0f));
				code = (vidram[ofst] & 0xfff) * 4 + ((row & 1) + (col & 1) * 2);
			}

			if (flags == 0 && DrvTransTab[tmap][code]) continue;

			INT32 color = ((vidram[ofst] >> 12) * 16) + color_base;

			{
				UINT8 *gfx = gfxbase + code * 0x40;
				UINT16 *dest = pTransDraw + sy * nScreenWidth + sx;
				UINT8 *prio = DrvPrioBitmap + sy * nScreenWidth + sx;

				for (INT32 y = 0; y < 8; y++, sy++, sx-=8) {
					if (sy >= nScreenHeight) break;

					for (INT32 x = 0; x < 8; x++, sx++, gfx++) {
						if (sx < 0 || sy < 0 || sx >= nScreenWidth) continue;

						INT32 pxl = *gfx;
						if (pxl != trans_mask) {
							dest[x] = pxl + color;
							prio[x] = priority;
						}
					}

					dest += nScreenWidth;
					prio += nScreenWidth;
				}
			}
		}
	}
}

static void screen_update()
{
	int reallyactive = 0;

	UINT32 pri = m_layers_order[(m_active_layers & 0x0f0f) >> 8];

	if (pri == 0xfffff) pri = 0x04132;

	for (INT32 i = 0;i < 5;i++) {
		reallyactive |= 1 << ((pri >> (4 * i)) & 0x0f);
	}

	INT32 active_layers = (m_active_layers & reallyactive) | (1 << ((pri & 0xf0000) >> 16));

	if (system_select == 0) {
		active_layers = 0x000b;
		pri = 0x0314f;
	}

	INT32 flag = 1;
	UINT32 primask = 0;

	for (INT32 i = 0; i < 5; i++)
	{
		int layer = (pri & 0xf0000) >> 16;
		pri <<= 4;

		switch (layer)
		{
			case 0:
			case 1:
			case 2:
				if (active_layers & (1 << layer))
				{
					if (nSpriteEnable & (1<<layer)) draw_layer(layer, flag, primask);
					flag = 0;
				}
				break;
			case 3:
			case 4:
				if (flag != 0)
				{
					flag = 0;
					BurnTransferClear();
				}

				if (sprite_flag & 0x100)
				{
					primask |= 1 << (layer-3);
				}
				else
					if (layer == 3) primask |= 3;

				break;
		}
	}

	if (active_layers & 0x08) {
		if (system_select == 0) {
			System1Z_draw_sprites();
		} else {
			System1A_draw_sprites();
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i+=2) {
			megasys_palette_write(i);
		}
		DrvRecalc = 0;
	}

	screen_update();

	BurnTransferCopy(DrvPalette);

	memcpy (DrvSprBuf1, DrvSprBuf0, 0x2000);
	memcpy (DrvObjBuf1, DrvObjBuf0, 0x2000);
	memcpy (DrvSprBuf0, DrvSprRAM , 0x2000);
	memcpy (DrvObjBuf0, DrvObjRAM , 0x2000);

	return 0;
}

static INT32 System1ZFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 6);
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 6000000 / 56, 3000000 / 56 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = (nCyclesTotal[0] * (i + 1)) / nInterleave;
		nCyclesDone[0] += SekRun(nSegment - nCyclesDone[0]);
		if (i ==   0) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		if (i == 128) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
		if (i == 240) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);

		BurnTimerUpdate((nCyclesTotal[1] * (i + 1)) / nInterleave);
		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // fix music in Legend of Makai (lomakai).  why is this needed? are irq's getting lost? -dink
	}

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 System1AFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, 6);
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nSegment;
	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { ((tshingen) ? 8000000 : 6000000) / 60, 7000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		nSegment = (nCyclesTotal[0] * (i + 1)) / nInterleave;
		nCyclesDone[0] += SekRun(nSegment - nCyclesDone[0]);
		if (i ==   0) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		if (i == 128) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
		if (i == 240) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		SekClose();

		SekOpen(1);
		nSegment = (nCyclesTotal[1] * (i + 1)) / nInterleave;
		if (sound_cpu_reset) {
			nCyclesDone[1] += SekIdle(nSegment - nCyclesDone[1]);
		} else {
			nCyclesDone[1] += SekRun(nSegment - nCyclesDone[1]);
		}

		if (pBurnSoundOut && i%8 == 7) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 8);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			MSM6295Render(1, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}

		SekClose();
	}

	SekOpen(1);

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength > 0) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			MSM6295Render(1, pSoundBuf, nSegmentLength);
		}
	}

	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 System1BFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, 6);
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nSegment;
	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 8000000 / 60, 7000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		nSegment = (nCyclesTotal[0] * (i + 1)) / nInterleave;
		nCyclesDone[0] += SekRun(nSegment - nCyclesDone[0]);
		if (i ==   0) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		if (i == 128) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		if (i == 240) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		SekClose();

		SekOpen(1);
		nSegment = (nCyclesTotal[1] * (i + 1)) / nInterleave;
		if (sound_cpu_reset) {
			nCyclesDone[1] += SekIdle(nSegment - nCyclesDone[1]);
		} else {
			nCyclesDone[1] += SekRun(nSegment - nCyclesDone[1]);
		}

		if (pBurnSoundOut && i%8 == 7) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 8);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			MSM6295Render(1, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}

		SekClose();
	}

	SekOpen(1);

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength > 0) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			MSM6295Render(1, pSoundBuf, nSegmentLength);
		}
	}

	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 System1CFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, 6);
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nSegment;
	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 12000000 / 60, 7000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		nSegment = (nCyclesTotal[0] * (i + 1)) / nInterleave;
		nCyclesDone[0] += SekRun(nSegment - nCyclesDone[0]);
		if (i ==   0) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		if (i == 128) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		if (i == 240) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		SekClose();

		SekOpen(1);
		nSegment = (nCyclesTotal[1] * (i + 1)) / nInterleave;
		if (sound_cpu_reset) {
			nCyclesDone[1] += SekIdle(nSegment - nCyclesDone[1]);
		} else {
			nCyclesDone[1] += SekRun(nSegment - nCyclesDone[1]);
		}

		if (pBurnSoundOut && i%8 == 7) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 8);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			MSM6295Render(1, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}

		SekClose();
	}

	SekOpen(1);

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength > 0) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			MSM6295Render(1, pSoundBuf, nSegmentLength);
		}
	}

	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 System1DFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 6);
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	SekOpen(0);
	SekRun(8000000 / 60);
	SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
	SekClose();

	if (pBurnSoundOut) {
		memset (pBurnSoundOut, 0, nBurnSoundLen * sizeof(INT16) * 2);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
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

		if (system_select == 0) {
			ZetScan(nAction);
			BurnYM2203Scan(nAction, pnMin);
		} else {
			BurnYM2151Scan(nAction);
			MSM6295Scan(0, nAction);
			MSM6295Scan(1, nAction);
		}

		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(soundlatch);
		SCAN_VAR(soundlatch2);
		SCAN_VAR(scroll_flag);
		SCAN_VAR(m_active_layers);
		SCAN_VAR(sprite_flag);
		SCAN_VAR(sprite_bank);
		SCAN_VAR(screen_flag);

		SCAN_VAR(input_select);
		SCAN_VAR(protection_val);

		SCAN_VAR(mcu_ram);
		SCAN_VAR(mcu_hs);

		SCAN_VAR(sound_cpu_reset);
		SCAN_VAR(oki_bank);
	}

	if (nAction & ACB_WRITE) {
		if (system_select == 0xD) {
			memcpy (DrvSndROM0 + 0x20000, DrvSndROM1 + oki_bank * 0x20000, 0x20000);
		}
	}

	return 0;
}


// P-47 - The Phantom Fighter (World)

static struct BurnRomInfo p47RomDesc[] = {
	{ "p47us3.bin",		0x20000, 0x022e58b8, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "p47us1.bin",		0x20000, 0xed926bd8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p47j_9.bin",		0x10000, 0xffcf318e, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "p47j_19.bin",	0x10000, 0xadb8c12e, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p47j_5.bin",		0x20000, 0xfe65b65c, 3 | BRF_GRA },           //  4 Tilemap #0 Tiles
	{ "p47j_6.bin",		0x20000, 0xe191d2d2, 3 | BRF_GRA },           //  5
	{ "p47j_7.bin",		0x20000, 0xf77723b7, 3 | BRF_GRA },           //  6

	{ "p47j_23.bin",	0x20000, 0x6e9bc864, 4 | BRF_GRA },           //  7 Tilemap #1 Tiles
	{ "p47j_12.bin",	0x20000, 0x5268395f, 4 | BRF_GRA },           //  8

	{ "p47us16.bin",	0x10000, 0x5a682c8f, 5 | BRF_GRA },           //  9 Tilemap #2 Tiles

	{ "p47j_27.bin",	0x20000, 0x9e2bde8e, 6 | BRF_GRA },           // 10 Sprites
	{ "p47j_18.bin",	0x20000, 0x29d8f676, 6 | BRF_GRA },           // 11
	{ "p47j_26.bin",	0x20000, 0x4d07581a, 6 | BRF_GRA },           // 12

	{ "p47j_20.bin",	0x20000, 0x2ed53624, 7 | BRF_SND },           // 13 OKI #0 Samples
	{ "p47j_21.bin",	0x20000, 0x6f56b56d, 7 | BRF_SND },           // 14

	{ "p47j_10.bin",	0x20000, 0xb9d79c1e, 8 | BRF_SND },           // 15 OKI #1 Samples
	{ "p47j_11.bin",	0x20000, 0xfa0d1887, 8 | BRF_SND },           // 16

	{ "p-47.14m",		0x00200, 0x1d877538, 9 | BRF_GRA },           // 17 Priority PROM
};

STD_ROM_PICK(p47)
STD_ROM_FN(p47)

static void p47RomLoadCallback()
{
	memmove (DrvGfxROM[1] + 0x020000, DrvGfxROM[1] + 0x000000, 0x040000);
	memmove (DrvGfxROM[3] + 0x060000, DrvGfxROM[3] + 0x040000, 0x020000);
}

static INT32 p47Init()
{
	return SystemInit(0xA, p47RomLoadCallback);
}

struct BurnDriver BurnDrvP47 = {
	"p47", NULL, NULL, NULL, "1988",
	"P-47 - The Phantom Fighter (World)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, p47RomInfo, p47RomName, NULL, NULL, CommonInputInfo, P47DIPInfo,
	p47Init, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// P-47 - The Freedom Fighter (Japan)

static struct BurnRomInfo p47jRomDesc[] = {
	{ "p47j_3.bin",		0x20000, 0x11c655e5, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "p47j_1.bin",		0x20000, 0x0a5998de, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p47j_9.bin",		0x10000, 0xffcf318e, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "p47j_19.bin",	0x10000, 0xadb8c12e, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p47j_5.bin",		0x20000, 0xfe65b65c, 3 | BRF_GRA },           //  4 Tilemap #0 Tiles
	{ "p47j_6.bin",		0x20000, 0xe191d2d2, 3 | BRF_GRA },           //  5
	{ "p47j_7.bin",		0x20000, 0xf77723b7, 3 | BRF_GRA },           //  6

	{ "p47j_23.bin",	0x20000, 0x6e9bc864, 4 | BRF_GRA },           //  7 Tilemap #1 Tiles
	{ "p47j_12.bin",	0x20000, 0x5268395f, 4 | BRF_GRA },           //  8

	{ "p47j_16.bin",	0x10000, 0x30e44375, 5 | BRF_GRA },           //  9 Tilemap #2 Tiles

	{ "p47j_27.bin",	0x20000, 0x9e2bde8e, 6 | BRF_GRA },           // 10 Sprites
	{ "p47j_18.bin",	0x20000, 0x29d8f676, 6 | BRF_GRA },           // 11
	{ "p47j_26.bin",	0x20000, 0x4d07581a, 6 | BRF_GRA },           // 12

	{ "p47j_20.bin",	0x20000, 0x2ed53624, 7 | BRF_SND },           // 13 OKI #0 Samples
	{ "p47j_21.bin",	0x20000, 0x6f56b56d, 7 | BRF_SND },           // 14

	{ "p47j_10.bin",	0x20000, 0xb9d79c1e, 8 | BRF_SND },           // 15 OKI #1 Samples
	{ "p47j_11.bin",	0x20000, 0xfa0d1887, 8 | BRF_SND },           // 16

	{ "p-47.14m",		0x00200, 0x1d877538, 9 | BRF_GRA },           // 17 Priority PROM
};

STD_ROM_PICK(p47j)
STD_ROM_FN(p47j)

struct BurnDriver BurnDrvP47j = {
	"p47j", "p47", NULL, NULL, "1988",
	"P-47 - The Freedom Fighter (Japan)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, p47jRomInfo, p47jRomName, NULL, NULL, CommonInputInfo, P47DIPInfo,
	p47Init, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// P-47 - The Freedom Fighter (Japan, Export)

static struct BurnRomInfo p47jeRomDesc[] = {
	{ "export_p-47_3.rom2",	0x20000, 0x37185412, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "export_p-47_1.rom1",	0x20000, 0x3925dd4f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p47j_9.bin",		0x10000, 0xffcf318e, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "p47j_19.bin",	0x10000, 0xadb8c12e, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p47j_5.bin",		0x20000, 0xfe65b65c, 3 | BRF_GRA },           //  4 Tilemap #0 Tiles
	{ "p47j_6.bin",		0x20000, 0xe191d2d2, 3 | BRF_GRA },           //  5
	{ "p47j_7.bin",		0x20000, 0xf77723b7, 3 | BRF_GRA },           //  6

	{ "p47j_23.bin",	0x20000, 0x6e9bc864, 4 | BRF_GRA },           //  7 Tilemap #1 Tiles
	{ "p47j_12.bin",	0x20000, 0x5268395f, 4 | BRF_GRA },           //  8

	{ "p47j_16.bin",	0x10000, 0x30e44375, 5 | BRF_GRA },           //  9 Tilemap #2 Tiles

	{ "p47j_27.bin",	0x20000, 0x9e2bde8e, 6 | BRF_GRA },           // 10 Sprites
	{ "p47j_18.bin",	0x20000, 0x29d8f676, 6 | BRF_GRA },           // 11
	{ "export_17.rom15",	0x20000, 0xb6c2e241, 6 | BRF_GRA },           // 12
	{ "p47j_26.bin",	0x20000, 0x4d07581a, 6 | BRF_GRA },           // 13

	{ "p47j_20.bin",	0x20000, 0x2ed53624, 7 | BRF_SND },           // 14 OKI #0 Samples
	{ "p47j_21.bin",	0x20000, 0x6f56b56d, 7 | BRF_SND },           // 15

	{ "p47j_10.bin",	0x20000, 0xb9d79c1e, 8 | BRF_SND },           // 16 OKI #1 Samples
	{ "p47j_11.bin",	0x20000, 0xfa0d1887, 8 | BRF_SND },           // 17

	{ "p-47.14m",		0x00200, 0x1d877538, 9 | BRF_GRA },           // 18 Priority PROM
};

STD_ROM_PICK(p47je)
STD_ROM_FN(p47je)

static void p47jeCallback()
{
	memmove (DrvGfxROM[1] + 0x020000, DrvGfxROM[1] + 0x000000, 0x040000);
}

static INT32 p47jeInit()
{
	return SystemInit(0xA, p47jeCallback);
}

struct BurnDriver BurnDrvP47je = {
	"p47je", "p47", NULL, NULL, "1988",
	"P-47 - The Freedom Fighter (Japan, Export)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, p47jeRomInfo, p47jeRomName, NULL, NULL, CommonInputInfo, P47DIPInfo,
	p47jeInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Kick Off (Japan)

static struct BurnRomInfo kickoffRomDesc[] = {
	{ "kioff03.rom",	0x10000, 0x3b01be65, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "kioff01.rom",	0x10000, 0xae6e68a1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "kioff09.rom",	0x10000, 0x1770e980, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "kioff19.rom",	0x10000, 0x1b03bbe4, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "kioff05.rom",	0x20000, 0xe7232103, 3 | BRF_GRA },           //  4 Tilemap #0 Tiles
	{ "kioff06.rom",	0x20000, 0xa0b3cb75, 3 | BRF_GRA },           //  5
	{ "kioff07.rom",	0x20000, 0xed649919, 3 | BRF_GRA },           //  6
	{ "kioff10.rom",	0x20000, 0xfd739fec, 3 | BRF_GRA },           //  7

	{ "kioff16.rom",	0x20000, 0x22c46314, 5 | BRF_GRA },           //  8 Tilemap #2 Tiles

	{ "kioff27.rom",	0x20000, 0xca221ae2, 6 | BRF_GRA },           //  9 Sprites
	{ "kioff18.rom",	0x20000, 0xd7909ada, 6 | BRF_GRA },           // 10
	{ "kioff17.rom",	0x20000, 0xf171559e, 6 | BRF_GRA },           // 11
	{ "kioff26.rom",	0x20000, 0x2a90df1b, 6 | BRF_GRA },           // 12

	{ "kioff20.rom",	0x20000, 0x5c28bd2d, 7 | BRF_SND },           // 13 OKI #0 Samples
	{ "kioff21.rom",	0x20000, 0x195940cf, 7 | BRF_SND },           // 14

	{ "kioff20.rom",	0x20000, 0x5c28bd2d, 8 | BRF_SND },           // 15 OKI #1 Samples
	{ "kioff21.rom",	0x20000, 0x195940cf, 8 | BRF_SND },           // 16

	{ "kick.bin",		0x00200, 0x85b30ac4, 9 | BRF_GRA },           // 17 Priority PROM
};

STD_ROM_PICK(kickoff)
STD_ROM_FN(kickoff)

static void kickoffCallback()
{
	memset (DrvGfxROM[1], 0xff, 0x80000);
}

static INT32 kickoffInit()
{
	return SystemInit(0xA, kickoffCallback);
}

struct BurnDriver BurnDrvKickoff = {
	"kickoff", NULL, NULL, NULL, "1988",
	"Kick Off (Japan)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSFOOTBALL, 0,
	NULL, kickoffRomInfo, kickoffRomName, NULL, NULL, CommonInputInfo, KickoffDIPInfo,
	kickoffInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Shingen Samurai-Fighter (Japan, English)

static struct BurnRomInfo tshingenRomDesc[] = {
	{ "shing_02.rom",	0x20000, 0xd9ab5b78, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "shing_01.rom",	0x20000, 0xa9d2de20, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "takeda5.bin",	0x10000, 0xfbdc51c0, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "takeda6.bin",	0x10000, 0x8fa65b69, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "m50747",		0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "takeda11.bin",	0x20000, 0xbf0b40a6, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles
	{ "shing_12.rom",	0x20000, 0x5e4adedb, 3 | BRF_GRA },           //  6

	{ "shing_15.rom",	0x20000, 0x9db18233, 4 | BRF_GRA },           //  7 Tilemap #1 Tiles
	{ "takeda16.bin",	0x20000, 0xceda9dd6, 4 | BRF_GRA },           //  8
	{ "takeda17.bin",	0x20000, 0x3d4371dc, 4 | BRF_GRA },           //  9

	{ "shing_19.rom",	0x10000, 0x97282d9d, 5 | BRF_GRA },           // 10 Tilemap #2 Tiles

	{ "shing_20.rom",	0x20000, 0x7f6f8384, 6 | BRF_GRA },           // 11 Sprites
	{ "takeda21.bin",	0x20000, 0x12fb006b, 6 | BRF_GRA },           // 12
	{ "takeda22.bin",	0x20000, 0xb165b6ae, 6 | BRF_GRA },           // 13
	{ "takeda23.bin",	0x20000, 0x37cb9214, 6 | BRF_GRA },           // 14

	{ "takeda9.bin",	0x20000, 0xdb7f3f4f, 7 | BRF_SND },           // 15 OKI #0 Samples
	{ "takeda10.bin",	0x20000, 0xc9959d71, 7 | BRF_SND },           // 16

	{ "shing_07.rom",	0x20000, 0xc37ecbdc, 8 | BRF_SND },           // 17 OKI #1 Samples
	{ "shing_08.rom",	0x20000, 0x36d56c8c, 8 | BRF_SND },           // 18

	{ "ts.bpr",			0x00200, 0x85b30ac4, 9 | BRF_GRA },           // 19 Priority PROM
};

STD_ROM_PICK(tshingen)
STD_ROM_FN(tshingen)

static INT32 tshingenInit()
{
	tshingen = 1;

#ifdef BUILD_A68K
	oldasmemulation = bBurnUseASMCPUEmulation;
	if (!bBurnUseASMCPUEmulation) {
		bprintf(0, _T("Switching to A68K for Takeda Shingen / Shingen Samurai-Fighter.\n"));
		bBurnUseASMCPUEmulation = true;
	}
#endif

	return SystemInit(0xA, phantasm_rom_decode);
}

struct BurnDriver BurnDrvTshingen = {
	"tshingen", NULL, NULL, NULL, "1988",
	"Shingen Samurai-Fighter (Japan, English)\0", "Game crashes in level 2, play tshingena instead!", "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, tshingenRomInfo, tshingenRomName, NULL, NULL, Common3ButtonInputInfo, TshingenDIPInfo,
	tshingenInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Takeda Shingen (Japan, Japanese)

static struct BurnRomInfo tshingenaRomDesc[] = {
	{ "takeda2.bin",	0x20000, 0x6ddfc9f3, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "takeda1.bin",	0x20000, 0x1afc6b7d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "takeda5.bin",	0x10000, 0xfbdc51c0, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "takeda6.bin",	0x10000, 0x8fa65b69, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "m50747",			0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "takeda11.bin",	0x20000, 0xbf0b40a6, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles
	{ "takeda12.bin",	0x20000, 0x07987d89, 3 | BRF_GRA },           //  6

	{ "takeda15.bin",	0x20000, 0x4c316b79, 4 | BRF_GRA },           //  7 Tilemap #1 Tiles
	{ "takeda16.bin",	0x20000, 0xceda9dd6, 4 | BRF_GRA },           //  8
	{ "takeda17.bin",	0x20000, 0x3d4371dc, 4 | BRF_GRA },           //  9

	{ "takeda19.bin",	0x10000, 0x2ca2420d, 5 | BRF_GRA },           // 10 Tilemap #2 Tiles

	{ "takeda20.bin",	0x20000, 0x1bfd636f, 6 | BRF_GRA },           // 11 Sprites
	{ "takeda21.bin",	0x20000, 0x12fb006b, 6 | BRF_GRA },           // 12
	{ "takeda22.bin",	0x20000, 0xb165b6ae, 6 | BRF_GRA },           // 13
	{ "takeda23.bin",	0x20000, 0x37cb9214, 6 | BRF_GRA },           // 14

	{ "takeda9.bin",	0x20000, 0xdb7f3f4f, 7 | BRF_SND },           // 15 OKI #0 Samples
	{ "takeda10.bin",	0x20000, 0xc9959d71, 7 | BRF_SND },           // 16

	{ "shing_07.rom",	0x20000, 0xc37ecbdc, 8 | BRF_SND },           // 17 OKI #1 Samples
	{ "shing_08.rom",	0x20000, 0x36d56c8c, 8 | BRF_SND },           // 18

	{ "ts.bpr",			0x00200, 0x85b30ac4, 9 | BRF_GRA },           // 19 Priority PROM
};

STD_ROM_PICK(tshingena)
STD_ROM_FN(tshingena)

struct BurnDriver BurnDrvTshingena = {
	"tshingena", "tshingen", NULL, NULL, "1988",
	"Takeda Shingen (Japan, Japanese)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, tshingenaRomInfo, tshingenaRomName, NULL, NULL, Common3ButtonInputInfo, TshingenDIPInfo,
	tshingenInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Ninja Kazan (World)

static struct BurnRomInfo kazanRomDesc[] = {
	{ "kazan.2",		0x20000, 0x072aa3d6, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "kazan.1",		0x20000, 0xb9801e2d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "iga_03.bin",		0x10000, 0xde5937ad, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "iga_04.bin",		0x10000, 0xafaf0480, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "iga_05.bin",		0x10000, 0x13580868, 2 | BRF_PRG | BRF_ESS }, //  4 68k #1 Code
	{ "iga_06.bin",		0x10000, 0x7904d5dd, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "m50747",			0x01000, 0x00000000, 0 | BRF_NODUMP },        //  6 MCU Code

	{ "kazan.11",		0x20000, 0x08e54137, 3 | BRF_GRA },           //  7 Tilemap #0 Tiles
	{ "kazan.12",		0x20000, 0xe89d58bd, 3 | BRF_GRA },           //  8

	{ "kazan.15",		0x20000, 0x48b28aa9, 4 | BRF_GRA },           //  9 Tilemap #1 Tiles
	{ "kazan.16",		0x20000, 0x07eab526, 4 | BRF_GRA },           // 10
	{ "kazan.17",		0x20000, 0x617269ea, 4 | BRF_GRA },           // 11
	{ "kazan.18",		0x20000, 0x52fc1b4b, 4 | BRF_GRA },           // 12

	{ "kazan.19",		0x10000, 0xb3a9a4ae, 5 | BRF_GRA },           // 13 Tilemap #2 Tiles

	{ "kazan.20",		0x20000, 0xee5819d8, 6 | BRF_GRA },           // 14 Sprites
	{ "kazan.21",		0x20000, 0xabf14d39, 6 | BRF_GRA },           // 15
	{ "kazan.22",		0x20000, 0x646933c4, 6 | BRF_GRA },           // 16
	{ "kazan.23",		0x20000, 0x0b531aee, 6 | BRF_GRA },           // 17

	{ "kazan.9",		0x20000, 0x5c28bd2d, 7 | BRF_SND },           // 18 OKI #0 Samples
	{ "kazan.10",		0x10000, 0xcd6c7978, 7 | BRF_SND },           // 19

	{ "kazan.7",		0x20000, 0x42f228f8, 8 | BRF_SND },           // 20 OKI #1 Samples
	{ "kazan.8",		0x20000, 0xebd1c883, 8 | BRF_SND },           // 21

	{ "kazan.14m",		0x00200, 0x85b30ac4, 9 | BRF_GRA },           // 22 Priority PROM
};

STD_ROM_PICK(kazan)
STD_ROM_FN(kazan)

static INT32 kazanInit()
{
	INT32 nRet = SystemInit(0xA, phantasm_rom_decode);

	if (nRet == 0)
	{
		*((UINT16*)(Drv68KROM0 + 0x000410)) = 0x4e73;	// hack - kill level 3 irq

		install_mcu_protection(mcu_config_type2, 0x2f000);
	}

	return nRet;
}

struct BurnDriver BurnDrvKazan = {
	"kazan", NULL, NULL, NULL, "1988",
	"Ninja Kazan (World)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, kazanRomInfo, kazanRomName, NULL, NULL, CommonInputInfo, KazanDIPInfo,
	kazanInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Iga Ninjyutsuden (Japan)

static struct BurnRomInfo iganinjuRomDesc[] = {
	{ "iga_02.bin",		0x20000, 0xbd00c280, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "iga_01.bin",		0x20000, 0xfa416a9e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "iga_03.bin",		0x10000, 0xde5937ad, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "iga_04.bin",		0x10000, 0xafaf0480, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "iga_05.bin",		0x10000, 0x13580868, 2 | BRF_PRG | BRF_ESS }, //  4 68k #1 Code
	{ "iga_06.bin",		0x10000, 0x7904d5dd, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "m50747",			0x01000, 0x00000000, 0 | BRF_NODUMP },        //  6 MCU Code

	{ "iga_14.bin",		0x40000, 0xc707d513, 3 | BRF_GRA },           //  7 Tilemap #0 Tiles

	{ "iga_18.bin",		0x80000, 0x6c727519, 4 | BRF_GRA },           //  8 Tilemap #1 Tiles

	{ "iga_19.bin",		0x20000, 0x98a7e998, 5 | BRF_GRA },           //  9 Tilemap #2 Tiles

	{ "iga_23.bin",		0x80000, 0xfb58c5f4, 6 | BRF_GRA },           // 10 Sprites

	{ "iga_10.bin",		0x40000, 0x67a89e0d, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "iga_08.bin",		0x40000, 0x857dbf60, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "iga.131",		0x00200, 0x1d877538, 9 | BRF_GRA },           // 13 Priority PROM
};

STD_ROM_PICK(iganinju)
STD_ROM_FN(iganinju)

struct BurnDriver BurnDrvIganinju = {
	"iganinju", "kazan", NULL, NULL, "1988",
	"Iga Ninjyutsuden (Japan)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, iganinjuRomInfo, iganinjuRomName, NULL, NULL, CommonInputInfo, KazanDIPInfo,
	kazanInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// The Astyanax

static struct BurnRomInfo astyanaxRomDesc[] = {
	{ "astyan2.bin",	0x20000, 0x1b598dcc, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "astyan1.bin",	0x20000, 0x1a1ad3cf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "astyan3.bin",	0x10000, 0x097b53a6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "astyan4.bin",	0x10000, 0x1e1cbdb2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "astyan5.bin",	0x10000, 0x11c74045, 2 | BRF_PRG | BRF_ESS }, //  4 68k #1 Code
	{ "astyan6.bin",	0x10000, 0xeecd4b16, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "m50747",			0x01000, 0x00000000, 0 | BRF_NODUMP },        //

	{ "astyan11.bin",	0x20000, 0x5593fec9, 3 | BRF_GRA },           //  7 Tilemap #0 Tiles
	{ "astyan12.bin",	0x20000, 0xe8b313ec, 3 | BRF_GRA },           //  8
	{ "astyan13.bin",	0x20000, 0x5f3496c6, 3 | BRF_GRA },           //  9
	{ "astyan14.bin",	0x20000, 0x29a09ec2, 3 | BRF_GRA },           // 10

	{ "astyan15.bin",	0x20000, 0x0d316615, 4 | BRF_GRA },           // 11 Tilemap #1 Tiles
	{ "astyan16.bin",	0x20000, 0xba96e8d9, 4 | BRF_GRA },           // 12
	{ "astyan17.bin",	0x20000, 0xbe60ba06, 4 | BRF_GRA },           // 13
	{ "astyan18.bin",	0x20000, 0x3668da3d, 4 | BRF_GRA },           // 14

	{ "astyan19.bin",	0x20000, 0x98158623, 5 | BRF_GRA },           // 15 Tilemap #2 Tiles

	{ "astyan20.bin",	0x20000, 0xc1ad9aa0, 6 | BRF_GRA },           // 16 Sprites
	{ "astyan21.bin",	0x20000, 0x0bf498ee, 6 | BRF_GRA },           // 17
	{ "astyan22.bin",	0x20000, 0x5f04d9b1, 6 | BRF_GRA },           // 18
	{ "astyan23.bin",	0x20000, 0x7bd4d1e7, 6 | BRF_GRA },           // 19

	{ "astyan9.bin",	0x20000, 0xa10b3f17, 7 | BRF_SND },           // 20 OKI #0 Samples
	{ "astyan10.bin",	0x20000, 0x4f704e7a, 7 | BRF_SND },           // 21

	{ "astyan7.bin",	0x20000, 0x319418cc, 8 | BRF_SND },           // 22 OKI #1 Samples
	{ "astyan8.bin",	0x20000, 0x5e5d2a22, 8 | BRF_SND },           // 23

	{ "rd.bpr",				0x00200, 0x85b30ac4, 9 | BRF_GRA },           // 24 Priority PROM
};

STD_ROM_PICK(astyanax)
STD_ROM_FN(astyanax)

static INT32 astyanaxInit()
{
	INT32 nRet = SystemInit(0xA, astyanax_rom_decode);

	if (nRet == 0) {
		install_mcu_protection(mcu_config_type1, 0x20000);
	}

	return nRet;
}

struct BurnDriver BurnDrvAstyanax = {
	"astyanax", NULL, NULL, NULL, "1989",
	"The Astyanax\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, astyanaxRomInfo, astyanaxRomName, NULL, NULL, Common3ButtonInputInfo, AstyanaxDIPInfo,
	astyanaxInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// The Lord of King (Japan)

static struct BurnRomInfo lordofkRomDesc[] = {
	{ "lokj02.bin",		0x20000, 0x0d7f9b4a, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "lokj01.bin",		0x20000, 0xbed3cb93, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "lokj03.bin",		0x20000, 0xd8702c91, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "lokj04.bin",		0x20000, 0xeccbf8c9, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "astyan5.bin",	0x10000, 0x11c74045, 2 | BRF_PRG | BRF_ESS }, //  4 68k #1 Code
	{ "astyan6.bin",	0x10000, 0xeecd4b16, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "m50747",			0x01000, 0x00000000, 0 | BRF_NODUMP },        //

	{ "astyan11.bin",	0x20000, 0x5593fec9, 3 | BRF_GRA },           //  7 Tilemap #0 Tiles
	{ "astyan12.bin",	0x20000, 0xe8b313ec, 3 | BRF_GRA },           //  8
	{ "astyan13.bin",	0x20000, 0x5f3496c6, 3 | BRF_GRA },           //  9
	{ "astyan14.bin",	0x20000, 0x29a09ec2, 3 | BRF_GRA },           // 10

	{ "astyan15.bin",	0x20000, 0x0d316615, 4 | BRF_GRA },           // 11 Tilemap #1 Tiles
	{ "astyan16.bin",	0x20000, 0xba96e8d9, 4 | BRF_GRA },           // 12
	{ "astyan17.bin",	0x20000, 0xbe60ba06, 4 | BRF_GRA },           // 13
	{ "astyan18.bin",	0x20000, 0x3668da3d, 4 | BRF_GRA },           // 14

	{ "astyan19.bin",	0x20000, 0x98158623, 5 | BRF_GRA },           // 15 Tilemap #2 Tiles

	{ "astyan20.bin",	0x20000, 0xc1ad9aa0, 6 | BRF_GRA },           // 16 Sprites
	{ "astyan21.bin",	0x20000, 0x0bf498ee, 6 | BRF_GRA },           // 17
	{ "astyan22.bin",	0x20000, 0x5f04d9b1, 6 | BRF_GRA },           // 18
	{ "astyan23.bin",	0x20000, 0x7bd4d1e7, 6 | BRF_GRA },           // 19

	{ "astyan9.bin",	0x20000, 0xa10b3f17, 7 | BRF_SND },           // 20 OKI #0 Samples
	{ "astyan10.bin",	0x20000, 0x4f704e7a, 7 | BRF_SND },           // 21

	{ "astyan7.bin",	0x20000, 0x319418cc, 8 | BRF_SND },           // 22 OKI #1 Samples
	{ "astyan8.bin",	0x20000, 0x5e5d2a22, 8 | BRF_SND },           // 23

	{ "rd.bpr",			0x00200, 0x85b30ac4, 9 | BRF_GRA },           // 24 Priority PROM
};

STD_ROM_PICK(lordofk)
STD_ROM_FN(lordofk)

struct BurnDriver BurnDrvLordofk = {
	"lordofk", "astyanax", NULL, NULL, "1989",
	"The Lord of King (Japan)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, lordofkRomInfo, lordofkRomName, NULL, NULL, Common3ButtonInputInfo, AstyanaxDIPInfo,
	astyanaxInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Hachoo!

static struct BurnRomInfo hachooRomDesc[] = {
	{ "hacho02.rom",	0x20000, 0x49489c27, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "hacho01.rom",	0x20000, 0x97fc9515, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "hacho05.rom",	0x10000, 0x6271f74f, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "hacho06.rom",	0x10000, 0xdb9e743c, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "m50747",			0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 mcu

	{ "hacho14.rom",	0x80000, 0x10188483, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "hacho15.rom",	0x20000, 0xe559347e, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles
	{ "hacho16.rom",	0x20000, 0x105fd8b5, 4 | BRF_GRA },           //  7
	{ "hacho17.rom",	0x20000, 0x77f46174, 4 | BRF_GRA },           //  8
	{ "hacho18.rom",	0x20000, 0x0be21111, 4 | BRF_GRA },           //  9

	{ "hacho19.rom",	0x20000, 0x33bc9de3, 5 | BRF_GRA },           // 10 Tilemap #2 Tiles

	{ "hacho20.rom",	0x20000, 0x2ae2011e, 6 | BRF_GRA },           // 11 Sprites
	{ "hacho21.rom",	0x20000, 0x6dcfb8d5, 6 | BRF_GRA },           // 12
	{ "hacho22.rom",	0x20000, 0xccabf0e0, 6 | BRF_GRA },           // 13
	{ "hacho23.rom",	0x20000, 0xff5f77aa, 6 | BRF_GRA },           // 14

	{ "hacho09.rom",	0x20000, 0xe9f35c90, 7 | BRF_SND },           // 15 OKI #0 Samples
	{ "hacho10.rom",	0x20000, 0x1aeaa188, 7 | BRF_SND },           // 16

	{ "hacho07.rom",	0x20000, 0x06e6ca7f, 8 | BRF_SND },           // 17 OKI #1 Samples
	{ "hacho08.rom",	0x20000, 0x888a6df1, 8 | BRF_SND },           // 18

	{ "ht.bin",			0x00200, 0x85302b15, 9 | BRF_GRA },           // 19 Priority PROM
};

STD_ROM_PICK(hachoo)
STD_ROM_FN(hachoo)

static INT32 hachooInit()
{
	ignore_oki_status_hack = 0;

	INT32 nRet = SystemInit(0xA, astyanax_rom_decode);

	if (nRet == 0) {
		install_mcu_protection(mcu_config_type1, 0x20000);
	}

	return nRet;
}

struct BurnDriver BurnDrvHachoo = {
	"hachoo", NULL, NULL, NULL, "1989",
	"Hachoo!\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, hachooRomInfo, hachooRomName, NULL, NULL, CommonInputInfo, HachooDIPInfo,
	hachooInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Jitsuryoku!! Pro Yakyuu (Japan)

static struct BurnRomInfo jitsuproRomDesc[] = {
	{ "jp_2.bin",		0x20000, 0x5d842ff2, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "jp_1.bin",		0x20000, 0x0056edec, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "jp_5.bin",		0x10000, 0x84454e9e, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "jp_6.bin",		0x10000, 0x1fa9b75b, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "m50747",			0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 mcu

	{ "jp_14.bin",		0x80000, 0xdb112abf, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "jp_18.bin",		0x80000, 0x3ed855e3, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "jp_19.bin",		0x20000, 0xff59111f, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "jp_23.bin",		0x80000, 0x275f48bd, 6 | BRF_GRA },           //  8 Sprites

	{ "jp_10.bin",		0x80000, 0x178e43c0, 7 | BRF_SND },           //  9 OKI #0 Samples

	{ "jp_8.bin",		0x80000, 0xeca67632, 8 | BRF_SND },           // 10 OKI #1 Samples

	{ "bs.bpr",			0x00200, 0x85b30ac4, 9 | BRF_GRA },           // 11 Priority PROM
};

STD_ROM_PICK(jitsupro)
STD_ROM_FN(jitsupro)

static void jitsupro_gfx_unmangle(UINT8 *rom, INT32 size)
{
	UINT8 *buf = rom + 0x80000;

	memcpy (buf, rom, size);

	for (INT32 i = 0; i < size; i++)
	{
		INT32 j = BITSWAP24(i,23,22,21,20,19,18,17,16,15,14,8,12,11,3,9,13,7,6,5,4,10,2,1,0);

		rom[i] = BITSWAP08(buf[j],4,3,5,7,6,2,1,0);
	}
}

static void jitsuproCallback()
{
	astyanax_rom_decode();
	jitsupro_gfx_unmangle(DrvGfxROM[0], 0x80000);
	jitsupro_gfx_unmangle(DrvGfxROM[3], 0x80000);
}

static INT32 jitsuproInit()
{
	INT32 nRet = SystemInit(0xA, jitsuproCallback);

	if (nRet == 0) {
		install_mcu_protection(mcu_config_type1, 0x20000);
	}

	return nRet;
}

struct BurnDriver BurnDrvJitsupro = {
	"jitsupro", NULL, NULL, NULL, "1989",
	"Jitsuryoku!! Pro Yakyuu (Japan)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, jitsuproRomInfo, jitsuproRomName, NULL, NULL, Common3ButtonInputInfo, JitsuproDIPInfo,
	jitsuproInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Plus Alpha

static struct BurnRomInfo plusalphRomDesc[] = {
	{ "pa-rom2.bin",	0x20000, 0x33244799, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "pa-rom1.bin",	0x20000, 0xa32fdcae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pa-rom3.bin",	0x10000, 0x1b739835, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pa-rom4.bin",	0x10000, 0xff760e80, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pa-rom5.bin",	0x10000, 0xddc2739b, 2 | BRF_PRG | BRF_ESS }, //  4 68k #1 Code
	{ "pa-rom6.bin",	0x10000, 0xf6f8a167, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "m50747",			0x01000, 0x00000000, 0 | BRF_NODUMP },        //  6 mcu

	{ "pa-rom11.bin",	0x20000, 0xeb709ae7, 3 | BRF_GRA },           //  7 Tilemap #0 Tiles
	{ "pa-rom12.bin",	0x20000, 0xcacbc350, 3 | BRF_GRA },           //  8
	{ "pa-rom13.bin",	0x20000, 0xfad093dd, 3 | BRF_GRA },           //  9
	{ "pa-rom14.bin",	0x20000, 0xd3676cd1, 3 | BRF_GRA },           // 10

	{ "pa-rom15.bin",	0x20000, 0x8787735b, 4 | BRF_GRA },           // 11 Tilemap #1 Tiles
	{ "pa-rom16.bin",	0x20000, 0xa06b813b, 4 | BRF_GRA },           // 12
	{ "pa-rom17.bin",	0x20000, 0xc6b38a4b, 4 | BRF_GRA },           // 13

	{ "pa-rom19.bin",	0x10000, 0x39ef193c, 5 | BRF_GRA },           // 14 Tilemap #2 Tiles

	{ "pa-rom20.bin",	0x20000, 0x86c557a8, 6 | BRF_GRA },           // 15 Sprites
	{ "pa-rom21.bin",	0x20000, 0x81140a88, 6 | BRF_GRA },           // 16
	{ "pa-rom22.bin",	0x20000, 0x97e39886, 6 | BRF_GRA },           // 17
	{ "pa-rom23.bin",	0x20000, 0x0383fb65, 6 | BRF_GRA },           // 18

	{ "pa-rom9.bin",	0x20000, 0x065364bd, 7 | BRF_SND },           // 19 OKI #0 Samples
	{ "pa-rom10.bin",	0x20000, 0x395df3b2, 7 | BRF_SND },           // 20

	{ "pa-rom7.bin",	0x20000, 0x9f5d800e, 8 | BRF_SND },           // 21 OKI #1 Samples
	{ "pa-rom8.bin",	0x20000, 0xae007750, 8 | BRF_SND },           // 22

	{ "prom.14m",		0x00200, 0x1d877538, 9 | BRF_GRA },           // 23 Priority PROM
};

STD_ROM_PICK(plusalph)
STD_ROM_FN(plusalph)

static INT32 plusalphInit()
{
	INT32 nRet = SystemInit(0xA, astyanax_rom_decode);

	if (nRet == 0) {
		install_mcu_protection(mcu_config_type1, 0x20000);
	}

	return nRet;
}

struct BurnDriver BurnDrvPlusalph = {
	"plusalph", NULL, NULL, NULL, "1989",
	"Plus Alpha\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, plusalphRomInfo, plusalphRomName, NULL, NULL, CommonInputInfo, PlusalphDIPInfo,
	plusalphInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 256, 3, 4
};


// Saint Dragon (set 1)

static struct BurnRomInfo stdragonRomDesc[] = {
	{ "jsd-02.bin",		0x20000, 0xcc29ab19, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "jsd-01.bin",		0x20000, 0x67429a57, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "jsd-05.bin",		0x10000, 0x8c04feaa, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "jsd-06.bin",		0x10000, 0x0bb62f3a, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "m50747",			0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 mcu

	{ "jsd-11.bin",		0x20000, 0x2783b7b1, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles
	{ "jsd-12.bin",		0x20000, 0x89466ab7, 3 | BRF_GRA },           //  6
	{ "jsd-13.bin",		0x20000, 0x9896ae82, 3 | BRF_GRA },           //  7
	{ "jsd-14.bin",		0x20000, 0x7e8da371, 3 | BRF_GRA },           //  8

	{ "jsd-15.bin",		0x20000, 0xe296bf59, 4 | BRF_GRA },           //  9 Tilemap #1 Tiles
	{ "jsd-16.bin",		0x20000, 0xd8919c06, 4 | BRF_GRA },           // 10
	{ "jsd-17.bin",		0x20000, 0x4f7ad563, 4 | BRF_GRA },           // 11
	{ "jsd-18.bin",		0x20000, 0x1f4da822, 4 | BRF_GRA },           // 12

	{ "jsd-19.bin",		0x10000, 0x25ce807d, 5 | BRF_GRA },           // 13 Tilemap #2 Tiles

	{ "jsd-20.bin",		0x20000, 0x2c6e93bb, 6 | BRF_GRA },           // 14 Sprites
	{ "jsd-21.bin",		0x20000, 0x864bcc61, 6 | BRF_GRA },           // 15
	{ "jsd-22.bin",		0x20000, 0x44fe2547, 6 | BRF_GRA },           // 16
	{ "jsd-23.bin",		0x20000, 0x6b010e1a, 6 | BRF_GRA },           // 17

	{ "jsd-09.bin",		0x20000, 0xe366bc5a, 7 | BRF_SND },           // 18 OKI #0 Samples
	{ "jsd-10.bin",		0x20000, 0x4a8f4fe6, 7 | BRF_SND },           // 19

	{ "jsd-07.bin",		0x20000, 0x6a48e979, 8 | BRF_SND },           // 20 OKI #1 Samples
	{ "jsd-08.bin",		0x20000, 0x40704962, 8 | BRF_SND },           // 21

	{ "prom.14m",		0x00200, 0x1d877538, 9 | BRF_GRA },           // 22 Priority PROM
};

STD_ROM_PICK(stdragon)
STD_ROM_FN(stdragon)

static INT32 stdragonInit()
{
	INT32 nRet = SystemInit(0xA, phantasm_rom_decode);

	if (nRet == 0) {
		install_mcu_protection(mcu_config_type2, 0x23ff0);
	}

	return nRet;
}

struct BurnDriver BurnDrvStdragon = {
	"stdragon", NULL, NULL, NULL, "1989",
	"Saint Dragon (set 1)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, stdragonRomInfo, stdragonRomName, NULL, NULL, CommonInputInfo, StdragonDIPInfo,
	stdragonInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Saint Dragon (set 2)

static struct BurnRomInfo stdragonaRomDesc[] = {
	{ "jsda-02.bin",	0x20000, 0xd65d4154, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "jsda-01.bin",	0x20000, 0xc40c8ee1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "jsd-05.bin",		0x10000, 0x8c04feaa, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "jsd-06.bin",		0x10000, 0x0bb62f3a, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "m50747",			0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 mcu

	{ "e71-14.bin",		0x80000, 0x8e26ff92, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "e72-18.bin",		0x80000, 0x0b234711, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "jsd-19.bin",		0x10000, 0x25ce807d, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "e73-23.bin",		0x80000, 0x00ca3e04, 6 | BRF_GRA },           //  8 Sprites

	{ "jsd-09.bin",		0x20000, 0xe366bc5a, 7 | BRF_SND },           //  9 OKI #0 Samples
	{ "jsd-10.bin",		0x20000, 0x4a8f4fe6, 7 | BRF_SND },           // 10

	{ "jsd-07.bin",		0x20000, 0x6a48e979, 8 | BRF_SND },           // 11 OKI #1 Samples
	{ "jsd-08.bin",		0x20000, 0x40704962, 8 | BRF_SND },           // 12

	{ "prom.14m",		0x00200, 0x1d877538, 9 | BRF_GRA },           // 13 Priority PROM
};

STD_ROM_PICK(stdragona)
STD_ROM_FN(stdragona)

static void stdragona_gfx_unmangle(UINT8 *rom, INT32 size)
{
	UINT8 *buf = (UINT8*)BurnMalloc(size);

	memcpy (buf, rom, size);

	for (INT32 i = 0;i < size;i++)
	{
		INT32 j = BITSWAP24(i,23,22,21,20,19,18,17,16,15,14,3,12,11,13,9,10,7,6,5,4,8,2,1,0);

		rom[i] = BITSWAP08(buf[j],3,7,5,6,4,2,1,0);
	}

	BurnFree (buf);
}

static void stdragonaCallback()
{
	phantasm_rom_decode();
	stdragona_gfx_unmangle(DrvGfxROM[0], 0x80000);
	stdragona_gfx_unmangle(DrvGfxROM[3], 0x80000);
}

static INT32 stdragonaInit()
{
	INT32 nRet = SystemInit(0xA, stdragonaCallback);

	if (nRet == 0) {
		install_mcu_protection(mcu_config_type2, 0x23ff0);
	}

	return nRet;
}

struct BurnDriver BurnDrvStdragona = {
	"stdragona", "stdragon", NULL, NULL, "1989",
	"Saint Dragon (set 2)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, stdragonaRomInfo, stdragonaRomName, NULL, NULL, CommonInputInfo, StdragonDIPInfo,
	stdragonaInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Saint Dragon (bootleg)
/*

Bootleg version of Saint Dragon. Two PCBs connected by two flat cables.
Sound section can host two oki chips (and roms) but only one is populated.
No ASICs just logic chips. 

- ROMs A-19 and A-20 are fitted 'piggy backed' with one pin
  from A-20 bent out and wired to a nearby TTL.
- Stage 5 has some of its background graphics corrupted.
  Don't know if it is a PCB issue or designed like that.

*/

static struct BurnRomInfo stdragonbRomDesc[] = {
	{ "a-4.bin",		0x10000, 0xc58fe5c2, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "a-2.bin",		0x10000, 0x46a7cdbb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a-3.bin",		0x10000, 0xf6a268c4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a-1.bin",		0x10000, 0x0fb439bd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "b-20.bin",		0x10000, 0x8c04feaa, 2 | BRF_PRG | BRF_ESS }, //  4 68k #1 Code
	{ "b-19.bin",		0x10000, 0x0bb62f3a, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "a-15.bin",		0x10000, 0x42f7d2cd, 3 | BRF_GRA },           //  6 Tilemap #0 Tiles
	{ "a-16.bin",		0x10000, 0x4f519a97, 3 | BRF_GRA },           //  7
	{ "a-14.bin",		0x10000, 0xd8ba8d4c, 3 | BRF_GRA },           //  8
	{ "a-18.bin",		0x10000, 0x5e35f269, 3 | BRF_GRA },           //  9
	{ "a-19.bin",		0x10000, 0xb818db20, 3 | BRF_GRA },           // 10
	{ "a-17.bin",		0x10000, 0x0f6094f9, 3 | BRF_GRA },           // 11
	{ "a-20.bin",		0x10000, 0xe8849b15, 3 | BRF_GRA },           // 12

	{ "a-9.bin",		0x10000, 0x135c2e0e, 4 | BRF_GRA },           // 13 Tilemap #1 Tiles
	{ "a-10.bin",		0x10000, 0x19cec47a, 4 | BRF_GRA },           // 14
	{ "a-5.bin",		0x10000, 0xda4ca7bf, 4 | BRF_GRA },           // 15
	{ "a-6.bin",		0x10000, 0x9d9b6470, 4 | BRF_GRA },           // 16
	{ "a-12.bin",		0x10000, 0x22382b5f, 4 | BRF_GRA },           // 17
	{ "a-11.bin",		0x10000, 0x26c2494d, 4 | BRF_GRA },           // 18
	{ "a-7.bin",		0x10000, 0xcee3a6f7, 4 | BRF_GRA },           // 19
	{ "a-8.bin",		0x10000, 0x883b99bb, 4 | BRF_GRA },           // 20

	{ "a-13.bin",		0x08000, 0x9e487aa1, 5 | BRF_GRA },           // 21 Tilemap #2 Tiles

	{ "a-22.bin",		0x10000, 0xc7ee6d89, 6 | BRF_GRA },           // 22 Sprites
	{ "a-23.bin",		0x10000, 0x79552709, 6 | BRF_GRA },           // 23
	{ "a-25.bin",		0x10000, 0xd8926711, 6 | BRF_GRA },           // 24
	{ "a-26.bin",		0x10000, 0x41d76447, 6 | BRF_GRA },           // 25
	{ "a-21.bin",		0x10000, 0x5af84bd5, 6 | BRF_GRA },           // 26
	{ "a-24.bin",		0x10000, 0x09ae3173, 6 | BRF_GRA },           // 27
	{ "a-27.bin",		0x10000, 0xc9049e98, 6 | BRF_GRA },           // 28
	{ "a-28.bin",		0x10000, 0xb4d12106, 6 | BRF_GRA },           // 29
	
	{ "a-29.bin",		0x10000, 0x0049aa65, 8 | BRF_SND },           // 30 OKI #1 Samples
	{ "a-30.bin",		0x10000, 0x05bce2c7, 8 | BRF_SND },           // 31
	{ "b-17.bin",		0x10000, 0x3e4e34d3, 8 | BRF_SND },           // 32
	{ "b-18.bin",		0x10000, 0x738a6643, 8 | BRF_SND },           // 33

	{ "prom.14m",		0x00200, 0x1d877538, 9 | BRF_GRA },           // 34 Priority PROM
};

STD_ROM_PICK(stdragonb)
STD_ROM_FN(stdragonb)

static void stdragonbCallback()
{
	stdragona_gfx_unmangle(DrvGfxROM[0], 0x80000);
	stdragona_gfx_unmangle(DrvGfxROM[3], 0x80000);
}

static INT32 stdragonbInit()
{
	return SystemInit(0xA, stdragonbCallback);
}

struct BurnDriver BurnDrvStdragonb = {
	"stdragonb", "stdragon", NULL, NULL, "1989",
	"Saint Dragon (bootleg)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, stdragonbRomInfo, stdragonbRomName, NULL, NULL, CommonInputInfo, StdragonDIPInfo,
	stdragonbInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Rod-Land (World)

static struct BurnRomInfo rodlandRomDesc[] = {
	{ "JALECO_ROD_LAND_2.ROM2",		0x20000, 0xc7e00593, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "JALECO_ROD_LAND_1.ROM1",		0x20000, 0x2e748ca1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "JALECO_ROD_LAND_3.ROM3",		0x10000, 0x62fdf6d7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "JALECO_ROD_LAND_4.ROM4",		0x10000, 0x44163c86, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "JALECO_ROD_LAND_5.ROM5",		0x10000, 0xc1617c28, 2 | BRF_PRG | BRF_ESS }, //  4 68k #1 Code
	{ "JALECO_ROD_LAND_6.ROM6",		0x10000, 0x663392b2, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "LH534H31.ROM14",				0x80000, 0x8201e1bb, 3 | BRF_GRA },           //  6 Tilemap #0 Tiles

	{ "LH534H32.ROM18",				0x80000, 0xf3b30ca6, 4 | BRF_GRA },           //  7 Tilemap #1 Tiles

	{ "LH2311J0.ROM19",				0x20000, 0x124d7e8f, 5 | BRF_GRA },           //  8 Tilemap #2 Tiles

	{ "LH534H33.ROM23",				0x80000, 0x936db174, 6 | BRF_GRA },           //  9 Sprites

	{ "LH5321T5.ROM10",				0x40000, 0xe1d1cd99, 7 | BRF_SND },           // 10 OKI #0 Samples

	{ "S202000DR.ROM8",				0x40000, 0x8a49d3a7, 8 | BRF_SND },           // 11 OKI #1 Samples

	{ "PS89013A.M14",				0x00200, 0x8914e72d, 9 | BRF_GRA },           // 12 Priority PROM
};

STD_ROM_PICK(rodland)
STD_ROM_FN(rodland)

static void rodland_gfx_unmangle(UINT8 *rom, INT32 size)
{
	UINT8 *buf = (UINT8*)BurnMalloc(size);

	memcpy (buf, rom, size);

	for (INT32 i = 0;i < size;i++)
	{
		INT32 j = BITSWAP24(i,23,22,21,20,19,18,17,16,15,14,10,12,11,8,9,3,7,6,5,4,13,2,1,0);

		rom[i] = BITSWAP08(buf[j], 6,4,5,3,7,2,1,0);
	}

	BurnFree (buf);
}

static void rodlandCallback()
{
	rodland_rom_decode();
	rodland_gfx_unmangle(DrvGfxROM[0], 0x80000);
	rodland_gfx_unmangle(DrvGfxROM[3], 0x80000);
}

static INT32 rodlandInit()
{
	return SystemInit(0xA, rodlandCallback);
}

struct BurnDriver BurnDrvRodland = {
	"rodland", NULL, NULL, NULL, "1990",
	"Rod-Land (World)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, rodlandRomInfo, rodlandRomName, NULL, NULL, CommonInputInfo, RodlandDIPInfo,
	rodlandInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Rod-Land (Japan)

static struct BurnRomInfo rodlandjRomDesc[] = {
	{ "JALECO_ROD_LAND_2.ROM2",		0x20000, 0xb1d2047e, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "JALECO_ROD_LAND_1.ROM1",		0x20000, 0x3c47c2a3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "JALECO_ROD_LAND_3.ROM3",		0x10000, 0xc5b1075f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "JALECO_ROD_LAND_4.ROM4",		0x10000, 0x9ec61048, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "JALECO_ROD_LAND_5.ROM5",		0x10000, 0xc1617c28, 2 | BRF_PRG | BRF_ESS }, //  4 68k #1 Code
	{ "JALECO_ROD_LAND_6.ROM6",		0x10000, 0x663392b2, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "LH534H31.ROM14",				0x80000, 0x8201e1bb, 3 | BRF_GRA },           //  6 Tilemap #0 Tiles

	{ "LH534H32.ROM18",				0x80000, 0xf3b30ca6, 4 | BRF_GRA },           //  7 Tilemap #1 Tiles

	{ "LH2311J0.ROM19",				0x20000, 0x124d7e8f, 5 | BRF_GRA },           //  8 Tilemap #2 Tiles

	{ "LH534H33.ROM23",				0x80000, 0x936db174, 6 | BRF_GRA },           //  9 Sprites

	{ "LH5321T5.ROM10",				0x40000, 0xe1d1cd99, 7 | BRF_SND },           // 10 OKI #0 Samples

	{ "S202000DR.ROM8",				0x40000, 0x8a49d3a7, 8 | BRF_SND },           // 11 OKI #1 Samples

	{ "PS89013A.M14",				0x00200, 0x8914e72d, 9 | BRF_GRA },           // 12 Priority PROM
};

STD_ROM_PICK(rodlandj)
STD_ROM_FN(rodlandj)

static void rodlandjCallback()
{
	astyanax_rom_decode();
	rodland_gfx_unmangle(DrvGfxROM[0], 0x80000);
	rodland_gfx_unmangle(DrvGfxROM[3], 0x80000);
}

static INT32 rodlandjInit()
{
	return SystemInit(0xA, rodlandjCallback);
}

struct BurnDriver BurnDrvRodlandj = {
	"rodlandj", "rodland", NULL, NULL, "1990",
	"Rod-Land (Japan)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, rodlandjRomInfo, rodlandjRomName, NULL, NULL, CommonInputInfo, RodlandDIPInfo,
	rodlandjInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Rod-Land (Japan bootleg)

static struct BurnRomInfo rodlandjbRomDesc[] = {
	{ "rl19.bin",			0x10000, 0x028de21f, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "rl17.bin",			0x10000, 0x9c720046, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rl20.bin",			0x10000, 0x3f536d07, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rl18.bin",			0x10000, 0x5aa61717, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rl_3.bin",			0x10000, 0xc5b1075f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rl_4.bin",			0x10000, 0x9ec61048, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "rl02.bin",			0x10000, 0xd26eae8f, 2 | BRF_PRG | BRF_ESS }, //  6 68k #1 Code
	{ "rl01.bin",			0x10000, 0x04cf24bc, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "LH534H31.ROM14",		0x80000, 0x8201e1bb, 3 | BRF_GRA },           //  6 Tilemap #0 Tiles

	{ "LH534H32.ROM18",		0x80000, 0xf3b30ca6, 4 | BRF_GRA },           //  7 Tilemap #1 Tiles

	{ "LH2311J0.ROM19",		0x20000, 0x124d7e8f, 5 | BRF_GRA },           //  8 Tilemap #2 Tiles

	{ "LH534H33.ROM23",		0x80000, 0x936db174, 6 | BRF_GRA },           //  9 Sprites

	{ "LH5321T5.ROM10",		0x40000, 0xe1d1cd99, 7 | BRF_SND },           // 10 OKI #0 Samples

	{ "S202000DR.ROM8",		0x40000, 0x8a49d3a7, 8 | BRF_SND },           // 11 OKI #1 Samples

	{ "PS89013A.M14",		0x00200, 0x8914e72d, 9 | BRF_GRA },           // 12 Priority PROM
};

STD_ROM_PICK(rodlandjb)
STD_ROM_FN(rodlandjb)

static void rodlandjbCallback()
{
	rodland_gfx_unmangle(DrvGfxROM[0], 0x80000);
	rodland_gfx_unmangle(DrvGfxROM[3], 0x80000);
}

static INT32 rodlandjbInit()
{
	return SystemInit(0xA, rodlandjbCallback);
}

struct BurnDriver BurnDrvRodlandjb = {
	"rodlandjb", "rodland", NULL, NULL, "1990",
	"Rod-Land (Japan bootleg)\0", NULL, "bootleg", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, rodlandjbRomInfo, rodlandjbRomName, NULL, NULL, CommonInputInfo, RodlandDIPInfo,
	rodlandjbInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// R&T (Rod-Land prototype?)
/* probably a prototype, original JP key and unscrambled ROMs, incorrect audio matches PCB */

static struct BurnRomInfo rittamRomDesc[] = {
	{ "2.ROM2",				0x20000, 0x93085af2, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "R+T_1.ROM1",			0x20000, 0x20446C34, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "JALECO_5.ROM5",		0x10000, 0xea6600ec, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "JALECO_6.ROM6",		0x10000, 0x51c3c0bc, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "11.ROM11",			0x20000, 0xad2bf897, 3 | BRF_GRA },           //  6 Tilemap #0 Tiles
	{ "12.ROM12",			0x20000, 0xd0224ed6, 3 | BRF_GRA },           //  7
	{ "13.ROM13",			0x20000, 0xb1d5d423, 3 | BRF_GRA },           //  8
	{ "14.ROM14",			0x20000, 0x20f8c361, 3 | BRF_GRA },           //  9

	{ "15.ROM15",			0x20000, 0x90bc97ac, 4 | BRF_GRA },           // 10 Tilemap #1 Tiles
	{ "16.ROM16",			0x20000, 0xe38750aa, 4 | BRF_GRA },           // 11 
	// ROM17 not populated - not sure why, missing?
	{ "18.ROM18",			0x20000, 0x57ccf24f, 4 | BRF_GRA },           // 12 

	{ "19.ROM19",			0x20000, 0x6daa1081, 5 | BRF_GRA },           // 13 Tilemap #2 Tiles

	{ "R+T_20.ROM20",		0x20000, 0x23bc2b0b, 6 | BRF_GRA },           // 14 Sprites
	{ "21.ROM21",			0x20000, 0x9d2b0ec4, 6 | BRF_GRA },           // 15 
	{ "22.ROM22",			0x20000, 0xbba2e2cf, 6 | BRF_GRA },           // 16
	{ "23.ROM23",			0x20000, 0x05536a18, 6 | BRF_GRA },           // 17

	{ "JALECO_9.ROM9",		0x20000, 0x065364bd, 7 | BRF_SND },           // 18 OKI #0 Samples
	{ "JALECO_10.ROM10",	0x20000, 0x395df3b2, 7 | BRF_SND },           // 19 

	{ "JALECO_7.ROM7",		0x20000, 0x76fd879f, 8 | BRF_SND },           // 20 OKI #1 Samples
	{ "JALECO_8.ROM8",		0x20000, 0xa771ab00, 8 | BRF_SND },           // 21 OKI #1 Samples

	{ "PS89013A.M14",		0x00200, 0x8914e72d, 9 | BRF_GRA },           // 22 Priority PROM
};

STD_ROM_PICK(rittam)
STD_ROM_FN(rittam)

static void rittamCallback()
{
	astyanax_rom_decode();
}

static INT32 rittamInit()
{
	return SystemInit(0xA, rittamCallback);
} 

struct BurnDriver BurnDrvRittam = {
	"rittam", "rodland", NULL, NULL, "1990",
	"R&T (Rod-Land prototype?)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, rittamRomInfo, rittamRomName, NULL, NULL, CommonInputInfo, RodlandDIPInfo,
	rittamInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Soldam

static struct BurnRomInfo soldamRomDesc[] = {
	{ "2ver1j.bin",		0x20000, 0x45444b07, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "1euro.bin",		0x20000, 0x9f9da28a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3ver1.bin",		0x10000, 0xc5382a07, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4ver1.bin",		0x10000, 0x1df7816f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "5ver1.bin",		0x10000, 0xd1019a67, 2 | BRF_PRG | BRF_ESS }, //  4 68k #1 Code
	{ "6ver1.bin",		0x10000, 0x3ed219b4, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "14ver1.bin",		0x80000, 0x73c90610, 3 | BRF_GRA },           //  6 Tilemap #0 Tiles

	{ "18ver1.bin",		0x80000, 0xe91a1afd, 4 | BRF_GRA },           //  7 Tilemap #1 Tiles

	{ "19ver1.bin",		0x20000, 0x38465da1, 5 | BRF_GRA },           //  8 Tilemap #2 Tiles

	{ "23ver1.bin",		0x80000, 0x0ca09432, 6 | BRF_GRA },           //  9 Sprites

	{ "10ver1.bin",		0x40000, 0x8d5613bf, 7 | BRF_SND },           // 10 OKI #0 Samples

	{ "8ver1.bin",		0x40000, 0xfcd36019, 8 | BRF_SND },           // 11 OKI #1 Samples

	{ "pr-91023.m14",	0x00200, 0x8914e72d, 9 | BRF_GRA },           // 12 Priority PROM
};

STD_ROM_PICK(soldam)
STD_ROM_FN(soldam)

static INT32 soldamInit()
{
	INT32 nRet = SystemInit(0xA, phantasm_rom_decode);

	if (nRet == 0) {
		SekOpen(0);
		SekMapMemory(DrvSprRAM,		0x8c000, 0x8c7ff, MAP_RAM); // mirror
		SekClose();
	}

	return nRet;
}

struct BurnDriver BurnDrvSoldam = {
	"soldam", NULL, NULL, NULL, "1992",
	"Soldam\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, soldamRomInfo, soldamRomName, NULL, NULL, CommonInputInfo, SoldamDIPInfo,
	soldamInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Soldam (Japan)

static struct BurnRomInfo soldamjRomDesc[] = {
	{ "soldam2.bin",	0x20000, 0xc73d29e4, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "soldam1.bin",	0x20000, 0xe7cb0c20, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3ver1.bin",		0x10000, 0xc5382a07, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4ver1.bin",		0x10000, 0x1df7816f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "5ver1.bin",		0x10000, 0xd1019a67, 2 | BRF_PRG | BRF_ESS }, //  4 68k #1 Code
	{ "6ver1.bin",		0x10000, 0x3ed219b4, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "soldam14.bin",	0x80000, 0x26cea54a, 3 | BRF_GRA },           //  6 Tilemap #0 Tiles

	{ "soldam18.bin",	0x80000, 0x7d8e4712, 4 | BRF_GRA },           //  7 Tilemap #1 Tiles

	{ "19ver1.bin",		0x20000, 0x38465da1, 5 | BRF_GRA },           //  8 Tilemap #2 Tiles

	{ "23ver1.bin",		0x80000, 0x0ca09432, 6 | BRF_GRA },           //  9 Sprites

	{ "10ver1.bin",		0x40000, 0x8d5613bf, 7 | BRF_SND },           // 10 OKI #0 Samples

	{ "8ver1.bin",		0x40000, 0xfcd36019, 8 | BRF_SND },           // 11 OKI #1 Samples

	{ "pr-91023.m14",	0x00200, 0x8914e72d, 9 | BRF_GRA },           // 12 Priority PROM
};

STD_ROM_PICK(soldamj)
STD_ROM_FN(soldamj)

static INT32 soldamjInit()
{
	INT32 nRet = SystemInit(0xA, astyanax_rom_decode);

	scroll_factor_8x8[1] = 4;

	if (nRet == 0) {
		SekOpen(0);
		SekMapMemory(DrvSprRAM,		0x8c000, 0x8c7ff, MAP_RAM); // mirror
		SekClose();
	}

	return nRet;
}

struct BurnDriver BurnDrvSoldamj = {
	"soldamj", "soldam", NULL, NULL, "1992",
	"Soldam (Japan)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, soldamjRomInfo, soldamjRomName, NULL, NULL, CommonInputInfo, SoldamDIPInfo,
	soldamjInit, DrvExit, System1AFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Avenging Spirit

static struct BurnRomInfo avspiritRomDesc[] = {
	{ "spirit05.rom",	0x40000, 0xb26a341a, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "spirit06.rom",	0x40000, 0x609f71fe, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "spirit01.rom",	0x20000, 0xd02ec045, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "spirit02.rom",	0x20000, 0x30213390, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "m50747",			0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "spirit12.rom",	0x80000, 0x728335d4, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "spirit11.rom",	0x80000, 0x7896f6b0, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "spirit09.rom",	0x20000, 0x0c37edf7, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "spirit10.rom",	0x80000, 0x2b1180b3, 6 | BRF_GRA },           //  8 Sprites

	{ "spirit14.rom",	0x40000, 0x13be9979, 7 | BRF_SND },           //  9 OKI #0 Samples

	{ "spirit13.rom",	0x40000, 0x05bc04d9, 8 | BRF_SND },           // 10 OKI #1 Samples

	{ "ph.bin",			0x00200, 0x8359650a, 9 | BRF_GRA },           // 11 Priority PROM
};

STD_ROM_PICK(avspirit)
STD_ROM_FN(avspirit)

static INT32 avspiritInit()
{
	input_select_values[0] = 0x37;
	input_select_values[1] = 0x35;
	input_select_values[2] = 0x36;
	input_select_values[3] = 0x33;
	input_select_values[4] = 0x34;

	INT32 nRet = SystemInit(0xB, NULL);

	if (nRet == 0) {
		SekOpen(0);
		SekMapMemory(Drv68KRAM0,	0x70000, 0x7ffff, MAP_RAM); // only 64k
		SekClose();
	}

	return nRet;
}

struct BurnDriver BurnDrvAvspirit = {
	"avspirit", NULL, NULL, NULL, "1991",
	"Avenging Spirit\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, avspiritRomInfo, avspiritRomName, NULL, NULL, CommonInputInfo, AvspiritDIPInfo,
	avspiritInit, DrvExit, System1BFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Phantasm (Japan)

static struct BurnRomInfo phantasmRomDesc[] = {
	{ "phntsm02.bin",	0x20000, 0xd96a3584, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "phntsm01.bin",	0x20000, 0xa54b4b87, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "phntsm03.bin",	0x10000, 0x1d96ce20, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "phntsm04.bin",	0x10000, 0xdc0c4994, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "phntsm05.bin",	0x10000, 0x3b169b4a, 2 | BRF_PRG | BRF_ESS }, //  4 68k #1 Code
	{ "phntsm06.bin",	0x10000, 0xdf2dfb2e, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "spirit12.rom",	0x80000, 0x728335d4, 3 | BRF_GRA },           //  6 Tilemap #0 Tiles

	{ "spirit11.rom",	0x80000, 0x7896f6b0, 4 | BRF_GRA },           //  7 Tilemap #1 Tiles

	{ "spirit09.rom",	0x20000, 0x0c37edf7, 5 | BRF_GRA },           //  8 Tilemap #2 Tiles

	{ "spirit10.rom",	0x80000, 0x2b1180b3, 6 | BRF_GRA },           //  9 Sprites

	{ "spirit14.rom",	0x40000, 0x13be9979, 7 | BRF_SND },           // 10 OKI #0 Samples

	{ "spirit13.rom",	0x40000, 0x05bc04d9, 8 | BRF_SND },           // 11 OKI #1 Samples

	{ "ph.bin",			0x00200, 0x8359650a, 9 | BRF_GRA },           // 12 Priority PROM
};

STD_ROM_PICK(phantasm)
STD_ROM_FN(phantasm)

static INT32 phantasmInit()
{
	return SystemInit(0xA, phantasm_rom_decode);
}

struct BurnDriver BurnDrvPhantasm = {
	"phantasm", "avspirit", NULL, NULL, "1990",
	"Phantasm (Japan)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, phantasmRomInfo, phantasmRomName, NULL, NULL, CommonInputInfo, PhantasmDIPInfo,
	phantasmInit, DrvExit, System1BFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Monky Elf (Korean bootleg of Avenging Spirit)

static struct BurnRomInfo monkelfRomDesc[] = {
	{ "6",			0x40000, 0x40b80914, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "5",			0x40000, 0x6c45465d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "4",			0x20000, 0xd02ec045, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "3",			0x20000, 0x30213390, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "8",			0x80000, 0x728335d4, 3 | BRF_GRA },           //  4 Tilemap #0 Tiles

	{ "9",			0x80000, 0x7896f6b0, 4 | BRF_GRA },           //  5 Tilemap #1 Tiles

	{ "10",			0x20000, 0x0c37edf7, 5 | BRF_GRA },           //  6 Tilemap #2 Tiles

	{ "7",			0x80000, 0x2b1180b3, 6 | BRF_GRA },           //  7 Sprites

	{ "1",			0x40000, 0x13be9979, 7 | BRF_SND },           //  8 OKI #0 Samples

	{ "2",			0x40000, 0x05bc04d9, 8 | BRF_SND },           //  9 OKI #1 Samples

	{ "82s147",		0x00200, 0x547eccc0, 9 | BRF_GRA },           // 10 Priority PROM
};

STD_ROM_PICK(monkelf)
STD_ROM_FN(monkelf)

static UINT16 __fastcall monkelf_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xe0002:
			return DrvInputs[1];

		case 0xe0004:
			return DrvInputs[2];

		case 0xe0006:
			return DrvDips[0];

		case 0xe0008:
			return DrvDips[1];

		case 0xe000a:
			return DrvInputs[0];

		case 0xe000e:
			return 0;
	}

	return 0xffff;
}

static UINT8 __fastcall monkelf_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xe0002:
			return DrvInputs[1] >> 8;

		case 0xe0003:
			return DrvInputs[1];

		case 0xe0004:
			return DrvInputs[2] >> 8;

		case 0xe0005:
			return DrvInputs[2];

		case 0xe0006:
		case 0xe0007:
			return DrvDips[0];

		case 0xe0008:
		case 0xe0009:
			return DrvDips[1];

		case 0xe000a:
			return DrvInputs[0] >> 8;

		case 0xe000b:
			return DrvInputs[0];
	}

	return 0xff;
}

static void monkelfCallback()
{
	*((UINT16*)(Drv68KROM0 + 0x0744)) = 0x4e71;	// bypass trap - very strange

	// convert bootleg priority prom to standard format
	for (INT32 i = 0x1fe; i >= 0; i -= 2) {
		DrvPrioPROM[i+0] = DrvPrioPROM[i+1] = (DrvPrioPROM[i/2] >> 4) & 0x03;
	}

	DrvPriorityDecode(); // re-decode
}

static INT32 monkelfInit()
{
	monkelf = 1;

	INT32 nRet = SystemInit(0xB, monkelfCallback);

	if (nRet == 0) {
		SekOpen(0);
		SekMapMemory(Drv68KRAM0,	0x70000, 0x7ffff, MAP_RAM); // only 64k

		SekMapHandler(2,		0x0e0000, 0x0e000f, MAP_READ);
		SekSetReadWordHandler(2,	monkelf_read_word);
		SekSetReadByteHandler(2,	monkelf_read_byte);
		SekClose();
	}

	return nRet;
}

struct BurnDriver BurnDrvMonkelf = {
	"monkelf", "avspirit", NULL, NULL, "1990",
	"Monky Elf (Korean bootleg of Avenging Spirit)\0", "imperfect graphics", "bootleg", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, monkelfRomInfo, monkelfRomName, NULL, NULL, CommonInputInfo, AvspiritDIPInfo,
	monkelfInit, DrvExit, System1BFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};



// E.D.F. : Earth Defense Force (set 1)

static struct BurnRomInfo edfRomDesc[] = {
	{ "edf5.b5",		0x40000, 0x105094d1, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "edf_06.rom",		0x40000, 0x94da2f0c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "edf1.f5",		0x20000, 0x2290ea19, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "edf2.f3",		0x20000, 0xce93643e, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "edf.mcu",		0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "edf_m04.rom",	0x80000, 0x6744f406, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "edf_m05.rom",	0x80000, 0x6f47e456, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "edf_09.rom",		0x20000, 0x96e38983, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "edf_m03.rom",	0x80000, 0xef469449, 6 | BRF_GRA },           //  8 Sprites

	{ "edf_m02.rom",	0x40000, 0xfc4281d2, 7 | BRF_SND },           //  9 OKI #0 Samples

	{ "edf_m01.rom",	0x40000, 0x9149286b, 8 | BRF_SND },           // 10 OKI #1 Samples

	{ "rd.20n",			0x00200, 0x1d877538, 9 | BRF_GRA },           // 11 Priority PROM
};

STD_ROM_PICK(edf)
STD_ROM_FN(edf)

static INT32 edfInit()
{
	input_select_values[0] = 0x20;
	input_select_values[1] = 0x21;
	input_select_values[2] = 0x22;
	input_select_values[3] = 0x23;
	input_select_values[4] = 0x24;

	return SystemInit(0xB, NULL);
}

struct BurnDriver BurnDrvEdf = {
	"edf", NULL, NULL, NULL, "1991",
	"E.D.F. : Earth Defense Force (set 1)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, edfRomInfo, edfRomName, NULL, NULL, CommonInputInfo, EdfDIPInfo,
	edfInit, DrvExit, System1BFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// E.D.F. : Earth Defense Force (set 2)

static struct BurnRomInfo edfaRomDesc[] = {
	{ "5.b5",			0x40000, 0x6edd3c53, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "6.b3",			0x40000, 0x4d8bfa8f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "edf1.f5",		0x20000, 0x2290ea19, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "edf2.f3",		0x20000, 0xce93643e, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "edf.mcu",		0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "edf_m04.rom",	0x80000, 0x6744f406, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "edf_m05.rom",	0x80000, 0x6f47e456, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "edf_09.rom",		0x20000, 0x96e38983, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "edf_m03.rom",	0x80000, 0xef469449, 6 | BRF_GRA },           //  8 Sprites

	{ "edf_m02.rom",	0x40000, 0xfc4281d2, 7 | BRF_SND },           //  9 OKI #0 Samples

	{ "edf_m01.rom",	0x40000, 0x9149286b, 8 | BRF_SND },           // 10 OKI #1 Samples

	{ "rd.20n",			0x00200, 0x1d877538, 9 | BRF_GRA },           // 11 Priority PROM
};

STD_ROM_PICK(edfa)
STD_ROM_FN(edfa)

struct BurnDriver BurnDrvEdfa = {
	"edfa", "edf", NULL, NULL, "1991",
	"E.D.F. : Earth Defense Force (set 2)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, edfaRomInfo, edfaRomName, NULL, NULL, CommonInputInfo, EdfDIPInfo,
	edfInit, DrvExit, System1BFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// E.D.F. : Earth Defense Force (North America)

static struct BurnRomInfo edfuRomDesc[] = {
	{ "edf5.b5",		0x40000, 0x105094d1, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "edf6.b3",		0x40000, 0x4797de97, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "edf1.f5",		0x20000, 0x2290ea19, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "edf2.f3",		0x20000, 0xce93643e, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "edf.mcu",		0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "edf_m04.rom",	0x80000, 0x6744f406, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "edf_m05.rom",	0x80000, 0x6f47e456, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "edf_09.rom",		0x20000, 0x96e38983, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "edf_m03.rom",	0x80000, 0xef469449, 6 | BRF_GRA },           //  8 Sprites

	{ "edf_m02.rom",	0x40000, 0xfc4281d2, 7 | BRF_SND },           //  9 OKI #0 Samples

	{ "edf_m01.rom",	0x40000, 0x9149286b, 8 | BRF_SND },           // 10 OKI #1 Samples

	{ "rd.20n",			0x00200, 0x1d877538, 9 | BRF_GRA },           // 11 Priority PROM
};

STD_ROM_PICK(edfu)
STD_ROM_FN(edfu)

struct BurnDriver BurnDrvEdfu = {
	"edfu", "edf", NULL, NULL, "1991",
	"E.D.F. : Earth Defense Force (North America)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, edfuRomInfo, edfuRomName, NULL, NULL, CommonInputInfo, EdfDIPInfo,
	edfInit, DrvExit, System1BFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// E.D.F. : Earth Defense Force (bootleg)

static struct BurnRomInfo edfblRomDesc[] = {
	{ "02.bin",		0x40000, 0x19a0dfa0, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "01.bin",		0x40000, 0xfc893ad0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "07.bin",		0x40000, 0x4495c228, 3 | BRF_GRA },           //  2 Tilemap #0 Tiles
	{ "06.bin",		0x40000, 0x3e37f226, 3 | BRF_GRA },           //  3

	{ "03.bin",		0x40000, 0xeea24345, 4 | BRF_GRA },           //  4 Tilemap #1 Tiles
	{ "04.bin",		0x40000, 0x2cfe9439, 4 | BRF_GRA },           //  5

	{ "05.bin",		0x20000, 0x96e38983, 5 | BRF_GRA },           //  6 Tilemap #2 Tiles

	{ "09.bin",		0x40000, 0xe89d27c0, 6 | BRF_GRA },           //  7 Sprites
	{ "08.bin",		0x40000, 0x603ac969, 6 | BRF_GRA },           //  8

	{ "12.bin",		0x10000, 0xe645f447, 7 | BRF_SND },           //  9 OKI #0 Samples

	{ "11.bin",		0x40000, 0x5a8896cb, 8 | BRF_SND },           // 10 okibanks
	{ "10.bin",		0x40000, 0xbaa7c91b, 8 | BRF_SND },           // 11

	{ "rd.20n",		0x00200, 0x1d877538, 9 | BRF_GRA },           // 12 Priority PROM
};

STD_ROM_PICK(edfbl)
STD_ROM_FN(edfbl)

static UINT16 __fastcall edfbl_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xe0002:
			return DrvInputs[0];

		case 0xe0004:
			return DrvInputs[1];

		case 0xe0006:
			return DrvInputs[2];

		case 0xe0008:
			return DrvDips[0];

		case 0xe000a:
			return DrvDips[1];
	}

	return 0;
}

static UINT8 __fastcall edfbl_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xe0002:
			return DrvInputs[0] >> 8;

		case 0xe0003:
			return DrvInputs[0];

		case 0xe0004:
			return DrvInputs[1] >> 8;

		case 0xe0005:
			return DrvInputs[1];

		case 0xe0006:
			return DrvInputs[2] >> 8;

		case 0xe0007:
			return DrvInputs[2];

		case 0xe0008:
		case 0xe0009:
			return DrvDips[0];

		case 0xe000a:
		case 0xe000b:
			return DrvDips[1];
	}

	return 0;
}

static INT32 edfblInit()
{
	INT32 nRet = SystemInit(0xB, NULL);

	if (nRet == 0) {
		SekOpen(0);
		SekMapHandler(2,		0x0e0000, 0x0e000f, MAP_READ);
		SekSetReadWordHandler(2,	edfbl_read_word);
		SekSetReadByteHandler(2,	edfbl_read_byte);
		SekClose();
	}

	return nRet;
}

struct BurnDriver BurnDrvEdfbl = {
	"edfbl", "edf", NULL, NULL, "1991",
	"E.D.F. : Earth Defense Force (bootleg)\0", "no sound", "bootleg", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, edfblRomInfo, edfblRomName, NULL, NULL, CommonInputInfo, EdfDIPInfo,
	edfblInit, DrvExit, System1BFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Hayaoshi Quiz Ouza Ketteisen - The King Of Quiz

static struct BurnRomInfo hayaosi1RomDesc[] = {
	{ "5",				0x40000, 0xeaf38fab, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "6",				0x40000, 0x341f8057, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1",				0x20000, 0xb088b27e, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "2",				0x20000, 0xcebc7b16, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "mo-91044.mcu",	0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "7",				0x80000, 0x3629c455, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "8",				0x80000, 0x15f0b2a3, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "9",				0x20000, 0x64d5b95e, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "10",				0x80000, 0x593e93d6, 6 | BRF_GRA },           //  8 Sprites

	{ "3",				0x40000, 0xf3f5787a, 7 | BRF_SND },           //  9 OKI #0 Samples

	{ "4",				0x40000, 0xac3f9bd2, 8 | BRF_SND },           // 10 OKI #1 Samples

	{ "pr-91044",		0x00200, 0xc69423d6, 9 | BRF_GRA },           // 11 Priority PROM
};

STD_ROM_PICK(hayaosi1)
STD_ROM_FN(hayaosi1)

static INT32 hayaosi1Init()
{
	input_select_values[0] = 0x51;
	input_select_values[1] = 0x52;
	input_select_values[2] = 0x53;
	input_select_values[3] = 0x54;
	input_select_values[4] = 0x55;

	INT32 nRet = SystemInit(0xB, NULL);

	if (nRet == 0) {
		MSM6295SetSamplerate(0, 2000000 / 132);
		MSM6295SetSamplerate(1, 2000000 / 132);
	}

	return nRet;
}

struct BurnDriver BurnDrvHayaosi1 = {
	"hayaosi1", NULL, NULL, NULL, "1993",
	"Hayaoshi Quiz Ouza Ketteisen - The King Of Quiz\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, hayaosi1RomInfo, hayaosi1RomName, NULL, NULL, Hayaosi1InputInfo, Hayaosi1DIPInfo,
	hayaosi1Init, DrvExit, System1BFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// 64th. Street - A Detective Story (World)

static struct BurnRomInfo Street64RomDesc[] = {
	{ "64th_03.rom",	0x40000, 0xed6c6942, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "64th_02.rom",	0x40000, 0x0621ed1d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "64th_08.rom",	0x10000, 0x632be0c1, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "64th_07.rom",	0x10000, 0x13595d01, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "64street.mcu",	0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "64th_01.rom",	0x80000, 0x06222f90, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "64th_06.rom",	0x80000, 0x2bfcdc75, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "64th_09.rom",	0x20000, 0xa4a97db4, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "64th_05.rom",	0x80000, 0xa89a7020, 6 | BRF_GRA },           //  8 Sprites
	{ "64th_04.rom",	0x80000, 0x98f83ef6, 6 | BRF_GRA },           //  9

	{ "64th_11.rom",	0x20000, 0xb0b8a65c, 7 | BRF_SND },           // 10 OKI #0 Samples

	{ "64th_10.rom",	0x40000, 0xa3390561, 8 | BRF_SND },           // 11 OKI #1 Samples

	{ "pr91009.12",		0x00200, 0xc69423d6, 9 | BRF_GRA },           // 12 Priority PROM
};

STD_ROM_PICK(Street64)
STD_ROM_FN(Street64)

static INT32 street64Init()
{
	input_select_values[0] = 0x57;
	input_select_values[1] = 0x53;
	input_select_values[2] = 0x54;
	input_select_values[3] = 0x55;
	input_select_values[4] = 0x56;

	return SystemInit(0xC, NULL);
}

struct BurnDriver BurnDrvStreet64 = {
	"64street", NULL, NULL, NULL, "1991",
	"64th. Street - A Detective Story (World)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, Street64RomInfo, Street64RomName, NULL, NULL, CommonInputInfo, Street64DIPInfo,
	street64Init, DrvExit, System1CFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// 64th. Street - A Detective Story (Japan, set 1)

static struct BurnRomInfo Street64jRomDesc[] = {
	{ "91105-3.bin",	0x40000, 0xa211a83b, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "91105-2.bin",	0x40000, 0x27c1f436, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "64th_08.rom",	0x10000, 0x632be0c1, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "64th_07.rom",	0x10000, 0x13595d01, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "64street.mcu",	0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "64th_01.rom",	0x80000, 0x06222f90, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "64th_06.rom",	0x80000, 0x2bfcdc75, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "64th_09.rom",	0x20000, 0xa4a97db4, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "64th_05.rom",	0x80000, 0xa89a7020, 6 | BRF_GRA },           //  8 Sprites
	{ "64th_04.rom",	0x80000, 0x98f83ef6, 6 | BRF_GRA },           //  9

	{ "64th_11.rom",	0x20000, 0xb0b8a65c, 7 | BRF_SND },           // 10 OKI #0 Samples

	{ "64th_10.rom",	0x40000, 0xa3390561, 8 | BRF_SND },           // 11 OKI #1 Samples

	{ "pr91009.12",		0x00200, 0xc69423d6, 9 | BRF_GRA },           // 12 Priority PROM
};

STD_ROM_PICK(Street64j)
STD_ROM_FN(Street64j)

struct BurnDriver BurnDrvStreet64j = {
	"64streetj", "64street", NULL, NULL, "1991",
	"64th. Street - A Detective Story (Japan, set 1)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, Street64jRomInfo, Street64jRomName, NULL, NULL, CommonInputInfo, Street64DIPInfo,
	street64Init, DrvExit, System1CFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// 64th. Street - A Detective Story (Japan, set 2)

static struct BurnRomInfo Street64jaRomDesc[] = {
	{ "ic53.bin",		0x40000, 0xc978d086, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "ic52.bin",		0x40000, 0xaf475852, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "64th_08.rom",	0x10000, 0x632be0c1, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "64th_07.rom",	0x10000, 0x13595d01, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "64street.mcu",	0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "64th_01.rom",	0x80000, 0x06222f90, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "64th_06.rom",	0x80000, 0x2bfcdc75, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "64th_09.rom",	0x20000, 0xa4a97db4, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "64th_05.rom",	0x80000, 0xa89a7020, 6 | BRF_GRA },           //  8 Sprites
	{ "64th_04.rom",	0x80000, 0x98f83ef6, 6 | BRF_GRA },           //  9

	{ "64th_11.rom",	0x20000, 0xb0b8a65c, 7 | BRF_SND },           // 10 OKI #0 Samples

	{ "64th_10.rom",	0x40000, 0xa3390561, 8 | BRF_SND },           // 11 OKI #1 Samples

	{ "pr91009.12",		0x00200, 0xc69423d6, 9 | BRF_GRA },           // 12 Priority PROM
};

STD_ROM_PICK(Street64ja)
STD_ROM_FN(Street64ja)

struct BurnDriver BurnDrvStreet64ja = {
	"64streetja", "64street", NULL, NULL, "1991",
	"64th. Street - A Detective Story (Japan, alt)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, Street64jaRomInfo, Street64jaRomName, NULL, NULL, CommonInputInfo, Street64DIPInfo,
	street64Init, DrvExit, System1CFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Big Striker

static struct BurnRomInfo bigstrikRomDesc[] = {
	{ "91105v11.3",		0x20000, 0x5d6e08ec, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "91105v11.2",		0x20000, 0x2120f05b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "91105v10.8",		0x10000, 0x7dd69ece, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "91105v10.7",		0x10000, 0xbc2c1508, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "bigstrik.mcu",	0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "91021-01.1",		0x80000, 0xf1945858, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "91021-03.6",		0x80000, 0xe88821e5, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "91105v11.9",		0x20000, 0x7be1c50c, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "91021-02.5",		0x80000, 0x199819ca, 6 | BRF_GRA },           //  8 Sprites

	{ "91105v10.11",	0x40000, 0x0ef8fd43, 7 | BRF_SND },           //  9 OKI #0 Samples

	{ "91105v10.10",	0x40000, 0xd273a92a, 8 | BRF_SND },           // 10 OKI #1 Samples

	{ "82s131.12",		0x00200, 0x4b00fccf, 9 | BRF_GRA },           // 11 Priority PROM
};

STD_ROM_PICK(bigstrik)
STD_ROM_FN(bigstrik)

static INT32 bigstrikInit()
{
	input_select_values[0] = 0x58;
	input_select_values[1] = 0x54;
	input_select_values[2] = 0x55;
	input_select_values[3] = 0x56;
	input_select_values[4] = 0x57;

	return SystemInit(0xC, NULL);
}

struct BurnDriver BurnDrvBigstrik = {
	"bigstrik", NULL, NULL, NULL, "1992",
	"Big Striker\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, bigstrikRomInfo, bigstrikRomName, NULL, NULL, Common3ButtonInputInfo, BigstrikDIPInfo,
	bigstrikInit, DrvExit, System1CFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Chimera Beast (prototype)

static struct BurnRomInfo chimerabRomDesc[] = {
	{ "prg3.bin",		0x40000, 0x70f1448f, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "prg2.bin",		0x40000, 0x821dbb85, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "prg8.bin",		0x10000, 0xa682b1ca, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "prg7.bin",		0x10000, 0x83b9982d, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "chimerab.mcu",	0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "s1.bin",			0x80000, 0xe4c2ac77, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "s2.bin",			0x80000, 0xfafb37a5, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "scr3.bin",		0x20000, 0x5fe38a83, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "b2.bin",			0x80000, 0x6e7f1778, 6 | BRF_GRA },           //  8 Sprites
	{ "b1.bin",			0x80000, 0x29c0385e, 6 | BRF_GRA },           //  9

	{ "voi11.bin",		0x40000, 0x14b3afe6, 7 | BRF_SND },           // 10 OKI #0 Samples

	{ "voi10.bin",		0x40000, 0x67498914, 8 | BRF_SND },           // 11 OKI #1 Samples

	{ "pr-91044",		0x00200, 0xc69423d6, 9 | BRF_GRA },        	  // 12 Priority PROM
};

STD_ROM_PICK(chimerab)
STD_ROM_FN(chimerab)

static INT32 chimerabInit()
{
	const UINT32 priority_data[16] = {
		0x14032,0x04132,0x14032,0x04132,0xfffff,0xfffff,0xfffff,0xfffff,
		0xfffff,0xfffff,0x01324,0xfffff,0xfffff,0xfffff,0xfffff,0xfffff
	};

	memcpy (m_layers_order, priority_data, 16 * sizeof(INT32));

	input_select_values[0] = 0x56;
	input_select_values[1] = 0x52;
	input_select_values[2] = 0x53;
	input_select_values[3] = 0x54;
	input_select_values[4] = 0x55;

	return SystemInit(0xC, NULL);
}

struct BurnDriver BurnDrvChimerab = {
	"chimerab", NULL, NULL, NULL, "1993",
	"Chimera Beast (prototype)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, chimerabRomInfo, chimerabRomName, NULL, NULL, CommonInputInfo, ChimerabDIPInfo,
	chimerabInit, DrvExit, System1CFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Cybattler

static struct BurnRomInfo cybattlrRomDesc[] = {
	{ "cb_03.rom",		0x40000, 0xbee20587, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "cb_02.rom",		0x40000, 0x2ed14c50, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cb_08.rom",		0x10000, 0xbf7b3558, 2 | BRF_PRG | BRF_ESS }, //  2 68k #1 Code
	{ "cb_07.rom",		0x10000, 0x85d219d7, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "cybattlr.mcu",	0x01000, 0x00000000, 0 | BRF_NODUMP },        //  4 MCU Code

	{ "cb_m01.rom",		0x80000, 0x1109337f, 3 | BRF_GRA },           //  5 Tilemap #0 Tiles

	{ "cb_m04.rom",		0x80000, 0x0c91798e, 4 | BRF_GRA },           //  6 Tilemap #1 Tiles

	{ "cb_09.rom",		0x20000, 0x37b1f195, 5 | BRF_GRA },           //  7 Tilemap #2 Tiles

	{ "cb_m03.rom",		0x80000, 0x4cd49f58, 6 | BRF_GRA },           //  8 Sprites
	{ "cb_m02.rom",		0x80000, 0x882825db, 6 | BRF_GRA },           //  9

	{ "cb_11.rom",		0x40000, 0x59d62d1f, 7 | BRF_SND },           // 10 OKI #0 Samples

	{ "cb_10.rom",		0x40000, 0x8af95eed, 8 | BRF_SND },           // 11 OKI #1 Samples

	{ "pr-91028.12",	0x00200, 0xcfe90082, 9 | BRF_GRA },           // 12 Priority PROM
};

STD_ROM_PICK(cybattlr)
STD_ROM_FN(cybattlr)

static INT32 cybattlrInit()
{
	input_select_values[0] = 0x56;
	input_select_values[1] = 0x52;
	input_select_values[2] = 0x53;
	input_select_values[3] = 0x54;
	input_select_values[4] = 0x55;

	return SystemInit(0xC, NULL);
}

struct BurnDriver BurnDrvCybattlr = {
	"cybattlr", NULL, NULL, NULL, "1993",
	"Cybattler\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, cybattlrRomInfo, cybattlrRomName, NULL, NULL, CommonInputInfo, CybattlrDIPInfo,
	cybattlrInit, DrvExit, System1CFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 256, 3, 4
};


// Legend of Makai (World)

static struct BurnRomInfo lomakaiRomDesc[] = {
	{ "lom_30.rom",		0x20000, 0xba6d65b8, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "lom_20.rom",		0x20000, 0x56a00dc2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "lom_01.rom",		0x10000, 0x46e85e90, 10 | BRF_PRG | BRF_ESS },//  2 Z80 Code

	{ "lom_05.rom",		0x20000, 0xd04fc713, 3 | BRF_GRA },           //  3 Tilemap #0 Tiles

	{ "lom_08.rom",		0x10000, 0xbdb15e67, 4 | BRF_GRA },           //  4 Tilemap #1 Tiles

	{ "lom_06.rom",		0x20000, 0xf33b6eed, 6 | BRF_GRA },           //  5 Sprites

	{ "makaiden.9",		0x00100, 0x3567065d, 0 | BRF_OPT },           //  6 Unknown PROMs
	{ "makaiden.10",	0x00100, 0xe6709c51, 0 | BRF_OPT },           //  7
};

STD_ROM_PICK(lomakai)
STD_ROM_FN(lomakai)

static INT32 lomakaiInit()
{
	return SystemInit(0x0/*Z*/, NULL);
}

struct BurnDriver BurnDrvLomakai = {
	"lomakai", NULL, NULL, NULL, "1988",
	"Legend of Makai (World)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, lomakaiRomInfo, lomakaiRomName, NULL, NULL, CommonInputInfo, LomakaiDIPInfo,
	lomakaiInit, DrvExit, System1ZFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 224, 4, 3
};


// Makai Densetsu (Japan)

static struct BurnRomInfo makaidenRomDesc[] = {
	{ "makaiden.3a",	0x20000, 0x87cf81d1, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "makaiden.2a",	0x20000, 0xd40e0fea, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "lom_01.rom",		0x10000, 0x46e85e90, 10 | BRF_PRG | BRF_ESS },//  2 Z80 Code

	{ "lom_05.rom",		0x20000, 0xd04fc713, 3 | BRF_GRA },           //  3 Tilemap #0 Tiles

	{ "makaiden.8",		0x10000, 0xa7f623f9, 4 | BRF_GRA },           //  4 Tilemap #1 Tiles

	{ "lom_06.rom",		0x20000, 0xf33b6eed, 6 | BRF_GRA },           //  5 Sprites

	{ "makaiden.9",		0x00100, 0x3567065d, 0 | BRF_OPT },           //  6 Unknown PROMs
	{ "makaiden.10",	0x00100, 0xe6709c51, 0 | BRF_OPT },           //  7
};

STD_ROM_PICK(makaiden)
STD_ROM_FN(makaiden)

struct BurnDriver BurnDrvMakaiden = {
	"makaiden", "lomakai", NULL, NULL, "1988",
	"Makai Densetsu (Japan)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, makaidenRomInfo, makaidenRomName, NULL, NULL, CommonInputInfo, LomakaiDIPInfo,
	lomakaiInit, DrvExit, System1ZFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 224, 4, 3
};


// Peek-a-Boo!

static struct BurnRomInfo peekabooRomDesc[] = {
	{ "j3",				0x020000, 0xf5f4cf33, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "j2",				0x020000, 0x7b3d430d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mo-90233.mcu",	0x001000, 0x00000000, 0 | BRF_NODUMP },        //  2 MCU Code

	{ "5",				0x080000, 0x34fa07bb, 3 | BRF_GRA },           //  3 Tilemap #0 Tiles

	{ "4",				0x020000, 0xf037794b, 4 | BRF_GRA },           //  4 Tilemap #1 Tiles

	{ "1",				0x080000, 0x5a444ecf, 6 | BRF_GRA },           //  5 Sprites

	{ "peeksamp.124",	0x100000, 0xe1206fa8, 8 | BRF_SND },           //  6 OKI #0 Samples

	{ "priority.69",	0x000200, 0xb40bff56, 9 | BRF_GRA },           //  7 Priority PROM
};

STD_ROM_PICK(peekaboo)
STD_ROM_FN(peekaboo)

static void peekabooCallback()
{
	memcpy (DrvSndROM0, DrvSndROM1, 0x40000); // set initial banks
}

static INT32 peekabooInit()
{
	return SystemInit(0xD, peekabooCallback);
}

struct BurnDriver BurnDrvPeekaboo = {
	"peekaboo", NULL, NULL, NULL, "1993",
	"Peek-a-Boo!\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, peekabooRomInfo, peekabooRomName, NULL, NULL, PeekabooInputInfo, PeekabooDIPInfo,
	peekabooInit, DrvExit, System1DFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Peek-a-Boo! (North America, ver 1.0)

static struct BurnRomInfo peekaboouRomDesc[] = {
	{ "pb92127a_3_ver1.0.ic29",	0x020000, 0x4603176a, 1 | BRF_PRG | BRF_ESS }, //  0 68k #0 Code
	{ "pb92127a_2_ver1.0.ic28",	0x020000, 0x7bf4716b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mo-90233.mcu",			0x001000, 0x00000000, 0 | BRF_NODUMP },        //  2 MCU Code

	{ "5",						0x080000, 0x34fa07bb, 3 | BRF_GRA },           //  3 Tilemap #0 Tiles

	{ "4",						0x020000, 0xf037794b, 4 | BRF_GRA },           //  4 Tilemap #1 Tiles

	{ "1",						0x080000, 0x5a444ecf, 6 | BRF_GRA },           //  5 Sprites

	{ "peeksamp.124",			0x100000, 0xe1206fa8, 8 | BRF_SND },           //  6 OKI #0 Samples

	{ "priority.69",			0x000200, 0xb40bff56, 9 | BRF_GRA },           //  7 Priority PROM
};

STD_ROM_PICK(peekaboou)
STD_ROM_FN(peekaboou)

struct BurnDriver BurnDrvPeekaboou = {
	"peekaboou", "peekaboo", NULL, NULL, "1993",
	"Peek-a-Boo! (North America, ver 1.0)\0", NULL, "Jaleco", "Mega System 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, peekaboouRomInfo, peekaboouRomName, NULL, NULL, PeekabooInputInfo, PeekabooDIPInfo,
	peekabooInit, DrvExit, System1DFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};
