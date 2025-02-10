// FB Neo "Legendary Wings" driver module
// Based on MAME driver by Paul Leaman

#include "tiles_generic.h"
#include "z80_intf.h"
#include "mcs51.h"
#include "burn_ym2203.h"
#include "msm5205.h"
#include "msm6295.h"
#include "dtimer.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSampleROM;
static UINT8 *DrvTileMap;
static UINT8 *DrvGfxMask;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *ScrollX;
static UINT8 *ScrollY;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvReset;
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDip[2];
static UINT8 DrvInputs[5];

static UINT8 interrupt_enable;
static UINT8 soundlatch;
static UINT8 soundlatch2;
static UINT8 flipscreen;
static UINT8 DrvZ80Bank;
static UINT8 DrvSampleBank;
static UINT8 DrvSpriteBank;

static UINT8 avengers_soundlatch2;
static UINT8 avengers_soundstate;

static UINT8 trojan_bg2_scrollx;
static UINT8 trojan_bg2_image;

static INT32 irq_counter = 0;
static dtimer msm5205_timer;

static INT32 fball = 0;
static INT32 avengers = 0;
static INT32 MSM5205InUse = 0;
static INT32 spritelen = 0;

static double hz = 60.00;

static INT32 cpu_cycles[6];
static INT32 nExtraCycles[4];
static INT32 scanline;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnInputInfo FballInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy4 + 3,	"p3 up"	},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy4 + 2,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy4 + 1,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy4 + 0,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p3 fire 2"	},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy5 + 3,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy5 + 2,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy5 + 1,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy5 + 0,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy5 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy5 + 5,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
};

STDINPUTINFO(Fball)

static struct BurnDIPInfo FballDIPList[]=
{
	DIP_OFFSET(0x1f)
	{0x00, 0xff, 0xff, 0x6d, NULL					},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x00, 0x01, 0x01, 0x01, "0"					},
	{0x00, 0x01, 0x01, 0x00, "1"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x06, 0x00, "1"					},
	{0x00, 0x01, 0x06, 0x02, "2"					},
	{0x00, 0x01, 0x06, 0x04, "3"					},
	{0x00, 0x01, 0x06, 0x06, "4"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x00, 0x01, 0x18, 0x00, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x18, 0x08, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x18, 0x10, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x18, 0x18, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x20, 0x20, "Off"					},
	{0x00, 0x01, 0x20, 0x00, "On"					},

	{0x00, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x80, 0x00, "Off"					},
	{0x00, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Fball)

static struct BurnDIPInfo LwingsDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xfe, NULL					},
	{0x01, 0xff, 0xff, 0xfe, NULL					},

	{0   , 0xfe, 0   ,    2, "Flip_Screen"			},
	{0x00, 0x01, 0x02, 0x02, "Off"					},
	{0x00, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x0c, "3"					},
	{0x00, 0x01, 0x0c, 0x04, "4"					},
	{0x00, 0x01, 0x0c, 0x08, "5"					},
	{0x00, 0x01, 0x0c, 0x00, "6"					},

	{0   , 0xfe, 0   ,    4, "Coin_B"				},
	{0x00, 0x01, 0x30, 0x00, "4C_1C"				},
	{0x00, 0x01, 0x30, 0x20, "3C_1C"				},
	{0x00, 0x01, 0x30, 0x10, "2C_1C"				},
	{0x00, 0x01, 0x30, 0x30, "1C_1C"				},

	{0   , 0xfe, 0   ,    4, "Coin_A"				},
	{0x00, 0x01, 0xc0, 0xc0, "1C_1C"				},
	{0x00, 0x01, 0xc0, 0x00, "2C_4C"				},
	{0x00, 0x01, 0xc0, 0x40, "1C_2C"				},
	{0x00, 0x01, 0xc0, 0x80, "1C_3C"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x06, 0x02, "Easy"					},
	{0x01, 0x01, 0x06, 0x06, "Medium"				},
	{0x01, 0x01, 0x06, 0x04, "Hard"					},
	{0x01, 0x01, 0x06, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo_Sounds"			},
	{0x01, 0x01, 0x08, 0x00, "Off"					},
	{0x01, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow_Continue"		},
	{0x01, 0x01, 0x10, 0x00, "No"					},
	{0x01, 0x01, 0x10, 0x10, "Yes"					},

	{0   , 0xfe, 0   ,    8, "Bonus_Life"			},
	{0x01, 0x01, 0xe0, 0xe0, "20000 and every 50000"},
	{0x01, 0x01, 0xe0, 0x60, "20000 and every 60000"},
	{0x01, 0x01, 0xe0, 0xa0, "20000 and every 70000"},
	{0x01, 0x01, 0xe0, 0x20, "30000 and every 60000"},
	{0x01, 0x01, 0xe0, 0xc0, "30000 and every 70000"},
	{0x01, 0x01, 0xe0, 0x40, "30000 and every 80000"},
	{0x01, 0x01, 0xe0, 0x80, "40000 and every 100000"},
	{0x01, 0x01, 0xe0, 0x00, "None"					},
};

STDDIPINFO(Lwings)

static struct BurnDIPInfo LwingsbDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xfe, NULL					},
	{0x01, 0xff, 0xff, 0xfe, NULL					},

	{0   , 0xfe, 0   ,    2, "Flip_Screen"			},
	{0x00, 0x01, 0x02, 0x02, "Off"					},
	{0x00, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x0c, "2"					},
	{0x00, 0x01, 0x0c, 0x04, "3"					},
	{0x00, 0x01, 0x0c, 0x08, "4"					},
	{0x00, 0x01, 0x0c, 0x00, "5"					},

	{0   , 0xfe, 0   ,    4, "Coin_B"				},
	{0x00, 0x01, 0x30, 0x00, "4C_1C"				},
	{0x00, 0x01, 0x30, 0x20, "3C_1C"				},
	{0x00, 0x01, 0x30, 0x10, "2C_1C"				},
	{0x00, 0x01, 0x30, 0x30, "1C_1C"				},

	{0   , 0xfe, 0   ,    4, "Coin_A"				},
	{0x00, 0x01, 0xc0, 0xc0, "1C_1C"				},
	{0x00, 0x01, 0xc0, 0x00, "2C_4C"				},
	{0x00, 0x01, 0xc0, 0x40, "1C_2C"				},
	{0x00, 0x01, 0xc0, 0x80, "1C_3C"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x06, 0x02, "Easy"					},
	{0x01, 0x01, 0x06, 0x06, "Medium"				},
	{0x01, 0x01, 0x06, 0x04, "Hard"					},
	{0x01, 0x01, 0x06, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo_Sounds"			},
	{0x01, 0x01, 0x08, 0x00, "Off"					},
	{0x01, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow_Continue"		},
	{0x01, 0x01, 0x10, 0x00, "No"					},
	{0x01, 0x01, 0x10, 0x10, "Yes"					},

	{0   , 0xfe, 0   ,    8, "Bonus_Life"			},
	{0x01, 0x01, 0xe0, 0xe0, "20000 and every 50000"},
	{0x01, 0x01, 0xe0, 0x60, "20000 and every 60000"},
	{0x01, 0x01, 0xe0, 0xa0, "20000 and every 70000"},
	{0x01, 0x01, 0xe0, 0x20, "30000 and every 60000"},
	{0x01, 0x01, 0xe0, 0xc0, "30000 and every 70000"},
	{0x01, 0x01, 0xe0, 0x40, "30000 and every 80000"},
	{0x01, 0x01, 0xe0, 0x80, "40000 and every 100000"},
	{0x01, 0x01, 0xe0, 0x00, "None"					},
};

STDDIPINFO(Lwingsb)

static struct BurnDIPInfo SectionzDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x3f, NULL					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip_Screen"			},
	{0x00, 0x01, 0x02, 0x02, "Off"					},
	{0x00, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x04, "2"					},
	{0x00, 0x01, 0x0c, 0x0c, "3"					},
	{0x00, 0x01, 0x0c, 0x08, "4"					},
	{0x00, 0x01, 0x0c, 0x00, "5"					},

	{0   , 0xfe, 0   ,    4, "Coin_A"				},
	{0x00, 0x01, 0x30, 0x00, "4C_1C"				},
	{0x00, 0x01, 0x30, 0x20, "3C_1C"				},
	{0x00, 0x01, 0x30, 0x10, "2C_1C"				},
	{0x00, 0x01, 0x30, 0x30, "1C_1C"				},

	{0   , 0xfe, 0   ,    4, "Coin_B"				},
	{0x00, 0x01, 0xc0, 0x00, "2C_1C"				},
	{0x00, 0x01, 0xc0, 0xc0, "1C_1C"				},
	{0x00, 0x01, 0xc0, 0x40, "1C_2C"				},
	{0x00, 0x01, 0xc0, 0x80, "1C_3C"				},

	{0   , 0xfe, 0   ,    2, "Allow_Continue"		},
	{0x01, 0x01, 0x01, 0x00, "No"					},
	{0x01, 0x01, 0x01, 0x01, "Yes"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x06, 0x02, "Easy"					},
	{0x01, 0x01, 0x06, 0x06, "Normal"				},
	{0x01, 0x01, 0x06, 0x04, "Hard"					},
	{0x01, 0x01, 0x06, 0x00, "Very_Hard"			},

	{0   , 0xfe, 0   ,    8, "Bonus_Life"			},
	{0x01, 0x01, 0x38, 0x38, "20000 50000"			},
	{0x01, 0x01, 0x38, 0x18, "20000 60000"			},
	{0x01, 0x01, 0x38, 0x28, "20000 70000"			},
	{0x01, 0x01, 0x38, 0x08, "30000 60000"			},
	{0x01, 0x01, 0x38, 0x30, "30000 70000"			},
	{0x01, 0x01, 0x38, 0x10, "30000 80000"			},
	{0x01, 0x01, 0x38, 0x20, "40000 100000"			},
	{0x01, 0x01, 0x38, 0x00, "None"					},

	{0   , 0xfe, 0   ,    3, "Cabinet"				},
	{0x01, 0x01, 0xc0, 0x00, "Upright One Player"	},
	{0x01, 0x01, 0xc0, 0x40, "Upright Two Players"	},
	{0x01, 0x01, 0xc0, 0xc0, "Cocktail"				},
};

STDDIPINFO(Sectionz)

static struct BurnDIPInfo TrojanlsDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x1c, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    3, "Cabinet"				},
	{0x00, 0x01, 0x03, 0x00, "Upright 1 Player"		},
	{0x00, 0x01, 0x03, 0x02, "Upright 2 Players"	},
	{0x00, 0x01, 0x03, 0x03, "Cocktail"				},

	{0   , 0xfe, 0   ,    8, "Bonus_Life"			},
	{0x00, 0x01, 0x1c, 0x10, "20000 60000"			},
	{0x00, 0x01, 0x1c, 0x0c, "20000 70000"			},
	{0x00, 0x01, 0x1c, 0x08, "20000 80000"			},
	{0x00, 0x01, 0x1c, 0x1c, "30000 60000"			},
	{0x00, 0x01, 0x1c, 0x18, "30000 70000"			},
	{0x00, 0x01, 0x1c, 0x14, "30000 80000"			},
	{0x00, 0x01, 0x1c, 0x04, "40000 80000"			},
	{0x00, 0x01, 0x1c, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Coin_A"				},
	{0x01, 0x01, 0x03, 0x00, "2C_1C"				},
	{0x01, 0x01, 0x03, 0x03, "1C_1C"				},
	{0x01, 0x01, 0x03, 0x02, "1C_2C"				},
	{0x01, 0x01, 0x03, 0x01, "1C_3C"				},

	{0   , 0xfe, 0   ,    4, "Coin_B"				},
	{0x01, 0x01, 0x0c, 0x00, "4C_1C"				},
	{0x01, 0x01, 0x0c, 0x04, "3C_1C"				},
	{0x01, 0x01, 0x0c, 0x08, "2C_1C"				},
	{0x01, 0x01, 0x0c, 0x0c, "1C_1C"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x30, 0x20, "2"					},
	{0x01, 0x01, 0x30, 0x30, "3"					},
	{0x01, 0x01, 0x30, 0x10, "4"					},
	{0x01, 0x01, 0x30, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "Flip_Screen"			},
	{0x01, 0x01, 0x40, 0x40, "Off"					},
	{0x01, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow_Continue"		},
	{0x01, 0x01, 0x80, 0x00, "No"					},
	{0x01, 0x01, 0x80, 0x80, "Yes"					},
};

STDDIPINFO(Trojanls)

static struct BurnDIPInfo TrojanDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xfc, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    3, "Cabinet"				},
	{0x00, 0x01, 0x03, 0x00, "Upright 1 Player"		},
	{0x00, 0x01, 0x03, 0x02, "Upright 2 Players"	},
	{0x00, 0x01, 0x03, 0x03, "Cocktail"				},

	{0   , 0xfe, 0   ,    8, "Bonus_Life"			},
	{0x00, 0x01, 0x1c, 0x10, "20000 60000"			},
	{0x00, 0x01, 0x1c, 0x0c, "20000 70000"			},
	{0x00, 0x01, 0x1c, 0x08, "20000 80000"			},
	{0x00, 0x01, 0x1c, 0x1c, "30000 60000"			},
	{0x00, 0x01, 0x1c, 0x18, "30000 70000"			},
	{0x00, 0x01, 0x1c, 0x14, "30000 80000"			},
	{0x00, 0x01, 0x1c, 0x04, "40000 80000"			},
	{0x00, 0x01, 0x1c, 0x00, "None"					},

	{0   , 0xfe, 0   ,    6, "Starting Level"		},
	{0x00, 0x01, 0xe0, 0xe0, "1"					},
	{0x00, 0x01, 0xe0, 0xc0, "2"					},
	{0x00, 0x01, 0xe0, 0xa0, "3"					},
	{0x00, 0x01, 0xe0, 0x80, "4"					},
	{0x00, 0x01, 0xe0, 0x60, "5"					},
	{0x00, 0x01, 0xe0, 0x40, "6"					},

	{0   , 0xfe, 0   ,    4, "Coin_A"				},
	{0x01, 0x01, 0x03, 0x00, "2C_1C"				},
	{0x01, 0x01, 0x03, 0x03, "1C_1C"				},
	{0x01, 0x01, 0x03, 0x02, "1C_2C"				},
	{0x01, 0x01, 0x03, 0x01, "1C_3C"				},

	{0   , 0xfe, 0   ,    4, "Coin_B"				},
	{0x01, 0x01, 0x0c, 0x00, "4C_1C"				},
	{0x01, 0x01, 0x0c, 0x04, "3C_1C"				},
	{0x01, 0x01, 0x0c, 0x08, "2C_1C"				},
	{0x01, 0x01, 0x0c, 0x0c, "1C_1C"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x30, 0x20, "2"					},
	{0x01, 0x01, 0x30, 0x30, "3"					},
	{0x01, 0x01, 0x30, 0x10, "4"					},
	{0x01, 0x01, 0x30, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "Flip_Screen"			},
	{0x01, 0x01, 0x40, 0x40, "Off"					},
	{0x01, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow_Continue"		},
	{0x01, 0x01, 0x80, 0x00, "No"					},
	{0x01, 0x01, 0x80, 0x80, "Yes"					},
};

