// FB Alpha Bagman driver module
// based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "resnet.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "tms5110.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvCtrlPROM;
static UINT8 *DrvTMSPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen[2];
static UINT8 irq_mask;
static UINT8 video_enable;

static UINT8 pal16r6_andmap[64];
static UINT8 pal16r6_columnvalue[32];
static UINT8 pal16r6_outvalue[8];

static INT32 speech_rom_address;
static UINT8 ls259_buf[8];

static INT32 botanic_input_xor = 0;

static INT32 squaitsamode = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8]; // paddle squaitsa
static UINT8 DrvDips[1];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static UINT8 PaddleX[2] = { 0, 0 }; // squaitsa paddle sheitsa
static UINT8 m_p1_res;
static UINT8 m_p2_res;
static INT32 m_p1_old_val;
static INT32 m_p2_old_val;

static struct BurnInputInfo BagmanInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Bagman)

static struct BurnInputInfo SbagmanInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Sbagman)

static struct BurnInputInfo SquaitsaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Squaitsa)

static struct BurnDIPInfo BagmanDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0f, 0x01, 0x03, 0x03, "2"				},
	{0x0f, 0x01, 0x03, 0x02, "3"				},
	{0x0f, 0x01, 0x03, 0x01, "4"				},
	{0x0f, 0x01, 0x03, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x0f, 0x01, 0x04, 0x00, "2C/1C 1C/1C 1C/3C 1C/7C"	},
	{0x0f, 0x01, 0x04, 0x04, "1C/1C 1C/2C 1C/6C 1C/14C"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0f, 0x01, 0x18, 0x18, "Easy"				},
	{0x0f, 0x01, 0x18, 0x10, "Medium"			},
	{0x0f, 0x01, 0x18, 0x08, "Hard"				},
	{0x0f, 0x01, 0x18, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x0f, 0x01, 0x20, 0x20, "English"			},
	{0x0f, 0x01, 0x20, 0x00, "French"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0f, 0x01, 0x40, 0x40, "30000"			},
	{0x0f, 0x01, 0x40, 0x00, "40000"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0f, 0x01, 0x80, 0x80, "Upright"			},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"			},
};

STDDIPINFO(Bagman)

static struct BurnDIPInfo BagmansDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0f, 0x01, 0x03, 0x03, "2"				},
	{0x0f, 0x01, 0x03, 0x02, "3"				},
	{0x0f, 0x01, 0x03, 0x01, "4"				},
	{0x0f, 0x01, 0x03, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x0f, 0x01, 0x04, 0x00, "2C/1C 1C/1C 1C/3C 1C/7C"	},
	{0x0f, 0x01, 0x04, 0x04, "1C/1C 1C/2C 1C/6C 1C/14C"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0f, 0x01, 0x18, 0x18, "Easy"				},
	{0x0f, 0x01, 0x18, 0x10, "Medium"			},
	{0x0f, 0x01, 0x18, 0x08, "Hard"				},
	{0x0f, 0x01, 0x18, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0f, 0x01, 0x20, 0x00, "Off"  			},
	{0x0f, 0x01, 0x20, 0x20, "On"   			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0f, 0x01, 0x40, 0x40, "30000"			},
	{0x0f, 0x01, 0x40, 0x00, "40000"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0f, 0x01, 0x80, 0x80, "Upright"			},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"			},
};

STDDIPINFO(Bagmans)

static struct BurnDIPInfo SbagmanDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0f, 0x01, 0x03, 0x03, "2"				},
	{0x0f, 0x01, 0x03, 0x02, "3"				},
	{0x0f, 0x01, 0x03, 0x01, "4"				},
	{0x0f, 0x01, 0x03, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x0f, 0x01, 0x04, 0x00, "2C/1C 1C/1C 1C/3C 1C/7C"	},
	{0x0f, 0x01, 0x04, 0x04, "1C/1C 1C/2C 1C/6C 1C/14C"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0f, 0x01, 0x18, 0x18, "Easy"				},
	{0x0f, 0x01, 0x18, 0x10, "Medium"			},
	{0x0f, 0x01, 0x18, 0x08, "Hard"				},
	{0x0f, 0x01, 0x18, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x0f, 0x01, 0x20, 0x20, "English"			},
	{0x0f, 0x01, 0x20, 0x00, "French"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0f, 0x01, 0x40, 0x40, "30000"			},
	{0x0f, 0x01, 0x40, 0x00, "40000"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0f, 0x01, 0x80, 0x80, "Upright"			},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"			},
};

STDDIPINFO(Sbagman)

static struct BurnDIPInfo PickinDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xcd, NULL				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x0f, 0x01, 0x01, 0x00, "2C/1C 1C/1C 1C/3C 1C/7C"	},
	{0x0f, 0x01, 0x01, 0x01, "1C/1C 1C/2C 1C/6C 1C/14C"	},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0f, 0x01, 0x06, 0x06, "2"				},
	{0x0f, 0x01, 0x06, 0x04, "3"				},
	{0x0f, 0x01, 0x06, 0x02, "4"				},
	{0x0f, 0x01, 0x06, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0f, 0x01, 0x08, 0x08, "Off"				},
	{0x0f, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x0f, 0x01, 0x40, 0x40, "English"			},
	{0x0f, 0x01, 0x40, 0x00, "French"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0f, 0x01, 0x80, 0x80, "Upright"			},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"			},
};

STDDIPINFO(Pickin)

static struct BurnDIPInfo BotanicfDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x8a, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0f, 0x01, 0x03, 0x00, "1"				},
	{0x0f, 0x01, 0x03, 0x03, "2"				},
	{0x0f, 0x01, 0x03, 0x02, "3"				},
	{0x0f, 0x01, 0x03, 0x01, "4"				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x0f, 0x01, 0x04, 0x00, "1C/1C 1C/2C 1C/6C 1C/14C"	},
	{0x0f, 0x01, 0x04, 0x04, "2C/1C 1C/2C 1C/3C 1C/7C"	},

	{0   , 0xfe, 0   ,    2, "Invulnerability Fruits"	},
	{0x0f, 0x01, 0x08, 0x08, "3"				},
	{0x0f, 0x01, 0x08, 0x00, "None"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0f, 0x01, 0x80, 0x80, "Upright"			},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"			},
};

STDDIPINFO(Botanicf)

