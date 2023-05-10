// FinalBurn Neo Jaleco MegaSystem 32 driver module
// Based on MAME driver by David Haywood and Paul Priest

#include "tiles_generic.h"
#include "v60_intf.h"
#include "z80_intf.h"
#include "burn_ymf271.h"
#include "bitswap.h"

// Note: f1superb 	- not added (non-working)
// some games have very minor gfx issues.

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvV60ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[4];
static UINT8 *DrvSndROM;
static UINT8 *DrvV60RAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvPriRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvRozRAM;
static UINT8 *DrvRozBuf;
static UINT8 *DrvLineRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvTxRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvRozCtrl;
static UINT8 *DrvTxCtrl;
static UINT8 *DrvBgCtrl;
static UINT8 *DrvSysCtrl;
static UINT8 *DrvSprCtrl;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 z80_bank;
static UINT16 bright[8];
static INT32 v60_irq_vector;
static UINT8 flipscreen;
static UINT8 soundlatch;
static UINT32 to_main;
static UINT32 tilemaplayoutcontrol;
static INT32 mahjong_select;

static INT32 input_is_mahjong;
static INT32 graphics_length[4];

static UINT8 DrvJoy1[32];
static UINT8 DrvMah1[8];
static UINT8 DrvMah2[8];
static UINT8 DrvMah3[8];
static UINT8 DrvMah4[8];
static UINT8 DrvMah5[8];
static UINT32 DrvInputs[1];
static UINT8 DrvMahjongInputs[5];
static UINT8 DrvDips[4];
static UINT8 DrvReset;

// wpksocv2
static INT16 Analog[1];
static INT32 analog_target;
static INT32 analog_adder;
static INT32 analog_clock;
static INT32 analog_starttimer;

static INT32 is_wpksocv2 = 0;
static INT32 is_p47acesa = 0; // game fails to boot when nvram is saved?

static struct BurnInputInfo MS32InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy1 + 22,	"p1 fire 5"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 17,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 21,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy1 + 23,	"p2 fire 5"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 18,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(MS32)

static struct BurnInputInfo MS32MahjongInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvMah1 + 0,	"p1 start"	},
	{"P1 A",			BIT_DIGITAL,	DrvMah1 + 1,	"mah a"		},
	{"P1 B",			BIT_DIGITAL,	DrvMah2 + 1,	"mah b"		},
	{"P1 C",			BIT_DIGITAL,	DrvMah3 + 1,	"mah c"		},
	{"P1 D",			BIT_DIGITAL,	DrvMah4 + 1,	"mah d"		},
	{"P1 E",			BIT_DIGITAL,	DrvMah1 + 2,	"mah e"		},
	{"P1 F",			BIT_DIGITAL,	DrvMah2 + 2,	"mah f"		},
	{"P1 G",			BIT_DIGITAL,	DrvMah3 + 2,	"mah g"		},
	{"P1 H",			BIT_DIGITAL,	DrvMah4 + 2,	"mah h"		},
	{"P1 I",			BIT_DIGITAL,	DrvMah1 + 4,	"mah i"		},
	{"P1 J",			BIT_DIGITAL,	DrvMah2 + 4,	"mah j"		},
	{"P1 K",			BIT_DIGITAL,	DrvMah3 + 4,	"mah k"		},
	{"P1 L",			BIT_DIGITAL,	DrvMah4 + 4,	"mah l"		},
	{"P1 M",			BIT_DIGITAL,	DrvMah1 + 3,	"mah m"		},
	{"P1 N",			BIT_DIGITAL,	DrvMah2 + 3,	"mah n"		},
	{"P1 Pon",			BIT_DIGITAL,	DrvMah4 + 3,	"mah pon"	},
	{"P1 Chi",			BIT_DIGITAL,	DrvMah3 + 3,	"mah chi"	},
	{"P1 Kan",			BIT_DIGITAL,	DrvMah1 + 5,	"mah kan"	},
	{"P1 Ron",			BIT_DIGITAL,	DrvMah3 + 5,	"mah ron"	},
	{"P1 Reach",		BIT_DIGITAL,	DrvMah2 + 5,	"mah reach"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 18,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(MS32Mahjong)

static struct BurnInputInfo Hayaosi2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 5"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 17,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 21,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 5"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p3 start"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy1 + 12,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy1 + 13,	"p3 fire 4"	},
	{"P3 Button 5",		BIT_DIGITAL,	DrvJoy1 + 7,	"p3 fire 5"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 18,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Hayaosi2)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo Wpksocv2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 start"	},
	{"P1 Select",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 select"	}, // needed!
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 right"	},
	A("P1 Kick",		BIT_ANALOG_REL, &Analog[0],		"p1 z-axis"	),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 17,	"p2 coin"	}, // needed!

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 18,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 19,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};
#undef A
STDINPUTINFO(Wpksocv2)