STDDIPINFO(Trojan)

static struct BurnDIPInfo AvengersDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Allow_Continue"		},
	{0x00, 0x01, 0x01, 0x00, "No"					},
	{0x00, 0x01, 0x01, 0x01, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Demo_Sounds"			},
	{0x00, 0x01, 0x02, 0x00, "Off"					},
	{0x00, 0x01, 0x02, 0x02, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x0c, 0x04, "Easy"					},
	{0x00, 0x01, 0x0c, 0x0c, "Normal"				},
	{0x00, 0x01, 0x0c, 0x08, "Hard"					},
	{0x00, 0x01, 0x0c, 0x00, "Very_Hard"			},

	{0   , 0xfe, 0   ,    4, "Bonus_Life"			},
	{0x00, 0x01, 0x30, 0x30, "20k 60k"				},
	{0x00, 0x01, 0x30, 0x10, "20k 70k"				},
	{0x00, 0x01, 0x30, 0x20, "20k 80k"				},
	{0x00, 0x01, 0x30, 0x00, "30k 80k"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0xc0, 0xc0, "3"					},
	{0x00, 0x01, 0xc0, 0x40, "4"					},
	{0x00, 0x01, 0xc0, 0x80, "5"					},
	{0x00, 0x01, 0xc0, 0x00, "6"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip_Screen"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Coin_B"				},
	{0x01, 0x01, 0x1c, 0x00, "4C_1C"				},
	{0x01, 0x01, 0x1c, 0x10, "3C_1C"				},
	{0x01, 0x01, 0x1c, 0x08, "2C_1C"				},
	{0x01, 0x01, 0x1c, 0x1c, "1C_1C"				},
	{0x01, 0x01, 0x1c, 0x0c, "1C_2C"				},
	{0x01, 0x01, 0x1c, 0x14, "1C_3C"				},
	{0x01, 0x01, 0x1c, 0x04, "1C_4C"				},
	{0x01, 0x01, 0x1c, 0x18, "1C_6C"				},

	{0   , 0xfe, 0   ,    8, "Coin_A"				},
	{0x01, 0x01, 0xe0, 0x00, "4C_1C"				},
	{0x01, 0x01, 0xe0, 0x80, "3C_1C"				},
	{0x01, 0x01, 0xe0, 0x40, "2C_1C"				},
	{0x01, 0x01, 0xe0, 0xe0, "1C_1C"				},
	{0x01, 0x01, 0xe0, 0x60, "1C_2C"				},
	{0x01, 0x01, 0xe0, 0xa0, "1C_3C"				},
	{0x01, 0x01, 0xe0, 0x20, "1C_4C"				},
	{0x01, 0x01, 0xe0, 0xc0, "1C_6C"				},
};

STDDIPINFO(Avengers)

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x020000;
	DrvZ80ROM1	= Next; Next += 0x010000;
	DrvZ80ROM2	= Next; Next += 0x010000;
	DrvMCUROM   = Next; Next += 0x001000;

	DrvTileMap	= Next; Next += 0x008000;

	DrvGfxROM0	= Next; Next += 0x020000;
	DrvGfxROM1	= Next; Next += 0x080000;
	DrvGfxROM2	= Next; Next += 0x080000;
	DrvGfxROM3	= Next; Next += 0x020000;

	DrvGfxMask	= Next; Next += 0x000020;

	MSM6295ROM	= Next;
	DrvSampleROM= Next; Next += 0x200000;

	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM0	= Next; Next += 0x002000;
	DrvZ80RAM1	= Next; Next += 0x000800;
	DrvPalRAM	= Next; Next += 0x000800;
	DrvFgRAM	= Next; Next += 0x000800;
	DrvBgRAM	= Next; Next += 0x000800;
	DrvSprRAM	= Next; Next += 0x000200;
	DrvSprBuf	= Next; Next += 0x000200;

	ScrollX		= Next; Next += 0x000002;
	ScrollY		= Next; Next += 0x000002;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

struct LatchyJibjab {
	UINT8 value;
	UINT32 state;

	//config
	UINT32 ackonread;
	INT32 verbose;
	INT32 id;

	void (*cb)(int);

	void init() {
		cb = NULL;

		ackonread = 1;
		id = 0;
		reset();
	}

	void init(void (*cb_param)(int)) {
		cb = cb_param;

		ackonread = 1;
		id = 0;
		reset();
	}

	void set_id(int id_) {
		id = id_;
	}

	void set_verbose(int verbose_) {
		verbose = verbose_;
	}

	void ack_on_read(int onoff) {
		ackonread = onoff;
	}

	void reset() {
		value = 0;
		state = 0;
	}

	void sstate(int now) {
		if (now != state) {
			state = now;
			if (cb != NULL) {
				cb(state);
			}
		}
	}

	void ack() {
		sstate(0);
	}

	UINT8 read() {
		if (ackonread) ack();
		return value;
	}
	void write(UINT8 v) {
		if (state && v != value && verbose) {
			bprintf(0, _T("latch %02d over-written! was / now: %02x  %02x\tSL %d\n"), id, value, v, scanline);
		}
		value = v;
		sstate(1);
	}

	void scan() {
		SCAN_VAR(state);
		SCAN_VAR(value);
	}
};

// i8751 MCU (avengers)
static void DrvMCUReset(); // forward
static void DrvMCUSync(); // ""

static UINT8 mcu_control;
static LatchyJibjab mcu_latch[3];
static UINT8 mcu_data[3];

static UINT8 mcu_read_port(INT32 port)
{
	switch (port) {
		case MCS51_PORT_P0:
			return (~mcu_control & 0x80) ? mcu_latch[0].read() : 0xff;

		case MCS51_PORT_P1:
			return scanline;

		case MCS51_PORT_P2:
			return (~mcu_control & 0x80) ? mcu_latch[1].read() : 0xff;
	}

	return 0xff;
}

static void mcu_write_port(INT32 port, UINT8 data)
{
	switch (port) {
		case MCS51_PORT_P0:
			mcu_data[0] = data; break;

		case MCS51_PORT_P2:
			mcu_data[1] = data; break;

		case MCS51_PORT_P3:
			if (~mcu_control & 0x40 && data & 0x40) {
				mcu_latch[2].write(mcu_data[0]);
				soundlatch = mcu_data[1];
				avengers_soundstate = 0x80;
			}

			if (~mcu_control & 0x80 && data & 0x80) {
				mcu_latch[0].ack();
			}

			mcu_control = data;
			break;
	}
}

static void latch0_cb(int state)
{
	mcs51_set_irq_line(MCS51_INT0_LINE, (state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void DrvMCUInit()
{
	mcs51Init(0);
	mcs51Open(0);
	mcs51_set_program_data(DrvMCUROM);
	mcs51_set_write_handler(mcu_write_port);
	mcs51_set_read_handler(mcu_read_port);
	DrvMCUReset();
	mcs51Close();

	mcu_latch[0].init(latch0_cb);
    mcu_latch[0].ack_on_read(0);
	mcu_latch[0].set_id(0);

	mcu_latch[1].init();
	mcu_latch[1].set_id(1);

	mcu_latch[2].init();
	mcu_latch[2].set_id(2);
}

static void DrvMCUExit() {
	mcs51_exit();
}

#if 0
static INT32 DrvMCUTotalCycles()
{
	return mcs51TotalCycles();
}
#endif

static INT32 DrvMCURun(INT32 cycles)
{
	return mcs51Run(cycles);
}

static INT32 DrvMCUIdle(INT32 cycles)
{
	return mcs51Idle(cycles);
}

static INT32 DrvMCUScan(INT32 nAction)
{
	mcs51_scan(nAction);

	SCAN_VAR(mcu_control);
	mcu_latch[0].scan();
	mcu_latch[1].scan();
	mcu_latch[2].scan();
	SCAN_VAR(mcu_data);

	return 0;
}

static void DrvMCUSync()
{
	INT32 todo = ((INT64)ZetTotalCycles(0) * cpu_cycles[3] / cpu_cycles[0]) - mcs51TotalCycles();

	if (todo > 0) {
		DrvMCURun(todo);
	}
}

static void DrvMCUReset()
{
	mcu_control = 0xff;
	mcu_latch[0].reset();
	mcu_latch[1].reset();
	mcu_latch[2].reset();
	memset(mcu_data, 0, sizeof(mcu_data));

	mcs51_reset();
}


static UINT8 __fastcall lwings_main_read(UINT16 address)
{
	if (address < 0xf800) {
		if (avengers && (ZetInFetch & 1)) { // only when fetching opcodes (not opargs)
			Z80Burn(2);
		}
  		return ZetReadByte(address);
	}

	switch (address)
	{
		case 0xf808:
		case 0xf809:
		case 0xf80a:
			return DrvInputs[address - 0xf808];

		case 0xf80b:
		case 0xf80c:
			return DrvDip[address - 0xf80b];

		case 0xf80d:
	    case 0xf80e:
			if (fball) {
				return DrvInputs[(address - 0xf80d) + 3];
			} else if (address == 0xf80d) {
				DrvMCUSync();
				return mcu_latch[2].read();
			}
	}

	return 0;
}

static void bankswitch(UINT8 data)
{
	DrvZ80Bank = data;

	INT32 bankaddress = 0x10000 + ((data >> 1) & 3) * 0x4000;

	ZetMapMemory(DrvZ80ROM0 + bankaddress, 0x8000, 0xbfff, MAP_READ); // fetch: handler
}

static inline void palette_update(INT32 entry)
{
	UINT16 p = DrvPalRAM[entry | 0x400] | (DrvPalRAM[entry] << 8);

	UINT8 r = (p >> 12) & 0xf;
	UINT8 g = (p >>  8) & 0xf;
	UINT8 b = (p >>  4) & 0xf;

	DrvPalette[entry] = BurnHighCol((r*16)+r, (g*16)+g, (b*16)+b, 0);
}

static void __fastcall lwings_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xf000) {
		DrvPalRAM[address & 0x7ff] = data;
		palette_update(address & 0x3ff);
		return;
	}

	if (avengers && (address & 0xfff8) == 0xf808) address += 0x10; // hack, map avengers f808+ to f818+, see below...

	switch (address)
	{
		case 0xf800:
		case 0xf801:
		case 0xf808:
		case 0xf809:
			ScrollX[address & 1] = data;
		return;

		case 0xf802:
		case 0xf803:
		case 0xf80a:
		case 0xf80b:
			ScrollY[address & 1] = data;
		return;

		case 0xf804:
			trojan_bg2_scrollx = data;
		return;

		case 0xf805:
			trojan_bg2_image = data;
		return;

		case 0xf80c:
			soundlatch = data;
		return;

		case 0xf80d:
			if (fball) {
		//		watchdog = 0;
			} else {
				soundlatch2 = data;
			}
		return;

		case (0xf80e + 0x10):
		case 0xf80e: {
			bankswitch(data);

			flipscreen = ~data & 0x01;

			DrvSpriteBank = (data & 0x10) >> 4;

			interrupt_enable = data & 0x08;
		}
		return;

		case (0xf809 + 0x10): // actually avengers 0xf809!
			DrvMCUSync();
			mcu_latch[0].write(data);
		return;

		case (0xf80c + 0x10):  // actually avengers 0xf80c!
			DrvMCUSync();
			mcu_latch[1].write(data);
		return;

		case (0xf80d + 0x10):  // actually avengers 0xf80d!
			soundlatch2 = data;
		return;
	}
}

static void __fastcall lwings_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
		case 0xe001:
		case 0xe002:
		case 0xe003:
			BurnYM2203Write((address & 2) >> 1, address & 1, data);
		return;

		case 0xe006:
			avengers_soundlatch2 = data;
		return;
	}
}

