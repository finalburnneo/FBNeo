// FB Neo Irem M72 driver module
// Based on MAME driver by Nicola Salmoria and Nao

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "nec_intf.h"
#include "irem_cpu.h"
#include "mcs51.h"
#include "dac.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvV30ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvMcuROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSndROM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvV30RAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvPalRAM;
static UINT8 *DrvProtRAM;
static UINT8 *DrvRowScroll;
static UINT8 *DrvSprRAM2;

static UINT8 *scroll;

static UINT8 *soundlatch;
static UINT8 *video_disable;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 irqvector;
static INT32 sample_address;
static INT32 irq_raster_position;
static INT32 z80_reset = 0;
static INT32 majtitle_rowscroll_enable = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;
static INT16 Analog[4];

static INT32 nExtraCycles[3];

static INT32 Clock_16mhz = 0;
static INT32 Kengo = 0;
static INT32 CosmicCop = 0;
static INT32 Poundfor = 0;
static INT32 airduelm72 = 0;
static INT32 m72_video_type = 0;
static INT32 m72_video_mask = 0;
static INT32 z80_nmi_enable = 0;
static INT32 enable_z80_reset = 0; // only if z80 is not rom-based!
static INT32 m72_irq_base = 0;
static INT32 code_mask[4];
static INT32 graphics_length[4];
static INT32 video_offsets[2] = { 0, 0 };

static INT32 use_mcu = 0;
static INT32 main_mhz;
static INT32 mcu_mhz;

enum { Z80_NO_NMI = 0, Z80_REAL_NMI, Z80_FAKE_NMI };
enum { VECTOR_INIT, YM2151_ASSERT, YM2151_CLEAR, Z80_ASSERT, Z80_CLEAR };


static struct BurnInputInfo CommonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Common)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo PoundforInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[0],		"p1 x-axis" ),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[1],		"p1 y-axis" ),

	{"P2 Coin",		    BIT_DIGITAL,	DrvJoy2 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 fire 2"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &Analog[2],		"p2 x-axis" ),
	A("P2 Trackball Y", BIT_ANALOG_REL, &Analog[3],		"p2 y-axis" ),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};
#undef A

STDINPUTINFO(Poundfor)