static struct BurnDIPInfo BotaniciDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xba, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0f, 0x01, 0x03, 0x00, "1"				},
	{0x0f, 0x01, 0x03, 0x03, "2"				},
	{0x0f, 0x01, 0x03, 0x02, "3"				},
	{0x0f, 0x01, 0x03, 0x01, "4"				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x0f, 0x01, 0x04, 0x00, "1C/1C 1C/2C"		},
	{0x0f, 0x01, 0x04, 0x04, "2C/1C 1C/2C"		},

	{0   , 0xfe, 0   ,    4, "Invulnerability Fruits"	},
	{0x0f, 0x01, 0x18, 0x00, "2"				},
	{0x0f, 0x01, 0x18, 0x08, "3"				},
	{0x0f, 0x01, 0x18, 0x10, "3 (duplicate 1)"	},
	{0x0f, 0x01, 0x18, 0x18, "3 (duplicate 2)"	},

	{0   , 0xfe, 0   ,    2, "Language / Disable Invlunerability Fruits"	},
	{0x0f, 0x01, 0x20, 0x20, "Fruits On, English"		},
	{0x0f, 0x01, 0x20, 0x00, "Fruits Off, Spanish"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0f, 0x01, 0x80, 0x80, "Upright"			},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"			},
};

STDDIPINFO(Botanici)

static struct BurnDIPInfo SquaitsaDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x5f, NULL				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x0f, 0x01, 0x01, 0x00, "2 Coins 1 Credits"},
	{0x0f, 0x01, 0x01, 0x01, "1 Coin  1 Credits"},

	{0   , 0xfe, 0   ,    4, "Max Points"		},
	{0x0f, 0x01, 0x06, 0x06, "7"				},
	{0x0f, 0x01, 0x06, 0x04, "11"				},
	{0x0f, 0x01, 0x06, 0x02, "15"				},
	{0x0f, 0x01, 0x06, 0x00, "21"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0f, 0x01, 0x18, 0x00, "Level 1"			},
	{0x0f, 0x01, 0x18, 0x08, "Level 2"			},
	{0x0f, 0x01, 0x18, 0x10, "Level 3"			},
	{0x0f, 0x01, 0x18, 0x18, "Level 4"			},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x0f, 0x01, 0x20, 0x20, "Spanish"			},
	{0x0f, 0x01, 0x20, 0x00, "English"			},

	{0   , 0xfe, 0   ,    2, "Body Fault"		},
	{0x0f, 0x01, 0x40, 0x40, "Off"				},
	{0x0f, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Protection?"		},
	{0x0f, 0x01, 0x80, 0x80, "Off"				},
	{0x0f, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Squaitsa)

static void update_pal()
{
	UINT8 row, val;

	static const UINT32 fusemap[64] = {
		0xffffffff, 0xfbb7b7bf, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0xffffffbb, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0xfffffbbb, 0xfffff77b, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0xffffbbbb, 0xffff7f7b, 0xffff77fb, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0xfffbbbbb, 0xfff7ff7b, 0xfff7f7fb, 0xfff77ffb, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0xffbbbbbb, 0xff7fff7b, 0xff7ff7fb, 0xff7f7ffb, 0xff77fffb, 0x00000000, 0x00000000, 0x00000000,
		0xfbbbbbbb, 0xf7ffff7b, 0xf7fff7fb, 0xf7ff7ffb, 0xf7f7fffb, 0xf77ffffb, 0x00000000, 0x00000000,
		0xffffffff, 0xefffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	};

	for (row = 0; row < 64; row++)
	{
		val = 1;
		for (UINT8 column = 0; column < 32; column++)
		{
			INT32 z = (fusemap[row] >> column) & 1;
			if ( z == 0 )
				val &= pal16r6_columnvalue[column];
		}
		pal16r6_andmap[row] = val;
	}

	for (row = 1, val = 0; row < 8; row++)
		val |= pal16r6_andmap[row];
	if (pal16r6_andmap[0] == 1)
	{
		pal16r6_columnvalue[2] = 1-val;
		pal16r6_columnvalue[3] = val;
		pal16r6_outvalue[0]    = 1-val;
	}
	else
	{
		pal16r6_columnvalue[2] = 0;
		pal16r6_columnvalue[3] = 1;
	}

	for (INT32 i = 0; i < 6; i++)
	{
		for (row = 8, val = 0; row < 16; row++)
			val |= pal16r6_andmap[row + (8 * i)];
		pal16r6_columnvalue[6 + (i * 4)] = 1-val;
		pal16r6_columnvalue[7 + (i * 4)] = val;
		pal16r6_outvalue[1 + i] = 1-val;
	}

	for (row = 57, val = 0; row < 64; row++)
		val |= pal16r6_andmap[row];
	if (pal16r6_andmap[56] == 1)
	{
		pal16r6_columnvalue[30] = 1-val;
		pal16r6_columnvalue[31] = val;
		pal16r6_outvalue[7]     = 1-val;
	}
	else
	{
		pal16r6_columnvalue[30] = 0;
		pal16r6_columnvalue[31] = 1;
	}
}

static void pal16r6_write(UINT8 offset, UINT8 data)
{
	UINT8 line = offset * 4;
	pal16r6_columnvalue[line + 0] = data & 1;
	pal16r6_columnvalue[line + 1] = 1 - (data & 1);
}

static UINT8 pal16r6_read()
{
	update_pal();

	return (pal16r6_outvalue[6]) + (pal16r6_outvalue[5] << 1) + (pal16r6_outvalue[4] << 2) +
		(pal16r6_outvalue[3] << 3) + (pal16r6_outvalue[2] << 4) + (pal16r6_outvalue[1] << 5);
}

static void pal16r6_reset()
{
	memset(&pal16r6_columnvalue, 0, sizeof(pal16r6_columnvalue));

	for (INT32 i = 0; i < 8; i++) {
		pal16r6_write(i, 1);
	}

	update_pal();
}

static void speech_start()
{
	speech_rom_address = 0x0;

	tms5110_CTL_set(TMS5110_CMD_SPEAK);
	tms5110_PDC_set(0);
	tms5110_PDC_set(1);
	tms5110_PDC_set(0);
}

static void speech_reset()
{
	tms5110_CTL_set(TMS5110_CMD_RESET);
	tms5110_PDC_set(0);
	tms5110_PDC_set(1);
	tms5110_PDC_set(0);

	tms5110_PDC_set(0);
	tms5110_PDC_set(1);
	tms5110_PDC_set(0);

	tms5110_PDC_set(0);
	tms5110_PDC_set(1);
	tms5110_PDC_set(0);

	speech_rom_address = 0x0;
}

static INT32 bagman_TMS5110_M0_cb()
{
	UINT8 *ROM = DrvTMSPROM;
	INT32 bit_no = (ls259_buf[0] << 2) | (ls259_buf[1] << 1) | (ls259_buf[2] << 0);
	UINT8 byte = 0;

	if (ls259_buf[4] == 0) {
		byte |= ROM[ speech_rom_address + 0x0000 ];
	}

	if (ls259_buf[5] == 0 ) {
		byte |= ROM[ speech_rom_address + 0x1000 ];
	}

	speech_rom_address++;
	speech_rom_address &= 0x0fff;

	return (byte >> (bit_no ^ 0x7)) & 1;
}

static void __fastcall bagman_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x9c00) return; // nop

	switch (address)
	{
		case 0xa000:
			irq_mask = data & 1;
			if (irq_mask == 0)
				ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0xa001:
			flipscreen[0] = data & 1; // x
		return;

		case 0xa002:
			flipscreen[1] = data & 1; // y
		return;

		case 0xa003:
			video_enable = data & 1;
		return;
		
		case 0xa004:
			// coin counter
		return;

		case 0xa007:
			// nop
		return;

		case 0xa800:
		case 0xa801:
		case 0xa802:
		case 0xa803:
		case 0xa804:
		case 0xa805:
		case 0xa806:
		case 0xa807:
			pal16r6_write(address & 7, data);

			if (ls259_buf[address & 7] != (data & 1)) {
				ls259_buf[address & 7] = data & 1;

				if ((address & 0x03) == 0x03) {
					if (ls259_buf[3] == 0) {
						speech_reset();
					} else {
						speech_start();
					}
				}
			}
		return;
	}
}