static UINT8 __fastcall lwings_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xc800:
			return soundlatch;

		case 0xe006:
			UINT8 Data = avengers_soundlatch2 | avengers_soundstate;
			avengers_soundstate = 0;
			return Data;
	}

	return 0;
}

static void oki_bank(INT32 data)
{
	DrvSampleBank = data;

	INT32 bank = (DrvSampleBank & 0x0e) * 0x10000;
	if (bank >= 0xc0000) bank -= 0xc0000;

	memcpy (DrvSampleROM + 0x20000, DrvSampleROM + 0x40000 + bank, 0x20000);
}

static void __fastcall fball_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			oki_bank(data);
		return;

		case 0xe000:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 __fastcall fball_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8000:
			return soundlatch;

		case 0xe000:
			return MSM6295Read(0);
	}

	return 0;
}

static void __fastcall trojan_adpcm_out(UINT16 port, UINT8 data)
{
	if ((port & 0xff) == 0x01) {
		MSM5205ResetWrite(0, (data >> 7) & 1);
		MSM5205DataWrite(0, data);
		MSM5205VCLKWrite(0, 1);
		MSM5205VCLKWrite(0, 0);
	}
}

static UINT8 __fastcall trojan_adpcm_in(UINT16 port)
{
	port &= 0xff;

	UINT8 ret = 0;

	if (port == 0x00) {
		ret = soundlatch2;
	}

	return ret;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0x00, RamEnd - AllRam);


	ZetOpen(0);
	ZetReset();
	bankswitch(0);
	ZetClose();

	ZetReset(1);

	if (MSM5205InUse) {
		ZetReset(2);
	}

	if (fball) {
		MSM6295Reset(0);
		oki_bank(0);
	} else {
		BurnYM2203Reset();
	}

	if (MSM5205InUse) {
		MSM5205Reset();
		timerReset();
	}

	if (avengers) {
		mcs51Open(0);
		DrvMCUReset();
		mcs51Close();
	}

	trojan_bg2_scrollx = 0;
	trojan_bg2_image = 0;

	avengers_soundlatch2 = 0;
	avengers_soundstate = 0;

	DrvSpriteBank = 0;
	DrvZ80Bank = 0;
	flipscreen = 0;
	interrupt_enable = 0;
	soundlatch = 0;
	soundlatch2 = 0;
	irq_counter = 0;

	memset(nExtraCycles, 0, sizeof(nExtraCycles));

	HiscoreReset();

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 0x000000, 0x000004 };
	INT32 Plane1[4]  = { RGN_FRAC(spritelen, 1, 2)+4, RGN_FRAC(spritelen, 1, 2)+0, 4, 0 };//0x080004, 0x080000, 0x000004, 0x000000 };
	INT32 Plane1a[4] = { 0x100004, 0x100000, 0x000004, 0x000000 };
	INT32 Plane2[4]  = { 0x180000, 0x100000, 0x080000, 0x000000 };
	INT32 Plane3[4]  = { 0x040000, 0x040004, 0x000000, 0x000004 };

	// sprite, char
	INT32 XOffs0[16] = { 0, 1, 2, 3, 8, 9, 10, 11, 256, 257, 258, 259, 264, 265, 266, 267 };
	INT32 YOffs0[16] = { 0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240 };

	// background
	INT32 XOffs1[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 128, 129, 130, 131, 132, 133, 134, 135 };
	INT32 YOffs1[16] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x08000);

	GfxDecode(0x0800, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane2, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x40000);

	if (DrvTileMap != NULL) {
		GfxDecode(0x0800, 4, 16, 16, Plane1a, XOffs0, YOffs0, 0x200, tmp, DrvGfxROM2);

		memcpy (tmp, DrvGfxROM3, 0x10000);

		GfxDecode(0x0200, 4, 16, 16, Plane3, XOffs0, YOffs0, 0x200, tmp, DrvGfxROM3);
	} else {
		GfxDecode(0x0400, 4, 16, 16, Plane1, XOffs0, YOffs0, 0x200, tmp, DrvGfxROM2);
	}

	BurnFree (tmp);

	return 0;
}

inline static INT32 DrvMSM5205SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)ZetTotalCycles() * nSoundRate / (3000000));
}

static void lwings_main_cpu_init()
{
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0, 0x0000, 0x7fff, MAP_READ); // fetch: handler
	// 8000 - bfff banked
	ZetMapMemory(DrvZ80RAM0, 0xc000, 0xddff, MAP_READ | MAP_WRITE);
	ZetMapMemory(DrvSprRAM,  0xde00, 0xdfff, MAP_READ | MAP_WRITE);
	ZetMapMemory(DrvFgRAM,   0xe000, 0xe7ff, MAP_READ | MAP_WRITE);
	ZetMapMemory(DrvBgRAM,   0xe800, 0xefff, MAP_READ | MAP_WRITE);
	ZetMapMemory(DrvPalRAM,  0xf000, 0xf7ff, MAP_READ); // write: handler

	ZetSetReadHandler(lwings_main_read);
	ZetSetWriteHandler(lwings_main_write);
	ZetClose();
}

static void lwings_sound_init()
{
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1, 0xc000, 0xc7ff, MAP_RAM);
	ZetSetReadHandler(lwings_sound_read);
	ZetSetWriteHandler(lwings_sound_write);
	ZetClose();

	BurnYM2203Init(2, 1500000, NULL, 0);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.10, BURN_SND_ROUTE_BOTH);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	DrvTileMap = NULL;

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000, 0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM0 + 0x10000, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x18000, 2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000, 3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, 4, 1)) return 1;

		for (INT32 i = 0; i < 8; i++) {
			if (BurnLoadRom(DrvGfxROM1 + i * 0x8000, i + 5, 1)) return 1;
		}

		for (INT32 i = 0; i < 4; i++) {
			if (BurnLoadRom(DrvGfxROM2 + i * 0x8000, i + 13, 1)) return 1;
		}

		spritelen = 0x20000;

		DrvGfxDecode();
	}

	ZetInit(0);
	lwings_main_cpu_init();

	ZetInit(1);
	lwings_sound_init();

	GenericTilesInit();

	cpu_cycles[0] = 6000000;
	cpu_cycles[1] = 3000000;
	cpu_cycles[2] = 0;

	DrvDoReset();

	return 0;
}

static INT32 sectionzInit()
{
	hz = 55.37;

	BurnSetRefreshRate(hz);

	return DrvInit();
}

static void msm5205_timer_cb(INT32 param)
{
	ZetSetIRQLine(2, 0, CPU_IRQSTATUS_HOLD);
}

static INT32 TrojanInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000, 0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x18000, 2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000, 3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000, 4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, 5, 1)) return 1;

		for (INT32 i = 0; i < 8; i++) {
			if (BurnLoadRom(DrvGfxROM1 + i * 0x8000, i + 6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + i * 0x8000, i + 14, 1)) return 1;
		}
		spritelen = 0x40000;

		if (BurnLoadRom(DrvGfxROM3 + 0x0000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x8000, 23, 1)) return 1;

		if (BurnLoadRom(DrvTileMap, 24, 1)) return 1;

		if (avengers) {
			if (BurnLoadRom(DrvMCUROM, 27, 1)) return 1;
			DrvMCUROM[0xb84] = 0x02;
			DrvMCUROM[0x481] = 0x00;
			DrvMCUROM[0x483] = 0xa0;
			DrvMCUROM[0x4c3] = 0x30;
			DrvMCUROM[0x4e0] = 0x00;
			bprintf(0, _T("Avengers MCU loaded and patched\n"));
			DrvMCUInit();
		}

		DrvGfxDecode();

		{
			for (INT32 i = 0; i < 32; i++) {
				DrvGfxMask[i] = (0xf07f0001 & (1 << i)) ? 1 : 0;
			}
		}
	}

	ZetInit(0);
	lwings_main_cpu_init();

	ZetInit(1);
	lwings_sound_init();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2, 0x0000, 0xffff, MAP_ROM);
	ZetSetInHandler(trojan_adpcm_in);
	ZetSetOutHandler(trojan_adpcm_out);
	ZetClose();

	MSM5205Init(0, DrvMSM5205SynchroniseStream, 384000, NULL, MSM5205_SEX_4B, 1);
	MSM5205SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	MSM5205InUse = 1;

	timerInit();
	timerAdd(msm5205_timer, 0, msm5205_timer_cb);
	// 4000hz timer @ 3mhz, continuous
	msm5205_timer.start(3000000 / 4000, 0, 1, 1);

	GenericTilesInit();

	cpu_cycles[0] = 3000000;
	cpu_cycles[1] = 3000000;
	cpu_cycles[2] = 3000000;
	cpu_cycles[3] = 0;

	if (avengers) {
		cpu_cycles[0] = 6000000;
		cpu_cycles[3] = 6000000 / 12; // mcu
	}
	DrvDoReset();

	return 0;
}

static INT32 FballInit()
{
	fball = 1;

	BurnAllocMemIndex();

	DrvTileMap = NULL;

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000, 0, 1)) return 1;
		// bank2 = 0x8000

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000, 1, 1)) return 1;
		memset (DrvZ80ROM1 + 0x1000, 0xff, 0x10000-0x1000);

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, 2, 1)) return 1;
		memset (DrvGfxROM0 + 0x4000, 0xff, 0x10000-0x4000);

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000, 4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000, 3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x30000, 6, 1)) return 1;
		memset (DrvGfxROM1 + 0x40000, 0, 0x50000);

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 8, 1)) return 1;
		spritelen = 0x40000;

		if (BurnLoadRom(DrvSampleROM + 0x00000, 9, 1)) return 1;
		if (BurnLoadRom(DrvSampleROM + 0x40000, 9, 1)) return 1;
		if (BurnLoadRom(DrvSampleROM + 0x80000, 10, 1)) return 1;
		if (BurnLoadRom(DrvSampleROM + 0xc0000, 11, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	lwings_main_cpu_init();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1, 0x0000, 0x0fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1, 0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(fball_sound_write);
	ZetSetReadHandler(fball_sound_read);
	ZetClose();

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	cpu_cycles[0] = 6000000;
	cpu_cycles[1] = 3000000;
	cpu_cycles[2] = 0;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	if (fball) {
		MSM6295Exit(0);
	} else {
		BurnYM2203Exit();
	}

	if (avengers) {
		DrvMCUExit();
	}

	if (MSM5205InUse) {
		MSM5205Exit();
		timerExit();
	}

	BurnFreeMemIndex();

	fball = 0;
	avengers = 0;
	MSM5205InUse = 0;

	hz = 60.00;

	return 0;
}

static void draw_foreground(INT32 colbase)
{
	for (INT32 offs = 0x20; offs < 0x3e0; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		INT32 color = DrvFgRAM[offs | 0x400];
		INT32 code = DrvFgRAM[offs] | ((color & 0xc0) << 2);

		INT32 flipx = color & 0x10;
		INT32 flipy = color & 0x20;

		color &= 0x0f;

		sy -= 8;

		Draw8x8MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 2, 0x03, colbase, DrvGfxROM0);
	}
}

static void draw_background()
{
	INT32 scrollx = (ScrollX[0] | (ScrollX[1] << 8)) & 0x1ff;
	INT32 scrolly = (ScrollY[0] | (ScrollY[1] << 8)) & 0x1ff;

	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sy = (offs & 0x1f) << 4;
		INT32 sx = (offs >> 5) << 4;
		    sy -= 8;

		sx -= scrollx;
		sy -= scrolly;
		if (sx < -15) sx += 512;
		if (sy < -15) sy += 512;

		if (sy < -15 || sx < -15 || sy >= nScreenHeight || sx >= nScreenWidth)
			continue;

		INT32 color = DrvBgRAM[offs | 0x400];
		INT32 code = DrvBgRAM[offs] | (color & 0xe0) << 3;;

		INT32 flipx = color & 0x08;
		INT32 flipy = color & 0x10;

		color &= 0x07;

		Draw16x16Tile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 0, DrvGfxROM1);
	}
}

static void lwings_draw_sprites()
{
	for (INT32 offs = 0x200 - 4; offs >= 0; offs -= 4)
	{
		INT32 sx = DrvSprBuf[offs + 3] - 0x100 * (DrvSprBuf[offs + 1] & 0x01);
		INT32 sy = DrvSprBuf[offs + 2];

		if (sy && sx)
		{
			INT32 code,color,flipx,flipy;

			if (sy > 0xf8) sy-=0x100;

			code  = DrvSprBuf[offs] | ((DrvSprBuf[offs + 1] & 0xc0) << 2);
			color = (DrvSprBuf[offs + 1] & 0x38) >> 3;
			flipx = DrvSprBuf[offs + 1] & 0x02;
			flipy = DrvSprBuf[offs + 1] & 0x04;

			color += 0x18;

			sy -= 8;

			Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 0x0f, 0, DrvGfxROM2);
		}
	}
}