static struct BurnDIPInfo Dip2CoinDIPList[]=
{
	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x16, 0x01, 0x40, 0x40, "Off"							},
	{0x16, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x16, 0x01, 0x80, 0x80, "Off"							},
	{0x16, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x17, 0x01, 0x01, 0x01, "Off"							},
	{0x17, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Coin Mode"					},
	{0x17, 0x01, 0x08, 0x08, "Mode 1"						},
	{0x17, 0x01, 0x08, 0x00, "Mode 2"						},

	{0   , 0xfe, 0   ,   16, "Coinage"						},
	{0x17, 0x01, 0xf0, 0xa0, "6 Coins 1 Credit"				},
	{0x17, 0x01, 0xf0, 0xb0, "5 Coins 1 Credit"				},
	{0x17, 0x01, 0xf0, 0xc0, "4 Coins 1 Credit"				},
	{0x17, 0x01, 0xf0, 0xd0, "3 Coins 1 Credit"				},
	{0x17, 0x01, 0xf0, 0xe0, "2 Coins 1 Credit"				},
	{0x17, 0x01, 0xf0, 0x10, "2 Coins to Start/1 to Cont."	},
	{0x17, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"			},
	{0x17, 0x01, 0xf0, 0x20, "4 Coins 3 Credits"			},
	{0x17, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"				},
	{0x17, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"			},
	{0x17, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"			},
	{0x17, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"			},
	{0x17, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"			},
	{0x17, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"			},
	{0x17, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"			},
	{0x17, 0x01, 0xf0, 0x00, "Free Play"					},
};

static struct BurnDIPInfo Dip1CoinDIPList[]=
{
	{0   , 0xfe, 0   ,   16, "Coinage"						},
	{0x16, 0x01, 0xf0, 0xa0, "6 Coins 1 Credit"				},
	{0x16, 0x01, 0xf0, 0xb0, "5 Coins 1 Credit"				},
	{0x16, 0x01, 0xf0, 0xc0, "4 Coins 1 Credit"				},
	{0x16, 0x01, 0xf0, 0xd0, "3 Coins 1 Credit"				},
	{0x16, 0x01, 0xf0, 0x10, "8 Coins 3 Credits"			},
	{0x16, 0x01, 0xf0, 0xe0, "2 Coins 1 Credit"				},
	{0x16, 0x01, 0xf0, 0x20, "5 Coins 3 Credits"			},
	{0x16, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"			},
	{0x16, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"				},
	{0x16, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"			},
	{0x16, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"			},
	{0x16, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"			},
	{0x16, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"			},
	{0x16, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"			},
	{0x16, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"			},
	{0x16, 0x01, 0xf0, 0x00, "Free Play"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x17, 0x01, 0x01, 0x01, "Off"							},
	{0x17, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Coin Mode"					},
	{0x17, 0x01, 0x04, 0x04, "Mode 1"						},
	{0x17, 0x01, 0x04, 0x00, "Mode 2"						},

	{0   , 0xfe, 0   ,    2, "Stop Mode"					},
	{0x17, 0x01, 0x20, 0x20, "Off"							},
	{0x17, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x17, 0x01, 0x80, 0x80, "Off"							},
	{0x17, 0x01, 0x80, 0x00, "On"							},
};

static struct BurnDIPInfo RtypeDIPList[]=
{
	{0x16, 0xff, 0xff, 0xfb, NULL							},
	{0x17, 0xff, 0xff, 0xfd, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x03, 0x02, "2"							},
	{0x16, 0x01, 0x03, 0x03, "3"							},
	{0x16, 0x01, 0x03, 0x01, "4"							},
	{0x16, 0x01, 0x03, 0x00, "5"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x16, 0x01, 0x04, 0x04, "Off"							},
	{0x16, 0x01, 0x04, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Bonus Life"					},
	{0x16, 0x01, 0x08, 0x00, "50K 150K 250K 400K 600K"		},
	{0x16, 0x01, 0x08, 0x08, "100K 200K 350K 500K 700K"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x17, 0x01, 0x02, 0x00, "Upright"						},
	{0x17, 0x01, 0x02, 0x02, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Difficulty"					},
	{0x17, 0x01, 0x08, 0x08, "Normal"						},
	{0x17, 0x01, 0x08, 0x00, "Hard"							},

	{0   , 0xfe, 0   ,    2, "Allow Continue"				},
	{0x17, 0x01, 0x10, 0x00, "No"							},
	{0x17, 0x01, 0x10, 0x10, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Invulnerability"				},
	{0x17, 0x01, 0x40, 0x40, "Off"							},
	{0x17, 0x01, 0x40, 0x00, "On"							},
};

STDDIPINFOEXT(Rtype,	Dip1Coin, Rtype	)

static struct BurnDIPInfo RtypepDIPList[]=
{
	{0x16, 0xff, 0xff, 0xfb, NULL							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x16, 0x01, 0x04, 0x04, "On"							},
	{0x16, 0x01, 0x04, 0x00, "Off"							},
};

STDDIPINFOEXT(Rtypep,	Rtype, Rtypep	)

static struct BurnDIPInfo XmultiplDIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL							},
	{0x17, 0xff, 0xff, 0xfd, NULL							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0x03, 0x02, "Very Easy"					},
	{0x16, 0x01, 0x03, 0x01, "Easy"							},
	{0x16, 0x01, 0x03, 0x03, "Normal"						},
	{0x16, 0x01, 0x03, 0x00, "Hard"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x0c, 0x08, "1"							},
	{0x16, 0x01, 0x0c, 0x04, "2"							},
	{0x16, 0x01, 0x0c, 0x0c, "3"							},
	{0x16, 0x01, 0x0c, 0x00, "4"							},

	{0   , 0xfe, 0   ,    4, "Cabinet"						},
	{0x17, 0x01, 0x02, 0x00, "Upright (single)"				},
	{0x17, 0x01, 0x02, 0x02, "Cocktail"						},
	{0x17, 0x01, 0x02, 0x00, "Upright (double) On"			},
	{0x17, 0x01, 0x02, 0x02, "Upright (double) Off"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x17, 0x01, 0x08, 0x00, "Off"							},
	{0x17, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    2, "Upright (double) Mode"		},
	{0x17, 0x01, 0x10, 0x10, "Off"							},
	{0x17, 0x01, 0x10, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Allow Continue"				},
	{0x17, 0x01, 0x20, 0x00, "No"							},
	{0x17, 0x01, 0x20, 0x20, "Yes"							},
};

STDDIPINFOEXT(Xmultipl,	Dip1Coin, Xmultipl	)

static struct BurnDIPInfo DbreedDIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL							},
	{0x17, 0xff, 0xff, 0xf5, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x03, 0x02, "2"							},
	{0x16, 0x01, 0x03, 0x03, "3"							},
	{0x16, 0x01, 0x03, 0x01, "4"							},
	{0x16, 0x01, 0x03, 0x00, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0x0c, 0x00, "Very Easy"					},
	{0x16, 0x01, 0x0c, 0x08, "Easy"							},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"						},
	{0x16, 0x01, 0x0c, 0x04, "Hard"							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x17, 0x01, 0x02, 0x00, "Upright"						},
	{0x17, 0x01, 0x02, 0x02, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x17, 0x01, 0x08, 0x08, "Off"							},
	{0x17, 0x01, 0x08, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Allow Continue"				},
	{0x17, 0x01, 0x10, 0x00, "No"							},
	{0x17, 0x01, 0x10, 0x10, "Yes"							},
};

STDDIPINFOEXT(Dbreed,	Dip1Coin, Dbreed	)

static struct BurnDIPInfo BchopperDIPList[]=
{
	{0x16, 0xff, 0xff, 0xfb, NULL							},
	{0x17, 0xff, 0xff, 0xfd, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x03, 0x00, "1"							},
	{0x16, 0x01, 0x03, 0x02, "2"							},
	{0x16, 0x01, 0x03, 0x03, "3"							},
	{0x16, 0x01, 0x03, 0x01, "4"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x16, 0x01, 0x04, 0x04, "Off"							},
	{0x16, 0x01, 0x04, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Bonus Life"					},
	{0x16, 0x01, 0x08, 0x08, "80K 200K 350K"				},
	{0x16, 0x01, 0x08, 0x00, "100K 250K 400K"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x17, 0x01, 0x02, 0x00, "Upright"						},
	{0x17, 0x01, 0x02, 0x02, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Difficulty"					},
	{0x17, 0x01, 0x08, 0x08, "Normal"						},
	{0x17, 0x01, 0x08, 0x00, "Hard"							},

	{0   , 0xfe, 0   ,    2, "Allow Continue"				},
	{0x17, 0x01, 0x10, 0x00, "No"							},
	{0x17, 0x01, 0x10, 0x10, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Invulnerability"				},
	{0x17, 0x01, 0x40, 0x40, "Off"							},
	{0x17, 0x01, 0x40, 0x00, "On"							},
};

STDDIPINFOEXT(Bchopper,	Dip1Coin, Bchopper	)

static struct BurnDIPInfo NspiritDIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL							},
	{0x17, 0xff, 0xff, 0xf5, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x03, 0x02, "2"							},
	{0x16, 0x01, 0x03, 0x03, "3"							},
	{0x16, 0x01, 0x03, 0x01, "4"							},
	{0x16, 0x01, 0x03, 0x00, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0x0c, 0x08, "Easy"							},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"						},
	{0x16, 0x01, 0x0c, 0x04, "Hard"							},
	{0x16, 0x01, 0x0c, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x17, 0x01, 0x02, 0x00, "Upright"						},
	{0x17, 0x01, 0x02, 0x02, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x17, 0x01, 0x08, 0x08, "Off"							},
	{0x17, 0x01, 0x08, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Allow Continue"				},
	{0x17, 0x01, 0x10, 0x00, "No"							},
	{0x17, 0x01, 0x10, 0x10, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Invulnerability"				},
	{0x17, 0x01, 0x40, 0x40, "Off"							},
	{0x17, 0x01, 0x40, 0x00, "On"							},
};

STDDIPINFOEXT(Nspirit,	Dip1Coin, Nspirit	)

static struct BurnDIPInfo ImgfightDIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL							},
	{0x17, 0xff, 0xff, 0xe5, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x03, 0x02, "2"							},
	{0x16, 0x01, 0x03, 0x03, "3"							},
	{0x16, 0x01, 0x03, 0x01, "4"							},
	{0x16, 0x01, 0x03, 0x00, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"						},
	{0x16, 0x01, 0x0c, 0x08, "Hard"							},
	{0x16, 0x01, 0x0c, 0x04, "Hardest"						},
	{0x16, 0x01, 0x0c, 0x00, "Debug Mode 2 lap"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x17, 0x01, 0x02, 0x00, "Upright"						},
	{0x17, 0x01, 0x02, 0x02, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x17, 0x01, 0x08, 0x08, "Off"							},
	{0x17, 0x01, 0x08, 0x00, "On"							},
};

STDDIPINFOEXT(Imgfight,	Dip1Coin, Imgfight	)

static struct BurnDIPInfo Rtype2DIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL							},
	{0x17, 0xff, 0xff, 0xf7, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x03, 0x02, "2"							},
	{0x16, 0x01, 0x03, 0x03, "3"							},
	{0x16, 0x01, 0x03, 0x01, "4"							},
	{0x16, 0x01, 0x03, 0x00, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0x0c, 0x08, "Easy"							},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"						},
	{0x16, 0x01, 0x0c, 0x04, "Hard"							},
	{0x16, 0x01, 0x0c, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x17, 0x01, 0x02, 0x00, "Off"							},
	{0x17, 0x01, 0x02, 0x02, "On"							},

	{0   , 0xfe, 0   ,    3, "Cabinet"						},
	{0x17, 0x01, 0x18, 0x10, "Upright"						},
	{0x17, 0x01, 0x18, 0x00, "Upright (2P)"					},
	{0x17, 0x01, 0x18, 0x18, "Cocktail"						},
};

STDDIPINFOEXT(Rtype2,	Dip1Coin, Rtype2	)

static struct BurnDIPInfo LohtDIPList[]=
{
	{0x16, 0xff, 0xff, 0xfb, NULL							},
	{0x17, 0xff, 0xff, 0xfd, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x03, 0x00, "2"							},
	{0x16, 0x01, 0x03, 0x03, "3"							},
	{0x16, 0x01, 0x03, 0x02, "4"							},
	{0x16, 0x01, 0x03, 0x01, "5"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x16, 0x01, 0x04, 0x04, "Off"							},
	{0x16, 0x01, 0x04, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x17, 0x01, 0x02, 0x00, "Upright"						},
	{0x17, 0x01, 0x02, 0x02, "Cocktail"						},
	
	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x17, 0x01, 0x18, 0x00, "Easy"							},
	{0x17, 0x01, 0x18, 0x18, "Normal"						},
	{0x17, 0x01, 0x18, 0x10, "Hard"							},
	{0x17, 0x01, 0x18, 0x08, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability"				},
	{0x17, 0x01, 0x40, 0x40, "Off"							},
	{0x17, 0x01, 0x40, 0x00, "On"							},
};

STDDIPINFOEXT(Loht,	Dip1Coin, Loht		)

static struct BurnDIPInfo AirduelDIPList[]=
{
	{0x16, 0xff, 0xff, 0xbf, NULL							},
	{0x17, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x03, 0x02, "2"							},
	{0x16, 0x01, 0x03, 0x03, "3"							},
	{0x16, 0x01, 0x03, 0x01, "4"							},
	{0x16, 0x01, 0x03, 0x00, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0x0c, 0x00, "Very Easy"					},
	{0x16, 0x01, 0x0c, 0x08, "Easy"							},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"						},
	{0x16, 0x01, 0x0c, 0x04, "Hard"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x17, 0x01, 0x01, 0x01, "Off"							},
	{0x17, 0x01, 0x01, 0x00, "On"							},
};

STDDIPINFOEXT(Airduel,	Dip2Coin, Airduel	)

static struct BurnDIPInfo GallopDIPList[]=
{
	{0x16, 0xff, 0xff, 0xbf, NULL							},
	{0x17, 0xff, 0xff, 0xf9, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x03, 0x02, "2"							},
	{0x16, 0x01, 0x03, 0x03, "3"							},
	{0x16, 0x01, 0x03, 0x01, "4"							},
	{0x16, 0x01, 0x03, 0x00, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0x0c, 0x00, "Very Easy"					},
	{0x16, 0x01, 0x0c, 0x08, "Easy"							},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"						},
	{0x16, 0x01, 0x0c, 0x04, "Hard"							},

	{0   , 0xfe, 0   ,    2, "Allow Continue"				},
	{0x16, 0x01, 0x20, 0x00, "No"							},
	{0x16, 0x01, 0x20, 0x20, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x17, 0x01, 0x01, 0x01, "Off"							},
	{0x17, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    3, "Cabinet"						},
	{0x17, 0x01, 0x06, 0x00, "Upright"						},
	{0x17, 0x01, 0x06, 0x02, "Upright (2P)"					},
	{0x17, 0x01, 0x06, 0x06, "Cocktail"						},
};

STDDIPINFOEXT(Gallop,	Dip2Coin, Gallop	)

static struct BurnDIPInfo KengoDIPList[]=
{
	{0x16, 0xff, 0xff, 0xbf, NULL							},
	{0x17, 0xff, 0xff, 0xf9, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x03, 0x02, "2"							},
	{0x16, 0x01, 0x03, 0x03, "3"							},
	{0x16, 0x01, 0x03, 0x01, "4"							},
	{0x16, 0x01, 0x03, 0x00, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0x0c, 0x00, "Very Easy"					},
	{0x16, 0x01, 0x0c, 0x08, "Easy"							},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"						},
	{0x16, 0x01, 0x0c, 0x04, "Hard"							},

	{0   , 0xfe, 0   ,    2, "Allow Continue"				},
	{0x16, 0x01, 0x20, 0x00, "No"							},
	{0x16, 0x01, 0x20, 0x20, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x17, 0x01, 0x01, 0x01, "Off"							},
	{0x17, 0x01, 0x01, 0x00, "On"							},
};

STDDIPINFOEXT(Kengo,	Dip2Coin, Kengo	)

static struct BurnDIPInfo HharryDIPList[]=
{
	{0x16, 0xff, 0xff, 0xbf, NULL							},
	{0x17, 0xff, 0xff, 0xfd, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x16, 0x01, 0x03, 0x02, "2"							},
	{0x16, 0x01, 0x03, 0x03, "3"							},
	{0x16, 0x01, 0x03, 0x01, "4"							},
	{0x16, 0x01, 0x03, 0x00, "5"							},
	
	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x16, 0x01, 0x0c, 0x00, "Very Easy"					},
	{0x16, 0x01, 0x0c, 0x08, "Easy"							},
	{0x16, 0x01, 0x0c, 0x0c, "Normal"						},
	{0x16, 0x01, 0x0c, 0x04, "Hard"							},

	{0   , 0xfe, 0   ,    2, "Continue Limit"				},
	{0x16, 0x01, 0x10, 0x00, "No"							},
	{0x16, 0x01, 0x10, 0x10, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Allow Continue"				},
	{0x16, 0x01, 0x20, 0x00, "No"							},
	{0x16, 0x01, 0x20, 0x20, "Yes"							},

	{0   , 0xfe, 0   ,    3, "Cabinet"						},
	{0x17, 0x01, 0x06, 0x04, "Upright"						},
	{0x17, 0x01, 0x06, 0x00, "Upright (2P)"					},
	{0x17, 0x01, 0x06, 0x06, "Cocktail"						},
};

STDDIPINFOEXT(Hharry,	Dip2Coin, Hharry	)

static struct BurnDIPInfo PoundforDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xbf, NULL							},
	{0x0f, 0xff, 0xff, 0xfd, NULL							},

	{0   , 0xfe, 0   ,    4, "Round Time"					},
	{0x0e, 0x01, 0x03, 0x02, "60"							},
	{0x0e, 0x01, 0x03, 0x03, "90"							},
	{0x0e, 0x01, 0x03, 0x01, "120"							},
	{0x0e, 0x01, 0x03, 0x00, "150"							},

	{0   , 0xfe, 0   ,    2, "Matches/Credit (2P)"			},
	{0x0e, 0x01, 0x04, 0x04, "1"							},
	{0x0e, 0x01, 0x04, 0x00, "2"							},

	{0   , 0xfe, 0   ,    2, "Rounds/Match"					},
	{0x0e, 0x01, 0x08, 0x08, "2"							},
	{0x0e, 0x01, 0x08, 0x00, "3"							},

	{0   , 0xfe, 0   ,    2, "Difficulty"					},
	{0x0e, 0x01, 0x10, 0x10, "Normal"						},
	{0x0e, 0x01, 0x10, 0x00, "Hard"							},

	{0   , 0xfe, 0   ,    2, "Trackball Size"				},
	{0x0e, 0x01, 0x20, 0x20, "Small"						},
	{0x0e, 0x01, 0x20, 0x00, "Large"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x0e, 0x01, 0x40, 0x40, "Off"							},
	{0x0e, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x0e, 0x01, 0x80, 0x80, "Off"							},
	{0x0e, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x0f, 0x01, 0x01, 0x01, "Off"							},
	{0x0f, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    3, "Cabinet"						},
	{0x0f, 0x01, 0x06, 0x04, "Upright"						},
	{0x0f, 0x01, 0x06, 0x02, "Upright (2P)"					},
	{0x0f, 0x01, 0x06, 0x06, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Coin Mode"					},
	{0x0f, 0x01, 0x08, 0x08, "Mode 1"						},
	{0x0f, 0x01, 0x08, 0x00, "Mode 2"						},
															
	{0   , 0xfe, 0   ,    16, "Coinage"						},
	{0x0f, 0x01, 0xf0, 0xa0, "6 Coins 1 Credit"				},
	{0x0f, 0x01, 0xf0, 0xb0, "5 Coins 1 Credit"				},
	{0x0f, 0x01, 0xf0, 0xc0, "4 Coins 1 Credit"				},
	{0x0f, 0x01, 0xf0, 0xd0, "3 Coins 1 Credit"				},
	{0x0f, 0x01, 0xf0, 0xe0, "2 Coins 1 Credit"				},
	{0x0f, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"			},
	{0x0f, 0x01, 0xf0, 0x20, "4 Coins 3 Credits"			},
	{0x0f, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"				},
	{0x0f, 0x01, 0xf0, 0x10, "1 Coin/1 Credit, 1 Coin/Cont."},
	{0x0f, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"			},
	{0x0f, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"			},
	{0x0f, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"			},
	{0x0f, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"			},
	{0x0f, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"			},
	{0x0f, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"			},
	{0x0f, 0x01, 0xf0, 0x00, "Free Play"					},
};

STDDIPINFO(Poundfor)

static UINT8 mcu_cmd = 0;
static UINT8 mcu_to_snd = 0;

// mcu jib
static UINT8 mcu_read_port(INT32 port)
{
	if (port == 0) {
		UINT8 ret =  DrvSndROM[sample_address & 0x3ffff];
		sample_address = (sample_address + 1) & 0x3ffff;
		return ret;
	}

	if (port == 2) {
		return mcu_cmd;
	}

	if (port >= 0xc000 && port <= 0xcfff) {
		if ((port & 0xfff) == 0xfff) { // latch on ffe or fff? -dink
			mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_NONE);
		}
		return DrvProtRAM[port & 0xfff];
	}

	return 0x00;
}

static void mcu_write_port(INT32 port, UINT8 data)
{
	if (port == MCS51_PORT_P1) {
		if (ZetGetActive() == -1) return; // mcs51_reset() writes here, (z80 is not active yet)
		DACSignedWrite(0, data);
		return;
	}

	if (port == 0) {
		sample_address = (sample_address & ~0x1fff) | (data << 5);
		return;
	}

	if (port == 1) {
		sample_address = (sample_address & 0x1fff) | (data << 13);
		return;
	}

	if (port == 2) {
		mcs51_set_irq_line(MCS51_INT1_LINE, CPU_IRQSTATUS_NONE);
		return;
	}

	if (port >= 0xc000 && port <= 0xcfff) {
		DrvProtRAM[port & 0xfff] = data;
		return;
	}
}

static void DrvMCUReset()
{
	mcu_to_snd = 0;
	mcu_cmd = 0;
	mcs51_reset();
}

static void DrvMCUInit()
{
	mcs51_init();
	mcs51_set_program_data(DrvMcuROM);
	mcs51_set_write_handler(mcu_write_port);
	mcs51_set_read_handler(mcu_read_port);

	DrvMCUReset();
}

static void DrvMCUExit() {
	mcs51_exit();
}

static INT32 DrvMCURun(INT32 cycles)
{
	return mcs51Run(cycles);
}

static void DrvMCUSync()
{
	INT32 todo = ((INT64)((double)VezTotalCycles() * ((double)mcu_mhz / 12)) / main_mhz) - mcs51TotalCycles();

	if (todo > 0) DrvMCURun(todo);
}

static INT32 DrvMCUScan(INT32 nAction)
{
	mcs51_scan(nAction);

	SCAN_VAR(mcu_to_snd);
	SCAN_VAR(mcu_cmd);

	return 0;
}


//--------------------------------------------------------------------------------------------------------------------------------------------------
// Protection handlers

static const UINT8 *protection_code = NULL;
static const UINT8 *protection_crc = NULL;
static const INT32 *protection_sample_offsets = NULL;

static UINT8 protection_read(INT32 address)
{
	if (use_mcu) {
		DrvMCUSync();
		return DrvProtRAM[(address) & 0xfff];
	}

	if (address == 0xb0ffb) {
		if (protection_code != NULL) {
			memcpy (DrvProtRAM, protection_code, 96);
		}
	}

	return DrvProtRAM[address & 0xfff];
}

static void protection_write(INT32 address, UINT8 data)
{
	if (use_mcu) {
		DrvMCUSync();
		if ((address & 0xfff) == 0xffe) {
			mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_ACK);
		}
		DrvProtRAM[(address) & 0xfff] = data;
		return;
	}

	DrvProtRAM[address & 0xfff] = data ^ 0xff;

	if (address == 0xb0fff && data == 0) {
		if (protection_crc != NULL) {
			memcpy (DrvProtRAM + 0xfe0, protection_crc, 18);
		}
	}
}

static void protection_sample_offset_write(UINT8 data) // 0xc0
{
	if (use_mcu) {
		if (airduelm72) {
			DrvMCUSync();
			mcu_cmd = data;
			mcs51_set_irq_line(MCS51_INT1_LINE, CPU_IRQSTATUS_ACK);
		} else {
			mcu_to_snd = data;
			ZetNmi(0);
		}
		return;
	}

	if (protection_sample_offsets != NULL) {
		if (data < protection_sample_offsets[0]) {
			sample_address = protection_sample_offsets[data+1];
		}
	}
}

static void m72_install_protection(const UINT8 *code, const UINT8 *crc, const INT32 *offs)
{
	protection_code = code;
	protection_crc = crc;
	protection_sample_offsets = offs;
}

#define install_protection(name) m72_install_protection(name##_code, name##_crc, name##_sample_offsets)

/* Dragon Breed */
static const INT32 dbreedm72_sample_offsets[10] = { 9, 0x00000, 0x00020, 0x02c40, 0x08160, 0x0c8c0, 0x0ffe0, 0x13000, 0x15820, 0x15f40 };

static const UINT8 dbreedm72_code[96] = {
	0xea,0x6c,0x00,0x00,0x00	// jmp  0000:$006c
};

static const UINT8 dbreedm72_crc[18] ={
	0xa4,0x96,0x5f,0xc0, 0xab,0x49,0x9f,0x19,0x84,0xe6,0xd6,0xca,0x00,0x00
};

/* Ninja Spirit */
static const INT32 nspirit_sample_offsets[10] = { 9, 0x0000, 0x0020, 0x2020, 0, 0x5720, 0, 0x7b60, 0x9b60, 0xc360 };

static const UINT8 nspirit_code[96] =
{
	0x68,0x00,0xa0,			// push 0a000h
	0x1f,				// pop ds
	0xc6,0x06,0x38,0x38,0x4e,	// mov [3838h], byte 04eh
	0xc6,0x06,0x3a,0x38,0x49,	// mov [383ah], byte 049h
	0xc6,0x06,0x3c,0x38,0x4e,	// mov [383ch], byte 04eh
	0xc6,0x06,0x3e,0x38,0x44,	// mov [383eh], byte 044h
	0xc6,0x06,0x40,0x38,0x4f,	// mov [3840h], byte 04fh
	0xc6,0x06,0x42,0x38,0x55,	// mov [3842h], byte 055h
	0x68,0x00,0xb0,			// push 0b000h
	0x1f,				// pop ds
	0xc6,0x06,0x00,0x09,0x49^0xff,	// mov [0900h], byte 049h
	0xc6,0x06,0x00,0x0a,0x49^0xff,	// mov [0a00h], byte 049h
	0xc6,0x06,0x00,0x0b,0x49^0xff,	// mov [0b00h], byte 049h
	0x68,0x00,0xd0,			// push 0d000h
	0x1f,				// pop ds
	// the following is for nspiritj only, the game checks for
	// "This game can only be played in Japan..." message in the video text buffer
	// the message is nowhere to be found in the ROMs, so has to be displayed by the mcu
	0xc6,0x06,0x70,0x16,0x57,	// mov [1670h], byte 057h
	0xc6,0x06,0x71,0x16,0x00,	// mov [1671h], byte 000h
	0xea,0x00,0x00,0x40,0x00	// jmp  0040:$0000
};

static const UINT8 nspirit_crc[18] = {
	0xfe,0x94,0x6e,0x4e, 0xc8,0x33,0xa7,0x2d,0xf2,0xa3,0xf9,0xe1, 0xa9,0x6c,0x02,0x95, 0x00,0x00
};

/* Daiku no Gensan */
static const INT32 dkgenm72_sample_offsets[29] = {
	28,
	0x00000, 0x00020, 0x01800, 0x02da0, 0x03be0, 0x05ae0, 0x06100, 0x06de0,
	0x07260, 0x07a60, 0x08720, 0x0a5c0, 0x0c3c0, 0x0c7a0, 0x0e140, 0x0fb00,
	0x10fa0, 0x10fc0, 0x10fe0, 0x11f40, 0x12b20, 0x130a0, 0x13c60, 0x14740,
	0x153c0, 0x197e0, 0x1af40, 0x1c080 };

static const UINT8 dkgenm72_code[96] = {
	0xea,0x3d,0x00,0x00,0x10	// jmp  1000:$003d
};

static const UINT8 dkgenm72_crc[18] = {
	0xc8,0xb4,0xdc,0xf8, 0xd3,0xba,0x48,0xed,0x79,0x08,0x1c,0xb3, 0x00,0x00
};

/* Legend of Hero Tonma */
static const INT32 loht_sample_offsets[8] = { 7, 0x0000, 0x0020, 0, 0x2c40, 0x4320, 0x7120, 0xb200 };

static const UINT8 loht_code[96] =
{
	0x68,0x00,0xa0,			// push 0a000h
	0x1f,				// pop ds
	0xc6,0x06,0x3c,0x38,0x47,	// mov [383ch], byte 047h
	0xc6,0x06,0x3d,0x38,0x47,	// mov [383dh], byte 047h
	0xc6,0x06,0x42,0x38,0x44,	// mov [3842h], byte 044h
	0xc6,0x06,0x43,0x38,0x44,	// mov [3843h], byte 044h
	0x68,0x00,0xb0,			// push 0b000h
	0x1f,				// pop ds
	0xc6,0x06,0x00,0x09,0x49^0xff,	// mov [0900h], byte 049h
	0xc6,0x06,0x00,0x0a,0x49^0xff,	// mov [0a00h], byte 049h
	0xc6,0x06,0x00,0x0b,0x49^0xff,	// mov [0b00h], byte 049h

	0x68,0x00,0xd0,			// push 0d000h // Japan set only
	0x1f,				// pop ds // Japan set only
	0xc6,0x06,0x70,0x16,0x57,	// mov [1670h], byte 057h // Japan set only - checks this (W) of WARNING

	0xea,0x5d,0x01,0x40,0x00	// jmp  0040:$015d
};

static const UINT8 loht_crc[18] = {
	0x39,0x00,0x82,0xae, 0x2c,0x9d,0x4b,0x73,0xfb,0xac,0xd4,0x6d, 0x6d,0x5b,0x77,0xc0, 0x00,0x00
};

//--------------------------------------------------------------------------------------------------------------------------------------------------


static void setvector_callback(INT32 param)
{
	switch (param)
	{
		case VECTOR_INIT:   irqvector  = 0xff; break;
		case YM2151_ASSERT: irqvector &= 0xef; break;
		case YM2151_CLEAR:  irqvector |= 0x10; break;
		case Z80_ASSERT:    irqvector &= 0xdf; break;
		case Z80_CLEAR:     irqvector |= 0x20; break;
	}

	if (irqvector == 0xff) {
		ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
	} else {
		ZetSetVector(irqvector);
		ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
	}
}

static void palette_write(INT32 offset, INT32 offset2)
{
	if (offset & 1) return;

	UINT16 *pal = (UINT16*)DrvPalRAM;
	offset = (offset / 2) & 0x00ff;

	INT32 offset3 = offset;
	if (offset2) {
		offset3 |= 0x0100;
		pal += 0x1000 / 2;
	}

	INT32 r = BURN_ENDIAN_SWAP_INT16(pal[offset + 0x000]) & 0x1f;
	INT32 g = BURN_ENDIAN_SWAP_INT16(pal[offset + 0x200]) & 0x1f;
	INT32 b = BURN_ENDIAN_SWAP_INT16(pal[offset + 0x400]) & 0x1f;

	DrvPalette[offset3] = BurnHighCol((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2), 0);
}

static UINT8 __fastcall m72_main_read(UINT32 address)
{
	if ((address & 0xff000) == 0xb0000) {
		return protection_read(address);
	}

	return 0;
}

static void __fastcall m72_main_write(UINT32 address, UINT8 data)
{
	if ((address & 0xff000) == 0xb0000) {
		protection_write(address, data);
		return;
	}

	if ((address & 0xff000) == 0xc8000) {
		if (address & 1) data = 0xff;
  		DrvPalRAM[(address & 0xdff) | 0x0000] = DrvPalRAM[(address & 0xdff) | 0x0200] = data | 0xe0;
		palette_write(address, 0);
		return;
	}

	if ((address & 0xff000) == 0xcc000) {
		if (address & 1) data = 0xff;
  		DrvPalRAM[(address & 0xdff) | 0x1000] = DrvPalRAM[(address & 0xdff) | 0x1200] = data | 0xe0;
		palette_write(address, 1);
		return;
	}
}

static void __fastcall rtype2_main_write(UINT32 address, UINT8 data)
{
	if ((address & 0xff000) == 0xc8000 || (address & 0xff000) == 0xa0000 || (address & 0xff000) == 0xcc000) {
		if (address & 1) data = 0xff;
  		DrvPalRAM[(address & 0xdff) | 0x0000] = DrvPalRAM[(address & 0xdff) | 0x0200] = data | 0xe0;
		palette_write(address, 0);
		return;
	}

	if ((address & 0xff000) == 0xd8000 || (address & 0xff000) == 0xa8000 || (address & 0xff000) == 0xa4000) {
		if (address & 1) data = 0xff;
  		DrvPalRAM[(address & 0xdff) | 0x1000] = DrvPalRAM[(address & 0xdff) | 0x1200] = data | 0xe0;
		palette_write(address, 1);
		return;
	}

	switch (address)
	{
		case 0xb0000:
		case 0xe0000: // majtitle
			irq_raster_position = (irq_raster_position & 0xff00) | (data << 0);
		return;

		case 0xb0001:
		case 0xe0001: // majtitle
			irq_raster_position = (irq_raster_position & 0x00ff) | (data << 8);
		return;

		case 0xbc000:
	//	case 0xbc001:
		case 0xec000: // majtitle
	//	case 0xec001:
			memcpy (DrvSprBuf, DrvSprRAM, 0x0400);
		return;
	}
}

static void __fastcall m72_main_write_port(UINT32 port, UINT8 data)
{
	//if (port!=0) bprintf (0, _T("%2.2x, %2.2x wp\n"), port, data);

	switch (port)
	{
		case 0x00:
			*soundlatch = data;
			setvector_callback(Z80_ASSERT);
		return;

		case 0x01:
		return;

		case 0x02:
//		case 0x03:
		{
			// coin counter = data & 3 (&1 = 0, &2 = 1)
			// flipscreen = ((data & 0x04) >> 2) ^ ((~input_port_read(space->machine, "DSW") >> 8) & 1);

			video_disable[0] = data & 0x08;

			if (enable_z80_reset) {
				if (data & 0x10) {
					z80_reset = 0;
					ZetSetRESETLine(0);
				} else if (!z80_reset) { // don't reset it if its already resetting - fixes BGM in airduel -dink
					ZetSetRESETLine(1);
					setvector_callback(VECTOR_INIT);
					z80_reset = 1;
				}
			}
		}
		return;

		case 0x04:
		case 0x05:
			memcpy (DrvSprBuf, DrvSprRAM, 0x0400);
		return;

		case 0x06:
			irq_raster_position = (irq_raster_position & 0xff00) | (data << 0);
		return;

		case 0x07:
			irq_raster_position = (irq_raster_position & 0x00ff) | (data << 8);
		return;

		case 0x40:
		case 0x41:
		case 0x43: // nop
		return;

		case 0x42:
			if (m72_irq_base == 0) {
				m72_irq_base = data << 2;
				//bprintf(0, _T("irq base vector %X.\n"), m72_irq_base);
			}
		return;

		case 0x80:
		case 0x81:// scrolly1
		case 0x82:
		case 0x83:// scrollx1
		case 0x84:
		case 0x85:// scrolly2
		case 0x86:
		case 0x87:// scrollx2
			scroll[port & 0x07] = data;
		return;

	//	case 0x8e:
		case 0x8f:
			majtitle_rowscroll_enable = data ? 1 : 0;
		return;

		case 0xc0:
			protection_sample_offset_write(data);
		return;
	}
}

static UINT16 __fastcall poundfor_trackball_r(INT32 port)
{
	static INT32 prev[4] = { 0, 0, 0, 0 };
	static INT32 diff[4] = { 0, 0, 0, 0 };

	INT32 offset = (port / 2) & 0x03;

	BurnTrackballUpdate(0);
	BurnTrackballUpdate(1);

	INT16 axis[4] = { (INT16)BurnTrackballReadWord(0, 0), (INT16)BurnTrackballReadWord(0, 1), (INT16)BurnTrackballReadWord(1, 0), (INT16)BurnTrackballReadWord(1, 1) };

	if (port == 8)
	{
		INT32 curr;

		for (INT32 i = 0; i < 4; i++)
		{
			curr = axis[i];
			diff[i] = (curr - prev[i]);
			prev[i] = curr;
		}
	}
	INT32 input = DrvInputs[0] | (DrvInputs[3] << 8);

	switch (offset)
	{
		default:
		case 0:
			return (diff[0] & 0xff) | ((diff[2] & 0xff) << 8);
		case 1:
			return ((diff[0] >> 8) & 0x1f) | (diff[2] & 0x1f00) | (~input & 0xe0e0);
		case 2:
			return (diff[1] & 0xff) | ((diff[3] & 0xff) << 8);
		case 3:
			return ((diff[1] >> 8) & 0x1f) | (diff[3] & 0x1f00);
	}

	return 0;
}

static UINT8 __fastcall m72_main_read_port(UINT32 port)
{
	if (Poundfor) {
		if ((port & 0xf8) == 0x08) {
			UINT16 ret = poundfor_trackball_r(port);
			return (ret >> ((port & 1) * 8)) & 0xff;
		}

		switch (port)
		{
			case 0x02: return DrvInputs[1];
			case 0x03: return 0xff;
			case 0x04: return DrvDips[0];
			case 0x05: return DrvDips[1];
		}

		return 0;
	}

	switch (port)
	{
		case 0x00: return DrvInputs[0];
		case 0x01: return DrvInputs[1];
		case 0x02: return DrvInputs[2];
		case 0x03: return 0xff;
		case 0x04: return DrvDips[0];
		case 0x05: return DrvDips[1];
	}

	return 0;
}

static void __fastcall m72_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x40: // poundfor
		case 0x01:
		case 0x41: // poundfor
			BurnYM2151Write(port & 1, data);
		return;

		case 0x06:
		case 0x42: // poundfor
		case 0x83: // rtype2
			setvector_callback(Z80_CLEAR);
		return;

		case 0x10: // poundfor
			sample_address >>= 4;
			sample_address = (sample_address & 0xff00) | (data << 0);
			sample_address <<= 4;
		return;

		case 0x11: // poundfor
			sample_address >>= 4;
			sample_address = (sample_address & 0x00ff) | (data << 8);
			sample_address <<= 4;
		return;

		case 0x12: // poundfor "sample end address" - not used
		case 0x13:
		return;

		case 0x80: // rtype2
			sample_address >>= 5;
			sample_address = (sample_address & 0xff00) | (data << 0);
			sample_address <<= 5;
		return;

		case 0x81: // rtype2
			sample_address >>= 5;
			sample_address = (sample_address & 0x00ff) | (data << 8);
			sample_address <<= 5;
		return;

		case 0x82:
			if (use_mcu) {
				DrvMCUSync();
				mcu_cmd = data;
				mcs51_set_irq_line(MCS51_INT1_LINE, CPU_IRQSTATUS_ACK);
				return;
			}

			DACSignedWrite(0, data);
			sample_address = (sample_address + 1) & 0x3ffff;
			if (!DrvSndROM[sample_address]) {
				DACWrite(0, 0); // clear dac @ end of sample, fixes distortion in rtype2 level4 after death while also killing an air-tank
			}
		return;
	}
}

static UINT8 __fastcall m72_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x40: // poundfor
		case 0x41: // poundfor
			return BurnYM2151Read();

		case 0x80: // rtype2
		case 0x42: // poundfor
		case 0x02:
			return *soundlatch;

		case 0x84:
			if (use_mcu) {
				return mcu_to_snd;
			}
			return DrvSndROM[sample_address & 0x3ffff];
	}

	return 0;
}

static void m72YM2151IRQHandler(INT32 nStatus)
{
	setvector_callback(nStatus ? YM2151_ASSERT : YM2151_CLEAR);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	VezOpen(0);
	VezReset();
	VezClose();

	ZetOpen(0);
	ZetReset();
	setvector_callback(VECTOR_INIT);
	z80_reset = (enable_z80_reset) ? 1 : 0;
	ZetSetRESETLine(z80_reset);
	BurnYM2151Reset();
	DACReset();
	ZetClose();

	if (use_mcu) DrvMCUReset();

	HiscoreReset();

	sample_address = 0;
	irq_raster_position = -1;
	if (!CosmicCop) m72_irq_base = 0;
	majtitle_rowscroll_enable = 0;

	nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = 0;

	return 0;
}

static void common_main_cpu_map(INT32 romaddr, INT32 ramaddr)
{
	VezInit(0, V30_TYPE);

	VezOpen(0);
	VezMapArea(0x00000, (romaddr-1), 0, DrvV30ROM + 0x000000);
	VezMapArea(0x00000, (romaddr-1), 2, DrvV30ROM + 0x000000);
	VezMapArea(0x00000+ramaddr, 0x03fff+ramaddr, 0, DrvV30RAM);
	VezMapArea(0x00000+ramaddr, 0x03fff+ramaddr, 1, DrvV30RAM);
	VezMapArea(0x00000+ramaddr, 0x03fff+ramaddr, 2, DrvV30RAM);
//	VezMapArea(0xb0000, 0xb0fff, 2, DrvProtRAM);
	VezMapArea(0xc0000, 0xc0fff, 0, DrvSprRAM);
	VezMapArea(0xc0000, 0xc0fff, 1, DrvSprRAM);
	VezMapArea(0xc0000, 0xc0fff, 2, DrvSprRAM);
	VezMapArea(0xc8000, 0xc8fff, 0, DrvPalRAM);
//	VezMapArea(0xc8000, 0xc8fff, 1, DrvPalRAM);
	VezMapArea(0xc8000, 0xc8fff, 2, DrvPalRAM);
	VezMapArea(0xcc000, 0xccfff, 0, DrvPalRAM + 0x1000);
//	VezMapArea(0xcc000, 0xccfff, 1, DrvPalRAM + 0x1000);
	VezMapArea(0xcc000, 0xccfff, 2, DrvPalRAM + 0x1000);
	VezMapArea(0xd0000, 0xd3fff, 0, DrvVidRAM0);
	VezMapArea(0xd0000, 0xd3fff, 1, DrvVidRAM0);
	VezMapArea(0xd0000, 0xd3fff, 2, DrvVidRAM0);
	VezMapArea(0xd8000, 0xdbfff, 0, DrvVidRAM1);
	VezMapArea(0xd8000, 0xdbfff, 1, DrvVidRAM1);
	VezMapArea(0xd8000, 0xdbfff, 2, DrvVidRAM1);
	VezMapArea(0xe0000, 0xeffff, 0, DrvZ80RAM);
	VezMapArea(0xe0000, 0xeffff, 1, DrvZ80RAM);
	VezMapArea(0xe0000, 0xeffff, 2, DrvZ80RAM);
	VezMapArea(0xff800, 0xfffff, 0, DrvV30ROM + 0x0ff800);
	VezMapArea(0xff800, 0xfffff, 2, DrvV30ROM + 0x0ff800);
	VezSetReadHandler(m72_main_read);
	VezSetWriteHandler(m72_main_write);
	VezSetReadPort(m72_main_read_port);
	VezSetWritePort(m72_main_write_port);
	VezClose();
}

static void common_040000_040000() { common_main_cpu_map(0x40000, 0x40000); }
static void common_080000_080000() { common_main_cpu_map(0x80000, 0x80000); }
static void common_080000_088000() { common_main_cpu_map(0x80000, 0x88000); }
static void common_080000_090000() { common_main_cpu_map(0x80000, 0x90000); }
static void common_080000_09c000() { common_main_cpu_map(0x80000, 0x9c000); }
static void common_080000_0a0000() { common_main_cpu_map(0x80000, 0xa0000); }

static void rtype2_main_cpu_map()
{
	VezInit(0, V30_TYPE);

	VezOpen(0);
	VezMapArea(0x00000, 0x7ffff, 0, DrvV30ROM + 0x000000);
	VezMapArea(0x00000, 0x7ffff, 2, DrvV30ROM + 0x000000);
	VezMapArea(0xc0000, 0xc0fff, 0, DrvSprRAM);
	VezMapArea(0xc0000, 0xc0fff, 1, DrvSprRAM);
	VezMapArea(0xc0000, 0xc0fff, 2, DrvSprRAM);
	VezMapArea(0xc8000, 0xc8fff, 0, DrvPalRAM);
//	VezMapArea(0xc8000, 0xc8fff, 1, DrvPalRAM);
	VezMapArea(0xc8000, 0xc8fff, 2, DrvPalRAM);
	VezMapArea(0xd0000, 0xd3fff, 0, DrvVidRAM0);
	VezMapArea(0xd0000, 0xd3fff, 1, DrvVidRAM0);
	VezMapArea(0xd0000, 0xd3fff, 2, DrvVidRAM0);
	VezMapArea(0xd4000, 0xd7fff, 0, DrvVidRAM1);
	VezMapArea(0xd4000, 0xd7fff, 1, DrvVidRAM1);
	VezMapArea(0xd4000, 0xd7fff, 2, DrvVidRAM1);
	VezMapArea(0xd8000, 0xd8fff, 0, DrvPalRAM + 0x1000);
//	VezMapArea(0xd8000, 0xd8fff, 1, DrvPalRAM + 0x1000);
	VezMapArea(0xd8000, 0xd8fff, 2, DrvPalRAM + 0x1000);
	VezMapArea(0xe0000, 0xe3fff, 0, DrvV30RAM);
	VezMapArea(0xe0000, 0xe3fff, 1, DrvV30RAM);
	VezMapArea(0xe0000, 0xe3fff, 2, DrvV30RAM);
	VezMapArea(0xff800, 0xfffff, 0, DrvV30ROM + 0x0ff800);
	VezMapArea(0xff800, 0xfffff, 2, DrvV30ROM + 0x0ff800);
	VezSetReadHandler(m72_main_read);
	VezSetWriteHandler(rtype2_main_write);
	VezSetReadPort(m72_main_read_port);
	VezSetWritePort(m72_main_write_port);
	VezClose();
}

static void hharryu_main_cpu_map()
{
	VezInit(0, V35_TYPE); // cosmiccop / kengo v35

	VezOpen(0);
	VezMapArea(0x00000, 0x7ffff, 0, DrvV30ROM + 0x000000);
	VezMapArea(0x00000, 0x7ffff, 2, DrvV30ROM + 0x000000);
	VezMapArea(0x80000, 0x83fff, 0, DrvVidRAM0); // kengo
	VezMapArea(0x80000, 0x83fff, 1, DrvVidRAM0);
	VezMapArea(0x80000, 0x83fff, 2, DrvVidRAM0);
	VezMapArea(0x84000, 0x87fff, 0, DrvVidRAM1); // kengo
	VezMapArea(0x84000, 0x87fff, 1, DrvVidRAM1);
	VezMapArea(0x84000, 0x87fff, 2, DrvVidRAM1);
	VezMapArea(0xa0000, 0xa0fff, 0, DrvPalRAM);
//	VezMapArea(0xa0000, 0xa0fff, 1, DrvPalRAM);
	VezMapArea(0xa0000, 0xa0fff, 2, DrvPalRAM);
	VezMapArea(0xa8000, 0xa8fff, 0, DrvPalRAM + 0x1000);
//	VezMapArea(0xa8000, 0xa8fff, 1, DrvPalRAM + 0x1000);
	VezMapArea(0xa8000, 0xa8fff, 2, DrvPalRAM + 0x1000);
	VezMapArea(0xc0000, 0xc0fff, 0, DrvSprRAM);
	VezMapArea(0xc0000, 0xc0fff, 1, DrvSprRAM);
	VezMapArea(0xc0000, 0xc0fff, 2, DrvSprRAM);
	VezMapArea(0xd0000, 0xd3fff, 0, DrvVidRAM0);
	VezMapArea(0xd0000, 0xd3fff, 1, DrvVidRAM0);
	VezMapArea(0xd0000, 0xd3fff, 2, DrvVidRAM0);
	VezMapArea(0xd4000, 0xd7fff, 0, DrvVidRAM1);
	VezMapArea(0xd4000, 0xd7fff, 1, DrvVidRAM1);
	VezMapArea(0xd4000, 0xd7fff, 2, DrvVidRAM1);
	VezMapArea(0xe0000, 0xe3fff, 0, DrvV30RAM);
	VezMapArea(0xe0000, 0xe3fff, 1, DrvV30RAM);
	VezMapArea(0xe0000, 0xe3fff, 2, DrvV30RAM);
	VezMapArea(0xff800, 0xfffff, 0, DrvV30ROM + 0x0ff800);
	VezMapArea(0xff800, 0xfffff, 2, DrvV30ROM + 0x0ff800);
	VezSetReadHandler(m72_main_read);
	VezSetWriteHandler(rtype2_main_write);
	VezSetReadPort(m72_main_read_port);
	VezSetWritePort(m72_main_write_port);
	VezClose();
}

static void majtitle_main_cpu_map()
{
	VezInit(0, V30_TYPE);

	VezOpen(0);
	VezMapArea(0x00000, 0x7ffff, 0, DrvV30ROM + 0x000000);
	VezMapArea(0x00000, 0x7ffff, 2, DrvV30ROM + 0x000000);
	VezMapArea(0xa0000, 0xa07ff, 0, DrvRowScroll);
	VezMapArea(0xa0000, 0xa07ff, 1, DrvRowScroll);
	VezMapArea(0xa0000, 0xa07ff, 2, DrvRowScroll);
	VezMapArea(0xa4000, 0xa4fff, 0, DrvPalRAM + 0x1000);
//	VezMapArea(0xa4000, 0xa4fff, 1, DrvPalRAM + 0x1000);
	VezMapArea(0xa4000, 0xa4fff, 2, DrvPalRAM + 0x1000);
	VezMapArea(0xac000, 0xaffff, 0, DrvVidRAM0);
	VezMapArea(0xac000, 0xaffff, 1, DrvVidRAM0);
	VezMapArea(0xac000, 0xaffff, 2, DrvVidRAM0);
	VezMapArea(0xb0000, 0xbffff, 0, DrvVidRAM1);
	VezMapArea(0xb0000, 0xbffff, 1, DrvVidRAM1);
	VezMapArea(0xb0000, 0xbffff, 2, DrvVidRAM1);
	VezMapArea(0xc0000, 0xc07ff, 0, DrvSprRAM);
	VezMapArea(0xc0000, 0xc07ff, 1, DrvSprRAM);
	VezMapArea(0xc0000, 0xc07ff, 2, DrvSprRAM);
	VezMapArea(0xc8000, 0xc87ff, 0, DrvSprRAM2);
	VezMapArea(0xc8000, 0xc87ff, 1, DrvSprRAM2);
	VezMapArea(0xc8000, 0xc87ff, 2, DrvSprRAM2);
	VezMapArea(0xcc000, 0xccfff, 0, DrvPalRAM);
//	VezMapArea(0xcc000, 0xccfff, 1, DrvPalRAM);
	VezMapArea(0xcc000, 0xccfff, 2, DrvPalRAM);
	VezMapArea(0xd0000, 0xd3fff, 0, DrvV30RAM);
	VezMapArea(0xd0000, 0xd3fff, 1, DrvV30RAM);
	VezMapArea(0xd0000, 0xd3fff, 2, DrvV30RAM);
	VezMapArea(0xff800, 0xfffff, 0, DrvV30ROM + 0x0ff800);
	VezMapArea(0xff800, 0xfffff, 2, DrvV30ROM + 0x0ff800);
	VezSetReadHandler(m72_main_read);
	VezSetWriteHandler(rtype2_main_write);
	VezSetReadPort(m72_main_read_port);
	VezSetWritePort(m72_main_write_port);
	VezClose();
}

static void sound_ram_map()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xffff, 0, DrvZ80RAM);
	ZetMapArea(0x0000, 0xffff, 1, DrvZ80RAM);
	ZetMapArea(0x0000, 0xffff, 2, DrvZ80RAM);
	ZetSetOutHandler(m72_sound_write_port);
	ZetSetInHandler(m72_sound_read_port);
	ZetClose();

	enable_z80_reset = 1;
}

static void sound_rom_map()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xefff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0xefff, 2, DrvZ80ROM);
	ZetMapArea(0xf000, 0xffff, 0, DrvZ80RAM);
	ZetMapArea(0xf000, 0xffff, 1, DrvZ80RAM);
	ZetMapArea(0xf000, 0xffff, 2, DrvZ80RAM);
	ZetSetOutHandler(m72_sound_write_port);
	ZetSetInHandler(m72_sound_read_port);
	ZetClose();

	enable_z80_reset = 0;
}

static INT32 DrvGfxDecode(UINT8 *gfx, INT32 len, INT32 type)
{
	INT32 Planes[4] = { ((len / 4) * 8) * 3, ((len / 4) * 8) * 2, ((len / 4) * 8) * 1, ((len / 4) * 8) * 0 };
	INT32 XOffs[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87 };
	INT32 YOffs[16] = { 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78 };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, gfx, len);

	if (type) {
		GfxDecode((len * 2) / ( 8 *  8), 4,  8,  8, Planes, XOffs, YOffs, 0x040, tmp, gfx);
	} else {
		GfxDecode((len * 2) / (16 * 16), 4, 16, 16, Planes, XOffs, YOffs, 0x100, tmp, gfx);
	}

	BurnFree (tmp);

	return 0;
}

static INT32 GetRoms(INT32 bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *V30ROM = DrvV30ROM;
	UINT8 *Z80ROM = DrvZ80ROM;
	UINT8 *GFXROM0 = DrvGfxROM0;
	UINT8 *GFXROM1 = DrvGfxROM1;
	UINT8 *GFXROM2 = DrvGfxROM2;
	UINT8 *GFXROM3 = DrvGfxROM3;
	UINT8 *SNDROM = DrvSndROM;
	UINT8 *MCUROM = DrvMcuROM;
	INT32 pglen = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 15) == 1) {
			if (bLoad) {
				if (BurnLoadRom(V30ROM + 1, i + 0, 2)) return 1;
				if (BurnLoadRom(V30ROM + 0, i + 1, 2)) return 1;
			}
			V30ROM += ri.nLen * 2;
			i++;
			continue;
		}

		if ((ri.nType & 15) == 6) {
			if (bLoad) if (BurnLoadRom(Z80ROM , i, 1)) return 1;
			Z80ROM += ri.nLen;
			continue;
		}

		if ((ri.nType & 15) == 2) {
			if (bLoad) if (BurnLoadRom(GFXROM0, i, 1)) return 1;
			GFXROM0 += ri.nLen;

			if (pglen == 0x10000 && ri.nLen == 0x8000) {	// rtype fix
				if (bLoad) if (BurnLoadRom(GFXROM0, i, 1)) return 1;
				GFXROM0 += ri.nLen;
			}
			pglen = ri.nLen;
			continue;
		}

		if ((ri.nType & 15) == 3) {
			if (bLoad) if (BurnLoadRom(GFXROM1, i, 1)) return 1;
			GFXROM1 += ri.nLen;
			continue;
		}

		if ((ri.nType & 15) == 4) {
			if (bLoad) if (BurnLoadRom(GFXROM2, i, 1)) return 1;
			GFXROM2 += ri.nLen;
			continue;
		}

		if ((ri.nType & 15) == 14) {
			if (bLoad) if (BurnLoadRom(GFXROM3, i, 1)) return 1;
			GFXROM3 += ri.nLen;
			continue;
		}

		if ((ri.nType & 15) == 5) {
			if (bLoad) if (BurnLoadRom(SNDROM , i, 1)) return 1;
			SNDROM += ri.nLen;
			continue;
		}

		if ((ri.nType & 15) == 7) {
			use_mcu = 1;
			if (bLoad) bprintf(0, _T("Loading i8751 MCU @ %d - size %x\n"), i, ri.nLen);
			if (bLoad) if (BurnLoadRom(MCUROM , i, 1)) return 1;
			MCUROM += ri.nLen;
			continue;
		}
	}

	if (bLoad) {
		// copy irq vectors
		memcpy (DrvV30ROM + 0xffff0, V30ROM - 0x10, 0x010);

		// mirror sound rom
		INT32 sndlen = SNDROM - DrvSndROM;
		if (sndlen < 0x40000 && sndlen) {
			for (INT32 i = 0; i < 0x40000; i++) {
				DrvSndROM[i] = DrvSndROM[i % sndlen];
			}
		}

		DrvGfxDecode(DrvGfxROM0, graphics_length[0], 0);
		DrvGfxDecode(DrvGfxROM1, graphics_length[1], 1);

		if (graphics_length[2] > 2) {
			DrvGfxDecode(DrvGfxROM2, graphics_length[2], 1);
		} else {
			DrvGfxROM2 = DrvGfxROM1;
			code_mask[2] = code_mask[1];
		}

		if (graphics_length[3] > 2) {
			DrvGfxDecode(DrvGfxROM3, graphics_length[3], 0);
		} else {
			code_mask[3] = 0;
		}

	} else {
		graphics_length[0] = GFXROM0 - DrvGfxROM0;
		graphics_length[1] = GFXROM1 - DrvGfxROM1;
		graphics_length[2] = GFXROM2 - DrvGfxROM2;
		graphics_length[3] = GFXROM3 - DrvGfxROM3;

		for (INT32 i = 1, j = 0; j < 4; graphics_length[j] = i, j++, i = 1) {
			while (i < graphics_length[j]) i <<= 1;
		}

		code_mask[0] = ((graphics_length[0] * 2) - 1) / (16 * 16);
		code_mask[1] = ((graphics_length[1] * 2) - 1) / (8 * 8);
		code_mask[2] = ((graphics_length[2] * 2) - 1) / (8 * 8);
		code_mask[3] = ((graphics_length[3] * 2) - 1) / (16 * 16);
	}

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvV30ROM 	= Next; Next += 0x200000;
	DrvZ80ROM	= Next; Next += 0x010000;
	DrvGfxROM0	= Next; Next += graphics_length[0] * 2;
	DrvGfxROM1	= Next; Next += graphics_length[1] * 2;
	DrvGfxROM2	= Next; Next += graphics_length[2] * 2;
	DrvGfxROM3	= Next; Next += graphics_length[3] * 2;
	DrvSndROM	= Next; Next += 0x040000;
	DrvMcuROM   = Next; Next += 0x010000;

	AllRam	= Next;

	DrvZ80RAM	= Next; Next += 0x010000;
	DrvSprRAM	= Next; Next += 0x001000;
	DrvSprRAM2	= Next; Next += 0x000800;
	DrvSprBuf	= Next; Next += 0x001000;
	DrvVidRAM0	= Next; Next += 0x004000;
	DrvVidRAM1	= Next; Next += 0x010000;
	DrvV30RAM	= Next; Next += 0x004000;
	DrvPalRAM	= Next; Next += 0x002000;

	DrvProtRAM	= Next; Next += 0x001000;
	DrvRowScroll	= Next; Next += 0x000800;

	soundlatch	= Next; Next += 0x000004; // 1
	video_disable	= Next; Next += 0x000004; // 1

	scroll		= Next; Next += 0x000008;

	RamEnd		= Next;

	DrvPalette	= (UINT32 *) Next; Next += 0x200 * sizeof(UINT32);

	MemEnd		= Next;

	return 0;
}


static INT32 DrvInit(void (*pCPUMapCallback)(), void (*pSNDMapCallback)(), INT32 (*pRomLoadCallback)(), INT32 z80_nmi, INT32 video_type, INT32 video_mask)
{
	BurnSetRefreshRate(55.00);

	GenericTilesInit();

	GetRoms(0);

	BurnAllocMemIndex();

	if (GetRoms(1)) return 1;

	if (pCPUMapCallback) {
		pCPUMapCallback();
	}

	if (pSNDMapCallback) {
		pSNDMapCallback();
	}

	if (pRomLoadCallback) {
		if (pRomLoadCallback()) return 1;
	}

	m72_irq_base = 0; // set by port 42. (programmable interrupt controller)
	z80_nmi_enable = z80_nmi;
	m72_video_type = video_type;
	m72_video_mask = video_mask;

	switch (video_type)
	{
		case 0: // m72
			video_offsets[0] = video_offsets[1] = 0;
		break;

		case 1: // rtype
		case 3: // majtitle
			video_offsets[0] = video_offsets[1] = -4;
		break;

		case 2: // hharry, xmultipl
		case 7: // cosmccop, hharryu, dkgensan (layer offsets of type 2, flipxy of type 1)
			video_offsets[0] = -4;
			video_offsets[1] = -6;
			if (video_type == 7) m72_video_type = 1; // cosmccop: diff flipx/y handling in draw_layer()
		break;

		case 4: // poundfor
			video_offsets[0] = video_offsets[1] = -6;
			m72_video_type = 1; // rtype
		break;

		case 5: // kengo
			video_offsets[0] = -3;
			video_offsets[1] = -6;
			m72_video_type = 1; // rtype
		break;

		case 6: // airduel m82
			video_offsets[0] = -4;
			video_offsets[1] = -6;
			m72_video_type = 1; // rtype
		break;
	}

	BurnYM2151InitBuffered(3579545, 1, NULL, 0);
	YM2151SetIrqHandler(0, &m72YM2151IRQHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachZet(3579545);

	DACInit(0, 0, 1, ZetTotalCycles, 3579545);
	DACSetRoute(0, 0.40, BURN_SND_ROUTE_BOTH);

	if (use_mcu) {
		DrvMCUInit();
		bprintf(0, _T("*** Irem M72, with i8751 mcu\n"));
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM2151Exit();
	DACExit();

	ZetExit();
	VezExit();

	BurnFreeMemIndex();

	if (Poundfor)
		BurnTrackballExit();

	m72_video_type = 0;
	enable_z80_reset = 0;
	z80_nmi_enable = 0;
	m72_irq_base = 0;
	Kengo = 0;
	CosmicCop = 0;
	Poundfor = 0;
	airduelm72 = 0;
	Clock_16mhz = 0;

	m72_install_protection(NULL,NULL,NULL);

	video_offsets[0] = video_offsets[1] = 0;

	if (use_mcu) {
		DrvMCUExit();
	}

	use_mcu = 0;

	return 0;
}

static void draw_layer(INT32 layer, INT32 forcelayer, INT32 type, INT32 start, INT32 finish) // bg = layer 1, fg = layer 0
{
	INT32 codeand = code_mask[1+layer];
	UINT16 *vram = (UINT16*)(layer ? DrvVidRAM1 : DrvVidRAM0);
	UINT8  *gfx  = (layer) ? DrvGfxROM2 : DrvGfxROM1;

	//	    layer, prio, forcelayer
	UINT16 transmask[2][3][2] = {
		{ { 0xffff, 0x0001 }, { 0x00ff, 0xff01 }, { 0x0001, 0xffff } },
		{ { 0xffff, 0x0000 }, { 0x00ff, 0xff00 }, { 0x0007, 0xfff8 } }
	};

	switch (m72_video_mask) {
		case 1: transmask[1][2][0] = 0x0001; transmask[1][2][1] = 0xfffe; break;
		case 2: transmask[1][2][0] = 0xff00; transmask[1][2][1] = 0x00ff; break;
		case 3: transmask[1][2][0] = 0x001f; transmask[1][2][1] = 0xffe0; break;
		case 4: transmask[1][2][0] = 0x00ff; transmask[1][2][1] = 0xff00; break;
	}

	INT32 scrolly = scroll[layer * 4 + 0] | (scroll[layer * 4 + 1] << 8);
	INT32 scrollx = scroll[layer * 4 + 2] | (scroll[layer * 4 + 3] << 8);

	scrolly = (scrolly + 128) & 0x1ff;
	scrollx = (scrollx + 64 + video_offsets[layer]) & ((0x200 << (type == 3 && layer == 1)) - 1);

	UINT16 *xscroll = (UINT16*)DrvRowScroll;

	for (INT32 sy = start; sy < finish; sy++)
	{
		UINT16 *dest = pTransDraw + (sy * nScreenWidth);
		//UINT8  *pri  = pPrioDraw + (sy * nScreenWidth);

		INT32 scrolly1 = (scrolly + sy) & 0x1ff;
		INT32 romoff1 = (scrolly1 & 0x07) << 3;

		for (INT32 sx = 0; sx < nScreenWidth + 8; sx+=8)
		{
			INT32 flipy, flipx, prio, scrollx1, offs;

			if (majtitle_rowscroll_enable && type == 3 && layer == 1) {
				scrollx1 = 256 + BURN_ENDIAN_SWAP_INT16(xscroll[scrolly1]) + sx + 64 + video_offsets[1];
			} else {
				scrollx1 = scrollx + sx;
				if (type == 3 && layer == 1) scrollx1 += 256;
			}

			if (type == 3 && layer == 1) {
				scrollx1 &= 0x3ff;
				offs = ((scrolly1 >> 3) << 8) | (scrollx1 >> 3);
			} else {
				scrollx1 &= 0x1ff;
				offs = ((scrolly1 >> 3) << 6) | (scrollx1 >> 3);
			}

			INT32 code  = BURN_ENDIAN_SWAP_INT16(vram[offs * 2 + 0]);
			INT32 color = BURN_ENDIAN_SWAP_INT16(vram[offs * 2 + 1]);

			if (type == 1 || type == 3) {
				flipy = color & 0x0040;
				flipx = color & 0x0020;
				prio  = (color & 0x0100) ? 2 : (color & 0x80) ? 1 : 0;
			} else {
				flipy = code & 0x8000;
				flipx = code & 0x4000;
				prio  = (color & 0x80) ? 2 : ((color & 0x40) ? 1 : 0);
			}

			INT32 mask = transmask[layer][prio][forcelayer];

			code &= codeand;
			color = ((color & 0x000f) << 4) | 0x100;
			prio = 1 << prio;

			{
				INT32 scrollx0 = scrollx1 & 0x07;
				INT32 x_xor = 0;
				INT32 romoff = romoff1;
				if (flipy) romoff ^= 0x38;
				if (flipx) x_xor = 7;

				UINT8 *rom = gfx + (code * 0x40) + romoff;

				INT32 xx = sx - scrollx0;

				for (INT32 x = 0; x < 8; x++, xx++) {
					if (xx < 0 || xx >= nScreenWidth) continue;

					INT32 pxl = rom[x ^ x_xor];
					if (mask & (1 << pxl)) continue;

					dest[xx] = pxl | color;
					//pri[xx] = prio;
				}
			}
		}
	}
}

static void draw_sprites()
{
	UINT16 *sprram = (UINT16*)DrvSprBuf;

	for (INT32 offs = 0; offs < 0x400/2;)
	{
		INT32 sx    = -256+(BURN_ENDIAN_SWAP_INT16(sprram[offs+3]) & 0x3ff);
		INT32 attr  = BURN_ENDIAN_SWAP_INT16(sprram[offs+2]);
		INT32 code  = BURN_ENDIAN_SWAP_INT16(sprram[offs+1]);
		INT32 sy    =  384-(BURN_ENDIAN_SWAP_INT16(sprram[offs+0]) & 0x1ff);

		INT32 color = attr & 0x0f;
		INT32 flipx = attr & 0x0800;
		INT32 flipy = attr & 0x0400;

		INT32 w = 1 << ((attr & 0xc000) >> 14);
		INT32 h = 1 << ((attr & 0x3000) >> 12);
		sy -= 16 * h;

		sx -= 64; // ?
#if 0
		if (*flipscreen)
		{
			sx = 512 - 16*w - sx;
			sy = 284 - 16*h - sy;
			flipx = !flipx;
			flipy = !flipy;
		}
#endif

		for (INT32 x = 0;x < w;x++)
		{
			for (INT32 y = 0;y < h;y++)
			{
				INT32 c = code;

				if (flipx) c += 8*(w-1-x);
				else c += 8*x;
				if (flipy) c += h-1-y;
				else c += y;

				c &= code_mask[0];

				INT32 xx = sx + 16 * x;
				INT32 yy = sy + 16 * y;

				if (xx < -15 || yy < -15 || xx >= nScreenWidth || yy >= nScreenHeight) continue;

				Draw16x16MaskTile(pTransDraw, c, xx, yy, flipx, flipy, color, 4, 0, 0, DrvGfxROM0);
			}
		}

		offs += w*4;
	}
}

static void majtitle_draw_sprites()
{
	UINT16 *spriteram16_2 = (UINT16*)DrvSprRAM2;

	for (INT32 offs = 0; offs < 0x400; offs += 4)
	{
		INT32 code,color,sx,sy,flipx,flipy,w,h,x,y;

		code = BURN_ENDIAN_SWAP_INT16(spriteram16_2[offs+1]);
		color = BURN_ENDIAN_SWAP_INT16(spriteram16_2[offs+2]) & 0x0f;
		sx = -256+(BURN_ENDIAN_SWAP_INT16(spriteram16_2[offs+3]) & 0x3ff);
		sy = 384-(BURN_ENDIAN_SWAP_INT16(spriteram16_2[offs+0]) & 0x1ff);
		flipx = BURN_ENDIAN_SWAP_INT16(spriteram16_2[offs+2]) & 0x0800;
		flipy = BURN_ENDIAN_SWAP_INT16(spriteram16_2[offs+2]) & 0x0400;

		w = 1;// << ((spriteram16_2[offs+2] & 0xc000) >> 14);
		h = 1 << ((BURN_ENDIAN_SWAP_INT16(spriteram16_2[offs+2]) & 0x3000) >> 12);
		sy -= 16 * h;

		sx -= 64; // ?
#if 0
		if (flip_screen_get(machine))
		{
			sx = 512 - 16*w - sx;
			sy = 256 - 16*h - sy;
			flipx = !flipx;
			flipy = !flipy;
		}
#endif

		for (x = 0;x < w;x++)
		{
			for (y = 0;y < h;y++)
			{
				INT32 c = code;

				if (flipx) c += 8*(w-1-x);
				else c += 8*x;
				if (flipy) c += h-1-y;
				else c += y;

				c &= code_mask[3];

				INT32 xx = sx + 16 * x;
				INT32 yy = sy + 16 * y;

				if (xx < -15 || yy < -15 || xx >= nScreenWidth || yy >= nScreenHeight) continue;

				Draw16x16MaskTile(pTransDraw, c, xx, yy, flipx, flipy, color, 4, 0, 0, DrvGfxROM3);
			}
		}
	}
}

static void dodrawline(INT32 start, INT32 finish)
{
	if (*video_disable || !pBurnDraw) return;

	if (nBurnLayer & 1) draw_layer(1, 1, m72_video_type, start, finish);
	if (nBurnLayer & 2) draw_layer(0, 1, m72_video_type, start, finish);

	GenericTilesSetClip(0, -1, start, finish);
	if (nSpriteEnable & 1) {
		if (m72_video_type == 3) majtitle_draw_sprites();
		draw_sprites();
	}
	GenericTilesClearClip();

	if (nBurnLayer & 4) draw_layer(1, 0, m72_video_type, start, finish);
	if (nBurnLayer & 8) draw_layer(0, 0, m72_video_type, start, finish);
}

static inline void DrvDrawInitFrame()
{
	BurnTransferClear();
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x200; i++) {
			palette_write((i & 0xff) * 2, i >> 8);
		}
		DrvRecalc = 0;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void compile_inputs()
{
	memset (DrvInputs, 0xff, 5);

	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
	}

	if (Poundfor) {
		BurnTrackballConfig(0, AXIS_NORMAL, AXIS_REVERSED);
		BurnTrackballConfig(1, AXIS_NORMAL, AXIS_REVERSED);
		BurnTrackballFrame(0, Analog[0], Analog[1], 5, 8);
		BurnTrackballFrame(1, Analog[2], Analog[3], 5, 8);
		BurnTrackballUpdate(0);
		BurnTrackballUpdate(1);
	}
}

static INT32 nPreviousLine = 0;

static void scanline_interrupts(INT32 scanline)
{
	if (scanline == (irq_raster_position - 128) && scanline < 256) {
		if (nPreviousLine <= scanline && scanline < nScreenHeight) {
			dodrawline(nPreviousLine, scanline+1);
			nPreviousLine = scanline + 1;
		}
		if (Kengo)
			VezSetIRQLineAndVector(NEC_INPUT_LINE_INTP2, 0xff, CPU_IRQSTATUS_AUTO);
		else
			VezSetIRQLineAndVector(0, (m72_irq_base + 8)/4, CPU_IRQSTATUS_AUTO);
	}
	else if (scanline == 256) // vblank
	{
		if (nPreviousLine < nScreenHeight) {
			dodrawline(nPreviousLine, nScreenHeight);
			nPreviousLine = 0;
		}
		if (Kengo)
			VezSetIRQLineAndVector(NEC_INPUT_LINE_INTP0, 0xff, CPU_IRQSTATUS_AUTO);
		else
			VezSetIRQLineAndVector(0, (m72_irq_base + 0)/4, CPU_IRQSTATUS_AUTO);
	}

	if (nPreviousLine >= nScreenHeight) nPreviousLine = 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	VezNewFrame();
	ZetNewFrame();
	if (use_mcu) mcs51NewFrame();

	compile_inputs();

	INT32 multiplier = 3;
	INT32 nInterleave = 284 * multiplier;
	INT32 nSampleInt = nInterleave / 142;
	INT32 nCyclesTotal[3] = {
		BurnSpeedAdjust(8000000 / 55.017606),
		BurnSpeedAdjust(3579545 / 55.017606),
		BurnSpeedAdjust(8000000 / 12 / 55.017606),
	};
	main_mhz = BurnSpeedAdjust(8000000);
	mcu_mhz = main_mhz;
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], 0 };
	INT32 z80samplecount = 0;

	if (Clock_16mhz) {// Ken-go, Cosmic Cop
		nCyclesTotal[0] = BurnSpeedAdjust(16000000 / 55.017606);
		main_mhz = BurnSpeedAdjust(16000000);
	}

	if (pBurnDraw) {
		DrvDrawInitFrame();
	}

	VezOpen(0);
	ZetOpen(0);
	VezIdle(nExtraCycles[0]); // for mcu sync to nec
	if (use_mcu) {
		mcs51Idle(nExtraCycles[2]);
	}

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if ((i%multiplier)==0)
			scanline_interrupts(i/multiplier);

		CPU_RUN(0, Vez);

		if (use_mcu) {
			CPU_RUN_SYNCINT(2, mcs51);
		}

		CPU_RUN_TIMER(1);

		if (i%nSampleInt == nSampleInt-1) { // 142x per frame
			if (z80_nmi_enable == Z80_FAKE_NMI && use_mcu == 0) {
				z80samplecount++;
				if (DrvSndROM[sample_address]) {
					DACSignedWrite(0, DrvSndROM[sample_address]);
					sample_address = (sample_address + 1) & 0x3ffff;
				} else {
					DACWrite(0, 0); // Clear DAC output buffer at end of sample - fixes distortion in Air Duel & second-to-last level of Mr. Heli
				}

			} else if (z80_nmi_enable == Z80_REAL_NMI) {
				z80samplecount++;
				ZetNmi();
			}
		}
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	if (use_mcu) nExtraCycles[2] = mcs51TotalCycles() - nCyclesTotal[2];

	//if (use_mcu) bprintf(0, _T("mcu %d  v30 %d  mcutotalcyc  %d   nectotalcyc  %d\n"), nExtraCycles[2], nExtraCycles[0], mcs51TotalCycles(), VezTotalCycles());

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	VezClose();
	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029705;
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
		BurnYM2151Scan(nAction, pnMin);
		DACScan(nAction, pnMin);
		VezScan(nAction);

		if (use_mcu) {
			DrvMCUScan(nAction);
		}

		if (Poundfor)
			BurnTrackballScan();

		SCAN_VAR(irq_raster_position);
		SCAN_VAR(m72_irq_base);

		SCAN_VAR(sample_address);
		SCAN_VAR(irqvector);
		SCAN_VAR(z80_reset);
		SCAN_VAR(majtitle_rowscroll_enable);

		SCAN_VAR(nExtraCycles);
	}

	return 0;
}



// R-Type (World)

static struct BurnRomInfo rtypeRomDesc[] = {
	{ "rt_r-h0-b.1b",	0x10000, 0x591c7754, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rt_r-l0-b.3b",	0x10000, 0xa1928df0, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "rt_r-h1-b.1c",	0x10000, 0xa9d71eca, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "rt_r-l1-b.3c",	0x10000, 0x0df3573d, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "rt_r-00.1h",		0x10000, 0xdad53bc0, 0x02 | BRF_GRA },           //  4 Sprites
	{ "rt_r-01.1j",		0x08000, 0x5e441e7f, 0x02 | BRF_GRA },           //  5
	{ "rt_r-10.1k",		0x10000, 0xd6a66298, 0x02 | BRF_GRA },           //  6
	{ "rt_r-11.1l",		0x08000, 0x791df4f8, 0x02 | BRF_GRA },           //  7
	{ "rt_r-20.3h",		0x10000, 0xfc247c8a, 0x02 | BRF_GRA },           //  8
	{ "rt_r-21.3j",		0x08000, 0xed793841, 0x02 | BRF_GRA },           //  9
	{ "rt_r-30.3k",		0x10000, 0xeb02a1cb, 0x02 | BRF_GRA },           // 10
	{ "rt_r-31.3l",		0x08000, 0x8558355d, 0x02 | BRF_GRA },           // 11

	{ "rt_b-a0.ic20",	0x08000, 0x4e212fb0, 0x03 | BRF_GRA },           // 12 Foreground Tiles
	{ "rt_b-a1.ic22",	0x08000, 0x8a65bdff, 0x03 | BRF_GRA },           // 13
	{ "rt_b-a2.ic20",	0x08000, 0x5a4ae5b9, 0x03 | BRF_GRA },           // 14
	{ "rt_b-a3.ic23",	0x08000, 0x73327606, 0x03 | BRF_GRA },           // 15

	{ "rt_b-b0.ic26",	0x08000, 0xa7b17491, 0x04 | BRF_GRA },           // 16 Background Tiles
	{ "rt_b-b1.ic27",	0x08000, 0xb9709686, 0x04 | BRF_GRA },           // 17
	{ "rt_b-b2.ic25",	0x08000, 0x433b229a, 0x04 | BRF_GRA },           // 18
	{ "rt_b-b3.ic24",	0x08000, 0xad89b072, 0x04 | BRF_GRA },           // 19

	{ "m72_a-8l-.ic66",	0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 20 Proms
	{ "m72_a-9l-.ic75",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 21

	{ "m72_a-3d-.ic11",	0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 22 PLDs
	{ "m72_a-4d-.ic19",	0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 23
	{ "m72_r-3a-.3a",	0x00117, 0x055af779, 0x00 | BRF_OPT },           // 24
};

STD_ROM_PICK(rtype)
STD_ROM_FN(rtype)

static INT32 rtypeInit()
{
	return DrvInit(common_040000_040000, sound_ram_map, NULL, Z80_NO_NMI, 0, 0);
}

struct BurnDriver BurnDrvRtype = {
	"rtype", NULL, NULL, NULL, "1987",
	"R-Type (World)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, rtypeRomInfo, rtypeRomName, NULL, NULL, NULL, NULL, CommonInputInfo, RtypeDIPInfo,
	rtypeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// R-Type (Japan)

static struct BurnRomInfo rtypejRomDesc[] = {
	{ "rt_r-h0-.1b",	0x10000, 0xc2940df2, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rt_r-l0-.3b",	0x10000, 0x858cc0f6, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "rt_r-h1-.1c",	0x10000, 0x5bcededa, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "rt_r-l1-.3c",	0x10000, 0x4821141c, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "rt_r-00.1h",		0x10000, 0xdad53bc0, 0x02 | BRF_GRA },           //  4 Sprites
	{ "rt_r-01.1j",		0x08000, 0x5e441e7f, 0x02 | BRF_GRA },           //  5
	{ "rt_r-10.1k",		0x10000, 0xd6a66298, 0x02 | BRF_GRA },           //  6
	{ "rt_r-11.1l",		0x08000, 0x791df4f8, 0x02 | BRF_GRA },           //  7
	{ "rt_r-20.3h",		0x10000, 0xfc247c8a, 0x02 | BRF_GRA },           //  8
	{ "rt_r-21.3j",		0x08000, 0xed793841, 0x02 | BRF_GRA },           //  9
	{ "rt_r-30.3k",		0x10000, 0xeb02a1cb, 0x02 | BRF_GRA },           // 10
	{ "rt_r-31.3l",		0x08000, 0x8558355d, 0x02 | BRF_GRA },           // 11

	{ "rt_b-a0.ic20",	0x08000, 0x4e212fb0, 0x03 | BRF_GRA },           // 12 Foreground Tiles
	{ "rt_b-a1.ic22",	0x08000, 0x8a65bdff, 0x03 | BRF_GRA },           // 13
	{ "rt_b-a2.ic20",	0x08000, 0x5a4ae5b9, 0x03 | BRF_GRA },           // 14
	{ "rt_b-a3.ic23",	0x08000, 0x73327606, 0x03 | BRF_GRA },           // 15

	{ "rt_b-b0.ic26",	0x08000, 0xa7b17491, 0x04 | BRF_GRA },           // 16 Background Tiles
	{ "rt_b-b1.ic27",	0x08000, 0xb9709686, 0x04 | BRF_GRA },           // 17
	{ "rt_b-b2.ic25",	0x08000, 0x433b229a, 0x04 | BRF_GRA },           // 18
	{ "rt_b-b3.ic24",	0x08000, 0xad89b072, 0x04 | BRF_GRA },           // 19

	{ "m72_a-8l-.ic66",	0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 20 Proms
	{ "m72_a-9l-.ic75",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 21

	{ "m72_a-3d-.ic11",	0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 22 PLDs
	{ "m72_a-4d-.ic19",	0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 23
	{ "m72_r-3a-.3a",	0x00117, 0x055af779, 0x00 | BRF_OPT },           // 24
};

STD_ROM_PICK(rtypej)
STD_ROM_FN(rtypej)

struct BurnDriver BurnDrvRtypej = {
	"rtypej", "rtype", NULL, NULL, "1987",
	"R-Type (Japan)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, rtypejRomInfo, rtypejRomName, NULL, NULL, NULL, NULL, CommonInputInfo, RtypeDIPInfo,
	rtypeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// R-Type (Japan prototype)

static struct BurnRomInfo rtypejpRomDesc[] = {
	{ "db_b1.1b",		0x10000, 0xc1865141, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "db_a1.3b",		0x10000, 0x5ad2bd90, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "db_b2.1c",		0x10000, 0xb4f6407e, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "db_a2.3c",		0x10000, 0x6098d86f, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "rt_r-00.1h",		0x10000, 0xdad53bc0, 0x02 | BRF_GRA },           //  4 Sprites
	{ "rt_r-01.1j",		0x08000, 0x5e441e7f, 0x02 | BRF_GRA },           //  5
	{ "rt_r-10.1k",		0x10000, 0xd6a66298, 0x02 | BRF_GRA },           //  6
	{ "rt_r-11.1l",		0x08000, 0x791df4f8, 0x02 | BRF_GRA },           //  7
	{ "rt_r-20.3h",		0x10000, 0xfc247c8a, 0x02 | BRF_GRA },           //  8
	{ "rt_r-21.3j",		0x08000, 0xed793841, 0x02 | BRF_GRA },           //  9
	{ "rt_r-30.3k",		0x10000, 0xeb02a1cb, 0x02 | BRF_GRA },           // 10
	{ "rt_r-31.3l",		0x08000, 0x8558355d, 0x02 | BRF_GRA },           // 11

	{ "rt_b-a0.ic20",	0x08000, 0x4e212fb0, 0x03 | BRF_GRA },           // 12 Foreground Tiles
	{ "rt_b-a1.ic22",	0x08000, 0x8a65bdff, 0x03 | BRF_GRA },           // 13
	{ "rt_b-a2.ic20",	0x08000, 0x5a4ae5b9, 0x03 | BRF_GRA },           // 14
	{ "rt_b-a3.ic23",	0x08000, 0x73327606, 0x03 | BRF_GRA },           // 15

	{ "rt_b-b0.ic26",	0x08000, 0xa7b17491, 0x04 | BRF_GRA },           // 16 Background Tiles
	{ "rt_b-b1.ic27",	0x08000, 0xb9709686, 0x04 | BRF_GRA },           // 17
	{ "rt_b-b2.ic25",	0x08000, 0x433b229a, 0x04 | BRF_GRA },           // 18
	{ "rt_b-b3.ic24",	0x08000, 0xad89b072, 0x04 | BRF_GRA },           // 19

	{ "m72_a-8l-.ic66",	0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 20 Proms
	{ "m72_a-9l-.ic75",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 21

	{ "m72_a-3d-.ic11",	0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 22 PLDs
	{ "m72_a-4d-.ic19",	0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 23
	{ "m72_r-3a-.3a",	0x00117, 0x055af779, 0x00 | BRF_OPT },           // 24
};

STD_ROM_PICK(rtypejp)
STD_ROM_FN(rtypejp)

struct BurnDriver BurnDrvRtypejp = {
	"rtypejp", "rtype", NULL, NULL, "1987",
	"R-Type (Japan prototype)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, rtypejpRomInfo, rtypejpRomName, NULL, NULL, NULL, NULL, CommonInputInfo, RtypepDIPInfo,
	rtypeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// R-Type (US)

static struct BurnRomInfo rtypeuRomDesc[] = {
	{ "rt_r-h0-a.1b",	0x10000, 0x36008a4e, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rt_r-l0-a.3b",	0x10000, 0x4aaa668e, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "rt_r-h1-a.1c",	0x10000, 0x7ebb2a53, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "rt_r-l1-a.3c",	0x10000, 0xc28b103b, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "rt_r-00.1h",		0x10000, 0xdad53bc0, 0x02 | BRF_GRA },           //  4 Sprites
	{ "rt_r-01.1j",		0x08000, 0x5e441e7f, 0x02 | BRF_GRA },           //  5
	{ "rt_r-10.1k",		0x10000, 0xd6a66298, 0x02 | BRF_GRA },           //  6
	{ "rt_r-11.1l",		0x08000, 0x791df4f8, 0x02 | BRF_GRA },           //  7
	{ "rt_r-20.3h",		0x10000, 0xfc247c8a, 0x02 | BRF_GRA },           //  8
	{ "rt_r-21.3j",		0x08000, 0xed793841, 0x02 | BRF_GRA },           //  9
	{ "rt_r-30.3k",		0x10000, 0xeb02a1cb, 0x02 | BRF_GRA },           // 10
	{ "rt_r-31.3l",		0x08000, 0x8558355d, 0x02 | BRF_GRA },           // 11

	{ "rt_b-a0.ic20",	0x08000, 0x4e212fb0, 0x03 | BRF_GRA },           // 12 Foreground Tiles
	{ "rt_b-a1.ic22",	0x08000, 0x8a65bdff, 0x03 | BRF_GRA },           // 13
	{ "rt_b-a2.ic20",	0x08000, 0x5a4ae5b9, 0x03 | BRF_GRA },           // 14
	{ "rt_b-a3.ic23",	0x08000, 0x73327606, 0x03 | BRF_GRA },           // 15

	{ "rt_b-b0.ic26",	0x08000, 0xa7b17491, 0x04 | BRF_GRA },           // 16 Background Tiles
	{ "rt_b-b1.ic27",	0x08000, 0xb9709686, 0x04 | BRF_GRA },           // 17
	{ "rt_b-b2.ic25",	0x08000, 0x433b229a, 0x04 | BRF_GRA },           // 18
	{ "rt_b-b3.ic24",	0x08000, 0xad89b072, 0x04 | BRF_GRA },           // 19

	{ "m72_a-8l-.ic66",	0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 20 Proms
	{ "m72_a-9l-.ic75",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 21

	{ "m72_a-3d-.ic11",	0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 22 PLDs
	{ "m72_a-4d-.ic19",	0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 23
	{ "m72_r-3a-.3a",	0x00117, 0x055af779, 0x00 | BRF_OPT },           // 24
};

STD_ROM_PICK(rtypeu)
STD_ROM_FN(rtypeu)

struct BurnDriver BurnDrvRtypeu = {
	"rtypeu", "rtype", NULL, NULL, "1987",
	"R-Type (US)\0", NULL, "Irem (Nintendo of America license)", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, rtypeuRomInfo, rtypeuRomName, NULL, NULL, NULL, NULL, CommonInputInfo, RtypeDIPInfo,
	rtypeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// R-Type (World bootleg)

static struct BurnRomInfo rtypebRomDesc[] = {
	{ "7.512",			0x10000, 0xeacc8024, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "1.512",			0x10000, 0x2e5fe27b, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "8.512",			0x10000, 0x22cc4950, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "2.512",			0x10000, 0xada7b90e, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "rt_r-00.1h",		0x10000, 0xdad53bc0, 0x02 | BRF_GRA },           //  4 Sprites
	{ "rt_r-01.1j",		0x08000, 0x5e441e7f, 0x02 | BRF_GRA },           //  5
	{ "rt_r-10.1k",		0x10000, 0xd6a66298, 0x02 | BRF_GRA },           //  6
	{ "rt_r-11.1l",		0x08000, 0x791df4f8, 0x02 | BRF_GRA },           //  7
	{ "rt_r-20.3h",		0x10000, 0xfc247c8a, 0x02 | BRF_GRA },           //  8
	{ "rt_r-21.3j",		0x08000, 0xed793841, 0x02 | BRF_GRA },           //  9
	{ "rt_r-30.3k",		0x10000, 0xeb02a1cb, 0x02 | BRF_GRA },           // 10
	{ "rt_r-31.3l",		0x08000, 0x8558355d, 0x02 | BRF_GRA },           // 11

	{ "rt_b-a0.ic20",	0x08000, 0x4e212fb0, 0x03 | BRF_GRA },           // 12 Foreground Tiles
	{ "rt_b-a1.ic22",	0x08000, 0x8a65bdff, 0x03 | BRF_GRA },           // 13
	{ "rt_b-a2.ic20",	0x08000, 0x5a4ae5b9, 0x03 | BRF_GRA },           // 14
	{ "rt_b-a3.ic23",	0x08000, 0x73327606, 0x03 | BRF_GRA },           // 15

	{ "rt_b-b0.ic26",	0x08000, 0xa7b17491, 0x04 | BRF_GRA },           // 16 Background Tiles
	{ "rt_b-b1.ic27",	0x08000, 0xb9709686, 0x04 | BRF_GRA },           // 17
	{ "rt_b-b2.ic25",	0x08000, 0x433b229a, 0x04 | BRF_GRA },           // 18
	{ "rt_b-b3.ic24",	0x08000, 0xad89b072, 0x04 | BRF_GRA },           // 19
};

STD_ROM_PICK(rtypeb)
STD_ROM_FN(rtypeb)

struct BurnDriver BurnDrvRtypeb = {
	"rtypeb", "rtype", NULL, NULL, "1987",
	"R-Type (World bootleg)\0", NULL, "bootleg", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, rtypebRomInfo, rtypebRomName, NULL, NULL, NULL, NULL, CommonInputInfo, RtypeDIPInfo,
	rtypeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// R-Type (Japan, bootleg M82 conversion)

static struct BurnRomInfo rtypem82bRomDesc[] = {
	{ "rt_h0.ic52",		0x20000, 0x5fa5068b, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rt_l0.ic60",		0x20000, 0xaee6fae8, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "rt_h1.ic51",		0x20000, 0x76389df4, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "rt_l1.ic59",		0x20000, 0x6af66a05, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "rt_sp.ic15",		0x10000, 0x24fded65, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rt_n0.ic44",		0x20000, 0x236e93ad, 0x02 | BRF_GRA },           //  5 Sprites
	{ "rt_n1.ic45",		0x20000, 0x94e0da50, 0x02 | BRF_GRA },           //  6
	{ "rt_n2.ic46",		0x20000, 0x6310dd0e, 0x02 | BRF_GRA },           //  7
	{ "rt_n3.ic36",		0x20000, 0xdd9674fb, 0x02 | BRF_GRA },           //  8

	{ "rt_c0.ic38",		0x40000, 0xc2511272, 0x03 | BRF_GRA },           //  9 Foreground Tiles
	{ "rt_c1.ic39",		0x40000, 0x6da33dae, 0x03 | BRF_GRA },           // 10
	{ "rt_c2.ic40",		0x40000, 0x29322d6e, 0x03 | BRF_GRA },           // 11
	{ "rt_c3.ic41",		0x40000, 0x0ab3a8db, 0x03 | BRF_GRA },           // 12

	{ "mt_f0.ic38",		0x20000, 0x2d5e05d5, 0x0e | BRF_GRA },           // 13 Sprites 2
	{ "mt_f1.ic39",		0x20000, 0xc68cd65f, 0x0e | BRF_GRA },           // 14
	{ "mt_f2.ic40",		0x20000, 0xa71feb2d, 0x0e | BRF_GRA },           // 15
	{ "mt_f3.ic41",		0x20000, 0x179f7562, 0x0e | BRF_GRA },           // 26

	{ "mt_vo.ic12",		0x20000, 0xeb24bb2c, 0x05 | BRF_SND },           // 17 DAC Samples
};

STD_ROM_PICK(rtypem82b)
STD_ROM_FN(rtypem82b)

static INT32 rtypem82bInit()
{
	return DrvInit(majtitle_main_cpu_map, sound_rom_map, NULL, Z80_REAL_NMI, 6, 0);
}

struct BurnDriver BurnDrvRtypem82b = {
	"rtypem82b", "rtype", NULL, NULL, "2009",
	"R-Type (Japan, bootleg M82 conversion)\0", NULL, "bootleg", "Irem M82",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, rtypem82bRomInfo, rtypem82bRomName, NULL, NULL, NULL, NULL, CommonInputInfo, RtypeDIPInfo,
	rtypem82bInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// X Multiply (World, M81 hardware)

static struct BurnRomInfo xmultiplRomDesc[] = {
	{ "xm_a-h1-.ic58",		0x20000, 0x449048cf, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "xm_a-l1-.ic67",		0x20000, 0x26ce39b0, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "xm_a-h0-.ic59",		0x10000, 0x509bc970, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "xm_a-l0-.ic68",		0x10000, 0x490a9ebc, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "xm_a-sp-.ic14",		0x10000, 0x006eef56, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "t44.00.ic3",			0x20000, 0xdb45186e, 0x02 | BRF_GRA },           //  5 Sprites
	{ "t45.01.ic2",			0x20000, 0x4d0764d4, 0x02 | BRF_GRA },           //  6
	{ "t46.10.ic5",			0x20000, 0xf0c465a4, 0x02 | BRF_GRA },           //  7
	{ "t47.11.ic4",			0x20000, 0x1263b24b, 0x02 | BRF_GRA },           //  8
	{ "t48.20.ic12",		0x20000, 0x4129944f, 0x02 | BRF_GRA },           //  9
	{ "t49.21.ic11",		0x20000, 0x2346e6f9, 0x02 | BRF_GRA },           // 10
	{ "t50.30.ic14",		0x20000, 0xe322543e, 0x02 | BRF_GRA },           // 11
	{ "t51.31.ic13",		0x20000, 0x229bf7b1, 0x02 | BRF_GRA },           // 12

	{ "t53.a0.ic50",		0x20000, 0x1a082494, 0x03 | BRF_GRA },           // 13 Foreground Tiles
	{ "t54.a1.ic49",		0x20000, 0x076c16c5, 0x03 | BRF_GRA },           // 14
	{ "t55.a2.ic51",		0x20000, 0x25d877a5, 0x03 | BRF_GRA },           // 15
	{ "t56.a3.ic52",		0x20000, 0x5b1213f5, 0x03 | BRF_GRA },           // 16

	{ "t57.b0.ic47",		0x20000, 0x0a84e0c7, 0x04 | BRF_GRA },           // 17 Background Tiles
	{ "t58.b1.ic48",		0x20000, 0xa874121d, 0x04 | BRF_GRA },           // 18
	{ "t59.b2.ic46",		0x20000, 0x69deb990, 0x04 | BRF_GRA },           // 19
	{ "t60.b3.ic45",		0x20000, 0x14c69f99, 0x04 | BRF_GRA },           // 20

	{ "t52.v0.ic11",		0x20000, 0x2db1bd80, 0x05 | BRF_SND },           // 21 DAC Samples

	{ "m81_a-9l-.ic72",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 22 Proms
	{ "m81_a-9p-.ic74",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 23

	{ "m81_b-9h-.ic94",		0x00117, 0x4bc1d393, 0x00 | BRF_OPT },           // 24 PLDs
	{ "m81_a-5l-.ic28",		0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 25
	{ "m81_a-2h-.ic2",		0x00117, 0x21ede612, 0x00 | BRF_OPT },           // 26
	{ "xm_a-7d-.ic48",		0x00117, 0x2ef7ad4e, 0x00 | BRF_OPT },           // 27
};

STD_ROM_PICK(xmultipl)
STD_ROM_FN(xmultipl)

static INT32 xmultiplInit()
{
	return DrvInit(common_080000_09c000, sound_rom_map, NULL, Z80_REAL_NMI, 2, 0);
}

struct BurnDriver BurnDrvXmultipl = {
	"xmultipl", NULL, NULL, NULL, "1989",
	"X Multiply (World, M81 hardware)\0", NULL, "Irem", "Irem M81",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, xmultiplRomInfo, xmultiplRomName, NULL, NULL, NULL, NULL, CommonInputInfo, XmultiplDIPInfo,
	xmultiplInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// X Multiply (Japan, M72 hardware)

static struct BurnRomInfo xmultiplm72RomDesc[] = {
	{ "xm_c-h3-.ic43",	0x20000, 0x20685021, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "xm_c-l3-.ic34",	0x20000, 0x93fdd200, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "xm_c-h0-.ic40",	0x10000, 0x9438dd8a, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "xm_c-l0-.ic37",	0x10000, 0x06a9e213, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "t44.00.ic53",	0x20000, 0xdb45186e, 0x02 | BRF_GRA },           //  4 Sprites
	{ "t45.01.ic52",	0x20000, 0x4d0764d4, 0x02 | BRF_GRA },           //  5
	{ "t46.10.ic51",	0x20000, 0xf0c465a4, 0x02 | BRF_GRA },           //  6
	{ "t47.11.ic50",	0x20000, 0x1263b24b, 0x02 | BRF_GRA },           //  7
	{ "t48.20.ic49",	0x20000, 0x4129944f, 0x02 | BRF_GRA },           //  8
	{ "t49.21.ic48",	0x20000, 0x2346e6f9, 0x02 | BRF_GRA },           //  9
	{ "t50.30.ic47",	0x20000, 0xe322543e, 0x02 | BRF_GRA },           // 10
	{ "t51.31.ic46",	0x20000, 0x229bf7b1, 0x02 | BRF_GRA },           // 11

	{ "t53.a0.ic21",	0x20000, 0x1a082494, 0x03 | BRF_GRA },           // 12 Foreground Tiles
	{ "t54.a1.ic22",	0x20000, 0x076c16c5, 0x03 | BRF_GRA },           // 13
	{ "t55.a2.ic20",	0x20000, 0x25d877a5, 0x03 | BRF_GRA },           // 14
	{ "t56.a3.ic23",	0x20000, 0x5b1213f5, 0x03 | BRF_GRA },           // 15

	{ "t57.b0.ic26",	0x20000, 0x0a84e0c7, 0x04 | BRF_GRA },           // 16 Background Tiles
	{ "t58.b1.ic27",	0x20000, 0xa874121d, 0x04 | BRF_GRA },           // 17
	{ "t59.b2.ic25",	0x20000, 0x69deb990, 0x04 | BRF_GRA },           // 18
	{ "t60.b3.ic24",	0x20000, 0x14c69f99, 0x04 | BRF_GRA },           // 19

	{ "t52.v0.ic44",	0x20000, 0x2db1bd80, 0x05 | BRF_SND },           // 20 DAC Samples

	{ "xm_c-pr-.ic1",	0x01000, 0xc8ceb3cd, 0x07 | BRF_PRG },           // 21 i8751 microcontroller

	{ "m72_a-8l-.ic66",	0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 22 Proms
	{ "m72_a-9l-.ic75",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 23

	{ "m72_a-3d-.ic11",	0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 24 PLDs
	{ "m72_a-4d-.ic19",	0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 25
	{ "xm_c-3f-.ic13",	0x00117, 0xf43e91e4, 0x00 | BRF_OPT },           // 26
};

STD_ROM_PICK(xmultiplm72)
STD_ROM_FN(xmultiplm72)

static INT32 xmultiplm72Init()
{
	return DrvInit(common_080000_080000, sound_ram_map, NULL, Z80_FAKE_NMI, 0, 0);
}

struct BurnDriver BurnDrvXmultiplm72 = {
	"xmultiplm72", "xmultipl", NULL, NULL, "1989",
	"X Multiply (Japan, M72 hardware)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, xmultiplm72RomInfo, xmultiplm72RomName, NULL, NULL, NULL, NULL, CommonInputInfo, XmultiplDIPInfo,
	xmultiplm72Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Dragon Breed (World, M81 hardware)

static struct BurnRomInfo dbreedRomDesc[] = {
	{ "db_a-h0-.ic59",		0x20000, 0xe1177267, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "db_a-l0-.ic68",		0x20000, 0xd82b167e, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "db_a-sp-.ic14",		0x10000, 0x54a61560, 0x06 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "db_k800m.00.ic2",	0x20000, 0xc027a8cf, 0x02 | BRF_GRA },           //  3 Sprites
	{ "db_k801m.10.ic5",	0x20000, 0x093faf33, 0x02 | BRF_GRA },           //  4
	{ "db_k802m.20.ic12",	0x20000, 0x055b4c59, 0x02 | BRF_GRA },           //  5
	{ "db_k803m.30.ic14",	0x20000, 0x8ed63922, 0x02 | BRF_GRA },           //  6

	{ "db_k804m.a0.ic50",	0x20000, 0x4c83e92e, 0x03 | BRF_GRA },           //  7 Foreground & Background Tiles
	{ "db_k805m.a1.ic49",	0x20000, 0x835ef268, 0x03 | BRF_GRA },           //  8
	{ "db_k806m.a2.ic51",	0x20000, 0x5117f114, 0x03 | BRF_GRA },           //  9
	{ "db_k807m.a3.ic52",	0x20000, 0x8eb0c978, 0x03 | BRF_GRA },           // 10

	{ "db_a-v0-.ic11",		0x20000, 0x312f7282, 0x05 | BRF_SND },           // 11 DAC Samples

	{ "m81_a-9l-.ic72",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 12 Proms
	{ "m81_a-9p-.ic74",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 13

	{ "m81_b-9h-.ic94",		0x00117, 0x4bc1d393, 0x00 | BRF_OPT },           // 14 PLDs
	{ "m81_a-5l-.ic28",		0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 15
	{ "m81_a-2h-.ic2",		0x00117, 0x21ede612, 0x00 | BRF_OPT },           // 16
	{ "db_a-7d-.ic48",		0x00117, 0x421cecd6, 0x00 | BRF_OPT },           // 17
};

STD_ROM_PICK(dbreed)
STD_ROM_FN(dbreed)

static INT32 dbreedRomLoadCallback()
{
	memcpy (DrvV30ROM + 0x60000, DrvV30ROM + 0x20000, 0x20000);

	return 0;
}

static INT32 dbreedInit()
{
	return DrvInit(common_080000_088000, sound_rom_map, dbreedRomLoadCallback, Z80_REAL_NMI, 2, 0);
}

struct BurnDriver BurnDrvDbreed = {
	"dbreed", NULL, NULL, NULL, "1989",
	"Dragon Breed (World, M81 hardware)\0", NULL, "Irem", "Irem M81",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, dbreedRomInfo, dbreedRomName, NULL, NULL, NULL, NULL, CommonInputInfo, DbreedDIPInfo,
	dbreedInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Dragon Breed (World, M72 hardware)

static struct BurnRomInfo dbreedm72RomDesc[] = {
	{ "db_c-h3-b.ic43",		0x20000, 0x4bf3063c, 0x01 | BRF_PRG | BRF_ESS },    //  0 V30 Code
	{ "db_c-l3-b.ic34",		0x20000, 0xe4b89b79, 0x01 | BRF_PRG | BRF_ESS },    //  1
	{ "db_c-h0-.ic40",		0x10000, 0x5aa79fb2, 0x01 | BRF_PRG | BRF_ESS },    //  2
	{ "db_c-l0-.ic37",		0x10000, 0xed0f5e06, 0x01 | BRF_PRG | BRF_ESS },    //  3

	{ "db_k800m.00.ic53",	0x20000, 0xc027a8cf, 0x02 | BRF_GRA },              //  4 Sprites
	{ "db_k801m.10.ic51",	0x20000, 0x093faf33, 0x02 | BRF_GRA },              //  5
	{ "db_k802m.20.ic49",	0x20000, 0x055b4c59, 0x02 | BRF_GRA },              //  6
	{ "db_k803m.30.ic47",	0x20000, 0x8ed63922, 0x02 | BRF_GRA },              //  7

	{ "db_k804m.a0.ic21",	0x20000, 0x4c83e92e, 0x03 | BRF_GRA },              //  8 Foreground Tiles
	{ "db_k805m.a1.ic22",	0x20000, 0x835ef268, 0x03 | BRF_GRA },              //  9
	{ "db_k806m.a2.ic20",	0x20000, 0x5117f114, 0x03 | BRF_GRA },              // 10
	{ "db_k807m.a3.ic23",	0x20000, 0x8eb0c978, 0x03 | BRF_GRA },              // 11

	{ "db_k804m.b0.ic26",	0x20000, 0x4c83e92e, 0x04 | BRF_GRA },              // 12 Background Tiles
	{ "db_k805m.b1.ic27",	0x20000, 0x835ef268, 0x04 | BRF_GRA },              // 13
	{ "db_k806m.b2.ic25",	0x20000, 0x5117f114, 0x04 | BRF_GRA },              // 14
	{ "db_k807m.b3.ic24",	0x20000, 0x8eb0c978, 0x04 | BRF_GRA },              // 15

	{ "db_c-v0.ic44",		0x20000, 0x312f7282, 0x05 | BRF_SND },              // 16 DAC Samples

	{ "db_c-pr-b.ic1",		0x01000, 0x00000000, 0x00 | BRF_PRG | BRF_NODUMP }, // 17 i8751 microcontroller

	{ "m72_a-8l-.ic66",		0x00100, 0xb460c438, 0x00 | BRF_OPT },              // 18 Proms
	{ "m72_a-9l-.ic75",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },              // 19

	{ "m72_a-3d-.ic11",		0x00117, 0x8a3732ff, 0x00 | BRF_OPT },              // 20 PLDs
	{ "m72_a-4d-.ic19",		0x00117, 0x56c29834, 0x00 | BRF_OPT },              // 21
	{ "db-c-3f-.ic13",		0x00117, 0x7184d421, 0x00 | BRF_OPT },              // 22
};

STD_ROM_PICK(dbreedm72)
STD_ROM_FN(dbreedm72)

static INT32 dbreedm72RomLoadCallback()
{
	memmove(DrvV30ROM + 0x60000, DrvV30ROM + 0x40000, 0x20000);

	return 0;
}

static INT32 dbreedm72Init()
{
	install_protection(dbreedm72);

	return DrvInit(common_080000_090000, sound_ram_map, dbreedm72RomLoadCallback, Z80_FAKE_NMI, 0, 0);
}

struct BurnDriver BurnDrvDbreedm72 = {
	"dbreedm72", "dbreed", NULL, NULL, "1989",
	"Dragon Breed (World, M72 hardware)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, dbreedm72RomInfo, dbreedm72RomName, NULL, NULL, NULL, NULL, CommonInputInfo, DbreedDIPInfo,
	dbreedm72Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Dragon Breed (Japan, M72 harware)
// with matching i8751 this set boots with a Japanese warning screen

static struct BurnRomInfo dbreedjm72RomDesc[] = {
	{ "db_c-h3-.ic43",		0x20000, 0x43425d67, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "db_c-l3-.ic34",		0x20000, 0x9c1abc85, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "db_c-h0-.ic40",		0x10000, 0x5aa79fb2, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "db_c-l0-.ic37",		0x10000, 0xed0f5e06, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "db_k800m.00.ic53",	0x20000, 0xc027a8cf, 0x02 | BRF_GRA },           //  4 Sprites
	{ "db_k801m.10.ic51",	0x20000, 0x093faf33, 0x02 | BRF_GRA },           //  5
	{ "db_k802m.20.ic49",	0x20000, 0x055b4c59, 0x02 | BRF_GRA },           //  6
	{ "db_k803m.30.ic47",	0x20000, 0x8ed63922, 0x02 | BRF_GRA },           //  7

	{ "db_k804m.a0.ic21",	0x20000, 0x4c83e92e, 0x03 | BRF_GRA },           //  8 Foreground Tiles
	{ "db_k805m.a1.ic22",	0x20000, 0x835ef268, 0x03 | BRF_GRA },           //  9
	{ "db_k806m.a2.ic20",	0x20000, 0x5117f114, 0x03 | BRF_GRA },           // 10
	{ "db_k807m.a3.ic23",	0x20000, 0x8eb0c978, 0x03 | BRF_GRA },           // 11

	{ "db_k804m.b0.ic26",	0x20000, 0x4c83e92e, 0x04 | BRF_GRA },           // 12 Background Tiles
	{ "db_k805m.b1.ic27",	0x20000, 0x835ef268, 0x04 | BRF_GRA },           // 13
	{ "db_k806m.b2.ic25",	0x20000, 0x5117f114, 0x04 | BRF_GRA },           // 14
	{ "db_k807m.b3.ic24",	0x20000, 0x8eb0c978, 0x04 | BRF_GRA },           // 15

	{ "db_c-v0.ic44",		0x20000, 0x312f7282, 0x05 | BRF_SND },           // 16 DAC Samples

	{ "db_c-pr-.ic1",		0x01000, 0x8bf2910c, 0x07 | BRF_PRG },           // 17 i8751 microcontroller

	{ "m72_a-8l-.ic66",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 18 Proms
	{ "m72_a-9l-.ic75",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 19

	{ "m72_a-3d-.ic11",		0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 20 PLDs
	{ "m72_a-4d-.ic19",		0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 21
	{ "db-c-3f-.ic13",		0x00117, 0x7184d421, 0x00 | BRF_OPT },           // 22
};

STD_ROM_PICK(dbreedjm72)
STD_ROM_FN(dbreedjm72)

struct BurnDriver BurnDrvDbreedjm72 = {
	"dbreedjm72", "dbreed", NULL, NULL, "1989",
	"Dragon Breed (Japan, M72 hardware)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, dbreedjm72RomInfo, dbreedjm72RomName, NULL, NULL, NULL, NULL, CommonInputInfo, DbreedDIPInfo,
	dbreedm72Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Battle Chopper (World)

static struct BurnRomInfo bchopperRomDesc[] = {
	{ "mh_c-h0-b.ic40",		0x10000, 0xf2feab16, 0x01 | BRF_PRG | BRF_ESS },    //  0 V30 Code
	{ "mh_c-l0-b.ic37",		0x10000, 0x9f887096, 0x01 | BRF_PRG | BRF_ESS },    //  1
	{ "mh_c-h1-b.ic41",		0x10000, 0xa995d64f, 0x01 | BRF_PRG | BRF_ESS },    //  2
	{ "mh_c-l1-b.ic36",		0x10000, 0x41dda999, 0x01 | BRF_PRG | BRF_ESS },    //  3
	{ "mh_c-h3-b.ic43",		0x10000, 0xab9451ca, 0x01 | BRF_PRG | BRF_ESS },    //  4
	{ "mh_c-l3-b.ic34",		0x10000, 0x11562221, 0x01 | BRF_PRG | BRF_ESS },    //  5

	{ "mh_c-00-a.ic53",		0x10000, 0xf6e6e660, 0x02 | BRF_GRA },              //  6 Sprites
	{ "mh_c-01-b.ic52",		0x10000, 0x708cdd37, 0x02 | BRF_GRA },              //  7
	{ "mh_c-10-a.ic51",		0x10000, 0x292c8520, 0x02 | BRF_GRA },              //  8
	{ "mh_c-11-b.ic50",		0x10000, 0x20904cf3, 0x02 | BRF_GRA },              //  9
	{ "mh_c-20-a.ic49",		0x10000, 0x1ab50c23, 0x02 | BRF_GRA },              // 10
	{ "mh_c-21-b.ic48",		0x10000, 0xc823d34c, 0x02 | BRF_GRA },              // 11
	{ "mh_c-30-a.ic47",		0x10000, 0x11f6c56b, 0x02 | BRF_GRA },              // 12
	{ "mh_c-31-b.ic46",		0x10000, 0x23134ec5, 0x02 | BRF_GRA },              // 13

	{ "mh_b-a0-b.ic21",		0x10000, 0xe46ed7bf, 0x03 | BRF_GRA },              // 14 Foreground Tiles
	{ "mh_b-a1-b.ic22",		0x10000, 0x590605ff, 0x03 | BRF_GRA },              // 15
	{ "mh_b-a2-b.ic20",		0x10000, 0xf8158226, 0x03 | BRF_GRA },              // 16
	{ "mh_b-a3-b.ic23",		0x10000, 0x0f07b9b7, 0x03 | BRF_GRA },              // 17

	{ "mh_b-b0-.ic26",		0x10000, 0xb5b95776, 0x04 | BRF_GRA },              // 18 Background Tiles
	{ "mh_b-b1-.ic27",		0x10000, 0x74ca16ee, 0x04 | BRF_GRA },              // 19
	{ "mh_b-b2-.ic25",		0x10000, 0xb82cca04, 0x04 | BRF_GRA },              // 20
	{ "mh_b-b3-.ic24",		0x10000, 0xa7afc920, 0x04 | BRF_GRA },              // 21

	{ "mh_c-v0-b.ic44",		0x10000, 0xd0c27e58, 0x05 | BRF_SND },              // 22 DAC Samples

	{ "mh_c-pr-b.ic1",		0x01000, 0x9d201fea, 0x07 | BRF_PRG }, 			 	// 23 i8751 microcontroller

	{ "m72_a-8l-.ic66",		0x00100, 0xb460c438, 0x00 | BRF_OPT },              // 24 Proms
	{ "m72_a-9l-.ic75",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },              // 25

	{ "m72_a-3d-.ic11",		0x00117, 0x8a3732ff, 0x00 | BRF_OPT },              // 26 PLDs
	{ "m72_a-4d-.ic19",		0x00117, 0x56c29834, 0x00 | BRF_OPT },              // 27
	{ "mh-c-3f-.ic13",		0x00117, 0x2d774e1e, 0x00 | BRF_OPT },              // 28
};

STD_ROM_PICK(bchopper)
STD_ROM_FN(bchopper)

static INT32 bchopperInit()
{
	return DrvInit(common_080000_0a0000, sound_ram_map, dbreedm72RomLoadCallback, Z80_FAKE_NMI, 0, 4);
}

struct BurnDriver BurnDrvBchopper = {
	"bchopper", NULL, NULL, NULL, "1987",
	"Battle Chopper (World)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, bchopperRomInfo, bchopperRomName, NULL, NULL, NULL, NULL, CommonInputInfo, BchopperDIPInfo,
	bchopperInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Mr. HELI no Daibouken (Japan)

static struct BurnRomInfo mrheliRomDesc[] = {
	{ "mh_c-h0-.ic40",	0x10000, 0xe2ca5646, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "mh_c-l0-.ic37",	0x10000, 0x643e23cd, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "mh_c-h1-.ic41",	0x10000, 0x8974e84d, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "mh_c-l1-.ic36",	0x10000, 0x5f8bda69, 0x01 | BRF_PRG | BRF_ESS }, //  3
	{ "mh_c-h3-.ic43",	0x10000, 0x143f596e, 0x01 | BRF_PRG | BRF_ESS }, //  4
	{ "mh_c-l3-.ic34",	0x10000, 0xc0982536, 0x01 | BRF_PRG | BRF_ESS }, //  5

	{ "mh_c-00.ic53",	0x20000, 0xdec4e121, 0x02 | BRF_GRA },           //  6 Sprites
	{ "mh_c-10.ic51",	0x20000, 0x7aaa151e, 0x02 | BRF_GRA },           //  7
	{ "mh_c-20.ic49",	0x20000, 0xeae0de74, 0x02 | BRF_GRA },           //  8
	{ "mh_c-30.ic47",	0x20000, 0x01d5052f, 0x02 | BRF_GRA },           //  9

	{ "mh_b-a0.ic21",	0x10000, 0x6a0db256, 0x03 | BRF_GRA },           // 10 Foreground Tiles
	{ "mh_b-a1.ic22",	0x10000, 0x14ec9795, 0x03 | BRF_GRA },           // 11
	{ "mh_b-a2.ic20",	0x10000, 0xdfcb510e, 0x03 | BRF_GRA },           // 12
	{ "mh_b-a3.ic23",	0x10000, 0x957e329b, 0x03 | BRF_GRA },           // 13

	{ "mh_b-b0-.ic26",	0x10000, 0xb5b95776, 0x04 | BRF_GRA },           // 14 Background Tiles
	{ "mh_b-b1-.ic27",	0x10000, 0x74ca16ee, 0x04 | BRF_GRA },           // 15
	{ "mh_b-b2-.ic25",	0x10000, 0xb82cca04, 0x04 | BRF_GRA },           // 16
	{ "mh_b-b3-.ic24",	0x10000, 0xa7afc920, 0x04 | BRF_GRA },           // 17

	{ "mh_c-v0-b.ic44",	0x10000, 0xd0c27e58, 0x05 | BRF_SND },           // 18 DAC Samples

	{ "mh_c-pr-.ic1",	0x01000, 0x897dc4ee, 0x07 | BRF_PRG }, 			 // 19 i8751 microcontroller

	{ "m72_a-8l-.ic66",	0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 20 Proms
	{ "m72_a-9l-.ic75",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 21

	{ "m72_a-3d-.ic11",	0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 22 PLDs
	{ "m72_a-4d-.ic19",	0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 23
	{ "mh-c-3f-.ic13",	0x00117, 0x2d774e1e, 0x00 | BRF_OPT },           // 24
};

STD_ROM_PICK(mrheli)
STD_ROM_FN(mrheli)

static INT32 mrheliInit()
{
	return DrvInit(common_080000_0a0000, sound_ram_map, dbreedm72RomLoadCallback, Z80_FAKE_NMI, 0, 4);
}

struct BurnDriver BurnDrvMrheli = {
	"mrheli", "bchopper", NULL, NULL, "1987",
	"Mr. HELI no Daibouken (Japan)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, mrheliRomInfo, mrheliRomName, NULL, NULL, NULL, NULL, CommonInputInfo, BchopperDIPInfo,
	mrheliInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Ninja Spirit (World)
// doesn't wait / check for japan warning string.. fails rom check if used with japanese mcu rom (World version?)

static struct BurnRomInfo nspiritRomDesc[] = {
	{ "nin_c-h0-b.ic40",	0x10000, 0x035692fa, 0x01 | BRF_PRG | BRF_ESS },    //  0 V30 Code
	{ "nin_c-l0-b.ic37",	0x10000, 0x9a405898, 0x01 | BRF_PRG | BRF_ESS },    //  1
	{ "nin_c-h1-.ic41",		0x10000, 0xcbc10586, 0x01 | BRF_PRG | BRF_ESS },    //  2
	{ "nin_c-l1-.ic36",		0x10000, 0xb75c9a4d, 0x01 | BRF_PRG | BRF_ESS },    //  3
	{ "nin_c-h2-.ic42",		0x10000, 0x8ad818fa, 0x01 | BRF_PRG | BRF_ESS },    //  4
	{ "nin_c-l2-.ic35",		0x10000, 0xc52ca78c, 0x01 | BRF_PRG | BRF_ESS },    //  5
	{ "nin_c-h3-b.ic43",	0x10000, 0x501104ef, 0x01 | BRF_PRG | BRF_ESS },    //  6
	{ "nin_c-l3-b.ic34",	0x10000, 0xfd7408b8, 0x01 | BRF_PRG | BRF_ESS },    //  7

	{ "nin-r00.ic53",		0x20000, 0x5f61d30b, 0x02 | BRF_GRA },              //  8 Sprites
	{ "nin-r10.ic51",		0x20000, 0x0caad107, 0x02 | BRF_GRA },              //  9
	{ "nin-r20.ic49",		0x20000, 0xef3617d3, 0x02 | BRF_GRA },              // 10
	{ "nin-r30.ic47",		0x20000, 0x175d2a24, 0x02 | BRF_GRA },              // 11

	{ "nin_b-a0.ic21",		0x10000, 0x63f8f658, 0x03 | BRF_GRA },              // 12 Foreground Tiles
	{ "nin_b-a1.ic22",		0x10000, 0x75eb8306, 0x03 | BRF_GRA },              // 13
	{ "nin_b-a2.ic20",		0x10000, 0xdf532172, 0x03 | BRF_GRA },              // 14
	{ "nin_b-a3.ic23",		0x10000, 0x4dedd64c, 0x03 | BRF_GRA },              // 15

	{ "b0.ic26",			0x10000, 0x1b0e08a6, 0x04 | BRF_GRA },              // 16 Background Tiles
	{ "b1.ic27",			0x10000, 0x728727f0, 0x04 | BRF_GRA },              // 17
	{ "b2.ic25",			0x10000, 0xf87efd75, 0x04 | BRF_GRA },              // 18
	{ "b3.ic24",			0x10000, 0x98856cb4, 0x04 | BRF_GRA },              // 19

	{ "nin-v0.ic44",		0x10000, 0xa32e8caf, 0x05 | BRF_SND },              // 20 DAC Samples

	{ "nin_c-pr-b.ic1",		0x01000, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP }, // 21 i8751 microcontroller

	{ "m72_a-8l-.ic66",		0x00100, 0xb460c438, 0x00 | BRF_OPT },              // 22 Proms
	{ "m72_a-9l-.ic75",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },              // 23

	{ "m72_a-3d-.ic11",		0x00117, 0x8a3732ff, 0x00 | BRF_OPT },              // 24 PLDs
	{ "m72_a-4d-.ic19",		0x00117, 0x56c29834, 0x00 | BRF_OPT },              // 25
	{ "nin-c-3f.ic13",		0x00117, 0xd1f3a2e2, 0x00 | BRF_OPT },              // 26
};

STD_ROM_PICK(nspirit)
STD_ROM_FN(nspirit)

static INT32 nspiritInit()
{
	install_protection(nspirit);

	return DrvInit(common_080000_0a0000, sound_ram_map, NULL, Z80_FAKE_NMI, 0, 3);
}

struct BurnDriver BurnDrvNspirit = {
	"nspirit", NULL, NULL, NULL, "1988",
	"Ninja Spirit (World)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_SCRFIGHT, 0,
	NULL, nspiritRomInfo, nspiritRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NspiritDIPInfo,
	nspiritInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Saigo no Nindou (Japan)

static struct BurnRomInfo nspiritjRomDesc[] = {
	{ "nin_c-h0-.ic40",		0x10000, 0x8603fab2, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "nin_c-l0-.ic37",		0x10000, 0xe520fa35, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "nin_c-h1-.ic41",		0x10000, 0xcbc10586, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "nin_c-l1-.ic36",		0x10000, 0xb75c9a4d, 0x01 | BRF_PRG | BRF_ESS }, //  3
	{ "nin_c-h2-.ic42",		0x10000, 0x8ad818fa, 0x01 | BRF_PRG | BRF_ESS }, //  4
	{ "nin_c-l2-.ic35",		0x10000, 0xc52ca78c, 0x01 | BRF_PRG | BRF_ESS }, //  5
	{ "nin_c-h3-.ic43",		0x10000, 0x95b63a61, 0x01 | BRF_PRG | BRF_ESS }, //  6
	{ "nin_c-l3-.ic34",		0x10000, 0xe754a87a, 0x01 | BRF_PRG | BRF_ESS }, //  7

	{ "nin-r00.ic53",		0x20000, 0x5f61d30b, 0x02 | BRF_GRA },           //  8 Sprites
	{ "nin-r10.ic51",		0x20000, 0x0caad107, 0x02 | BRF_GRA },           //  9
	{ "nin-r20.ic49",		0x20000, 0xef3617d3, 0x02 | BRF_GRA },           // 10
	{ "nin-r30.ic47",		0x20000, 0x175d2a24, 0x02 | BRF_GRA },           // 11

	{ "nin_b-a0.ic21",		0x10000, 0x63f8f658, 0x03 | BRF_GRA },           // 12 Foreground Tiles
	{ "nin_b-a1.ic22",		0x10000, 0x75eb8306, 0x03 | BRF_GRA },           // 13
	{ "nin_b-a2.ic20",		0x10000, 0xdf532172, 0x03 | BRF_GRA },           // 14
	{ "nin_b-a3.ic23",		0x10000, 0x4dedd64c, 0x03 | BRF_GRA },           // 15

	{ "b0.ic26",			0x10000, 0x1b0e08a6, 0x04 | BRF_GRA },           // 16 Background Tiles
	{ "b1.ic27",			0x10000, 0x728727f0, 0x04 | BRF_GRA },           // 17
	{ "b2.ic25",			0x10000, 0xf87efd75, 0x04 | BRF_GRA },           // 18
	{ "b3.ic24",			0x10000, 0x98856cb4, 0x04 | BRF_GRA },           // 19

	{ "nin-v0.ic44",		0x10000, 0xa32e8caf, 0x05 | BRF_SND },           // 20 DAC Samples

	{ "nin_c-pr-.ic1",		0x01000, 0x802d440a, 0x07 | BRF_PRG },           // 21 i8751 microcontroller

	{ "m72_a-8l-.ic66",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 22 Proms
	{ "m72_a-9l-.ic75",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 23

	{ "m72_a-3d-.ic11",		0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 24 PLDs
	{ "m72_a-4d-.ic19",		0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 25
	{ "nin-c-3f.ic13",		0x00117, 0xd1f3a2e2, 0x00 | BRF_OPT },           // 26
};

STD_ROM_PICK(nspiritj)
STD_ROM_FN(nspiritj)

static INT32 nspiritjInit()
{
	return DrvInit(common_080000_0a0000, sound_ram_map, NULL, Z80_FAKE_NMI, 0, 3);
}

struct BurnDriver BurnDrvNspiritj = {
	"nspiritj", "nspirit", NULL, NULL, "1988",
	"Saigo no Nindou (Japan)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_SCRFIGHT, 0,
	NULL, nspiritjRomInfo, nspiritjRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NspiritDIPInfo,
	nspiritjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Image Fight (World)

static struct BurnRomInfo imgfightRomDesc[] = {
	{ "if_c-h0-a.ic40",	0x10000, 0xf5c94464, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "if_c-l0-a.ic37",	0x10000, 0x87c534fe, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "if_c-h3-.ic43",	0x20000, 0xea030541, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "if_c-l3-.ic34",	0x20000, 0xc66ae348, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "if-c-00.ic53",	0x20000, 0x745e6638, 0x02 | BRF_GRA },           //  4 Sprites
	{ "if-c-10.ic51",	0x20000, 0xb7108449, 0x02 | BRF_GRA },           //  5
	{ "if-c-20.ic49",	0x20000, 0xaef33cba, 0x02 | BRF_GRA },           //  6
	{ "if-c-30.ic47",	0x20000, 0x1f98e695, 0x02 | BRF_GRA },           //  7

	{ "if_b-a0.ic21",	0x10000, 0x34ee2d77, 0x03 | BRF_GRA },           //  8 Foreground Tiles
	{ "if_b-a1.ic22",	0x10000, 0x6bd2845b, 0x03 | BRF_GRA },           //  9
	{ "if_b-a2.ic20",	0x10000, 0x090d50e5, 0x03 | BRF_GRA },           // 10
	{ "if_b-a3.ic23",	0x10000, 0x3a8e3083, 0x03 | BRF_GRA },           // 11

	{ "if_b-b0.ic26",	0x10000, 0xb425c829, 0x04 | BRF_GRA },           // 12 Background Tiles
	{ "if_b-b1.ic27",	0x10000, 0xe9bfe23e, 0x04 | BRF_GRA },           // 13
	{ "if_b-b2.ic25",	0x10000, 0x256e50f2, 0x04 | BRF_GRA },           // 14
	{ "if_b-b3.ic24",	0x10000, 0x4c682785, 0x04 | BRF_GRA },           // 15

	{ "if_c-v0.ic44",	0x10000, 0xcb64a194, 0x05 | BRF_SND },           // 16 DAC Samples
	{ "if_c-v1.ic45",	0x10000, 0x45b68bf5, 0x05 | BRF_SND },           // 17

	{ "if_c-pr-a.ic1",  0x01000, 0x55f10458, 0x07 | BRF_PRG },           // 18 i8751 microcontroller

	{ "m72_a-8l-.ic66",	0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 19 Proms
	{ "m72_a-9l-.ic75",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 20

	{ "m72_a-3d-.ic11",	0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 21 PLDs
	{ "m72_a-4d-.ic19",	0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 22
	{ "if-c-3f-.ic13",	0x00117, 0x2d774e1e, 0x00 | BRF_OPT },           // 23
};

STD_ROM_PICK(imgfight)
STD_ROM_FN(imgfight)

static INT32 imgfightRomLoadCallback()
{
	memmove(DrvV30ROM + 0x40000, DrvV30ROM + 0x20000, 0x40000);

	return 0;
}

static INT32 imgfightInit()
{
	return DrvInit(common_080000_0a0000, sound_ram_map, imgfightRomLoadCallback, Z80_FAKE_NMI, 0, 2);
}

struct BurnDriver BurnDrvImgfight = {
	"imgfight", NULL, NULL, NULL, "1988",
	"Image Fight (World)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_IREM_M72, GBF_VERSHOOT, 0,
	NULL, imgfightRomInfo, imgfightRomName, NULL, NULL, NULL, NULL, CommonInputInfo, ImgfightDIPInfo,
	imgfightInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 384, 3, 4
};


// Image Fight (Japan)

static struct BurnRomInfo imgfightjRomDesc[] = {
	{ "if_c-h0-.ic40",	0x10000, 0x592d2d80, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "if_c-l0-.ic37",	0x10000, 0x61f89056, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "if_c-h3-.ic43",	0x20000, 0xea030541, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "if_c-l3-.ic34",	0x20000, 0xc66ae348, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "if-c-00.ic53",	0x20000, 0x745e6638, 0x02 | BRF_GRA },           //  4 Sprites
	{ "if-c-10.ic51",	0x20000, 0xb7108449, 0x02 | BRF_GRA },           //  5
	{ "if-c-20.ic49",	0x20000, 0xaef33cba, 0x02 | BRF_GRA },           //  6
	{ "if-c-30.ic47",	0x20000, 0x1f98e695, 0x02 | BRF_GRA },           //  7

	{ "if_b-a0.ic21",	0x10000, 0x34ee2d77, 0x03 | BRF_GRA },           //  8 Foreground Tiles
	{ "if_b-a1.ic22",	0x10000, 0x6bd2845b, 0x03 | BRF_GRA },           //  9
	{ "if_b-a2.ic20",	0x10000, 0x090d50e5, 0x03 | BRF_GRA },           // 10
	{ "if_b-a3.ic23",	0x10000, 0x3a8e3083, 0x03 | BRF_GRA },           // 11

	{ "if_b-b0.ic26",	0x10000, 0xb425c829, 0x04 | BRF_GRA },           // 12 Background Tiles
	{ "if_b-b1.ic27",	0x10000, 0xe9bfe23e, 0x04 | BRF_GRA },           // 13
	{ "if_b-b2.ic25",	0x10000, 0x256e50f2, 0x04 | BRF_GRA },           // 14
	{ "if_b-b3.ic24",	0x10000, 0x4c682785, 0x04 | BRF_GRA },           // 15

	{ "if_c-v0.ic44",	0x10000, 0xcb64a194, 0x05 | BRF_SND },           // 16 DAC Samples
	{ "if_c-v1.ic45",	0x10000, 0x45b68bf5, 0x05 | BRF_SND },           // 17

	{ "if_c-pr-.ic1",	0x01000, 0xef0d5098, 0x07 | BRF_PRG }, 			 // 18 i8751 microcontroller

	{ "m72_a-8l-.ic66",	0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 19 Proms
	{ "m72_a-9l-.ic75",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 20

	{ "m72_a-3d-.ic11",	0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 21 PLDs
	{ "m72_a-4d-.ic19",	0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 22
	{ "if-c-3f-.ic13",	0x00117, 0x2d774e1e, 0x00 | BRF_OPT },           // 23
};

STD_ROM_PICK(imgfightj)
STD_ROM_FN(imgfightj)

struct BurnDriver BurnDrvImgfightj = {
	"imgfightj", "imgfight", NULL, NULL, "1988",
	"Image Fight (Japan)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_IREM_M72, GBF_VERSHOOT, 0,
	NULL, imgfightjRomInfo, imgfightjRomName, NULL, NULL, NULL, NULL, CommonInputInfo, ImgfightDIPInfo,
	imgfightInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 384, 3, 4
};


// Image Fight (Japan, bootleg)

static struct BurnRomInfo imgfightjbRomDesc[] = {
	{ "ic108.9b",		0x10000, 0x592d2d80, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "ic89.7b",		0x10000, 0x61f89056, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ic111.9e",		0x10000, 0x6aae3a46, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "ic92.7e",		0x10000, 0x38fce272, 0x01 | BRF_PRG | BRF_ESS }, //  3
	{ "ic110.9d",		0x10000, 0x0e0aefcd, 0x01 | BRF_PRG | BRF_ESS }, //  4
	{ "ic91.7d",		0x10000, 0xd69c0722, 0x01 | BRF_PRG | BRF_ESS }, //  5

	{ "ic96.7k",		0x10000, 0xd4febb03, 0x02 | BRF_GRA },           //  6 Sprites
	{ "ic97.7l",		0x10000, 0x973d7bbc, 0x02 | BRF_GRA },           //  7
	{ "ic115.9k",		0x10000, 0x2328880b, 0x02 | BRF_GRA },           //  8
	{ "ic116.9l",		0x10000, 0x6da001ea, 0x02 | BRF_GRA },           //  9
	{ "ic94.7h",		0x10000, 0x92bc7fda, 0x02 | BRF_GRA },           // 10
	{ "ic95.7j",		0x10000, 0xe63a5918, 0x02 | BRF_GRA },           // 11
	{ "ic113.9h",		0x10000, 0x27caec8e, 0x02 | BRF_GRA },           // 12
	{ "ic114.9j",		0x10000, 0x1933eb65, 0x02 | BRF_GRA },           // 13

	{ "ic30.3d",		0x10000, 0x34ee2d77, 0x03 | BRF_GRA },           // 14 Foreground Tiles
	{ "ic31.3e",		0x10000, 0x6bd2845b, 0x03 | BRF_GRA },           // 15
	{ "ic29.3c",		0x10000, 0x090d50e5, 0x03 | BRF_GRA },           // 16
	{ "ic32.3f",		0x10000, 0x3a8e3083, 0x03 | BRF_GRA },           // 17

	{ "ic35.3k",		0x10000, 0xb425c829, 0x04 | BRF_GRA },           // 18 Background Tiles
	{ "ic36.3l",		0x10000, 0xe9bfe23e, 0x04 | BRF_GRA },           // 19
	{ "ic34.3j",		0x10000, 0x256e50f2, 0x04 | BRF_GRA },           // 20
	{ "ic33.3h",		0x10000, 0x4c682785, 0x04 | BRF_GRA },           // 21

	{ "ic28.lower.2n",	0x10000, 0xcb64a194, 0x05 | BRF_SND },           // 22 DAC Samples
	{ "ic28.upper.2n",	0x10000, 0x45b68bf5, 0x05 | BRF_SND },           // 23

	{ "25.ic27.2l",		0x02000, 0xd83359a2, 0x07 | BRF_PRG }, 			 // 24 i8751 microcontroller (i80c31 instead of i8751, contents identical to imgfightj MCU, with second half padded with 0xff)
};

STD_ROM_PICK(imgfightjb)
STD_ROM_FN(imgfightjb)

struct BurnDriver BurnDrvImgfightjb = {
	"imgfightjb", "imgfight", NULL, NULL, "1988",
	"Image Fight (Japan, bootleg)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_IREM_M72, GBF_VERSHOOT, 0,
	NULL, imgfightjbRomInfo, imgfightjbRomName, NULL, NULL, NULL, NULL, CommonInputInfo, ImgfightDIPInfo,
	imgfightInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 384, 3, 4
};


// Air Duel (World, M82 hardware)

static struct BurnRomInfo airduelRomDesc[] = {
	{ "ad_=m82=_a-h0-d.ic52",	0x20000, 0xdbecc726, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "ad_=m82=_a-l0-d.ic60",	0x20000, 0x6a9fcf59, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ad_=m82=_a-h1-d.ic51",	0x20000, 0xbafc152a, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "ad_=m82=_a-l1-d.ic59",	0x20000, 0x9e2b1ae7, 0x01 | BRF_PRG | BRF_ESS }, //  3
	
	{ "ad_=m82=_a-sp-d.ic15",	0x10000, 0x16a858a3, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "ad_=m82=_b-n0-d.ic44",	0x20000, 0x2f0d599b, 0x02 | BRF_GRA },           //  5 Sprites
	{ "ad_=m82=_b-n1-d.ic45",	0x20000, 0x9865856b, 0x02 | BRF_GRA },           //  6
	{ "ad_=m82=_b-n2-d.ic46",	0x20000, 0xd392aef2, 0x02 | BRF_GRA },           //  7
	{ "ad_=m82=_b-n3-d.ic36",	0x20000, 0x923240c3, 0x02 | BRF_GRA },           //  8

	{ "ad_=m82=_a-c0-d.ic49",	0x20000, 0xce134b47, 0x03 | BRF_GRA },           //  9 Foreground Tiles
	{ "ad_=m82=_a-c1-d.ic48",	0x20000, 0x097fd853, 0x03 | BRF_GRA },           // 10
	{ "ad_=m82=_a-c2-d.ic57",	0x20000, 0x6a94c1b9, 0x03 | BRF_GRA },           // 11
	{ "ad_=m82=_a-c3-d.ic56",	0x20000, 0x6637c349, 0x03 | BRF_GRA },           // 12

	{ "mt_f0.ic38",				0x20000, 0x2d5e05d5, 0x0e | BRF_GRA },           // 17 Sprites 2
	{ "mt_f1.ic39",				0x20000, 0xc68cd65f, 0x0e | BRF_GRA },           // 18
	{ "mt_f2.ic40",				0x20000, 0xa71feb2d, 0x0e | BRF_GRA },           // 19
	{ "mt_f3.ic41",				0x20000, 0x179f7562, 0x0e | BRF_GRA },           // 20

	{ "ad_=m82=_a-vo-d.ic12",	0x20000, 0x339f474d, 0x05 | BRF_SND },           // 21 DAC Samples

	{ "mt_a-2h-.ic5",			0x00117, 0x21ede612, 0x00 | BRF_OPT },           // 22 PLDs
	{ "mt_a-5l-.ic31",			0x00117, 0xe3064bfe, 0x00 | BRF_OPT },           // 23
	{ "mt_a-7d-.ic43",			0x00117, 0xd7ec1cc6, 0x00 | BRF_OPT },           // 24
};

STD_ROM_PICK(airduel)
STD_ROM_FN(airduel)

static INT32 airduelInit()
{
	return DrvInit(majtitle_main_cpu_map, sound_rom_map, NULL, Z80_REAL_NMI, 6, 0);
}

struct BurnDriver BurnDrvAirduel = {
	"airduel", NULL, NULL, NULL, "1990",
	"Air Duel (World, M82 hardware)\0", NULL, "Irem", "Irem M82",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_IREM_M72, GBF_VERSHOOT, 0,
	NULL, airduelRomInfo, airduelRomName, NULL, NULL, NULL, NULL, CommonInputInfo, AirduelDIPInfo,
	airduelInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 384, 3, 4
};


// Air Duel (US location test, M82 hardware)

static struct BurnRomInfo airdueluRomDesc[] = {
	{ "r10-m82a-h0.ic52",	0x20000, 0x17f19965, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "r10-m82a-l0.ic60",	0x20000, 0xf8b54d6c, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "r10-m82-h1.ic51",	0x20000, 0xbafc152a, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "r10-m82-l1.ic59",	0x20000, 0x9e2b1ae7, 0x01 | BRF_PRG | BRF_ESS }, //  3
	
	{ "r10-bgm.ic15",		0x10000, 0x16a858a3, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "r10-obj0.ic44",		0x20000, 0x2f0d599b, 0x02 | BRF_GRA },           //  5 Sprites
	{ "r10-obj1.ic45",		0x20000, 0x9865856b, 0x02 | BRF_GRA },           //  6
	{ "r10-obj2.ic46",		0x20000, 0xd392aef2, 0x02 | BRF_GRA },           //  7
	{ "r10-obj3.ic36",		0x20000, 0x923240c3, 0x02 | BRF_GRA },           //  8

	{ "r10-chr0.ic49",		0x20000, 0xce134b47, 0x03 | BRF_GRA },           //  9 Foreground Tiles
	{ "r10-chr1.ic48",		0x20000, 0x097fd853, 0x03 | BRF_GRA },           // 10
	{ "r10-chr2.ic57",		0x20000, 0x6a94c1b9, 0x03 | BRF_GRA },           // 11
	{ "r10-chr3.ic56",		0x20000, 0x6637c349, 0x03 | BRF_GRA },           // 12

	{ "mt_f0.ic38",			0x20000, 0x2d5e05d5, 0x0e | BRF_GRA },           // 17 Sprites 2
	{ "mt_f1.ic39",			0x20000, 0xc68cd65f, 0x0e | BRF_GRA },           // 18
	{ "mt_f2.ic40",			0x20000, 0xa71feb2d, 0x0e | BRF_GRA },           // 19
	{ "mt_f3.ic41",			0x20000, 0x179f7562, 0x0e | BRF_GRA },           // 20

	{ "r10-vo.ic12",		0x20000, 0x339f474d, 0x05 | BRF_SND },           // 21 DAC Samples

	{ "mt_a-2h-.ic5",		0x00117, 0x21ede612, 0x00 | BRF_OPT },           // 22 PLDs
	{ "mt_a-5l-.ic31",		0x00117, 0xe3064bfe, 0x00 | BRF_OPT },           // 23
	{ "mt_a-7d-.ic43",		0x00117, 0xd7ec1cc6, 0x00 | BRF_OPT },           // 24
};

STD_ROM_PICK(airduelu)
STD_ROM_FN(airduelu)

struct BurnDriver BurnDrvAirduelu = {
	"airduelu", "airduel", NULL, NULL, "1990",
	"Air Duel (US location test, M82 hardware)\0", NULL, "Irem America", "Irem M82",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED | BDF_PROTOTYPE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_IREM_M72, GBF_VERSHOOT, 0,
	NULL, airdueluRomInfo, airdueluRomName, NULL, NULL, NULL, NULL, CommonInputInfo, AirduelDIPInfo,
	airduelInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 384, 3, 4
};


// Air Duel (World, M72 hardware)

static struct BurnRomInfo airduelm72RomDesc[] = {
	{ "ad_c-h0-c.ic40",	0x20000, 0x6467ed0f, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "ad_c-l0-c.ic37",	0x20000, 0xb90c4ffd, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ad_c-h3-.ic43",	0x20000, 0x9f7cfca3, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "ad_c-l3-.ic34",	0x20000, 0x9dd343f7, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "ad-00.ic53",		0x20000, 0x2f0d599b, 0x02 | BRF_GRA },           //  4 Sprites
	{ "ad-10.ic51",		0x20000, 0x9865856b, 0x02 | BRF_GRA },           //  5
	{ "ad-20.ic49",		0x20000, 0xd392aef2, 0x02 | BRF_GRA },           //  6
	{ "ad-30.ic47",		0x20000, 0x923240c3, 0x02 | BRF_GRA },           //  7

	{ "ad-a0.ic21",		0x20000, 0xce134b47, 0x03 | BRF_GRA },           //  8 Foreground Tiles
	{ "ad-a1.ic22",		0x20000, 0x097fd853, 0x03 | BRF_GRA },           //  9
	{ "ad-a2.ic20",		0x20000, 0x6a94c1b9, 0x03 | BRF_GRA },           // 10
	{ "ad-a3.ic23",		0x20000, 0x6637c349, 0x03 | BRF_GRA },           // 11

	{ "ad-b0.ic26",		0x20000, 0xce134b47, 0x04 | BRF_GRA },           // 12 Background Tiles
	{ "ad-b1.ic27",		0x20000, 0x097fd853, 0x04 | BRF_GRA },           // 13
	{ "ad-b2.ic25",		0x20000, 0x6a94c1b9, 0x04 | BRF_GRA },           // 14
	{ "ad-b3.ic24",		0x20000, 0x6637c349, 0x04 | BRF_GRA },           // 15

	{ "ad-v0.ic44",		0x20000, 0x339f474d, 0x05 | BRF_SND },           // 16 DAC Samples

	{ "ad_c-pr-c.ic1",	0x01000, 0x8785e4e2, 0x07 | BRF_PRG },           // 17 i8751 microcontroller

	{ "m72_a-8l-.ic66",	0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 18 Proms
	{ "m72_a-9l-.ic75",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 19

	{ "m72_a-3d-.ic11",	0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 20 PLDs
	{ "m72_a-4d-.ic19",	0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 21
	{ "ad_c-3f-.ic13",	0x00117, 0x9748fa38, 0x00 | BRF_OPT },           // 22
};

STD_ROM_PICK(airduelm72)
STD_ROM_FN(airduelm72)

static INT32 airduelm72Init()
{
	airduelm72 = 1;

	return DrvInit(common_080000_0a0000, sound_ram_map, NULL, Z80_FAKE_NMI, 0, 0);
}

struct BurnDriver BurnDrvAirduelm72 = {
	"airduelm72", "airduel", NULL, NULL, "1990",
	"Air Duel (World, M72 hardware)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_VERSHOOT, 0,
	NULL, airduelm72RomInfo, airduelm72RomName, NULL, NULL, NULL, NULL, CommonInputInfo, AirduelDIPInfo,
	airduelm72Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 384, 3, 4
};


// Air Duel (Japan, M72 hardware)

static struct BurnRomInfo airdueljm72RomDesc[] = {
	{ "ad_c-h0-.ic40",	0x20000, 0x12140276, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "ad_c-l0-.ic37",	0x20000, 0x4ac0b91d, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "ad_c-h3-.ic43",	0x20000, 0x9f7cfca3, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "ad_c-l3-.ic34",	0x20000, 0x9dd343f7, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "ad-00.ic53",		0x20000, 0x2f0d599b, 0x02 | BRF_GRA },           //  4 Sprites
	{ "ad-10.ic51",		0x20000, 0x9865856b, 0x02 | BRF_GRA },           //  5
	{ "ad-20.ic49",		0x20000, 0xd392aef2, 0x02 | BRF_GRA },           //  6
	{ "ad-30.ic47",		0x20000, 0x923240c3, 0x02 | BRF_GRA },           //  7

	{ "ad-a0.ic21",		0x20000, 0xce134b47, 0x03 | BRF_GRA },           //  8 Foreground Tiles
	{ "ad-a1.ic22",		0x20000, 0x097fd853, 0x03 | BRF_GRA },           //  9
	{ "ad-a2.ic20",		0x20000, 0x6a94c1b9, 0x03 | BRF_GRA },           // 10
	{ "ad-a3.ic23",		0x20000, 0x6637c349, 0x03 | BRF_GRA },           // 11

	{ "ad-b0.ic26",		0x20000, 0xce134b47, 0x04 | BRF_GRA },           // 12 Background Tiles
	{ "ad-b1.ic27",		0x20000, 0x097fd853, 0x04 | BRF_GRA },           // 13
	{ "ad-b2.ic25",		0x20000, 0x6a94c1b9, 0x04 | BRF_GRA },           // 14
	{ "ad-b3.ic24",		0x20000, 0x6637c349, 0x04 | BRF_GRA },           // 15

	{ "ad-v0.ic44",		0x20000, 0x339f474d, 0x05 | BRF_SND },           // 16 DAC Samples

	{ "ad_c-pr-.ic1",	0x01000, 0x45584e52, 0x07 | BRF_PRG },           // 17 i8751 microcontroller

	{ "m72_a-8l-.ic66",	0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 18 Proms
	{ "m72_a-9l-.ic75",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 19

	{ "m72_a-3d-.ic11",	0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 20 PLDs
	{ "m72_a-4d-.ic19",	0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 21
	{ "ad_c-3f-.ic13",	0x00117, 0x9748fa38, 0x00 | BRF_OPT },           // 22
};

STD_ROM_PICK(airdueljm72)
STD_ROM_FN(airdueljm72)

struct BurnDriver BurnDrvAirdueljm72 = {
	"airdueljm72", "airduel", NULL, NULL, "1990",
	"Air Duel (Japan, M72 hardware)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_VERSHOOT, 0,
	NULL, airdueljm72RomInfo, airdueljm72RomName, NULL, NULL, NULL, NULL, CommonInputInfo, AirduelDIPInfo,
	airduelm72Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 384, 3, 4
};


// R-Type II (World)

static struct BurnRomInfo rtype2RomDesc[] = {
	{ "rt2_a-h0-d.ic54",	0x20000, 0xd8ece6f4, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rt2_a-l0-d.ic60",	0x20000, 0x32cfb2e4, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "rt2_a-h1-d.ic53",	0x20000, 0x4f6e9b15, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "rt2_a-l1-d.ic59",	0x20000, 0x0fd123bf, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "rt2_a-sp-.ic17",		0x10000, 0x73ffecb4, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rt2_b-n0.ic31",		0x20000, 0x2cd8f913, 0x02 | BRF_GRA },           //  5 Sprites
	{ "rt2_b-n1.ic21",		0x20000, 0x5033066d, 0x02 | BRF_GRA },           //  6
	{ "rt2_b-n2.ic32",		0x20000, 0xec3a0450, 0x02 | BRF_GRA },           //  7
	{ "rt2_b-n3.ic22",		0x20000, 0xdb6176fc, 0x02 | BRF_GRA },           //  8

	{ "rt2_a-g00.ic50",		0x20000, 0xf3f8736e, 0x03 | BRF_GRA },           //  9 Foreground & Background Tiles
	{ "rt2_a-g01.ic51",		0x20000, 0xb4c543af, 0x03 | BRF_GRA },           // 10
	{ "rt2_a-g10.ic56",		0x20000, 0x4cb80d66, 0x03 | BRF_GRA },           // 11
	{ "rt2_a-g11.ic57",		0x20000, 0xbee128e0, 0x03 | BRF_GRA },           // 12
	{ "rt2_a-g20.ic65",		0x20000, 0x2dc9c71a, 0x03 | BRF_GRA },           // 13
	{ "rt2_a-g21.ic66",		0x20000, 0x7533c428, 0x03 | BRF_GRA },           // 14
	{ "rt2_a-g30.ic63",		0x20000, 0xa6ad67f2, 0x03 | BRF_GRA },           // 15
	{ "rt2_a-g31.ic64",		0x20000, 0x3686d555, 0x03 | BRF_GRA },           // 16

	{ "rt2_a-vo-.ic14",		0x20000, 0x637172d5, 0x05 | BRF_SND },           // 17 DAC Samples

	{ "rt2_b-4n-.ic23",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 18 Proms
	{ "rt2_b-4p-.ic24",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 19

	{ "rt2_a-2h-.ic5",		0x00117, 0x21ede612, 0x00 | BRF_OPT },           // 20 PLDs
	{ "rt2_a-5l-.ic33",		0x00117, 0x86e87e50, 0x00 | BRF_OPT },           // 21
	{ "rt2_a-7d-.ic45",		0x00104, 0x53c1e087, 0x00 | BRF_OPT },           // 22
	{ "rt2_b-3a-.ic9",		0x00117, 0x8ad303aa, 0x00 | BRF_OPT },           // 23
};

STD_ROM_PICK(rtype2)
STD_ROM_FN(rtype2)

static INT32 rtype2Init()
{
	return DrvInit(rtype2_main_cpu_map, sound_rom_map, NULL, Z80_REAL_NMI, 1, 0);
}

struct BurnDriver BurnDrvRtype2 = {
	"rtype2", NULL, NULL, NULL, "1989",
	"R-Type II (World)\0", NULL, "Irem", "Irem M82",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, rtype2RomInfo, rtype2RomName, NULL, NULL, NULL, NULL, CommonInputInfo, Rtype2DIPInfo,
	rtype2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// R-Type II (Japan)

static struct BurnRomInfo rtype2jRomDesc[] = {
	{ "rt2_a-h0-.ic54",		0x20000, 0x7857ccf6, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rt2_a-l0-.ic60",		0x20000, 0xcb22cd6e, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "rt2_a-h1-.ic53",		0x20000, 0x49e75d28, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "rt2_a-l1-.ic59",		0x20000, 0x12ec1676, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "rt2_a-sp-.ic17",		0x10000, 0x73ffecb4, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rt2_b-n0.ic31",		0x20000, 0x2cd8f913, 0x02 | BRF_GRA },           //  5 Sprites
	{ "rt2_b-n1.ic21",		0x20000, 0x5033066d, 0x02 | BRF_GRA },           //  6
	{ "rt2_b-n2.ic32",		0x20000, 0xec3a0450, 0x02 | BRF_GRA },           //  7
	{ "rt2_b-n3.ic22",		0x20000, 0xdb6176fc, 0x02 | BRF_GRA },           //  8

	{ "rt2_a-g00.ic50",		0x20000, 0xf3f8736e, 0x03 | BRF_GRA },           //  9 Foreground & Background Tiles
	{ "rt2_a-g01.ic51",		0x20000, 0xb4c543af, 0x03 | BRF_GRA },           // 10
	{ "rt2_a-g10.ic56",		0x20000, 0x4cb80d66, 0x03 | BRF_GRA },           // 11
	{ "rt2_a-g11.ic57",		0x20000, 0xbee128e0, 0x03 | BRF_GRA },           // 12
	{ "rt2_a-g20.ic65",		0x20000, 0x2dc9c71a, 0x03 | BRF_GRA },           // 13
	{ "rt2_a-g21.ic66",		0x20000, 0x7533c428, 0x03 | BRF_GRA },           // 14
	{ "rt2_a-g30.ic63",		0x20000, 0xa6ad67f2, 0x03 | BRF_GRA },           // 15
	{ "rt2_a-g31.ic64",		0x20000, 0x3686d555, 0x03 | BRF_GRA },           // 16

	{ "rt2_a-vo-.ic14",		0x20000, 0x637172d5, 0x05 | BRF_SND },           // 17 DAC Samples

	{ "rt2_b-4n-.ic23",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 18 Proms
	{ "rt2_b-4p-.ic24",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 19

	{ "rt2_a-2h-.ic5",		0x00117, 0x21ede612, 0x00 | BRF_OPT },           // 20 PLDs
	{ "rt2_a-5l-.ic33",		0x00117, 0x86e87e50, 0x00 | BRF_OPT },           // 21
	{ "rt2_a-7d-.ic45",		0x00104, 0x53c1e087, 0x00 | BRF_OPT },           // 22
	{ "rt2_b-3a-.ic9",		0x00117, 0x8ad303aa, 0x00 | BRF_OPT },           // 23
};

STD_ROM_PICK(rtype2j)
STD_ROM_FN(rtype2j)

struct BurnDriver BurnDrvRtype2j = {
	"rtype2j", "rtype2", NULL, NULL, "1989",
	"R-Type II (Japan)\0", NULL, "Irem", "Irem M82",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, rtype2jRomInfo, rtype2jRomName, NULL, NULL, NULL, NULL, CommonInputInfo, Rtype2DIPInfo,
	rtype2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// R-Type II (Japan, revision C)

static struct BurnRomInfo rtype2jcRomDesc[] = {
	{ "rt2_a-h0-c.ic54",	0x20000, 0xef9a9990, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rt2_a-l0-c.ic60",	0x20000, 0xd8b9da64, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "rt2_a-h1-c.ic53",	0x20000, 0x1b1870f4, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "rt2_a-l1-c.ic59",	0x20000, 0x60fdff35, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "rt2_a-sp-.ic17",		0x10000, 0x73ffecb4, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rt2_b-n0.ic31",		0x20000, 0x2cd8f913, 0x02 | BRF_GRA },           //  5 Sprites
	{ "rt2_b-n1.ic21",		0x20000, 0x5033066d, 0x02 | BRF_GRA },           //  6
	{ "rt2_b-n2.ic32",		0x20000, 0xec3a0450, 0x02 | BRF_GRA },           //  7
	{ "rt2_b-n3.ic22",		0x20000, 0xdb6176fc, 0x02 | BRF_GRA },           //  8

	{ "rt2_a-g00.ic50",		0x20000, 0xf3f8736e, 0x03 | BRF_GRA },           //  9 Foreground & Background Tiles
	{ "rt2_a-g01.ic51",		0x20000, 0xb4c543af, 0x03 | BRF_GRA },           // 10
	{ "rt2_a-g10.ic56",		0x20000, 0x4cb80d66, 0x03 | BRF_GRA },           // 11
	{ "rt2_a-g11.ic57",		0x20000, 0xbee128e0, 0x03 | BRF_GRA },           // 12
	{ "rt2_a-g20.ic65",		0x20000, 0x2dc9c71a, 0x03 | BRF_GRA },           // 13
	{ "rt2_a-g21.ic66",		0x20000, 0x7533c428, 0x03 | BRF_GRA },           // 14
	{ "rt2_a-g30.ic63",		0x20000, 0xa6ad67f2, 0x03 | BRF_GRA },           // 15
	{ "rt2_a-g31.ic64",		0x20000, 0x3686d555, 0x03 | BRF_GRA },           // 16

	{ "rt2_a-vo-.ic14",		0x20000, 0x637172d5, 0x05 | BRF_SND },           // 17 DAC Samples

	{ "rt2_b-4n-.ic23",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 18 Proms
	{ "rt2_b-4p-.ic24",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 19

	{ "rt2_a-2h-.ic5",		0x00117, 0x21ede612, 0x00 | BRF_OPT },           // 20 PLDs
	{ "rt2_a-5l-.ic33",		0x00117, 0x86e87e50, 0x00 | BRF_OPT },           // 21
	{ "rt2_a-7d-.ic45",		0x00104, 0x53c1e087, 0x00 | BRF_OPT },           // 22
	{ "rt2_b-3a-.ic9",		0x00117, 0x8ad303aa, 0x00 | BRF_OPT },           // 23
};

STD_ROM_PICK(rtype2jc)
STD_ROM_FN(rtype2jc)

struct BurnDriver BurnDrvRtype2jc = {
	"rtype2jc", "rtype2", NULL, NULL, "1989",
	"R-Type II (Japan, revision C)\0", NULL, "Irem", "Irem M82",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, rtype2jcRomInfo, rtype2jcRomName, NULL, NULL, NULL, NULL, CommonInputInfo, Rtype2DIPInfo,
	rtype2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// R-Type II (Japan, bootleg M82 conversion)

static struct BurnRomInfo rtype2m82bRomDesc[] = {
	{ "rt2_h0.ic52",	0x20000, 0x47639a78, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rt2_l0.ic60",	0x20000, 0xa1661cdf, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "rt2_h1.ic51",	0x20000, 0x4b79840c, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "rt2_l1.ic59",	0x20000, 0x6ab3ae42, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "rt2_sp.ic15",	0x10000, 0x73ffecb4, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rt2_n0.ic44",	0x20000, 0x2cd8f913, 0x02 | BRF_GRA },           //  5 Sprites
	{ "rt2_n1.ic45",	0x20000, 0x5033066d, 0x02 | BRF_GRA },           //  6
	{ "rt2_n2.ic46",	0x20000, 0xec3a0450, 0x02 | BRF_GRA },           //  7
	{ "rt2_n3.ic36",	0x20000, 0xdb6176fc, 0x02 | BRF_GRA },           //  8

	{ "rt2_c0.ic38",	0x40000, 0xf5bad5f2, 0x03 | BRF_GRA },           //  9 Foreground & Background Tiles
	{ "rt2_c1.ic39",	0x40000, 0x71451778, 0x03 | BRF_GRA },           // 10
	{ "rt2_c2.ic40",	0x40000, 0xc6b0c352, 0x03 | BRF_GRA },           // 11
	{ "rt2_c3.ic41",	0x40000, 0x6d530a32, 0x03 | BRF_GRA },           // 12

	{ "mt_f0.ic38",		0x20000, 0x2d5e05d5, 0x0e | BRF_GRA },           // 13 Sprites 2
	{ "mt_f1.ic39",		0x20000, 0xc68cd65f, 0x0e | BRF_GRA },           // 14
	{ "mt_f2.ic40",		0x20000, 0xa71feb2d, 0x0e | BRF_GRA },           // 15
	{ "mt_f3.ic41",		0x20000, 0x179f7562, 0x0e | BRF_GRA },           // 16

	{ "rt2_vo.ic12",	0x20000, 0x637172d5, 0x05 | BRF_SND },           // 17 DAC Samples
};

STD_ROM_PICK(rtype2m82b)
STD_ROM_FN(rtype2m82b)

struct BurnDriver BurnDrvRtype2m82b = {
	"rtype2m82b", "rtype2", NULL, NULL, "1997",
	"R-Type II (Japan, bootleg M82 conversion)\0", NULL, "bootleg", "Irem M82",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, rtype2m82bRomInfo, rtype2m82bRomName, NULL, NULL, NULL, NULL, CommonInputInfo, Rtype2DIPInfo,
	rtypem82bInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Hammerin' Harry (World, M81 hardware)

static struct BurnRomInfo hharryRomDesc[] = {
	{ "gen=m81=_a-h0-v.ic59",	0x20000, 0xc52802a5, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "gen=m81=_a-l0-v.ic68",	0x20000, 0xf463074c, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "gen=m81=_a-h1-0.ic58",	0x10000, 0x3ae21335, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "gen=m81=_a-l1-0.ic67",	0x10000, 0xbc6ac5f9, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "gen=m81=_a-sp-0.ic14",	0x10000, 0x80e210e7, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "gen=m81=_00.ic3",		0x20000, 0xec5127ef, 0x02 | BRF_GRA },           //  5 Sprites
	{ "gen=m81=_10.ic5",		0x20000, 0xdef65294, 0x02 | BRF_GRA },           //  6
	{ "gen=m81=_20.ic12",		0x20000, 0xbb0d6ad4, 0x02 | BRF_GRA },           //  7
	{ "gen=m81=_30.ic14",		0x20000, 0x4351044e, 0x02 | BRF_GRA },           //  8

	{ "gen=m81=_a0.ic50",		0x20000, 0xc577ba5f, 0x03 | BRF_GRA },           //  9 Foreground & Background Tiles
	{ "gen=m81=_a1.ic49",		0x20000, 0x429d12ab, 0x03 | BRF_GRA },           // 10
	{ "gen=m81=_a2.ic51",		0x20000, 0xb5b163b0, 0x03 | BRF_GRA },           // 11
	{ "gen=m81=_a3.ic52",		0x20000, 0x8ef566a1, 0x03 | BRF_GRA },           // 12

	{ "gen=m81=_a-v0-0.ic11",	0x20000, 0xfaaacaff, 0x05 | BRF_SND },           // 13 DAC Samples

	{ "m81_a-9l-.ic72",			0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 14 Proms
	{ "m81_a-9p-.ic74",			0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 15

	{ "m81_b-9h-.ic94",			0x00117, 0x4bc1d393, 0x00 | BRF_OPT },           // 16 PLDs
	{ "m81_a-5l-.ic28",			0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 17
	{ "m81_a-2h-.ic2",			0x00117, 0x21ede612, 0x00 | BRF_OPT },           // 18
	{ "gen=81=_a-7d-a.ic48",	0x00117, 0x92fe9eff, 0x00 | BRF_OPT },           // 19
};

STD_ROM_PICK(hharry)
STD_ROM_FN(hharry)

static INT32 hharryInit()
{
	return DrvInit(common_080000_0a0000, sound_rom_map, dbreedm72RomLoadCallback, Z80_REAL_NMI, 2, 1);
}

struct BurnDriver BurnDrvHharry = {
	"hharry", NULL, NULL, NULL, "1990",
	"Hammerin' Harry (World, M81 hardware)\0", NULL, "Irem", "Irem M82",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, hharryRomInfo, hharryRomName, NULL, NULL, NULL, NULL, CommonInputInfo, HharryDIPInfo,
	hharryInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Hammerin' Harry (US, M84 hardware)

static struct BurnRomInfo hharryuRomDesc[] = {
	{ "gen_a-h0-u.ic55",		0x20000, 0xede7f755, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "gen_a-l0-u.ic61",		0x20000, 0xdf0726ae, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "gen_a-h1-f.ic54",		0x10000, 0x31b741c5, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "gen_a-l1-f.ic60",		0x10000, 0xb23e966c, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "gen=84=_a-sp-0-f.ic14",	0x10000, 0x80e210e7, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "hh_n0.ic33",				0x20000, 0xec5127ef, 0x02 | BRF_GRA },           //  5 Sprites
	{ "hh_n1.ic34",				0x20000, 0xdef65294, 0x02 | BRF_GRA },           //  6
	{ "hh_n2.ic35",				0x20000, 0xbb0d6ad4, 0x02 | BRF_GRA },           //  7
	{ "hh_n3.ic36",				0x20000, 0x4351044e, 0x02 | BRF_GRA },           //  8

	{ "hh_a0.ic51",				0x20000, 0xc577ba5f, 0x03 | BRF_GRA },           //  9 Foreground & Background Tiles
	{ "hh_a1.ic57",				0x20000, 0x429d12ab, 0x03 | BRF_GRA },           // 10
	{ "hh_a2.ic66",				0x20000, 0xb5b163b0, 0x03 | BRF_GRA },           // 11
	{ "hh_a3.ic64",				0x20000, 0x8ef566a1, 0x03 | BRF_GRA },           // 12

	{ "gen=84=_a-vo-f.ic17",	0x20000, 0xfaaacaff, 0x05 | BRF_SND },           // 13 DAC Samples

	{ "gen=84=_c-4n-.ic21",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 14 Proms
	{ "gen=84=_c-4p-.ic22",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 15

	{ "gen=84=_a-2h.ic5",		0x00117, 0x21ede612, 0x00 | BRF_OPT },           // 16 PLDs
	{ "gen=84=_a-5l.ic33",		0x00117, 0x579e257d, 0x00 | BRF_OPT },           // 17
	{ "gen=84=_a-7d.ic45",		0x00117, 0x79ef86f2, 0x00 | BRF_OPT },           // 18
	{ "gen=84=_c-3a.ic8",		0x00117, 0xc1e19913, 0x00 | BRF_OPT },           // 19
};

STD_ROM_PICK(hharryu)
STD_ROM_FN(hharryu)

static INT32 hharryuInit()
{
	return DrvInit(hharryu_main_cpu_map, sound_rom_map, dbreedm72RomLoadCallback, Z80_REAL_NMI, 7, 1);
}

struct BurnDriver BurnDrvHharryu = {
	"hharryu", "hharry", NULL, NULL, "1990",
	"Hammerin' Harry (US, M84 hardware)\0", NULL, "Irem America", "Irem M82",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, hharryuRomInfo, hharryuRomName, NULL, NULL, NULL, NULL, CommonInputInfo, HharryDIPInfo,
	hharryuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Hammerin' Harry (World, M84 hardware bootleg)

static struct BurnRomInfo hharrybRomDesc[] = {
	{ "4-a-27c010a.bin",	0x20000, 0x755c0874, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "6-a-27c010a.bin",	0x20000, 0xf10fb55c, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "3-a-27c512.bin",		0x10000, 0x31b741c5, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "5-a-27c512.bin",		0x10000, 0xb23e966c, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "2-a-27c512.bin",		0x10000, 0x80e210e7, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "17-c-27c010a.bin",	0x20000, 0xec5127ef, 0x02 | BRF_GRA },           //  5 Sprites
	{ "16-c-27c010a.bin",	0x20000, 0xdef65294, 0x02 | BRF_GRA },           //  6
	{ "14-c-27c010a.bin",	0x20000, 0xbb0d6ad4, 0x02 | BRF_GRA },           //  7
	{ "15-c-27c010a.bin",	0x20000, 0x4351044e, 0x02 | BRF_GRA },           //  8

	{ "13-b-27c010a.bin",	0x20000, 0xc577ba5f, 0x03 | BRF_GRA },           //  9 Foreground & Background Tiles
	{ "11-b-27c010a.bin",	0x20000, 0x429d12ab, 0x03 | BRF_GRA },           // 10
	{ "9-b-27c010a.bin",	0x20000, 0xb5b163b0, 0x03 | BRF_GRA },           // 11
	{ "7-b-27c010a.bin",	0x20000, 0x8ef566a1, 0x03 | BRF_GRA },           // 12

	{ "1-a-27c010a.bin",	0x20000, 0xfaaacaff, 0x05 | BRF_SND },           // 13 DAC Samples
	
	{ "19-c-82s129.bin",	0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 14 Proms
	{ "18-c-82s129.bin",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 15

	{ "a-pal16l8.bin",		0x00104, 0x1358c513, 0x00 | BRF_OPT },           // 16 PLDs
};

STD_ROM_PICK(hharryb)
STD_ROM_FN(hharryb)

struct BurnDriver BurnDrvHharryb = {
	"hharryb", "hharry", NULL, NULL, "1990",
	"Hammerin' Harry (World, M84 hardware bootleg)\0", NULL, "bootleg", "Irem M82",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, hharrybRomInfo, hharrybRomName, NULL, NULL, NULL, NULL, CommonInputInfo, HharryDIPInfo,
	hharryuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Daiku no Gensan (Japan, M84 hardware)

static struct BurnRomInfo dkgensanRomDesc[] = {
	{ "gen_a-h0-.ic55",		0x20000, 0x07a45f6d, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "gen_a-l0-.ic61",		0x20000, 0x46478fea, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "gen_a-h1-.ic54",		0x10000, 0x54e5b73c, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "gen_a-l1-.ic60",		0x10000, 0x894f8a9f, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "gen_a-sp-.ic17",		0x10000, 0xe83cfc2c, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "hh_n0.ic33",			0x20000, 0xec5127ef, 0x02 | BRF_GRA },           //  5 Sprites
	{ "hh_n1.ic34",			0x20000, 0xdef65294, 0x02 | BRF_GRA },           //  6
	{ "hh_n2.ic35",			0x20000, 0xbb0d6ad4, 0x02 | BRF_GRA },           //  7
	{ "hh_n3.ic36",			0x20000, 0x4351044e, 0x02 | BRF_GRA },           //  8

	{ "hh_a0.ic51",			0x20000, 0xc577ba5f, 0x03 | BRF_GRA },           //  9 Foreground & Background Tiles
	{ "hh_a1.ic57",			0x20000, 0x429d12ab, 0x03 | BRF_GRA },           // 10
	{ "hh_a2.ic66",			0x20000, 0xb5b163b0, 0x03 | BRF_GRA },           // 11
	{ "hh_a3.ic64",			0x20000, 0x8ef566a1, 0x03 | BRF_GRA },           // 12

	{ "gen_a-vo-.ic14",		0x20000, 0xd8595c66, 0x05 | BRF_SND },           // 13 DAC Samples

	{ "gen=84=_c-4n-.ic21",	0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 14 Proms
	{ "gen=84=_c-4p-.ic22",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 15

	{ "gen=84=_a-2h.ic5",	0x00117, 0x21ede612, 0x00 | BRF_OPT },           // 16 PLDs
	{ "gen=84=_a-5l.ic33",	0x00117, 0x579e257d, 0x00 | BRF_OPT },           // 17
	{ "gen=84=_a-7d.ic45",	0x00117, 0x79ef86f2, 0x00 | BRF_OPT },           // 18
	{ "gen=84=_c-3a.ic8",	0x00117, 0xc1e19913, 0x00 | BRF_OPT },           // 19
};

STD_ROM_PICK(dkgensan)
STD_ROM_FN(dkgensan)

struct BurnDriver BurnDrvDkgensan = {
	"dkgensan", "hharry", NULL, NULL, "1990",
	"Daiku no Gensan (Japan, M84 hardware)\0", NULL, "Irem", "Irem M82",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, dkgensanRomInfo, dkgensanRomName, NULL, NULL, NULL, NULL, CommonInputInfo, HharryDIPInfo,
	hharryuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Daiku no Gensan (Japan, M72 hardware)

static struct BurnRomInfo dkgensanm72RomDesc[] = {
	{ "ge72_h0-.ic40",			0x20000, 0xa0ad992c, 0x01 | BRF_PRG | BRF_ESS },    //  0 V30 Code
	{ "ge72_l0-.ic37",			0x20000, 0x996396f0, 0x01 | BRF_PRG | BRF_ESS },    //  1
	{ "ge72_h3-.ic43",			0x10000, 0xd8b86005, 0x01 | BRF_PRG | BRF_ESS },    //  2
	{ "ge72_l3-.ic34",			0x10000, 0x23d303a5, 0x01 | BRF_PRG | BRF_ESS },    //  3

	{ "hh_00.ic53",				0x20000, 0xec5127ef, 0x02 | BRF_GRA },              //  4 Sprites
	{ "hh_10.ic51",				0x20000, 0xdef65294, 0x02 | BRF_GRA },              //  5
	{ "hh_20.ic49",				0x20000, 0xbb0d6ad4, 0x02 | BRF_GRA },              //  6
	{ "hh_30.ic47",				0x20000, 0x4351044e, 0x02 | BRF_GRA },              //  7

	{ "ge72b-a0.ic21",			0x10000, 0xf5f56b2a, 0x03 | BRF_GRA },              //  8 Foreground Tiles
	{ "ge72-a1.ic22",			0x10000, 0xd194ea08, 0x03 | BRF_GRA },              //  9
	{ "ge72-a2.ic20",			0x10000, 0x2b06bcc3, 0x03 | BRF_GRA },              // 10
	{ "ge72-a3.ic23",			0x10000, 0x94b96bfa, 0x03 | BRF_GRA },              // 11

	{ "ge72-b0.ic26",			0x10000, 0x208796b3, 0x04 | BRF_GRA },              // 12 Background Tiles
	{ "ge72-b1.ic27",			0x10000, 0xb4a7f490, 0x04 | BRF_GRA },              // 13
	{ "ge72b-b2.ic25",			0x10000, 0x34fe8f7f, 0x04 | BRF_GRA },              // 14
	{ "ge72b-b3.ic24",			0x10000, 0x4b0e92f4, 0x04 | BRF_GRA },              // 15

	{ "gen-v0.ic44",			0x20000, 0xd8595c66, 0x05 | BRF_SND },              // 16 DAC Samples

	{ "dkgenm72_i8751.mcu",		0x10000, 0x00000000, 0x00 | BRF_PRG | BRF_NODUMP }, // 17 i8751 microcontroller

	{ "m72_a-8l-.ic66",			0x00100, 0xb460c438, 0x00 | BRF_OPT },              // 18 Proms
	{ "m72_a-9l-.ic75",			0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },              // 19

	{ "m72_a-3d-.ic11",			0x00117, 0x8a3732ff, 0x00 | BRF_OPT },              // 20 PLDs
	{ "m72_a-4d-.ic19",			0x00117, 0x56c29834, 0x00 | BRF_OPT },              // 21
	{ "gen=m72=c-3f-b.ic13",	0x00117, 0x028932a4, 0x00 | BRF_OPT },              // 22
};

STD_ROM_PICK(dkgensanm72)
STD_ROM_FN(dkgensanm72)

static INT32 dkgensanm72Init()
{
	install_protection(dkgenm72);

	return DrvInit(common_080000_0a0000, sound_ram_map, dbreedm72RomLoadCallback, Z80_FAKE_NMI, 0, 0);
}

struct BurnDriver BurnDrvDkgensanm72 = {
	"dkgensanm72", "hharry", NULL, NULL, "1990",
	"Daiku no Gensan (Japan, M72 hardware)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, dkgensanm72RomInfo, dkgensanm72RomName, NULL, NULL, NULL, NULL, CommonInputInfo, HharryDIPInfo,
	dkgensanm72Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Daiku no Gensan (Japan, M82 hardware)

static struct BurnRomInfo dkgensanm82RomDesc[] = {
	{ "gen_=m84=_a-h0-d.ic52",	0x20000, 0xa1ca8855, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "gen_=m84=_a-l0-d.ic60",	0x20000, 0x247117b0, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "gen_=m84=_a-h1-d.ic51",	0x10000, 0x54e5b73c, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "gen_=m84=_a-l1-d.ic59",	0x10000, 0x894f8a9f, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "gen_=m84=_a-sp-d.ic15",	0x10000, 0xe83cfc2c, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "gen_=m72=_c-l0-b.ic44",	0x20000, 0xec5127ef, 0x02 | BRF_GRA },           //  5 Sprites
	{ "gen_=m72=_c-l3-b.ic45",	0x20000, 0xdef65294, 0x02 | BRF_GRA },           //  6
	{ "gen_=m72=_c-h0-b.ic46",	0x20000, 0xbb0d6ad4, 0x02 | BRF_GRA },           //  7
	{ "gen_=m72=_c-h3-b.ic36",	0x20000, 0x4351044e, 0x02 | BRF_GRA },           //  8

	{ "gen_=m81=_a-l0-a.ic49",	0x20000, 0xc577ba5f, 0x03 | BRF_GRA },           //  9 Foreground & Background Tiles
	{ "gen_=m81=_a-l1-a.ic48",	0x20000, 0x429d12ab, 0x03 | BRF_GRA },           // 10
	{ "gen_=m81=_a-h0-a.ic57",	0x20000, 0xb5b163b0, 0x03 | BRF_GRA },           // 11
	{ "gen_=m81=_a-h1-a.ic56",	0x20000, 0x8ef566a1, 0x03 | BRF_GRA },           // 12

	{ "mt_f0.ic38",				0x20000, 0x2d5e05d5, 0x0e | BRF_GRA },           // 13 Sprites 2
	{ "mt_f1.ic39",				0x20000, 0xc68cd65f, 0x0e | BRF_GRA },           // 14
	{ "mt_f2.ic40",				0x20000, 0xa71feb2d, 0x0e | BRF_GRA },           // 15
	{ "mt_f3.ic41",				0x20000, 0x179f7562, 0x0e | BRF_GRA },           // 16

	{ "gen_=m84=_a-vo-d.ic12",	0x20000, 0xd8595c66, 0x05 | BRF_SND },           // 17 DAC Samples

	{ "mt_a-2h-.ic5",			0x00117, 0x21ede612, 0x00 | BRF_OPT },           // 18 PLDs
	{ "mt_a-5l-.ic31",			0x00117, 0xe3064bfe, 0x00 | BRF_OPT },           // 19
	{ "gen=m84=a-7d-d.ic43",	0x00117, 0xa3ee35fe, 0x00 | BRF_OPT },           // 20
	{ "gen=m72=c-3f-b.ic23",	0x00117, 0x028932a4, 0x00 | BRF_OPT },           // 21
};

STD_ROM_PICK(dkgensanm82)
STD_ROM_FN(dkgensanm82)

struct BurnDriver BurnDrvDkgensanm82 = {
	"dkgensanm82", "hharry", NULL, NULL, "1990",
	"Daiku no Gensan (Japan, M82 hardware)\0", NULL, "Irem", "Irem M82",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_SCRFIGHT | GBF_PLATFORM, 0,
	NULL, dkgensanm82RomInfo, dkgensanm82RomName, NULL, NULL, NULL, NULL, CommonInputInfo, HharryDIPInfo,
	hharryuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Lightning Swords (World)

static struct BurnRomInfo ltswordsRomDesc[] = {
	{ "h0.ic55",			0x20000, 0x22f342b2, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "l0.ic61",			0x20000, 0x0210d592, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "ken_d-sp-.ic17",		0x10000, 0x233ca1cf, 0x06 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ken_m31.ic31",		0x20000, 0xe00b95a6, 0x02 | BRF_GRA },           //  3 Sprites
	{ "ken_m21.ic21",		0x20000, 0xd7722f87, 0x02 | BRF_GRA },           //  4
	{ "ken_m32.ic32",		0x20000, 0x30a844c4, 0x02 | BRF_GRA },           //  5
	{ "ken_m22.ic22",		0x20000, 0xa00dac85, 0x02 | BRF_GRA },           //  6

	{ "ken_m51.ic51",		0x20000, 0x1646cf4f, 0x03 | BRF_GRA },           //  7 Foreground & Background Tiles
	{ "ken_m57.ic57",		0x20000, 0xa9f88d90, 0x03 | BRF_GRA },           //  8
	{ "ken_m66.ic66",		0x20000, 0xe9d17645, 0x03 | BRF_GRA },           //  9
	{ "ken_m64.ic64",		0x20000, 0xdf46709b, 0x03 | BRF_GRA },           // 10

	{ "ken_m14.ic14",		0x20000, 0x6651e9b7, 0x05 | BRF_SND },           // 11 DAC Samples

	{ "ken_b-4n-.ic23",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 12 Proms
	{ "ken_b-4p-.ic24",		0x00100, 0x526f10ca, 0x00 | BRF_OPT },           // 13

	{ "ken_d-2h-.ic5",		0x00117, 0xa83807e9, 0x00 | BRF_OPT },           // 14 PLDs
	{ "ken_d-5l-.ic33",		0x00117, 0xc719b8a3, 0x00 | BRF_OPT },           // 15
	{ "ken_d-7b-.ic45",		0x00117, 0xb0558dc5, 0x00 | BRF_OPT },           // 16
	{ "ken_b-3a-.ic9",		0x00117, 0xad1a7942, 0x00 | BRF_OPT },           // 17
};

STD_ROM_PICK(ltswords)
STD_ROM_FN(ltswords)

static INT32 kengoInit()
{
	Kengo = 1;
	INT32 nRet = DrvInit(hharryu_main_cpu_map, sound_rom_map, NULL, Z80_REAL_NMI, 5, 0);

	if (nRet == 0) {
		Clock_16mhz = 1;
		VezOpen(0);
		VezSetDecode((UINT8 *)&gunforce_decryption_table);
		VezClose();
	}

	return nRet;
}

struct BurnDriver BurnDrvLtswords = {
	"ltswords", NULL, NULL, NULL, "1991",
	"Lightning Swords (World)\0", NULL, "Irem", "Irem M84?",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_SCRFIGHT, 0,
	NULL, ltswordsRomInfo, ltswordsRomName, NULL, NULL, NULL, NULL, CommonInputInfo, KengoDIPInfo,
	kengoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Ken-Go (World)

static struct BurnRomInfo kengoRomDesc[] = {
	{ "ken_d-h0-c.ic55",	0x20000, 0xf4ddeea5, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "ken_d-l0-c.ic61",	0x20000, 0x04dc0f81, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "ken_d-sp-.ic17",		0x10000, 0x233ca1cf, 0x06 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ken_m31.ic31",		0x20000, 0xe00b95a6, 0x02 | BRF_GRA },           //  3 Sprites
	{ "ken_m21.ic21",		0x20000, 0xd7722f87, 0x02 | BRF_GRA },           //  4
	{ "ken_m32.ic32",		0x20000, 0x30a844c4, 0x02 | BRF_GRA },           //  5
	{ "ken_m22.ic22",		0x20000, 0xa00dac85, 0x02 | BRF_GRA },           //  6

	{ "ken_m51.ic51",		0x20000, 0x1646cf4f, 0x03 | BRF_GRA },           //  7 Foreground & Background Tiles
	{ "ken_m57.ic57",		0x20000, 0xa9f88d90, 0x03 | BRF_GRA },           //  8
	{ "ken_m66.ic66",		0x20000, 0xe9d17645, 0x03 | BRF_GRA },           //  9
	{ "ken_m64.ic64",		0x20000, 0xdf46709b, 0x03 | BRF_GRA },           // 10

	{ "ken_m14.ic14",		0x20000, 0x6651e9b7, 0x05 | BRF_SND },           // 11 DAC Samples

	{ "ken_b-4n-.ic23",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 12 Proms
	{ "ken_b-4p-.ic24",		0x00100, 0x526f10ca, 0x00 | BRF_OPT },           // 13

	{ "ken_d-2h-.ic5",		0x00117, 0xa83807e9, 0x00 | BRF_OPT },           // 14 PLDs
	{ "ken_d-5l-.ic33",		0x00117, 0xc719b8a3, 0x00 | BRF_OPT },           // 15
	{ "ken_d-7b-.ic45",		0x00117, 0xb0558dc5, 0x00 | BRF_OPT },           // 16
	{ "ken_b-3a-.ic9",		0x00117, 0xad1a7942, 0x00 | BRF_OPT },           // 17
};

STD_ROM_PICK(kengo)
STD_ROM_FN(kengo)

struct BurnDriver BurnDrvKengo = {
	"kengo", "ltswords", NULL, NULL, "1991",
	"Ken-Go (World)\0", NULL, "Irem", "Irem M84?",
	L"Ken-Go (World)\0\u5263\u8c6a\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_SCRFIGHT, 0,
	NULL, kengoRomInfo, kengoRomName, NULL, NULL, NULL, NULL, CommonInputInfo, KengoDIPInfo,
	kengoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Ken-Go (Japan)

static struct BurnRomInfo kengojRomDesc[] = {
	{ "ken_d-h0-.ic55",		0x20000, 0xed3da88c, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "ken_d-l0-.ic61",		0x20000, 0x92c57d8e, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "ken_d-sp.ic17",		0x10000, 0x233ca1cf, 0x06 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ken_m31.ic31",		0x20000, 0xe00b95a6, 0x02 | BRF_GRA },           //  3 Sprites
	{ "ken_m21.ic21",		0x20000, 0xd7722f87, 0x02 | BRF_GRA },           //  4
	{ "ken_m32.ic32",		0x20000, 0x30a844c4, 0x02 | BRF_GRA },           //  5
	{ "ken_m22.ic22",		0x20000, 0xa00dac85, 0x02 | BRF_GRA },           //  6

	{ "ken_m51.ic51",		0x20000, 0x1646cf4f, 0x03 | BRF_GRA },           //  7 Foreground & Background Tiles
	{ "ken_m57.ic57",		0x20000, 0xa9f88d90, 0x03 | BRF_GRA },           //  8
	{ "ken_m66.ic66",		0x20000, 0xe9d17645, 0x03 | BRF_GRA },           //  9
	{ "ken_m64.ic64",		0x20000, 0xdf46709b, 0x03 | BRF_GRA },           // 10

	{ "ken_m14.ic14",		0x20000, 0x6651e9b7, 0x05 | BRF_SND },           // 11 DAC Samples
	
	{ "ken_b-4n-.ic23",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 12 Proms
	{ "ken_b-4p-.ic24",		0x00100, 0x526f10ca, 0x00 | BRF_OPT },           // 13

	{ "ken_d-2h-.ic5",		0x00117, 0xa83807e9, 0x00 | BRF_OPT },           // 14 PLDs
	{ "ken_d-5l-.ic33",		0x00117, 0xc719b8a3, 0x00 | BRF_OPT },           // 15
	{ "ken_d-7b-.ic45",		0x00117, 0xb0558dc5, 0x00 | BRF_OPT },           // 16
	{ "ken_b-3a-.ic9",		0x00117, 0xad1a7942, 0x00 | BRF_OPT },           // 17
};

STD_ROM_PICK(kengoj)
STD_ROM_FN(kengoj)

struct BurnDriver BurnDrvKengoj = {
	"kengoj", "ltswords", NULL, NULL, "1991",
	"Ken-Go (Japan)\0", NULL, "Irem", "Irem M84?",
	L"Ken-Go (Japan)\0\u5263\u8c6a\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_SCRFIGHT, 0,
	NULL, kengojRomInfo, kengojRomName, NULL, NULL, NULL, NULL, CommonInputInfo, KengoDIPInfo,
	kengoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Cosmic Cop (World)

static struct BurnRomInfo cosmccopRomDesc[] = {
	{ "cc_d-h0-b.ic55",		0x40000, 0x38958b01, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "cc_d-l0-b.ic61",		0x40000, 0xeff87f70, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "cc_d-sp-.ic17",		0x10000, 0x3e3ace60, 0x06 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "cc_b-n0-.ic31",		0x20000, 0x9d99deaa, 0x02 | BRF_GRA },           //  3 Sprites
	{ "cc_b-n1-.ic21",		0x20000, 0x7eb083ed, 0x02 | BRF_GRA },           //  4
	{ "cc_b-n2-.ic32",		0x20000, 0x9421489e, 0x02 | BRF_GRA },           //  5
	{ "cc_b-n3-.ic22",		0x20000, 0x920ec735, 0x02 | BRF_GRA },           //  6

	{ "cc_d-g00-.ic51",		0x20000, 0xe7f3d772, 0x03 | BRF_GRA },           //  7 Foreground & Background Tiles
	{ "cc_d-g10-.ic57",		0x20000, 0x418b4e4c, 0x03 | BRF_GRA },           //  8
	{ "cc_d-g20-.ic66",		0x20000, 0xa4b558eb, 0x03 | BRF_GRA },           //  9
	{ "cc_d-g30-.ic64",		0x20000, 0xf64a3166, 0x03 | BRF_GRA },           // 10

	{ "cc_d-v0-.ic14",		0x20000, 0x6247bade, 0x05 | BRF_SND },           // 11 DAC Samples

	{ "ken_b-4n-.ic23",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 12 Proms
	{ "ken_b-4p-.ic24",		0x00100, 0x526f10ca, 0x00 | BRF_OPT },           // 13

	{ "ken_d-2h-.ic5",		0x00117, 0xa83807e9, 0x00 | BRF_OPT },           // 14 PLDs
	{ "ken_d-5l-.ic33",		0x00117, 0xc719b8a3, 0x00 | BRF_OPT },           // 15
	{ "cc_d-7b-.ic45",		0x00117, 0x75ff4517, 0x00 | BRF_OPT },           // 16
	{ "ken_b-3a-.ic9",		0x00117, 0xad1a7942, 0x00 | BRF_OPT },           // 17
};

STD_ROM_PICK(cosmccop)
STD_ROM_FN(cosmccop)

static INT32 cosmccopInit()
{
	Clock_16mhz = 1;

	INT32 rc = DrvInit(hharryu_main_cpu_map, sound_rom_map, NULL, Z80_REAL_NMI, 7, 0);

	m72_irq_base = 0x60; // Cosmic Cop doesn't write to port 0x42 (irq config), set it manually. (after DrvInit()!)
	CosmicCop = 1;

	return rc;
}

struct BurnDriver BurnDrvCosmccop = {
	"cosmccop", NULL, NULL, NULL, "1991",
	"Cosmic Cop (World)\0", NULL, "Irem", "Irem M84",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, cosmccopRomInfo, cosmccopRomName, NULL, NULL, NULL, NULL, CommonInputInfo, GallopDIPInfo,
	cosmccopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Gallop - Armed Police Unit (Japan, M72 hardware)

static struct BurnRomInfo gallopm72RomDesc[] = {
	{ "cc_c-h0-.ic40",	0x20000, 0x2217dcd0, 0x01 | BRF_PRG | BRF_ESS },    //  0 V30 Code
	{ "cc_c-l0-.ic37",	0x20000, 0xff39d7fb, 0x01 | BRF_PRG | BRF_ESS },    //  1
	{ "cc_c-h3-.ic43",	0x20000, 0x9b2bbab9, 0x01 | BRF_PRG | BRF_ESS },    //  2
	{ "cc_c-l3-.ic34",	0x20000, 0xacd3278e, 0x01 | BRF_PRG | BRF_ESS },    //  3

	{ "cc_c-00.ic53",	0x20000, 0x9d99deaa, 0x02 | BRF_GRA },              //  4 Sprites
	{ "cc_c-10.ic51",	0x20000, 0x7eb083ed, 0x02 | BRF_GRA },              //  5
	{ "cc_c-20.ic49",	0x20000, 0x9421489e, 0x02 | BRF_GRA },              //  6
	{ "cc_c-30.ic47",	0x20000, 0x920ec735, 0x02 | BRF_GRA },              //  7

	{ "cc_b-a0.ic21",	0x10000, 0xa33472bd, 0x03 | BRF_GRA },              //  8 Foreground Tiles
	{ "cc_b-a1.ic22",	0x10000, 0x118b1f2d, 0x03 | BRF_GRA },              //  9
	{ "cc_b-a2.ic20",	0x10000, 0x83cebf48, 0x03 | BRF_GRA },              // 10
	{ "cc_b-a3.ic23",	0x10000, 0x572903fc, 0x03 | BRF_GRA },              // 11

	{ "cc_b-b0.ic26",	0x10000, 0x0df5b439, 0x04 | BRF_GRA },              // 12 Background Tiles
	{ "cc_b-b1.ic27",	0x10000, 0x010b778f, 0x04 | BRF_GRA },              // 13
	{ "cc_b-b2.ic25",	0x10000, 0xbda9f6fb, 0x04 | BRF_GRA },              // 14
	{ "cc_b-b3.ic24",	0x10000, 0xd361ba3f, 0x04 | BRF_GRA },              // 15

	{ "cc_c-v0.ic44",	0x20000, 0x6247bade, 0x05 | BRF_SND },              // 16 DAC Samples
	
	{ "cc_c-pr-.ic1",	0x01000, 0xac4421b1, 0x07 | BRF_PRG },              // 17 i8751 microcontroller

	{ "m72_a-8l-.ic66",	0x00100, 0xb460c438, 0x00 | BRF_OPT },              // 18 Proms
	{ "m72_a-9l-.ic75",	0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },              // 19

	{ "m72_a-3d-.ic11",	0x00117, 0x8a3732ff, 0x00 | BRF_OPT },              // 20 PLDs
	{ "m72_a-4d-.ic19",	0x00117, 0x56c29834, 0x00 | BRF_OPT },              // 21
	{ "cc_c-3f-.ic13",	0x00117, 0x16ca7c50, 0x00 | BRF_OPT },              // 22
};

STD_ROM_PICK(gallopm72)
STD_ROM_FN(gallopm72)

static INT32 gallopm72Init()
{
	Clock_16mhz = 1;

	return DrvInit(common_080000_0a0000, sound_ram_map, NULL, Z80_FAKE_NMI, 0, 0);
}

struct BurnDriver BurnDrvGallopm72 = {
	"gallopm72", "cosmccop", NULL, NULL, "1991",
	"Gallop - Armed Police Unit (Japan, M72 hardware)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, gallopm72RomInfo, gallopm72RomName, NULL, NULL, NULL, NULL, CommonInputInfo, GallopDIPInfo,
	gallopm72Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Gallop - Armed Police Unit (Japan, M84 hardware)

static struct BurnRomInfo gallopRomDesc[] = {
	{ "cc_d-h0-.ic55",		0x40000, 0xdac9eec3, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "cc_d-l0-.ic61",		0x40000, 0x10e37ee1, 0x01 | BRF_PRG | BRF_ESS }, //  1

	{ "cc_d-sp-.ic17",		0x10000, 0x3e3ace60, 0x06 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "cc_b-n0-.ic31",		0x20000, 0x9d99deaa, 0x02 | BRF_GRA },           //  3 Sprites
	{ "cc_b-n1-.ic21",		0x20000, 0x7eb083ed, 0x02 | BRF_GRA },           //  4
	{ "cc_b-n2-.ic32",		0x20000, 0x9421489e, 0x02 | BRF_GRA },           //  5
	{ "cc_b-n3-.ic22",		0x20000, 0x920ec735, 0x02 | BRF_GRA },           //  6

	{ "cc_d-g00-.ic51",		0x20000, 0xe7f3d772, 0x03 | BRF_GRA },           //  7 Foreground & Background Tiles
	{ "cc_d-g10-.ic57",		0x20000, 0x418b4e4c, 0x03 | BRF_GRA },           //  8
	{ "cc_d-g20-.ic66",		0x20000, 0xa4b558eb, 0x03 | BRF_GRA },           //  9
	{ "cc_d-g30-.ic64",		0x20000, 0xf64a3166, 0x03 | BRF_GRA },           // 10

	{ "cc_d-v0-.ic14",		0x20000, 0x6247bade, 0x05 | BRF_SND },           // 11 DAC Samples

	{ "ken_b-4n-.ic23",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 12 Proms
	{ "ken_b-4p-.ic24",		0x00100, 0x526f10ca, 0x00 | BRF_OPT },           // 13

	{ "ken_d-2h-.ic5",		0x00117, 0xa83807e9, 0x00 | BRF_OPT },           // 14 PLDs
	{ "ken_d-5l-.ic33",		0x00117, 0xc719b8a3, 0x00 | BRF_OPT },           // 15
	{ "cc_d-7b-.ic45",		0x00117, 0x75ff4517, 0x00 | BRF_OPT },           // 16
	{ "ken_b-3a-.ic9",		0x00117, 0xad1a7942, 0x00 | BRF_OPT },           // 17
};

STD_ROM_PICK(gallop)
STD_ROM_FN(gallop)

struct BurnDriver BurnDrvGallop = {
	"gallop", "cosmccop", NULL, NULL, "1991",
	"Gallop - Armed Police Unit (Japan, M84 hardware)\0", NULL, "Irem", "Irem M84",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_HORSHOOT, 0,
	NULL, gallopRomInfo, gallopRomName, NULL, NULL, NULL, NULL, CommonInputInfo, GallopDIPInfo,
	cosmccopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};



// Legend of Hero Tonma (World)

static struct BurnRomInfo lohtRomDesc[] = {
	{ "tom_c-h0-b.ic40",	0x20000, 0xa63204b6, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "tom_c-l0-b.ic37",	0x20000, 0xe788002f, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "tom_c-h3-.ic43",		0x20000, 0x714778b5, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "tom_c-l3-.ic34",		0x20000, 0x2f049b03, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "tom_m53.ic53",		0x20000, 0x0b83265f, 0x02 | BRF_GRA },           //  4 Sprites
	{ "tom_m51.ic51",		0x20000, 0x8ec5f6f3, 0x02 | BRF_GRA },           //  5
	{ "tom_m49.ic49",		0x20000, 0xa41d3bfd, 0x02 | BRF_GRA },           //  6
	{ "tom_m47.ic47",		0x20000, 0x9d81a25b, 0x02 | BRF_GRA },           //  7

	{ "tom_m21.ic21",		0x10000, 0x3ca3e771, 0x03 | BRF_GRA },           //  8 Foreground Tiles
	{ "tom_m22.ic22",		0x10000, 0x7a05ee2f, 0x03 | BRF_GRA },           //  9
	{ "tom_m20.ic20",		0x10000, 0x79aa2335, 0x03 | BRF_GRA },           // 10
	{ "tom_m23.ic23",		0x10000, 0x789e8b24, 0x03 | BRF_GRA },           // 11

	{ "tom_m26.ic26",		0x10000, 0x44626bf6, 0x04 | BRF_GRA },           // 12 Background Tiles
	{ "tom_m27.ic27",		0x10000, 0x464952cf, 0x04 | BRF_GRA },           // 13
	{ "tom_m25.ic25",		0x10000, 0x3db9b2c7, 0x04 | BRF_GRA },           // 14
	{ "tom_m24.ic24",		0x10000, 0xf01fe899, 0x04 | BRF_GRA },           // 15

	{ "tom_m44.ic44",		0x10000, 0x3ed51d1f, 0x05 | BRF_SND },           // 16 DAC Samples

	{ "tom_c-pr-b.ic1",		0x01000, 0x9c9545f1, 0x07 | BRF_PRG },           // 17 i8751 microcontroller

	{ "m72_a-8l-.ic66",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 18 Proms
	{ "m72_a-9l-.ic75",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 19

	{ "m72_a-3d-.ic11",		0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 20 PLDs
	{ "m72_a-4d-.ic19",		0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 21
	{ "tom-c-3f.ic13",		0x00117, 0x2d774e1e, 0x00 | BRF_OPT },           // 22
};

STD_ROM_PICK(loht)
STD_ROM_FN(loht)

static INT32 lohtInit()
{
	return DrvInit(common_080000_0a0000, sound_ram_map, NULL, Z80_FAKE_NMI, 0, 0);
}

struct BurnDriver BurnDrvLoht = {
	"loht", NULL, NULL, NULL, "1989",
	"Legend of Hero Tonma (World)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_RUNGUN, 0,
	NULL, lohtRomInfo, lohtRomName, NULL, NULL, NULL, NULL, CommonInputInfo, LohtDIPInfo,
	lohtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Legend of Hero Tonma (Japan)

static struct BurnRomInfo lohtjRomDesc[] = {
	{ "tom_c-h0-.ic40",		0x20000, 0x2a752998, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "tom_c-l0-.ic37",		0x20000, 0xa224d928, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "tom_c-h3-.ic43",		0x20000, 0x714778b5, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "tom_c-l3-.ic34",		0x20000, 0x2f049b03, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "r200.ic53",			0x20000, 0x0b83265f, 0x02 | BRF_GRA },           //  4 Sprites
	{ "r210.ic51",			0x20000, 0x8ec5f6f3, 0x02 | BRF_GRA },           //  5
	{ "r220.ic49",			0x20000, 0xa41d3bfd, 0x02 | BRF_GRA },           //  6
	{ "r230.ic47",			0x20000, 0x9d81a25b, 0x02 | BRF_GRA },           //  7

	{ "r2a0.a0.ic21",		0x10000, 0x3ca3e771, 0x03 | BRF_GRA },           //  8 Foreground Tiles
	{ "r2a1.a1.ic22",		0x10000, 0x7a05ee2f, 0x03 | BRF_GRA },           //  9
	{ "r2a2.a2.ic20",		0x10000, 0x79aa2335, 0x03 | BRF_GRA },           // 10
	{ "r2a3.a3.ic23",		0x10000, 0x789e8b24, 0x03 | BRF_GRA },           // 11

	{ "078.b0.ic26",		0x10000, 0x44626bf6, 0x04 | BRF_GRA },           // 12 Background Tiles
	{ "079.b1.ic27",		0x10000, 0x464952cf, 0x04 | BRF_GRA },           // 13
	{ "080.b2.ic25",		0x10000, 0x3db9b2c7, 0x04 | BRF_GRA },           // 14
	{ "081.b3.ic24",		0x10000, 0xf01fe899, 0x04 | BRF_GRA },           // 15

	{ "082.ic44",			0x10000, 0x3ed51d1f, 0x05 | BRF_SND },           // 16 DAC Samples

	{ "tom_c-pr-.ic1",		0x01000, 0x9fa9b496, 0x07 | BRF_PRG },           // 17 i8751 microcontroller

	{ "m72_a-8l-.ic66",		0x00100, 0xb460c438, 0x00 | BRF_OPT },           // 18 Proms
	{ "m72_a-9l-.ic75",		0x00100, 0xa4f2c4bc, 0x00 | BRF_OPT },           // 19

	{ "m72_a-3d-.ic11",		0x00117, 0x8a3732ff, 0x00 | BRF_OPT },           // 20 PLDs
	{ "m72_a-4d-.ic19",		0x00117, 0x56c29834, 0x00 | BRF_OPT },           // 21
	{ "tom-c-3f.ic13",		0x00117, 0x2d774e1e, 0x00 | BRF_OPT },           // 22
};

STD_ROM_PICK(lohtj)
STD_ROM_FN(lohtj)

struct BurnDriver BurnDrvLohtj = {
	"lohtj", "loht", NULL, NULL, "1989",
	"Legend of Hero Tonma (Japan)\0", NULL, "Irem", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_RUNGUN, 0,
	NULL, lohtjRomInfo, lohtjRomName, NULL, NULL, NULL, NULL, CommonInputInfo, LohtDIPInfo,
	lohtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};

static INT32 lohtbRomLoadCallback()
{
	// Hack the program rom to work for now
	UINT8 _0x400[216] = {
		0xFA, 0xB9, 0xFF, 0xFF, 0xE2, 0xFE, 0xB8, 0x17, 0x00, 0xE7, 0x40, 0xB8, 0x20, 0x00, 0xE7, 0x42, 
		0xB8, 0x0F, 0x00, 0xE7, 0x42, 0xB8, 0x00, 0xA0, 0x8E, 0xD0, 0x8E, 0xD8, 0xBE, 0x00, 0x3F, 0xB0, 
		0x00, 0x88, 0x04, 0x3A, 0x04, 0x75, 0x08, 0x46, 0xFE, 0xC0, 0x75, 0xF5, 0xE9, 0x18, 0x00, 0x33, 
		0xF6, 0xE9, 0x03, 0x00, 0xBE, 0x40, 0x00, 0xB8, 0x00, 0xB0, 0x8E, 0xD8, 0xC7, 0x06, 0xFE, 0x0F, 
		0x00, 0x00, 0xEA, 0x62, 0x11, 0x00, 0x00, 0xE5, 0x04, 0x0F, 0x19, 0xC0, 0x0F, 0x74, 0xE0, 0xBC, 
		0x00, 0x40, 0x33, 0xDB, 0xB9, 0x00, 0x10, 0xB8, 0x00, 0xB0, 0x8E, 0xD8, 0x8A, 0xC3, 0x02, 0xC7, 
		0x88, 0x07, 0x43, 0xE2, 0xF7, 0x33, 0xC9, 0xFE, 0xC9, 0x75, 0xFC, 0xFE, 0xCD, 0x75, 0xF8, 0x33, 
		0xDB, 0xB9, 0x00, 0x10, 0x8A, 0xC3, 0x02, 0xC7, 0x34, 0xFF, 0x3A, 0x07, 0x75, 0xB6, 0x43, 0xE2, 
		0xF3, 0x33, 0xDB, 0xB9, 0xFC, 0x0F, 0xFE, 0x07, 0x43, 0xE2, 0xFB, 0x90, 0x90, 0x90, 0x9A, 0x00, 
		0x00, 0x00, 0xB0, 0x73, 0xF6, 0x90, 0x90, 0x90, 0xB8, 0x00, 0xB0, 0x8E, 0xC0, 0x33, 0xD2, 0x42, 
		0x81, 0xE2, 0x01, 0x00, 0xBF, 0xB2, 0x04, 0xE8, 0x7B, 0x00, 0xE8, 0x3E, 0x00, 0xEB, 0xF0, 0x90, 
		0x90, 0x90, 0x0B, 0x38, 0x29, 0x43, 0x4F, 0x50, 0x59, 0x52, 0x49, 0x47, 0x48, 0x54, 0x20, 0x31, 
		0x39, 0x38, 0x39, 0x20, 0x49, 0x52, 0x45, 0x4D, 0x20, 0x43, 0x4F, 0x52, 0x50, 0x4F, 0x52, 0x41, 
		0x54, 0x49, 0x4F, 0x4E, 0x0B, 0x50, 0x2B, 0x41
	};

	memcpy (DrvV30ROM + 0x00400, _0x400, 216);

	DrvV30ROM[0x3FC] = 0x88;
	DrvV30ROM[0x3FD] = 0x05;
	DrvV30ROM[0xB42] = 0x1E;
	DrvV30ROM[0xB43] = 0x06;
	DrvV30ROM[0xB44] = 0xB8;
	DrvV30ROM[0x405B] = 0x05;
	DrvV30ROM[0x4066] = 0x05;
	DrvV30ROM[0x103DA] = 0xB0;
	DrvV30ROM[0x103DD] = 0xFE;
	DrvV30ROM[0x103DE] = 0x06;
	DrvV30ROM[0x103DF] = 0xFE;
	DrvV30ROM[0x103E0] = 0x0F;
	DrvV30ROM[0x103E1] = 0xE5;
	DrvV30ROM[0x103E2] = 0x02;
	DrvV30ROM[0x103E3] = 0xA9;
	DrvV30ROM[0x103E4] = 0x20;
	DrvV30ROM[0x103E5] = 0x00;
	DrvV30ROM[0x103E6] = 0x75;
	DrvV30ROM[0x103E7] = 0x05;
	DrvV30ROM[0x103E8] = 0xEA;
	DrvV30ROM[0x103E9] = 0x2F;
	DrvV30ROM[0x103EA] = 0x04;
	DrvV30ROM[0x103EB] = 0x00;
	DrvV30ROM[0x103EC] = 0x00;
	DrvV30ROM[0x103ED] = 0xB8;
	DrvV30ROM[0x10459] = 0xE7;
	DrvV30ROM[0x1045A] = 0x04;
	DrvV30ROM[0x1045B] = 0xE5;
	DrvV30ROM[0x1045C] = 0x02;
	DrvV30ROM[0x1045D] = 0x25;
	DrvV30ROM[0x1045E] = 0x80;
	DrvV30ROM[0x1045F] = 0x00;
	DrvV30ROM[0x10460] = 0x74;
	DrvV30ROM[0x10461] = 0xF9;
	DrvV30ROM[0x10462] = 0x36;
	DrvV30ROM[0x10463] = 0xA1;
	DrvV30ROM[0x10464] = 0xB2;
	DrvV30ROM[0x10465] = 0x23;
	DrvV30ROM[0x10466] = 0xE7;
	DrvV30ROM[0x10467] = 0x82;
	DrvV30ROM[0x10468] = 0x36;
	DrvV30ROM[0x10469] = 0xA1;
	DrvV30ROM[0x1046A] = 0xB6;
	DrvV30ROM[0x1046B] = 0x23;
	DrvV30ROM[0x1046C] = 0xE7;
	DrvV30ROM[0x104B4] = 0x32;
	DrvV30ROM[0x104B5] = 0xE4;
	DrvV30ROM[0x104B6] = 0x3C;
	DrvV30ROM[0x104B7] = 0x7F;
	DrvV30ROM[0x104B8] = 0x72;
	DrvV30ROM[0x10577] = 0x47;
	DrvV30ROM[0x10578] = 0xDC;
	DrvV30ROM[0x10585] = 0x61;
	DrvV30ROM[0x10586] = 0x07;
	DrvV30ROM[0x10587] = 0x1F;
	DrvV30ROM[0x10599] = 0x00;
	DrvV30ROM[0x10793] = 0xE7;
	DrvV30ROM[0x10794] = 0x02;
	DrvV30ROM[0x1E1C0] = 0xBA;
	DrvV30ROM[0x1EA29] = 0xE7;
	DrvV30ROM[0x1EA2A] = 0x02;

	// invert graphics roms
	for (INT32 i = 0; i < 0x80000; i++) {
		DrvGfxROM1[i] ^= 0x0f;
		DrvGfxROM2[i] ^= 0x0f;
	}

	return 0;
}

// Legend of Hero Tonma (Playmark unprotected bootleg)

static struct BurnRomInfo lohtbRomDesc[] = {
	{ "lohtb03.b",			0x20000, 0x8b845a70, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "lohtb05.d",			0x20000, 0xe90f7623, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "lohtb02.a",			0x20000, 0x714778b5, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "lohtb04.c",			0x20000, 0x2f049b03, 0x01 | BRF_PRG | BRF_ESS }, //  3

	{ "lohtb01.02",			0x10000, 0xe4bd8f03, 0x06 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "lohtb14.11",			0x10000, 0xdf5ac5ee, 0x02 | BRF_GRA },           //  5 Sprites
	{ "lohtb18.15",			0x10000, 0xd7ecf849, 0x02 | BRF_GRA },           //  6
	{ "lohtb15.12",			0x10000, 0x45220b01, 0x02 | BRF_GRA },           //  7
	{ "lohtb19.16",			0x10000, 0x35d1a808, 0x02 | BRF_GRA },           //  8
	{ "lohtb16.13",			0x10000, 0x25b85cfc, 0x02 | BRF_GRA },           //  9
	{ "lohtb20.17",			0x10000, 0x464d8579, 0x02 | BRF_GRA },           // 10
	{ "lohtb17.14",			0x10000, 0x763fa4ec, 0x02 | BRF_GRA },           // 11
	{ "lohtb21.18",			0x10000, 0xa73568c7, 0x02 | BRF_GRA },           // 12

	{ "lohtb13.10",			0x10000, 0x359f17d4, 0x03 | BRF_GRA },           // 13 Foreground Tiles
	{ "lohtb11.08",			0x10000, 0x73391e8a, 0x03 | BRF_GRA },           // 14
	{ "lohtb09.06",			0x10000, 0x7096d390, 0x03 | BRF_GRA },           // 15
	{ "lohtb07.04",			0x10000, 0x71a27b81, 0x03 | BRF_GRA },           // 16

	{ "lohtb12.09",			0x10000, 0x4d5e9b53, 0x04 | BRF_GRA },           // 17 Background Tiles
	{ "lohtb10.07",			0x10000, 0x4f75a26a, 0x04 | BRF_GRA },           // 18
	{ "lohtb08.05",			0x10000, 0x34854262, 0x04 | BRF_GRA },           // 19
	{ "lohtb06.03",			0x10000, 0xf923183c, 0x04 | BRF_GRA },           // 20

	{ "gal16v8-25qp.ic3",	0x00117, 0x6acdfafb, 0x00 | BRF_OPT },           // 21 PLDs
};

STD_ROM_PICK(lohtb)
STD_ROM_FN(lohtb)

static INT32 lohtbInit()
{
	install_protection(loht);

	return DrvInit(common_080000_0a0000, sound_ram_map, lohtbRomLoadCallback, Z80_FAKE_NMI, 0, 0);
}

struct BurnDriver BurnDrvLohtb = {
	"lohtb", "loht", NULL, NULL, "1989",
	"Legend of Hero Tonma (Playmark unprotected bootleg)\0", NULL, "bootleg (Playmark)", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_IREM_M72, GBF_RUNGUN, 0,
	NULL, lohtbRomInfo, lohtbRomName, NULL, NULL, NULL, NULL, CommonInputInfo, LohtDIPInfo,
	lohtbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Legend of Hero Tonma (Japan, bootleg with i8751)

static struct BurnRomInfo lohtb2RomDesc[] = {
	{ "loht-a2.bin",	0x10000, 0xccc90e54, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "loht-a10.bin",	0x10000, 0x3aa06730, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "loht-a3.bin",	0x10000, 0xff8a98de, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "loht-a11.bin",	0x10000, 0xeab1d7bc, 0x01 | BRF_PRG | BRF_ESS }, //  3
	{ "loht-a5.bin",	0x10000, 0x79e007ec, 0x01 | BRF_PRG | BRF_ESS }, //  4
	{ "loht-a13.bin",	0x10000, 0xb951346e, 0x01 | BRF_PRG | BRF_ESS }, //  5
	{ "loht-a4.bin",	0x10000, 0x254ea4d5, 0x01 | BRF_PRG | BRF_ESS }, //  6
	{ "loht-a12.bin",	0x10000, 0xcfb0390d, 0x01 | BRF_PRG | BRF_ESS }, //  7

	{ "loht-a16.bin",	0x10000, 0xdf5ac5ee, 0x02 | BRF_GRA },           //  8 Sprites
	{ "loht-a17.bin",	0x10000, 0xd7ecf849, 0x02 | BRF_GRA },           //  9
	{ "loht-a8.bin",	0x10000, 0x45220b01, 0x02 | BRF_GRA },           // 10
	{ "loht-a9.bin",	0x10000, 0x4af9bb3c, 0x02 | BRF_GRA },           // 11
	{ "loht-a14.bin",	0x10000, 0x25b85cfc, 0x02 | BRF_GRA },           // 12
	{ "loht-a15.bin",	0x10000, 0x464d8579, 0x02 | BRF_GRA },           // 13
	{ "loht-a6.bin",	0x10000, 0x763fa4ec, 0x02 | BRF_GRA },           // 14
	{ "loht-a7.bin",	0x10000, 0xa73568c7, 0x02 | BRF_GRA },           // 15

	{ "loht-a19.bin",	0x10000, 0x3ca3e771, 0x03 | BRF_GRA },           // 16 Foreground Tiles
	{ "loht-a20.bin",	0x10000, 0x7a05ee2f, 0x03 | BRF_GRA },           // 17
	{ "loht-a18.bin",	0x10000, 0x79aa2335, 0x03 | BRF_GRA },           // 18
	{ "loht-a21.bin",	0x10000, 0x789e8b24, 0x03 | BRF_GRA },           // 19

	{ "loht-a24.bin",	0x10000, 0x44626bf6, 0x04 | BRF_GRA },           // 20 Background Tiles
	{ "loht-a25.bin",	0x10000, 0x464952cf, 0x04 | BRF_GRA },           // 21
	{ "loht-a23.bin",	0x10000, 0x3db9b2c7, 0x04 | BRF_GRA },           // 22
	{ "loht-a22.bin",	0x10000, 0xf01fe899, 0x04 | BRF_GRA },           // 23

	{ "loht-a1.bin",	0x10000, 0x3ed51d1f, 0x05 | BRF_SND },           // 24 DAC Samples

	{ "loht-a26.bin",	0x02000, 0xac901e17, 0x07 | BRF_PRG },           // 25 i8751 microcontroller
};

STD_ROM_PICK(lohtb2)
STD_ROM_FN(lohtb2)

struct BurnDriver BurnDrvLohtb2 = {
	"lohtb2", "loht", NULL, NULL, "1989",
	"Legend of Hero Tonma (Japan, bootleg with i8751)\0", NULL, "bootleg", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_IREM_M72, GBF_RUNGUN, 0,
	NULL, lohtb2RomInfo, lohtb2RomName, NULL, NULL, NULL, NULL, CommonInputInfo, LohtDIPInfo,
	lohtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Legend of Hero Tonma (World, bootleg with i8751)

static struct BurnRomInfo lohtb3RomDesc[] = {
	{ "9.9",		0x20000, 0xf1e4ebb7, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "1.1",		0x20000, 0xb9384e93, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "i-10.10",	0x10000, 0x79e007ec, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "i-2.2",		0x10000, 0xb951346e, 0x01 | BRF_PRG | BRF_ESS }, //  3
	{ "i-11.11",	0x10000, 0x254ea4d5, 0x01 | BRF_PRG | BRF_ESS }, //  4
	{ "i-3.3",		0x10000, 0xcfb0390d, 0x01 | BRF_PRG | BRF_ESS }, //  5

	{ "i-8.8",		0x10000, 0xdf5ac5ee, 0x02 | BRF_GRA },           //  6 Sprites
	{ "i-7.7",		0x10000, 0xd7ecf849, 0x02 | BRF_GRA },           //  7
	{ "i-15.15",	0x10000, 0x45220b01, 0x02 | BRF_GRA },           //  8
	{ "i-14.14",	0x10000, 0x35d1a808, 0x02 | BRF_GRA },           //  9
	{ "i-6.6",		0x10000, 0x25b85cfc, 0x02 | BRF_GRA },           // 10
	{ "i-5.5",		0x10000, 0x464d8579, 0x02 | BRF_GRA },           // 11
	{ "i-13.13",	0x10000, 0x763fa4ec, 0x02 | BRF_GRA },           // 12
	{ "i-12.12",	0x10000, 0xa73568c7, 0x02 | BRF_GRA },           // 13

	{ "i-20.20",	0x10000, 0x3ca3e771, 0x03 | BRF_GRA },           // 14 Foreground Tiles
	{ "r-21.21",	0x10000, 0x7a05ee2f, 0x03 | BRF_GRA },           // 15
	{ "i-19.19",	0x10000, 0x79aa2335, 0x03 | BRF_GRA },           // 16
	{ "i-22.22",	0x10000, 0x789e8b24, 0x03 | BRF_GRA },           // 17

	{ "r-25.25",	0x10000, 0x44626bf6, 0x04 | BRF_GRA },           // 18 Background Tiles
	{ "r-26.26",	0x10000, 0x464952cf, 0x04 | BRF_GRA },           // 19
	{ "r-24.24",	0x10000, 0x3db9b2c7, 0x04 | BRF_GRA },           // 20
	{ "i-23.23",	0x10000, 0x2e2a085d, 0x04 | BRF_GRA },           // 21

	{ "i-4.4",		0x10000, 0x3ed51d1f, 0x05 | BRF_SND },           // 22 DAC Samples

	{ "c8751h.bin",	0x01000, 0x9c9545f1, 0x07 | BRF_PRG },           // 23 i8751 microcontroller
};

STD_ROM_PICK(lohtb3)
STD_ROM_FN(lohtb3)

struct BurnDriver BurnDrvLohtb3 = {
	"lohtb3", "loht", NULL, NULL, "1997",
	"Legend of Hero Tonma (World, bootleg with i8751)\0", NULL, "bootleg", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_IREM_M72, GBF_RUNGUN, 0,
	NULL, lohtb3RomInfo, lohtb3RomName, NULL, NULL, NULL, NULL, CommonInputInfo, LohtDIPInfo,
	lohtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Legend of Hero Tonma (Gaelco bootleg, Modular System)

static struct BurnRomInfo loht_msRomDesc[] = {
	{ "6_lg_604.ic17",				0x10000, 0x7e84b6ce, 0x01 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "6_lg_601.ic8",				0x10000, 0x7e080cb8, 0x01 | BRF_PRG | BRF_ESS }, //  1
	{ "6_lg_605.ic20",				0x10000, 0xabdcd211, 0x01 | BRF_PRG | BRF_ESS }, //  2
	{ "6_lg_602.ic11",				0x10000, 0x150d1178, 0x01 | BRF_PRG | BRF_ESS }, //  3
	{ "6_lg_606.ic26",				0x20000, 0x714778b5, 0x01 | BRF_PRG | BRF_ESS }, //  4
	{ "6_lg_603.ic25",				0x20000, 0x2f049b03, 0x01 | BRF_PRG | BRF_ESS }, //  5

	{ "1_lg_1.ic12",				0x10000, 0x7aa26b54, 0x06 | BRF_PRG | BRF_ESS }, //  6 Z80 Code

	{ "5_lg_501.ic5",				0x10000, 0xdf5ac5ee, 0x02 | BRF_GRA },           //  7 Sprites
	{ "5_lg_502.ic6",				0x10000, 0xd7ecf849, 0x02 | BRF_GRA },           //  8
	{ "5_lg_503.ic14",				0x10000, 0x45220b01, 0x02 | BRF_GRA },           //  9
	{ "5_lg_504.ic15",				0x10000, 0x35d1a808, 0x02 | BRF_GRA },           // 10
	{ "5_lg_505.ic20",				0x10000, 0x25b85cfc, 0x02 | BRF_GRA },           // 11
	{ "5_lg_506.ic21",				0x10000, 0x464d8579, 0x02 | BRF_GRA },           // 12
	{ "5_lg_507.ic26",				0x10000, 0x763fa4ec, 0x02 | BRF_GRA },           // 13
	{ "5_lg_508.ic27",				0x10000, 0xa73568c7, 0x02 | BRF_GRA },           // 14

	{ "8_lg_801.ic15",				0x10000, 0x359f17d4, 0x03 | BRF_GRA },           // 15 Foreground Tiles
	{ "8_lg_803.ic22",				0x10000, 0x73391e8a, 0x03 | BRF_GRA },           // 16
	{ "8_lg_805.ic30",				0x10000, 0x7096d390, 0x03 | BRF_GRA },           // 17
	{ "8_lg_807.ic37",				0x10000, 0x1c113901, 0x03 | BRF_GRA },           // 18

	{ "8_lg_802.ic14",				0x10000, 0x4d5e9b53, 0x04 | BRF_GRA },           // 19 Background Tiles
	{ "8_lg_804.ic21",				0x10000, 0x4f75a26a, 0x04 | BRF_GRA },           // 20
	{ "8_lg_806.ic29",				0x10000, 0x34854262, 0x04 | BRF_GRA },           // 21
	{ "8_lg_808.ic36",				0x10000, 0xf923183c, 0x04 | BRF_GRA },           // 22

	{ "51_502_82s129n.ic10",		0x00100, 0x15085e44, 0x00 | BRF_OPT },           // 23
	{ "21_204_82s129an.ic4",		0x00100, 0x74470450, 0x00 | BRF_OPT },           // 24
	{ "21_209_82s129an.ic12",		0x00100, 0x7922a7ab, 0x00 | BRF_OPT },           // 25
	{ "1_105_82s123n.ic20",			0x00020, 0x14d72781, 0x00 | BRF_OPT },           // 26

	{ "51_p0503_pal16r6.ic46",		0x00104, 0x07eb86d2, 0x00 | BRF_OPT },           // 27 PLDs

	// these were read protected
	{ "5_5136_gal16v8-25lp.ic9",	0x00001, 0x00000000, 0x00 | BRF_NODUMP },        // 28
	{ "5_5236_gal16v8-25lp.ic8",	0x00001, 0x00000000, 0x00 | BRF_NODUMP },        // 29
	{ "6_686_gal16v8.ic13",			0x00001, 0x00000000, 0x00 | BRF_NODUMP },        // 30
	{ "6_8638_gal16v8.ic7",			0x00001, 0x00000000, 0x00 | BRF_NODUMP },        // 31
	{ "6_subcpu_8600_gal16v8.ic3",	0x00001, 0x00000000, 0x00 | BRF_NODUMP },        // 32
	{ "7_7136_gal20v8-25lp.ic7",	0x00001, 0x00000000, 0x00 | BRF_NODUMP },        // 33
	{ "7_7236_gal20v8-25lp.ic54",	0x00001, 0x00000000, 0x00 | BRF_NODUMP },        // 34
	{ "7_7336_gal16v8.ic55",		0x00001, 0x00000000, 0x00 | BRF_NODUMP },        // 35
	{ "7_7436_gal16v8.ic9",			0x00001, 0x00000000, 0x00 | BRF_NODUMP },        // 36
	{ "7_7536_gal16v8.ic59",		0x00001, 0x00000000, 0x00 | BRF_NODUMP },        // 37
	{ "7_7636_gal20v8-25lp.ic44",	0x00001, 0x00000000, 0x00 | BRF_NODUMP },        // 38
};

STD_ROM_PICK(loht_ms)
STD_ROM_FN(loht_ms)

struct BurnDriver BurnDrvLoht_ms = {
	"loht_ms", "loht", NULL, NULL, "1989",
	"Legend of Hero Tonma (Gaelco bootleg, Modular System)\0", NULL, "bootleg (Gaelco / Ervisa)", "Irem M72",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_IREM_M72, GBF_RUNGUN, 0,
	NULL, loht_msRomInfo, loht_msRomName, NULL, NULL, NULL, NULL, CommonInputInfo, LohtDIPInfo,
	lohtbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Pound for Pound (World)

static struct BurnRomInfo poundforRomDesc[] = {
	{ "pp-a-h0-b.9e",	0x20000, 0x50d4a2d8, 0x01 | BRF_PRG | BRF_ESS },    //  0 V30 Code
	{ "pp-a-l0-b.9d",	0x20000, 0xbd997942, 0x01 | BRF_PRG | BRF_ESS },    //  1
	{ "pp-a-h1-.9f",	0x20000, 0xf6c82f48, 0x01 | BRF_PRG | BRF_ESS },    //  2
	{ "pp-a-l1-.9c",	0x20000, 0x5b07b087, 0x01 | BRF_PRG | BRF_ESS },    //  3

	{ "pp-a-sp.4j",		0x10000, 0x3f458a5b, 0x06 | BRF_PRG | BRF_ESS },    //  4 Z80 Code

	{ "ppb-n0.ic21",	0x40000, 0x951a41f8, 0x02 | BRF_GRA },              //  5 Sprites
	{ "ppb-n1.ic22",	0x40000, 0xc609b7f2, 0x02 | BRF_GRA },              //  6
	{ "ppb-n2.ic35",	0x40000, 0x318c0b5f, 0x02 | BRF_GRA },              //  7
	{ "ppb-n3.ic10",	0x40000, 0x93dc9490, 0x02 | BRF_GRA },              //  8

	{ "ppa-g00.ic19",	0x20000, 0x8a88a174, 0x03 | BRF_GRA },              //  9 Foreground & Background Tiles
	{ "ppa-g10.ic11",	0x20000, 0xe48a66ac, 0x03 | BRF_GRA },              // 10
	{ "ppa-g20.ic18",	0x20000, 0x12b93e79, 0x03 | BRF_GRA },              // 11
	{ "ppa-g30.ic10",	0x20000, 0xfaa39aee, 0x03 | BRF_GRA },              // 12

	{ "ppa-v0.ic6",		0x40000, 0x03321664, 0x05 | BRF_SND },              // 13 DAC Samples

	{ "m85_b-1f-.ic5",	0x00100, 0x00000000, 0x00 | BRF_NODUMP | BRF_OPT }, // 14 Proms
	{ "m85_b-3f-.ic12",	0x00100, 0x00000000, 0x00 | BRF_NODUMP | BRF_OPT }, // 15

	{ "m85_a-5h-.5h",	0x00117, 0xa7ce2e57, 0x00 | BRF_OPT },              // 16 PLDs
	{ "m85_a-6j-.6j",	0x00117, 0x733ed0f9, 0x00 | BRF_OPT },              // 17
	{ "m85_b-1a-.ic1",	0x00117, 0x3cf26744, 0x00 | BRF_OPT },              // 18
};

STD_ROM_PICK(poundfor)
STD_ROM_FN(poundfor)

static INT32 poundforInit()
{
	INT32 rc = DrvInit(rtype2_main_cpu_map, sound_rom_map, NULL, Z80_FAKE_NMI, 4, 0);

	if (!rc) {
		Poundfor = 1;
		BurnTrackballInit(2);
	}

	return rc;
}

struct BurnDriver BurnDrvPoundfor = {
	"poundfor", NULL, NULL, NULL, "1990",
	"Pound for Pound (World)\0", NULL, "Irem", "Irem M85",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_SPORTSMISC, 0,
	NULL, poundforRomInfo, poundforRomName, NULL, NULL, NULL, NULL, PoundforInputInfo, PoundforDIPInfo,
	poundforInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 384, 3, 4
};


// Pound for Pound (Japan)

static struct BurnRomInfo poundforjRomDesc[] = {
	{ "pp-a-h0-.9e",	0x20000, 0xf0165e3b, 0x01 | BRF_PRG | BRF_ESS },    //  0 V30 Code
	{ "pp-a-l0-.9d",	0x20000, 0xf954f99f, 0x01 | BRF_PRG | BRF_ESS },    //  1
	{ "pp-a-h1-.9f",	0x20000, 0xf6c82f48, 0x01 | BRF_PRG | BRF_ESS },    //  2
	{ "pp-a-l1-.9c",	0x20000, 0x5b07b087, 0x01 | BRF_PRG | BRF_ESS },    //  3

	{ "pp-a-sp.4j",		0x10000, 0x3f458a5b, 0x06 | BRF_PRG | BRF_ESS },    //  4 Z80 Code

	{ "ppb-n0.ic21",	0x40000, 0x951a41f8, 0x02 | BRF_GRA },              //  5 Sprites
	{ "ppb-n1.ic22",	0x40000, 0xc609b7f2, 0x02 | BRF_GRA },              //  6
	{ "ppb-n2.ic35",	0x40000, 0x318c0b5f, 0x02 | BRF_GRA },              //  7
	{ "ppb-n3.ic10",	0x40000, 0x93dc9490, 0x02 | BRF_GRA },              //  8

	{ "ppa-g00.ic19",	0x20000, 0x8a88a174, 0x03 | BRF_GRA },              //  9 Foreground & Background Tiles
	{ "ppa-g10.ic11",	0x20000, 0xe48a66ac, 0x03 | BRF_GRA },              // 10
	{ "ppa-g20.ic18",	0x20000, 0x12b93e79, 0x03 | BRF_GRA },              // 11
	{ "ppa-g30.ic10",	0x20000, 0xfaa39aee, 0x03 | BRF_GRA },              // 12

	{ "ppa-v0.ic6",		0x40000, 0x03321664, 0x05 | BRF_SND },              // 13 DAC Samples

	{ "m85_b-1f-.ic5",	0x00100, 0x00000000, 0x00 | BRF_NODUMP | BRF_OPT }, // 14 Proms
	{ "m85_b-3f-.ic12",	0x00100, 0x00000000, 0x00 | BRF_NODUMP | BRF_OPT }, // 15

	{ "m85_a-5h-.5h",	0x00117, 0xa7ce2e57, 0x00 | BRF_OPT },              // 16 PLDs
	{ "m85_a-6j-.6j",	0x00117, 0x733ed0f9, 0x00 | BRF_OPT },              // 17
	{ "m85_b-1a-.ic1",	0x00117, 0x3cf26744, 0x00 | BRF_OPT },              // 18
};

STD_ROM_PICK(poundforj)
STD_ROM_FN(poundforj)

struct BurnDriver BurnDrvPoundforj = {
	"poundforj", "poundfor", NULL, NULL, "1990",
	"Pound for Pound (Japan)\0", NULL, "Irem", "Irem M85",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_SPORTSMISC, 0,
	NULL, poundforjRomInfo, poundforjRomName, NULL, NULL, NULL, NULL, PoundforInputInfo, PoundforDIPInfo,
	poundforInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 384, 3, 4
};


// Pound for Pound (US)

static struct BurnRomInfo poundforuRomDesc[] = {
	{ "pp-a-h0-a.9e",	0x20000, 0xff4c83a4, 0x01 | BRF_PRG | BRF_ESS },    //  0 V30 Code
	{ "pp-a-l0-a.9d",	0x20000, 0x3374ce8f, 0x01 | BRF_PRG | BRF_ESS },    //  1
	{ "pp-a-h1-.9f",	0x20000, 0xf6c82f48, 0x01 | BRF_PRG | BRF_ESS },    //  2
	{ "pp-a-l1-.9c",	0x20000, 0x5b07b087, 0x01 | BRF_PRG | BRF_ESS },    //  3

	{ "pp-a-sp.4j",		0x10000, 0x3f458a5b, 0x06 | BRF_PRG | BRF_ESS },    //  4 Z80 Code

	{ "ppb-n0.ic21",	0x40000, 0x951a41f8, 0x02 | BRF_GRA },              //  5 Sprites
	{ "ppb-n1.ic22",	0x40000, 0xc609b7f2, 0x02 | BRF_GRA },              //  6
	{ "ppb-n2.ic35",	0x40000, 0x318c0b5f, 0x02 | BRF_GRA },              //  7
	{ "ppb-n3.ic10",	0x40000, 0x93dc9490, 0x02 | BRF_GRA },              //  8

	{ "ppa-g00.ic19",	0x20000, 0x8a88a174, 0x03 | BRF_GRA },              //  9 Foreground & Background Tiles
	{ "ppa-g10.ic11",	0x20000, 0xe48a66ac, 0x03 | BRF_GRA },              // 10
	{ "ppa-g20.ic18",	0x20000, 0x12b93e79, 0x03 | BRF_GRA },              // 11
	{ "ppa-g30.ic10",	0x20000, 0xfaa39aee, 0x03 | BRF_GRA },              // 12

	{ "ppa-v0.ic6",		0x40000, 0x03321664, 0x05 | BRF_SND },              // 13 DAC Samples

	{ "m85_b-1f-.ic5",	0x00100, 0x00000000, 0x00 | BRF_NODUMP | BRF_OPT }, // 14 Proms
	{ "m85_b-3f-.ic12",	0x00100, 0x00000000, 0x00 | BRF_NODUMP | BRF_OPT }, // 15

	{ "m85_a-5h-.5h",	0x00117, 0xa7ce2e57, 0x00 | BRF_OPT },              // 16 PLDs
	{ "m85_a-6j-.6j",	0x00117, 0x733ed0f9, 0x00 | BRF_OPT },              // 17
	{ "m85_b-1a-.ic1",	0x00117, 0x3cf26744, 0x00 | BRF_OPT },              // 18
};

STD_ROM_PICK(poundforu)
STD_ROM_FN(poundforu)

struct BurnDriver BurnDrvPoundforu = {
	"poundforu", "poundfor", NULL, NULL, "1990",
	"Pound for Pound (US)\0", NULL, "Irem America", "Irem M85",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_SPORTSMISC, 0,
	NULL, poundforuRomInfo, poundforuRomName, NULL, NULL, NULL, NULL, PoundforInputInfo, PoundforDIPInfo,
	poundforInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 384, 3, 4
};


// Major Title (World)

static struct BurnRomInfo majtitleRomDesc[] = {
	{ "mt_a-h0-a.ic52",	0x20000, 0x36aadb67, 0x01 | BRF_PRG | BRF_ESS },    //  0 V30 Code
	{ "mt_a-l0-a.ic60",	0x20000, 0x2e1b6242, 0x01 | BRF_PRG | BRF_ESS },    //  1
	{ "mt_a-h1-a.ic51",	0x20000, 0xe1402a22, 0x01 | BRF_PRG | BRF_ESS },    //  2
	{ "mt_a-l1-a.ic59",	0x20000, 0x0efa409a, 0x01 | BRF_PRG | BRF_ESS },    //  3

	{ "mt_a-sp.ic15",	0x10000, 0xe44260a9, 0x06 | BRF_PRG | BRF_ESS },    //  4 Z80 Code

	{ "mt_n0.ic44",		0x40000, 0x5618cddc, 0x02 | BRF_GRA },              //  5 Sprites
	{ "mt_n1.ic45",		0x40000, 0x483b873b, 0x02 | BRF_GRA },              //  6
	{ "mt_n2.ic46",		0x40000, 0x4f5d665b, 0x02 | BRF_GRA },              //  7
	{ "mt_n3.ic36",		0x40000, 0x83571549, 0x02 | BRF_GRA },              //  8

	{ "mt_c0.ic49",		0x20000, 0x780e7a02, 0x03 | BRF_GRA },              //  9 Foreground & Background Tiles
	{ "mt_c1.ic48",		0x20000, 0x45ad1381, 0x03 | BRF_GRA },              // 10
	{ "mt_c2.ic57",		0x20000, 0x5df5856d, 0x03 | BRF_GRA },              // 11
	{ "mt_c3.ic56",		0x20000, 0xf5316cc8, 0x03 | BRF_GRA },              // 12

	{ "mt_f0.ic38",		0x20000, 0x2d5e05d5, 0x0e | BRF_GRA },              // 13 Sprites
	{ "mt_f1.ic39",		0x20000, 0xc68cd65f, 0x0e | BRF_GRA },              // 14
	{ "mt_f2.ic40",		0x20000, 0xa71feb2d, 0x0e | BRF_GRA },              // 15
	{ "mt_f3.ic41",		0x20000, 0x179f7562, 0x0e | BRF_GRA },              // 16

	{ "mt_vo.ic12",		0x20000, 0xeb24bb2c, 0x05 | BRF_SND },              // 17 DAC Samples

	{ "mt_b-6a-.ic37",	0x00100, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP }, // 18 Proms
	{ "mt_b-7c-.ic47",	0x00100, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP }, // 19

	{ "mt_b-3q-.ic23",	0x00117, 0x8c864543, 0x00 | BRF_OPT },              // 20 PLDs
	{ "mt_a-2h-.ic5",	0x00117, 0x21ede612, 0x00 | BRF_OPT },              // 21
	{ "mt_a-5l-.ic31",	0x00117, 0xe3064bfe, 0x00 | BRF_OPT },              // 22
	{ "mt_a-7d-.ic43",	0x00117, 0xd7ec1cc6, 0x00 | BRF_OPT },              // 23
};

STD_ROM_PICK(majtitle)
STD_ROM_FN(majtitle)

static INT32 majtitleInit()
{
	return DrvInit(majtitle_main_cpu_map, sound_rom_map, NULL, Z80_REAL_NMI, 3, 0);
}

struct BurnDriver BurnDrvMajtitle = {
	"majtitle", NULL, NULL, NULL, "1990",
	"Major Title (World)\0", NULL, "Irem", "Irem M84",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M72, GBF_SPORTSMISC, 0,
	NULL, majtitleRomInfo, majtitleRomName, NULL, NULL, NULL, NULL, CommonInputInfo, Rtype2DIPInfo,
	majtitleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};


// Major Title (Japan)

static struct BurnRomInfo majtitlejRomDesc[] = {
	{ "mt_a-h0-.ic52",		0x20000, 0xb9682c70, 0x01 | BRF_PRG | BRF_ESS },    //  0 V30 Code
	{ "mt_a-l0-.ic60",		0x20000, 0x702c9fd6, 0x01 | BRF_PRG | BRF_ESS },    //  1
	{ "mt_a-h1-.ic51",		0x20000, 0xd9e97c30, 0x01 | BRF_PRG | BRF_ESS },    //  2
	{ "mt_a-l1-.ic59",		0x20000, 0x8dbd91b5, 0x01 | BRF_PRG | BRF_ESS },    //  3

	{ "mt_a-sp.ic15",		0x10000, 0xe44260a9, 0x06 | BRF_PRG | BRF_ESS },    //  4 Z80 Code

	{ "mt_n0.ic44",		0x40000, 0x5618cddc, 0x02 | BRF_GRA },              //  5 Sprites
	{ "mt_n1.ic45",		0x40000, 0x483b873b, 0x02 | BRF_GRA },              //  6
	{ "mt_n2.ic46",		0x40000, 0x4f5d665b, 0x02 | BRF_GRA },              //  7
	{ "mt_n3.ic36",		0x40000, 0x83571549, 0x02 | BRF_GRA },              //  8

	{ "mt_c0.ic49",		0x20000, 0x780e7a02, 0x03 | BRF_GRA },              //  9 Foreground & Background Tiles
	{ "mt_c1.ic48",		0x20000, 0x45ad1381, 0x03 | BRF_GRA },              // 10
	{ "mt_c2.ic57",		0x20000, 0x5df5856d, 0x03 | BRF_GRA },              // 11
	{ "mt_c3.ic56",		0x20000, 0xf5316cc8, 0x03 | BRF_GRA },              // 12

	{ "mt_f0.ic38",		0x20000, 0x2d5e05d5, 0x0e | BRF_GRA },              // 13 Sprites
	{ "mt_f1.ic39",		0x20000, 0xc68cd65f, 0x0e | BRF_GRA },              // 14
	{ "mt_f2.ic40",		0x20000, 0xa71feb2d, 0x0e | BRF_GRA },              // 15
	{ "mt_f3.ic41",		0x20000, 0x179f7562, 0x0e | BRF_GRA },              // 16

	{ "mt_vo.ic12",		0x20000, 0xeb24bb2c, 0x05 | BRF_SND },              // 17 DAC Samples

	{ "mt_b-6a-.ic37",	0x00100, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP }, // 18 Proms
	{ "mt_b-7c-.ic47",	0x00100, 0x00000000, 0x00 | BRF_OPT | BRF_NODUMP }, // 19

	{ "mt_a-2h-.ic5",	0x00117, 0x21ede612, 0x00 | BRF_OPT },              // 20 PLDs
	{ "mt_a-5l-.ic31",	0x00117, 0xe3064bfe, 0x00 | BRF_OPT },              // 21
	{ "mt_a-7d-.ic43",	0x00117, 0xd7ec1cc6, 0x00 | BRF_OPT },              // 22
	{ "mt_b-3q-.ic23",	0x00117, 0x8c864543, 0x00 | BRF_OPT },              // 23
};

STD_ROM_PICK(majtitlej)
STD_ROM_FN(majtitlej)

struct BurnDriver BurnDrvMajtitlej = {
	"majtitlej", "majtitle", NULL, NULL, "1990",
	"Major Title (Japan)\0", NULL, "Irem", "Irem M84",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED | BDF_CLONE, 2, HARDWARE_IREM_M72, GBF_SPORTSMISC, 0,
	NULL, majtitlejRomInfo, majtitlejRomName, NULL, NULL, NULL, NULL, CommonInputInfo, Rtype2DIPInfo,
	majtitleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	384, 256, 4, 3
};