static UINT8 __fastcall bagman_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return pal16r6_read();

		case 0xb000:
			return DrvDips[0];

		case 0xb800:
			return 0; // nop (watchdog?)
	}

	return 0;
}

static void __fastcall pickin_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xb000:
			AY8910Write(1, 0, data);
		return;

		case 0xb800:
			AY8910Write(1, 1, data);
		return;
	}

	bagman_main_write(address, data);
}

static UINT8 __fastcall pickin_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa800:
			return DrvDips[0];

		case 0xb800:
			return AY8910Read(1);
	}

	return 0;
}

static void __fastcall bagman_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x08:
		case 0x09:
			AY8910Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall bagman_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x0c:
			return AY8910Read(0);
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] | ((attr & 0x20) << 3) | ((attr & 0x10) << 5);

	TILE_SET_INFO(0, code, attr, 0);
}

static UINT8 ay8910_read_A(UINT32)
{
	if (squaitsamode) {
		UINT8 dial_val = PaddleX[0];

		if(m_p1_res != 0x60)
			m_p1_res = 0x60;
		else if(dial_val > m_p1_old_val)
			m_p1_res = 0x40;
		else if(dial_val < m_p1_old_val)
			m_p1_res = 0x20;
		else
			m_p1_res = 0x60;

		m_p1_old_val = dial_val;

		return (DrvInputs[0] & 0x9f) | (m_p1_res);
	} else {
		return DrvInputs[0];
	}
}

static UINT8 ay8910_read_B(UINT32)
{
	if (squaitsamode) {
		UINT8 dial_val = PaddleX[1];

		if(m_p2_res != 0x60)
			m_p2_res = 0x60;
		else if(dial_val > m_p2_old_val)
			m_p2_res = 0x40;
		else if(dial_val < m_p2_old_val)
			m_p2_res = 0x20;
		else
			m_p2_res = 0x60;

		m_p2_old_val = dial_val;

		return (DrvInputs[1] & 0x9f) | (m_p2_res);
	} else {
		return DrvInputs[1];
	}
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	tms5110_reset();

	pal16r6_reset();

	video_enable = 1;
	irq_mask = 0;
	flipscreen[0] = 0;
	flipscreen[1] = 0;

	speech_rom_address = 0;
	memset(&ls259_buf, 0, sizeof(ls259_buf));

	PaddleX[0] = PaddleX[1] = 0;
	m_p1_old_val = m_p2_old_val = 0;
	m_p1_res = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000060;

	DrvCtrlPROM		= Next; Next += 0x000020;

	DrvTMSPROM		= Next; Next += 0x002000;

	DrvPalette		= (UINT32*)Next; Next += 0x0040 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvSprRAM 		= DrvColRAM; // 0-1f

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2] = { 0, 0x10000 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x4000);

	GfxDecode(0x0400, 2,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);
	GfxDecode(0x0080, 2, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 BagmanCommonInit(INT32 game, INT32 memmap)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		switch (game)
		{
			case 0: // bagman
			{
				if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x3000,  3, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x4000,  4, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x5000,  5, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM0 + 0x0000,  6, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x1000,  8, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x2000,  7, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x3000,  9, 1)) return 1;

				if (BurnLoadRom(DrvColPROM + 0x0000, 10, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x0020, 11, 1)) return 1;

				if (BurnLoadRom(DrvCtrlPROM + 0x000, 12, 1)) return 1;

				if (BurnLoadRom(DrvTMSPROM + 0x0000, 13, 1)) return 1;
				if (BurnLoadRom(DrvTMSPROM + 0x1000, 14, 1)) return 1;
			}
			break;

			case 1: // sbagman
			{
				if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x3000,  3, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x4000,  4, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x5000,  5, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x6000,  6, 1)) return 1;
				memcpy (DrvZ80ROM + 0xc000, DrvZ80ROM + 0x6000, 0x0e00);
				memcpy (DrvZ80ROM + 0xfe00, DrvZ80ROM + 0x6e00, 0x0200);
				if (BurnLoadRom(DrvZ80ROM  + 0x6000,  7, 1)) return 1;
				memcpy (DrvZ80ROM + 0xd000, DrvZ80ROM + 0x6000, 0x0400);
				memcpy (DrvZ80ROM + 0xe400, DrvZ80ROM + 0x6400, 0x0200);
				memcpy (DrvZ80ROM + 0xd600, DrvZ80ROM + 0x6600, 0x0a00);
				if (BurnLoadRom(DrvZ80ROM  + 0x6000,  8, 1)) return 1;
				memcpy (DrvZ80ROM + 0xe000, DrvZ80ROM + 0x6000, 0x0400);
				memcpy (DrvZ80ROM + 0xd400, DrvZ80ROM + 0x6400, 0x0200);
				memcpy (DrvZ80ROM + 0xe600, DrvZ80ROM + 0x6600, 0x0a00);
				if (BurnLoadRom(DrvZ80ROM  + 0x6000,  9, 1)) return 1;
				memcpy (DrvZ80ROM + 0xf000, DrvZ80ROM + 0x6000, 0x0e00);
				memcpy (DrvZ80ROM + 0xce00, DrvZ80ROM + 0x6e00, 0x0200);

				if (BurnLoadRom(DrvGfxROM0 + 0x0000, 10, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x1000, 12, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x2000, 11, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x3000, 13, 1)) return 1;

				if (BurnLoadRom(DrvColPROM + 0x0000, 14, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x0020, 15, 1)) return 1;

				if (BurnLoadRom(DrvCtrlPROM + 0x000, 16, 1)) return 1;

				if (BurnLoadRom(DrvTMSPROM + 0x0000, 17, 1)) return 1;
				if (BurnLoadRom(DrvTMSPROM + 0x1000, 18, 1)) return 1;
			}
			break;

			case 2: // bagmani
			{
				if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x3000,  3, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x4000,  4, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x5000,  5, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0xc000,  6, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0xd000,  7, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0xe000,  8, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0xf000,  9, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM0 + 0x0000, 10, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x1000, 12, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x2000, 11, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x3000, 13, 1)) return 1;

				if (BurnLoadRom(DrvColPROM + 0x0000, 14, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x0020, 15, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x0040, 16, 1)) return 1;
			}
			break;

			case 3: // pickin
			{
				if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x3000,  3, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x4000,  4, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x5000,  5, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM0 + 0x0000,  6, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x1000,  6, 1)) return 1; // reload
				if (BurnLoadRom(DrvGfxROM0 + 0x2000,  7, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x3000,  7, 1)) return 1; // reload

				if (BurnLoadRom(DrvColPROM + 0x0000,  8, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x0020,  9, 1)) return 1;
			}
			break;

			case 4: // botanic
			{
				if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x3000,  3, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x4000,  4, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x5000,  5, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM0 + 0x0000,  6, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x2000,  7, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x1000,  8, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x3000,  9, 1)) return 1;

				if (BurnLoadRom(DrvColPROM + 0x0000, 10, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x0020, 11, 1)) return 1;
			}
			break;

			case 5: // squaitsa
			{
				if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM0 + 0x0000,  3, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x2000,  4, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x1000,  5, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM0 + 0x3000,  6, 1)) return 1;

				if (BurnLoadRom(DrvColPROM + 0x0000,  7, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x0020,  8, 1)) return 1;
			}
			break;
		}

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0x6000, 0x67ff, MAP_RAM); // bagman
	ZetMapMemory(DrvZ80RAM,				0x7000, 0x77ff, MAP_RAM); // pickin
	ZetMapMemory(DrvVidRAM,				0x8800, 0x8bff, MAP_RAM); // pickin
	ZetMapMemory(DrvVidRAM,				0x9000, 0x93ff, MAP_RAM); // bagman
	ZetMapMemory(DrvColRAM,				0x9800, 0x9bff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0xc000,	0xc000, 0xffff, MAP_ROM); // sbagman
	ZetSetWriteHandler(memmap ? pickin_main_write : bagman_main_write);
	ZetSetReadHandler(memmap ? pickin_main_read : bagman_main_read);
	ZetSetOutHandler(bagman_main_write_port);
	ZetSetInHandler(bagman_main_read_port);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 0);
	AY8910SetPorts(0, &ay8910_read_A, &ay8910_read_B, NULL, NULL);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);

	tms5110_init(640000);
	tms5110_M0_callback(bagman_TMS5110_M0_cb);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x10000, 0, 0xf);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	tms5110_exit();

	BurnFree(AllMem);
	
	botanic_input_xor = 0;
	squaitsamode = 0;

	return 0;
}