static void draw_16x16_with_mask(INT32 sx, INT32 sy, INT32 code, INT32 color, UINT8 *gfxbase, UINT8 *mask, INT32 flipx, INT32 flipy)
{
	UINT8 *src = gfxbase + (code << 8);

	color = (color << 4) | 0x100;

	if (flipy) {
		src += 0xf0;

		if (flipx) {
			for (INT32 y = 15; y >= 0; y--, src-=16)
			{
				INT32 yy = sy + y;
				if (yy < 0) break;
				if (yy >= nScreenHeight) continue;

				for (INT32 x = 15; x >= 0; x--)
				{
					INT32 xx = sx + x;
					if (xx < 0) break;
					if (xx >= nScreenWidth) continue;

					INT32 o = color | src[15-x];
					if (mask[src[15-x]]) continue;

					pTransDraw[(yy * nScreenWidth) + xx] = o;
				}
			}
		} else {
			for (INT32 y = 15; y >= 0; y--, src-=16)
			{
				INT32 yy = sy + y;
				if (yy < 0) break;
				if (yy >= nScreenHeight) continue;

				for (INT32 x = 0; x < 16; x++)
				{
					INT32 xx = sx + x;
					if (xx < 0) continue;
					if (xx >= nScreenWidth) break;

					INT32 o = color | src[x];
					if (mask[src[x]]) continue;

					pTransDraw[(yy * nScreenWidth) + xx] = o;
				}
			}
		}
	} else {
		if (flipx) {
			for (INT32 y = 0; y < 16; y++, src+=16)
			{
				INT32 yy = sy + y;
				if (yy < 0) continue;
				if (yy >= nScreenHeight) break;

				for (INT32 x = 15; x >= 0; x--)
				{
					INT32 xx = sx + x;
					if (xx < 0) break;
					if (xx >= nScreenWidth) continue;

					INT32 o = color | src[15-x];
					if (mask[src[15-x]]) continue;

					pTransDraw[(yy * nScreenWidth) + xx] = o;
				}
			}
		} else {
			for (INT32 y = 0; y < 16; y++, src+=16)
			{
				INT32 yy = sy + y;
				if (yy < 0) continue;
				if (yy >= nScreenHeight) break;

				for (INT32 x = 0; x < 16; x++)
				{
					INT32 xx = sx + x;
					if (xx < 0) continue;
					if (xx >= nScreenWidth) break;

					INT32 o = color | src[x];
					if (mask[src[x]]) continue;

					pTransDraw[(yy * nScreenWidth) + xx] = o;
				}
			}
		}
	}
}

static void trojan_draw_background(INT32 priority)
{
	INT32 scrollx = (ScrollX[0] | (ScrollX[1] << 8)) & 0x1ff;
	INT32 scrolly = (ScrollY[0] | (ScrollY[1] << 8)) & 0x1ff;

	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 color = DrvBgRAM[offs | 0x400];
		if (priority && ((~color >> 3) & 1)) continue;

		INT32 sy = (offs & 0x1f) << 4;
		INT32 sx = (offs >> 5) << 4;
		    sy -= 8;

		sx -= scrollx;
		sy -= scrolly;
		if (sx < -15) sx += 512;
		if (sy < -15) sy += 512;

		if (sy < -15 || sx < -15 || sy >= nScreenHeight || sx >= nScreenWidth)
			continue;

		INT32 code = DrvBgRAM[offs] | ((color & 0xe0) << 3);
		INT32 flipx = color & 0x10;
		INT32 flipy = 0;

		color &= 0x07;

		draw_16x16_with_mask(sx, sy, code, color, DrvGfxROM1, DrvGfxMask + priority * 16, flipx, flipy);
	}
}

static void trojan_draw_background2()
{
	for (INT32 offs = 0; offs < 32 * 16; offs++)
	{
		INT32 sx = (offs & 0x1f) << 4;
		INT32 sy = (offs >> 5) << 4;

		sx -= trojan_bg2_scrollx;
		if (sx < -15) sx += 512;
		    sy -= 8;

		if (sy < -15 || sx < -15 || sy >= nScreenHeight || sx >= nScreenWidth)
			continue;

		INT32 offset = ((((offs << 6) & 0x7800) | ((offs << 1) & 0x3e)) + (trojan_bg2_image << 5)) & 0x7fff;

		INT32 color = DrvTileMap[offset + 1];
		INT32 code  = DrvTileMap[offset + 0] | ((color & 0x80) << 1);
		INT32 flipx = color & 0x10;
		INT32 flipy = color & 0x20;

		color &= 7;

		Draw16x16Tile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 0, DrvGfxROM3);
	}
}

static void trojan_draw_sprites()
{
	for (INT32 offs = 0x180 - 4; offs >= 0; offs -= 4)
	{
		INT32 sx = DrvSprBuf[offs + 3] - 0x100 * (DrvSprBuf[offs + 1] & 0x01);
		INT32 sy = DrvSprBuf[offs + 2];

		if (sy || sx)
		{
			INT32 flipx, flipy;

			if (sy > 0xf8) sy-=0x100;

			INT32 color = DrvSprBuf[offs + 1];
			INT32 code  = DrvSprBuf[offs] | ((color & 0x20) << 4) | ((color & 0x40) << 2) | ((color & 0x80) << 3) | (DrvSpriteBank << 10);

			if (avengers)
			{
				flipx = 0;
				flipy = ~color & 0x10;
			}
			else
			{
				flipx = color & 0x10;
				flipy = 1;
			}

			color = ((color >> 1) & 7) + 0x28;

		   	sy -= 8;

			Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 0x0f, 0, DrvGfxROM2);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x400; i++) {
			palette_update(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (DrvTileMap == NULL) {
		if (nBurnLayer & 1) draw_background();
		if (nSpriteEnable & 1) lwings_draw_sprites();
		if (nBurnLayer & 2) draw_foreground(0x200);
	} else {
		if (nBurnLayer & 1) trojan_draw_background2();
		if (nBurnLayer & 2) trojan_draw_background(0);
		if (nSpriteEnable & 1) trojan_draw_sprites();
		if (nBurnLayer & 4) trojan_draw_background(1);
		if (nBurnLayer & 8) draw_foreground(0x300);
	}

	if (flipscreen) {
		UINT16 *ptr = pTransDraw + (nScreenWidth * nScreenHeight) - 1;

		for (INT32 i = 0; i < nScreenWidth * nScreenHeight / 2; i++, ptr--)
		{
			INT32 n = pTransDraw[i];
			pTransDraw[i] = *ptr;
			*ptr = n;
		}
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();
	if (avengers) mcs51NewFrame();
	timerNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if ((DrvInputs[1] & 0x03) == 0) DrvInputs[1] |= 0x03;
		if ((DrvInputs[1] & 0x0c) == 0) DrvInputs[1] |= 0x0c;
		if ((DrvInputs[2] & 0x03) == 0) DrvInputs[2] |= 0x03;
		if ((DrvInputs[2] & 0x0c) == 0) DrvInputs[2] |= 0x0c;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[5] = { (INT32)(cpu_cycles[0] / hz), (INT32)(cpu_cycles[1] / hz), (INT32)(cpu_cycles[2] / hz), (INT32)(cpu_cycles[3] / hz), (INT32)(cpu_cycles[2] / hz) }; // main, sound, pcm, mcu, dtimer (for pcm, same hz)
	INT32 nCyclesDone[5] = { nExtraCycles[0], 0, 0, 0, 0 };

	if (MSM5205InUse) {
		MSM5205NewFrame(0, 3000000, nInterleave);
	}

	if (avengers) {
		mcs51Open(0);
		DrvMCUIdle(nExtraCycles[3]);
	}

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = (248 + i) % 256; // frame starts @ vblank

		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (interrupt_enable && i == (nInterleave - 1)) {
			if (avengers) {
				Z80SetIrqLine(Z80_INPUT_LINE_NMI, 1);
				Z80SetIrqLine(Z80_INPUT_LINE_NMI, 0);
			} else {
				ZetSetVector(0xd7);
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			}
		}
		ZetClose();

		ZetOpen(1);
		CPU_RUN_TIMER(1);
		if ((i % (nInterleave / 4)) == ((nInterleave / 4) - 1)) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		if (MSM5205InUse) {
			ZetOpen(2);
			CPU_RUN(2, Zet);
			CPU_RUN(4, timer);
			MSM5205UpdateScanline(i);
			ZetClose();
		}

		if (avengers) {
			DrvMCUSync();
		}
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];

	if (avengers) {
		nExtraCycles[3] = mcs51TotalCycles() - nCyclesTotal[3];

		mcs51Close();
	}

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		if (MSM5205InUse) MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x200);

	return 0;
}

static INT32 FballFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 5);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}

		if ((DrvInputs[1] & 0x03) == 0) DrvInputs[1] |= 0x03;
		if ((DrvInputs[1] & 0x0c) == 0) DrvInputs[1] |= 0x0c;
		if ((DrvInputs[2] & 0x03) == 0) DrvInputs[2] |= 0x03;
		if ((DrvInputs[2] & 0x0c) == 0) DrvInputs[2] |= 0x0c;
		if ((DrvInputs[3] & 0x03) == 0) DrvInputs[3] |= 0x03;
		if ((DrvInputs[3] & 0x0c) == 0) DrvInputs[3] |= 0x0c;
		if ((DrvInputs[4] & 0x03) == 0) DrvInputs[4] |= 0x03;
		if ((DrvInputs[4] & 0x0c) == 0) DrvInputs[4] |= 0x0c;
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[2] = { cpu_cycles[0] / 60, cpu_cycles[1] / 60};
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetNewFrame();

	for (INT32 i = 0; i < nInterleave; i++, irq_counter++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (interrupt_enable && i == (nInterleave-1)) {
			ZetNmi();
		}
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (irq_counter == 27) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
			irq_counter = 0;
		}
		ZetClose();
	}

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x200);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029692;
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

		if (avengers) {
			DrvMCUScan(nAction);
		}

		SCAN_VAR(DrvSampleBank);

		if (fball) {
			MSM6295Scan(nAction, pnMin);
			oki_bank(0); // wrong
		} else {
			BurnYM2203Scan(nAction, pnMin);
		}
		if (MSM5205InUse) {
			MSM5205Scan(nAction, pnMin);
			timerScan();
		}

		SCAN_VAR(interrupt_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(soundlatch2);
		SCAN_VAR(flipscreen);
		SCAN_VAR(DrvZ80Bank);
		SCAN_VAR(DrvSpriteBank);
		SCAN_VAR(irq_counter);

		SCAN_VAR(avengers_soundlatch2);
		SCAN_VAR(avengers_soundstate);
		SCAN_VAR(trojan_bg2_scrollx);
		SCAN_VAR(trojan_bg2_image);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(DrvZ80Bank);
		ZetClose();
	}

	return 0;
}


// Section Z (US)

static struct BurnRomInfo sectionzRomDesc[] = {
	{ "sz_01.6c",		0x8000, 0x69585125, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "sz_02.7c",		0x8000, 0x22f161b8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sz_03.9c",		0x8000, 0x4c7111ed, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "sz_04.11e",		0x8000, 0xa6073566, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "sz_05.9h",		0x4000, 0x3173ba2e, 4 | BRF_GRA },           //  4 Characters

	{ "sz_14.3e",		0x8000, 0x63782e30, 5 | BRF_GRA },           //  5 Background Layer 1 Tiles
	{ "sz_08.1e",		0x8000, 0xd57d9f13, 5 | BRF_GRA },           //  6
	{ "sz_13.3d",		0x8000, 0x1b3d4d7f, 5 | BRF_GRA },           //  7
	{ "sz_07.1d",		0x8000, 0xf5b3a29f, 5 | BRF_GRA },           //  8
	{ "sz_12.3b",		0x8000, 0x11d47dfd, 5 | BRF_GRA },           //  9
	{ "sz_06.1b",		0x8000, 0xdf703b68, 5 | BRF_GRA },           // 10
	{ "sz_15.3f",		0x8000, 0x36bb9bf7, 5 | BRF_GRA },           // 11
	{ "sz_09.1f",		0x8000, 0xda8f06c9, 5 | BRF_GRA },           // 12

	{ "sz_17.3j",		0x8000, 0x8df7b24a, 6 | BRF_GRA },           // 13 Sprites
	{ "sz_11.1j",		0x8000, 0x685d4c54, 6 | BRF_GRA },           // 14
	{ "sz_16.3h",		0x8000, 0x500ff2bb, 6 | BRF_GRA },           // 15
	{ "sz_10.1h",		0x8000, 0x00b3d244, 6 | BRF_GRA },           // 16

	{ "szb01.15g",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 17 Proms (not used)
};

STD_ROM_PICK(sectionz)
STD_ROM_FN(sectionz)

struct BurnDriver BurnDrvSectionz = {
	"sectionz", NULL, NULL, NULL, "1985",
	"Section Z (US)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_HORSHOOT, 0,
	NULL, sectionzRomInfo, sectionzRomName, NULL, NULL, NULL, NULL, DrvInputInfo, SectionzDIPInfo,
	sectionzInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Section Z (Japan, rev. A)

static struct BurnRomInfo sectionzaRomDesc[] = {
	{ "sz_01a.6c",		0x8000, 0x98df49fd, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "sz_02.7c",		0x8000, 0x22f161b8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "szj_03.9c",		0x8000, 0x94547abf, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "sz_04.11e",		0x8000, 0xa6073566, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "sz_05.9h",		0x4000, 0x3173ba2e, 4 | BRF_GRA },           //  4 Characters

