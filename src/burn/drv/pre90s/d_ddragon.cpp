// FB Neo Double Dragon driver module
// Based on MAME driver by Philip Bennett,Carlos A. Lozano, Rob Rosenbrock,
// Phil Stroffolino, Ernesto Corvi, David Haywood, and R. Belmont

// to investigate: stoffy - hs broken applying(?)

#include "tiles_generic.h"
#include "z80_intf.h"
#include "hd6309_intf.h"
#include "m6800_intf.h"
#include "m6805_intf.h"
#include "m6809_intf.h"
#include "burn_ym2151.h"
#include "msm5205.h"
#include "msm6295.h"
#include "bitswap.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *RamStart;
static UINT8 *RamEnd;
static UINT8 *DrvHD6309Rom;
static UINT8 *DrvSubCPURom;
static UINT8 *DrvSoundCPURom;
static UINT8 *DrvMCURom;
static UINT8 *DrvMSM5205Rom;
static UINT8 *DrvHD6309Ram;
static UINT8 *DrvSubCPURam;
static UINT8 *DrvSoundCPURam;
static UINT8 *DrvMCURam;
static UINT8 *DrvFgVideoRam;
static UINT8 *DrvBgVideoRam;
static UINT8 *DrvSpriteRam;
static UINT8 *DrvShareRam;
static UINT8 *DrvPaletteRam;
static UINT8 *DrvChars;
static UINT8 *DrvTiles;
static UINT8 *DrvSprites;

static UINT32 *DrvPalette;

static INT32 gfx_count[3]; // tile counts
static INT32 snd_len[2]; // sound rom lengths

static INT32 nCycles[4];
static INT32 nExtraCycles[4];

static UINT8 main_bank;
static UINT8 main_last;
static UINT8 sub_disable;
static UINT8 sub_last;
static UINT8 vblank;
static UINT8 soundlatch;
static UINT16 scrollx;
static UINT16 scrolly;

static UINT8 adpcm_idle[2];
static UINT32 adpcm_pos[2];
static UINT32 adpcm_end[2];
static INT32 adpcm_data[2];

static INT32 subcpu_type;
static INT32 soundcpu_type;
enum {
	CPU_NONE	= 0,
	CPU_HD63701	= 1,
	CPU_HD6309	= 2,
	CPU_M6803	= 3,
	CPU_Z80		= 4,
	CPU_M6809	= 5
};

static INT32 is_game;
enum {
	DDRAGON		= 0,
	DDRAGON2	= 1,
	DARKTOWER	= 2,
	TOFFY		= 3,
	TSTRIKE     = 4
};

static UINT8 DrvJoy0[8];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 lastline;
static INT32 scanline;

static struct BurnInputInfo DdragonInputList[] =
{
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy0 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy0 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy0 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy0 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy0 + 0,	"p1 right"	},
	{"P1 Fire 1",		BIT_DIGITAL,	DrvJoy0 + 4,	"p1 fire 1"	},
	{"P1 Fire 2",		BIT_DIGITAL,	DrvJoy0 + 5,	"p1 fire 2"	},
	{"P1 Fire 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy0 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 right"	},
	{"P2 Fire 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"P2 Fire 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},
	{"P2 Fire 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Dip 1",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip 2",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ddragon)

static struct BurnDIPInfo DdragonDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x00, 0x01, 0x07, 0x00, "4 Coins 1 Credit"			},
	{0x00, 0x01, 0x07, 0x01, "3 Coins 1 Credit"			},
	{0x00, 0x01, 0x07, 0x02, "2 Coins 1 Credit"			},
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Credit"			},
	{0x00, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x00, 0x01, 0x07, 0x03, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x00, 0x01, 0x38, 0x00, "4 Coins 1 Credit"			},
	{0x00, 0x01, 0x38, 0x08, "3 Coins 1 Credit"			},
	{0x00, 0x01, 0x38, 0x10, "2 Coins 1 Credit"			},
	{0x00, 0x01, 0x38, 0x38, "1 Coin  1 Credit"			},
	{0x00, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x00, 0x01, 0x38, 0x18, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x40, 0x40, "Upright"					},
	{0x00, 0x01, 0x40, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x00, 0x01, 0x80, 0x80, "Off"						},
	{0x00, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0x03, 0x01, "Easy"						},
	{0x01, 0x01, 0x03, 0x03, "Medium"					},
	{0x01, 0x01, 0x03, 0x02, "Hard"						},
	{0x01, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x01, 0x01, 0x04, 0x00, "Off"						},
	{0x01, 0x01, 0x04, 0x04, "On"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x01, 0x01, 0x30, 0x10, "20k"						},
	{0x01, 0x01, 0x30, 0x00, "40k"						},
	{0x01, 0x01, 0x30, 0x30, "30k and every 60k"		},
	{0x01, 0x01, 0x30, 0x20, "40k and every 80k"		},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0xc0, 0xc0, "2"						},
	{0x01, 0x01, 0xc0, 0x80, "3"						},
	{0x01, 0x01, 0xc0, 0x40, "4"						},
	{0x01, 0x01, 0xc0, 0x00, "Infinite (Cheat)"			},
};

STDDIPINFO(Ddragon)

static struct BurnDIPInfo Ddragon2DIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0x96, NULL						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x00, 0x01, 0x07, 0x00, "4 Coins 1 Credit"			},
	{0x00, 0x01, 0x07, 0x01, "3 Coins 1 Credit"			},
	{0x00, 0x01, 0x07, 0x02, "2 Coins 1 Credit"			},
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Credit"			},
	{0x00, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x00, 0x01, 0x07, 0x03, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x00, 0x01, 0x38, 0x00, "4 Coins 1 Credit"			},
	{0x00, 0x01, 0x38, 0x08, "3 Coins 1 Credit"			},
	{0x00, 0x01, 0x38, 0x10, "2 Coins 1 Credit"			},
	{0x00, 0x01, 0x38, 0x38, "1 Coin  1 Credit"			},
	{0x00, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x00, 0x01, 0x38, 0x18, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x40, 0x40, "Upright"					},
	{0x00, 0x01, 0x40, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x00, 0x01, 0x80, 0x80, "Off"						},
	{0x00, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0x03, 0x01, "Easy"						},
	{0x01, 0x01, 0x03, 0x03, "Medium"					},
	{0x01, 0x01, 0x03, 0x02, "Hard"						},
	{0x01, 0x01, 0x03, 0x00, "Hardest"					},

	{0	 , 0xfe, 0	 ,    2, "Demo Sounds"				},
	{0x01, 0x01, 0x04, 0x00, "Off"						},
	{0x01, 0x01, 0x04, 0x04, "On"						},

	{0   , 0xfe, 0   ,    2, "Hurricane Kick"			},
	{0x01, 0x01, 0x08, 0x00, "Easy"						},
	{0x01, 0x01, 0x08, 0x08, "Normal"					},

	{0   , 0xfe, 0   ,    4, "Timer"					},
	{0x01, 0x01, 0x30, 0x00, "Very Fast"				},
	{0x01, 0x01, 0x30, 0x10, "Fast"						},
	{0x01, 0x01, 0x30, 0x30, "Normal"					},
	{0x01, 0x01, 0x30, 0x20, "Slow"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0xc0, 0xc0, "1"						},
	{0x01, 0x01, 0xc0, 0x80, "2"						},
	{0x01, 0x01, 0xc0, 0x40, "3"						},
	{0x01, 0x01, 0xc0, 0x00, "4"						},
};

STDDIPINFO(Ddragon2)

static struct BurnDIPInfo DdungeonDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0x00, NULL						},
	{0x01, 0xff, 0xff, 0x9d, NULL						},

	{0	 , 0xfe, 0	 ,   16, "Coin A"					},
	{0x00, 0x01, 0x0f, 0x03, "4 Coins 1 Credit" 		},
	{0x00, 0x01, 0x0f, 0x02, "3 Coins 1 Credit" 		},
	{0x00, 0x01, 0x0f, 0x07, "4 Coins 2 Credits"		},
	{0x00, 0x01, 0x0f, 0x01, "2 Coins 1 Credit" 		},
	{0x00, 0x01, 0x0f, 0x06, "3 Coins 2 Credits"		},
	{0x00, 0x01, 0x0f, 0x0b, "4 Coins 3 Credits"		},
	{0x00, 0x01, 0x0f, 0x0f, "4 Coins 4 Credits"		},
	{0x00, 0x01, 0x0f, 0x0a, "3 Coins 3 Credits"		},
	{0x00, 0x01, 0x0f, 0x05, "2 Coins 2 Credits"		},
	{0x00, 0x01, 0x0f, 0x00, "1 Coin  1 Credit" 		},
	{0x00, 0x01, 0x0f, 0x0e, "3 Coins 4 Credits"		},
	{0x00, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x0f, 0x0d, "2 Coins 4 Credits"		},
	{0x00, 0x01, 0x0f, 0x04, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x0f, 0x08, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},

	{0	 , 0xfe, 0	 ,   16, "Coin B"					},
	{0x00, 0x01, 0xf0, 0x30, "4 Coins 1 Credit" 		},
	{0x00, 0x01, 0xf0, 0x20, "3 Coins 1 Credit" 		},
	{0x00, 0x01, 0xf0, 0x70, "4 Coins 2 Credits"		},
	{0x00, 0x01, 0xf0, 0x10, "2 Coins 1 Credit" 		},
	{0x00, 0x01, 0xf0, 0x60, "3 Coins 2 Credits"		},
	{0x00, 0x01, 0xf0, 0xb0, "4 Coins 3 Credits"		},
	{0x00, 0x01, 0xf0, 0xf0, "4 Coins 4 Credits"		},
	{0x00, 0x01, 0xf0, 0xa0, "3 Coins 3 Credits"		},
	{0x00, 0x01, 0xf0, 0x50, "2 Coins 2 Credits"		},
	{0x00, 0x01, 0xf0, 0x00, "1 Coin  1 Credit" 		},
	{0x00, 0x01, 0xf0, 0xe0, "3 Coins 4 Credits"		},
	{0x00, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0xf0, 0xd0, "2 Coins 4 Credits"		},
	{0x00, 0x01, 0xf0, 0x40, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},

	{0	 , 0xfe, 0	 ,    4, "Lives"					},
	{0x01, 0x01, 0x03, 0x00, "1"						},
	{0x01, 0x01, 0x03, 0x01, "2"						},
	{0x01, 0x01, 0x03, 0x02, "3"						},
	{0x01, 0x01, 0x03, 0x03, "4"						},

	{0	 , 0xfe, 0	 ,    4, "Difficulty"				},
	{0x01, 0x01, 0xf0, 0xf0, "Easy" 					},
	{0x01, 0x01, 0xf0, 0x90, "Medium"					},
	{0x01, 0x01, 0xf0, 0x70, "Hard" 					},
	{0x01, 0x01, 0xf0, 0x00, "Hardest"					},
};

STDDIPINFO(Ddungeon)

static struct BurnDIPInfo DarktowrDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0x00, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0	 , 0xfe, 0	 ,   16, "Coin A"					},
	{0x00, 0x01, 0x0f, 0x03, "4 Coins 1 Credit" 		},
	{0x00, 0x01, 0x0f, 0x02, "3 Coins 1 Credit" 		},
	{0x00, 0x01, 0x0f, 0x07, "4 Coins 2 Credits"		},
	{0x00, 0x01, 0x0f, 0x01, "2 Coins 1 Credit" 		},
	{0x00, 0x01, 0x0f, 0x06, "3 Coins 2 Credits"		},
	{0x00, 0x01, 0x0f, 0x0b, "4 Coins 3 Credits"		},
	{0x00, 0x01, 0x0f, 0x0f, "4 Coins 4 Credits"		},
	{0x00, 0x01, 0x0f, 0x0a, "3 Coins 3 Credits"		},
	{0x00, 0x01, 0x0f, 0x05, "2 Coins 2 Credits"		},
	{0x00, 0x01, 0x0f, 0x00, "1 Coin  1 Credit" 		},
	{0x00, 0x01, 0x0f, 0x0e, "3 Coins 4 Credits"		},
	{0x00, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0x0f, 0x0d, "2 Coins 4 Credits"		},
	{0x00, 0x01, 0x0f, 0x04, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x0f, 0x08, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},

	{0	 , 0xfe, 0	 ,   16, "Coin B"					},
	{0x00, 0x01, 0xf0, 0x30, "4 Coins 1 Credit" 		},
	{0x00, 0x01, 0xf0, 0x20, "3 Coins 1 Credit" 		},
	{0x00, 0x01, 0xf0, 0x70, "4 Coins 2 Credits"		},
	{0x00, 0x01, 0xf0, 0x10, "2 Coins 1 Credit" 		},
	{0x00, 0x01, 0xf0, 0x60, "3 Coins 2 Credits"		},
	{0x00, 0x01, 0xf0, 0xb0, "4 Coins 3 Credits"		},
	{0x00, 0x01, 0xf0, 0xf0, "4 Coins 4 Credits"		},
	{0x00, 0x01, 0xf0, 0xa0, "3 Coins 3 Credits"		},
	{0x00, 0x01, 0xf0, 0x50, "2 Coins 2 Credits"		},
	{0x00, 0x01, 0xf0, 0x00, "1 Coin  1 Credit" 		},
	{0x00, 0x01, 0xf0, 0xe0, "3 Coins 4 Credits"		},
	{0x00, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x00, 0x01, 0xf0, 0xd0, "2 Coins 4 Credits"		},
	{0x00, 0x01, 0xf0, 0x40, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
};

STDDIPINFO(Darktowr)