static void DrvPaletteInit()
{
	static const INT32 resistances_rg[3] = { 1000, 470, 220 };
	static const INT32 resistances_b[2] = { 470, 220 };
	double weights_r[3], weights_g[3], weights_b[2];

	compute_resistor_weights(0, 255,    -1.0,
							 3, resistances_rg, weights_r, 470, 0,
							 3, resistances_rg, weights_g, 470, 0,
							 2, resistances_b, weights_b, 470, 0);

	for (INT32 i = 0; i < 0x40; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = combine_3_weights(weights_r, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = combine_3_weights(weights_g, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = combine_2_weights(weights_b, bit0, bit1);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x20 - 4; offs >= 0; offs -= 4)
	{
		INT32 sx = DrvSprRAM[offs + 3];
		INT32 sy = DrvSprRAM[offs + 2];
		if ((sx && sy) == 0) continue;

		INT32 color = DrvSprRAM[offs + 1] & 0x1f;
		INT32 code  =(DrvSprRAM[offs] & 0x3f) | ((DrvSprRAM[offs + 1] & 0x20) << 1);
		INT32 flipx = DrvSprRAM[offs] & 0x40;
		INT32 flipy = DrvSprRAM[offs] & 0x80;

		sy = 256 - sy - 16;

		if (flipscreen[0] && flipscreen[1])
		{
			sx = 256 - sx - 15;
			sy = 255 - sy - 15;
			flipx = !flipx;
			flipy = !flipy;
		}

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 16, flipx, flipy, color, 2, 0, 0, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();
	
//	if (video_enable)
	{
		GenericTilemapSetFlip(0, (flipscreen[0] ? TMAP_FLIPX : 0) | (flipscreen[1] ? TMAP_FLIPY : 0));
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

#if 0 // if ((sx && sy) == 0) fixes the need for this in draw_sprites. maybe
		if (!squaitsamode) {
			if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
				GenericTilesSetClip(16, -1, -1, -1);
			} else {
				GenericTilesSetClip(-1, nScreenWidth - 16, -1, -1);
			}
		}
#endif
		if (nBurnLayer & 2) draw_sprites();
		GenericTilesClearClip();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff ^ botanic_input_xor;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		if (DrvJoy3[0]) PaddleX[0] += 5;
		if (DrvJoy3[1]) PaddleX[0] -= 5;
		if (DrvJoy3[2]) PaddleX[1] += 5;
		if (DrvJoy3[3]) PaddleX[1] -= 5;
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal = 3072000 / 60;
	INT32 nCyclesDone = 0;
	INT32 nSoundBufferPos = 0;

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone += ZetRun(nCyclesTotal / nInterleave);

		if (i == 240 && irq_mask) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);

		// Render Sound Segment
		if (pBurnSoundOut && i&1) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave/2);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	ZetClose();

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(pSoundBuf, nSegmentLength);
		}

		tms5110_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
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
		AY8910Scan(nAction, pnMin);
		tms5110_scan(nAction, pnMin);

		SCAN_VAR(irq_mask);
		SCAN_VAR(flipscreen[0]);
		SCAN_VAR(flipscreen[1]);
		SCAN_VAR(video_enable);
		SCAN_VAR(speech_rom_address);
		SCAN_VAR(ls259_buf);
		SCAN_VAR(PaddleX);
		SCAN_VAR(m_p1_old_val);
		SCAN_VAR(m_p2_old_val);
		SCAN_VAR(m_p1_res);
		SCAN_VAR(m_p2_res);
	}

	return 0;
}


// Bagman