	{ "sz_14.3e",		0x8000, 0x63782e30, 5 | BRF_GRA },           //  5 Background Layer 1 Tiles
	{ "sz_08.1e",		0x8000, 0xd57d9f13, 5 | BRF_GRA },           //  6
	{ "sz_13.3d",		0x8000, 0x1b3d4d7f, 5 | BRF_GRA },           //  7
	{ "sz_07.1d",		0x8000, 0xf5b3a29f, 5 | BRF_GRA },           //  8
	{ "sz_12.3b",		0x8000, 0x11d47dfd, 5 | BRF_GRA },           //  9
	{ "sz_06.1b",		0x8000, 0xdf703b68, 5 | BRF_GRA },           // 10
	{ "sz_15.3f",		0x8000, 0x36bb9bf7, 5 | BRF_GRA },           // 11
	{ "sz_09.1f",		0x8000, 0xda8f06c9, 5 | BRF_GRA },           // 12

	{ "sz_17.3j",		0x8000, 0x8df7b24a, 6 | BRF_GRA },           // 13 Sprites
	{ "sz_11.1j",		0x8000, 0x685d4c54, 6 | BRF_GRA },           // 14
	{ "sz_16.3h",		0x8000, 0x500ff2bb, 6 | BRF_GRA },           // 15
	{ "sz_10.1h",		0x8000, 0x00b3d244, 6 | BRF_GRA },           // 16

	{ "szb01.15g",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 17 Proms (not used)
};

STD_ROM_PICK(sectionza)
STD_ROM_FN(sectionza)

struct BurnDriver BurnDrvSectionza = {
	"sectionza", "sectionz", NULL, NULL, "1985",
	"Section Z (Japan, rev. A)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_HORSHOOT, 0,
	NULL, sectionzaRomInfo, sectionzaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, SectionzDIPInfo,
	sectionzInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Legendary Wings (US, rev. C)

static struct BurnRomInfo lwingsRomDesc[] = {
	{ "lwu_01c.6c",		0x8000, 0xb55a7f60, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "lwu_02c.7c",		0x8000, 0xa5efbb1b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "lw_03.9c",		0x8000, 0xec5cc201, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "lw_04.11e",		0x8000, 0xa20337a2, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "lw_05.9h",		0x4000, 0x091d923c, 4 | BRF_GRA },           //  4 Characters

	{ "lw_14.3e",		0x8000, 0x5436392c, 5 | BRF_GRA },           //  5 Background Layer 1 Tiles
	{ "lw_08.1e",		0x8000, 0xb491bbbb, 5 | BRF_GRA },           //  6
	{ "lw_13.3d",		0x8000, 0xfdd1908a, 5 | BRF_GRA },           //  7
	{ "lw_07.1d",		0x8000, 0x5c73d406, 5 | BRF_GRA },           //  8
	{ "lw_12.3b",		0x8000, 0x32e17b3c, 5 | BRF_GRA },           //  9
	{ "lw_06.1b",		0x8000, 0x52e533c1, 5 | BRF_GRA },           // 10
	{ "lw_15.3f",		0x8000, 0x99e134ba, 5 | BRF_GRA },           // 11
	{ "lw_09.1f",		0x8000, 0xc8f28777, 5 | BRF_GRA },           // 12

	{ "lw_17.3j",		0x8000, 0x5ed1bc9b, 6 | BRF_GRA },           // 13 Sprites
	{ "lw_11.1j",		0x8000, 0x2a0790d6, 6 | BRF_GRA },           // 14
	{ "lw_16.3h",		0x8000, 0xe8834006, 6 | BRF_GRA },           // 15
	{ "lw_10.1h",		0x8000, 0xb693f5a5, 6 | BRF_GRA },           // 16

	{ "szb01.15g",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 17 Proms (not used)
};

STD_ROM_PICK(lwings)
STD_ROM_FN(lwings)

struct BurnDriver BurnDrvLwings = {
	"lwings", NULL, NULL, NULL, "1986",
	"Legendary Wings (US, rev. C)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, lwingsRomInfo, lwingsRomName, NULL, NULL, NULL, NULL, DrvInputInfo, LwingsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Legendary Wings (US)

static struct BurnRomInfo lwingsaRomDesc[] = {
	{ "u13-l",			0x8000, 0x3069c01c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "u14-k",			0x8000, 0x5d91c828, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "lw_03.9c",		0x8000, 0xec5cc201, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "lw_04.11e",		0x8000, 0xa20337a2, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "lw_05.9h",		0x4000, 0x091d923c, 4 | BRF_GRA },           //  4 Characters

	{ "lw_14.3e",		0x8000, 0x5436392c, 5 | BRF_GRA },           //  5 Background Layer 1 Tiles
	{ "lw_08.1e",		0x8000, 0xb491bbbb, 5 | BRF_GRA },           //  6
	{ "lw_13.3d",		0x8000, 0xfdd1908a, 5 | BRF_GRA },           //  7
	{ "lw_07.1d",		0x8000, 0x5c73d406, 5 | BRF_GRA },           //  8
	{ "lw_12.3b",		0x8000, 0x32e17b3c, 5 | BRF_GRA },           //  9
	{ "lw_06.1b",		0x8000, 0x52e533c1, 5 | BRF_GRA },           // 10
	{ "lw_15.3f",		0x8000, 0x99e134ba, 5 | BRF_GRA },           // 11
	{ "lw_09.1f",		0x8000, 0xc8f28777, 5 | BRF_GRA },           // 12

	{ "lw_17.3j",		0x8000, 0x5ed1bc9b, 6 | BRF_GRA },           // 13 Sprites
	{ "lw_11.1j",		0x8000, 0x2a0790d6, 6 | BRF_GRA },           // 14
	{ "lw_16.3h",		0x8000, 0xe8834006, 6 | BRF_GRA },           // 15
	{ "lw_10.1h",		0x8000, 0xb693f5a5, 6 | BRF_GRA },           // 16

	{ "szb01.15g",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 17 Proms (not used)
};

STD_ROM_PICK(lwingsa)
STD_ROM_FN(lwingsa)

struct BurnDriver BurnDrvLwingsa = {
	"lwingsa", "lwings", NULL, NULL, "1986",
	"Legendary Wings (US)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, lwingsaRomInfo, lwingsaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, LwingsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Ares no Tsubasa (Japan)

static struct BurnRomInfo lwingsjRomDesc[] = {
	{ "at_01b.6c",		0x8000, 0x2068a738, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "at_02.7c",		0x8000, 0xd6a2edc4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "at_03.9c",		0x8000, 0xec5cc201, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "at_03.11e",		0x8000, 0xa20337a2, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "at_05.9h",		0x4000, 0x091d923c, 4 | BRF_GRA },           //  4 Characters

	{ "at_14.3e",		0x8000, 0x176e3027, 5 | BRF_GRA },           //  5 Background Layer 1 Tiles
	{ "at_08.1e",		0x8000, 0xf5d25623, 5 | BRF_GRA },           //  6
	{ "at_13.3d",		0x8000, 0x001caa35, 5 | BRF_GRA },           //  7
	{ "at_07.1d",		0x8000, 0x0ba008c3, 5 | BRF_GRA },           //  8
	{ "at_12.3b",		0x8000, 0x4f8182e9, 5 | BRF_GRA },           //  9
	{ "at_06.1b",		0x8000, 0xf1617374, 5 | BRF_GRA },           // 10
	{ "at_15.3f",		0x8000, 0x9b374dcc, 5 | BRF_GRA },           // 11
	{ "at_09.1f",		0x8000, 0x23654e0a, 5 | BRF_GRA },           // 12

	{ "at_17.3j",		0x8000, 0x8f3c763a, 6 | BRF_GRA },           // 13 Sprites
	{ "at_11.1j",		0x8000, 0x7cc90a1d, 6 | BRF_GRA },           // 14
	{ "at_16.3h",		0x8000, 0x7d58f532, 6 | BRF_GRA },           // 15
	{ "at_10.1h",		0x8000, 0x3e396eda, 6 | BRF_GRA },           // 16

	{ "szb01.15g",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 17 Proms (not used)
};

STD_ROM_PICK(lwingsj)
STD_ROM_FN(lwingsj)

struct BurnDriver BurnDrvLwingsj = {
	"lwingsj", "lwings", NULL, NULL, "1986",
	"Ares no Tsubasa (Japan, rev. B)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, lwingsjRomInfo, lwingsjRomName, NULL, NULL, NULL, NULL, DrvInputInfo, LwingsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Ares no Tsubasa (Japan, rev. A)

static struct BurnRomInfo lwingsjaRomDesc[] = {
	{ "at_01a.6c",		0x8000, 0x568f1ea5, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "at_02.7c",		0x8000, 0xd6a2edc4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "at_03.9c",		0x8000, 0xec5cc201, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "at_03.11e",		0x8000, 0xa20337a2, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "at_05.9h",		0x4000, 0x091d923c, 4 | BRF_GRA },           //  4 Characters

	{ "at_14.3e",		0x8000, 0x176e3027, 5 | BRF_GRA },           //  5 Background Layer 1 Tiles
	{ "at_08.1e",		0x8000, 0xf5d25623, 5 | BRF_GRA },           //  6
	{ "at_13.3d",		0x8000, 0x001caa35, 5 | BRF_GRA },           //  7
	{ "at_07.1d",		0x8000, 0x0ba008c3, 5 | BRF_GRA },           //  8
	{ "at_12.3b",		0x8000, 0x4f8182e9, 5 | BRF_GRA },           //  9
	{ "at_06.1b",		0x8000, 0xf1617374, 5 | BRF_GRA },           // 10
	{ "at_15.3f",		0x8000, 0x9b374dcc, 5 | BRF_GRA },           // 11
	{ "at_09.1f",		0x8000, 0x23654e0a, 5 | BRF_GRA },           // 12

	{ "at_17.3j",		0x8000, 0x8f3c763a, 6 | BRF_GRA },           // 13 Sprites
	{ "at_11.1j",		0x8000, 0x7cc90a1d, 6 | BRF_GRA },           // 14
	{ "at_16.3h",		0x8000, 0x7d58f532, 6 | BRF_GRA },           // 15
	{ "at_10.1h",		0x8000, 0x3e396eda, 6 | BRF_GRA },           // 16

	{ "szb01.15g",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 17 Proms (not used)
};

STD_ROM_PICK(lwingsja)
STD_ROM_FN(lwingsja)

struct BurnDriver BurnDrvLwingsja = {
	"lwingsja", "lwings", NULL, NULL, "1986",
	"Ares no Tsubasa (Japan, rev. A)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, lwingsjaRomInfo, lwingsjaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, LwingsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Legendary Wings (bootleg)

static struct BurnRomInfo lwingsbRomDesc[] = {
	{ "ic17.bin",		0x8000, 0xfe8a8823, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ic18.bin",		0x8000, 0x2a00cde8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic19.bin",		0x8000, 0xec5cc201, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ic37.bin",		0x8000, 0xa20337a2, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "ic60.bin",		0x4000, 0x091d923c, 4 | BRF_GRA },           //  4 Characters

	{ "ic50.bin",		0x8000, 0x5436392c, 5 | BRF_GRA },           //  5 Background Layer 1 Tiles
	{ "ic49.bin",		0x8000, 0xffdbdd69, 5 | BRF_GRA },           //  6
	{ "ic26.bin",		0x8000, 0xfdd1908a, 5 | BRF_GRA },           //  7
	{ "ic25.bin",		0x8000, 0x5c73d406, 5 | BRF_GRA },           //  8
	{ "ic2.bin",		0x8000, 0x32e17b3c, 5 | BRF_GRA },           //  9
	{ "ic1.bin",		0x8000, 0x52e533c1, 5 | BRF_GRA },           // 10
	{ "ic63.bin",		0x8000, 0x99e134ba, 5 | BRF_GRA },           // 11
	{ "ic62.bin",		0x8000, 0xc8f28777, 5 | BRF_GRA },           // 12

	{ "ic99.bin",		0x8000, 0x163946da, 6 | BRF_GRA },           // 13 Sprites
	{ "ic98.bin",		0x8000, 0x7cc90a1d, 6 | BRF_GRA },           // 14
	{ "ic87.bin",		0x8000, 0xbca275ac, 6 | BRF_GRA },           // 15
	{ "ic86.bin",		0x8000, 0x3e396eda, 6 | BRF_GRA },           // 16

	{ "63s141.15g",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 17 Proms (not used)
};

STD_ROM_PICK(lwingsb)
STD_ROM_FN(lwingsb)

struct BurnDriver BurnDrvLwingsb = {
	"lwingsb", "lwings", NULL, NULL, "1986",
	"Legendary Wings (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, lwingsbRomInfo, lwingsbRomName, NULL, NULL, NULL, NULL, DrvInputInfo, LwingsbDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Trojan (US set 1)

static struct BurnRomInfo trojanRomDesc[] = {
	{ "tb_04.10n",		0x8000, 0xc1bbeb4e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tb_06.13n",		0x8000, 0xd49592ef, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tb_05.12n",		0x8000, 0x9273b264, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tb_02.15h",		0x8000, 0x21154797, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "tb_01.6d",		0x4000, 0x1c0f91b2, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "tb_03.8k",		0x4000, 0x581a2b4c, 4 | BRF_GRA },           //  5 Characters