static struct BurnDIPInfo ToffyDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0x00, NULL						},
	{0x01, 0xff, 0xff, 0x89, NULL						},

	{0   , 0xfe, 0   ,   16, "Coin A"					},
	{0x00, 0x01, 0x0f, 0x03, "4 Coins 1 Credit"			},
	{0x00, 0x01, 0x0f, 0x02, "3 Coins 1 Credit"			},
	{0x00, 0x01, 0x0f, 0x07, "4 Coins 2 Credits"		},
	{0x00, 0x01, 0x0f, 0x01, "2 Coins 1 Credit"			},
	{0x00, 0x01, 0x0f, 0x06, "3 Coins 2 Credits"		},
	{0x00, 0x01, 0x0f, 0x05, "2 Coins 2 Credits"		},
	{0x00, 0x01, 0x0f, 0x00, "1 Coin  1 Credit"			},
	{0x00, 0x01, 0x0f, 0x0b, "4 Coins 5 Credits"		},
	{0x00, 0x01, 0x0f, 0x0f, "4 Coin/6 Credits"			},
	{0x00, 0x01, 0x0f, 0x0a, "3 Coin/5 Credits"			},
	{0x00, 0x01, 0x0f, 0x04, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x0f, 0x0e, "3 Coin/6 Credits"			},
	{0x00, 0x01, 0x0f, 0x09, "2 Coins 5 Credits"		},
	{0x00, 0x01, 0x0f, 0x0d, "2 Coins 6 Credits"		},
	{0x00, 0x01, 0x0f, 0x08, "1 Coin  5 Credits"		},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,   16, "Coin B"					},
	{0x00, 0x01, 0xf0, 0x30, "4 Coins 1 Credit"			},
	{0x00, 0x01, 0xf0, 0x20, "3 Coins 1 Credit"			},
	{0x00, 0x01, 0xf0, 0x70, "4 Coins 2 Credits"		},
	{0x00, 0x01, 0xf0, 0x10, "2 Coins 1 Credit"			},
	{0x00, 0x01, 0xf0, 0x60, "3 Coins 2 Credits"		},
	{0x00, 0x01, 0xf0, 0x50, "2 Coins 2 Credits"		},
	{0x00, 0x01, 0xf0, 0x00, "1 Coin  1 Credit"			},
	{0x00, 0x01, 0xf0, 0xb0, "4 Coins 5 Credits"		},
	{0x00, 0x01, 0xf0, 0xf0, "4 Coin/6 Credits"			},
	{0x00, 0x01, 0xf0, 0xa0, "3 Coin/5 Credits"			},
	{0x00, 0x01, 0xf0, 0x40, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0xf0, 0xe0, "3 Coin/6 Credits"			},
	{0x00, 0x01, 0xf0, 0x90, "2 Coins 5 Credits"		},
	{0x00, 0x01, 0xf0, 0xd0, "2 Coins 6 Credits"		},
	{0x00, 0x01, 0xf0, 0x80, "1 Coin  5 Credits"		},
	{0x00, 0x01, 0xf0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0x03, 0x00, "2"						},
	{0x01, 0x01, 0x03, 0x01, "3"						},
	{0x01, 0x01, 0x03, 0x02, "4"						},
	{0x01, 0x01, 0x03, 0x03, "5"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x01, 0x01, 0x18, 0x10, "30k, 50k and 100k"		},
	{0x01, 0x01, 0x18, 0x08, "50k and 100k"				},
	{0x01, 0x01, 0x18, 0x18, "100k and 200k"			},
	{0x01, 0x01, 0x18, 0x00, "None"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0xc0, 0xc0, "Easy"						},
	{0x01, 0x01, 0xc0, 0x80, "Normal"					},
	{0x01, 0x01, 0xc0, 0x40, "Hard"						},
	{0x01, 0x01, 0xc0, 0x00, "Hardest"					},
};

STDDIPINFO(Toffy)

static struct BurnDIPInfo TstrikeDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x00, 0x01, 0x07, 0x00, "4 Coins 1 Credit"			},
	{0x00, 0x01, 0x07, 0x01, "3 Coins 1 Credit"			},
	{0x00, 0x01, 0x07, 0x02, "2 Coins 1 Credit"			},
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Credit"			},
	{0x00, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x00, 0x01, 0x07, 0x03, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x00, 0x01, 0x38, 0x00, "4 Coins 1 Credit"			},
	{0x00, 0x01, 0x38, 0x08, "3 Coins 1 Credit"			},
	{0x00, 0x01, 0x38, 0x10, "2 Coins 1 Credit"			},
	{0x00, 0x01, 0x38, 0x38, "1 Coin  1 Credit"			},
	{0x00, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x00, 0x01, 0x38, 0x18, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x40, 0x40, "Upright"					},
	{0x00, 0x01, 0x40, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x00, 0x01, 0x80, 0x80, "Off"						},
	{0x00, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x01, 0x01, 0x01, 0x01, "Off"						},
	{0x01, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x01, 0x01, 0x02, 0x02, "Off"						},
	{0x01, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0x0c, 0x0c, "Easy"						},
	{0x01, 0x01, 0x0c, 0x08, "Normal"					},
	{0x01, 0x01, 0x0c, 0x04, "Hard"						},
	{0x01, 0x01, 0x0c, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0x30, 0x30, "1"						},
	{0x01, 0x01, 0x30, 0x20, "2"						},
	{0x01, 0x01, 0x30, 0x10, "3"						},
	{0x01, 0x01, 0x30, 0x00, "4"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x01, 0x01, 0xc0, 0xc0, "100k and 200k"			},
	{0x01, 0x01, 0xc0, 0x80, "200k and 300k"			},
	{0x01, 0x01, 0xc0, 0x40, "300k and 400k"			},
	{0x01, 0x01, 0xc0, 0x00, "400k and 500k"			},
};

STDDIPINFO(Tstrike)

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvHD6309Rom           = Next; Next += 0x30000;
	DrvSubCPURom           = Next; Next += 0x10000;
	DrvSoundCPURom         = Next; Next += 0x10000;
	DrvMCURom              = Next; Next += 0x00800;

	DrvMSM5205Rom          = Next;
	MSM6295ROM             = Next; Next += 0x40000;

	RamStart               = Next;

	DrvHD6309Ram           = Next; Next += 0x01800;
	DrvSubCPURam           = Next; Next += 0x01000;
	DrvSoundCPURam         = Next; Next += 0x01000;
	DrvMCURam              = Next; Next += 0x00080; // dtower
	DrvShareRam            = Next; Next += 0x00400;
	DrvFgVideoRam          = Next; Next += 0x00800;
	DrvBgVideoRam          = Next; Next += 0x00800;
	DrvPaletteRam          = Next; Next += 0x00400;
	DrvSpriteRam           = Next; Next += 0x00800;

	RamEnd                 = Next;

	DrvChars               = Next; Next += 0x1000 * 8 * 8;
	DrvTiles               = Next; Next += 0x1000 * 16 * 16;
	DrvSprites             = Next; Next += 0x2000 * 16 * 16;
	DrvPalette             = (UINT32*)Next; Next += 0x00180 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

// sub(sprite) cpu could be one of 4 cpu-types
// for ease - we'll access all of them via the cpu registry in the cheat engine.
static cheat_core *cheat_ptr = NULL;
static cpu_core_config *cheat_subptr = NULL;

static void subInit()
{
	cheat_ptr = GetCpuCheatRegister(1); // second cpu is always subcpu!
	cheat_subptr = cheat_ptr->cpuconfig;
}

static INT32 subTotalCycles()
{
	return cheat_subptr->totalcycles();
}

static INT32 subRun(INT32 cycles)
{
	return cheat_subptr->run(cycles);
}

static void subNewFrame()
{
	cheat_subptr->newframe();
}

static void subOpen()
{
	cheat_subptr->open(cheat_ptr->nCPU);
}

static void subClose()
{
	cheat_subptr->close();
}

static void subEndRun()
{
	cheat_subptr->runend();
}

static void subIRQ(INT32 vector, INT32 status)
{
	cheat_subptr->irq(cheat_ptr->nCPU, vector, status);
}

static void subReset()
{
	cheat_subptr->reset();
}

static void subExit()
{
	if (cheat_subptr) cheat_subptr->exit();

	cheat_ptr = NULL;
	cheat_subptr = NULL;
}

static INT32 subScan(INT32 nAction)
{
	return cheat_subptr->scan(nAction);
}

static INT32 subIdle(INT32 cycles)
{
	return cheat_subptr->idle(cycles);
}

static void sync_sub()
{
	INT32 nCyclesTotal[4] = {
		(INT32)((double)nCycles[0] * nBurnCPUSpeedAdjust / 0x0100),
		(INT32)((double)nCycles[1] * nBurnCPUSpeedAdjust / 0x0100),
		nCycles[2], nCycles[3]
	};

	subOpen();

	INT32 cyc = HD6309TotalCycles() * nCyclesTotal[1] / nCyclesTotal[0];
	cyc -= subTotalCycles();

	if (cyc > 0) {
		if (sub_disable) {
			subIdle(cyc);
		} else {
			subRun(cyc);
		}
	}

	subClose();
}

static INT32 DrvDoReset()
{
	HD6309Reset(0);

	subOpen();
	subReset();
	subClose();

	if (soundcpu_type == CPU_M6809) {
		M6809Reset(0);
		MSM5205Reset();
	}

	if (soundcpu_type == CPU_Z80) {
		ZetReset(1);
		MSM6295Reset(0);
	}

	if (is_game == DARKTOWER || is_game == TSTRIKE) {
		m68705Reset();
	}

	BurnYM2151Reset();

	main_bank = 0;
	vblank = 0;
	soundlatch = 0;
	scrollx = 0;
	scrolly = 0;

	adpcm_idle[0] = 1;
	adpcm_idle[1] = 1;
	adpcm_pos[0] = 0;
	adpcm_pos[1] = 0;
	adpcm_end[0] = 0;
	adpcm_end[1] = 0;
	adpcm_data[0] = -1;
	adpcm_data[1] = -1;

	sub_disable = 0;
	sub_last = 0;
	main_last = 0;

	DrvMCURam[0] = 0xff;
	DrvMCURam[1] = 0xff;
	DrvMCURam[2] = 0xff;
	DrvMCURam[3] = 0xff;

	nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = nExtraCycles[3] = 0;

	HiscoreReset();

	return 0;
}

static void main_write(UINT16 address, UINT8 data); // forward

static UINT8 main_read(UINT16 address)
{
	if (address >= 0x3810 && address <= 0x3bff) {
		return 0; // ddragon2 unmapped reads (lots of them when playing)
	}

	if (address >= 0x2000 && address <= 0x27ff) {
		return (sub_disable) ? DrvShareRam[address & 0x1ff] : 0xff;
	}

	if (address >= 0x2800 && address <= 0x2fff) {
		return DrvSpriteRam[address & 0x7ff];
	}

	if ((is_game == DARKTOWER || is_game == TSTRIKE) && address >= 0x4000 && address <= 0x7fff) {
		UINT32 Offset = address & 0x3fff;

		if (is_game == TSTRIKE) {
			switch (HD6309GetPC(-1)) {
				case 0x9ace:
					return 0;
				case 0x9ae4:
					return 0x63;
				default:
					return DrvHD6309Ram[0xbe1];
			}
		}

		return (Offset == 0x1401 || Offset == 0x0001) ? DrvMCURam[0] : 0xff;
	}

	switch (address) {
		case 0x3800:
			return DrvInputs[0];

		case 0x3801:
			return DrvInputs[1];

		case 0x3802:
			return (DrvInputs[2] & ~0x18) | (vblank ? 0x08 : 0) | (sub_disable ? 0 : 0x10);

		case 0x3803:
			return DrvDips[0];

		case 0x3804:
			return DrvDips[1];

		case 0x3807:
		case 0x3808:
		case 0x3809:
		case 0x380a:
			return 0; // nop

		case 0x380b:
			HD6309SetIRQLine(HD6309_INPUT_LINE_NMI, CPU_IRQSTATUS_NONE);
			return 0xff;

		case 0x380c:
			HD6309SetIRQLine(HD6309_FIRQ_LINE, CPU_IRQSTATUS_NONE);
			return 0xff;

		case 0x380d:
			HD6309SetIRQLine(HD6309_IRQ_LINE, CPU_IRQSTATUS_NONE);
			return 0xff;

		case 0x380e:
			main_write(0x380e, 0xff);
			return 0xff;

		case 0x380f:
			subIRQ(CPU_IRQLINE_NMI, CPU_IRQSTATUS_ACK);
			return 0xff;
	}

	return 0;
}

static void bankswitch(UINT8 data)
{
	main_bank = data;

	if (is_game == TOFFY) {
		data &= 0x20;
	}

	UINT8 bank = (data & 0xe0) >> 5;

	HD6309MapMemory(DrvHD6309Rom + 0x8000 + (bank * 0x4000), 0x4000, 0x7fff, MAP_ROM);

	if ((is_game == DARKTOWER || is_game == TSTRIKE) && bank == 4) {
		HD6309MemCallback(0x4000, 0x7fff, MAP_RAM);
	}
}

static void main_write(UINT16 address, UINT8 data)
{
	if (address >= 0x2000 && address <= 0x27ff) {
		if (!sub_disable) return;
		DrvShareRam[address & 0x1ff] = data;
		return;
	}
	if (address >= 0x2800 && address <= 0x2fff) {
		DrvSpriteRam[address & 0x7ff] = data;
		return;
	}
	if ((is_game == DARKTOWER || is_game == TSTRIKE) && address >= 0x4000 && address <= 0x7fff) {
		UINT32 Offset = address & 0x3fff;

		if (Offset == 0x1400 || Offset == 0x0000) {
			DrvMCURam[1] = BITSWAP08(data, 0, 1, 2, 3, 4, 5, 6, 7);
		}
		return;
	}

	switch (address) {
		case 0x3808:
			sync_sub();

			bankswitch(data);

			if (data & 0x08 && ~main_last & 0x08) { // Reset on rising edge
				subOpen();
				subReset();
				subClose();
			}

			scrollx = (scrollx & 0xff) | ((data & 0x01) << 8);
			scrolly = (scrolly & 0xff) | ((data & 0x02) << 7);

			sub_disable = (is_game == TOFFY) ? 0x18 : (~data & 0x08) | (data & 0x10); // 0x08(low) reset, 0x10(high) halted

			main_last = data;
			return;

		case 0x3809:
			scrollx = (scrollx & 0x100) | (data & 0xff);
			return;

		case 0x380a:
			scrolly = (scrolly & 0x100) | (data & 0xff);
			return;

		case 0x380b:
			HD6309SetIRQLine(HD6309_INPUT_LINE_NMI, CPU_IRQSTATUS_NONE);
			return;

		case 0x380c:
			HD6309SetIRQLine(HD6309_FIRQ_LINE, CPU_IRQSTATUS_NONE);
			return;

		case 0x380d:
			HD6309SetIRQLine(HD6309_IRQ_LINE, CPU_IRQSTATUS_NONE);
			return;

		case 0x380e:
			soundlatch = data;

			if (soundcpu_type == CPU_M6809) {
				M6809SetIRQLine(0, M6809_IRQ_LINE, CPU_IRQSTATUS_HOLD);
			}

			if (soundcpu_type == CPU_Z80) {
				ZetNmi(1);
			}
			return;

		case 0x380f:
			subIRQ(CPU_IRQLINE_NMI, CPU_IRQSTATUS_ACK);
			return;
	}
}