static struct BurnRomInfo bagmanRomDesc[] = {
	{ "e9_b05.bin",		0x1000, 0xe0156191, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "f9_b06.bin",		0x1000, 0x7b758982, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "f9_b07.bin",		0x1000, 0x302a077b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "k9_b08.bin",		0x1000, 0xf04293cb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "m9_b09s.bin",	0x1000, 0x68e83e4f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "n9_b10.bin",		0x1000, 0x1d6579f7, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "e1_b02.bin",		0x1000, 0x4a0a6b55, 2 | BRF_GRA },           //  6 Graphics
	{ "j1_b04.bin",		0x1000, 0xc680ef04, 2 | BRF_GRA },           //  7
	{ "c1_b01.bin",		0x1000, 0x705193b2, 2 | BRF_GRA },           //  8
	{ "f1_b03s.bin",	0x1000, 0xdba1eda7, 2 | BRF_GRA },           //  9

	{ "p3.bin",			0x0020, 0x2a855523, 4 | BRF_GRA },           // 10 Color Data
	{ "r3.bin",			0x0020, 0xae6f1019, 4 | BRF_GRA },           // 11

	{ "r6.bin",			0x0020, 0xc58a4f6a, 5 | BRF_GRA },           // 12 TMS5110 State Machine

	{ "r9_b11.bin",		0x1000, 0x2e0057ff, 6 | BRF_GRA },           // 13 TMS5110 Speech Data
	{ "t9_b12.bin",		0x1000, 0xb2120edd, 6 | BRF_GRA },           // 14
};

STD_ROM_PICK(bagman)
STD_ROM_FN(bagman)

static INT32 BagmanInit()
{
	return BagmanCommonInit(0, 0);
}

struct BurnDriver BurnDrvBagman = {
	"bagman", NULL, NULL, NULL, "1982",
	"Bagman\0", NULL, "Valadon Automation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bagmanRomInfo, bagmanRomName, NULL, NULL, NULL, NULL, BagmanInputInfo, BagmanDIPInfo,
	BagmanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
}; 


// Le Bagnard (set 1)

static struct BurnRomInfo bagnardRomDesc[] = {
	{ "e9_b05.bin",		0x1000, 0xe0156191, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "f9_b06.bin",		0x1000, 0x7b758982, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "f9_b07.bin",		0x1000, 0x302a077b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "k9_b08.bin",		0x1000, 0xf04293cb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bagnard.009",	0x1000, 0x4f0088ab, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bagnard.010",	0x1000, 0xcd2cac01, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "e1_b02.bin",		0x1000, 0x4a0a6b55, 2 | BRF_GRA },           //  6 Graphics
	{ "j1_b04.bin",		0x1000, 0xc680ef04, 2 | BRF_GRA },           //  7
	{ "bagnard.001",	0x1000, 0x060b044c, 2 | BRF_GRA },           //  8
	{ "bagnard.003",	0x1000, 0x8043bc1a, 2 | BRF_GRA },           //  9

	{ "p3.bin",			0x0020, 0x2a855523, 3 | BRF_GRA },           // 10 Color Data
	{ "r3.bin",			0x0020, 0xae6f1019, 3 | BRF_GRA },           // 11

	{ "r6.bin",			0x0020, 0xc58a4f6a, 4 | BRF_SND },           // 12 TMS5110 State Machine

	{ "r9_b11.bin",		0x1000, 0x2e0057ff, 5 | BRF_SND },           // 13 TMS5110 Speech Data
	{ "t9_b12.bin",		0x1000, 0xb2120edd, 5 | BRF_SND },           // 14
};

STD_ROM_PICK(bagnard)
STD_ROM_FN(bagnard)

struct BurnDriver BurnDrvBagnard = {
	"bagnard", "bagman", NULL, NULL, "1982",
	"Le Bagnard (set 1)\0", NULL, "Valadon Automation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bagnardRomInfo, bagnardRomName, NULL, NULL, NULL, NULL, BagmanInputInfo, BagmanDIPInfo,
	BagmanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Le Bagnard (set 2)

static struct BurnRomInfo bagnardaRomDesc[] = {
	{ "bagman.005",		0x1000, 0x98fca49c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "bagman.006",		0x1000, 0x8f447432, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bagman.007",		0x1000, 0x236203a6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bagman.008",		0x1000, 0x8bd8c6cb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bagman.009",		0x1000, 0x6211ba82, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bagman.010",		0x1000, 0x08ed1247, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "bagman.002",		0x1000, 0x7dc57abc, 2 | BRF_GRA },           //  6 Graphics
	{ "bagman.004",		0x1000, 0x1e21577e, 2 | BRF_GRA },           //  7
	{ "bagman.001",		0x1000, 0x1eb56acd, 2 | BRF_GRA },           //  8
	{ "bagman.003",		0x1000, 0x0ad82a39, 2 | BRF_GRA },           //  9

	{ "p3.bin",			0x0020, 0x2a855523, 3 | BRF_GRA },           // 10 Color Data
	{ "r3.bin",			0x0020, 0xae6f1019, 3 | BRF_GRA },           // 11

	{ "r6.bin",			0x0020, 0xc58a4f6a, 4 | BRF_SND },           // 12 TMS5110 State Machine

	{ "r9_b11.bin",		0x1000, 0x2e0057ff, 5 | BRF_SND },           // 13 TMS5110 Speech Data
	{ "t9_b12.bin",		0x1000, 0xb2120edd, 5 | BRF_SND },           // 14
};

STD_ROM_PICK(bagnarda)
STD_ROM_FN(bagnarda)

struct BurnDriver BurnDrvBagnarda = {
	"bagnarda", "bagman", NULL, NULL, "1982",
	"Le Bagnard (set 2)\0", NULL, "Valadon Automation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bagnardaRomInfo, bagnardaRomName, NULL, NULL, NULL, NULL, BagmanInputInfo, BagmanDIPInfo,
	BagmanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Le Bagnard (Itisa, Spain)

static struct BurnRomInfo bagnardiRomDesc[] = {
	{ "bagnardi_05.e9",	0x1000, 0xe0156191, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "bagnardi_06.f9",	0x1000, 0x2e98c072, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bagnardi_07.j9",	0x1000, 0x698f17b3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bagnardi_08.k9",	0x1000, 0xf212e287, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bagnardi_09.m9",	0x1000, 0x4f0088ab, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bagnardi_10.n9",	0x1000, 0x423c54be, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "bagnardi_02.e1",	0x1000, 0x4a0a6b55, 2 | BRF_GRA },           //  6 Graphics
	{ "bagnardi_04.j1",	0x1000, 0xc680ef04, 2 | BRF_GRA },           //  7
	{ "bagnardi_01.c1",	0x1000, 0x060b044c, 2 | BRF_GRA },           //  8
	{ "bagnardi_03.f1",	0x1000, 0x8043bc1a, 2 | BRF_GRA },           //  9