	{ "tb_13.6b",		0x8000, 0x285a052b, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "tb_09.6a",		0x8000, 0xaeb693f7, 5 | BRF_GRA },           //  7
	{ "tb_12.4b",		0x8000, 0xdfb0fe5c, 5 | BRF_GRA },           //  8
	{ "tb_08.4a",		0x8000, 0xd3a4c9d1, 5 | BRF_GRA },           //  9
	{ "tb_11.3b",		0x8000, 0x00f0f4fd, 5 | BRF_GRA },           // 10
	{ "tb_07.3a",		0x8000, 0xdff2ee02, 5 | BRF_GRA },           // 11
	{ "tb_14.8b",		0x8000, 0x14bfac18, 5 | BRF_GRA },           // 12
	{ "tb_10.8a",		0x8000, 0x71ba8a6d, 5 | BRF_GRA },           // 13

	{ "tb_18.7l",		0x8000, 0x862c4713, 6 | BRF_GRA },           // 14 Sprites
	{ "tb_16.3l",		0x8000, 0xd86f8cbd, 6 | BRF_GRA },           // 15
	{ "tb_17.5l",		0x8000, 0x12a73b3f, 6 | BRF_GRA },           // 16
	{ "tb_15.2l",		0x8000, 0xbb1a2769, 6 | BRF_GRA },           // 17
	{ "tb_22.7n",		0x8000, 0x39daafd4, 6 | BRF_GRA },           // 18
	{ "tb_20.3n",		0x8000, 0x94615d2a, 6 | BRF_GRA },           // 19
	{ "tb_21.5n",		0x8000, 0x66c642bd, 6 | BRF_GRA },           // 20
	{ "tb_19.2n",		0x8000, 0x81d5ab36, 6 | BRF_GRA },           // 21

	{ "tb_25.15n",		0x8000, 0x6e38c6fa, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "tb_24.13n",		0x8000, 0x14fc6cf2, 7 | BRF_GRA },           // 23