static void sub_write_byte(UINT16 address, UINT8 data)
{
	if (address <= 0x001f) {
		if (subcpu_type == CPU_M6803) {
			m6803_internal_registers_w(address, data);
		} else {
			if (address == 0x17) {
				if (~data & 1) {
					subIRQ(HD63701_INPUT_LINE_NMI, CPU_IRQSTATUS_NONE);
				}
				if (data & 2 && ~sub_last & 2) {
					HD6309SetIRQLine(0, HD6309_IRQ_LINE, CPU_IRQSTATUS_ACK);
				}
				sub_last = data;
			}
		}
		return;
	}

	if (address >= 0x0020 && address <= 0x0fff) {
		DrvSubCPURam[address & 0xfff] = data;
		return;
	}

	if (address >= 0x8000 && address <= 0x81ff) {
		DrvShareRam[address & 0x1ff] = data;
		return;
	}

	if (address >= 0xc7fe && address <= 0xc8ff) {
		// unmapped writes (ddragonb)
		return;
	}
}

static UINT8 sub_read_byte(UINT16 address)
{
	if (address >= 0x0020 && address <= 0x0fff) {
		return DrvSubCPURam[address & 0xfff];
	}

	if (address >= 0x8000 && address <= 0x81ff) {
		return DrvShareRam[address & 0x1ff];
	}

	return 0;
}

static void dragonbaM6803_sub_write_port(UINT16 port, UINT8 data)
{
	if (~data & 0x08) {
		M6803SetIRQLine(M6803_INPUT_LINE_NMI, CPU_IRQSTATUS_NONE);
	}

	if (data & 0x10 && ~sub_last & 0x10) {
		HD6309SetIRQLine(0, HD6309_IRQ_LINE, CPU_IRQSTATUS_ACK);
	}

	sub_last = data;
}

static void __fastcall dd2_sub_write(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0xd000:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			return;

		case 0xe000:
			HD6309SetIRQLine(0, HD6309_IRQ_LINE, CPU_IRQSTATUS_ACK);
			subEndRun();
			return;
	}
}

static UINT8 sound_read(UINT16 address)
{
	switch (address) {
		case 0x1000:
			return soundlatch;

		case 0x1800:
			return adpcm_idle[0] + (adpcm_idle[1] << 1);

		case 0x2801:
			return BurnYM2151Read();
	}

	return 0;
}

static void sound_write(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0x2800:
		case 0x2801:
			BurnYM2151Write(address & 1, data);
			return;

		case 0x3800:
		case 0x3801:
			adpcm_idle[address & 1] = 0;
			MSM5205ResetWrite(address & 1, 0);
			return;

		case 0x3802:
		case 0x3803:
			adpcm_end[address & 1] = (data & 0x7f) * 0x200;
			return;

		case 0x3804:
		case 0x3805:
			adpcm_pos[address & 1] = (data & 0x7f) * 0x200;
			return;

		case 0x3806:
		case 0x3807:
			adpcm_idle[address & 1] = 1;
			MSM5205ResetWrite(address & 1, 1);
			return;
	}
}

static UINT8 __fastcall dd2_sound_read(UINT16 address)
{
	switch (address) {
		case 0x8801:
			return BurnYM2151Read();

		case 0x9800:
			return MSM6295Read(0);

		case 0xa000:
			return soundlatch;
	}

	return 0;
}

static void __fastcall dd2_sound_write(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0x8800:
		case 0x8801:
			BurnYM2151Write(address & 1, data);
			return;

		case 0x9800:
			MSM6295Write(0, data);
			return;
	}
}

static void DrvYM2151IrqHandler(INT32 Irq)
{
	M6809SetIRQLine(M6809_FIRQ_LINE, (Irq) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void Ddragon2YM2151IrqHandler(INT32 Irq)
{
	ZetSetIRQLine(0, (Irq) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)M6809TotalCycles() * nSoundRate / 1500000);
}

static void msm5205_clock(INT32 chip)
{
	if (adpcm_pos[chip] >= adpcm_end[chip] || adpcm_pos[chip] >= snd_len[chip]) {
		adpcm_idle[chip] = 1;
		MSM5205ResetWrite(chip, 1);
	} else {
		if (adpcm_data[chip] != -1) {
			MSM5205DataWrite(chip, adpcm_data[chip] & 0x0f);
			adpcm_data[chip] = -1;
		} else {
			UINT8 *ROM = DrvMSM5205Rom + chip * 0x10000;

			adpcm_data[chip] = ROM[(adpcm_pos[chip]++) & 0xffff];
			MSM5205DataWrite(chip, adpcm_data[chip] >> 4);
		}
	}
}

static void DrvMSM5205Vck0()
{
	msm5205_clock(0);
}

static void DrvMSM5205Vck1()
{
	msm5205_clock(1);
}

static tilemap_scan( bg )
{
	return (col & 0x0f) + ((row << 4) & 0xf0) + ((col << 4) & 0x100) + ((row << 5) & 0x200);
}

static tilemap_callback( bg )
{
	INT32 Attr = DrvBgVideoRam[offs * 2 + 0];
	INT32 Code = DrvBgVideoRam[offs * 2 + 1] + ((Attr & 0x07) << 8);

	TILE_SET_INFO(0, Code, Attr >> 3, TILE_FLIPYX((Attr & 0xc0) >> 6));
}

static tilemap_callback( fg )
{
	INT32 Attr = DrvFgVideoRam[offs * 2 + 0];
	INT32 Code = DrvFgVideoRam[offs * 2 + 1] + ((Attr & 0x07) << 8);

	TILE_SET_INFO(1, Code, Attr >> 5, 0);
}

static void tmap_init()
{
	GenericTilemapInit(0, bg_map_scan,		 bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -8);

	GenericTilemapSetGfx(0, DrvTiles, 4, 16, 16, gfx_count[2] * 16 * 16, 0x100, 0x07);
	GenericTilemapSetGfx(1, DrvChars, 4,  8,  8, gfx_count[0] *  8 *  8, 0x000, 0x07);
}

static INT32 RomLoader(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;

	BurnAllocMemIndex();

	UINT8 *pLoad[8] = { DrvHD6309Rom, DrvSubCPURom, DrvSoundCPURom, DrvMCURom };
	INT32 pLen[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	UINT8 *gLoad[8] = { DrvChars, DrvSprites, DrvTiles };
	INT32 gLen[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	UINT8 *sLoad[8] = { DrvMSM5205Rom, DrvMSM5205Rom + 0x10000}; // 6295 @ same (first) location!
	INT32 sLen[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	snd_len[0] = snd_len[1] = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		INT32 nType = ri.nType & 7;
		INT32 nTypeZB = (ri.nType & 7) - 1;

		if ((ri.nType & BRF_PRG) && (nType >= 1) && (nType <= 4))
		{
			if (bLoad) {
				//bprintf (0, _T("PRG%d: %5.5x, %d\n"), nTypeZB, pLen[nTypeZB], i);
				if (BurnLoadRom(pLoad[nTypeZB], i, 1)) return 1;
			}
			pLoad[nTypeZB] += ri.nLen;
			pLen[nTypeZB] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (nType >= 1) && (nType <= 3))
		{
			if (bLoad) {
				//bprintf (0, _T("gfx%d: %5.5x, %d\n"), nTypeZB, gLen[nTypeZB], i);
				if (BurnLoadRom(gLoad[nTypeZB], i, 1)) return 1;
			}
			gLoad[nTypeZB] += ri.nLen;
			gLen[nTypeZB] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_SND) && (nType == 1 || nType == 2))
		{
			if (bLoad) {
				//bprintf (0, _T("sound %d: %5.5x, %d\n"), nTypeZB, sLen[nTypeZB], i);
				if (BurnLoadRom(sLoad[nTypeZB], i, 1)) return 1;
			}
			sLoad[nTypeZB] += ri.nLen;
			sLen[nTypeZB] += ri.nLen;
			snd_len[nTypeZB] = ri.nLen;
			continue;
		}
	}

	if (bLoad) { // decode gfx
		//bprintf(0, _T("snd roms: %x  %x\n"), snd_len[0], snd_len[1]);
		//bprintf(0, _T("gLen  %x  %x  %x\n"), gLen[0], gLen[1], gLen[2]);

		INT32 CharPlaneOffsets[4]		= { 0, 2, 4, 6 };
		INT32 CharXOffsets[8]			= { 1, 0, 65, 64, 129, 128, 193, 192 };
		INT32 CharYOffsets[8]			= { 0, 8, 16, 24, 32, 40, 48, 56 };
		INT32 SpritePlaneOffsets[4]		= { RGN_FRAC(gLen[1], 1, 2), RGN_FRAC(gLen[1], 1, 2) + 4, 0, 4 };
		INT32 TilePlaneOffsets[4]		= { RGN_FRAC(gLen[2], 1, 2), RGN_FRAC(gLen[2], 1, 2) + 4, 0, 4 };
		INT32 TileXOffsets[16]			= { 3, 2, 1, 0, 131, 130, 129, 128, 259, 258, 257, 256, 387, 386, 385, 384 };
		INT32 TileYOffsets[16]			= { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120 };

		UINT8 *temp = (UINT8 *)BurnMalloc(0x100000);

		switch (is_game) {
			case TOFFY:
				for (INT32 i = 0; i < 0x10000; i++)
					DrvHD6309Rom[i] = BITSWAP08(DrvHD6309Rom[i], 6,7,5,4,3,2,1,0);

				// rom is swapped compared to dd/dd2
				BurnSwapMemBlock(DrvHD6309Rom + 0x0000, DrvHD6309Rom + 0x8000, 0x8000);

				for (int i = 0; i < 0x10000; i++)
					DrvChars[i] = BITSWAP08(DrvChars[i], 7,6,5,3,4,2,1,0);

				for (int i = 0; i < 0x20000; i++)
					DrvSprites[i] = BITSWAP08(DrvSprites[i], 7,6,5,4,3,2,0,1);

				for (int i = 0; i < 0x10000; i++) {
					DrvTiles[i + 0*0x10000] = BITSWAP08(DrvTiles[i + 0*0x10000], 7,6,1,4,3,2,5,0);
					DrvTiles[i + 1*0x10000] = BITSWAP08(DrvTiles[i + 1*0x10000], 7,6,2,4,3,5,1,0);
				}
				// no break! (fallthrough)
			case DDRAGON:
			case DDRAGON2:
			case DARKTOWER:
			case TSTRIKE:
			default:
				gfx_count[0] = ((gLen[0] * 8) / 4) / ( 8* 8);
				gfx_count[1] = ((gLen[1] * 8) / 4) / (16*16);
				gfx_count[2] = ((gLen[2] * 8) / 4) / (16*16);
				//bprintf(0, _T("decoding: %x  %x  %x  (chars, sprites, tiles)\n"), gfx_count[0], gfx_count[1], gfx_count[2]);

				memcpy(temp, DrvChars, gLen[0]);
				GfxDecode(gfx_count[0], 4,  8,  8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x100, temp, DrvChars);

				memcpy(temp, DrvSprites, gLen[1]);
				GfxDecode(gfx_count[1], 4, 16, 16, SpritePlaneOffsets, TileXOffsets, TileYOffsets, 0x200, temp, DrvSprites);

				memcpy(temp, DrvTiles, gLen[2]);
				GfxDecode(gfx_count[2], 4, 16, 16, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x200, temp, DrvTiles);
				break;
		}

		BurnFree(temp);
	}

	return 0;
}

static INT32 DdragonCommonInit()
{
	BurnSetRefreshRate(57.444853);

	HD6309Init(0);
	HD6309Open(0);
	HD6309MapMemory(DrvHD6309Ram,	0x0000, 0x0fff, MAP_RAM);
	HD6309MapMemory(DrvPaletteRam,	0x1000, 0x13ff, MAP_RAM);
	HD6309MapMemory(DrvFgVideoRam,	0x1800, 0x1fff, MAP_RAM);
	//HD6309MapMemory(DrvSpriteRam,	0x2000, 0x2fff, MAP_WRITE); // handler
	HD6309MapMemory(DrvBgVideoRam,	0x3000, 0x37ff, MAP_RAM);
	HD6309MapMemory(DrvHD6309Rom + 0x8000,  0x4000, 0x7fff, MAP_ROM);
	HD6309MapMemory(DrvHD6309Rom,	0x8000, 0xffff, MAP_ROM);
	HD6309SetReadHandler(main_read);
	HD6309SetWriteHandler(main_write);
	HD6309Close();

	if (subcpu_type == CPU_HD63701) {
		HD63701Init(0);
		HD63701Open(0);
		HD63701MapMemory(DrvSubCPURom, 0xc000, 0xffff, MAP_ROM);
		HD63701SetReadHandler(sub_read_byte);
		HD63701SetWriteHandler(sub_write_byte);
		HD63701Close();
	}

	if (subcpu_type == CPU_HD6309) {
		HD6309Init(1);
		HD6309Open(1);
		HD6309MapMemory(DrvSubCPURom, 0xc000, 0xffff, MAP_ROM);
		HD6309SetReadHandler(sub_read_byte);
		HD6309SetWriteHandler(sub_write_byte);
		HD6309Close();
	}

	if (subcpu_type == CPU_M6803) {
		M6803Init(0);
		M6803Open(0);
		M6803MapMemory(DrvSubCPURom, 0xc000, 0xffff, MAP_ROM);
		M6803SetReadHandler(sub_read_byte);
		M6803SetWriteHandler(sub_write_byte);
		M6803SetWritePortHandler(dragonbaM6803_sub_write_port);
		M6803Close();
	}

	subInit();

	if (soundcpu_type == CPU_M6809) {
		M6809Init(0);
		M6809Open(0);
		M6809MapMemory(DrvSoundCPURam, 0x0000, 0x0fff, MAP_RAM);
		M6809MapMemory(DrvSoundCPURom, 0x8000, 0xffff, MAP_ROM);
		M6809SetReadHandler(sound_read);
		M6809SetWriteHandler(sound_write);
		M6809Close();

		BurnYM2151InitBuffered(3579545, 1, NULL, 0);
		BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
		BurnYM2151SetAllRoutes(0.60, BURN_SND_ROUTE_BOTH);
		BurnTimerAttachM6809(1500000);

		MSM5205Init(0, DrvSynchroniseStream, 375000, DrvMSM5205Vck0, MSM5205_S48_4B, 1);
		MSM5205Init(1, DrvSynchroniseStream, 375000, DrvMSM5205Vck1, MSM5205_S48_4B, 1);
		MSM5205SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
		MSM5205SetRoute(1, 0.50, BURN_SND_ROUTE_BOTH);
	}

	if (is_game == DARKTOWER || is_game == TSTRIKE) {
		m6805Init(1, 0x800);
		m6805MapMemory(DrvMCURom + 0x80,	0x080,	0x7ff, MAP_ROM);
		m6805MapMemory(DrvMCURam,			0x000,	0x07f, MAP_RAM);
	}

	nCycles[0] = (INT32)((double)3000000 / 57.44853 + 0.5);
	nCycles[1] = (INT32)((double)1500000 / 57.44853 + 0.5);
	nCycles[2] = (INT32)((double)1500000 / 57.44853 + 0.5);
	nCycles[3] = (INT32)((double)4000000 / 57.44853 + 0.5); // darktower m68705 mcu

	GenericTilesInit();

	tmap_init();

	DrvDoReset();

	return 0;
}

static INT32 Ddragon2Init()
{
	subcpu_type = CPU_Z80;
	soundcpu_type = CPU_Z80;
	is_game = DDRAGON2;

	if (RomLoader(1)) return 1;

	BurnSetRefreshRate(57.444853);

	HD6309Init(0);
	HD6309Open(0);
	HD6309MapMemory(DrvHD6309Ram,	0x0000, 0x17ff, MAP_RAM);
	HD6309MapMemory(DrvFgVideoRam,	0x1800, 0x1fff, MAP_RAM);
	//HD6309MapMemory(DrvSpriteRam,	0x2000, 0x2fff, MAP_WRITE); // handler
	HD6309MapMemory(DrvBgVideoRam,	0x3000, 0x37ff, MAP_RAM);
	HD6309MapMemory(DrvPaletteRam,	0x3c00, 0x3fff, MAP_RAM);
	HD6309MapMemory(DrvHD6309Rom + 0x8000,  0x4000, 0x7fff, MAP_ROM);
	HD6309MapMemory(DrvHD6309Rom,	0x8000, 0xffff, MAP_ROM);
	HD6309SetReadHandler(main_read);
	HD6309SetWriteHandler(main_write);
	HD6309Close();

	ZetInit(0);
	ZetOpen(0);
	ZetSetWriteHandler(dd2_sub_write);
	ZetMapMemory(DrvSubCPURom,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvShareRam, 		0xc000, 0xc3ff, MAP_RAM);
	ZetClose();

	subInit();

	ZetInit(1);
	ZetOpen(1);
	ZetSetReadHandler(dd2_sound_read);
	ZetSetWriteHandler(dd2_sound_write);
	ZetMapMemory(DrvSoundCPURom,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSoundCPURam,	0x8000, 0x87ff, MAP_RAM);
	ZetClose();

	BurnYM2151InitBuffered(3579545, 1, NULL, 0);
	BurnYM2151SetIrqHandler(&Ddragon2YM2151IrqHandler);
	BurnYM2151SetAllRoutes(1.40, BURN_SND_ROUTE_BOTH);
	BurnTimerAttachZet(3579545);

	MSM6295Init(0, 1056000 / 132, 1);
	MSM6295SetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);

	nCycles[0] = (INT32)((double)3000000 / 57.44853 + 0.5);
	nCycles[1] = (INT32)((double)4000000 / 57.44853 + 0.5);
	nCycles[2] = (INT32)((double)3579545 / 57.44853 + 0.5);

	GenericTilesInit();

	tmap_init();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	HD6309Exit();
	subExit(); // incl. second Z80 w/ DD2 hw
	if (soundcpu_type == CPU_M6809) M6809Exit();
	if (is_game == DARKTOWER || is_game == TSTRIKE) m6805Exit();

	BurnYM2151Exit();
	if (soundcpu_type == CPU_Z80) {
		MSM6295Exit(0);
	} else {
		MSM5205Exit();
	}

	GenericTilesExit();

	BurnFreeMemIndex();

	subcpu_type = CPU_NONE;
	soundcpu_type = CPU_NONE;
	is_game = 0;

	return 0;
}

static void DrvCalcPalette()
{
	for (INT32 i = 0; i < 0x180; i++) {
		INT32 data = DrvPaletteRam[i] + (DrvPaletteRam[0x200 + i] << 8);

		UINT8 r = pal4bit(data >> 0);
		UINT8 g = pal4bit(data >> 4);
		UINT8 b = pal4bit(data >> 8);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x180; i += 5) {
		INT32 attr = DrvSpriteRam[i + 1];

		if (~attr & 0x80) continue;

		INT32 sx = 240 - DrvSpriteRam[i + 4] + ((attr & 2) << 7);
		INT32 sy = 232 - DrvSpriteRam[i + 0] + ((attr & 1) << 8);
		INT32 size = (attr & 0x30) >> 4;
		INT32 flipx = attr & 0x08;
		INT32 flipy = attr & 0x04;
		INT32 dx = -16;
		INT32 dy = -16;

		INT32 color;
		INT32 code;

		if (is_game == DDRAGON2) {
			color = (DrvSpriteRam[i + 2] >> 5);
			code = (DrvSpriteRam[i + 3] + ((DrvSpriteRam[i + 2] & 0x1f) << 8)) & ~size;
		} else {
			color = (DrvSpriteRam[i + 2] >> 4) & 0x07;
			code = (DrvSpriteRam[i + 3] + ((DrvSpriteRam[i + 2] & 0x0f) << 8)) & ~size;
		}

		switch (size) {
			case 0:
				Draw16x16MaskTile(pTransDraw, code + 0, sx, sy, flipx, flipy, color, 4, 0, 128, DrvSprites);
				break;

			case 1:
				Draw16x16MaskTile(pTransDraw, code + 0, sx, sy + dy, flipx, flipy, color, 4, 0, 128, DrvSprites);
				Draw16x16MaskTile(pTransDraw, code + 1, sx, sy, flipx, flipy, color, 4, 0, 128, DrvSprites);
				break;

			case 2:
				Draw16x16MaskTile(pTransDraw, code + 0, sx + dx, sy, flipx, flipy, color, 4, 0, 128, DrvSprites);
				Draw16x16MaskTile(pTransDraw, code + 2, sx, sy, flipx, flipy, color, 4, 0, 128, DrvSprites);
				break;

			case 3:
				Draw16x16MaskTile(pTransDraw, code + 0, sx + dx, sy + dy, flipx, flipy, color, 4, 0, 128, DrvSprites);
				Draw16x16MaskTile(pTransDraw, code + 1, sx + dx, sy, flipx, flipy, color, 4, 0, 128, DrvSprites);
				Draw16x16MaskTile(pTransDraw, code + 2, sx, sy + dy, flipx, flipy, color, 4, 0, 128, DrvSprites);
				Draw16x16MaskTile(pTransDraw, code + 3, sx, sy, flipx, flipy, color, 4, 0, 128, DrvSprites);
				break;
		}
	}
}

static void DrvDrawBegin()
{
	if (!pBurnDraw) return;
	BurnTransferClear();

	lastline = 0;
}

static void partial_update()
{
	if (!pBurnDraw) return;

	if (scanline < 0 || scanline > nScreenHeight || scanline == lastline || lastline > scanline) return;

	//bprintf(0, _T("%07d: partial %d - %d.    scrollx %d   scrolly %d\n"), nCurrentFrame, lastline, scanline, scrollx, scrolly);

	DrvCalcPalette();

	GenericTilesSetClip(0, nScreenWidth, lastline, scanline);

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollY(0, scrolly);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) draw_sprites();
	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 0);

	GenericTilesClearClip();

	lastline = scanline;
}