	{ "p3.bin",			0x0020, 0x2a855523, 3 | BRF_GRA },           // 10 Color Data
	{ "r3.bin",			0x0020, 0xae6f1019, 3 | BRF_GRA },           // 11

	{ "r6.bin",			0x0020, 0xc58a4f6a, 4 | BRF_SND },           // 12 TMS5110 State Machine

	{ "bagnardi_11.r9",	0x1000, 0x2e0057ff, 5 | BRF_SND },           // 13 TMS5110 Speech Data
	{ "bagnardi_12.t9",	0x1000, 0xb2120edd, 5 | BRF_SND },           // 14
};

STD_ROM_PICK(bagnardi)
STD_ROM_FN(bagnardi)

struct BurnDriver BurnDrvBagnardi = {
	"bagnardi", "bagman", NULL, NULL, "1982",
	"Le Bagnard (Itisa, Spain)\0", NULL, "Valadon Automation (Itisa license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bagnardiRomInfo, bagnardiRomName, NULL, NULL, NULL, NULL, BagmanInputInfo, BagmanDIPInfo,
	BagmanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Bagman (Stern Electronics, set 1)

static struct BurnRomInfo bagmansRomDesc[] = {
	{ "a4_9e.bin",		0x1000, 0x5fb0a1a3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "a5-9f",			0x1000, 0x2ddf6bb9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a4_9j.bin",		0x1000, 0xb2da8b77, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a5-9k",			0x1000, 0xf91d617b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a4_9m.bin",		0x1000, 0xb8e75eb6, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "a5-9n",			0x1000, 0x68e4b64d, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "a2_1e.bin",		0x1000, 0xf217ac09, 2 | BRF_GRA },           //  6 Graphics
	{ "j1_b04.bin",		0x1000, 0xc680ef04, 2 | BRF_GRA },           //  7
	{ "a2_1c.bin",		0x1000, 0xf3e11bd7, 2 | BRF_GRA },           //  8
	{ "a2_1f.bin",		0x1000, 0xd0f7105b, 2 | BRF_GRA },           //  9

	{ "p3.bin",			0x0020, 0x2a855523, 3 | BRF_GRA },           // 10 Color Data
	{ "r3.bin",			0x0020, 0xae6f1019, 3 | BRF_GRA },           // 11

	{ "r6.bin",			0x0020, 0xc58a4f6a, 4 | BRF_SND },           // 12 TMS5110 State Machine

	{ "r9_b11.bin",		0x1000, 0x2e0057ff, 5 | BRF_SND },           // 13 TMS5110 Speech Data
	{ "t9_b12.bin",		0x1000, 0xb2120edd, 5 | BRF_SND },           // 14
};

STD_ROM_PICK(bagmans)
STD_ROM_FN(bagmans)

struct BurnDriver BurnDrvBagmans = {
	"bagmans", "bagman", NULL, NULL, "1982",
	"Bagman (Stern Electronics, set 1)\0", NULL, "Valadon Automation (Stern Electronics license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bagmansRomInfo, bagmansRomName, NULL, NULL, NULL, NULL, BagmanInputInfo, BagmansDIPInfo,
	BagmanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Bagman (Stern Electronics, set 2)

static struct BurnRomInfo bagmans2RomDesc[] = {
	{ "a4_9e.bin",		0x1000, 0x5fb0a1a3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "a4_9f.bin",		0x1000, 0x7871206e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a4_9j.bin",		0x1000, 0xb2da8b77, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a4_9k.bin",		0x1000, 0x36b6a944, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a4_9m.bin",		0x1000, 0xb8e75eb6, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "a4_9n.bin",		0x1000, 0x83fccb1c, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "a2_1e.bin",		0x1000, 0xf217ac09, 2 | BRF_GRA },           //  6 Graphics
	{ "j1_b04.bin",		0x1000, 0xc680ef04, 2 | BRF_GRA },           //  7
	{ "a2_1c.bin",		0x1000, 0xf3e11bd7, 2 | BRF_GRA },           //  8
	{ "a2_1f.bin",		0x1000, 0xd0f7105b, 2 | BRF_GRA },           //  9

	{ "p3.bin",			0x0020, 0x2a855523, 3 | BRF_GRA },           // 10 Color Data
	{ "r3.bin",			0x0020, 0xae6f1019, 3 | BRF_GRA },           // 11

	{ "r6.bin",			0x0020, 0xc58a4f6a, 4 | BRF_SND },           // 12 TMS5110 State Machine

	{ "r9_b11.bin",		0x1000, 0x2e0057ff, 5 | BRF_SND },           // 13 TMS5110 Speech Data
	{ "t9_b12.bin",		0x1000, 0xb2120edd, 5 | BRF_SND },           // 14
};

STD_ROM_PICK(bagmans2)
STD_ROM_FN(bagmans2)

struct BurnDriver BurnDrvBagmans2 = {
	"bagmans2", "bagman", NULL, NULL, "1982",
	"Bagman (Stern Electronics, set 2)\0", NULL, "Valadon Automation (Stern Electronics license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bagmans2RomInfo, bagmans2RomName, NULL, NULL, NULL, NULL, BagmanInputInfo, BagmanDIPInfo,
	BagmanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Bagman (Taito)

static struct BurnRomInfo bagmanjRomDesc[] = {
	{ "bf8_06.e9",		0x1000, 0x5fb0a1a3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "bf8_07.f9",		0x1000, 0x7871206e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bf8_08.j9",		0x1000, 0xae037d0a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bf8_09.k9",		0x1000, 0x36b6a944, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bf8_10.m9",		0x1000, 0xb8e75eb6, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bf8_11.n9",		0x1000, 0x83fccb1c, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "bf8_03.e1",		0x1000, 0xf217ac09, 2 | BRF_GRA },           //  6 Graphics
	{ "bf8_05.j1",		0x1000, 0xc680ef04, 2 | BRF_GRA },           //  7
	{ "bf8_02.c1",		0x1000, 0x404283ed, 2 | BRF_GRA },           //  8
	{ "bf8_04-1.f1",	0x1000, 0x3f5c991e, 2 | BRF_GRA },           //  9

	{ "p3.bin",			0x0020, 0x2a855523, 3 | BRF_GRA },           // 10 Color Data
	{ "r3.bin",			0x0020, 0xae6f1019, 3 | BRF_GRA },           // 11

	{ "r6.bin",			0x0020, 0xc58a4f6a, 4 | BRF_GRA },           // 12 TMS5110 State Machine