	{ "tb_23.9n",		0x8000, 0xeda13c0e, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "tbb-2.7j",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "tbb-1.1e",		0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(trojan)
STD_ROM_FN(trojan)

struct BurnDriver BurnDrvTrojan = {
	"trojan", NULL, NULL, NULL, "1986",
	"Trojan (US set 1)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, trojanRomInfo, trojanRomName, NULL, NULL, NULL, NULL, DrvInputInfo, TrojanlsDIPInfo,
	TrojanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Trojan (US set 2)

static struct BurnRomInfo trojanaRomDesc[] = {
	{ "tb_04.10n",		0x8000, 0x0113a551, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tb_06.13n",		0x8000, 0xaa127a5b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tb_05.12n",		0x8000, 0x9273b264, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tb_02.15h",		0x8000, 0x21154797, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "tb_01.6d",		0x4000, 0x1c0f91b2, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "tb_03.8k",		0x4000, 0x581a2b4c, 4 | BRF_GRA },           //  5 Characters

	{ "tb_13.6b",		0x8000, 0x285a052b, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "tb_09.6a",		0x8000, 0xaeb693f7, 5 | BRF_GRA },           //  7
	{ "tb_12.4b",		0x8000, 0xdfb0fe5c, 5 | BRF_GRA },           //  8
	{ "tb_08.4a",		0x8000, 0xd3a4c9d1, 5 | BRF_GRA },           //  9
	{ "tb_11.3b",		0x8000, 0x00f0f4fd, 5 | BRF_GRA },           // 10
	{ "tb_07.3a",		0x8000, 0xdff2ee02, 5 | BRF_GRA },           // 11
	{ "tb_14.8b",		0x8000, 0x14bfac18, 5 | BRF_GRA },           // 12
	{ "tb_10.8a",		0x8000, 0x71ba8a6d, 5 | BRF_GRA },           // 13

	{ "tb_18.7l",		0x8000, 0x862c4713, 6 | BRF_GRA },           // 14 Sprites
	{ "tb_16.3l",		0x8000, 0xd86f8cbd, 6 | BRF_GRA },           // 15
	{ "tb_17.5l",		0x8000, 0x12a73b3f, 6 | BRF_GRA },           // 16
	{ "tb_15.2l",		0x8000, 0xbb1a2769, 6 | BRF_GRA },           // 17
	{ "tb_22.7n",		0x8000, 0x39daafd4, 6 | BRF_GRA },           // 18
	{ "tb_20.3n",		0x8000, 0x94615d2a, 6 | BRF_GRA },           // 19
	{ "tb_21.5n",		0x8000, 0x66c642bd, 6 | BRF_GRA },           // 20
	{ "tb_19.2n",		0x8000, 0x81d5ab36, 6 | BRF_GRA },           // 21

	{ "tb_25.15n",		0x8000, 0x6e38c6fa, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "tb_24.13n",		0x8000, 0x14fc6cf2, 7 | BRF_GRA },           // 23

	{ "tb_23.9n",		0x8000, 0xeda13c0e, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "tbb-2.7j",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "tbb-1.1e",		0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(trojana)
STD_ROM_FN(trojana)

struct BurnDriver BurnDrvTrojana = {
	"trojana", "trojan", NULL, NULL, "1986",
	"Trojan (US set 2)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, trojanaRomInfo, trojanaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, TrojanDIPInfo,
	TrojanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Trojan (Romstar, set 1)

static struct BurnRomInfo trojanrRomDesc[] = {
	{ "tbu_04.10n",		0x8000, 0x92670f27, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tbu_06.13n",		0x8000, 0xa4951173, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tbu_05.12n",		0x8000, 0x9273b264, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tb_02.15h",		0x8000, 0x21154797, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "tb_01.6d",		0x4000, 0x1c0f91b2, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "tb_03.8k",		0x4000, 0x581a2b4c, 4 | BRF_GRA },           //  5 Characters

	{ "tb_13.6b",		0x8000, 0x285a052b, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "tb_09.6a",		0x8000, 0xaeb693f7, 5 | BRF_GRA },           //  7
	{ "tb_12.4b",		0x8000, 0xdfb0fe5c, 5 | BRF_GRA },           //  8
	{ "tb_08.4a",		0x8000, 0xd3a4c9d1, 5 | BRF_GRA },           //  9
	{ "tb_11.3b",		0x8000, 0x00f0f4fd, 5 | BRF_GRA },           // 10
	{ "tb_07.3a",		0x8000, 0xdff2ee02, 5 | BRF_GRA },           // 11
	{ "tb_14.8b",		0x8000, 0x14bfac18, 5 | BRF_GRA },           // 12
	{ "tb_10.8a",		0x8000, 0x71ba8a6d, 5 | BRF_GRA },           // 13

	{ "tb_18.7l",		0x8000, 0x862c4713, 6 | BRF_GRA },           // 14 Sprites
	{ "tb_16.3l",		0x8000, 0xd86f8cbd, 6 | BRF_GRA },           // 15
	{ "tb_17.5l",		0x8000, 0x12a73b3f, 6 | BRF_GRA },           // 16
	{ "tb_15.2l",		0x8000, 0xbb1a2769, 6 | BRF_GRA },           // 17
	{ "tb_22.7n",		0x8000, 0x39daafd4, 6 | BRF_GRA },           // 18
	{ "tb_20.3n",		0x8000, 0x94615d2a, 6 | BRF_GRA },           // 19
	{ "tb_21.5n",		0x8000, 0x66c642bd, 6 | BRF_GRA },           // 20
	{ "tb_19.2n",		0x8000, 0x81d5ab36, 6 | BRF_GRA },           // 21

	{ "tb_25.15n",		0x8000, 0x6e38c6fa, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "tb_24.13n",		0x8000, 0x14fc6cf2, 7 | BRF_GRA },           // 23

	{ "tb_23.9n",		0x8000, 0xeda13c0e, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "tbb-2.7j",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "tbb-1.1e",		0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(trojanr)
STD_ROM_FN(trojanr)

struct BurnDriver BurnDrvTrojanr = {
	"trojanr", "trojan", NULL, NULL, "1986",
	"Trojan (Romstar, set 1)\0", NULL, "Capcom (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, trojanrRomInfo, trojanrRomName, NULL, NULL, NULL, NULL, DrvInputInfo, TrojanDIPInfo,
	TrojanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Trojan (Romstar, set 2)

static struct BurnRomInfo trojanraRomDesc[] = {
	{ "tbu_04.10n",		0x8000, 0x0e003b0f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tbu_06.13n",		0x8000, 0xa5a3d848, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tbu_05.12n",		0x8000, 0x9273b264, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tb_02.15h",		0x8000, 0x21154797, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "tb_01.6d",		0x4000, 0x1c0f91b2, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "tb_03.8k",		0x4000, 0x581a2b4c, 4 | BRF_GRA },           //  5 Characters

	{ "tb_13.6b",		0x8000, 0x285a052b, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "tb_09.6a",		0x8000, 0xaeb693f7, 5 | BRF_GRA },           //  7
	{ "tb_12.4b",		0x8000, 0xdfb0fe5c, 5 | BRF_GRA },           //  8
	{ "tb_08.4a",		0x8000, 0xd3a4c9d1, 5 | BRF_GRA },           //  9
	{ "tb_11.3b",		0x8000, 0x00f0f4fd, 5 | BRF_GRA },           // 10
	{ "tb_07.3a",		0x8000, 0xdff2ee02, 5 | BRF_GRA },           // 11
	{ "tb_14.8b",		0x8000, 0x14bfac18, 5 | BRF_GRA },           // 12
	{ "tb_10.8a",		0x8000, 0x71ba8a6d, 5 | BRF_GRA },           // 13

	{ "tb_18.7l",		0x8000, 0x862c4713, 6 | BRF_GRA },           // 14 Sprites
	{ "tb_16.3l",		0x8000, 0xd86f8cbd, 6 | BRF_GRA },           // 15
	{ "tb_17.5l",		0x8000, 0x12a73b3f, 6 | BRF_GRA },           // 16
	{ "tb_15.2l",		0x8000, 0xbb1a2769, 6 | BRF_GRA },           // 17
	{ "tb_22.7n",		0x8000, 0x39daafd4, 6 | BRF_GRA },           // 18
	{ "tb_20.3n",		0x8000, 0x94615d2a, 6 | BRF_GRA },           // 19
	{ "tb_21.5n",		0x8000, 0x66c642bd, 6 | BRF_GRA },           // 20
	{ "tb_19.2n",		0x8000, 0x81d5ab36, 6 | BRF_GRA },           // 21

	{ "tb_25.15n",		0x8000, 0x6e38c6fa, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "tb_24.13n",		0x8000, 0x14fc6cf2, 7 | BRF_GRA },           // 23

	{ "tb_23.9n",		0x8000, 0xeda13c0e, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "tbb-2.7j",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "tbb-1.1e",		0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(trojanra)
STD_ROM_FN(trojanra)

struct BurnDriver BurnDrvTrojanra = {
	"trojanra", "trojan", NULL, NULL, "1986",
	"Trojan (Romstar, set 2)\0", NULL, "Capcom (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, trojanraRomInfo, trojanraRomName, NULL, NULL, NULL, NULL, DrvInputInfo, TrojanDIPInfo,
	TrojanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Trojan (bootleg)

static struct BurnRomInfo trojanbRomDesc[] = {
	{ "4.11l",			0x8000, 0xaad03bc7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "6.11p",			0x8000, 0x8ad19c83, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5.11m",			0x8000, 0x9273b264, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "2.6q",			0x8000, 0x21154797, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "1.3f",			0x8000, 0x83c715b2, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "3.8h",			0x4000, 0x581a2b4c, 4 | BRF_GRA },           //  5 Characters

	{ "13.3e",			0x8000, 0x285a052b, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "9.1e",			0x8000, 0xaeb693f7, 5 | BRF_GRA },           //  7
	{ "12.3d",			0x8000, 0xdfb0fe5c, 5 | BRF_GRA },           //  8
	{ "8.1d",			0x8000, 0xd3a4c9d1, 5 | BRF_GRA },           //  9
	{ "11.3b",			0x8000, 0x00f0f4fd, 5 | BRF_GRA },           // 10
	{ "7.1b",			0x8000, 0xdff2ee02, 5 | BRF_GRA },           // 11
	{ "14.3g",			0x8000, 0x14bfac18, 5 | BRF_GRA },           // 12
	{ "10.1g",			0x8000, 0x71ba8a6d, 5 | BRF_GRA },           // 13

	{ "18.10f",			0x8000, 0x862c4713, 6 | BRF_GRA },           // 14 Sprites
	{ "16.10c",			0x8000, 0xd86f8cbd, 6 | BRF_GRA },           // 15
	{ "17.10e",			0x8000, 0x12a73b3f, 6 | BRF_GRA },           // 16
	{ "15.10b",			0x8000, 0xbb1a2769, 6 | BRF_GRA },           // 17
	{ "22.12f",			0x8000, 0x39daafd4, 6 | BRF_GRA },           // 18
	{ "20.12c",			0x8000, 0x94615d2a, 6 | BRF_GRA },           // 19
	{ "21.12e",			0x8000, 0x66c642bd, 6 | BRF_GRA },           // 20
	{ "19.12b",			0x8000, 0x81d5ab36, 6 | BRF_GRA },           // 21

	{ "25.12q",			0x8000, 0x6e38c6fa, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "24.12o",			0x8000, 0x14fc6cf2, 7 | BRF_GRA },           // 23

	{ "23.12h",			0x8000, 0xeda13c0e, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "82s129.8g",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "82s129.4a",		0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(trojanb)
STD_ROM_FN(trojanb)

struct BurnDriver BurnDrvTrojanb = {
	"trojanb", "trojan", NULL, NULL, "1986",
	"Trojan (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, trojanbRomInfo, trojanbRomName, NULL, NULL, NULL, NULL, DrvInputInfo, TrojanDIPInfo,
	TrojanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Tatakai no Banka (Japan)

static struct BurnRomInfo trojanjRomDesc[] = {
	{ "tb_04.10n",		0x8000, 0x0b5a7f49, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tb_06.13n",		0x8000, 0xdee6ed92, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tb_05.12n",		0x8000, 0x9273b264, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tb_02.15h",		0x8000, 0x21154797, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "tb_01.6d",		0x8000, 0x83c715b2, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "tb_03.8k",		0x4000, 0x581a2b4c, 4 | BRF_GRA },           //  5 Characters

	{ "tb_13.6b",		0x8000, 0x285a052b, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "tb_09.6a",		0x8000, 0xaeb693f7, 5 | BRF_GRA },           //  7
	{ "tb_12.4b",		0x8000, 0xdfb0fe5c, 5 | BRF_GRA },           //  8
	{ "tb_08.4a",		0x8000, 0xd3a4c9d1, 5 | BRF_GRA },           //  9
	{ "tb_11.3b",		0x8000, 0x00f0f4fd, 5 | BRF_GRA },           // 10
	{ "tb_07.3a",		0x8000, 0xdff2ee02, 5 | BRF_GRA },           // 11
	{ "tb_14.8b",		0x8000, 0x14bfac18, 5 | BRF_GRA },           // 12
	{ "tb_10.8a",		0x8000, 0x71ba8a6d, 5 | BRF_GRA },           // 13

	{ "tb_18.7l",		0x8000, 0x862c4713, 6 | BRF_GRA },           // 14 Sprites
	{ "tb_16.3l",		0x8000, 0xd86f8cbd, 6 | BRF_GRA },           // 15
	{ "tb_17.5l",		0x8000, 0x12a73b3f, 6 | BRF_GRA },           // 16
	{ "tb_15.2l",		0x8000, 0xbb1a2769, 6 | BRF_GRA },           // 17
	{ "tb_22.7n",		0x8000, 0x39daafd4, 6 | BRF_GRA },           // 18
	{ "tb_20.3n",		0x8000, 0x94615d2a, 6 | BRF_GRA },           // 19
	{ "tb_21.5n",		0x8000, 0x66c642bd, 6 | BRF_GRA },           // 20
	{ "tb_19.2n",		0x8000, 0x81d5ab36, 6 | BRF_GRA },           // 21

	{ "tb_25.15n",		0x8000, 0x6e38c6fa, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "tb_24.13n",		0x8000, 0x14fc6cf2, 7 | BRF_GRA },           // 23

	{ "tb_23.9n",		0x8000, 0xeda13c0e, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "tbb-2.7j",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "tbb-1.1e",		0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(trojanj)
STD_ROM_FN(trojanj)

struct BurnDriver BurnDrvTrojanj = {
	"trojanj", "trojan", NULL, NULL, "1986",
	"Tatakai no Banka (Japan)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, trojanjRomInfo, trojanjRomName, NULL, NULL, NULL, NULL, DrvInputInfo, TrojanDIPInfo,
	TrojanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Tatakai no Banka (Japan, old ver.)

static struct BurnRomInfo trojanjoRomDesc[] = {
	{ "tb_04.10n",		0x8000, 0x134dc35b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tb_06.13n",		0x8000, 0xfdda9d55, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tb_05.12n",		0x8000, 0x9273b264, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tb_02.15h",		0x8000, 0x21154797, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "tb_01.6d",		0x8000, 0x83c715b2, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "tb_03.8k",		0x4000, 0x581a2b4c, 4 | BRF_GRA },           //  5 Characters

	{ "tb_13.6b",		0x8000, 0x285a052b, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "tb_09.6a",		0x8000, 0xaeb693f7, 5 | BRF_GRA },           //  7
	{ "tb_12.4b",		0x8000, 0xdfb0fe5c, 5 | BRF_GRA },           //  8
	{ "tb_08.4a",		0x8000, 0xd3a4c9d1, 5 | BRF_GRA },           //  9
	{ "tb_11.3b",		0x8000, 0x00f0f4fd, 5 | BRF_GRA },           // 10
	{ "tb_07.3a",		0x8000, 0xdff2ee02, 5 | BRF_GRA },           // 11
	{ "tb_14.8b",		0x8000, 0x14bfac18, 5 | BRF_GRA },           // 12
	{ "tb_10.8a",		0x8000, 0x71ba8a6d, 5 | BRF_GRA },           // 13

	{ "tb_18.7l",		0x8000, 0x862c4713, 6 | BRF_GRA },           // 14 Sprites
	{ "tb_16.3l",		0x8000, 0xd86f8cbd, 6 | BRF_GRA },           // 15
	{ "tb_17.5l",		0x8000, 0x12a73b3f, 6 | BRF_GRA },           // 16
	{ "tb_15.2l",		0x8000, 0xbb1a2769, 6 | BRF_GRA },           // 17
	{ "tb_22.7n",		0x8000, 0x39daafd4, 6 | BRF_GRA },           // 18
	{ "tb_20.3n",		0x8000, 0x94615d2a, 6 | BRF_GRA },           // 19
	{ "tb_21.5n",		0x8000, 0x66c642bd, 6 | BRF_GRA },           // 20
	{ "tb_19.2n",		0x8000, 0x81d5ab36, 6 | BRF_GRA },           // 21

	{ "tb_25.15n",		0x8000, 0x6e38c6fa, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "tb_24.13n",		0x8000, 0x14fc6cf2, 7 | BRF_GRA },           // 23

	{ "tb_23.9n",		0x8000, 0xeda13c0e, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "tbb-2.7j",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "tbb-1.1e",		0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(trojanjo)
STD_ROM_FN(trojanjo)

struct BurnDriver BurnDrvTrojanjo = {
	"trojanjo", "trojan", NULL, NULL, "1986",
	"Tatakai no Banka (Japan, old ver.)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, trojanjoRomInfo, trojanjoRomName, NULL, NULL, NULL, NULL, DrvInputInfo, TrojanDIPInfo,
	TrojanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Trojan (location test)

static struct BurnRomInfo trojanltRomDesc[] = {
	{ "tb_04.10n",		0x8000, 0x52a4f8a1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tb_06.13n",		0x8000, 0xef182e53, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tb_05.12n",		0x8000, 0x9273b264, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tb_02.15h",		0x8000, 0x21154797, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "tb01.3f",		0x8000, 0x83c715b2, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "tb_03.8k",		0x4000, 0x581a2b4c, 4 | BRF_GRA },           //  5 Characters

	{ "tb_13.6b",		0x8000, 0x285a052b, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "tb_09.6a",		0x8000, 0xaeb693f7, 5 | BRF_GRA },           //  7
	{ "tb_12.4b",		0x8000, 0xdfb0fe5c, 5 | BRF_GRA },           //  8
	{ "tb_08.4a",		0x8000, 0xd3a4c9d1, 5 | BRF_GRA },           //  9
	{ "tb_11.3b",		0x8000, 0x00f0f4fd, 5 | BRF_GRA },           // 10
	{ "tb_07.3a",		0x8000, 0xdff2ee02, 5 | BRF_GRA },           // 11
	{ "tb_14.8b",		0x8000, 0x14bfac18, 5 | BRF_GRA },           // 12
	{ "tb_10.8a",		0x8000, 0x71ba8a6d, 5 | BRF_GRA },           // 13

	{ "tb_18.7l",		0x8000, 0x862c4713, 6 | BRF_GRA },           // 14 Sprites
	{ "tb_16.3l",		0x8000, 0xd86f8cbd, 6 | BRF_GRA },           // 15
	{ "tb_17.5l",		0x8000, 0x12a73b3f, 6 | BRF_GRA },           // 16
	{ "tb_15.2l",		0x8000, 0xbb1a2769, 6 | BRF_GRA },           // 17
	{ "tb_22.7n",		0x8000, 0x39daafd4, 6 | BRF_GRA },           // 18
	{ "tb_20.3n",		0x8000, 0x94615d2a, 6 | BRF_GRA },           // 19
	{ "tb_21.5n",		0x8000, 0x66c642bd, 6 | BRF_GRA },           // 20
	{ "tb_19.2n",		0x8000, 0x81d5ab36, 6 | BRF_GRA },           // 21

	{ "tb_25.15n",		0x8000, 0x6e38c6fa, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "tb_24.13n",		0x8000, 0x14fc6cf2, 7 | BRF_GRA },           // 23

	{ "tb_23.9n",		0x8000, 0xeda13c0e, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "tbb-2.7j",		0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "tbb-1.1e",		0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(trojanlt)
STD_ROM_FN(trojanlt)

struct BurnDriver BurnDrvTrojanlt = {
	"trojanlt", "trojan", NULL, NULL, "1986",
	"Trojan (location test)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, trojanltRomInfo, trojanltRomName, NULL, NULL, NULL, NULL, DrvInputInfo, TrojanDIPInfo,
	TrojanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


/*
Known to exist but currently undumped, a set with all ROMs stamped U for the US region and stamped as
  revision D, with each having a red stripe across the label.
It was common for Capcom to use the same ROM label across regional sets but add a RED stripe for the US
  region, BLUE stripe for Europe and no stripe for the Japanese region.
*/

// Avengers (US, rev. D)

static struct BurnRomInfo avengersRomDesc[] = {
	{ "avu_04d.10n",	0x8000, 0xa94aadcc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "avu_06d.13n",	0x8000, 0x39cd80bd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "avu_05d.12n",	0x8000, 0x06b1cec9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "av_02.15h",		0x8000, 0x107a2e17, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "av_01.6d",		0x8000, 0xc1e5d258, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "av_03.8k",		0x8000, 0xefb5883e, 4 | BRF_GRA },           //  5 Characters

	{ "av_13.6b",		0x8000, 0x9b5ff305, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "av_09.6a",		0x8000, 0x08323355, 5 | BRF_GRA },           //  7
	{ "av_12.4b",		0x8000, 0x6d5261ba, 5 | BRF_GRA },           //  8
	{ "av_08.4a",		0x8000, 0xa13d9f54, 5 | BRF_GRA },           //  9
	{ "av_11.3b",		0x8000, 0xa2911d8b, 5 | BRF_GRA },           // 10
	{ "av_07.3a",		0x8000, 0xcde78d32, 5 | BRF_GRA },           // 11
	{ "av_14.8b",		0x8000, 0x44ac2671, 5 | BRF_GRA },           // 12
	{ "av_10.8a",		0x8000, 0xb1a717cb, 5 | BRF_GRA },           // 13

	{ "av_18.7l",		0x8000, 0x3c876a17, 6 | BRF_GRA },           // 14 Sprites
	{ "av_16.3l",		0x8000, 0x4b1ff3ac, 6 | BRF_GRA },           // 15
	{ "av_17.5l",		0x8000, 0x4eb543ef, 6 | BRF_GRA },           // 16
	{ "av_15.2l",		0x8000, 0x8041de7f, 6 | BRF_GRA },           // 17
	{ "av_22.7n",		0x8000, 0xbdaa8b22, 6 | BRF_GRA },           // 18
	{ "av_20.3n",		0x8000, 0x566e3059, 6 | BRF_GRA },           // 19
	{ "av_21.5n",		0x8000, 0x301059aa, 6 | BRF_GRA },           // 20
	{ "av_19.2n",		0x8000, 0xa00485ec, 6 | BRF_GRA },           // 21

	{ "avu_25.15n",		0x8000, 0x230d9e30, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "avu_24.13n",		0x8000, 0xa6354024, 7 | BRF_GRA },           // 23

	{ "av_23.9n",		0x8000, 0xc0a93ef6, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "tbb_2bpr.7j",	0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "tbb_1bpr.1e",	0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26

	{ "av.13k",			0x1000, 0x505a0987, 0 | BRF_PRG | BRF_ESS }, // 27 MCU
};

STD_ROM_PICK(avengers)
STD_ROM_FN(avengers)

static INT32 AvengersInit()
{
	avengers = 1;

	return TrojanInit();
}

struct BurnDriver BurnDrvAvengers = {
	"avengers", NULL, NULL, NULL, "1987",
	"Avengers (US, rev. D)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_SCRFIGHT, 0,
	NULL, avengersRomInfo, avengersRomName, NULL, NULL, NULL, NULL, DrvInputInfo, AvengersDIPInfo,
	AvengersInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Avengers (US, rev. C)

static struct BurnRomInfo avengersaRomDesc[] = {
	{ "avu_04c.10n",	0x8000, 0x4555b925, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "avu_06c.13n",	0x8000, 0xea202879, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "av_05.12n",		0x8000, 0x9a214b42, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "av_02.15h",		0x8000, 0x107a2e17, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "av_01.6d",		0x8000, 0xc1e5d258, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "av_03.8k",		0x8000, 0xefb5883e, 4 | BRF_GRA },           //  5 Characters

	{ "av_13.6b",		0x8000, 0x9b5ff305, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "av_09.6a",		0x8000, 0x08323355, 5 | BRF_GRA },           //  7
	{ "av_12.4b",		0x8000, 0x6d5261ba, 5 | BRF_GRA },           //  8
	{ "av_08.4a",		0x8000, 0xa13d9f54, 5 | BRF_GRA },           //  9
	{ "av_11.3b",		0x8000, 0xa2911d8b, 5 | BRF_GRA },           // 10
	{ "av_07.3a",		0x8000, 0xcde78d32, 5 | BRF_GRA },           // 11
	{ "av_14.8b",		0x8000, 0x44ac2671, 5 | BRF_GRA },           // 12
	{ "av_10.8a",		0x8000, 0xb1a717cb, 5 | BRF_GRA },           // 13

	{ "av_18.7l",		0x8000, 0x3c876a17, 6 | BRF_GRA },           // 14 Sprites
	{ "av_16.3l",		0x8000, 0x4b1ff3ac, 6 | BRF_GRA },           // 15
	{ "av_17.5l",		0x8000, 0x4eb543ef, 6 | BRF_GRA },           // 16
	{ "av_15.2l",		0x8000, 0x8041de7f, 6 | BRF_GRA },           // 17
	{ "av_22.7n",		0x8000, 0xbdaa8b22, 6 | BRF_GRA },           // 18
	{ "av_20.3n",		0x8000, 0x566e3059, 6 | BRF_GRA },           // 19
	{ "av_21.5n",		0x8000, 0x301059aa, 6 | BRF_GRA },           // 20
	{ "av_19.2n",		0x8000, 0xa00485ec, 6 | BRF_GRA },           // 21

	{ "avu_25.15n",		0x8000, 0x230d9e30, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "avu_24.13n",		0x8000, 0xa6354024, 7 | BRF_GRA },           // 23

	{ "av_23.9n",		0x8000, 0xc0a93ef6, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "tbb_2bpr.7j",	0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "tbb_1bpr.1e",	0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26

	{ "av.13k",			0x1000, 0x505a0987, 0 | BRF_PRG | BRF_ESS }, // 27 MCU
};

STD_ROM_PICK(avengersa)
STD_ROM_FN(avengersa)

struct BurnDriver BurnDrvAvengersa = {
	"avengersa", "avengers", NULL, NULL, "1987",
	"Avengers (US, rev. C)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_SCRFIGHT, 0,
	NULL, avengersaRomInfo, avengersaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, AvengersDIPInfo,
	AvengersInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Avengers (US, rev. A)

static struct BurnRomInfo avengersbRomDesc[] = {
	{ "av_04a.10n",		0x8000, 0x0fea7ac5, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "av_06a.13n",		0x8000, 0x491a712c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "av_05.12n",		0x8000, 0x9a214b42, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "av_02.15h",		0x8000, 0x107a2e17, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "av_01.6d",		0x8000, 0xc1e5d258, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "av_03.8k",		0x8000, 0xefb5883e, 4 | BRF_GRA },           //  5 Characters

	{ "av_13.6b",		0x8000, 0x9b5ff305, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "av_09.6a",		0x8000, 0x08323355, 5 | BRF_GRA },           //  7
	{ "av_12.4b",		0x8000, 0x6d5261ba, 5 | BRF_GRA },           //  8
	{ "av_08.4a",		0x8000, 0xa13d9f54, 5 | BRF_GRA },           //  9
	{ "av_11.3b",		0x8000, 0xa2911d8b, 5 | BRF_GRA },           // 10
	{ "av_07.3a",		0x8000, 0xcde78d32, 5 | BRF_GRA },           // 11
	{ "av_14.8b",		0x8000, 0x44ac2671, 5 | BRF_GRA },           // 12
	{ "av_10.8a",		0x8000, 0xb1a717cb, 5 | BRF_GRA },           // 13

	{ "av_18.7l",		0x8000, 0x3c876a17, 6 | BRF_GRA },           // 14 Sprites
	{ "av_16.3l",		0x8000, 0x4b1ff3ac, 6 | BRF_GRA },           // 15
	{ "av_17.5l",		0x8000, 0x4eb543ef, 6 | BRF_GRA },           // 16
	{ "av_15.2l",		0x8000, 0x8041de7f, 6 | BRF_GRA },           // 17
	{ "av_22.7n",		0x8000, 0xbdaa8b22, 6 | BRF_GRA },           // 18
	{ "av_20.3n",		0x8000, 0x566e3059, 6 | BRF_GRA },           // 19
	{ "av_21.5n",		0x8000, 0x301059aa, 6 | BRF_GRA },           // 20
	{ "av_19.2n",		0x8000, 0xa00485ec, 6 | BRF_GRA },           // 21

	{ "avu_25.15n",		0x8000, 0x230d9e30, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "avu_24.13n",		0x8000, 0xa6354024, 7 | BRF_GRA },           // 23

	{ "av_23.9n",		0x8000, 0xc0a93ef6, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "tbb_2bpr.7j",	0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "tbb_1bpr.1e",	0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26

	{ "av.13k",			0x1000, 0x505a0987, 0 | BRF_PRG | BRF_ESS }, // 27 MCU
};

STD_ROM_PICK(avengersb)
STD_ROM_FN(avengersb)

struct BurnDriver BurnDrvAvengersb = {
	"avengersb", "avengers", NULL, NULL, "1987",
	"Avengers (US, rev. A)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_SCRFIGHT, 0,
	NULL, avengersbRomInfo, avengersbRomName, NULL, NULL, NULL, NULL, DrvInputInfo, AvengersDIPInfo,
	AvengersInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Avengers (US)

static struct BurnRomInfo avengerscRomDesc[] = {
	{ "av_04.10n",		0x8000, 0xc785e1f2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "av_06.13n",		0x8000, 0xc6f84a5f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "av_05.12n",		0x8000, 0xf9a9a92f, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "av_02.15h",		0x8000, 0x107a2e17, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "av_01.6d",		0x8000, 0xc1e5d258, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "av_03.8k",		0x8000, 0xefb5883e, 4 | BRF_GRA },           //  5 Characters

	{ "av_13.6b",		0x8000, 0x9b5ff305, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "av_09.6a",		0x8000, 0x08323355, 5 | BRF_GRA },           //  7
	{ "av_12.4b",		0x8000, 0x6d5261ba, 5 | BRF_GRA },           //  8
	{ "av_08.4a",		0x8000, 0xa13d9f54, 5 | BRF_GRA },           //  9
	{ "av_11.3b",		0x8000, 0xa2911d8b, 5 | BRF_GRA },           // 10
	{ "av_07.3a",		0x8000, 0xcde78d32, 5 | BRF_GRA },           // 11
	{ "av_14.8b",		0x8000, 0x44ac2671, 5 | BRF_GRA },           // 12
	{ "av_10.8a",		0x8000, 0xb1a717cb, 5 | BRF_GRA },           // 13

	{ "av_18.7l",		0x8000, 0x3c876a17, 6 | BRF_GRA },           // 14 Sprites
	{ "av_16.3l",		0x8000, 0x4b1ff3ac, 6 | BRF_GRA },           // 15
	{ "av_17.5l",		0x8000, 0x4eb543ef, 6 | BRF_GRA },           // 16
	{ "av_15.2l",		0x8000, 0x8041de7f, 6 | BRF_GRA },           // 17
	{ "av_22.7n",		0x8000, 0xbdaa8b22, 6 | BRF_GRA },           // 18
	{ "av_20.3n",		0x8000, 0x566e3059, 6 | BRF_GRA },           // 19
	{ "av_21.5n",		0x8000, 0x301059aa, 6 | BRF_GRA },           // 20
	{ "av_19.2n",		0x8000, 0xa00485ec, 6 | BRF_GRA },           // 21

	{ "avu_25.15n",		0x8000, 0x230d9e30, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "avu_24.13n",		0x8000, 0xa6354024, 7 | BRF_GRA },           // 23

	{ "av_23.9n",		0x8000, 0xc0a93ef6, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "tbb_2bpr.7j",	0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "tbb_1bpr.1e",	0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26

	{ "av.13k",			0x1000, 0x505a0987, 0 | BRF_PRG | BRF_ESS }, // 27 MCU
};

STD_ROM_PICK(avengersc)
STD_ROM_FN(avengersc)

struct BurnDriver BurnDrvAvengersc = {
	"avengersc", "avengers", NULL, NULL, "1987",
	"Avengers (US)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_SCRFIGHT, 0,
	NULL, avengerscRomInfo, avengerscRomName, NULL, NULL, NULL, NULL, DrvInputInfo, AvengersDIPInfo,
	AvengersInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Hissatsu Buraiken (Japan, rev. A)

static struct BurnRomInfo buraikenRomDesc[] = {
	{ "av_04a.10n",		0x8000, 0x361fc614, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "av_06a.13n",		0x8000, 0x491a712c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "av_05.12n",		0x8000, 0x9a214b42, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "av_02.15h",		0x8000, 0x107a2e17, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "av_01.6d",		0x8000, 0xc1e5d258, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "av_03.8k",		0x8000, 0xefb5883e, 4 | BRF_GRA },           //  5 Characters

	{ "av_13.6b",		0x8000, 0x9b5ff305, 5 | BRF_GRA },           //  6 Background Layer 1 Tiles
	{ "av_09.6a",		0x8000, 0x08323355, 5 | BRF_GRA },           //  7
	{ "av_12.4b",		0x8000, 0x6d5261ba, 5 | BRF_GRA },           //  8
	{ "av_08.4a",		0x8000, 0xa13d9f54, 5 | BRF_GRA },           //  9
	{ "av_11.3b",		0x8000, 0xa2911d8b, 5 | BRF_GRA },           // 10
	{ "av_07.3a",		0x8000, 0xcde78d32, 5 | BRF_GRA },           // 11
	{ "av_14.8b",		0x8000, 0x44ac2671, 5 | BRF_GRA },           // 12
	{ "av_10.8a",		0x8000, 0xb1a717cb, 5 | BRF_GRA },           // 13

	{ "av_18.7l",		0x8000, 0x3c876a17, 6 | BRF_GRA },           // 14 Sprites
	{ "av_16.3l",		0x8000, 0x4b1ff3ac, 6 | BRF_GRA },           // 15
	{ "av_17.5l",		0x8000, 0x4eb543ef, 6 | BRF_GRA },           // 16
	{ "av_15.2l",		0x8000, 0x8041de7f, 6 | BRF_GRA },           // 17
	{ "av_22.7n",		0x8000, 0xbdaa8b22, 6 | BRF_GRA },           // 18
	{ "av_20.3n",		0x8000, 0x566e3059, 6 | BRF_GRA },           // 19
	{ "av_21.5n",		0x8000, 0x301059aa, 6 | BRF_GRA },           // 20
	{ "av_19.2n",		0x8000, 0xa00485ec, 6 | BRF_GRA },           // 21

	{ "av_25.15n",		0x8000, 0x88a505a7, 7 | BRF_GRA },           // 22 Background Layer 2 Tiles
	{ "av_24.13n",		0x8000, 0x1f4463c8, 7 | BRF_GRA },           // 23

	{ "av_23.9n",		0x8000, 0xc0a93ef6, 8 | BRF_GRA },           // 24 Background Layer 2 Tile Map

	{ "tbb_2bpr.7j",	0x0100, 0xd96bcc98, 0 | BRF_OPT },           // 25 Proms (not used)
	{ "tbb_1bpr.1e",	0x0100, 0x5052fa9d, 0 | BRF_OPT },           // 26

	{ "av.13k",			0x1000, 0x505a0987, 0 | BRF_PRG | BRF_ESS }, // 27 MCU
};

STD_ROM_PICK(buraiken)
STD_ROM_FN(buraiken)

struct BurnDriver BurnDrvBuraiken = {
	"buraiken", "avengers", NULL, NULL, "1987",
	"Hissatsu Buraiken (Japan, rev. A)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_SCRFIGHT, 0,
	NULL, buraikenRomInfo, buraikenRomName, NULL, NULL, NULL, NULL, DrvInputInfo, AvengersDIPInfo,
	AvengersInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Fire Ball (FM Work)

static struct BurnRomInfo fballRomDesc[] = {
	{ "d4.bin",			0x20000, 0x6122b3dc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "a05.bin",		0x10000, 0x474dd19e, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "j03.bin",		0x10000, 0xbe11627f, 3 | BRF_GRA },           //  2 Characters

	{ "b15.bin",		0x20000, 0x2169ad3e, 4 | BRF_GRA },           //  3 Background Layer 1 Tiles
	{ "c15.bin",		0x20000, 0x0f77b03e, 4 | BRF_GRA },           //  4
	{ "e15.bin",		0x20000, 0x89a761d2, 4 | BRF_GRA },           //  5
	{ "f15.bin",		0x20000, 0x34b3f9a2, 4 | BRF_GRA },           //  6

	{ "j15.bin",		0x20000, 0xed7be8e7, 5 | BRF_GRA },           //  7 Sprites
	{ "h15.bin",		0x20000, 0x6ffb5433, 5 | BRF_GRA },           //  8

	{ "a03.bin",		0x40000, 0x22b0d089, 6 | BRF_SND },           //  9 msm6295 Samples
	{ "a02.bin",		0x40000, 0x951d6579, 6 | BRF_SND },           // 10
	{ "a01.bin",		0x40000, 0x020b5261, 6 | BRF_SND },           // 11
};

STD_ROM_PICK(fball)
STD_ROM_FN(fball)

struct BurnDriver BurnDrvFball = {
	"fball", NULL, NULL, NULL, "1992",
	"Fire Ball (FM Work)\0", "Black Bar on the left side is normal.", "FM Work", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_MAZE, 0,
	NULL, fballRomInfo, fballRomName, NULL, NULL, NULL, NULL, FballInputInfo, FballDIPInfo,
	FballInit, DrvExit, FballFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};