static INT32 DrvDraw()
{
	partial_update();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	HD6309NewFrame();
	subNewFrame();

	const INT32 MULT = 6;
	if (soundcpu_type == CPU_M6809) {
		M6809NewFrame();
		MSM5205NewFrame(0, 1500000, 272*MULT);
	}
	if (is_game == DARKTOWER || is_game == TSTRIKE) m6805NewFrame();

	{
		memset(DrvInputs, 0xff, sizeof(DrvInputs));
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy0[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 272*MULT;
	INT32 nCyclesTotal[4] = {
		(INT32)((double)nCycles[0] * nBurnCPUSpeedAdjust / 0x0100),
		(INT32)((double)nCycles[1] * nBurnCPUSpeedAdjust / 0x0100),
		nCycles[2], nCycles[3]
	};
	INT32 nCyclesDone[4] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2], nExtraCycles[3] };
	INT32 next_firq = 16;

	subOpen();
	subIdle(nExtraCycles[1]); // because _SYNCINT
	subClose();

	vblank = 0;
	DrvDrawBegin();

	for (INT32 i = 0; i < nInterleave; i++) {
		scanline = i / MULT;

		HD6309Open(0);
		if (i == 240*MULT) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}

			vblank = 1;
			HD6309SetIRQLine(HD6309_INPUT_LINE_NMI, CPU_IRQSTATUS_ACK);
		}
		if (i == next_firq*MULT) {
			if (i == 240*MULT) {
				nCyclesDone[0] += HD6309Run(nCyclesTotal[0] / 272 / 2); // run half a line before FIRQ on VBL line
				next_firq += 8; // next: 264
			}
			HD6309SetIRQLine(HD6309_FIRQ_LINE, CPU_IRQSTATUS_ACK);
			next_firq += 16;
		}

		CPU_RUN(0, HD6309);
		HD6309Close();

		subOpen();
		if (sub_disable) {
			CPU_IDLE_SYNCINT(1, sub);
		} else {
			CPU_RUN_SYNCINT(1, sub);
		}
		subClose();

		if (soundcpu_type == CPU_M6809) {
			M6809Open(0);
			CPU_RUN_TIMER(2);
			MSM5205UpdateScanline(i);
			M6809Close();
		}

		if (soundcpu_type == CPU_Z80) {
			ZetOpen(1);
			CPU_RUN_TIMER(2);
			ZetClose();
		}

		if (is_game == DARKTOWER || is_game == TSTRIKE) {
			CPU_RUN_SYNCINT(3, m6805);
		}
		if ((scanline & 7) == 7 && is_game == TSTRIKE) {
			// TSTRIKE multiplexes sprites
		    partial_update();
		}
	}

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);

		if (soundcpu_type == CPU_Z80) {
			MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		}

		if (soundcpu_type == CPU_M6809) {
			MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
			MSM5205Render(1, pBurnSoundOut, nBurnSoundLen);
		}
	}

	subOpen();
	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = subTotalCycles() - nCyclesTotal[1];
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];
	nExtraCycles[3] = nCyclesDone[3] - nCyclesTotal[3];
	subClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin != NULL) {
		*pnMin = 0x029696;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(RamStart, RamEnd - RamStart, "All Ram");
	}

	if (nAction & ACB_DRIVER_DATA) {
		HD6309Scan(nAction);

		subScan(nAction); // incl. second z80 for DD2
		if (soundcpu_type == CPU_M6809) M6809Scan(nAction);
		if (is_game == DARKTOWER || is_game == TSTRIKE) m6805Scan(nAction); // m68705

		BurnYM2151Scan(nAction, pnMin);
		if (soundcpu_type == CPU_Z80) MSM6295Scan(nAction, pnMin);
		if (soundcpu_type == CPU_M6809) MSM5205Scan(nAction, pnMin);

		SCAN_VAR(main_bank);
		SCAN_VAR(main_last);
		SCAN_VAR(sub_disable);
		SCAN_VAR(sub_last);
		SCAN_VAR(soundlatch);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);

		SCAN_VAR(adpcm_idle);
		SCAN_VAR(adpcm_pos);
		SCAN_VAR(adpcm_end);
		SCAN_VAR(adpcm_data);

		SCAN_VAR(nExtraCycles);

		if (nAction & ACB_WRITE) {
			HD6309Open(0);
			bankswitch(main_bank);
			HD6309Close();
		}
	}

	return 0;
}

static struct BurnRomInfo ddragonRomDesc[] = {
	{ "21j-1.26",      	0x08000, 0xae714964, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "21j-2-3.25",    	0x08000, 0x5779705e, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "21a-3.24",      	0x08000, 0xdbf24897, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "21j-4.23",      	0x08000, 0x6c9f46fa, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "21jm-0.ic55",   	0x04000, 0xf5232d03, 2 | BRF_ESS | BRF_PRG }, //  4	HD63701 Program Code

	{ "21j-0-1",       	0x08000, 0x9efa95bb, 3 | BRF_ESS | BRF_PRG }, //  5	M6809 Program Code

	{ "21j-5",         	0x08000, 0x7a8b8db4, 1 | BRF_GRA },	     	  //  6	Characters

	{ "21j-a",         	0x10000, 0x574face3, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "21j-b",         	0x10000, 0x40507a76, 2 | BRF_GRA },	     	  //  8
	{ "21j-c",         	0x10000, 0xbb0bc76f, 2 | BRF_GRA },	     	  //  9
	{ "21j-d",         	0x10000, 0xcb4f231b, 2 | BRF_GRA },	     	  // 10
	{ "21j-e",         	0x10000, 0xa0a0c261, 2 | BRF_GRA },	     	  // 11
	{ "21j-f",         	0x10000, 0x6ba152f6, 2 | BRF_GRA },	     	  // 12
	{ "21j-g",         	0x10000, 0x3220a0b6, 2 | BRF_GRA },	     	  // 13
	{ "21j-h",         	0x10000, 0x65c7517d, 2 | BRF_GRA },	     	  // 14

	{ "21j-8",         	0x10000, 0x7c435887, 3 | BRF_GRA },	     	  // 15	Tiles
	{ "21j-9",         	0x10000, 0xc6640aed, 3 | BRF_GRA },	     	  // 16
	{ "21j-i",         	0x10000, 0x5effb0a0, 3 | BRF_GRA },	     	  // 17
	{ "21j-j",         	0x10000, 0x5fb42e7c, 3 | BRF_GRA },	     	  // 18