	{ "bf8_12.r9",		0x1000, 0x2e0057ff, 5 | BRF_GRA },           // 13 TMS5110 Speech Data
	{ "bf8_13.t9",		0x1000, 0xb2120edd, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(bagmanj)
STD_ROM_FN(bagmanj)

struct BurnDriver BurnDrvBagmanj = {
	"bagmanj", "bagman", NULL, NULL, "1982",
	"Bagman (Taito)\0", NULL, "Valadon Automation (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bagmanjRomInfo, bagmanjRomName, NULL, NULL, NULL, NULL, BagmanInputInfo, BagmanDIPInfo,
	BagmanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Super Bagman

static struct BurnRomInfo sbagmanRomDesc[] = {
	{ "5.9e",			0x1000, 0x1b1d6b0a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "6.9f",			0x1000, 0xac49cb82, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "7.9j",			0x1000, 0x9a1c778d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "8.9k",			0x1000, 0xb94fbb73, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "9.9m",			0x1000, 0x601f34ba, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "10.9n",			0x1000, 0x5f750918, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "13.8d",			0x1000, 0x944a4453, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "14.8f",			0x1000, 0x83b10139, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "15.8j",			0x1000, 0xfe924879, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "16.8k",			0x1000, 0xb77eb1f5, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "2.1e",			0x1000, 0xf4d3d4e6, 2 | BRF_GRA },           // 10 Graphics
	{ "4.1j",			0x1000, 0x2c6a510d, 2 | BRF_GRA },           // 11
	{ "1.1c",			0x1000, 0xa046ff44, 2 | BRF_GRA },           // 12
	{ "3.1f",			0x1000, 0xa4422da4, 2 | BRF_GRA },           // 13

	{ "p3.bin",			0x0020, 0x2a855523, 3 | BRF_GRA },           // 14 Color Data
	{ "r3.bin",			0x0020, 0xae6f1019, 3 | BRF_GRA },           // 15

	{ "r6.bin",			0x0020, 0xc58a4f6a, 4 | BRF_SND },           // 16 TMS5110 State Machine

	{ "11.9r",			0x1000, 0x2e0057ff, 5 | BRF_SND },           // 17 TMS5110 Speech Data
	{ "12.9t",			0x1000, 0xb2120edd, 5 | BRF_SND },           // 18
};

STD_ROM_PICK(sbagman)
STD_ROM_FN(sbagman)

static INT32 SbagmanInit()
{
	return BagmanCommonInit(1, 0);
}

struct BurnDriver BurnDrvSbagman = {
	"sbagman", NULL, NULL, NULL, "1984",
	"Super Bagman\0", NULL, "Valadon Automation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, sbagmanRomInfo, sbagmanRomName, NULL, NULL, NULL, NULL, SbagmanInputInfo, SbagmanDIPInfo,
	SbagmanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Super Bagman (Stern Electronics)

static struct BurnRomInfo sbagmansRomDesc[] = {
	{ "sbag_9e.bin",	0x1000, 0xc19696f2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "6.9f",			0x1000, 0xac49cb82, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "7.9j",			0x1000, 0x9a1c778d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "8.9k",			0x1000, 0xb94fbb73, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sbag_9m.bin",	0x1000, 0xb21e246e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "10.9n",			0x1000, 0x5f750918, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "13.8d",			0x1000, 0x944a4453, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sbag_f8.bin",	0x1000, 0x0f3e6de4, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "15.8j",			0x1000, 0xfe924879, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "16.8k",			0x1000, 0xb77eb1f5, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "2.1e",			0x1000, 0xf4d3d4e6, 2 | BRF_GRA },           // 10 Graphics
	{ "4.1j",			0x1000, 0x2c6a510d, 2 | BRF_GRA },           // 11
	{ "sbag_1c.bin",	0x1000, 0x262f870a, 2 | BRF_GRA },           // 12
	{ "sbag_1f.bin",	0x1000, 0x350ed0fb, 2 | BRF_GRA },           // 13

	{ "p3.bin",			0x0020, 0x2a855523, 3 | BRF_GRA },           // 14 Color Data
	{ "r3.bin",			0x0020, 0xae6f1019, 3 | BRF_GRA },           // 15

	{ "r6.bin",			0x0020, 0xc58a4f6a, 4 | BRF_SND },           // 16 TMS5110 State Machine

	{ "11.9r",			0x1000, 0x2e0057ff, 5 | BRF_SND },           // 17 TMS5110 Speech Data
	{ "12.9t",			0x1000, 0xb2120edd, 5 | BRF_SND },           // 18
};

STD_ROM_PICK(sbagmans)
STD_ROM_FN(sbagmans)

struct BurnDriver BurnDrvSbagmans = {
	"sbagmans", "sbagman", NULL, NULL, "1984",
	"Super Bagman (Stern Electronics)\0", NULL, "Valadon Automation (Stern Electronics license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, sbagmansRomInfo, sbagmansRomName, NULL, NULL, NULL, NULL, SbagmanInputInfo, SbagmanDIPInfo,
	SbagmanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Super Bagman (Itisa, Spain)

static struct BurnRomInfo sbagmaniRomDesc[] = {
	{ "sb1.5d",			0x1000, 0x5e24f90f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "sb2.5f",			0x1000, 0x746ed840, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sb3.5h",			0x1000, 0xfdfc22ce, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sb4.5k",			0x1000, 0xb94fbb73, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sb5.5l",			0x1000, 0x98067a20, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sb6.5n",			0x1000, 0x4726e997, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sb-a.bin",		0x1000, 0x0d29a52d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sb-b.bin",		0x1000, 0xf48091c4, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sb-c.bin",		0x1000, 0x7648a042, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sb-d.bin",		0x1000, 0xba82bf0c, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "sb8.11k",		0x1000, 0x189d9bd6, 2 | BRF_GRA },           // 10 Graphics
	{ "sb10.11n",		0x1000, 0x2c6a510d, 2 | BRF_GRA },           // 11
	{ "sb7.11h",		0x1000, 0xa046ff44, 2 | BRF_GRA },           // 12
	{ "sb9.11l",		0x1000, 0xa4422da4, 2 | BRF_GRA },           // 13

	{ "am27s19dc.6v",	0x0020, 0xb3fc1505, 3 | BRF_GRA },           // 14 Color Data
	{ "6331-1n.6u",		0x0020, 0xb4e827a5, 3 | BRF_GRA },           // 15
	{ "6331-1n.6t",		0x0020, 0xab1940fa, 3 | BRF_GRA },           // 16

	{ "6331-1n.1",		0x0020, 0x4d222e6f, 0 | BRF_PRG | BRF_OPT }, // 17 Z80 Code PROMs
	{ "6331-1n.2",		0x0020, 0xecd06ffb, 0 | BRF_PRG | BRF_OPT }, // 18
};

STD_ROM_PICK(sbagmani)
STD_ROM_FN(sbagmani)

static INT32 SbagmaniInit()
{
	return BagmanCommonInit(2, 0);
}

struct BurnDriver BurnDrvSbagmani = {
	"sbagmani", "sbagman", NULL, NULL, "1984",
	"Super Bagman (Itisa, Spain)\0", NULL, "Valadon Automation (Itisa license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, sbagmaniRomInfo, sbagmaniRomName, NULL, NULL, NULL, NULL, SbagmanInputInfo, SbagmanDIPInfo,
	SbagmaniInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Pickin'

static struct BurnRomInfo pickinRomDesc[] = {
	{ "9e",				0x1000, 0xefd0bd43, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "9f",				0x1000, 0xb5785a23, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "9j",				0x1000, 0x65ee9fd4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "9k",				0x1000, 0x7b23350e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "9m",				0x1000, 0x935a7248, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "9n",				0x1000, 0x52485d1d, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "1f",				0x1000, 0xc5e96ac6, 2 | BRF_GRA },           //  6 Graphics
	{ "1j",				0x1000, 0x41c4ac1c, 2 | BRF_GRA },           //  7

	{ "6331-1.3p",		0x0020, 0xfac81668, 3 | BRF_GRA },           //  8 Color Data
	{ "6331-1.3r",		0x0020, 0x14ee1603, 3 | BRF_GRA },           //  9
};

STD_ROM_PICK(pickin)
STD_ROM_FN(pickin)

static INT32 PickinInit()
{
	return BagmanCommonInit(3, 1);
}

struct BurnDriver BurnDrvPickin = {
	"pickin", NULL, NULL, NULL, "1983",
	"Pickin'\0", NULL, "Valadon Automation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, pickinRomInfo, pickinRomName, NULL, NULL, NULL, NULL, BagmanInputInfo, PickinDIPInfo,
	PickinInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Botanic (English / Spanish)

static struct BurnRomInfo botanicRomDesc[] = {
	{ "5.9e",			0x1000, 0x907f01c7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "06.9f",			0x1000, 0xff2533fb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "07.9j",			0x1000, 0xb7c544ef, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "08.9k",			0x1000, 0x2df22793, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "09.9m",			0x1000, 0xf7d908ec, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "10.9n",			0x1000, 0x7ce9fbc8, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "2.1e",			0x1000, 0xbea449a6, 2 | BRF_GRA },           //  6 Graphics
	{ "4.1j",			0x1000, 0xa5deb8ed, 2 | BRF_GRA },           //  7
	{ "1.1c",			0x1000, 0xa1148d89, 2 | BRF_GRA },           //  8
	{ "3.1f",			0x1000, 0x70be5565, 2 | BRF_GRA },           //  9

	{ "prom.3p",		0x0020, 0xa8a2ddd2, 3 | BRF_GRA },           // 10 Color Data
	{ "prom.3r",		0x0020, 0xedf88f34, 3 | BRF_GRA },           // 11
};

STD_ROM_PICK(botanic)
STD_ROM_FN(botanic)

static INT32 BotanicInit()
{
	botanic_input_xor = 0x02;
	return BagmanCommonInit(4, 1);
}

struct BurnDriver BurnDrvBotanic = {
	"botanic", NULL, NULL, NULL, "1983",
	"Botanic (English / Spanish)\0", NULL, "Itisa", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, botanicRomInfo, botanicRomName, NULL, NULL, NULL, NULL, BagmanInputInfo, BotaniciDIPInfo,
	BotanicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Botanic (French)

static struct BurnRomInfo botanicfRomDesc[] = {
	{ "bota_05.9e",		0x1000, 0xcc66e6f8, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "bota_06.9f",		0x1000, 0x59892f41, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bota_07.9j",		0x1000, 0xb7c544ef, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bota_08.9k",		0x1000, 0x0afea479, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bota_09.9m",		0x1000, 0x2da36120, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bota_10.9n",		0x1000, 0x7ce9fbc8, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "bota_02.1e",		0x1000, 0xbea449a6, 2 | BRF_GRA },           //  6 Graphics
	{ "bota_04.1j",		0x1000, 0xa5deb8ed, 2 | BRF_GRA },           //  7
	{ "bota_01.1c",		0x1000, 0xa1148d89, 2 | BRF_GRA },           //  8
	{ "bota_03.1f",		0x1000, 0x70be5565, 2 | BRF_GRA },           //  9

	{ "bota_3p.3p",		0x0020, 0xa8a2ddd2, 3 | BRF_GRA },           // 10 Color Data
	{ "bota_3a.3a",		0x0020, 0xedf88f34, 3 | BRF_GRA },           // 11
};

STD_ROM_PICK(botanicf)
STD_ROM_FN(botanicf)

static INT32 BotanicfInit()
{
	return BagmanCommonInit(4, 1);
}

struct BurnDriver BurnDrvBotanicf = {
	"botanicf", "botanic", NULL, NULL, "1984",
	"Botanic (French)\0", NULL, "Itisa (Valadon Automation license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, botanicfRomInfo, botanicfRomName, NULL, NULL, NULL, NULL, BagmanInputInfo, BotanicfDIPInfo,
	BotanicfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Squash (Itisa)

static struct BurnRomInfo squaitsaRomDesc[] = {
	{ "sq5.3.9e",		0x1000, 0x04128d92, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "sq6.4.9f",		0x1000, 0x4ff7dd56, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sq7.5.9j",		0x1000, 0xe46ecda6, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "sq2.1.1e",		0x1000, 0x0eb6ecad, 2 | BRF_GRA },           //  3 Graphics
	{ "sq4.2.1j",		0x1000, 0x8d875b0e, 2 | BRF_GRA },           //  4
	{ "sq1.1c",			0x1000, 0xb6d563e5, 2 | BRF_GRA },           //  5
	{ "sq3.1f",			0x1000, 0x0d9d87e6, 2 | BRF_GRA },           //  6

	{ "mmi6331.3p",		0x0020, 0x06eab7ce, 3 | BRF_GRA },           //  7 Color Data
	{ "mmi6331.3r",		0x0020, 0x86c1e7db, 3 | BRF_GRA },           //  8
};

STD_ROM_PICK(squaitsa)
STD_ROM_FN(squaitsa)

static INT32 SquaitsaInit()
{
	squaitsamode = 1;
	return BagmanCommonInit(5, 1);
}

struct BurnDriver BurnDrvSquaitsa = {
	"squaitsa", NULL, NULL, NULL, "1984",
	"Squash (Itisa)\0", NULL, "Itisa", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, squaitsaRomInfo, squaitsaRomName, NULL, NULL, NULL, NULL, SquaitsaInputInfo, SquaitsaDIPInfo,
	SquaitsaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};