static struct BurnDIPInfo MS32DIPList[] = {
	{0x18, 0xff, 0xff, 0xff, NULL							},
	{0x19, 0xff, 0xff, 0xff, NULL							},
	{0x1a, 0xff, 0xff, 0x7f, NULL							},
	{0x1b, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x19, 0x01, 0x01, 0x01, "Off"							},
	{0x19, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x19, 0x01, 0x02, 0x02, "Off"							},
	{0x19, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    8, "Coin B"						},
	{0x19, 0x01, 0x1c, 0x00, "5 Coins 1 Credits"			},
	{0x19, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"			},
	{0x19, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"			},
	{0x19, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"			},
	{0x19, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"			},
	{0x19, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"			},
	{0x19, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"			},
	{0x19, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"			},

	{0   , 0xfe, 0   ,    8, "Coin A"						},
	{0x19, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"			},
	{0x19, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"			},
	{0x19, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"			},
	{0x19, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"			},
	{0x19, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"			},
	{0x19, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"			},
	{0x19, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"			},
	{0x19, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"			},
};

//STDDIPINFO(MS32)

static struct BurnDIPInfo TetrispDIPList[]=
{
	{0x1a, 0xff, 0xff, 0x7f, NULL							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x18, 0x01, 0x01, 0x01, "Off"							},
	{0x18, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x18, 0x01, 0x02, 0x00, "Off"							},
	{0x18, 0x01, 0x02, 0x02, "On"							},

	{0   , 0xfe, 0   ,    3, "Join In"						},
	{0x18, 0x01, 0x0c, 0x0c, "All Modes"					},
	{0x18, 0x01, 0x0c, 0x04, "Normal and Puzzle Modes"		},
	{0x18, 0x01, 0x0c, 0x08, "VS Mode"						},

	{0   , 0xfe, 0   ,    4, "Winning Rounds (PvP)"			},
	{0x18, 0x01, 0x30, 0x00, "1/1"							},
	{0x18, 0x01, 0x30, 0x30, "2/3"							},
	{0x18, 0x01, 0x30, 0x10, "3/5"							},
	{0x18, 0x01, 0x30, 0x20, "4/7"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x18, 0x01, 0xc0, 0x00, "Easy"							},
	{0x18, 0x01, 0xc0, 0xc0, "Normal"						},
	{0x18, 0x01, 0xc0, 0x40, "Hard"							},
	{0x18, 0x01, 0xc0, 0x80, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Freeze"						},
	{0x1a, 0x01, 0x01, 0x01, "Off"							},
	{0x1a, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "After VS Mode"				},
	{0x1a, 0x01, 0x08, 0x08, "Game Ends"					},
	{0x1a, 0x01, 0x08, 0x00, "Winner Continues"				},

	{0   , 0xfe, 0   ,    2, "Voice"						},
	{0x1a, 0x01, 0x10, 0x00, "English Only"					},
	{0x1a, 0x01, 0x10, 0x10, "Yes"							},

	{0   , 0xfe, 0   ,    2, "FBI Logo"						},
	{0x1a, 0x01, 0x40, 0x40, "No"							},
	{0x1a, 0x01, 0x40, 0x00, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Language"						},
	{0x1a, 0x01, 0x80, 0x80, "Japanese"						},
	{0x1a, 0x01, 0x80, 0x00, "English"						},
};

STDDIPINFOEXT(Tetrisp, MS32, Tetrisp)

static struct BurnDIPInfo P47acesDIPList[]=
{
	{0x1a, 0xff, 0xff, 0x7f, NULL							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x18, 0x01, 0x01, 0x01, "Off"							},
	{0x18, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x18, 0x01, 0x02, 0x00, "Off"							},
	{0x18, 0x01, 0x02, 0x02, "On"							},

	{0   , 0xfe, 0   ,    2, "Bonus Life"					},
	{0x18, 0x01, 0x04, 0x04, "500k"							},
	{0x18, 0x01, 0x04, 0x00, "None"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x18, 0x01, 0x30, 0x00, "1"							},
	{0x18, 0x01, 0x30, 0x20, "2"							},
	{0x18, 0x01, 0x30, 0x30, "3"							},
	{0x18, 0x01, 0x30, 0x10, "4"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x18, 0x01, 0xc0, 0x00, "Easy"							},
	{0x18, 0x01, 0xc0, 0xc0, "Normal"						},
	{0x18, 0x01, 0xc0, 0x40, "Hard"							},
	{0x18, 0x01, 0xc0, 0x80, "Hardest"						},

	{0   , 0xfe, 0   ,    3, "FG/BG X offset"				},
	{0x1a, 0x01, 0x03, 0x03, "0/0"							},
	{0x1a, 0x01, 0x03, 0x02, "5/5"							},
	{0x1a, 0x01, 0x03, 0x00, "2/4"							},

	{0   , 0xfe, 0   ,    2, "FBI Logo"						},
	{0x1a, 0x01, 0x40, 0x40, "No"							},
	{0x1a, 0x01, 0x40, 0x00, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Language"						},
	{0x1a, 0x01, 0x80, 0x80, "Japanese"						},
	{0x1a, 0x01, 0x80, 0x00, "English"						},
};

STDDIPINFOEXT(P47aces, MS32, P47aces)

static struct BurnDIPInfo BbbxingDIPList[]=
{
	{0x18, 0xff, 0xff, 0xfd, NULL							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x18, 0x01, 0x01, 0x01, "Off"							},
	{0x18, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Language"						},
	{0x18, 0x01, 0x02, 0x02, "Japanese"						},
	{0x18, 0x01, 0x02, 0x00, "English"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x18, 0x01, 0x04, 0x00, "Off"							},
	{0x18, 0x01, 0x04, 0x04, "On"							},

	{0   , 0xfe, 0   ,    4, "Timer Speed"					},
	{0x18, 0x01, 0x30, 0x00, "60/60"						},
	{0x18, 0x01, 0x30, 0x20, "50/60"						},
	{0x18, 0x01, 0x30, 0x10, "40/60"						},
	{0x18, 0x01, 0x30, 0x30, "35/60"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x18, 0x01, 0xc0, 0x00, "Easy"							},
	{0x18, 0x01, 0xc0, 0xc0, "Normal"						},
	{0x18, 0x01, 0xc0, 0x40, "Hard"							},
	{0x18, 0x01, 0xc0, 0x80, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Grute's Attack Power"			},
	{0x1a, 0x01, 0x02, 0x02, "Normal"						},
	{0x1a, 0x01, 0x02, 0x00, "Harder"						},

	{0   , 0xfe, 0   ,    2, "Biff's Attack Power"			},
	{0x1a, 0x01, 0x04, 0x04, "Normal"						},
	{0x1a, 0x01, 0x04, 0x00, "Harder"						},

	{0   , 0xfe, 0   ,    2, "Carolde's Attack Power"		},
	{0x1a, 0x01, 0x08, 0x08, "Normal"						},
	{0x1a, 0x01, 0x08, 0x00, "Harder"						},

	{0   , 0xfe, 0   ,    2, "Jose's Attack Power"			},
	{0x1a, 0x01, 0x10, 0x10, "Normal"						},
	{0x1a, 0x01, 0x10, 0x00, "Harder"						},

	{0   , 0xfe, 0   ,    2, "Thamalatt's Attack Power"		},
	{0x1a, 0x01, 0x20, 0x20, "Normal"						},
	{0x1a, 0x01, 0x20, 0x00, "Harder"						},

	{0   , 0xfe, 0   ,    2, "Kim's Attack Power"			},
	{0x1a, 0x01, 0x40, 0x40, "Normal"						},
	{0x1a, 0x01, 0x40, 0x00, "Harder"						},

	{0   , 0xfe, 0   ,    2, "Jyoji's Attack Power"			},
	{0x1a, 0x01, 0x80, 0x80, "Normal"						},
	{0x1a, 0x01, 0x80, 0x00, "Harder"						},
};

STDDIPINFOEXT(Bbbxing, MS32, Bbbxing)

static struct BurnDIPInfo DesertwrDIPList[]=
{
	{0x1a, 0xff, 0xff, 0x6f, NULL							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x18, 0x01, 0x01, 0x01, "Off"							},
	{0x18, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x18, 0x01, 0x02, 0x00, "Off"							},
	{0x18, 0x01, 0x02, 0x02, "On"							},

	{0   , 0xfe, 0   ,    3, "Armors"						},
	{0x18, 0x01, 0x30, 0x10, "2"							},
	{0x18, 0x01, 0x30, 0x30, "3"							},
	{0x18, 0x01, 0x30, 0x20, "4"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x18, 0x01, 0xc0, 0x00, "Easy"							},
	{0x18, 0x01, 0xc0, 0xc0, "Normal"						},
	{0x18, 0x01, 0xc0, 0x40, "Hard"							},
	{0x18, 0x01, 0xc0, 0x80, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Title screen"					},
	{0x1a, 0x01, 0x10, 0x10, "Japanese"						},
	{0x1a, 0x01, 0x10, 0x00, "English"						},

	{0   , 0xfe, 0   ,    2, "FBI Logo"						},
	{0x1a, 0x01, 0x40, 0x40, "No"							},
	{0x1a, 0x01, 0x40, 0x00, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Language"						},
	{0x1a, 0x01, 0x80, 0x80, "Japanese"						},
	{0x1a, 0x01, 0x80, 0x00, "English"						},
};

STDDIPINFOEXT(Desertwr, MS32, Desertwr)

static struct BurnDIPInfo GametngkDIPList[]=
{
	{0x1a, 0xff, 0xff, 0x7f, NULL							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x18, 0x01, 0x01, 0x01, "Off"							},
	{0x18, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x18, 0x01, 0x02, 0x00, "Off"							},
	{0x18, 0x01, 0x02, 0x02, "On"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x18, 0x01, 0x30, 0x00, "1"							},
	{0x18, 0x01, 0x30, 0x30, "3"							},
	{0x18, 0x01, 0x30, 0x10, "4"							},
	{0x18, 0x01, 0x30, 0x20, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x18, 0x01, 0xc0, 0x00, "Easy"							},
	{0x18, 0x01, 0xc0, 0xc0, "Normal"						},
	{0x18, 0x01, 0xc0, 0x40, "Hard"							},
	{0x18, 0x01, 0xc0, 0x80, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Freeze"						},
	{0x1a, 0x01, 0x01, 0x01, "Off"							},
	{0x1a, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"		},
	{0x1a, 0x01, 0x02, 0x02, "Off"							},
	{0x1a, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Voice"						},
	{0x1a, 0x01, 0x20, 0x00, "No"							},
	{0x1a, 0x01, 0x20, 0x20, "Yes"							},

	{0   , 0xfe, 0   ,    2, "FBI Logo"						},
	{0x1a, 0x01, 0x40, 0x40, "No"							},
	{0x1a, 0x01, 0x40, 0x00, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Language"						},
	{0x1a, 0x01, 0x80, 0x80, "Japanese"						},
	{0x1a, 0x01, 0x80, 0x00, "English"						},
};

STDDIPINFOEXT(Gametngk, MS32, Gametngk)

static struct BurnDIPInfo GratiaDIPList[]=
{
	{0x1a, 0xff, 0xff, 0x7f, NULL							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x18, 0x01, 0x01, 0x01, "Off"							},
	{0x18, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x18, 0x01, 0x02, 0x00, "Off"							},
	{0x18, 0x01, 0x02, 0x02, "On"							},

	{0   , 0xfe, 0   ,    2, "Bonus Life"					},
	{0x18, 0x01, 0x04, 0x04, "200k and every 1000k"			},
	{0x18, 0x01, 0x04, 0x00, "None"							},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"		},
	{0x18, 0x01, 0x08, 0x08, "Off"							},
	{0x18, 0x01, 0x08, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x18, 0x01, 0x30, 0x00, "1"							},
	{0x18, 0x01, 0x30, 0x20, "2"							},
	{0x18, 0x01, 0x30, 0x30, "3"							},
	{0x18, 0x01, 0x30, 0x10, "4"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x18, 0x01, 0xc0, 0x00, "Easy"							},
	{0x18, 0x01, 0xc0, 0xc0, "Normal"						},
	{0x18, 0x01, 0xc0, 0x40, "Hard"							},
	{0x18, 0x01, 0xc0, 0x80, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "FBI Logo"						},
	{0x1a, 0x01, 0x40, 0x40, "No"							},
	{0x1a, 0x01, 0x40, 0x00, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Language"						},
	{0x1a, 0x01, 0x80, 0x80, "Japanese"						},
	{0x1a, 0x01, 0x80, 0x00, "English"						},
};

STDDIPINFOEXT(Gratia, MS32, Gratia)

static struct BurnDIPInfo Tp2m32DIPList[]=
{
	{0x1a, 0xff, 0xff, 0x7f, NULL							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x18, 0x01, 0x01, 0x01, "Off"							},
	{0x18, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x18, 0x01, 0x02, 0x00, "Off"							},
	{0x18, 0x01, 0x02, 0x02, "On"							},

	{0   , 0xfe, 0   ,    4, "VS Match"						},
	{0x18, 0x01, 0x0c, 0x00, "1"							},
	{0x18, 0x01, 0x0c, 0x0c, "3"							},
	{0x18, 0x01, 0x0c, 0x04, "5"							},
	{0x18, 0x01, 0x0c, 0x08, "7"							},

	{0   , 0xfe, 0   ,    4, "Puzzle Difficulty"			},
	{0x18, 0x01, 0x30, 0x00, "Easy"							},
	{0x18, 0x01, 0x30, 0x30, "Normal"						},
	{0x18, 0x01, 0x30, 0x10, "Hard"							},
	{0x18, 0x01, 0x30, 0x20, "Very Hard"					},

	{0   , 0xfe, 0   ,    4, "Endless Difficulty"			},
	{0x18, 0x01, 0xc0, 0x00, "Easy"							},
	{0x18, 0x01, 0xc0, 0xc0, "Normal"						},
	{0x18, 0x01, 0xc0, 0x40, "Hard"							},
	{0x18, 0x01, 0xc0, 0x80, "Very Hard"					},

	{0   , 0xfe, 0   ,    2, "Freeze"						},
	{0x1a, 0x01, 0x01, 0x01, "Off"							},
	{0x1a, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Bomb"							},
	{0x1a, 0x01, 0x06, 0x04, "0"							},
	{0x1a, 0x01, 0x06, 0x02, "1"							},
	{0x1a, 0x01, 0x06, 0x06, "2"							},
	{0x1a, 0x01, 0x06, 0x00, "3"							},

	{0   , 0xfe, 0   ,    2, "Join In"						},
	{0x1a, 0x01, 0x08, 0x00, "Off"							},
	{0x1a, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    2, "Voice"						},
	{0x1a, 0x01, 0x20, 0x00, "Off"							},
	{0x1a, 0x01, 0x20, 0x20, "On"							},

	{0   , 0xfe, 0   ,    2, "FBI Logo"						},
	{0x1a, 0x01, 0x40, 0x40, "No"							},
	{0x1a, 0x01, 0x40, 0x00, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Language"						},
	{0x1a, 0x01, 0x80, 0x80, "Japanese"						},
	{0x1a, 0x01, 0x80, 0x00, "English"						},
};

STDDIPINFOEXT(Tp2m32, MS32, Tp2m32)

static struct BurnDIPInfo Hayaosi2DIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL							},
	{0x17, 0xff, 0xff, 0xff, NULL							},
	{0x18, 0xff, 0xff, 0xff, NULL							},
	{0x19, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x16, 0x01, 0x04, 0x00, "Off"							},
	{0x16, 0x01, 0x04, 0x04, "On"							},

	{0   , 0xfe, 0   ,    2, "Questions (VS Mode)"			},
	{0x16, 0x01, 0x08, 0x08, "Default"						},
	{0x16, 0x01, 0x08, 0x00, "More"							},

	{0   , 0xfe, 0   ,    4, "Time (Race Mode)"				},
	{0x16, 0x01, 0x30, 0x00, "Default - 0:15"				},
	{0x16, 0x01, 0x30, 0x20, "Default - 0:10"				},
	{0x16, 0x01, 0x30, 0x30, "Default"						},
	{0x16, 0x01, 0x30, 0x10, "Default + 0:15"				},

	{0   , 0xfe, 0   ,    4, "Computer's AI (VS Mode)"		},
	{0x16, 0x01, 0xc0, 0x40, "Low"							},
	{0x16, 0x01, 0xc0, 0xc0, "Normal"						},
	{0x16, 0x01, 0xc0, 0x80, "High"							},
	{0x16, 0x01, 0xc0, 0x00, "Highest"						},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x17, 0x01, 0x02, 0x02, "Off"							},
	{0x17, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    8, "Coin B"						},
	{0x17, 0x01, 0x1c, 0x00, "5 Coins 1 Credits"			},
	{0x17, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"			},
	{0x17, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"			},
	{0x17, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"			},
	{0x17, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"			},
	{0x17, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"			},
	{0x17, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"			},
	{0x17, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"			},

	{0   , 0xfe, 0   ,    8, "Coin A"						},
	{0x17, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"			},
	{0x17, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"			},
	{0x17, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"			},
	{0x17, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"			},
	{0x17, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"			},
	{0x17, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"			},
	{0x17, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"			},
	{0x17, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"			},
};

STDDIPINFO(Hayaosi2)

static struct BurnDIPInfo Hayaosi3DIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL							},
	{0x17, 0xff, 0xff, 0xff, NULL							},
	{0x18, 0xff, 0xff, 0xff, NULL							},
	{0x19, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x16, 0x01, 0x02, 0x00, "Off"							},
	{0x16, 0x01, 0x02, 0x02, "On"							},

	{0   , 0xfe, 0   ,    2, "Questions (VS Mode)"			},
	{0x16, 0x01, 0x08, 0x08, "Default"						},
	{0x16, 0x01, 0x08, 0x00, "More"							},

	{0   , 0xfe, 0   ,    4, "Time (Race Mode)"				},
	{0x16, 0x01, 0x30, 0x00, "Default - 0:15"				},
	{0x16, 0x01, 0x30, 0x20, "Default - 0:10"				},
	{0x16, 0x01, 0x30, 0x30, "Default"						},
	{0x16, 0x01, 0x30, 0x10, "Default + 0:15"				},

	{0   , 0xfe, 0   ,    4, "Computer's AI (VS Mode)"		},
	{0x16, 0x01, 0xc0, 0x40, "Low"							},
	{0x16, 0x01, 0xc0, 0xc0, "Normal"						},
	{0x16, 0x01, 0xc0, 0x80, "High"							},
	{0x16, 0x01, 0xc0, 0x00, "Highest"						},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x17, 0x01, 0x02, 0x02, "Off"							},
	{0x17, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    8, "Coin B"						},
	{0x17, 0x01, 0x1c, 0x00, "5 Coins 1 Credits"			},
	{0x17, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"			},
	{0x17, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"			},
	{0x17, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"			},
	{0x17, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"			},
	{0x17, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"			},
	{0x17, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"			},
	{0x17, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"			},

	{0   , 0xfe, 0   ,    8, "Coin A"						},
	{0x17, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"			},
	{0x17, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"			},
	{0x17, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"			},
	{0x17, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"			},
	{0x17, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"			},
	{0x17, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"			},
	{0x17, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"			},
	{0x17, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"			},
};

STDDIPINFO(Hayaosi3)

static struct BurnDIPInfo KirarastDIPList[]=
{
	{0x17, 0xff, 0xff, 0xfd, NULL							},
	{0x18, 0xff, 0xff, 0xff, NULL							},
	{0x19, 0xff, 0xff, 0xff, NULL							},
	{0x1a, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x17, 0x01, 0x01, 0x01, "Off"							},
	{0x17, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Campaign Mode"				},
	{0x17, 0x01, 0x04, 0x00, "Off"							},
	{0x17, 0x01, 0x04, 0x04, "On"							},

	{0   , 0xfe, 0   ,    2, "Tsumo Pinfu"					},
	{0x17, 0x01, 0x08, 0x00, "Off"							},
	{0x17, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x17, 0x01, 0x10, 0x00, "Off"							},
	{0x17, 0x01, 0x10, 0x10, "On"							},

	{0   , 0xfe, 0   ,    8, "Difficulty"					},
	{0x17, 0x01, 0xe0, 0x00, "Easiest"						},
	{0x17, 0x01, 0xe0, 0x80, "Very Easy"					},
	{0x17, 0x01, 0xe0, 0x40, "Easier"						},
	{0x17, 0x01, 0xe0, 0xc0, "Easy"							},
	{0x17, 0x01, 0xe0, 0xe0, "Normal"						},
	{0x17, 0x01, 0xe0, 0x60, "Hard"							},
	{0x17, 0x01, 0xe0, 0xa0, "Harder"						},
	{0x17, 0x01, 0xe0, 0x20, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x18, 0x01, 0x01, 0x01, "Off"							},
	{0x18, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x18, 0x01, 0x02, 0x02, "Off"							},
	{0x18, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    8, "Coinage"						},
	{0x18, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"			},
	{0x18, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"			},
	{0x18, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"			},
	{0x18, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"			},
	{0x18, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"			},
	{0x18, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"			},
	{0x18, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"			},
	{0x18, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"			},
};

STDDIPINFO(Kirarast)

static struct BurnDIPInfo Suchie2DIPList[]=
{
	{0x17, 0xff, 0xff, 0xff, NULL							},
	{0x18, 0xff, 0xff, 0xff, NULL							},
	{0x19, 0xff, 0xff, 0xff, NULL							},
	{0x1a, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x17, 0x01, 0xc0, 0x00, "Easy"							},
	{0x17, 0x01, 0xc0, 0xc0, "Normal"						},
	{0x17, 0x01, 0xc0, 0x40, "Hard"							},
	{0x17, 0x01, 0xc0, 0x80, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x18, 0x01, 0x01, 0x01, "Off"							},
	{0x18, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x18, 0x01, 0x02, 0x02, "Off"							},
	{0x18, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Campaign Mode"				},
	{0x18, 0x01, 0x04, 0x00, "Off"							},
	{0x18, 0x01, 0x04, 0x04, "On"							},

	{0   , 0xfe, 0   ,    2, "Tsumo Pinfu"					},
	{0x18, 0x01, 0x08, 0x00, "Off"							},
	{0x18, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x18, 0x01, 0x10, 0x00, "Off"							},
	{0x18, 0x01, 0x10, 0x10, "On"							},

	{0   , 0xfe, 0   ,    8, "Coinage"						},
	{0x18, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"			},
	{0x18, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"			},
	{0x18, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"			},
	{0x18, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"			},
	{0x18, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"			},
	{0x18, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"			},
	{0x18, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"			},
	{0x18, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"			},
};

STDDIPINFO(Suchie2)

static struct BurnDIPInfo Wpksocv2DIPList[]=
{
	DIP_OFFSET(0x0c)
	{0x00, 0xff, 0xff, 0xff, NULL							},
	{0x01, 0xff, 0xff, 0xff, NULL							},
	{0x02, 0xff, 0xff, 0x5f, NULL							},
	{0x03, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x01, 0x01, 0x10, 0x00, "Off"							},
	{0x01, 0x01, 0x10, 0x10, "On"							},

	{0   , 0xfe, 0   ,    8, "Coinage"						},
	{0x01, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"			},
	{0x01, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"			},
	{0x01, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"			},
	{0x01, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"			},
	{0x01, 0x01, 0xe0, 0x00, "2 Coins 3 Credits"			},
	{0x01, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"			},
	{0x01, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"			},
	{0x01, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"			},

	{0   , 0xfe, 0   ,    2, "Tickets"						},
	{0x02, 0x01, 0x20, 0x00, "Off"							},
	{0x02, 0x01, 0x20, 0x20, "On"							},

	{0   , 0xfe, 0   ,    3, "Region"						},
	{0x02, 0x01, 0xc0, 0x40, "USA"							},
	{0x02, 0x01, 0xc0, 0x00, "Asia"							},
	{0x02, 0x01, 0xc0, 0xc0, "Japan"						},
};

STDDIPINFO(Wpksocv2)

static INT32 irq_callback(INT32)
{
	INT32 i;
	for (i = 15; i >= 0 && !(v60_irq_vector & (1 << i)); i--);
	v60_irq_vector &= ~(1 << i);

	if (!v60_irq_vector) v60SetIRQLine(0, CPU_IRQSTATUS_NONE);

	return i;
}

static void irq_raise(INT32 level)
{
	v60_irq_vector |= 1 << level;

	v60SetIRQLine(0, CPU_IRQSTATUS_ACK);
}

static inline void sound_sync()
{
	INT32 cyc = (v60TotalCycles() * 8) / 20 - ZetTotalCycles();
	if (cyc > 0) {
		BurnTimerUpdate(ZetTotalCycles() + cyc);
	}
}

static void palette_write(INT32 i)
{
	UINT16 *ram = (UINT16*)DrvPalRAM;

	UINT8 r = BURN_ENDIAN_SWAP_INT16(ram[i * 4]) >> 8;
	UINT8 g = BURN_ENDIAN_SWAP_INT16(ram[i * 4]);
	UINT8 b = BURN_ENDIAN_SWAP_INT16(ram[i * 4 + 2]);

	if (i < 0x4000) {
		INT32 br = 0x100 - (bright[0] >> 8);
		INT32 bg = 0x100 - (bright[0] & 0xff);
		INT32 bb = 0x100 - (bright[2] & 0xff);

		r = (r * br) / 0x100;
		g = (g * bg) / 0x100;
		b = (b * bb) / 0x100;
	}

	DrvPalette[i] = BurnHighCol(r, g, b, 0);
	DrvPalette[i + 0x8000] = BurnHighCol(r/2, g/2, b/2, 0); // shadow
}

static void ms32_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffc0000) == 0xfd400000) {
		UINT16 *ram = (UINT16*)DrvPalRAM;
		ram[(address / 2) & 0x1ffff] = BURN_ENDIAN_SWAP_INT16(data);
		if ((address & 2) == 0) palette_write((address / 8) & 0x7fff);
		return;
	}

	if ((address & 0xffffff80) == 0xfce00000) {
		UINT16 *ram = (UINT16*)DrvSysCtrl;
		ram[(address / 4) & 0x1f] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if ((address & ~0x7f) == 0xfce00200) {
		UINT16 *ram = (UINT16*)DrvSprCtrl;
		ram[(address / 2) & 0x3f] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if ((address & ~0x7f) == 0xfce00600) {
		UINT16 *ram = (UINT16*)DrvRozCtrl;
		ram[(address / 2) & 0x3f] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if ((address & ~0x1f) == 0xfce00a00) {
		UINT16 *ram = (UINT16*)DrvTxCtrl;
		ram[(address / 2) & 0x0f] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if ((address & ~0x1f) == 0xfce00a20) {
		UINT16 *ram = (UINT16*)DrvBgCtrl;
		ram[(address / 2) & 0x0f] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if ((address & 0xffffe000) == 0xfe200000) {
		if ((address & 2) == 0) {
			UINT16 *ram = (UINT16*)DrvLineRAM;
			ram[(address / 4) & 0x7fff] = BURN_ENDIAN_SWAP_INT16(data);
		}
		return;
	}

	switch (address)
	{
		case 0xfc800000:
			sound_sync();
			soundlatch = data & 0xff;
			ZetNmi();
			BurnTimerUpdate(ZetTotalCycles() + 320);
		return;

		case 0xfc800002:
		return; // soundlatch high word (nop)

	// system ctrl ((address & 2) == 0)
		case 0xfce00000:
			flipscreen = data & 0x02;
		return;

		case 0xfce00034:
		return;

		case 0xfce00038:
			sound_sync();
			if (data & 1) ZetReset();
		return;

		case 0xfce0004c:
//			to_main = 0xff;
		return;

		case 0xfce00050: // watchdog
		case 0xfce00054:
		case 0xfce00058:
		case 0xfce0005c:
		return;
	// system ctrl

		case 0xfce00280:
		case 0xfce00282:
		case 0xfce00284:
		case 0xfce00286:
		case 0xfce00288:
		case 0xfce0028a:
		case 0xfce0028c:
		case 0xfce0028e:
			if (data != bright[(address / 2) & 7]) {
				bright[(address / 2) & 7] = data;
				DrvRecalc = 1;
			}
		return;

		case 0xfce00a7c:
			tilemaplayoutcontrol = data;
		return;

		case 0xfce00a7e:
		return; // high word of tilemap layout

		case 0xfce00e00:
		case 0xfce00e02:
			// coin counters / lockout
		return;

		case 0xfce00e04:
		case 0xfce00e06:
		case 0xfce00e08:
		case 0xfce00e0a:
		case 0xfce00e0c:
		case 0xfce00e0e:
		return;

		case 0xfd1c0000:
			mahjong_select = data;
		return;

		case 0xfd1c0002:
		return; // mahjong select high word
	}

	bprintf (0, _T("MWW: %8.8x, %4.4x\n"), address, data);
}

static void ms32_main_write_long(UINT32 address, UINT32 data)
{
	//bprintf(0, _T("WL: %x  %x\n"),address,data);
	ms32_main_write_word(address + 0, data);
	ms32_main_write_word(address + 2, data >> 16);
}

static void ms32_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffc0000) == 0xfd400000) {
		DrvPalRAM[address & 0x3ffff] = data;
		if ((address & 2) == 0) palette_write((address / 8) & 0x7fff);
		return;
	}

	if ((address & 0xfffffff0) == 0xfce00280) {
		UINT16 value = data << ((address & 1) * 8);
		UINT16 mask = 0xff << ((address & 1) * 8);
		INT32 offset = (address / 2) & 7;
		if (bright[offset] != ((bright[offset] & ~mask) |  (value & mask))) {
			bright[offset] = (bright[offset] & ~mask) | (value & mask);
			DrvRecalc = 1;
		}
		return;
	}

	if ((address & 0xffffff80) == 0xfce00600) {
		UINT16 value = data << ((address & 1) * 8);
		UINT16 mask = 0xff << ((address & 1) * 8);
		INT32 offset = (address / 2) & 7;
		UINT16 *ram = (UINT16*)DrvRozCtrl;
		ram[offset] = BURN_ENDIAN_SWAP_INT16((BURN_ENDIAN_SWAP_INT16(ram[offset]) & ~mask) | (value & mask));
		return;
	}

	switch (address)
	{
		case 0xfc800000:
			sound_sync();
			soundlatch = data;
			ZetNmi();
			BurnTimerUpdate(ZetTotalCycles() + 320);
		return;

		case 0xfce00000:
		return; // watchdog

		case 0xfce00038:
			sound_sync();
			if (data & 1) ZetReset();
		return;

		case 0xfce00e00:
		return; // coin counters

		case 0xfd1c0000:
			mahjong_select = data;
		return;
	}

	bprintf (0, _T("MWB: %8.8x, %2.2x\n"), address, data);
}

static UINT32 inputs_read()
{
	if (input_is_mahjong) {
		for (INT32 i = 0; i < 5; i++) {
			if ((1 << i) == mahjong_select) {
				return (DrvInputs[0] & 0xffffff00) | DrvMahjongInputs[i];
			}
		}
	}

	if (is_wpksocv2) {
		analog_target = ProcessAnalog(Analog[0], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0x0f);
		analog_clock++;
		if (analog_clock >= 8) {
			analog_clock = 0;
			if (analog_adder < analog_target) analog_adder++;
			if (analog_adder > analog_target) analog_adder--;

			if (analog_starttimer > 0) analog_starttimer--;
		}

		// Notes:
		// Bit 0: starts "ready to kick" (mapped to Select and Start)
		// Bit 0-7: ball impact counter
		// Need to see a video of the machine in-use to better understand Bit 0. -dink feb24 2021
		// I think Bit0 might be a sensor for the solenoid, to make sure the ball is fully
		// extended before starting the kick sequence.
		// TODO: find the solenoid register and make this nicer. -dink
		if (DrvJoy1[20] || DrvJoy1[21]) {
			analog_starttimer = 250; // bit0 needs to be held for "a little bit"
		}

		return (DrvInputs[0] & ~0xf) | analog_adder | ((analog_starttimer > 0) ? 1 : 0);
	}

	return DrvInputs[0];
}

static UINT16 ms32_main_read_word(UINT32 address)
{
	if ((address & 0xffffe000) == 0xfe200000) {
		UINT16 *ram = (UINT16*)DrvLineRAM;
		return BURN_ENDIAN_SWAP_INT16(ram[(address / 4) & 0x7fff]);
	}

	if ((address & 0xffffff80) == 0xfce00000) {
		UINT16 *ram = (UINT16*)DrvSysCtrl;
		return BURN_ENDIAN_SWAP_INT16(ram[(address / 4) & 0x1f]);
	}

	switch (address)
	{
		case 0xfc800000:
		case 0xfc800002:
			return 0xffff;

		case 0xfcc00004:
		case 0xfcc00006:
			return inputs_read() >> ((address & 2) * 8);

		case 0xfcc00008:  // nop?
			return 0xffff;

		case 0xfcc00010:
		case 0xfcc00012:
			return DrvDips[(address & 2) | 0] | (DrvDips[(address & 2) | 1] << 8);

		case 0xfd000000:
			sound_sync();
			v60_irq_vector &= ~(1 << 1);
			v60SetIRQLine(0, v60_irq_vector ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE); // tp2m32 needs this?
			return to_main ^ 0xff;

		case 0xfd000002:
			return 0; // sound read high word

	}

	bprintf (0, _T("MRW: %8.8x\n"), address);

	return 0xffff;
}

static UINT32 ms32_main_read_long(UINT32 address)
{
	return ms32_main_read_word(address) | (ms32_main_read_word(address + 2) << 16);
}

static UINT8 ms32_main_read_byte(UINT32 address)
{
	return ms32_main_read_word(address & ~1) >> ((address & 1) * 8);
}

static void bankswitch(INT32 data)
{
	z80_bank = data;

	ZetMapMemory(DrvZ80ROM + 0x4000 + (data & 0xf) * 0x4000, 0x8000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM + 0x4000 + (data >> 4)  * 0x4000, 0xc000, 0xffff, MAP_ROM);
}

static void __fastcall ms32_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x3f00) {
		BurnYMF271Write(address & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0x3f10:
			to_main = data;
			irq_raise(1);
		return;

		case 0x3f80:
			bankswitch(data);
		return;
	}
}

static UINT8 __fastcall ms32_sound_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x3f00) {
		return BurnYMF271Read(address & 0xf);
	}

	switch (address)
	{
		case 0x3f10:
			return soundlatch ^ 0xff;
	}

	return 0;
}

static tilemap_callback( tx )
{
	UINT16 *ram = (UINT16*)DrvTxRAM;

	TILE_SET_INFO(3, BURN_ENDIAN_SWAP_INT16(ram[offs * 4]), BURN_ENDIAN_SWAP_INT16(ram[offs * 4 + 2]), 0);
}

static tilemap_callback( roz )
{
	UINT16 *ram = (UINT16*)DrvRozRAM;

	TILE_SET_INFO(1, BURN_ENDIAN_SWAP_INT16(ram[offs * 4]), BURN_ENDIAN_SWAP_INT16(ram[offs * 4 + 2]), 0);
}

static tilemap_callback( bg )
{
	UINT16 *ram = (UINT16*)DrvBgRAM;

	TILE_SET_INFO(2, BURN_ENDIAN_SWAP_INT16(ram[offs * 4]), BURN_ENDIAN_SWAP_INT16(ram[offs * 4 + 2]), 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	if (is_p47acesa) memset(DrvNVRAM, 0xff, 0x8000);

	v60Open(0);
	v60_irq_vector = 0;
	v60Reset();
	v60SetIRQLine(0, CPU_IRQSTATUS_NONE);
	v60Close();

	ZetOpen(0);
	bankswitch(0x10);
	ZetReset();
	BurnYMF271Reset();
	ZetClose();

	flipscreen = 0;
	soundlatch = 0;
	to_main = 0;
	tilemaplayoutcontrol = 0;
	mahjong_select = 0;

	analog_target = 0;
	analog_adder = 0;
	analog_clock = 0;

	// mame says to do this
	memset (bright, 0xff, sizeof(bright));
	UINT32 *sprite_ctrl = (UINT32*)DrvSprCtrl;
	sprite_ctrl[0x10/4] = BURN_ENDIAN_SWAP_INT32(0x8000);

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvV60ROM		= Next; Next += 0x0200000;
	DrvZ80ROM		= Next; Next += 0x0040000;

	DrvGfxROM[0]	= Next; Next += graphics_length[0];
	DrvGfxROM[1]	= Next; Next += graphics_length[1];
	DrvGfxROM[2]	= Next; Next += graphics_length[2];
	DrvGfxROM[3]	= Next; Next += graphics_length[3];

	DrvSndROM		= Next; Next += 0x0400000;

	DrvPalette		= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x0008000;

	AllRam			= Next;

	DrvV60RAM		= Next; Next += 0x0020000;
	DrvZ80RAM		= Next; Next += 0x0004000;
	DrvPalRAM		= Next; Next += 0x0040000;
	DrvRozRAM		= Next; Next += 0x0020000;
	DrvRozBuf		= Next; Next += 0x0020000;
	DrvLineRAM		= Next; Next += 0x0002000;
	DrvTxRAM		= Next; Next += 0x0008000;
	DrvBgRAM		= Next; Next += 0x0008000;
	DrvSprRAM		= Next; Next += 0x0020000;
	DrvSprBuf		= Next; Next += 0x0020000;
	DrvPriRAM		= Next; Next += 0x0008000;
	DrvRozCtrl		= Next; Next += 0x0000100;
	DrvTxCtrl		= Next; Next += 0x0000100;
	DrvBgCtrl		= Next; Next += 0x0000100;
	DrvSysCtrl		= Next; Next += 0x0000100;
	DrvSprCtrl		= Next; Next += 0x0000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvROMLoad(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[7] = { DrvV60ROM, DrvGfxROM[0], DrvGfxROM[1], DrvGfxROM[2], DrvGfxROM[3], DrvZ80ROM, DrvSndROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (bLoad) {
				if (BurnLoadRom(pLoad[0] + 0x000003, i+0, 4)) return 1;
				if (BurnLoadRom(pLoad[0] + 0x000002, i+1, 4)) return 1;
				if (BurnLoadRom(pLoad[0] + 0x000001, i+2, 4)) return 1;
				if (BurnLoadRom(pLoad[0] + 0x000000, i+3, 4)) return 1;
			}
			pLoad[0] += ri.nLen * 4;
			i += 3;
			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (bLoad) {
				if (BurnLoadRomExt(pLoad[1] + 0x000000, i+0, 4, LD_GROUP(2))) return 1;
				if (BurnLoadRomExt(pLoad[1] + 0x000002, i+1, 4, LD_GROUP(2))) return 1;
			}
			pLoad[1] += ri.nLen * 2;
			i+=1;
			continue;
		}

		if ((ri.nType & 7) >= 3 && (ri.nType & 7) <= 7) {
			if (bLoad) {
				if (BurnLoadRom(pLoad[(ri.nType & 7) - 1], i, 1)) return 1;
			}
			pLoad[(ri.nType & 7) - 1] += ri.nLen;
			continue;
		}
	}

	if (!bLoad) {
		for (INT32 i = 0; i < 4; i++) {
			graphics_length[i] = pLoad[i+1] - DrvGfxROM[i];
	//		bprintf (0, _T("Gfx %d: %x\n"), i, graphics_length[i]);
		}
	}

	return 0;
}

static void ms32_rearrange_sprites(UINT8 *rom, INT32 length)
{
	UINT8 *dst = (UINT8*)BurnMalloc(length);

	for (INT32 i = 0; i < length; i++)
	{
		INT32 j = (i & ~0x07f8) | ((i & 0x00f8) << 3) | ((i & 0x0700) >> 5);

		dst[i] = rom[j];
	}

	memcpy (rom, dst, length);

	BurnFree(dst);
}

void decrypt_ms32_tx(UINT8 *rom, INT32 length, INT32 addr_xor, INT32 data_xor)
{
	UINT8 *dst = (UINT8*)BurnMalloc(length);

	addr_xor ^= 0x1005d;

	for(INT32 i = 0; i < length; i++)
	{
		INT32 j = 0;
		i ^= addr_xor;

		if (BIT(i,18)) j ^= 0x40000;    // 18
		if (BIT(i,17)) j ^= 0x60000;    // 17
		if (BIT(i, 7)) j ^= 0x70000;    // 16
		if (BIT(i, 3)) j ^= 0x78000;    // 15
		if (BIT(i,14)) j ^= 0x7c000;    // 14
		if (BIT(i,13)) j ^= 0x7e000;    // 13
		if (BIT(i, 0)) j ^= 0x7f000;    // 12
		if (BIT(i,11)) j ^= 0x7f800;    // 11
		if (BIT(i,10)) j ^= 0x7fc00;    // 10
		if (BIT(i, 9)) j ^= 0x00200;    //  9
		if (BIT(i, 8)) j ^= 0x00300;    //  8
		if (BIT(i,16)) j ^= 0x00380;    //  7
		if (BIT(i, 6)) j ^= 0x003c0;    //  6
		if (BIT(i,12)) j ^= 0x003e0;    //  5
		if (BIT(i, 4)) j ^= 0x003f0;    //  4
		if (BIT(i,15)) j ^= 0x003f8;    //  3
		if (BIT(i, 2)) j ^= 0x003fc;    //  2
		if (BIT(i, 1)) j ^= 0x003fe;    //  1
		if (BIT(i, 5)) j ^= 0x003ff;    //  0

		i ^= addr_xor;

		dst[i] = rom[j] ^ i ^ data_xor;
	}

	memcpy (rom, dst, length);

	BurnFree (dst);
}

void decrypt_ms32_bg(UINT8 *rom, INT32 length, INT32 addr_xor, INT32 data_xor)
{
	UINT8 *dst = (UINT8*)BurnMalloc(length);

	addr_xor ^= 0xc1c5b;

	for (INT32 i = 0; i < length; i++)
	{
		INT32 j = (i & ~0xfffff); // top bits are not affected
		i ^= addr_xor;

		if (BIT(i,19)) j ^= 0x80000;    // 19
		if (BIT(i, 8)) j ^= 0xc0000;    // 18
		if (BIT(i,17)) j ^= 0xe0000;    // 17
		if (BIT(i, 2)) j ^= 0xf0000;    // 16
		if (BIT(i,15)) j ^= 0xf8000;    // 15
		if (BIT(i,14)) j ^= 0xfc000;    // 14
		if (BIT(i,13)) j ^= 0xfe000;    // 13
		if (BIT(i,12)) j ^= 0xff000;    // 12
		if (BIT(i, 1)) j ^= 0xff800;    // 11
		if (BIT(i,10)) j ^= 0xffc00;    // 10
		if (BIT(i, 9)) j ^= 0x00200;    //  9
		if (BIT(i, 3)) j ^= 0x00300;    //  8
		if (BIT(i, 7)) j ^= 0x00380;    //  7
		if (BIT(i, 6)) j ^= 0x003c0;    //  6
		if (BIT(i, 5)) j ^= 0x003e0;    //  5
		if (BIT(i, 4)) j ^= 0x003f0;    //  4
		if (BIT(i,18)) j ^= 0x003f8;    //  3
		if (BIT(i,16)) j ^= 0x003fc;    //  2
		if (BIT(i,11)) j ^= 0x003fe;    //  1
		if (BIT(i, 0)) j ^= 0x003ff;    //  0

		i ^= addr_xor;

		dst[i] = rom[j] ^ i ^ data_xor;
	}

	memcpy (rom, dst, length);

	BurnFree (dst);
}

static INT32 CommonInit(UINT32 bg_addrxor, UINT32 bg_dataxor, UINT32 tx_addrxor, UINT32 tx_dataxor)
{
	DrvROMLoad(false);
	BurnAllocMemIndex();
	DrvROMLoad(true);

	ms32_rearrange_sprites(DrvGfxROM[0], graphics_length[0]);
	decrypt_ms32_bg(DrvGfxROM[2], graphics_length[2], bg_addrxor, bg_dataxor);
	decrypt_ms32_tx(DrvGfxROM[3], graphics_length[3], tx_addrxor, tx_dataxor);

	v70Init();
	v60Open(0);
	v60MapMemory(DrvV60RAM,		0xfee00000, 0xfee1ffff, MAP_RAM);
	v60MapMemory(DrvV60ROM,		0xffc00000, 0xffdfffff, MAP_ROM);
	v60MapMemory(DrvV60ROM,		0xffe00000, 0xffffffff, MAP_ROM);
	v60MapMemory(DrvNVRAM,		0xfc000000, 0xfc007fff, MAP_RAM);
	for (INT32 i = 0; i < 0x40000; i += 0x08000) {
		v60MapMemory(DrvPriRAM,		0xfd180000 + i, 0xfd187fff + i, MAP_RAM);
	}
	v60MapMemory(DrvPalRAM,		0xfd400000, 0xfd43ffff, MAP_ROM);
	for (INT32 i = 0; i < 0x100000; i += 0x20000) {
		v60MapMemory(DrvRozRAM,		0xfe000000 + i, 0xfe01ffff + i, MAP_RAM);
		v60MapMemory(DrvSprRAM,		0xfe800000 + i, 0xfe81ffff + i, MAP_RAM);
	}
	v60MapMemory(DrvRozRAM,		0xfe1e0000, 0xfe1fffff, MAP_RAM); // additional mirror... roz, right??
	v60MapMemory(DrvTxRAM,		0xfec00000, 0xfec07fff, MAP_RAM);
	v60MapMemory(DrvBgRAM,		0xfec08000, 0xfec0ffff, MAP_RAM);
	v60MapMemory(DrvTxRAM,		0xfec10000, 0xfec17fff, MAP_RAM);
	v60MapMemory(DrvBgRAM,		0xfec18000, 0xfec1ffff, MAP_RAM);
	v60SetWriteLongHandler(ms32_main_write_long);
	v60SetWriteWordHandler(ms32_main_write_word);
	v60SetWriteByteHandler(ms32_main_write_byte);
	v60SetReadLongHandler(ms32_main_read_long);
	v60SetReadWordHandler(ms32_main_read_word);
	v60SetReadByteHandler(ms32_main_read_byte);
	v60SetIRQCallback(irq_callback);
	v60Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x3eff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x4000, 0x7fff, MAP_RAM);
	ZetSetWriteHandler(ms32_sound_write);
	ZetSetReadHandler(ms32_sound_read);
	ZetClose();

	BurnYMF271Init(16934400, DrvSndROM, 0x400000, NULL, 0);
	BurnYMF271SetRoute(0, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYMF271SetRoute(1, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnYMF271SetRoute(2, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYMF271SetRoute(3, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachZet(8000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, tx_map_callback,   8,  8,  64,  64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg_map_callback,  16, 16,  64,  64);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, bg_map_callback,  16, 16, 256,  16);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, roz_map_callback, 16, 16, 128, 128);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 8, 256, 256, graphics_length[0], 0x0000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 8,  16,  16, graphics_length[1], 0x2000, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 8,  16,  16, graphics_length[2], 0x1000, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 8,   8,   8, graphics_length[3], 0x6000, 0xf);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
//	GenericTilemapSetTransparent(3, 0); // draw opaque, do transparency on roz_copy
	GenericTilemapBuildSkipTable(0, 3, 0);
	GenericTilemapBuildSkipTable(1, 2, 0);
	GenericTilemapBuildSkipTable(2, 2, 0);
	GenericTilemapUseDirtyTiles(3);

	BurnBitmapAllocate(1, nScreenWidth, nScreenHeight, false); // sprite bitmap
	BurnBitmapAllocate(2, 16 * 128, 16 * 128, true); // roz bitmap
	BurnBitmapAllocate(3, 256, 256, false);

	input_is_mahjong = (BurnDrvGetGenreFlags() == GBF_MAHJONG) ? 1 : 0;

	DrvDoReset();

	return 0;
}

static INT32 ss91022_10_init()
{
	return CommonInit(0x00000, 0xa3, 0x00000, 0x35);
}

static INT32 ss92046_01_init()
{
	return CommonInit(0x00001, 0x9b, 0x00020, 0x7e);
}

static INT32 ss92047_01_init()
{
	return CommonInit(0x24000, 0x55, 0x24000, 0x18);
}

static INT32 ss92048_01_init()
{
	return CommonInit(0x20400, 0xd4, 0x20400, 0xd6);
}

static INT32 DrvExit()
{
	GenericTilesExit();

	v60Exit();
	ZetExit();
	BurnYMF271Exit();

	BurnFreeMemIndex();

	is_wpksocv2 = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	INT32 br = 0x100 - (bright[0] >> 8);
	INT32 bg = 0x100 - (bright[0] & 0xff);
	INT32 bb = 0x100 - (bright[2] & 0xff);

	UINT16 *ram = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x8000; i++)
	{
		UINT8 r = BURN_ENDIAN_SWAP_INT16(ram[i * 4]) >> 8;
		UINT8 g = BURN_ENDIAN_SWAP_INT16(ram[i * 4]);
		UINT8 b = BURN_ENDIAN_SWAP_INT16(ram[i * 4 + 2]);

		if (i < 0x4000) {
			r = (r * br) / 0x100;
			g = (g * bg) / 0x100;
			b = (b * bb) / 0x100;
		}

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
		DrvPalette[i + 0x8000] = BurnHighCol(r/2, g/2, b/2, 0); // shadow
	}
}

static inline void copy_roz(INT32 sty, INT32 ey, UINT32 startx, UINT32 starty, INT32 incxx, INT32 incxy, INT32 incyx, INT32 incyy, INT32 wrap, INT32 priority)
{
	UINT16 *src = BurnBitmapGetBitmap(2);
	UINT16 *dst = pTransDraw + sty * nScreenWidth;
	UINT8  *pri = pPrioDraw + sty * nScreenWidth;

	startx += sty * incyx;
	starty += sty * incyy;

	for (INT32 sy = sty; sy < ey; sy++, startx+=incyx, starty+=incyy)
	{
		UINT32 cx = startx;
		UINT32 cy = starty;

		if (wrap)
		{
			for (INT32 x = 0; x < nScreenWidth; x++, cx+=incxx, cy+=incxy, dst++, pri++)
			{
				INT32 p = src[((cy >> 5) & 0x3ff800) + ((cx >> 16) & 0x7ff)]; // wrap

				if (p & 0xff) {
					*dst = p;
					*pri |= priority;
				}
			}
		}
		else // correct?
		{
			for (INT32 x = 0; x < nScreenWidth; x++, cx+=incxx, cy+=incxy, dst++, pri++)
			{
				UINT16 cxx = cx >> 16;
				UINT16 cyy = cy >> 16;
				if (cxx >= 0x800 || cyy >= 0x800) continue;

				INT32 p = src[((cy >> 5) & 0x3ff800) + ((cx >> 16) & 0x7ff)]; // wrap

				if (p & 0xff) {
					*dst = p;
					*pri |= priority;
				}
			}
		}
	}
}

static void draw_roz_layer(INT32 priority)
{
	// update tilemap
	{
		UINT16 *src = (UINT16*)DrvRozRAM;
		UINT16 *dst = (UINT16*)DrvRozBuf;

		for (INT32 i = 0; i < 128 * 128; i++) {
			if ((src[i * 4] != dst[i * 4]) || (src[i * 4 + 2] != dst[i * 4 + 2])) {
				dst[i * 4] = src[i * 4];
				dst[i * 4 + 2] = src[i * 4 + 2];
				GenericTilemapSetTileDirty(3, i);
			}
		}

		GenericTilemapDraw(3, 2, 0);
	}

	UINT16 *roz_ctrl = (UINT16*)DrvRozCtrl;
	UINT16 *lineram = (UINT16*)DrvLineRAM;

	if (BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x5c/2]) & 1)
	{
		for (INT32 y = 0; y < nScreenHeight; y++)
		{
			UINT16 *lineaddr = lineram + 8 * (y & 0xff);

			INT32 start2x = BURN_ENDIAN_SWAP_INT16(lineaddr[0x00/4]) | ((BURN_ENDIAN_SWAP_INT16(lineaddr[0x04/4]) & 3) << 16);
			INT32 start2y = BURN_ENDIAN_SWAP_INT16(lineaddr[0x08/4]) | ((BURN_ENDIAN_SWAP_INT16(lineaddr[0x0c/4]) & 3) << 16);
			INT32 incxx   = BURN_ENDIAN_SWAP_INT16(lineaddr[0x10/4]) | ((BURN_ENDIAN_SWAP_INT16(lineaddr[0x14/4]) & 1) << 16);
			INT32 incxy   = BURN_ENDIAN_SWAP_INT16(lineaddr[0x18/4]) | ((BURN_ENDIAN_SWAP_INT16(lineaddr[0x1c/4]) & 1) << 16);
			INT32 startx  = BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x00/2]) | ((BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x04/2]) & 3) << 16);
			INT32 starty  = BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x08/2]) | ((BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x0c/2]) & 3) << 16);
			INT32 offsx   = BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x30/2]);
			INT32 offsy   = BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x34/2]);

			offsx += (BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x38/2]) & 1) * 0x400;
			offsy += (BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x3c/2]) & 1) * 0x400;

			if (start2x & 0x20000) start2x |= ~0x3ffff;
			if (start2y & 0x20000) start2y |= ~0x3ffff;
			if (startx & 0x20000) startx |= ~0x3ffff;
			if (starty & 0x20000) starty |= ~0x3ffff;
			if (incxx & 0x10000) incxx |= ~0x1ffff;
			if (incxy & 0x10000) incxy |= ~0x1ffff;

			copy_roz(y, y+1, (start2x+startx+offsx)<<16, (start2y+starty+offsy)<<16, incxx<<8, incxy<<8, 0, 0, 1, priority);
		}
	}
	else    // "simple" mode
	{
		INT32 startx = BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x00/2]) | ((BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x04/2]) & 3) << 16);
		INT32 starty = BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x08/2]) | ((BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x0c/2]) & 3) << 16);
		INT32 incxx  = BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x10/2]) | ((BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x14/2]) & 1) << 16);
		INT32 incxy  = BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x18/2]) | ((BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x1c/2]) & 1) << 16);
		INT32 incyy  = BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x20/2]) | ((BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x24/2]) & 1) << 16);
		INT32 incyx  = BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x28/2]) | ((BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x2c/2]) & 1) << 16);
		INT32 offsx  = BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x30/2]);
		INT32 offsy  = BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x34/2]);

		offsx += (BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x38/2]) & 1) * 0x400;
		offsy += (BURN_ENDIAN_SWAP_INT16(roz_ctrl[0x3c/2]) & 1) * 0x400;

		if (startx & 0x20000) startx |= ~0x3ffff;
		if (starty & 0x20000) starty |= ~0x3ffff;
		if (incxx & 0x10000) incxx |= ~0x1ffff;
		if (incxy & 0x10000) incxy |= ~0x1ffff;
		if (incyy & 0x10000) incyy |= ~0x1ffff;
		if (incyx & 0x10000) incyx |= ~0x1ffff;

		copy_roz(0, nScreenHeight, (startx+offsx)<<16, (starty+offsy)<<16, incxx<<8, incxy<<8, incyx<<8, incyy<<8, 1, priority);
	}
}

static void predraw(INT32 code, INT32 tx, INT32 ty, INT32 width, INT32 height)
{
	GenericTilesGfx *gfx = &GenericGfxData[0];
	UINT8 *dst = (UINT8*)BurnBitmapGetBitmap(3);
	UINT8 *src = gfx->gfxbase + (code * 256 * 256) + ty * 256 + tx;

	for (INT32 y = 0; y < height; y++)
	{
		memcpy (dst + y * width, src + y * 256, width);
	}
}

static void draw_sprites(UINT16 *sprram_top, UINT32 sprram_size)
{
	GenericTilesGfx *gfx = &GenericGfxData[0];
	UINT8 *bitmap = (UINT8*)BurnBitmapGetBitmap(3);
	UINT16 *sprite_ctrl = (UINT16*)DrvSprCtrl;

	UINT16 *source = sprram_top;
	UINT16 *finish = sprram_top + (sprram_size - 0x10);

	INT32 reverseorder = (BURN_ENDIAN_SWAP_INT16(sprite_ctrl[0x10/2]) & 0x8000) >> 15;

	if (reverseorder)
	{
		source  = sprram_top + (sprram_size - 0x10);
		finish  = sprram_top;
	}

	for (;reverseorder ? (source>=finish) : (source<finish); reverseorder ? (source-=16) : (source+=16))
	{
		INT32 attr  = BURN_ENDIAN_SWAP_INT16(source[0]);
		if ((attr & 0x0004) == 0) continue;

		INT32 pri   = attr & 0x00f0;
		INT32 flipx = attr & 1;
		INT32 flipy = attr & 2;
		INT32 code  = BURN_ENDIAN_SWAP_INT16(source[2]);
		INT32 color = BURN_ENDIAN_SWAP_INT16(source[4]);
		INT32 tx    = code & 0xff;
		INT32 ty    = code >> 8;
		INT32 size  = BURN_ENDIAN_SWAP_INT16(source[6]);
		INT32 xsize = (size & 0xff) + 1;
		INT32 ysize = (size >> 8) + 1;
		INT32 sx    = (BURN_ENDIAN_SWAP_INT16(source[10]) & 0x3ff) - (BURN_ENDIAN_SWAP_INT16(source[10]) & 0x400);
		INT32 sy    = (BURN_ENDIAN_SWAP_INT16(source[8]) & 0x1ff) - (BURN_ENDIAN_SWAP_INT16(source[8]) & 0x200);
		INT32 xzoom = BURN_ENDIAN_SWAP_INT16(source[12]);
		INT32 yzoom = BURN_ENDIAN_SWAP_INT16(source[14]);
		code        = (color & 0x0fff);
		color       = color >> 12;

		if (!yzoom || !xzoom)
			continue;

		yzoom = 0x1000000 / yzoom;
		xzoom = 0x1000000 / xzoom;

		predraw(code % gfx->code_mask, tx, ty, xsize, ysize); // sprites start as 256x256 and are clipped, copy this and then apply zoom to it below
		RenderZoomedTile(BurnBitmapGetBitmap(1), bitmap, 0, (color | pri) << 8, 0, sx, sy, flipx, flipy, xsize, ysize, xzoom, yzoom);
	}
}

static void draw_layers()
{
	UINT32 *tx_scroll = (UINT32*)DrvTxCtrl;
	UINT32 *bg_scroll = (UINT32*)DrvBgCtrl;
	INT32 asc_pri = 0, scr_pri = 0, rot_pri = 0;

	((DrvPriRAM[0x2b00 * 2] == 0x34) ? asc_pri : rot_pri) += 1;
	((DrvPriRAM[0x2e00 * 2] == 0x34) ? asc_pri : scr_pri) += 1;
	if (DrvPriRAM[0x3a00 * 2] == 0x09) asc_pri = 3;
	else (((DrvPriRAM[0x3a00 * 2] & 0x30) == 0x00) ? scr_pri : rot_pri) += 1;

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? (TMAP_FLIPX|TMAP_FLIPY) : 0);

	GenericTilemapSetScrollX(0, BURN_ENDIAN_SWAP_INT32(tx_scroll[0x00/4]) + BURN_ENDIAN_SWAP_INT32(tx_scroll[0x08/4]) + 0x18);
	GenericTilemapSetScrollY(0, BURN_ENDIAN_SWAP_INT32(tx_scroll[0x0c/4]) + BURN_ENDIAN_SWAP_INT32(tx_scroll[0x14/4]));
	GenericTilemapSetScrollX(1 + (tilemaplayoutcontrol & 1), BURN_ENDIAN_SWAP_INT32(bg_scroll[0x00/4]) + BURN_ENDIAN_SWAP_INT32(bg_scroll[0x08/4]) + 0x10);
	GenericTilemapSetScrollY(1 + (tilemaplayoutcontrol & 1), BURN_ENDIAN_SWAP_INT32(bg_scroll[0x0c/4]) + BURN_ENDIAN_SWAP_INT32(bg_scroll[0x14/4]));

	BurnBitmapPrimapClear(0);
	BurnBitmapFill(0, 0);
	BurnBitmapFill(1, 0);

	draw_sprites((UINT16*)DrvSprBuf, 0x20000/2);

	for (INT32 i = 0; i < 3; i++)
	{
		if (rot_pri == i)
		{
			if (nBurnLayer & 4) draw_roz_layer(2);
		}
		else if (scr_pri == i)
		{
			if (nBurnLayer & 2) GenericTilemapDraw(1 + (tilemaplayoutcontrol & 1), 0, 1);
		}
		else if (asc_pri == i)
		{
			if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 4);
		}
	}
}

static void mixer_draw()
{
	UINT8 pri_masks[16];

	// build priority mask table
	{
		for (INT32 spritepri = 0; spritepri < 0x10; spritepri++) {
			UINT32 primask = 0;
			UINT8 *priram  = DrvPriRAM + (((spritepri * 16) | 0xa00) * 2);

			if (priram[0x2a00] & 0x38) primask |= 1 << 0;
			if (priram[0x2800] & 0x38) primask |= 1 << 1;
			if (priram[0x2200] & 0x38) primask |= 1 << 2;
			if (priram[0x2000] & 0x38) primask |= 1 << 3;
			if (priram[0x0a00] & 0x38) primask |= 1 << 4;
			if (priram[0x0800] & 0x38) primask |= 1 << 5;
			if (priram[0x0200] & 0x38) primask |= 1 << 6;
			if (priram[0x0000] & 0x38) primask |= 1 << 7;

			pri_masks[spritepri] = primask;
		}
	}

	for (INT32 yy = 0; yy < nScreenHeight; yy++)
	{
		UINT8 *srcptr_tilepri = BurnBitmapGetPrimapPosition(0, 0, yy);
		UINT16 *srcptr_spri =   BurnBitmapGetPosition(1, 0, yy);
		UINT16 *dstptr_bitmap = BurnBitmapGetPosition(0, 0, yy);

		for (INT32 xx = 0; xx < nScreenWidth; xx++)
		{
			UINT8  src_tilepri	= srcptr_tilepri[xx];
			UINT16 src_spri		= srcptr_spri[xx];
			UINT8  primask  	= pri_masks[src_spri >> 12];

			if (primask == 0)
			{
				if (src_spri & 0xff) dstptr_bitmap[xx] = src_spri & 0x0fff;
			}
			else if (primask == 0xf0)
			{
				if ((src_tilepri & 0x04) == 0)
				{
					if (src_spri & 0xff) dstptr_bitmap[xx] = src_spri & 0x0fff;
				}
			}
			else if (primask == 0xfc)
			{
				if ((src_tilepri & 0x06) == 0)
				{
					if (src_spri & 0xff) dstptr_bitmap[xx] = src_spri & 0x0fff;
				}
			}
			else if (primask == 0xfe)
			{
				if (src_tilepri == 0)
				{
					if (src_spri & 0xff) dstptr_bitmap[xx] = src_spri & 0x0fff;
				}
				else if ((src_tilepri & 0x04) == 0)
				{
					dstptr_bitmap[xx] |= 0x8000; // shadow
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	draw_layers();

	mixer_draw();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xffffffff;

		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

		{
			memset (DrvMahjongInputs, 0xff, sizeof(DrvMahjongInputs));

			for (INT32 i = 0; i < 8; i++) {
				DrvMahjongInputs[0] ^= (DrvMah1[i] & 1) << i;
				DrvMahjongInputs[1] ^= (DrvMah2[i] & 1) << i;
				DrvMahjongInputs[2] ^= (DrvMah3[i] & 1) << i;
				DrvMahjongInputs[3] ^= (DrvMah4[i] & 1) << i;
				DrvMahjongInputs[4] ^= (DrvMah5[i] & 1) << i;
			}
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { 20000000 / 60, 8000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	v60NewFrame();
	ZetNewFrame();

	v60Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, v60);
		CPU_RUN_TIMER(1);

		if (i == 0) irq_raise(10);
		if (i == 8) irq_raise(9);
		if ((i & 7) == 0 && i <= 224) irq_raise(0);
		if (i == 223) memcpy (DrvSprBuf, DrvSprRAM, 0x20000);
	}

	if (pBurnSoundOut) {
		BurnYMF271Update(nBurnSoundLen);
	}

	ZetClose();
	v60Close();

	if (pBurnDraw) {
		BurnDrvRedraw();
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
		v60Scan(nAction);
		ZetScan(nAction);

		BurnYMF271Scan(nAction, pnMin);

		SCAN_VAR(z80_bank);
		SCAN_VAR(bright);
		SCAN_VAR(v60_irq_vector);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(to_main);
		SCAN_VAR(tilemaplayoutcontrol);
		SCAN_VAR(mahjong_select);

		SCAN_VAR(analog_target);
		SCAN_VAR(analog_adder);
		SCAN_VAR(analog_clock);
		SCAN_VAR(analog_starttimer);
	}

	if (nAction & ACB_NVRAM && !is_p47acesa) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x008000;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(z80_bank);
		ZetClose();

		GenericTilemapAllTilesDirty(3); // redraw roz layer
	}

	return 0;
}


// Tetris Plus (ver 1.0)

static struct BurnRomInfo tetrispRomDesc[] = {
	{ "mb93166_ver1.0-26.26",				0x080000, 0xd318a9ba, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mb93166_ver1.0-27.27",				0x080000, 0x2d69b6d3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mb93166_ver1.0-28.28",				0x080000, 0x87522e16, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mb93166_ver1.0-29.29",				0x080000, 0x43a61941, 1 | BRF_PRG | BRF_ESS }, //  3

	// swapped order from mame!
	{ "mr95024-02.13",						0x200000, 0x4a825990, 2 | BRF_GRA },           //  4 Sprites
	{ "mr95024-01.01",						0x200000, 0xcb0e92b9, 2 | BRF_GRA },           //  5

	{ "mr95024-04.11",						0x200000, 0xc0d5246f, 3 | BRF_GRA },           //  6 ROZ Tiles

	{ "mr95024-03.10",						0x200000, 0xa03e4a8d, 4 | BRF_GRA },           //  7 Background Tiles

	{ "mb93166_ver1.0-30.30",				0x080000, 0xcea7002d, 5 | BRF_GRA },           //  8 Characters

	{ "mb93166_ver1.0-21.21",				0x040000, 0x5c565e3b, 6 | BRF_PRG | BRF_ESS }, //  9 Z80 Code

	{ "mr92042-01.22",						0x200000, 0x0fa26f65, 7 | BRF_SND },           // 10 YMF271 Samples
	{ "mr95024-05.23",						0x200000, 0x57502a17, 7 | BRF_SND },           // 11

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 12 Motherboard PAL
};

STD_ROM_PICK(tetrisp)
STD_ROM_FN(tetrisp)

struct BurnDriver BurnDrvTetrisp = {
	"tetrisp", NULL, NULL, NULL, "1995",
	"Tetris Plus (ver 1.0)\0", NULL, "Jaleco / BPS", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, tetrispRomInfo, tetrispRomName, NULL, NULL, NULL, NULL, MS32InputInfo, TetrispDIPInfo,
	ss92046_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// P-47 Aces (ver 1.1)

static struct BurnRomInfo p47acesRomDesc[] = {
	{ "p-47_aces_3-31_rom_26_ver1.1.26",	0x080000, 0x99c0e211, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "p-47_aces_3-31_rom_27_ver1.1.27",	0x080000, 0x2a0c107a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p-47_aces_3-31_rom_28_ver1.1.28",	0x080000, 0x53509d28, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p-47_aces_3-31_rom_29_ver1.1.29",	0x080000, 0x91e7b7da, 1 | BRF_PRG | BRF_ESS }, //  3

	// swapped order from mame!
	{ "mr94020-01.13",						0x200000, 0xa6ccf999, 2 | BRF_GRA },           //  4 Sprites
	{ "mr94020-02.1",						0x200000, 0x28732d3c, 2 | BRF_GRA },           //  5
	{ "mr94020-03.14",						0x200000, 0xefc52b38, 2 | BRF_GRA },           //  6
	{ "mr94020-04.2",						0x200000, 0x128db576, 2 | BRF_GRA },           //  7
	{ "mr94020-05.15",						0x200000, 0xca164b17, 2 | BRF_GRA },           //  8
	{ "mr94020-06.3",						0x200000, 0x324cd504, 2 | BRF_GRA },           //  9
	{ "mr94020-07.16",						0x100000, 0xc23c5467, 2 | BRF_GRA },           // 10
	{ "mr94020-08.4",						0x100000, 0x4b3372be, 2 | BRF_GRA },           // 11

	{ "mr94020-11.11",						0x200000, 0xc1fe16b3, 3 | BRF_GRA },           // 12 ROZ Tiles
	{ "mr94020-12.12",						0x200000, 0x75871325, 3 | BRF_GRA },           // 13

	{ "mr94020-10.10",						0x200000, 0xa44e9e06, 4 | BRF_GRA },           // 14 Background Tiles
	{ "mr94020-09.9",						0x200000, 0x226014a6, 4 | BRF_GRA },           // 15

	{ "p-47_ver1.0-30.30",					0x080000, 0x7ba90fad, 5 | BRF_GRA },           // 16 Characters

	{ "p-47_ver1.0-21.21",					0x040000, 0xf2d43927, 6 | BRF_PRG | BRF_ESS }, // 17 Z80 Code

	{ "mr92042-01.22",						0x200000, 0x0fa26f65, 7 | BRF_SND },           // 18 YMF271 Samples
	{ "mr94020-13.23",						0x200000, 0x547fa4d4, 7 | BRF_SND },           // 19

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 20 Motherboard PAL
};

STD_ROM_PICK(p47aces)
STD_ROM_FN(p47aces)

struct BurnDriver BurnDrvP47aces = {
	"p47aces", NULL, NULL, NULL, "1995",
	"P-47 Aces (ver 1.1)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, p47acesRomInfo, p47acesRomName, NULL, NULL, NULL, NULL, MS32InputInfo, P47acesDIPInfo,
	ss92048_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// P-47 Aces (ver 1.0)

static struct BurnRomInfo p47acesaRomDesc[] = {
	{ "p47_ver1.0-26.26",					0x080000, 0xe017b819, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "p47_ver1.0-27.27",					0x080000, 0xbd1b81e0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p47_ver1.0-28.28",					0x080000, 0x4742a5f7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p47_ver1.0-29.29",					0x080000, 0x86e17d8b, 1 | BRF_PRG | BRF_ESS }, //  3

	// swapped order from mame!
	{ "mr94020-01.13",						0x200000, 0xa6ccf999, 2 | BRF_GRA },           //  4 Sprites
	{ "mr94020-02.1",						0x200000, 0x28732d3c, 2 | BRF_GRA },           //  5
	{ "mr94020-03.14",						0x200000, 0xefc52b38, 2 | BRF_GRA },           //  6
	{ "mr94020-04.2",						0x200000, 0x128db576, 2 | BRF_GRA },           //  7
	{ "mr94020-05.15",						0x200000, 0xca164b17, 2 | BRF_GRA },           //  8
	{ "mr94020-06.3",						0x200000, 0x324cd504, 2 | BRF_GRA },           //  9
	{ "mr94020-07.16",						0x100000, 0xc23c5467, 2 | BRF_GRA },           // 10
	{ "mr94020-08.4",						0x100000, 0x4b3372be, 2 | BRF_GRA },           // 11

	{ "mr94020-11.11",						0x200000, 0xc1fe16b3, 3 | BRF_GRA },           // 12 ROZ Tiles
	{ "mr94020-12.12",						0x200000, 0x75871325, 3 | BRF_GRA },           // 13

	{ "mr94020-10.10",						0x200000, 0xa44e9e06, 4 | BRF_GRA },           // 14 Background Tiles
	{ "mr94020-09.9",						0x200000, 0x226014a6, 4 | BRF_GRA },           // 15

	{ "p-47_ver1.0-30.30",					0x080000, 0x7ba90fad, 5 | BRF_GRA },           // 16 Characters

	{ "p-47_ver1.0-21.21",					0x040000, 0xf2d43927, 6 | BRF_PRG | BRF_ESS }, // 17 Z80 Code

	{ "mr92042-01.22",						0x200000, 0x0fa26f65, 7 | BRF_SND },           // 18 YMF271 Samples
	{ "mr94020-13.23",						0x200000, 0x547fa4d4, 7 | BRF_SND },           // 19

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 20 Motherboard PAL
};

STD_ROM_PICK(p47acesa)
STD_ROM_FN(p47acesa)

static INT32 p47acesaInit()
{
	is_p47acesa = 1;

	return ss92048_01_init();
}

struct BurnDriver BurnDrvP47acesa = {
	"p47acesa", "p47aces", NULL, NULL, "1995",
	"P-47 Aces (ver 1.0)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, p47acesaRomInfo, p47acesaRomName, NULL, NULL, NULL, NULL, MS32InputInfo, P47acesDIPInfo,
	p47acesaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Best Bout Boxing (ver 1.3)

static struct BurnRomInfo bbbxingRomDesc[] = {
	{ "mb93138a_25_ver1.3.25",				0x080000, 0xb526b41e, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mb93138a_27_ver1.3.27",				0x080000, 0x45b27ad8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mb93138a_29_ver1.3.29",				0x080000, 0x85bbbe79, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mb93138a_31_ver1.3.31",				0x080000, 0xe0c865ed, 1 | BRF_PRG | BRF_ESS }, //  3

	// swapped order from mame!
	{ "mr92042-06.13",						0x200000, 0x4b8c1574, 2 | BRF_GRA },           //  4 Sprites
	{ "mr92042-07.1",						0x200000, 0xc1c10c3b, 2 | BRF_GRA },           //  5
	{ "mr92042-08.14",						0x200000, 0xe9cfd83b, 2 | BRF_GRA },           //  6
	{ "mr92042-09.2",						0x200000, 0x03b77c1e, 2 | BRF_GRA },           //  7
	{ "mr92042-10.15",						0x200000, 0x6ab64a10, 2 | BRF_GRA },           //  8
	{ "mr92042-11.3",						0x200000, 0xbba0d1a4, 2 | BRF_GRA },           //  9
	{ "mr92042-12.16",						0x200000, 0xe001d6cb, 2 | BRF_GRA },           // 10
	{ "mr92042-13.4",						0x200000, 0x97f97e3a, 2 | BRF_GRA },           // 11
	{ "mb93138a_17_ver1.0.17",				0x080000, 0x1d7ebaf0, 2 | BRF_GRA },           // 12
	{ "mb93138a_5_ver1.0.5",				0x080000, 0x64989edf, 2 | BRF_GRA },           // 13

	{ "mr92042-05.9",						0x200000, 0xa41cb650, 3 | BRF_GRA },           // 14 ROZ Tiles

	{ "mr92042-04.11",						0x200000, 0x85238ca9, 4 | BRF_GRA },           // 15 Background Tiles

	{ "mr92042-01.32",						0x080000, 0x3ffdae75, 5 | BRF_GRA },           // 16 Characters

	{ "mb93138a_21_ver1.0.21",				0x040000, 0x5f3ea01f, 6 | BRF_PRG | BRF_ESS }, // 17 Z80 Code

	{ "mr92042-01.22",						0x200000, 0x0fa26f65, 7 | BRF_SND },           // 18 YMF271 Samples
	{ "mr92042-02.23",						0x200000, 0xb7875a23, 7 | BRF_SND },           // 19

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 20 Motherboard PAL
};

STD_ROM_PICK(bbbxing)
STD_ROM_FN(bbbxing)

struct BurnDriver BurnDrvBbbxing = {
	"bbbxing", NULL, NULL, NULL, "1994",
	"Best Bout Boxing (ver 1.3)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, bbbxingRomInfo, bbbxingRomName, NULL, NULL, NULL, NULL, MS32InputInfo, BbbxingDIPInfo,
	ss92046_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Desert War / Wangan Sensou (ver 1.0)

static struct BurnRomInfo desertwrRomDesc[] = {
	{ "mb93166_ver1.0-26.26",				0x080000, 0x582b9584, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mb93166_ver1.0-27.27",				0x080000, 0xcb60dda3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mb93166_ver1.0-28.28",				0x080000, 0x0de40efb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mb93166_ver1.0-29.29",				0x080000, 0xfc25eae2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mb94038-01.13",						0x200000, 0xf11f83e2, 2 | BRF_GRA },           //  4 Sprites
	{ "mb94038-02.1",						0x200000, 0x3d1fa710, 2 | BRF_GRA },           //  5
	{ "mb94038-03.14",						0x200000, 0x84fd5790, 2 | BRF_GRA },           //  6
	{ "mb94038-04.2",						0x200000, 0xb9ef5b78, 2 | BRF_GRA },           //  7
	{ "mb94038-05.15",						0x200000, 0xfeee1b8d, 2 | BRF_GRA },           //  8
	{ "mb94038-06.3",						0x200000, 0xd417f289, 2 | BRF_GRA },           //  9
	{ "mb94038-07.16",						0x200000, 0x426f4193, 2 | BRF_GRA },           // 10
	{ "mb94038-08.4",						0x200000, 0xf4088399, 2 | BRF_GRA },           // 11

	{ "mb94038-11.11",						0x200000, 0xbf2ec3a3, 3 | BRF_GRA },           // 12 ROZ Tiles
	{ "mb94038-12.12",						0x200000, 0xd0e113da, 3 | BRF_GRA },           // 13

	{ "mb94038-09.10",						0x200000, 0x72ec1ce7, 4 | BRF_GRA },           // 14 Background Tiles
	{ "mb94038-10.9",						0x200000, 0x1e17f2a9, 4 | BRF_GRA },           // 15

	{ "mb93166_ver1.0-29.30",				0x080000, 0x980ab89c, 5 | BRF_GRA },           // 16 Characters

	{ "mb93166_ver1.0-21.21",				0x040000, 0x9300be4c, 6 | BRF_PRG | BRF_ESS }, // 17 Z80 Code

	{ "mb92042-01.22",						0x200000, 0x0fa26f65, 7 | BRF_SND },           // 18 YMF271 Samples
	{ "mb94038-13.23",						0x200000, 0xb0cac8f2, 7 | BRF_SND },           // 19

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 20 Motherboard PAL
};

STD_ROM_PICK(desertwr)
STD_ROM_FN(desertwr)

struct BurnDriver BurnDrvDesertwr = {
	"desertwr", NULL, NULL, NULL, "1995",
	"Desert War / Wangan Sensou (ver 1.0)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, desertwrRomInfo, desertwrRomName, NULL, NULL, NULL, NULL, MS32InputInfo, DesertwrDIPInfo,
	ss91022_10_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	224, 320, 3, 4
};


// The Game Paradise - Master of Shooting! / Game Tengoku - The Game Paradise (ver 1.0)

static struct BurnRomInfo gametngkRomDesc[] = {
	{ "mb94166_ver1.0-26.26",				0x080000, 0xe622e774, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mb94166_ver1.0-27.27",				0x080000, 0xda862b9c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mb94166_ver1.0-28.28",				0x080000, 0xb3738934, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mb94166_ver1.0-29.29",				0x080000, 0x45154a45, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr94041-01.13",						0x200000, 0x3f99adf7, 2 | BRF_GRA },           //  4 Sprites
	{ "mr94041-02.1",						0x200000, 0xc3c5ae69, 2 | BRF_GRA },           //  5
	{ "mr94041-03.14",						0x200000, 0xd858b6de, 2 | BRF_GRA },           //  6
	{ "mr94041-04.2",						0x200000, 0x8c96ca20, 2 | BRF_GRA },           //  7
	{ "mr94041-05.15",						0x200000, 0xac664a0b, 2 | BRF_GRA },           //  8
	{ "mr94041-06.3",						0x200000, 0x70dd0dd4, 2 | BRF_GRA },           //  9
	{ "mr94041-07.16",						0x200000, 0xa6966af5, 2 | BRF_GRA },           // 10
	{ "mr94041-08.4",						0x200000, 0xd7d2f73a, 2 | BRF_GRA },           // 11

	{ "mr94041-11.11",						0x200000, 0x00dcdbc3, 3 | BRF_GRA },           // 12 ROZ Tiles
	{ "mr94041-12.12",						0x200000, 0x0ce48329, 3 | BRF_GRA },           // 13

	{ "mr94041-09.10",						0x200000, 0xa33e6051, 4 | BRF_GRA },           // 14 Background Tiles
	{ "mr94041-10.9",						0x200000, 0xb3497147, 4 | BRF_GRA },           // 15

	{ "mb94166_ver1.0-30.30",				0x080000, 0xc0f27b7f, 5 | BRF_GRA },           // 16 Characters

	{ "mb94166_ver1.0-21.21",				0x040000, 0x38dcb837, 6 | BRF_PRG | BRF_ESS }, // 17 Z80 Code

	{ "mr94041-13.22",						0x200000, 0xfba84caf, 7 | BRF_SND },           // 18 YMF271 Samples
	{ "mr94041-14.23",						0x200000, 0x2d6308bd, 7 | BRF_SND },           // 19

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 20 Motherboard PAL
};

STD_ROM_PICK(gametngk)
STD_ROM_FN(gametngk)

struct BurnDriver BurnDrvGametngk = {
	"gametngk", NULL, NULL, NULL, "1995",
	"The Game Paradise - Master of Shooting! / Game Tengoku - The Game Paradise (ver 1.0)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, gametngkRomInfo, gametngkRomName, NULL, NULL, NULL, NULL, MS32InputInfo, GametngkDIPInfo,
	ss91022_10_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	224, 320, 3, 4
};


// Gratia - Second Earth (ver 1.0, 92047-01 version)

static struct BurnRomInfo gratiaRomDesc[] = {
	{ "94019_26_ver1.0.26",					0x080000, 0xf398cba5, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "94019_27_ver1.0.27",					0x080000, 0xba3318c5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "94019_28_ver1.0.28",					0x080000, 0xe0762e89, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "94019_29_ver1.0.29",					0x080000, 0x8059800b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr94019-01.13",						0x200000, 0x92d8ae9b, 2 | BRF_GRA },           //  4 Sprites
	{ "mr94019-02.1",						0x200000, 0xf7bd9cc4, 2 | BRF_GRA },           //  5
	{ "mr94019-03.14",						0x200000, 0x62a69590, 2 | BRF_GRA },           //  6
	{ "mr94019-04.2",						0x200000, 0x5a76a39b, 2 | BRF_GRA },           //  7
	{ "mr94019-05.15",						0x200000, 0xa16994df, 2 | BRF_GRA },           //  8
	{ "mr94019-06.3",						0x200000, 0x01d52ef1, 2 | BRF_GRA },           //  9

	{ "mr94019-08.12",						0x200000, 0xabd124e0, 3 | BRF_GRA },           // 10 ROZ Tiles
	{ "mr94019-09.11",						0x200000, 0x711ab08b, 3 | BRF_GRA },           // 11

	{ "94019_2.07",							0x200000, 0x043f969b, 4 | BRF_GRA },           // 12 Background Tiles

	{ "94019_2.030",						0x080000, 0xf9543fcf, 5 | BRF_GRA },           // 13 Characters

	{ "94019_21ver1.0.21",					0x040000, 0x6e8dd039, 6 | BRF_PRG | BRF_ESS }, // 14 Z80 Code

	{ "mr92042-01.22",						0x200000, 0x0fa26f65, 7 | BRF_SND },           // 15 YMF271 Samples
	{ "mr94019-10.23",						0x200000, 0xa751e316, 7 | BRF_SND },           // 16

	{ "91022-01.ic83",						0x000001, 0x00000000, 8 | BRF_NODUMP | BRF_GRA },           // 17 Motherboard PAL
};

STD_ROM_PICK(gratia)
STD_ROM_FN(gratia)

struct BurnDriver BurnDrvGratia = {
	"gratia", NULL, NULL, NULL, "1996",
	"Gratia - Second Earth (ver 1.0, 92047-01 version)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, gratiaRomInfo, gratiaRomName, NULL, NULL, NULL, NULL, MS32InputInfo, GratiaDIPInfo,
	ss92047_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Gratia - Second Earth (ver 1.0, 91022-10 version)

static struct BurnRomInfo gratiaaRomDesc[] = {
	{ "94019_26_ver1.0.26",					0x080000, 0xf398cba5, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "94019_27_ver1.0.27",					0x080000, 0xba3318c5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "94019_28_ver1.0.28",					0x080000, 0xe0762e89, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "94019_29_ver1.0.29",					0x080000, 0x8059800b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr94019-01.13",						0x200000, 0x92d8ae9b, 2 | BRF_GRA },           //  4 Sprites
	{ "mr94019-02.1",						0x200000, 0xf7bd9cc4, 2 | BRF_GRA },           //  5
	{ "mr94019-03.14",						0x200000, 0x62a69590, 2 | BRF_GRA },           //  6
	{ "mr94019-04.2",						0x200000, 0x5a76a39b, 2 | BRF_GRA },           //  7
	{ "mr94019-05.15",						0x200000, 0xa16994df, 2 | BRF_GRA },           //  8
	{ "mr94019-06.3",						0x200000, 0x01d52ef1, 2 | BRF_GRA },           //  9

	{ "mr94019-08.12",						0x200000, 0xabd124e0, 3 | BRF_GRA },           // 10 ROZ Tiles
	{ "mr94019-09.11",						0x200000, 0x711ab08b, 3 | BRF_GRA },           // 11

	{ "mr94019-07.10",						0x200000, 0x561a786b, 4 | BRF_GRA },           // 12 Background Tiles

	{ "94019_30ver1.0.30",					0x080000, 0x026b5379, 5 | BRF_GRA },           // 13 Characters

	{ "94019_21ver1.0.21",					0x040000, 0x6e8dd039, 6 | BRF_PRG | BRF_ESS }, // 14 Z80 Code

	{ "mr92042-01.22",						0x200000, 0x0fa26f65, 7 | BRF_SND },           // 15 YMF271 Samples
	{ "mr94019-10.23",						0x200000, 0xa751e316, 7 | BRF_SND },           // 16

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 17 Motherboard PAL
};

STD_ROM_PICK(gratiaa)
STD_ROM_FN(gratiaa)

struct BurnDriver BurnDrvGratiaa = {
	"gratiaa", "gratia", NULL, NULL, "1996",
	"Gratia - Second Earth (ver 1.0, 91022-10 version)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, gratiaaRomInfo, gratiaaRomName, NULL, NULL, NULL, NULL, MS32InputInfo, GratiaDIPInfo,
	ss91022_10_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Tetris Plus 2 (ver 1.0, MegaSystem 32 Version)

static struct BurnRomInfo tp2m32RomDesc[] = {
	{ "mp2_26.ver1.0.26",					0x080000, 0x152f0ccf, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mp2_27.ver1.0.27",					0x080000, 0xd89468d0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mp2_28.ver1.0.28",					0x080000, 0x041aac23, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mp2_29.ver1.0.29",					0x080000, 0x4e83b2ca, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr96019-01.13",						0x400000, 0x06f7dc64, 2 | BRF_GRA },           //  4 Sprites
	{ "mr96019-02.1",						0x400000, 0x3e613bed, 2 | BRF_GRA },           //  5

	{ "mr96019-04.11",						0x200000, 0xb5a03129, 3 | BRF_GRA },           //  6 ROZ Tiles

	{ "mr96019-03.10",						0x400000, 0x94af8057, 4 | BRF_GRA },           //  7 Background Tiles

	{ "mp2_30.ver1.0.30",					0x080000, 0x6845e476, 5 | BRF_GRA },           //  8 Characters

	{ "mp2_21.ver1.0.21",					0x040000, 0x2bcc4176, 6 | BRF_PRG | BRF_ESS }, //  9 Z80 Code

	{ "mr96019-05.22",						0x200000, 0x74aa5c31, 7 | BRF_SND },           // 10 YMF271 Samples

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 11 Motherboard PAL
};

STD_ROM_PICK(tp2m32)
STD_ROM_FN(tp2m32)

struct BurnDriver BurnDrvTp2m32 = {
	"tp2m32", "tetrisp2", NULL, NULL, "1997",
	"Tetris Plus 2 (ver 1.0, MegaSystem 32 Version)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, tp2m32RomInfo, tp2m32RomName, NULL, NULL, NULL, NULL, MS32InputInfo, Tp2m32DIPInfo,
	ss91022_10_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// World PK Soccer V2 (ver 1.1)

static struct BurnRomInfo wpksocv2RomDesc[] = {
	{ "pk_soccer_v2_rom_25_ver._1.1.25",	0x080000, 0x6c22a56c, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "pk_soccer_v2_rom_27_ver._1.1.27",	0x080000, 0x50c594a8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pk_soccer_v2_rom_29_ver._1.1.29",	0x080000, 0x22acd835, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pk_soccer_v2_rom_31_ver._1.1.31",	0x080000, 0xf25e50f5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr95033-01.13",						0x200000, 0x1f76ed57, 2 | BRF_GRA },           //  4 Sprites
	{ "mr95033-02.1",						0x200000, 0x5b119910, 2 | BRF_GRA },           //  5
	{ "mr95033-03.14",						0x200000, 0x8b6099ed, 2 | BRF_GRA },           //  6
	{ "mr95033-04.2",						0x200000, 0x59144dc6, 2 | BRF_GRA },           //  7
	{ "mr95033-05.15",						0x200000, 0xcc5b8d0b, 2 | BRF_GRA },           //  8
	{ "mr95033-06.3",						0x200000, 0x2f79942f, 2 | BRF_GRA },           //  9

	{ "mr95033-07.9",						0x200000, 0x76cd2e0b, 3 | BRF_GRA },           // 10 ROZ Tiles

	{ "mr95033-09.11",						0x200000, 0x8a6dae81, 4 | BRF_GRA },           // 11 Background Tiles

	{ "pk_soccer_v2_rom_32_ver._1.1.32",	0x080000, 0xbecc25c2, 5 | BRF_GRA },           // 12 Characters

	{ "mb93139_ver1.0_ws-21.21",			0x040000, 0xbdeff5d6, 6 | BRF_PRG | BRF_ESS }, // 13 Z80 Code

	{ "mr92042-01.22",						0x200000, 0x0fa26f65, 7 | BRF_SND },           // 14 YMF271 Samples
	{ "mr95033-08.23",						0x200000, 0x89a291fa, 7 | BRF_SND },           // 15

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 16 Motherboard PAL
};

STD_ROM_PICK(wpksocv2)
STD_ROM_FN(wpksocv2)

static INT32 Wpksocv2Init()
{
	is_wpksocv2 = 1;

	return ss92046_01_init();
}

struct BurnDriver BurnDrvWpksocv2 = {
	"wpksocv2", NULL, NULL, NULL, "1996",
	"World PK Soccer V2 (ver 1.1)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, wpksocv2RomInfo, wpksocv2RomName, NULL, NULL, NULL, NULL, Wpksocv2InputInfo, Wpksocv2DIPInfo,
	Wpksocv2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Hayaoshi Quiz Grand Champion Taikai

static struct BurnRomInfo hayaosi2RomDesc[] = {
	{ "mb93138a.25",						0x080000, 0x563c6f2f, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mb93138a.27",						0x080000, 0xfe8e283a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mb93138a.29",						0x080000, 0xe6fe3d0d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mb93138a.31",						0x080000, 0xd944bf8c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr93038.04",							0x200000, 0xab5edb11, 2 | BRF_GRA },           //  4 Sprites
	{ "mr93038.05",							0x200000, 0x274522f1, 2 | BRF_GRA },           //  5
	{ "mr93038.06",							0x200000, 0xf9961ebf, 2 | BRF_GRA },           //  6
	{ "mr93038.07",							0x200000, 0x1abef1c5, 2 | BRF_GRA },           //  7
	{ "mb93138a.15",						0x080000, 0xa5f64d87, 2 | BRF_GRA },           //  8
	{ "mb93138a.3",							0x080000, 0xa2ae2b21, 2 | BRF_GRA },           //  9

	{ "mr93038.03",							0x200000, 0x6999dec9, 3 | BRF_GRA },           // 10 ROZ Tiles

	{ "mr93038.08",							0x100000, 0x21282cb0, 4 | BRF_GRA },           // 11 Background Tiles

	{ "mb93138a.32",						0x080000, 0xf563a144, 5 | BRF_GRA },           // 12 Characters

	{ "mb93138a.21",						0x040000, 0x8e8048b0, 6 | BRF_PRG | BRF_ESS }, // 13 Z80 Code

	{ "mr92042.01",							0x200000, 0x0fa26f65, 7 | BRF_SND },           // 14 YMF271 Samples
	{ "mr93038.01",							0x200000, 0xb8a38bfc, 7 | BRF_SND },           // 15

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 16 Motherboard PAL
};

STD_ROM_PICK(hayaosi2)
STD_ROM_FN(hayaosi2)

struct BurnDriver BurnDrvHayaosi2 = {
	"hayaosi2", NULL, NULL, NULL, "1994",
	"Hayaoshi Quiz Grand Champion Taikai\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, hayaosi2RomInfo, hayaosi2RomName, NULL, NULL, NULL, NULL, Hayaosi2InputInfo, Hayaosi2DIPInfo,
	ss92046_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Hayaoshi Quiz Nettou Namahousou (ver 1.5)

static struct BurnRomInfo hayaosi3RomDesc[] = {
	{ "mb93138_25_ver1.5.25",				0x080000, 0xba8cec03, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mb93138_27_ver1.5.27",				0x080000, 0x571725df, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mb93138_29_ver1.5.29",				0x080000, 0xda891976, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mb93138_31_ver1.5.31",				0x080000, 0x2d17bb06, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr94027.01",							0x200000, 0xc72e5c6e, 2 | BRF_GRA },           //  4 Sprites
	{ "mr94027.02",							0x200000, 0x59976568, 2 | BRF_GRA },           //  5
	{ "mr94027.03",							0x200000, 0x3ff68f4f, 2 | BRF_GRA },           //  6
	{ "mr94027.04",							0x200000, 0x6a16d13a, 2 | BRF_GRA },           //  7
	{ "mr94027.05",							0x200000, 0x59545977, 2 | BRF_GRA },           //  8
	{ "mr94027.06",							0x200000, 0x1618785a, 2 | BRF_GRA },           //  9
	{ "mr94027.07",							0x200000, 0xc66099c4, 2 | BRF_GRA },           // 10
	{ "mr94027.08",							0x200000, 0x753b05e0, 2 | BRF_GRA },           // 11

	{ "mr94027.09",							0x200000, 0x32ead437, 3 | BRF_GRA },           // 12 ROZ Tiles

	{ "mr94027.11",							0x200000, 0xb65d5096, 4 | BRF_GRA },           // 13 Background Tiles

	{ "mb93138_32_ver1.0.32",				0x080000, 0xdf5d00b4, 5 | BRF_GRA },           // 14 Characters

	{ "mb93138.21",							0x040000, 0x008bc217, 6 | BRF_PRG | BRF_ESS }, // 15 Z80 Code

	{ "mr92042.01",							0x200000, 0x0fa26f65, 7 | BRF_SND },           // 16 YMF271 Samples
	{ "mr94027.10",							0x200000, 0xe7cabe41, 7 | BRF_SND },           // 17

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 18 Motherboard PAL
};

STD_ROM_PICK(hayaosi3)
STD_ROM_FN(hayaosi3)

struct BurnDriver BurnDrvHayaosi3 = {
	"hayaosi3", NULL, NULL, NULL, "1994",
	"Hayaoshi Quiz Nettou Namahousou (ver 1.5)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, hayaosi3RomInfo, hayaosi3RomName, NULL, NULL, NULL, NULL, Hayaosi2InputInfo, Hayaosi3DIPInfo,
	ss92046_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Hayaoshi Quiz Nettou Namahousou (ver 1.2)

static struct BurnRomInfo hayaosi3aRomDesc[] = {
	{ "mb93138_25_ver1.2.25",				0x080000, 0x71b1f51b, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mb93138_27_ver1.2.27",				0x080000, 0x2657e8dc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mb93138_29_ver1.2.29",				0x080000, 0x8999b41b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mb93138_31_ver1.2.31",				0x080000, 0xf5d4ef54, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr94027.01",							0x200000, 0xc72e5c6e, 2 | BRF_GRA },           //  4 Sprites
	{ "mr94027.02",							0x200000, 0x59976568, 2 | BRF_GRA },           //  5
	{ "mr94027.03",							0x200000, 0x3ff68f4f, 2 | BRF_GRA },           //  6
	{ "mr94027.04",							0x200000, 0x6a16d13a, 2 | BRF_GRA },           //  7
	{ "mr94027.05",							0x200000, 0x59545977, 2 | BRF_GRA },           //  8
	{ "mr94027.06",							0x200000, 0x1618785a, 2 | BRF_GRA },           //  9
	{ "mr94027.07",							0x200000, 0xc66099c4, 2 | BRF_GRA },           // 10
	{ "mr94027.08",							0x200000, 0x753b05e0, 2 | BRF_GRA },           // 11

	{ "mr94027.09",							0x200000, 0x32ead437, 3 | BRF_GRA },           // 12 ROZ Tiles

	{ "mr94027.11",							0x200000, 0xb65d5096, 4 | BRF_GRA },           // 13 Background Tiles

	{ "mb93138_32_ver1.0.32",				0x080000, 0xdf5d00b4, 5 | BRF_GRA },           // 14 Characters

	{ "mb93138_21_ver1.0.21",				0x040000, 0x008bc217, 6 | BRF_PRG | BRF_ESS }, // 15 Z80 Code

	{ "mr92042.01",							0x200000, 0x0fa26f65, 7 | BRF_SND },           // 16 YMF271 Samples
	{ "mr94027.10",							0x200000, 0xe7cabe41, 7 | BRF_SND },           // 17

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 18 Motherboard PAL
};

STD_ROM_PICK(hayaosi3a)
STD_ROM_FN(hayaosi3a)

struct BurnDriver BurnDrvHayaosi3a = {
	"hayaosi3a", "hayaosi3", NULL, NULL, "1994",
	"Hayaoshi Quiz Nettou Namahousou (ver 1.2)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, hayaosi3aRomInfo, hayaosi3aRomName, NULL, NULL, NULL, NULL, Hayaosi2InputInfo, Hayaosi3DIPInfo,
	ss92046_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Idol Janshi Suchie-Pai II (ver 1.1)

static struct BurnRomInfo suchie2RomDesc[] = {
	{ "mb-93166-26_ver1.1.26",				0x080000, 0xe4e62134, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mb-93166-27_ver1.1.27",				0x080000, 0x7bd00919, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mb-93166-28_ver1.1.28",				0x080000, 0xaa49eec2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mb-93166-29_ver1.1.29",				0x080000, 0x92763e41, 1 | BRF_PRG | BRF_ESS }, //  3

	// swapped order from mame!
	{ "mb94019-01.13",						0x200000, 0x1ddfe825, 2 | BRF_GRA },           //  4 Sprites
	{ "mb94019-02.1",						0x200000, 0xf9d692f2, 2 | BRF_GRA },           //  5
	{ "mb94019-03.14",						0x200000, 0xb26426c4, 2 | BRF_GRA },           //  6
	{ "mb94019-04.2",						0x200000, 0x24ca77ec, 2 | BRF_GRA },           //  7
	{ "mb94019-05.15",						0x200000, 0x1da63eb4, 2 | BRF_GRA },           //  8
	{ "mb94019-06.3",						0x200000, 0xc8aa4b57, 2 | BRF_GRA },           //  9
	{ "mb94019-07.16",						0x200000, 0x34f471a8, 2 | BRF_GRA },           // 10
	{ "mb94019-08.4",						0x200000, 0x4b07edc9, 2 | BRF_GRA },           // 11

	{ "94019-09.11",						0x200000, 0xcde7bb6f, 3 | BRF_GRA },           // 12 ROZ Tiles

	{ "mb94019-12.10",						0x100000, 0x15ae05d9, 4 | BRF_GRA },           // 13 Background Tiles

	{ "mb-93166-30_ver1.0.30",				0x080000, 0x0c738883, 5 | BRF_GRA },           // 14 Characters

	{ "mb-93166-21_ver1.0.21",				0x040000, 0xe7fd1bf4, 6 | BRF_PRG | BRF_ESS }, // 15 Z80 Code

	{ "mb94019-10.22",						0x200000, 0x745d41ec, 7 | BRF_SND },           // 16 YMF271 Samples
	{ "mb94019-11.23",						0x200000, 0x021dc350, 7 | BRF_SND },           // 17

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 18 Motherboard PAL
};

STD_ROM_PICK(suchie2)
STD_ROM_FN(suchie2)

struct BurnDriver BurnDrvSuchie2 = {
	"suchie2", NULL, NULL, NULL, "1994",
	"Idol Janshi Suchie-Pai II (ver 1.1)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, suchie2RomInfo, suchie2RomName, NULL, NULL, NULL, NULL, MS32MahjongInputInfo, Suchie2DIPInfo,
	ss92048_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Idol Janshi Suchie-Pai II (ver 1.0)

static struct BurnRomInfo suchie2oRomDesc[] = {
	{ "mb-93166-26_ver1.0.26",				0x080000, 0x21dc94dd, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mb-93166-27_ver1.0.27",				0x080000, 0x5bf18a7d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mb-93166-28_ver1.0.28",				0x080000, 0xb1261d51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mb-93166-29_ver1.0.29",				0x080000, 0x9211c82a, 1 | BRF_PRG | BRF_ESS }, //  3

	// swapped order from mame!
	{ "mb94019-01.13",						0x200000, 0x1ddfe825, 2 | BRF_GRA },           //  4 Sprites
	{ "mb94019-02.1",						0x200000, 0xf9d692f2, 2 | BRF_GRA },           //  5
	{ "mb94019-03.14",						0x200000, 0xb26426c4, 2 | BRF_GRA },           //  6
	{ "mb94019-04.2",						0x200000, 0x24ca77ec, 2 | BRF_GRA },           //  7
	{ "mb94019-05.15",						0x200000, 0x1da63eb4, 2 | BRF_GRA },           //  8
	{ "mb94019-06.3",						0x200000, 0xc8aa4b57, 2 | BRF_GRA },           //  9
	{ "mb94019-07.16",						0x200000, 0x34f471a8, 2 | BRF_GRA },           // 10
	{ "mb94019-08.4",						0x200000, 0x4b07edc9, 2 | BRF_GRA },           // 11

	{ "mb94019-09.11",						0x200000, 0xcde7bb6f, 3 | BRF_GRA },           // 12 ROZ Tiles

	{ "mb94019-12.10",						0x100000, 0x15ae05d9, 4 | BRF_GRA },           // 13 Background Tiles

	{ "mb-93166-30_ver1.0.30",				0x080000, 0x0c738883, 5 | BRF_GRA },           // 14 Characters

	{ "mb-93166-21_ver1.0.21",				0x040000, 0xe7fd1bf4, 6 | BRF_PRG | BRF_ESS }, // 15 Z80 Code

	{ "mb94019-10.22",						0x200000, 0x745d41ec, 7 | BRF_SND },           // 16 YMF271 Samples
	{ "mb94019-11.23",						0x200000, 0x021dc350, 7 | BRF_SND },           // 17

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 18 Motherboard PAL
};

STD_ROM_PICK(suchie2o)
STD_ROM_FN(suchie2o)

struct BurnDriver BurnDrvSuchie2o = {
	"suchie2o", "suchie2", NULL, NULL, "1994",
	"Idol Janshi Suchie-Pai II (ver 1.0)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, suchie2oRomInfo, suchie2oRomName, NULL, NULL, NULL, NULL, MS32MahjongInputInfo, Suchie2DIPInfo,
	ss92048_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Ryuusei Janshi Kirara Star

static struct BurnRomInfo kirarastRomDesc[] = {
	{ "mr95025.26",							0x080000, 0xeb7faf5f, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mr95025.27",							0x080000, 0x80644d05, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mr95025.28",							0x080000, 0x6df8c384, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mr95025.29",							0x080000, 0x3b6e681b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr95025.01",							0x200000, 0x02279069, 2 | BRF_GRA },           //  4 Sprites
	{ "mr95025.02",							0x200000, 0x885161d4, 2 | BRF_GRA },           //  5
	{ "mr95025.03",							0x200000, 0x1ae06df9, 2 | BRF_GRA },           //  6
	{ "mr95025.04",							0x200000, 0x91ab7006, 2 | BRF_GRA },           //  7
	{ "mr95025.05",							0x200000, 0xe61af029, 2 | BRF_GRA },           //  8
	{ "mr95025.06",							0x200000, 0x63f64ffc, 2 | BRF_GRA },           //  9
	{ "mr95025.07",							0x200000, 0x0263a010, 2 | BRF_GRA },           // 10
	{ "mr95025.08",							0x200000, 0x8efc00d6, 2 | BRF_GRA },           // 11

	{ "mr95025.10",							0x200000, 0xba7ad413, 3 | BRF_GRA },           // 12 ROZ Tiles
	{ "mr95025.11",							0x200000, 0x11557299, 3 | BRF_GRA },           // 13

	{ "mr95025.09",							0x200000, 0xca6cbd17, 4 | BRF_GRA },           // 14 Background Tiles

	{ "mr95025.30",							0x080000, 0xaee6e0c2, 5 | BRF_GRA },           // 15 Characters

	{ "mr95025.21",							0x040000, 0xa6c70c7f, 6 | BRF_PRG | BRF_ESS }, // 16 Z80 Code

	{ "mr95025.12",							0x200000, 0x1dd4f766, 7 | BRF_SND },           // 17 YMF271 Samples
	{ "mr95025.13",							0x200000, 0x0adfe5b8, 7 | BRF_SND },           // 18

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 19 Motherboard PAL
};

STD_ROM_PICK(kirarast)
STD_ROM_FN(kirarast)

struct BurnDriver BurnDrvKirarast = {
	"kirarast", NULL, NULL, NULL, "1996",
	"Ryuusei Janshi Kirara Star\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, kirarastRomInfo, kirarastRomName, NULL, NULL, NULL, NULL, MS32MahjongInputInfo, KirarastDIPInfo,
	ss92047_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Mahjong Angel Kiss (ver 1.0)

static struct BurnRomInfo akissRomDesc[] = {
	{ "mb93166_ver1.0-26.26",				0x080000, 0x5bdd01ee, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mb93166_ver1.0-27.27",				0x080000, 0xbb11b2c9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mb93166_ver1.0-28.28",				0x080000, 0x20565478, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mb93166_ver1.0-29.29",				0x080000, 0xff454f0d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mb95008-01.13",						0x200000, 0x1be66420, 2 | BRF_GRA },           //  4 Sprites
	{ "mb95008-02.1",						0x200000, 0x1cc4808e, 2 | BRF_GRA },           //  5
	{ "mb95008-03.14",						0x200000, 0x4045f0dc, 2 | BRF_GRA },           //  6
	{ "mb95008-04.2",						0x200000, 0xef3c139d, 2 | BRF_GRA },           //  7
	{ "mb95008-05.15",						0x200000, 0x43ea4a84, 2 | BRF_GRA },           //  8
	{ "mb95008-06.3",						0x200000, 0x24f23d4e, 2 | BRF_GRA },           //  9
	{ "mb95008-07.16",						0x200000, 0xbf47747e, 2 | BRF_GRA },           // 10
	{ "mb95008-08.4",						0x200000, 0x34829a09, 2 | BRF_GRA },           // 11

	{ "mb95008-10.11",						0x200000, 0x52da2e9e, 3 | BRF_GRA },           // 12 ROZ Tiles

	{ "mb95008-09.10",						0x200000, 0x7236f6a0, 4 | BRF_GRA },           // 13 Background Tiles

	{ "mb93166_ver1.0-30.30",				0x080000, 0x1807c1ea, 5 | BRF_GRA },           // 14 Characters

	{ "mb93166_ver1.0-21.21",				0x040000, 0x01a03687, 6 | BRF_PRG | BRF_ESS }, // 15 Z80 Code

	{ "mb95008-11.22",						0x200000, 0x23b9af76, 7 | BRF_SND },           // 16 YMF271 Samples
	{ "mb95008-12.23",						0x200000, 0x780a2f45, 7 | BRF_SND },           // 17

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 18 Motherboard PAL
};

STD_ROM_PICK(akiss)
STD_ROM_FN(akiss)

struct BurnDriver BurnDrvAkiss = {
	"akiss", NULL, NULL, NULL, "1995",
	"Mahjong Angel Kiss (ver 1.0)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, akissRomInfo, akissRomName, NULL, NULL, NULL, NULL, MS32MahjongInputInfo, Suchie2DIPInfo,
	ss92047_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Vs. Janshi Brandnew Stars

static struct BurnRomInfo bnstars1RomDesc[] = {
	{ "mb-93142.36",						0x080000, 0x2eb6a503, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "mb-93142.37",						0x080000, 0x49f60882, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mb-93142.38",						0x080000, 0x6e1312cd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mb-93142.39",						0x080000, 0x56b98539, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr96004-01.20",						0x200000, 0x3366d104, 2 | BRF_GRA },           //  4 Sprites
	{ "mr96004-02.28",						0x200000, 0xad556664, 2 | BRF_GRA },           //  5
	{ "mr96004-03.21",						0x200000, 0xb399e2b1, 2 | BRF_GRA },           //  6
	{ "mr96004-04.29",						0x200000, 0xf4f4cf4a, 2 | BRF_GRA },           //  7
	{ "mr96004-05.22",						0x200000, 0xcd6c357e, 2 | BRF_GRA },           //  8
	{ "mr96004-06.30",						0x200000, 0xfc6daad7, 2 | BRF_GRA },           //  9
	{ "mr96004-07.23",						0x200000, 0x177e32fa, 2 | BRF_GRA },           // 10
	{ "mr96004-08.31",						0x200000, 0xf6df27b2, 2 | BRF_GRA },           // 11

	// all of the following have double roms, don't bother loading them when making a driver for this...
	{ "mr96004-09.1",						0x400000, 0x7f8ea9f0, 3 | BRF_GRA },           // 12 Screen 1 Roz Tiles

	{ "mr96004-09.7",						0x400000, 0x7f8ea9f0, 0 | BRF_GRA },           // 13 Screen 2 Roz Tiles

	{ "mr96004-11.11",						0x200000, 0xe6da552c, 5 | BRF_GRA },           // 14 Screen 1 Background Tiles

	{ "vsjanshi6.5",						0x080000, 0xfdbbac21, 6 | BRF_GRA },           // 15 Screen 1 Characters

	{ "mr96004-11.13",						0x200000, 0xe6da552c, 0 | BRF_GRA },           // 16 Screen 2 Background Tiles

	{ "vsjanshi5.6",						0x080000, 0xfdbbac21, 0 | BRF_GRA },           // 17 Screen 2 Characters

	{ "sb93145.5",							0x040000, 0x0424e899, 6 | BRF_PRG | BRF_ESS }, // 18 Z80 Code

	{ "mr96004-10.1",						0x400000, 0x83f4303a, 7 | BRF_SND },           // 19 YMF271 #0 Samples

	{ "mr96004-10.1",						0x400000, 0x83f4303a, 0 | BRF_SND },           // 20 YMF271 #1 Samples
};

STD_ROM_PICK(bnstars1)
STD_ROM_FN(bnstars1)

struct BurnDriver BurnDrvBnstars1 = {
	"bnstars1", NULL, NULL, NULL, "1997",
	"Vs. Janshi Brandnew Stars\0", "Not currently emulated", "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, bnstars1RomInfo, bnstars1RomName, NULL, NULL, NULL, NULL, MS32MahjongInputInfo, Suchie2DIPInfo, //BnstarsInputInfo, BnstarsDIPInfo,
	ss92046_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};


// Vs. Janshi Brandnew Stars (Ver 1.1, MegaSystem32 Version)

static struct BurnRomInfo bnstarsRomDesc[] = {
	{ "vsjanshi_26_ver1.1.26",				0x080000, 0x75eeec8f, 1 | BRF_PRG | BRF_ESS }, //  0 V70 Code
	{ "vsjanshi_27_ver1.1.27",				0x080000, 0x69f24ab9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vsjanshi_28_ver1.1.28",				0x080000, 0xd075cfb6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "vsjanshi_29_ver1.1.29",				0x080000, 0xbc395b50, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mr96004-01.13",						0x200000, 0x3366d104, 2 | BRF_GRA },           //  4 Sprites
	{ "mr96004-02.1",						0x200000, 0xad556664, 2 | BRF_GRA },           //  5
	{ "mr96004-03.14",						0x200000, 0xb399e2b1, 2 | BRF_GRA },           //  6
	{ "mr96004-04.2",						0x200000, 0xf4f4cf4a, 2 | BRF_GRA },           //  7
	{ "mr96004-05.15",						0x200000, 0xcd6c357e, 2 | BRF_GRA },           //  8
	{ "mr96004-06.3",						0x200000, 0xfc6daad7, 2 | BRF_GRA },           //  9
	{ "mr96004-07.16",						0x200000, 0x177e32fa, 2 | BRF_GRA },           // 10
	{ "mr96004-08.4",						0x200000, 0xf6df27b2, 2 | BRF_GRA },           // 11

	{ "mr96004-09.11",						0x400000, 0x7f8ea9f0, 3 | BRF_GRA },           // 12 ROZ Tiles

	{ "mr96004-11.10",						0x200000, 0xe6da552c, 4 | BRF_GRA },           // 13 Background Tiles

	{ "vsjanshi_30_ver1.0.30",				0x080000, 0xfdbbac21, 5 | BRF_GRA },           // 14 Characters

	{ "vsjanshi_21_ver1.0.21",				0x040000, 0xd622bce1, 6 | BRF_PRG | BRF_ESS }, // 15 Z80 Code

	{ "mr96004-10.22",						0x400000, 0x83f4303a, 7 | BRF_SND },           // 16 YMF271 Samples

	{ "91022-01.ic83",						0x000001, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 17 Motherboard PAL
};

STD_ROM_PICK(bnstars)
STD_ROM_FN(bnstars)

struct BurnDriver BurnDrvBnstars = {
	"bnstars", "bnstars1", NULL, NULL, "1997",
	"Vs. Janshi Brandnew Stars (Ver 1.1, MegaSystem32 Version)\0", NULL, "Jaleco", "MegaSystem 32",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, bnstarsRomInfo, bnstarsRomName, NULL, NULL, NULL, NULL, MS32MahjongInputInfo, Suchie2DIPInfo,
	ss92046_01_init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 224, 4, 3
};