	{ "21j-6",         	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 19	Samples
	{ "21j-7",         	0x10000, 0x904de6f8, 2 | BRF_SND },	     	  // 20

	{ "21j-k-0.101",   	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 21	PROMs
	{ "21j-l-0.16",    	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 22
};

STD_ROM_PICK(ddragon)
STD_ROM_FN(ddragon)

static INT32 DdragonInit()
{
	subcpu_type = CPU_HD63701;
	soundcpu_type = CPU_M6809;
	is_game = DDRAGON;

	if (RomLoader(1)) return 1;
	if (DdragonCommonInit()) return 1;

	return 0;
}

struct BurnDriver BurnDrvDdragon = {
	"ddragon", NULL, NULL, NULL, "1987",
	"Double Dragon (World set 1)\0", NULL, "Technos Japan (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragonRomInfo, ddragonRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DdragonDIPInfo,
	DdragonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragonaRomDesc[] = {
	{ "e1-1.26",       	0x08000, 0x4b951643, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "21a-2-4.25",    	0x08000, 0x5cd67657, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "21a-3.24",      	0x08000, 0xdbf24897, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "e4-1.23",       	0x08000, 0xb1e26935, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "21jm-0.ic55",   	0x04000, 0xf5232d03, 2 | BRF_ESS | BRF_PRG }, //  4	HD63701 Program Code

	{ "21j-0-1",       	0x08000, 0x9efa95bb, 3 | BRF_ESS | BRF_PRG }, //  5	M6809 Program Code

	{ "21j-5",         	0x08000, 0x7a8b8db4, 1 | BRF_GRA },	     	  //  6	Characters

	{ "21j-a",         	0x10000, 0x574face3, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "21j-b",         	0x10000, 0x40507a76, 2 | BRF_GRA },	     	  //  8
	{ "21j-c",         	0x10000, 0xbb0bc76f, 2 | BRF_GRA },	     	  //  9
	{ "21j-d",         	0x10000, 0xcb4f231b, 2 | BRF_GRA },	     	  // 10
	{ "21j-e",         	0x10000, 0xa0a0c261, 2 | BRF_GRA },	     	  // 11
	{ "21j-f",         	0x10000, 0x6ba152f6, 2 | BRF_GRA },	     	  // 12
	{ "21j-g",         	0x10000, 0x3220a0b6, 2 | BRF_GRA },	     	  // 13
	{ "21j-h",         	0x10000, 0x65c7517d, 2 | BRF_GRA },	     	  // 14

	{ "21j-8",         	0x10000, 0x7c435887, 3 | BRF_GRA },	     	  // 15	Tiles
	{ "21j-9",         	0x10000, 0xc6640aed, 3 | BRF_GRA },	     	  // 16
	{ "21j-i",         	0x10000, 0x5effb0a0, 3 | BRF_GRA },	     	  // 17
	{ "21j-j",         	0x10000, 0x5fb42e7c, 3 | BRF_GRA },	     	  // 18

	{ "21j-6",         	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 19	Samples
	{ "21j-7",         	0x10000, 0x904de6f8, 2 | BRF_SND },	     	  // 20

	{ "21j-k-0.101",   	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 21	PROMs
	{ "21j-l-0.16",    	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 22
};

STD_ROM_PICK(ddragona)
STD_ROM_FN(ddragona)

struct BurnDriver BurnDrvDdragona = {
	"ddragona", "ddragon", NULL, NULL, "1987",
	"Double Dragon (World set 2)\0", NULL, "Technos Japan (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragonaRomInfo, ddragonaRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DdragonDIPInfo,
	DdragonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragonjRomDesc[] = {
	{ "21j-1-5.26",    	0x08000, 0x42045dfd, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "21j-2-3.25",    	0x08000, 0x5779705e, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "21j-3.24",      	0x08000, 0x3bdea613, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "21j-4-1.23",    	0x08000, 0x728f87b9, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "21jm-0.ic55",   	0x04000, 0xf5232d03, 2 | BRF_ESS | BRF_PRG }, //  4	HD63701 Program Code

	{ "21j-0-1",       	0x08000, 0x9efa95bb, 3 | BRF_ESS | BRF_PRG }, //  5	M6809 Program Code

	{ "21j-5",         	0x08000, 0x7a8b8db4, 1 | BRF_GRA },	     	  //  6	Characters

	{ "21j-a",         	0x10000, 0x574face3, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "21j-b",         	0x10000, 0x40507a76, 2 | BRF_GRA },	     	  //  8
	{ "21j-c",         	0x10000, 0xbb0bc76f, 2 | BRF_GRA },	     	  //  9
	{ "21j-d",         	0x10000, 0xcb4f231b, 2 | BRF_GRA },	     	  // 10
	{ "21j-e",         	0x10000, 0xa0a0c261, 2 | BRF_GRA },	     	  // 11
	{ "21j-f",         	0x10000, 0x6ba152f6, 2 | BRF_GRA },	     	  // 12
	{ "21j-g",         	0x10000, 0x3220a0b6, 2 | BRF_GRA },	     	  // 13
	{ "21j-h",         	0x10000, 0x65c7517d, 2 | BRF_GRA },	     	  // 14

	{ "21j-8",         	0x10000, 0x7c435887, 3 | BRF_GRA },	     	  // 15	Tiles
	{ "21j-9",         	0x10000, 0xc6640aed, 3 | BRF_GRA },	     	  // 16
	{ "21j-i",         	0x10000, 0x5effb0a0, 3 | BRF_GRA },	     	  // 17
	{ "21j-j",         	0x10000, 0x5fb42e7c, 3 | BRF_GRA },	     	  // 18

	{ "21j-6",         	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 19	Samples
	{ "21j-7",         	0x10000, 0x904de6f8, 2 | BRF_SND },	     	  // 20

	{ "21j-k-0",       	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 21	PROMs
	{ "21j-l-0",       	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 22
};

STD_ROM_PICK(ddragonj)
STD_ROM_FN(ddragonj)

struct BurnDriver BurnDrvDdragonj = {
	"ddragonj", "ddragon", NULL, NULL, "1987",
	"Double Dragon (Japan set 1)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragonjRomInfo, ddragonjRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DdragonDIPInfo,
	DdragonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragonjaRomDesc[] = {
	{ "21j-1-8.26",    	0x08000, 0x54c1ef98, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code - BAD DUMP
	{ "21j-2-4.25",    	0x08000, 0x5cd67657, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "21j-3.24",      	0x08000, 0x3bdea613, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "21j-4-2.23",    	0x08000, 0x6312767d, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "21jm-0.ic55",   	0x04000, 0xf5232d03, 2 | BRF_ESS | BRF_PRG }, //  4	HD63701 Program Code

	{ "21j-0-1",       	0x08000, 0x9efa95bb, 3 | BRF_ESS | BRF_PRG }, //  5	M6809 Program Code

	{ "21j-5",         	0x08000, 0x7a8b8db4, 1 | BRF_GRA },	     	  //  6	Characters

	{ "21j-a",         	0x10000, 0x574face3, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "21j-b",         	0x10000, 0x40507a76, 2 | BRF_GRA },	     	  //  8
	{ "21j-c",         	0x10000, 0xbb0bc76f, 2 | BRF_GRA },	     	  //  9
	{ "21j-d",         	0x10000, 0xcb4f231b, 2 | BRF_GRA },	     	  // 10
	{ "21j-e",         	0x10000, 0xa0a0c261, 2 | BRF_GRA },	     	  // 11
	{ "21j-f",         	0x10000, 0x6ba152f6, 2 | BRF_GRA },	     	  // 12
	{ "21j-g",         	0x10000, 0x3220a0b6, 2 | BRF_GRA },	     	  // 13
	{ "21j-h",         	0x10000, 0x65c7517d, 2 | BRF_GRA },	     	  // 14

	{ "21j-8",         	0x10000, 0x7c435887, 3 | BRF_GRA },	     	  // 15	Tiles
	{ "21j-9",         	0x10000, 0xc6640aed, 3 | BRF_GRA },	     	  // 16
	{ "21j-i",         	0x10000, 0x5effb0a0, 3 | BRF_GRA },	     	  // 17
	{ "21j-j",         	0x10000, 0x5fb42e7c, 3 | BRF_GRA },	     	  // 18

	{ "21j-6",         	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 19	Samples
	{ "21j-7",         	0x10000, 0x904de6f8, 2 | BRF_SND },	     	  // 20

	{ "21j-k-0",       	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 21	PROMs
	{ "21j-l-0",       	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 22
};

STD_ROM_PICK(ddragonja)
STD_ROM_FN(ddragonja)

struct BurnDriver BurnDrvDdragonja = {
	"ddragonja", "ddragon", NULL, NULL, "1987",
	"Double Dragon (Japan set 2)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragonjaRomInfo, ddragonjaRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DdragonDIPInfo,
	DdragonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragonuRomDesc[] = {
	{ "21a-1-5.26",    	0x08000, 0xe24a6e11, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "21j-2-3.25",    	0x08000, 0x5779705e, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "21a-3.24",      	0x08000, 0xdbf24897, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "21a-4.23",      	0x08000, 0x6ea16072, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "21jm-0.ic55",   	0x04000, 0xf5232d03, 2 | BRF_ESS | BRF_PRG }, //  4	HD63701 Program Code

	{ "21j-0-1",       	0x08000, 0x9efa95bb, 3 | BRF_ESS | BRF_PRG }, //  5	M6809 Program Code

	{ "21j-5",         	0x08000, 0x7a8b8db4, 1 | BRF_GRA },	     	  //  6	Characters

	{ "21j-a",         	0x10000, 0x574face3, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "21j-b",         	0x10000, 0x40507a76, 2 | BRF_GRA },	     	  //  8
	{ "21j-c",         	0x10000, 0xbb0bc76f, 2 | BRF_GRA },	     	  //  9
	{ "21j-d",         	0x10000, 0xcb4f231b, 2 | BRF_GRA },	     	  // 10
	{ "21j-e",         	0x10000, 0xa0a0c261, 2 | BRF_GRA },	     	  // 11
	{ "21j-f",         	0x10000, 0x6ba152f6, 2 | BRF_GRA },	     	  // 12
	{ "21j-g",         	0x10000, 0x3220a0b6, 2 | BRF_GRA },	     	  // 13
	{ "21j-h",         	0x10000, 0x65c7517d, 2 | BRF_GRA },	     	  // 14

	{ "21j-8",         	0x10000, 0x7c435887, 3 | BRF_GRA },	     	  // 15	Tiles
	{ "21j-9",         	0x10000, 0xc6640aed, 3 | BRF_GRA },	     	  // 16
	{ "21j-i",         	0x10000, 0x5effb0a0, 3 | BRF_GRA },	     	  // 17
	{ "21j-j",         	0x10000, 0x5fb42e7c, 3 | BRF_GRA },	     	  // 18

	{ "21j-6",         	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 19	Samples
	{ "21j-7",         	0x10000, 0x904de6f8, 2 | BRF_SND },	     	  // 20

	{ "21j-k-0.101",   	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 21	PROMs
	{ "21j-l-0.16",    	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 22
};

STD_ROM_PICK(ddragonu)
STD_ROM_FN(ddragonu)

struct BurnDriver BurnDrvDdragonu = {
	"ddragonu", "ddragon", NULL, NULL, "1987",
	"Double Dragon (US set 1)\0", NULL, "Technos Japan (Taito America license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragonuRomInfo, ddragonuRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DdragonDIPInfo,
	DdragonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragonuaRomDesc[] = {
	{ "21a-1",         	0x08000, 0x1d625008, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "21a-2_4",       	0x08000, 0x5cd67657, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "21a-3",         	0x08000, 0xdbf24897, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "21a-4_2",       	0x08000, 0x9b019598, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "21jm-0.ic55",   	0x04000, 0xf5232d03, 2 | BRF_ESS | BRF_PRG }, //  4	HD63701 Program Code

	{ "21j-0-1",       	0x08000, 0x9efa95bb, 3 | BRF_ESS | BRF_PRG }, //  5	M6809 Program Code

	{ "21j-5",         	0x08000, 0x7a8b8db4, 1 | BRF_GRA },	     	  //  6	Characters

	{ "21j-a",         	0x10000, 0x574face3, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "21j-b",         	0x10000, 0x40507a76, 2 | BRF_GRA },	     	  //  8
	{ "21j-c",         	0x10000, 0xbb0bc76f, 2 | BRF_GRA },	     	  //  9
	{ "21j-d",         	0x10000, 0xcb4f231b, 2 | BRF_GRA },	     	  // 10
	{ "21j-e",         	0x10000, 0xa0a0c261, 2 | BRF_GRA },	     	  // 11
	{ "21j-f",         	0x10000, 0x6ba152f6, 2 | BRF_GRA },	     	  // 12
	{ "21j-g",         	0x10000, 0x3220a0b6, 2 | BRF_GRA },	     	  // 13
	{ "21j-h",         	0x10000, 0x65c7517d, 2 | BRF_GRA },	     	  // 14

	{ "21j-8",         	0x10000, 0x7c435887, 3 | BRF_GRA },	     	  // 15	Tiles
	{ "21j-9",         	0x10000, 0xc6640aed, 3 | BRF_GRA },	     	  // 16
	{ "21j-i",         	0x10000, 0x5effb0a0, 3 | BRF_GRA },	     	  // 17
	{ "21j-j",         	0x10000, 0x5fb42e7c, 3 | BRF_GRA },	     	  // 18

	{ "21j-6",         	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 19	Samples
	{ "21j-7",         	0x10000, 0x904de6f8, 2 | BRF_SND },	     	  // 20

	{ "21j-k-0.101",   	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 21	PROMs
	{ "21j-l-0.16",    	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 22
};

STD_ROM_PICK(ddragonua)
STD_ROM_FN(ddragonua)

struct BurnDriver BurnDrvDdragoua = {
	"ddragonua", "ddragon", NULL, NULL, "1987",
	"Double Dragon (US set 2)\0", NULL, "Technos Japan (Taito America license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragonuaRomInfo, ddragonuaRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DdragonDIPInfo,
	DdragonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragonubRomDesc[] = {
	{ "21a-1_6.bin",   	0x08000, 0xf354b0e1, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "21a-2_4",       	0x08000, 0x5cd67657, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "21a-3",         	0x08000, 0xdbf24897, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "21a-4_2",       	0x08000, 0x9b019598, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "21jm-0.ic55",   	0x04000, 0xf5232d03, 2 | BRF_ESS | BRF_PRG }, //  4	HD63701 Program Code

	{ "21j-0-1",       	0x08000, 0x9efa95bb, 3 | BRF_ESS | BRF_PRG }, //  5	M6809 Program Code

	{ "21j-5",         	0x08000, 0x7a8b8db4, 1 | BRF_GRA },	     	  //  6	Characters

	{ "21j-a",         	0x10000, 0x574face3, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "21j-b",         	0x10000, 0x40507a76, 2 | BRF_GRA },	     	  //  8
	{ "21j-c",         	0x10000, 0xbb0bc76f, 2 | BRF_GRA },	     	  //  9
	{ "21j-d",         	0x10000, 0xcb4f231b, 2 | BRF_GRA },	     	  // 10
	{ "21j-e",         	0x10000, 0xa0a0c261, 2 | BRF_GRA },	     	  // 11
	{ "21j-f",         	0x10000, 0x6ba152f6, 2 | BRF_GRA },	     	  // 12
	{ "21j-g",         	0x10000, 0x3220a0b6, 2 | BRF_GRA },	     	  // 13
	{ "21j-h",         	0x10000, 0x65c7517d, 2 | BRF_GRA },	     	  // 14

	{ "21j-8",         	0x10000, 0x7c435887, 3 | BRF_GRA },	     	  // 15	Tiles
	{ "21j-9",         	0x10000, 0xc6640aed, 3 | BRF_GRA },	     	  // 16
	{ "21j-i",         	0x10000, 0x5effb0a0, 3 | BRF_GRA },	     	  // 17
	{ "21j-j",         	0x10000, 0x5fb42e7c, 3 | BRF_GRA },	     	  // 18

	{ "21j-6",         	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 19	Samples
	{ "21j-7",         	0x10000, 0x904de6f8, 2 | BRF_SND },	     	  // 20

	{ "21j-k-0.101",   	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 21	PROMs
	{ "21j-l-0.16",    	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 22
};

STD_ROM_PICK(ddragonub)
STD_ROM_FN(ddragonub)

struct BurnDriver BurnDrvDdragonub = {
	"ddragonub", "ddragon", NULL, NULL, "1987",
	"Double Dragon (US set 3)\0", NULL, "Technos Japan (Taito America license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragonubRomInfo, ddragonubRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DdragonDIPInfo,
	DdragonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragonbl2RomDesc[] = {
	{ "b2_4.bin",      	0x08000, 0x668dfa19, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "b2_5.bin",      	0x08000, 0x5779705e, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "b2_6.bin",      	0x08000, 0x3bdea613, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "b2_7.bin",      	0x08000, 0x728f87b9, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "63701.bin",     	0x04000, 0xf5232d03, 2 | BRF_ESS | BRF_PRG }, //  4	HD63701 Program Code

	{ "b2_3.bin",      	0x08000, 0x9efa95bb, 3 | BRF_ESS | BRF_PRG }, //  5	M6809 Program Code

	{ "b2_8.bin",      	0x08000, 0x7a8b8db4, 1 | BRF_GRA },	     	  //  6	Characters

	{ "11.bin",        	0x10000, 0x574face3, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "12.bin",        	0x10000, 0x40507a76, 2 | BRF_GRA },	     	  //  8
	{ "13.bin",        	0x10000, 0xc8b91e17, 2 | BRF_GRA },	     	  //  9
	{ "14.bin",        	0x10000, 0xcb4f231b, 2 | BRF_GRA },	     	  // 10
	{ "15.bin",        	0x10000, 0xa0a0c261, 2 | BRF_GRA },	     	  // 11
	{ "16.bin",        	0x10000, 0x6ba152f6, 2 | BRF_GRA },	     	  // 12
	{ "17.bin",        	0x10000, 0x3220a0b6, 2 | BRF_GRA },	     	  // 13
	{ "18.bin",        	0x10000, 0x65c7517d, 2 | BRF_GRA },	     	  // 14

	{ "9.bin",         	0x10000, 0x7c435887, 3 | BRF_GRA },	     	  // 15	Tiles
	{ "10.bin",        	0x10000, 0xc6640aed, 3 | BRF_GRA },	     	  // 16
	{ "19.bin",        	0x10000, 0x22d65df2, 3 | BRF_GRA },	     	  // 17
	{ "20.bin",        	0x10000, 0x5fb42e7c, 3 | BRF_GRA },	     	  // 18

	{ "b2_1.bin",      	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 19	Samples
	{ "2.bin",         	0x10000, 0x904de6f8, 2 | BRF_SND },	     	  // 20

	{ "21j-k-0.101",   	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 21	PROMs
	{ "21j-l-0.16",    	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 22
};

STD_ROM_PICK(ddragonbl2)
STD_ROM_FN(ddragonbl2)

struct BurnDriver BurnDrvDdragobl2 = {
	"ddragonbl2", "ddragon", NULL, NULL, "1987",
	"Double Dragon (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragonbl2RomInfo, ddragonbl2RomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DdragonDIPInfo,
	DdragonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragonblRomDesc[] = {
	{ "21j-1.26",      	0x08000, 0xae714964, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "21j-2-3.25",    	0x08000, 0x5779705e, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "21a-3.24",      	0x08000, 0xdbf24897, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "21j-4.23",      	0x08000, 0x6c9f46fa, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "ic38",          	0x04000, 0x6a6a0325, 2 | BRF_ESS | BRF_PRG }, //  4	HD6309 Program Code

	{ "21j-0-1",       	0x08000, 0x9efa95bb, 3 | BRF_ESS | BRF_PRG }, //  5	M6809 Program Code

	{ "21j-5",         	0x08000, 0x7a8b8db4, 1 | BRF_GRA },	     	  //  6	Characters

	{ "21j-a",         	0x10000, 0x574face3, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "21j-b",         	0x10000, 0x40507a76, 2 | BRF_GRA },	     	  //  8
	{ "21j-c",         	0x10000, 0xbb0bc76f, 2 | BRF_GRA },	     	  //  9
	{ "21j-d",         	0x10000, 0xcb4f231b, 2 | BRF_GRA },	     	  // 10
	{ "21j-e",         	0x10000, 0xa0a0c261, 2 | BRF_GRA },	     	  // 11
	{ "21j-f",         	0x10000, 0x6ba152f6, 2 | BRF_GRA },	     	  // 12
	{ "21j-g",         	0x10000, 0x3220a0b6, 2 | BRF_GRA },	     	  // 13
	{ "21j-h",         	0x10000, 0x65c7517d, 2 | BRF_GRA },	     	  // 14

	{ "21j-8",         	0x10000, 0x7c435887, 3 | BRF_GRA },	     	  // 15	Tiles
	{ "21j-9",         	0x10000, 0xc6640aed, 3 | BRF_GRA },	     	  // 16
	{ "21j-i",         	0x10000, 0x5effb0a0, 3 | BRF_GRA },	     	  // 17
	{ "21j-j",         	0x10000, 0x5fb42e7c, 3 | BRF_GRA },	     	  // 18

	{ "21j-6",         	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 19	Samples
	{ "21j-7",         	0x10000, 0x904de6f8, 2 | BRF_SND },	     	  // 20

	{ "21j-k-0.101",   	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 21	PROMs
	{ "21j-l-0.16",    	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 22
};

STD_ROM_PICK(ddragonbl)
STD_ROM_FN(ddragonbl)

static INT32 ddragonblInit()
{
	subcpu_type = CPU_HD6309;
	soundcpu_type = CPU_M6809;
	is_game = DDRAGON;

	if (RomLoader(1)) return 1;
	if (DdragonCommonInit()) return 1;

	return 0;
}

struct BurnDriver BurnDrvDdragonbl = {
	"ddragonbl", "ddragon", NULL, NULL, "1987",
	"Double Dragon (bootleg with HD6309)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragonblRomInfo, ddragonblRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DdragonDIPInfo,
	ddragonblInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragonblaRomDesc[] = {
	{ "5.bin",         	0x08000, 0xae714964, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "4.bin",         	0x10000, 0x48045762, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "3.bin",         	0x08000, 0xdbf24897, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "2_32.bin",      	0x04000, 0x67875473, 2 | BRF_ESS | BRF_PRG }, //  3	M6803 Program Code

	{ "6.bin",         	0x08000, 0x9efa95bb, 3 | BRF_ESS | BRF_PRG }, //  4	M6809 Program Code

	{ "1.bin",         	0x08000, 0x7a8b8db4, 1 | BRF_GRA },	     	  //  5	Characters

	{ "21j-a",         	0x10000, 0x574face3, 2 | BRF_GRA },	     	  //  6	Sprites
	{ "21j-b",         	0x10000, 0x40507a76, 2 | BRF_GRA },	     	  //  7
	{ "21j-c",         	0x10000, 0xbb0bc76f, 2 | BRF_GRA },	     	  //  8
	{ "21j-d",         	0x10000, 0xcb4f231b, 2 | BRF_GRA },	     	  //  9
	{ "21j-e",         	0x10000, 0xa0a0c261, 2 | BRF_GRA },	     	  // 10
	{ "21j-f",         	0x10000, 0x6ba152f6, 2 | BRF_GRA },	     	  // 11
	{ "21j-g",         	0x10000, 0x3220a0b6, 2 | BRF_GRA },	     	  // 12
	{ "21j-h",         	0x10000, 0x65c7517d, 2 | BRF_GRA },	     	  // 13

	{ "21j-8",         	0x10000, 0x7c435887, 3 | BRF_GRA },	     	  // 14	Tiles
	{ "21j-9",         	0x10000, 0xc6640aed, 3 | BRF_GRA },	     	  // 15
	{ "21j-i",         	0x10000, 0x5effb0a0, 3 | BRF_GRA },	     	  // 16
	{ "21j-j",         	0x10000, 0x5fb42e7c, 3 | BRF_GRA },	     	  // 17

	{ "8.bin",         	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 18	Samples
	{ "7.bin",         	0x10000, 0xf9311f72, 2 | BRF_SND },	     	  // 19

	{ "21j-k-0.101",   	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 20	PROMs
	{ "21j-l-0.16",    	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 21
};

STD_ROM_PICK(ddragonbla)
STD_ROM_FN(ddragonbla)

static INT32 DdragonblaInit()
{
	subcpu_type = CPU_M6803;
	soundcpu_type = CPU_M6809;
	is_game = DDRAGON;

	if (RomLoader(1)) return 1;
	BurnSwapMemBlock(DrvHD6309Rom + 0x18000, DrvHD6309Rom + 0x10000, 0x8000);
	if (DdragonCommonInit()) return 1;

	return 0;
}

struct BurnDriver BurnDrvDdragnbla = {
	"ddragonbla", "ddragon", NULL, NULL, "1987",
	"Double Dragon (bootleg with MC6803)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragonblaRomInfo, ddragonblaRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DdragonDIPInfo,
	DdragonblaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragon2RomDesc[] = {
	{ "26a9-04.bin",   	0x08000, 0xf2cfc649, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "26aa-03.bin",   	0x08000, 0x44dd5d4b, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "26ab-0.bin",    	0x08000, 0x49ddddcd, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "26ac-0e.63",    	0x08000, 0x57acad2c, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "26ae-0.bin",    	0x10000, 0xea437867, 2 | BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code

	{ "26ad-0.bin",    	0x08000, 0x75e36cd6, 3 | BRF_ESS | BRF_PRG }, //  5	Z80 #2 Program Code

	{ "26a8-0e.19",    	0x10000, 0x4e80cd36, 1 | BRF_GRA },	     	  //  6	Characters

	{ "26j0-0.bin",    	0x20000, 0xdb309c84, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "26j1-0.bin",    	0x20000, 0xc3081e0c, 2 | BRF_GRA },	     	  //  8
	{ "26af-0.bin",    	0x20000, 0x3a615aad, 2 | BRF_GRA },	     	  //  9
	{ "26j2-0.bin",    	0x20000, 0x589564ae, 2 | BRF_GRA },	     	  // 10
	{ "26j3-0.bin",    	0x20000, 0xdaf040d6, 2 | BRF_GRA },	     	  // 11
	{ "26a10-0.bin",   	0x20000, 0x6d16d889, 2 | BRF_GRA },	     	  // 12

	{ "26j4-0.bin",    	0x20000, 0xa8c93e76, 3 | BRF_GRA },	     	  // 13	Tiles
	{ "26j5-0.bin",    	0x20000, 0xee555237, 3 | BRF_GRA },	     	  // 14

	{ "26j6-0.bin",    	0x20000, 0xa84b2a29, 1 | BRF_SND },	     	  // 15	Samples
	{ "26j7-0.bin",    	0x20000, 0xbc6a48d5, 1 | BRF_SND },	     	  // 16

	{ "21j-k-0",       	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 17	PROMs
	{ "prom.16",       	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 18
};

STD_ROM_PICK(ddragon2)
STD_ROM_FN(ddragon2)

struct BurnDriver BurnDrvDdragon2 = {
	"ddragon2", NULL, NULL, NULL, "1988",
	"Double Dragon II: The Revenge (World)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragon2RomInfo, ddragon2RomName, NULL, NULL, NULL, NULL, DdragonInputInfo, Ddragon2DIPInfo,
	Ddragon2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragon2jRomDesc[] = {
	{ "26a9-0_j.ic38",  0x08000, 0x5e4fcdff, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "26aa-0_j.ic52",  0x08000, 0xbfb4ee04, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "26ab-0.ic53",    0x08000, 0x49ddddcd, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "26ac-0_j.ic63",  0x08000, 0x165858c7, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "26ae-0.ic37",    0x10000, 0xea437867, 2 | BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code

	{ "26ad-0.ic41",    0x08000, 0x3788af3b, 3 | BRF_ESS | BRF_PRG }, //  5	Z80 #2 Program Code

	{ "26a8-0e.19",    	0x10000, 0x4e80cd36, 1 | BRF_GRA },	     	  //  6	Characters

	{ "26j0-0.bin",    	0x20000, 0xdb309c84, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "26j1-0.bin",    	0x20000, 0xc3081e0c, 2 | BRF_GRA },	     	  //  8
	{ "26af-0.bin",    	0x20000, 0x3a615aad, 2 | BRF_GRA },	     	  //  9
	{ "26j2-0.bin",    	0x20000, 0x589564ae, 2 | BRF_GRA },	     	  // 10
	{ "26j3-0.bin",    	0x20000, 0xdaf040d6, 2 | BRF_GRA },	     	  // 11
	{ "26a10-0.bin",   	0x20000, 0x6d16d889, 2 | BRF_GRA },	     	  // 12

	{ "26j4-0.bin",    	0x20000, 0xa8c93e76, 3 | BRF_GRA },	     	  // 13	Tiles
	{ "26j5-0.bin",    	0x20000, 0xee555237, 3 | BRF_GRA },	     	  // 14

	{ "26j6-0.bin",    	0x20000, 0xa84b2a29, 1 | BRF_SND },	     	  // 15	Samples
	{ "26j7-0.bin",    	0x20000, 0xbc6a48d5, 1 | BRF_SND },	     	  // 16

	{ "21j-k-0",       	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 17	PROMs / BAD DUMP
	{ "prom.16",       	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 18
};

STD_ROM_PICK(ddragon2j)
STD_ROM_FN(ddragon2j)

struct BurnDriver BurnDrvDdragon2j = {
	"ddragon2j", "ddragon2", NULL, NULL, "1988",
	"Double Dragon II: The Revenge (Japan)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragon2jRomInfo, ddragon2jRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, Ddragon2DIPInfo,
	Ddragon2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragon2uRomDesc[] = {
	{ "26a9-04.bin",   	0x08000, 0xf2cfc649, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "26aa-03.bin",   	0x08000, 0x44dd5d4b, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "26ab-0.bin",    	0x08000, 0x49ddddcd, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "26ac-02.bin",   	0x08000, 0x097eaf26, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "26ae-0.bin",    	0x10000, 0xea437867, 2 | BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code

	{ "26ad-0.bin",    	0x08000, 0x75e36cd6, 3 | BRF_ESS | BRF_PRG }, //  5	Z80 #2 Program Code

	{ "26a8-0.bin",    	0x10000, 0x3ad1049c, 1 | BRF_GRA },	     	  //  6	Characters

	{ "26j0-0.bin",    	0x20000, 0xdb309c84, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "26j1-0.bin",    	0x20000, 0xc3081e0c, 2 | BRF_GRA },	     	  //  8
	{ "26af-0.bin",    	0x20000, 0x3a615aad, 2 | BRF_GRA },	     	  //  9
	{ "26j2-0.bin",    	0x20000, 0x589564ae, 2 | BRF_GRA },	     	  // 10
	{ "26j3-0.bin",    	0x20000, 0xdaf040d6, 2 | BRF_GRA },	     	  // 11
	{ "26a10-0.bin",   	0x20000, 0x6d16d889, 2 | BRF_GRA },	     	  // 12

	{ "26j4-0.bin",    	0x20000, 0xa8c93e76, 3 | BRF_GRA },	     	  // 13	Tiles
	{ "26j5-0.bin",    	0x20000, 0xee555237, 3 | BRF_GRA },	     	  // 14

	{ "26j6-0.bin",    	0x20000, 0xa84b2a29, 1 | BRF_SND },	     	  // 15	Samples
	{ "26j7-0.bin",    	0x20000, 0xbc6a48d5, 1 | BRF_SND },	     	  // 16

	{ "21j-k-0",       	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 17	PROMs / BAD DUMP
	{ "prom.16",       	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 18
};

STD_ROM_PICK(ddragon2u)
STD_ROM_FN(ddragon2u)

struct BurnDriver BurnDrvDdragon2u = {
	"ddragon2u", "ddragon2", NULL, NULL, "1988",
	"Double Dragon II: The Revenge (US)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragon2uRomInfo, ddragon2uRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, Ddragon2DIPInfo,
	Ddragon2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragon2blRomDesc[] = {
	{ "3",    		   	0x08000, 0x5cc38bad, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "4",    		   	0x08000, 0x78750947, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "5",    		   	0x08000, 0x49ddddcd, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "6",    		   	0x08000, 0x097eaf26, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "2",    		   	0x10000, 0xea437867, 2 | BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code

	{ "11",    		   	0x08000, 0x75e36cd6, 3 | BRF_ESS | BRF_PRG }, //  5	Z80 #2 Program Code

	{ "1",    			0x10000, 0x3ad1049c, 1 | BRF_GRA },	     	  //  6	Characters

	{ "27",   			0x10000, 0xfe42df5d, 2 | BRF_GRA },	     	  //  7  Sprites
	{ "26",   			0x10000, 0x42f582c6, 2 | BRF_GRA },	     	  //  8
	{ "23",   			0x10000, 0xe157319f, 2 | BRF_GRA },	     	  //  9
	{ "22",   			0x10000, 0x82e952c9, 2 | BRF_GRA },	     	  // 10
	{ "25",   			0x10000, 0x4a4a085d, 2 | BRF_GRA },	     	  // 11
	{ "24",   			0x10000, 0xc9d52536, 2 | BRF_GRA },	     	  // 12
	{ "21",   			0x10000, 0x32ab0897, 2 | BRF_GRA },	     	  // 13
	{ "20",   			0x10000, 0xa68e168f, 2 | BRF_GRA },	     	  // 14
	{ "17",   			0x10000, 0x882f99b1, 2 | BRF_GRA },	     	  // 15
	{ "16",   			0x10000, 0xe2fe3eca, 2 | BRF_GRA },	     	  // 16
	{ "18",   			0x10000, 0x0e1c6c63, 2 | BRF_GRA },	     	  // 17
	{ "19",  			0x10000, 0x0e21eae0, 2 | BRF_GRA },	     	  // 18

	{ "15",   			0x10000, 0x3c3f16f6, 3 | BRF_GRA },	     	  // 19  Tiles
	{ "13",   			0x10000, 0x7c21be72, 3 | BRF_GRA },	     	  // 20
	{ "14",   			0x10000, 0xe92f91f4, 3 | BRF_GRA },	     	  // 21
	{ "12",   			0x10000, 0x6896e2f7, 3 | BRF_GRA },	     	  // 22

	{ "7",    			0x10000, 0x6d9e3f0f, 1 | BRF_SND },	     	  // 23  Samples
	{ "9",    			0x10000, 0x0c15dec9, 1 | BRF_SND },	     	  // 24
	{ "8",    			0x10000, 0x151b22b4, 1 | BRF_SND },	     	  // 25
	{ "10",   			0x10000, 0xae2fc028, 1 | BRF_SND },	     	  // 26

	{ "21j-k-0",       	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 27	PROMs / BAD DUMP
	{ "prom.16",       	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 28
};

STD_ROM_PICK(ddragon2bl)
STD_ROM_FN(ddragon2bl)

struct BurnDriver BurnDrvDdragon2bl = {
	"ddragon2bl", "ddragon2", NULL, NULL, "1988",
	"Double Dragon II: The Revenge (US, bootleg, set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragon2blRomInfo, ddragon2blRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, Ddragon2DIPInfo,
	Ddragon2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddragon2b2RomDesc[] = {
	// f205v id 1182
	{ "dd2ub-3.3g",    	0x08000, 0xf5bd19d2, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "dd2ub-4.4g",    	0x08000, 0x78750947, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "26ab-0.bin",    	0x08000, 0x49ddddcd, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "dd2ub-6.5g",    	0x08000, 0x097eaf26, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "26ae-0.bin",    	0x10000, 0xea437867, 2 | BRF_ESS | BRF_PRG }, //  4	Z80 #1 Program Code

	{ "26ad-0.bin",    	0x08000, 0x75e36cd6, 3 | BRF_ESS | BRF_PRG }, //  5	Z80 #2 Program Code

//	{ "dd2ub-1.2r",    	0x10000, 0xadd7ffc6, 1 | BRF_GRA },	     	  //  6	Characters - BAD DUMP
	{ "26a8-0.bin",    	0x10000, 0x3ad1049c, 1 | BRF_GRA },	     	  //  6	Characters - using US one due to Winners Don't Use Drugs screen

	{ "dd2ub-27.8n",   	0x10000, 0xfe42df5d, 2 | BRF_GRA },	     	  //  7  Sprites
	{ "dd2ub-26.7n",   	0x10000, 0xd2d9a400, 2 | BRF_GRA },	     	  //  8
	{ "dd2ub-23.8k",   	0x10000, 0xe157319f, 2 | BRF_GRA },	     	  //  9
	{ "dd2ub-22.7k",   	0x10000, 0x9f10018c, 2 | BRF_GRA },	     	  // 10
	{ "dd2ub-25.6n",   	0x10000, 0x4a4a085d, 2 | BRF_GRA },	     	  // 11
	{ "dd2ub-24.5n",   	0x10000, 0xc9d52536, 2 | BRF_GRA },	     	  // 12
	{ "dd2ub-21.8g",   	0x10000, 0x32ab0897, 2 | BRF_GRA },	     	  // 13
	{ "dd2ub-20.7g",   	0x10000, 0xf564bd18, 2 | BRF_GRA },	     	  // 14
	{ "dd2ub-17.8d",   	0x10000, 0x882f99b1, 2 | BRF_GRA },	     	  // 15
	{ "dd2ub-16.7d",   	0x10000, 0xcf3c34d5, 2 | BRF_GRA },	     	  // 16
	{ "dd2ub-18.9d",   	0x10000, 0x0e1c6c63, 2 | BRF_GRA },	     	  // 17
	{ "dd2ub-19.10d",  	0x10000, 0x0e21eae0, 2 | BRF_GRA },	     	  // 18

	{ "dd2ub-15.5d",   	0x10000, 0x3c3f16f6, 3 | BRF_GRA },	     	  // 19  Tiles
	{ "dd2ub-13.4d",   	0x10000, 0x7c21be72, 3 | BRF_GRA },	     	  // 20
	{ "dd2ub-14.5b",   	0x10000, 0xe92f91f4, 3 | BRF_GRA },	     	  // 21
	{ "dd2ub-12.4b",   	0x10000, 0x6896e2f7, 3 | BRF_GRA },	     	  // 22

	{ "dd2ub-7.3f",    	0x10000, 0x6d9e3f0f, 1 | BRF_SND },	     	  // 23  Samples
	{ "dd2ub-9.5c",    	0x10000, 0x0c15dec9, 1 | BRF_SND },	     	  // 24
	{ "dd2ub-8.3d",    	0x10000, 0x151b22b4, 1 | BRF_SND },	     	  // 25
	{ "dd2ub-10.5b",   	0x10000, 0x95885e12, 1 | BRF_SND },	     	  // 26
};

STD_ROM_PICK(ddragon2b2)
STD_ROM_FN(ddragon2b2)

struct BurnDriver BurnDrvDdragon2b2 = {
	"ddragon2b2", "ddragon2", NULL, NULL, "1988",
	"Double Dragon II: The Revenge (US, bootleg, set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_SCRFIGHT, 0,
	NULL, ddragon2b2RomInfo, ddragon2b2RomName, NULL, NULL, NULL, NULL, DdragonInputInfo, Ddragon2DIPInfo,
	Ddragon2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddungeonRomDesc[] = {
	{ "dd26.26",       	0x08000, 0xa6e7f608, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "dd25.25",       	0x08000, 0x922e719c, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "63701.bin",     	0x04000, 0xf5232d03, 2 | BRF_ESS | BRF_PRG }, //  2	HD63701 Program Code

	{ "dd30.30",       	0x08000, 0xef1af99a, 3 | BRF_ESS | BRF_PRG }, //  3	M6809 Program Code

	{ "dd_mcu.bin",    	0x00800, 0x34cbb2d3, 4 | BRF_ESS | BRF_PRG }, //  4	M68705 MCU Program Code

	{ "dd20.20",       	0x08000, 0xd976b78d, 1 | BRF_GRA },	     	  //  5	Characters

	{ "dd117.117",     	0x08000, 0xe912ca81, 2 | BRF_GRA },	     	  //  6	Sprites
	{ "dd113.113",     	0x08000, 0x43264ad8, 2 | BRF_GRA },	     	  //  7

	{ "dd78.78",       	0x08000, 0x3deacae9, 3 | BRF_GRA },	     	  //  8	Tiles
	{ "dd109.109",     	0x08000, 0x5a2f31eb, 3 | BRF_GRA },	     	  //  9

	{ "21j-6",         	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 10	Samples
	{ "21j-7",         	0x10000, 0x904de6f8, 2 | BRF_SND },	     	  // 11

	{ "21j-k-0.101",   	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 12	PROMs
	{ "21j-l-0.16",    	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 13
};

STD_ROM_PICK(ddungeon)
STD_ROM_FN(ddungeon)

static INT32 DdungeonInit()
{
	subcpu_type = CPU_HD63701;
	soundcpu_type = CPU_M6809;
	is_game = DARKTOWER;

	if (RomLoader(1)) return 1;
	if (DdragonCommonInit()) return 1;

	return 0;
}

struct BurnDriver BurnDrvDdungeon = {
	"ddungeon", NULL, NULL, NULL, "1992",
	"Dangerous Dungeons (set 1)\0", NULL, "The Game Room", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_MAZE | GBF_PUZZLE, 0,
	NULL, ddungeonRomInfo, ddungeonRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DdungeonDIPInfo,
	DdungeonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo ddungeoneRomDesc[] = {
	{ "dd26.26",       	0x08000, 0xa6e7f608, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "dd25.25",       	0x08000, 0x922e719c, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "63701.bin",     	0x04000, 0xf5232d03, 2 | BRF_ESS | BRF_PRG }, //  2	HD63701 Program Code

	{ "21j-0-1",       	0x08000, 0x9efa95bb, 3 | BRF_ESS | BRF_PRG }, //  3	M6809 Program Code

	{ "dd_mcu.bin",    	0x00800, 0x34cbb2d3, 4 | BRF_ESS | BRF_PRG }, //  4	M68705 MCU Program Code

	{ "dd6.bin",       	0x08000, 0x057588ca, 1 | BRF_GRA },	     	  //  5	Characters

	{ "dd-7r.bin",     	0x08000, 0x50d6ab5d, 2 | BRF_GRA },	     	  //  6	Sprites
	{ "dd113.113",     	0x08000, 0x43264ad8, 2 | BRF_GRA },	     	  //  7

	{ "dd78.78",       	0x08000, 0x3deacae9, 3 | BRF_GRA },	     	  //  8	Tiles
	{ "dd109.109",     	0x08000, 0x5a2f31eb, 3 | BRF_GRA },	     	  //  9

	{ "21j-6",         	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 10	Samples
	{ "21j-7",         	0x10000, 0x904de6f8, 2 | BRF_SND },	     	  // 11

	{ "21j-k-0.101",   	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 12	PROMs
	{ "21j-l-0.16",    	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 13
};

STD_ROM_PICK(ddungeone)
STD_ROM_FN(ddungeone)

struct BurnDriver BurnDrvDdungeone = {
	"ddungeone", "ddungeon", NULL, NULL, "1992",
	"Dangerous Dungeons (set 2)\0", NULL, "East Coast Coin Company", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_MAZE | GBF_PUZZLE, 0,
	NULL, ddungeoneRomInfo, ddungeoneRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DdungeonDIPInfo,
	DdungeonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

static struct BurnRomInfo darktowrRomDesc[] = {
	{ "dt.26",         	0x08000, 0x8134a472, 1 | BRF_ESS | BRF_PRG }, //  0	HD6309 Program Code
	{ "21j-2-3.25",    	0x08000, 0x5779705e, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "dt.24",         	0x08000, 0x523a5413, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "63701.bin",     	0x04000, 0xf5232d03, 2 | BRF_ESS | BRF_PRG }, //  3	HD63701 Program Code

	{ "21j-0-1",       	0x08000, 0x9efa95bb, 3 | BRF_ESS | BRF_PRG }, //  4	M6809 Program Code

	{ "68705prt.mcu",  	0x00800, 0x34cbb2d3, 4 | BRF_ESS | BRF_PRG }, //  5	M68705 MCU Program Code

	{ "dt.20",         	0x08000, 0x860b0298, 1 | BRF_GRA },	     	  //  6	Characters

	{ "dt.117",        	0x10000, 0x750dd0fa, 2 | BRF_GRA },	     	  //  7	Sprites
	{ "dt.116",        	0x10000, 0x22cfa87b, 2 | BRF_GRA },	     	  //  8
	{ "dt.115",        	0x10000, 0x8a9f1c34, 2 | BRF_GRA },	     	  //  9
	{ "21j-d",         	0x10000, 0xcb4f231b, 2 | BRF_GRA },	     	  // 10
	{ "dt.113",        	0x10000, 0x7b4bbf9c, 2 | BRF_GRA },	     	  // 11
	{ "dt.112",        	0x10000, 0xdf3709d4, 2 | BRF_GRA },	     	  // 12
	{ "dt.111",        	0x10000, 0x59032154, 2 | BRF_GRA },	     	  // 13
	{ "21j-h",         	0x10000, 0x65c7517d, 2 | BRF_GRA },	     	  // 14

	{ "dt.78",         	0x10000, 0x72c15604, 3 | BRF_GRA },	     	  // 15	Tiles
	{ "21j-9",         	0x10000, 0xc6640aed, 3 | BRF_GRA },	     	  // 16
	{ "dt.109",        	0x10000, 0x15bdcb62, 3 | BRF_GRA },	     	  // 17
	{ "21j-j",         	0x10000, 0x5fb42e7c, 3 | BRF_GRA },	     	  // 18

	{ "21j-6",         	0x10000, 0x34755de3, 1 | BRF_SND },	     	  // 19	Samples
	{ "21j-7",         	0x10000, 0x904de6f8, 2 | BRF_SND },	     	  // 20

	{ "21j-k-0.101",   	0x00100, 0xfdb130a9, 8 | BRF_OPT },	     	  // 21	PROMs
	{ "21j-l-0.16",    	0x00200, 0x46339529, 8 | BRF_OPT },	     	  // 22
};

STD_ROM_PICK(darktowr)
STD_ROM_FN(darktowr)

struct BurnDriver BurnDrvDarktowr = {
	"darktowr", NULL, NULL, NULL, "1992",
	"Dark Tower\0", NULL, "The Game Room", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_PLATFORM, 0,
	NULL, darktowrRomInfo, darktowrRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, DarktowrDIPInfo,
	DdungeonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

// Toffy

static struct BurnRomInfo toffyRomDesc[] = {
	{ "2-27512.rom",	0x10000, 0x244709dd, 1 | BRF_PRG | BRF_ESS }, //  0	HD6309 Program Code

	{ "u142.1",			0x10000, 0x541bd7f0, 3 | BRF_PRG | BRF_ESS }, //  1	M6809 Program Code (sound)

	{ "7-27512.rom",	0x10000, 0xf9e8ec64, 1 | BRF_GRA },           //  2 Characters

	{ "4-27512.rom",	0x10000, 0x94b5ef6f, 3 | BRF_GRA },           //  3 Tiles
	{ "3-27512.rom",	0x10000, 0xa7a053a3, 3 | BRF_GRA },           //  4

	{ "6-27512.rom",	0x10000, 0x2ba7ca47, 2 | BRF_GRA },           //  5 Sprites
	{ "5-27512.rom",	0x10000, 0x4f91eec6, 2 | BRF_GRA },           //  6
};

STD_ROM_PICK(toffy)
STD_ROM_FN(toffy)

static INT32 ToffyInit()
{
	subcpu_type = CPU_NONE;
	soundcpu_type = CPU_M6809;
	is_game = TOFFY;

	if (RomLoader(1)) return 1;
	if (DdragonCommonInit()) return 1;

	return 0;
}

struct BurnDriver BurnDrvToffy = {
	"toffy", NULL, NULL, NULL, "1993",
	"Toffy\0", NULL, "Midas", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_MAZE | GBF_PUZZLE, 0,
	NULL, toffyRomInfo, toffyRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, ToffyDIPInfo,
	ToffyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

// Super Toffy

static struct BurnRomInfo stoffyRomDesc[] = {
	{ "2.u70",			0x10000, 0x6203aeb5, 1 | BRF_PRG | BRF_ESS }, //  0	HD6309 Program Code

	{ "1.u142",			0x10000, 0x541bd7f0, 3 | BRF_PRG | BRF_ESS }, //  1	M6809 Program Code (sound)

	{ "7.u35",			0x10000, 0x1cf13736, 1 | BRF_GRA },           //  2 Characters

	{ "4.u78",			0x10000, 0x2066c3c7, 3 | BRF_GRA },           //  3 Tiles
	{ "3.u77",			0x10000, 0x3625f813, 3 | BRF_GRA },           //  4

	{ "6.u80",			0x10000, 0xff190865, 2 | BRF_GRA },           //  5 Sprites
	{ "5.u79",			0x10000, 0x333d5b8a, 2 | BRF_GRA },           //  6
};

STD_ROM_PICK(stoffy)
STD_ROM_FN(stoffy)

struct BurnDriver BurnDrvStoffy = {
	"stoffy", NULL, NULL, NULL, "1994",
	"Super Toffy\0", NULL, "Midas", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_MAZE | GBF_PUZZLE, 0,
	NULL, stoffyRomInfo, stoffyRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, ToffyDIPInfo,
	ToffyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

// Super Toffy (Unico license)

static struct BurnRomInfo stoffyuRomDesc[] = {
	{ "u70.2",			0x10000, 0x3c156610, 1 | BRF_PRG | BRF_ESS }, //  0	HD6309 Program Code

	{ "1.u142",			0x10000, 0x541bd7f0, 3 | BRF_PRG | BRF_ESS }, //  1	M6809 Program Code (sound)

	{ "u35.7",			0x10000, 0x83735d25, 1 | BRF_GRA },           //  2 Characters

	{ "u78.4",			0x10000, 0x9743a74d, 3 | BRF_GRA },           //  3 Tiles
	{ "u77.3",			0x10000, 0xf267109a, 3 | BRF_GRA },           //  4

	{ "6.u80",			0x10000, 0xff190865, 2 | BRF_GRA },           //  5 Sprites
	{ "5.u79",			0x10000, 0x333d5b8a, 2 | BRF_GRA },           //  6
};

STD_ROM_PICK(stoffyu)
STD_ROM_FN(stoffyu)

struct BurnDriver BurnDrvStoffyu = {
	"stoffyu", "stoffy", NULL, NULL, "1994",
	"Super Toffy (Unico license)\0", NULL, "Midas (Unico license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_MAZE | GBF_PUZZLE, 0,
	NULL, stoffyuRomInfo, stoffyuRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, ToffyDIPInfo,
	ToffyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	256, 240, 4, 3
};

// Thunder Strike (set 1)

static struct BurnRomInfo tstrikeRomDesc[] = {
	{ "prog.rom",		0x08000, 0xbf011a00, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tstrike.25",		0x08000, 0xb6a0c2f3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tstrike.24",		0x08000, 0x363816fa, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "63701.bin",		0x04000, 0xf5232d03, 2 | BRF_PRG | BRF_ESS }, //  3 sub

	{ "tstrike.30",		0x08000, 0x3f3f04a1, 3 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "68705prt.mcu",	0x00800, 0x34cbb2d3, 4 | BRF_PRG | BRF_ESS }, //  5 mcu

	{ "alpha.rom",		0x08000, 0x3a7c3185, 1 | BRF_GRA },           //  6 gfx1

	{ "tstrike.117",	0x10000, 0xf7122c0d, 2 | BRF_GRA },           //  7 gfx2
	{ "21j-b",			0x10000, 0x40507a76, 2 | BRF_GRA },           //  8
	{ "tstrike.115",	0x10000, 0xa13c7b62, 2 | BRF_GRA },           //  9
	{ "21j-d",			0x10000, 0xcb4f231b, 2 | BRF_GRA },           // 10
	{ "tstrike.113",	0x10000, 0x5ad60938, 2 | BRF_GRA },           // 11
	{ "21j-f",			0x10000, 0x6ba152f6, 2 | BRF_GRA },           // 12
	{ "tstrike.111",	0x10000, 0x7b9c87ad, 2 | BRF_GRA },           // 13
	{ "21j-h",			0x10000, 0x65c7517d, 2 | BRF_GRA },           // 14

	{ "tstrike.78",		0x10000, 0x88284aec, 3 | BRF_GRA },           // 15 gfx3
	{ "21j-9",			0x10000, 0xc6640aed, 3 | BRF_GRA },           // 16
	{ "tstrike.109",	0x10000, 0x8c2cd0bb, 3 | BRF_GRA },           // 17
	{ "21j-j",			0x10000, 0x5fb42e7c, 3 | BRF_GRA },           // 18

	{ "tstrike.94",		0x10000, 0x8a2c09fc, 1 | BRF_SND },           // 19 adpcm1
	{ "tstrike.95",		0x08000, 0x1812eecb, 2 | BRF_SND },           // 20 adpcm2

	{ "21j-k-0.101",	0x00100, 0xfdb130a9, 8 | BRF_OPT },           // 21 proms
	{ "21j-l-0.16",		0x00200, 0x46339529, 8 | BRF_OPT },           // 22
};

STD_ROM_PICK(tstrike)
STD_ROM_FN(tstrike)

static INT32 TstrikeInit()
{
	subcpu_type = CPU_HD63701;
	soundcpu_type = CPU_M6809;
	is_game = TSTRIKE;

	if (RomLoader(1)) return 1;
	if (DdragonCommonInit()) return 1;

	return 0;
}

struct BurnDriver BurnDrvTstrike = {
	"tstrike", NULL, NULL, NULL, "1991",
	"Thunder Strike (set 1)\0", NULL, "East Coast Coin Company", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_HORSHOOT, 0,
	NULL, tstrikeRomInfo, tstrikeRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, TstrikeDIPInfo,
	TstrikeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180,
	240, 240, 4, 3
};

// Thunder Strike (set 2, older)

static struct BurnRomInfo tstrikeaRomDesc[] = {
	{ "tstrike.26",		0x08000, 0x871b10bc, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tstrike.25",		0x08000, 0xb6a0c2f3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tstrike.24",		0x08000, 0x363816fa, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "63701.bin",		0x04000, 0xf5232d03, 2 | BRF_PRG | BRF_ESS }, //  3 sub

	{ "tstrike.30",		0x08000, 0x3f3f04a1, 3 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "68705prt.mcu",	0x00800, 0x34cbb2d3, 4 | BRF_PRG | BRF_ESS }, //  5 mcu

	{ "tstrike.20",		0x08000, 0xb6b8bfa0, 1 | BRF_GRA },           //  6 gfx1

	{ "tstrike.117",	0x10000, 0xf7122c0d, 2 | BRF_GRA },           //  7 gfx2
	{ "21j-b",			0x10000, 0x40507a76, 2 | BRF_GRA },           //  8
	{ "tstrike.115",	0x10000, 0xa13c7b62, 2 | BRF_GRA },           //  9
	{ "21j-d",			0x10000, 0xcb4f231b, 2 | BRF_GRA },           // 10
	{ "tstrike.113",	0x10000, 0x5ad60938, 2 | BRF_GRA },           // 11
	{ "21j-f",			0x10000, 0x6ba152f6, 2 | BRF_GRA },           // 12
	{ "tstrike.111",	0x10000, 0x7b9c87ad, 2 | BRF_GRA },           // 13
	{ "21j-h",			0x10000, 0x65c7517d, 2 | BRF_GRA },           // 14

	{ "tstrike.78",		0x10000, 0x88284aec, 3 | BRF_GRA },           // 15 gfx3
	{ "21j-9",			0x10000, 0xc6640aed, 3 | BRF_GRA },           // 16
	{ "tstrike.109",	0x10000, 0x8c2cd0bb, 3 | BRF_GRA },           // 17
	{ "21j-j",			0x10000, 0x5fb42e7c, 3 | BRF_GRA },           // 18

	{ "tstrike.94",		0x10000, 0x8a2c09fc, 1 | BRF_SND },           // 19 adpcm1
	{ "tstrike.95",		0x08000, 0x1812eecb, 2 | BRF_SND },           // 20 adpcm2

	{ "21j-k-0.101",	0x00100, 0xfdb130a9, 8 | BRF_OPT },           // 21 proms
	{ "21j-l-0.16",		0x00200, 0x46339529, 8 | BRF_OPT },           // 22
};

STD_ROM_PICK(tstrikea)
STD_ROM_FN(tstrikea)

struct BurnDriver BurnDrvTstrikea = {
	"tstrikea", "tstrike", NULL, NULL, "1991",
	"Thunder Strike (set 2, older)\0", NULL, "The Game Room", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TECHNOS, GBF_HORSHOOT, 0,
	NULL, tstrikeaRomInfo, tstrikeaRomName, NULL, NULL, NULL, NULL, DdragonInputInfo, TstrikeDIPInfo,
	DdungeonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x180, // yes, DdungeonInit!
	240, 240, 4, 3
};
