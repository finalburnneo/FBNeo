// Crazy Climber FBNeo Driver
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80OPS;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvDecodePROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvUser1;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvZ80RAM1_0;
static UINT8 *DrvBGSprRAM;
static UINT8 *DrvBgVidRAM;
static UINT8 *DrvBgColRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvPalRAM;
static INT16 *samplebuf;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 DrvGfxROM0Len;
static INT32 DrvGfxROM1Len;

static INT32 flipscreen[2];
static INT32 interrupt_enable;
static UINT8 yamato_p0;
static UINT8 yamato_p1;
static UINT8 yamato_bg[3];
static UINT8 swimmer_background_color;
static UINT8 swimmer_sidebg;
static UINT8 swimmer_palettebank;
static UINT8 soundlatch;
static INT32 bankdata;

static UINT8 DrvReset;
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvInputs[4];
static UINT8 DrvDips[2];

// per-game constants
enum { CCLIMBER=0, SILVLAND, CKONG, CKONGB, YAMATO, GUZZLER, TANGRAMQ, AU, CANNONB, TOPROLLR, BAGMANF };
static INT32 game_select = 0;
static INT32 bigsprite_index = 0;
static INT32 uses_sub = 0;
static INT32 uses_samples = 0;

static struct BurnInputInfo CclimberInputList[] = {
	{"P1 Coin", 		BIT_DIGITAL, 	DrvJoy3 + 0, 	"p1 coin"  },
	{"P1 Start", 		BIT_DIGITAL, 	DrvJoy3 + 2, 	"p1 start" },
	{"P1 Up (left)",	BIT_DIGITAL, 	DrvJoy1 + 0, 	"p1 up"    },
	{"P1 Down (left)",	BIT_DIGITAL, 	DrvJoy1 + 1, 	"p1 down"  },
	{"P1 Left (left)",	BIT_DIGITAL, 	DrvJoy1 + 2, 	"p1 left"  },
	{"P1 Right (left)", BIT_DIGITAL,	DrvJoy1 + 3, 	"p1 right" },
	{"P1 Up (right)", 	BIT_DIGITAL, 	DrvJoy1 + 4, 	"p3 up"    },
	{"P1 Down (right)", BIT_DIGITAL, 	DrvJoy1 + 5, 	"p3 down"  },
	{"P1 Left (right)", BIT_DIGITAL, 	DrvJoy1 + 6, 	"p3 left"  },
	{"P1 Right (right)",BIT_DIGITAL, 	DrvJoy1 + 7, 	"p3 right" },

	{"P2 Coin", 		BIT_DIGITAL, 	DrvJoy3 + 1, 	"p2 coin"  },
	{"P2 Start", 		BIT_DIGITAL, 	DrvJoy3 + 3, 	"p2 start" },
	{"P2 Up (left)", 	BIT_DIGITAL, 	DrvJoy2 + 0, 	"p2 up"    },
	{"P2 Down (left)", 	BIT_DIGITAL, 	DrvJoy2 + 1, 	"p2 down"  },
	{"P2 Left (left)", 	BIT_DIGITAL, 	DrvJoy2 + 2, 	"p2 left"  },
	{"P2 Right (left)", BIT_DIGITAL, 	DrvJoy2 + 3, 	"p2 right" },
	{"P2 Up (right)", 	BIT_DIGITAL, 	DrvJoy2 + 4, 	"p4 up"    },
	{"P2 Down (right)", BIT_DIGITAL, 	DrvJoy2 + 5, 	"p4 down"  },
	{"P2 Left (right)", BIT_DIGITAL, 	DrvJoy2 + 6, 	"p4 left"  },
	{"P2 Right (right)",BIT_DIGITAL, 	DrvJoy2 + 7, 	"p4 right" },

	{"Reset", 			BIT_DIGITAL, 	&DrvReset  , 	"reset"    },
	{"Dip A", 			BIT_DIPSWITCH,	DrvDips + 0, 	"dip"      },
	{"Dip B", 			BIT_DIPSWITCH,	DrvDips + 1, 	"dip"      },
};

STDINPUTINFO(Cclimber)

static struct BurnInputInfo RpatrolInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Rpatrol)

static struct BurnInputInfo CkongInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Ckong)

static struct BurnInputInfo GuzzlerInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Guzzler)


static struct BurnInputInfo YamatoInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Yamato)

static struct BurnInputInfo SwimmerInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Swimmer)

static struct BurnInputInfo TangramqInputList[] = {
	{"P1 Coin", 		BIT_DIGITAL,	DrvJoy3 + 5, 	"p1 coin"	},
	{"P1 Start", 		BIT_DIGITAL, 	DrvJoy3 + 2, 	"p1 start"	},

	{"P1 Button 1", 	BIT_DIGITAL, 	DrvJoy1 + 1, 	"p1 fire 1"	},
	{"P1 Right", 		BIT_DIGITAL, 	DrvJoy1 + 2, 	"p1 right"	},
	{"P1 Left", 		BIT_DIGITAL, 	DrvJoy1 + 3, 	"p1 left"	},

	{"P2 Coin",			BIT_DIGITAL, 	DrvJoy4 + 3, 	"p2 coin"	},
	{"P2 Start", 		BIT_DIGITAL, 	DrvJoy3 + 1, 	"p2 start"	},

	{"P2 Button 1", 	BIT_DIGITAL, 	DrvJoy2 + 1, 	"p2 fire 1"	},
	{"P2 Right", 		BIT_DIGITAL, 	DrvJoy2 + 2, 	"p2 right"	},
	{"P2 Left", 		BIT_DIGITAL, 	DrvJoy2 + 3, 	"p2 left"	},

	{"Reset", 			BIT_DIGITAL, 	&DrvReset, 		"reset"		},
	{"Dip A", 			BIT_DIPSWITCH,	DrvDips + 0, 	"dip"		},
	{"Dip B", 			BIT_DIPSWITCH,	DrvDips + 1, 	"dip"		},
};

STDINPUTINFO(Tangramq)

static struct BurnInputInfo AuInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Au)

static struct BurnInputInfo CannonbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"Start",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"Select",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Cannonb)

static struct BurnInputInfo ToprollrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Toprollr)

static struct BurnDIPInfo CclimberDIPList[]=
{
	{0x15, 0xff, 0xff, 0x00, NULL		},
	{0x16, 0xff, 0xff, 0xf0, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x03, 0x00, "3"		},
	{0x15, 0x01, 0x03, 0x01, "4"		},
	{0x15, 0x01, 0x03, 0x02, "5"		},
	{0x15, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    2, "Rack Test (Cheat)"		},
	{0x15, 0x01, 0x08, 0x00, "Off"		},
	{0x15, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x15, 0x01, 0x30, 0x30, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x30, 0x20, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x30, 0x00, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x15, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0xc0, 0xc0, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x16, 0x01, 0x10, 0x10, "Upright"		},
	{0x16, 0x01, 0x10, 0x00, "Cocktail"		},
};

STDDIPINFO(Cclimber)

static struct BurnDIPInfo CclimberjDIPList[]=
{
	{0x15, 0xff, 0xff, 0x00, NULL		},
	{0x16, 0xff, 0xff, 0xf0, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x03, 0x00, "3"		},
	{0x15, 0x01, 0x03, 0x01, "4"		},
	{0x15, 0x01, 0x03, 0x02, "5"		},
	{0x15, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    2, "Bonus_Life"		},
	{0x15, 0x01, 0x04, 0x00, "30000"		},
	{0x15, 0x01, 0x04, 0x04, "50000"		},

	{0   , 0xfe, 0   ,    2, "Rack Test (Cheat)"		},
	{0x15, 0x01, 0x08, 0x00, "Off"		},
	{0x15, 0x01, 0x08, 0x08, "On"		},
	
	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x15, 0x01, 0x30, 0x30, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x30, 0x20, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x30, 0x00, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x15, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0xc0, 0xc0, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x16, 0x01, 0x10, 0x10, "Upright"		},
	{0x16, 0x01, 0x10, 0x00, "Cocktail"		},
};

STDDIPINFO(Cclimberj)

static struct BurnDIPInfo RpatrolDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x90, NULL		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0a, 0x01, 0x03, 0x02, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0x03, 0x00, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0x03, 0x03, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0a, 0x01, 0x0c, 0x00, "3"		},
	{0x0a, 0x01, 0x0c, 0x04, "4"		},
	{0x0a, 0x01, 0x0c, 0x08, "5"		},
	{0x0a, 0x01, 0x0c, 0x0c, "6"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0a, 0x01, 0x10, 0x10, "Upright"		},
	{0x0a, 0x01, 0x10, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Unknown 1"		},
	{0x0a, 0x01, 0x20, 0x00, "Off"		},
	{0x0a, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown 2"		},
	{0x0a, 0x01, 0x40, 0x00, "Off"		},
	{0x0a, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Memory Test"		},
	{0x0a, 0x01, 0x80, 0x00, "Retry on Error"		},
	{0x0a, 0x01, 0x80, 0x80, "Stop on Error"		},
};

STDDIPINFO(Rpatrol)

static struct BurnDIPInfo CkongDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x80, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x00, "3"		},
	{0x0f, 0x01, 0x03, 0x01, "4"		},
	{0x0f, 0x01, 0x03, 0x02, "5"		},
	{0x0f, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0f, 0x01, 0x0c, 0x00, "7000"		},
	{0x0f, 0x01, 0x0c, 0x04, "10000"		},
	{0x0f, 0x01, 0x0c, 0x08, "15000"		},
	{0x0f, 0x01, 0x0c, 0x0c, "20000"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0f, 0x01, 0x70, 0x70, "5 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x50, "4 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x30, "3 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x10, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x70, 0x20, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x70, 0x40, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0x70, 0x60, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x80, 0x80, "Upright"		},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Ckong)

static struct BurnDIPInfo CkongbDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x82, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x00, "1"		},
	{0x0f, 0x01, 0x03, 0x01, "2"		},
	{0x0f, 0x01, 0x03, 0x02, "3"		},
	{0x0f, 0x01, 0x03, 0x03, "4"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0f, 0x01, 0x0c, 0x00, "7000"		},
	{0x0f, 0x01, 0x0c, 0x04, "10000"		},
	{0x0f, 0x01, 0x0c, 0x08, "15000"		},
	{0x0f, 0x01, 0x0c, 0x0c, "20000"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0f, 0x01, 0x70, 0x70, "5 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x50, "4 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x30, "3 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x10, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x70, 0x20, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x70, 0x40, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0x70, 0x60, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x80, 0x80, "Upright"		},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Ckongb)

static struct BurnDIPInfo Ckongb2DIPList[]=
{
	{0x0f, 0xff, 0xff, 0x81, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x00, "2"		},
	{0x0f, 0x01, 0x03, 0x01, "3"		},
	{0x0f, 0x01, 0x03, 0x02, "4"		},
	{0x0f, 0x01, 0x03, 0x03, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0f, 0x01, 0x0c, 0x00, "7000"		},
	{0x0f, 0x01, 0x0c, 0x04, "10000"		},
	{0x0f, 0x01, 0x0c, 0x08, "15000"		},
	{0x0f, 0x01, 0x0c, 0x0c, "20000"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0f, 0x01, 0x70, 0x70, "5 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x50, "4 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x30, "3 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x10, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x70, 0x20, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x70, 0x40, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0x70, 0x60, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x80, 0x80, "Upright"		},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Ckongb2)

static struct BurnDIPInfo GuzzlerDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x00, NULL		},
	{0x10, 0xff, 0xff, 0x10, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x00, "3"		},
	{0x0f, 0x01, 0x03, 0x01, "4"		},
	{0x0f, 0x01, 0x03, 0x02, "5"		},
	{0x0f, 0x01, 0x03, 0x03, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0f, 0x01, 0x0c, 0x04, "20K, every 50K"		},
	{0x0f, 0x01, 0x0c, 0x00, "30K, every 100K"		},
	{0x0f, 0x01, 0x0c, 0x08, "30K only"		},
	{0x0f, 0x01, 0x0c, 0x0c, "None"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0f, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x30, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x30, 0x30, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0f, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x10, 0x10, "Upright"		},
	{0x10, 0x01, 0x10, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "High Score Names"		},
	{0x10, 0x01, 0x20, 0x20, "3 Letters"		},
	{0x10, 0x01, 0x20, 0x00, "10 Letters"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x10, 0x01, 0xc0, 0x00, "Easy"		},
	{0x10, 0x01, 0xc0, 0x40, "Medium"		},
	{0x10, 0x01, 0xc0, 0x80, "Hard"		},
	{0x10, 0x01, 0xc0, 0xc0, "Hardest"		},
};

STDDIPINFO(Guzzler)

static struct BurnDIPInfo YamatoDIPList[]=
{
	{0x11, 0xff, 0xff, 0x80, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x03, 0x00, "3"		},
	{0x11, 0x01, 0x03, 0x01, "4"		},
	{0x11, 0x01, 0x03, 0x02, "5"		},
	{0x11, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x11, 0x01, 0x1c, 0x0c, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x1c, 0x04, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x1c, 0x18, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x1c, 0x10, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x1c, 0x1c, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x11, 0x01, 0x20, 0x00, "Every 30000"		},
	{0x11, 0x01, 0x20, 0x20, "Every 50000"		},

	{0   , 0xfe, 0   ,    2, "Speed"		},
	{0x11, 0x01, 0x40, 0x00, "Slow"		},
	{0x11, 0x01, 0x40, 0x40, "Fast"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x80, 0x80, "Upright"		},
	{0x11, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Yamato)

static struct BurnDIPInfo SwimmerDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x00, NULL		},
	{0x10, 0xff, 0xff, 0x30, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x00, "3"		},
	{0x0f, 0x01, 0x03, 0x01, "4"		},
	{0x0f, 0x01, 0x03, 0x02, "5"		},
	{0x0f, 0x01, 0x03, 0x03, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0f, 0x01, 0x0c, 0x00, "10000"		},
	{0x0f, 0x01, 0x0c, 0x04, "20000"		},
	{0x0f, 0x01, 0x0c, 0x08, "30000"		},
	{0x0f, 0x01, 0x0c, 0x0c, "None"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0f, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x30, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x30, 0x30, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0f, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x10, 0x10, "Upright"		},
	{0x10, 0x01, 0x10, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x10, 0x01, 0x20, 0x00, "Off"		},
	{0x10, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x10, 0x01, 0xc0, 0x00, "Easy"		},
	{0x10, 0x01, 0xc0, 0x40, "Hard"		},
	{0x10, 0x01, 0xc0, 0x80, "Harder"		},
	{0x10, 0x01, 0xc0, 0xc0, "Hardest"		},
};

STDDIPINFO(Swimmer)

static struct BurnDIPInfo TangramqDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x8e, NULL			},
	{0x0c, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0b, 0x01, 0x03, 0x00, "1"			},
	{0x0b, 0x01, 0x03, 0x01, "2"			},
	{0x0b, 0x01, 0x03, 0x02, "3"			},
	{0x0b, 0x01, 0x03, 0x03, "5"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x0b, 0x01, 0x04, 0x04, "Off"			},
	{0x0b, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"	},
	{0x0b, 0x01, 0x08, 0x00, "Off"			},
	{0x0b, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0b, 0x01, 0x70, 0x70, "4 Coins 1 Credits"		},
	{0x0b, 0x01, 0x70, 0x60, "3 Coins 1 Credits"		},
	{0x0b, 0x01, 0x70, 0x50, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x70, 0x00, "1 Coins 1 Credits"		},
	{0x0b, 0x01, 0x70, 0x10, "1 Coin  2 Credits"		},
	{0x0b, 0x01, 0x70, 0x20, "1 Coin  3 Credits"		},
	{0x0b, 0x01, 0x70, 0x30, "1 Coin  5 Credits"		},
	{0x0b, 0x01, 0x70, 0x40, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0b, 0x01, 0x80, 0x80, "Upright"		},
	{0x0b, 0x01, 0x80, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Infinite Lives"	},
	{0x0c, 0x01, 0x10, 0x10, "Off"		},
	{0x0c, 0x01, 0x10, 0x00, "On"		},
};

STDDIPINFO(Tangramq)

static struct BurnDIPInfo AuDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0x00, NULL		},
	{0x01, 0xff, 0xff, 0x80, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x00, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x03, 0x03, "Disabled"				},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x00, 0x01, 0x30, 0x00, "30000, 100000, Every 100000"	},
	{0x00, 0x01, 0x30, 0x10, "20000, 50000, Every 50000"	},
	{0x00, 0x01, 0x30, 0x20, "30000"						},
	{0x00, 0x01, 0x30, 0x30, "None"							},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x00, 0x01, 0xc0, 0x00, "3"					},
	{0x00, 0x01, 0xc0, 0x40, "4"					},
	{0x00, 0x01, 0xc0, 0x80, "5"					},
	{0x00, 0x01, 0xc0, 0xc0, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x01, 0x01, 0x80, 0x80, "Upright"		},
	{0x01, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Au)

static struct BurnDIPInfo CannonbDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    4, "Display"		},
	{0x0f, 0x01, 0x03, 0x03, "Scores and Progression Bars"		},
	{0x0f, 0x01, 0x03, 0x01, "Scores only"		},
	{0x0f, 0x01, 0x03, 0x02, "Progression Bars only"		},
	{0x0f, 0x01, 0x03, 0x00, "None"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x04, 0x04, "Upright"		},
	{0x0f, 0x01, 0x04, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x18, 0x00, "3"		},
	{0x0f, 0x01, 0x18, 0x08, "4"		},
	{0x0f, 0x01, 0x18, 0x10, "5"		},
	{0x0f, 0x01, 0x18, 0x18, "6"		},
};

STDDIPINFO(Cannonb)

static struct BurnDIPInfo ToprollrDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x80, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x00, "3"		},
	{0x0f, 0x01, 0x03, 0x01, "4"		},
	{0x0f, 0x01, 0x03, 0x02, "5"		},
	{0x0f, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x0f, 0x01, 0x1c, 0x0c, "4 Coins 1 Credits"		},
	{0x0f, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"		},
	{0x0f, 0x01, 0x1c, 0x04, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x1c, 0x18, "2 Coins 3 Credits"		},
	{0x0f, 0x01, 0x1c, 0x10, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0x1c, 0x1c, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0f, 0x01, 0x20, 0x00, "Every 30000"		},
	{0x0f, 0x01, 0x20, 0x20, "Every 50000"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0f, 0x01, 0x40, 0x00, "Easy"		},
	{0x0f, 0x01, 0x40, 0x40, "Hard"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x80, 0x80, "Upright"		},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Toprollr)

// cclimber sample player
static INT32 sample_num = 0;
static INT32 sample_freq = 0;
static INT32 sample_vol = 0;

static INT32 sample_len = 0;
static INT32 sample_pos = -1; // -1 not playing, 0 start

static void cclimber_render(INT16 *buffer, INT32 nLen)
{
	if (sample_pos < 0) return; // stopped

	if ((sample_pos >> 16) >= 0x10000 ) {
		sample_pos = -1; // stop
		return;
	}

	INT32 step = (sample_freq << 16) / nBurnSoundRate;
	INT32 pos = 0;
	INT16 *rom = samplebuf;

	while (pos < nLen)
	{
		INT32 sample = (INT32)(rom[(sample_pos >> 16)] * 0.2);

		buffer[0] = BURN_SND_CLIP((INT32)(buffer[0] + sample));
		buffer[1] = BURN_SND_CLIP((INT32)(buffer[1] + sample));

		sample_pos += step;

		buffer+=2;
		pos++;

		if (sample_pos >= 0xfff0000 || (sample_pos >> 16) >= sample_len) {
			sample_pos = -1; // stop
			break;
		}
	}
}

// 4bit decoder from mame
#define SAMPLE_CONV4(a) (0x1111*((a&0x0f))-0x8000)

static void sample_start()
{
	const UINT8 *rom = DrvSndROM;

	if (!rom || !uses_samples) return;

	INT32 len = 0;
	INT32 start = 32 * sample_num;

	while (start + len < 0x2000 && rom[start+len] != 0x70)
	{
		INT32 sample;

		sample = (rom[start + len] & 0xf0) >> 4;
		samplebuf[2*len] = SAMPLE_CONV4(sample) * sample_vol / 31;

		sample = rom[start + len] & 0x0f;
		samplebuf[2*len + 1] = SAMPLE_CONV4(sample) * sample_vol / 31;

		len++;
	}
	sample_len = len * 2;
	sample_pos = 0;
}

// end sample player

static void bankswitch(INT32 offset, INT32 data)
{
	offset -= 5;

	bankdata = (bankdata & ~(1 << offset)) | ((data & 1) << offset);

	if (bankdata < 3)
	{
		INT32 bank = bankdata * 0x10000; 

		ZetMapMemory(DrvZ80ROM + bank, 0x0000, 0x5fff, MAP_READ);
		ZetMapArea(0x0000, 0x5fff, 2, DrvZ80OPS + bank, DrvZ80ROM + bank);
	}
}

static void __fastcall cclimber_write(UINT16 address, UINT8 data)
{
	if (address >= 0x9c00 && address <= 0x9fff) {
		INT32 offset = (address - 0x9c00) & 0x0fdf;
		DrvColRAM[offset] = data;
		DrvColRAM[offset + 0x20] = data;
		return;
	}

	if (game_select == AU && (address & 0xf880) == 0xb800) {
		DrvPalRAM[address & 0x7f] = data;
		return;
	}

	switch (address)
	{
		case 0xa000:
			interrupt_enable = data;
		return;

		case 0xa001:
			if (game_select == CANNONB) flipscreen[0] = flipscreen[1] = data & 1;
		case 0xa002:
			if (game_select != CANNONB)
			{
				// Note: coctail can't be unflipped without breaking other games :(
				// Note2: find a better way..
				flipscreen[address & 1] = data & 1;
			}
		return;

		case 0xa003:
			switch (game_select)
			{
				case GUZZLER:
					swimmer_sidebg = data;
					break;
				case CKONGB:
				case BAGMANF:
					interrupt_enable = data;
					break;
				case YAMATO:
					yamato_bg[2] = data & 1;
					break;
			}
		return;

		case 0xa004:
			if (data != 0) sample_start();
			if (game_select == GUZZLER) {
				swimmer_palettebank = data;
			}
		return;

		case 0xa005:
			yamato_bg[1] = data & 1;
			if (game_select == TOPROLLR) {
				bankswitch(address & 0xf, data);
			}
		return;

		case 0xa006:
			yamato_bg[0] = data & 1;
			if (game_select == TOPROLLR) {
				bankswitch(address & 0xf, data);
			}
		return;

		case 0xa800:
			sample_freq = 3072000 / 4 / (256 - data);
			if (game_select == GUZZLER || game_select == AU) {
				soundlatch = data;
				ZetSetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
			}
		return;

		case 0xb000:
			sample_vol = data & 0x1f;
			if (game_select == TANGRAMQ) {
				soundlatch = data;
				ZetSetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
			}
		return;

		case 0xb800:
			if (game_select == GUZZLER) {
				swimmer_background_color = data;
			}
		return;
	}

	return;
}

static UINT8 __fastcall cclimber_read(UINT16 address)
{
	switch (game_select)
	{
		case AU:
		case GUZZLER:
			switch (address)
			{
				case 0xa000:
					return DrvInputs[1];
				case 0xa800:
					return DrvInputs[0];
				case 0xb000:
					return DrvDips[0];
				case 0xb800:
					return DrvDips[1] | DrvInputs[2];
				case 0xb880:
					return DrvInputs[3];
			}
			return 0;
		case TANGRAMQ:
			switch (address)
			{
				case 0x8000:
					return DrvInputs[2];
				case 0x8020:
					return DrvInputs[3];
				case 0xa000:
					return DrvInputs[0];
				case 0xa800:
					return DrvInputs[1];
				case 0xb000:
					return DrvDips[1];
				case 0xb800:
					return DrvDips[0];
			}
			return 0;
		case TOPROLLR:
			switch (address)
			{
				case 0xa000:
					return DrvInputs[0];
				case 0xa800:
					return DrvInputs[1];
				case 0xb000:
					return DrvDips[0];
				case 0xb800:
					return DrvInputs[2];
			}
			return 0;
		default:
			switch (address)
			{
				case 0xa000:
					return DrvInputs[0];
				case 0xa800:
					return DrvInputs[1];
				case 0xb000:
					return DrvDips[0];
				case 0xb800:
					return (DrvDips[1] & 0x10) | (DrvInputs[2] & ~0x10);
				case 0xba00:
					return DrvInputs[3];
			}
			return 0;
	}

	return 0;
}

static UINT8 __fastcall swimmer_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000: {
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			UINT8 sl = soundlatch;
			soundlatch = 0;
			return sl;
		}
	}

	return 0;
}

static void __fastcall cclimber_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x08:
		case 0x09:
			if (game_select != YAMATO) AY8910Write(0, port & 1, data);
		return;
		case 0x00:
			yamato_p0 = data;
		return;
		case 0x01:
			yamato_p1 = data;
		return;
	}
}

static UINT8 __fastcall cclimber_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x0c:
			return AY8910Read(0);
	}

	return 0;
}

static void __fastcall sub_out(UINT16 port, UINT8 data)
{
	port &= 0xff;

	if (game_select == GUZZLER || game_select == AU) { // swimmer / guzzler / au
		switch (port)
		{
			case 0x00:
		    case 0x01:
		    case 0x80:
		    case 0x81:
				AY8910Write(port>>7, ~port & 1, data);
			return;
		}

		return;
	}

	switch (port)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			AY8910Write((port&2)>>1, port & 1, data);
		return;
	}
}

static UINT8 __fastcall sub_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x04:
			return yamato_p0;
		case 0x08:
			return yamato_p1;
	}

	return 0;
}

static void __fastcall tangramq_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0x8008:
		case 0x8009:
			AY8910Write((address >> 3) & 1, address & 1, data);
		return;
	}
}

static UINT8 __fastcall tangramq_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x4000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);

	flipscreen[0] = flipscreen[1] = 0;
	interrupt_enable = 0;

	if (game_select == AU) {
		// load palram with defaults for the boot test to show
		for (INT32 i = 0;i < 0x80/2; i++) {
			UINT8 r = (i & 1) ? 7 : 0;
			UINT8 g = (i & 2) ? 7 : 0;
			UINT8 b = (i & 4) ? 7 : 0;

			DrvPalRAM[i*2+1] = r | g << 4;
			DrvPalRAM[i*2+0] = b;
		}
	}

	yamato_p0 = yamato_p1 = 0;
	swimmer_background_color = swimmer_sidebg = swimmer_palettebank = soundlatch = 0;

	ZetOpen(0);
	if (game_select == TOPROLLR) {
		bankswitch(0,0);
	}
	ZetReset();
	ZetClose();

	if (uses_sub) {
		ZetOpen(1);
		ZetReset();
		ZetClose();
	}

	AY8910Reset(0);
	AY8910Reset(1);

	HiscoreReset();

	return 0;
}

static void cclimber_sample_select_w(UINT32, UINT32 data)
{
	sample_num = data;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x0030000;
   	DrvZ80OPS		= Next; Next += 0x0030000;

	DrvGfxROM0		= Next; Next += 0x0060000;
	DrvGfxROM1		= Next; Next += 0x0060000;
	DrvGfxROM2		= Next; Next += 0x0060000;
	DrvGfxROM3		= Next; Next += 0x0060000;

	DrvColPROM		= Next; Next += 0x0000300;
	DrvDecodePROM	= Next; Next += 0x0000100;

	DrvSndROM		= Next; Next += 0x0012000;

	samplebuf       = (INT16*)Next; Next += 0x0010000 * sizeof(INT16); //decoded sample

	DrvUser1		= Next; Next += 0x0010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x0000c00;
	DrvZ80RAM1		= Next; Next += 0x0000800;
	DrvZ80RAM2		= Next; Next += 0x0000800;
	DrvZ80RAM1_0  	= Next; Next += 0x0001000;
	DrvBGSprRAM		= Next; Next += 0x0000100;
	DrvSprRAM		= Next; Next += 0x0000400;
	DrvColRAM		= Next; Next += 0x0000400;
	DrvVidRAM		= Next; Next += 0x0000400;
	if (game_select == AU) {
		DrvPalRAM	= Next; Next += 0x0000080;
	}
	if (game_select == TOPROLLR) {
		DrvBgColRAM	= Next; Next += 0x0000400;
		DrvBgVidRAM	= Next; Next += 0x0000400;
	}
	
	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(UINT8 *gfx_base, UINT8 *gfx_dest, INT32 len, INT32 size)
{
	INT32 Plane[2] = { 0, (len / 2) * 8 };
	INT32 PlaneSwimmer[3] = { 0, RGN_FRAC(len, 1, 3), RGN_FRAC(len, 2, 3) };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, gfx_base, len);
	if (game_select == GUZZLER || game_select == AU) { // swimmer, guzzler
		GfxDecode(((len * 8) / 3) / (size * size), 3, size, size, PlaneSwimmer, XOffs, YOffs, (size * size), tmp, gfx_dest);
	} else {
		GfxDecode(((len * 8) / 2) / (size * size), 2, size, size, Plane, XOffs, YOffs, (size * size), tmp, gfx_dest);
	}

	BurnFree (tmp);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0;i < 96; i++)
	{
		INT32 bit0, bit1, bit2;
		INT32 r, g, b;

		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		r = bit0 * 33 + bit1 * 71 + bit2 * 151;
		r = (int)(r + 0.5);

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		g = bit0 * 33 + bit1 * 71 + bit2 * 151;
		g = (int)(g + 0.5);

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		b = bit0 * 71 + bit1 * 151;
		b = (int)(b + 0.5);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
	if (game_select == SILVLAND) {
		DrvPalette[0x42] = BurnHighCol(0xff, 0xce, 0xce, 0);
	}
}

static void YamatoPaletteInit()
{
	const UINT8 *color_prom = DrvColPROM;
	int i;

	/* chars - 12 bits RGB */
	for (i = 0; i < 0x40; i++)
	{
		int bit0, bit1, bit2, bit3;
		int r, g, b;

		/* red component */
		bit0 = (color_prom[i + 0x00] >> 0) & 0x01;
		bit1 = (color_prom[i + 0x00] >> 1) & 0x01;
		bit2 = (color_prom[i + 0x00] >> 2) & 0x01;
		bit3 = (color_prom[i + 0x00] >> 3) & 0x01;
		r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		/* green component */
		bit0 = (color_prom[i + 0x00] >> 4) & 0x01;
		bit1 = (color_prom[i + 0x00] >> 5) & 0x01;
		bit2 = (color_prom[i + 0x00] >> 6) & 0x01;
		bit3 = (color_prom[i + 0x00] >> 7) & 0x01;
		g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		/* blue component */
		bit0 = (color_prom[i + 0x40] >> 0) & 0x01;
		bit1 = (color_prom[i + 0x40] >> 1) & 0x01;
		bit2 = (color_prom[i + 0x40] >> 2) & 0x01;
		bit3 = (color_prom[i + 0x40] >> 3) & 0x01;
		b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}

	/* big sprite - 8 bits RGB */
	for (i = 0; i < 0x20; i++)
	{
		int bit0, bit1, bit2;
		int r, g, b;

		/* red component */
		bit0 = (color_prom[i + 0x80] >> 0) & 0x01;
		bit1 = (color_prom[i + 0x80] >> 1) & 0x01;
		bit2 = (color_prom[i + 0x80] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* green component */
		bit0 = (color_prom[i + 0x80] >> 3) & 0x01;
		bit1 = (color_prom[i + 0x80] >> 4) & 0x01;
		bit2 = (color_prom[i + 0x80] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i + 0x80] >> 6) & 0x01;
		bit2 = (color_prom[i + 0x80] >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i + 0x40] = BurnHighCol(r, g, b, 0);
	}

	// BG Gradient colors are held in 0x60 - 0xe0
}

static void swimmer_set_background_pen()
{
	int bit0, bit1, bit2;
	int r, g, b;

	/* red component */
	bit0 = 0;
	bit1 = (swimmer_background_color >> 6) & 0x01;
	bit2 = (swimmer_background_color >> 7) & 0x01;
	r = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

	/* green component */
	bit0 = (swimmer_background_color >> 3) & 0x01;
	bit1 = (swimmer_background_color >> 4) & 0x01;
	bit2 = (swimmer_background_color >> 5) & 0x01;
	g = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

	/* blue component */
	bit0 = (swimmer_background_color >> 0) & 0x01;
	bit1 = (swimmer_background_color >> 1) & 0x01;
	bit2 = (swimmer_background_color >> 2) & 0x01;
	b = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

	DrvPalette[0x00] = BurnHighCol(r, g, b, 0);
}

static void SwimmerPaletteInit()
{
	const UINT8 *color_prom = DrvColPROM;
	int i;

	for (i = 0; i < 0x100; i++)
	{
		int bit0, bit1, bit2;
		int r, g, b;

		/* red component */
		bit0 = (color_prom[i + 0x000] >> 0) & 0x01;
		bit1 = (color_prom[i + 0x000] >> 1) & 0x01;
		bit2 = (color_prom[i + 0x000] >> 2) & 0x01;
		r = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		/* green component */
		bit0 = (color_prom[i + 0x000] >> 3) & 0x01;
		bit1 = (color_prom[i + 0x100] >> 0) & 0x01;
		bit2 = (color_prom[i + 0x100] >> 1) & 0x01;
		g = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i + 0x100] >> 2) & 0x01;
		bit2 = (color_prom[i + 0x100] >> 3) & 0x01;
		b = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}

	color_prom += 0x200;

	/* big sprite */
	for (i = 0; i < 0x20; i++)
	{
		int bit0, bit1, bit2;
		int r, g, b;

		/* red component */
		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		r = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		/* green component */
		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i] >> 4) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		g = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i] >> 6) & 0x01;
		bit2 = (color_prom[i] >> 7) & 0x01;
		b = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		DrvPalette[i + 0x100] = BurnHighCol(r, g, b, 0);
	}

	/* side panel backgrond pen */
	DrvPalette[0x120] = BurnHighCol(0x20, 0x98, 0x79, 0);

	swimmer_set_background_pen();
}

static void toprollrPaletteInit()
{
	UINT8 *color_prom = DrvColPROM;

	for (INT32 i = 0; i < 0xa0; i++)
	{
		int bit0, bit1, bit2;

		// red component
		bit0 = (color_prom[i] >> 0) & 1;
		bit1 = (color_prom[i] >> 1) & 1;
		bit2 = (color_prom[i] >> 2) & 1;
		int const r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		// green component
		bit0 = (color_prom[i] >> 3) & 1;
		bit1 = (color_prom[i] >> 4) & 1;
		bit2 = (color_prom[i] >> 5) & 1;
		int const g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		// blue component
		bit0 = 0;
		bit1 = (color_prom[i] >> 6) & 1;
		bit2 = (color_prom[i] >> 7) & 1;
		int const b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 GetRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *Load0 = DrvZ80ROM;
	UINT8 *Loadg0 = DrvGfxROM0;
	UINT8 *Loadg1 = DrvGfxROM2;
	UINT8 *Loadg3 = DrvGfxROM3;
	UINT8 *Loadc = DrvColPROM;
	UINT8 *Loads = DrvSndROM;
	UINT8 *LoadU = DrvUser1;
	DrvGfxROM0Len = 0;
	DrvGfxROM1Len = 0;
	INT32 samples_romlen = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 0xf) == 1) {
			if (BurnLoadRom(Load0, i, 1)) return 1;
			Load0 += ri.nLen;

			continue;
		}

		if ((ri.nType & 0xf) == 2) {
			if (ri.nType & 0x20) { // ckongo
				UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);	
				if (BurnLoadRom(tmp, i+0, 1)) return 1;		
				if (BurnLoadRom(tmp+0x1000, i+1, 1)) return 1;
				memcpy (Loadg0 + 0x0000, tmp + 0x0000, 0x0800);
				memcpy (Loadg0 + 0x1000, tmp + 0x0800, 0x0800);
				memcpy (Loadg0 + 0x0800, tmp + 0x1000, 0x0800);
				memcpy (Loadg0 + 0x1800, tmp + 0x1800, 0x0800);
				Loadg0 += 0x2000;
				DrvGfxROM0Len += 0x2000;
				i++;
				BurnFree(tmp);
				continue;
			}
		
			if (BurnLoadRom(Loadg0, i, 1)) return 1;
			if (ri.nType & 0x10) { // loads 0x800 bytes at 0x0000 and 0x1000
				UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
				memmove(tmp, Loadg0, 0x1000);
				memset(Loadg0, 0, 0x1000);
				memmove(Loadg0 + 0x00000, tmp + 0x00000, 0x800);
				memmove(Loadg0 + 0x01000, tmp + 0x00800, 0x800);
				BurnFree(tmp);
				Loadg0 += 0x2000;
				DrvGfxROM0Len += 0x2000;
			} else {
				int tmplen = (game_select == CCLIMBER || game_select == SILVLAND || game_select == CANNONB) ? 0x1000 : ri.nLen;
				Loadg0 += tmplen;
				DrvGfxROM0Len += tmplen;
			}

			continue;
		}

		if ((ri.nType & 0xf) == 3) {
			if (BurnLoadRom(Loadg1, i, 1)) return 1;
			Loadg1 += ri.nLen;
			DrvGfxROM1Len += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 4) {
			if (BurnLoadRom(LoadU, i, 1)) return 1;
			LoadU += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 6) {
			if (BurnLoadRom(Loadc, i, 1)) return 1;
			Loadc += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 7) {
			if (BurnLoadRom(Loads, i, 1)) return 1;
			Loads += ri.nLen;
			if (ri.nType & BRF_SND) samples_romlen += ri.nLen;
			if (ri.nType & BRF_PRG) uses_sub = 1;
			continue;
		}

		if ((ri.nType & 0xf) == 8) {
			if (BurnLoadRom(Loadg3, i, 1)) return 1;
			Loadg3 += ri.nLen;
			continue;
		}
	
		if ((ri.nType & 0xf) == 9) {
			if (BurnLoadRom(DrvDecodePROM, i, 1)) return 1;
			continue;
		}
	}

	if (samples_romlen == 0x2000) {
		bprintf(0, _T(" *  Game has built-in rom samples.\n"));
		uses_samples = 1;
	}

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (GetRoms()) return 1;

		DrvGfxDecode(DrvGfxROM0 + ((game_select == CANNONB) ? 0x1000 : 0), DrvGfxROM1, DrvGfxROM0Len, 16);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, DrvGfxROM0Len,  8);
		DrvGfxDecode(DrvGfxROM2, DrvGfxROM2, DrvGfxROM1Len,  8);
		DrvGfxDecode(DrvGfxROM3, DrvGfxROM3, 0x2000,  8);

		if (game_select != AU) DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);

	if (game_select == GUZZLER || game_select == AU) { // swimmer, guzzler
		ZetMapMemory(DrvZ80ROM,		    0x0000, 0x7fff, MAP_ROM);
		ZetMapMemory(DrvZ80ROM + 0x8000,0xe000, 0xffff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM0,		0x8000, 0x87ff, MAP_RAM);
		ZetMapMemory(DrvZ80RAM1,		0xc000, 0xc7ff, MAP_RAM);
		ZetMapMemory(DrvBGSprRAM,		0x8800, 0x88ff, MAP_RAM);
		ZetMapMemory(DrvBGSprRAM,		0x8900, 0x89ff, MAP_RAM); // mirror
	} else {
		ZetMapMemory(DrvZ80ROM,		    0x0000, 0x5fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM0,		0x6000, 0x6bff, MAP_RAM);
		if (game_select != TANGRAMQ) {
			ZetMapMemory(DrvZ80RAM1,		0x8000, 0x83ff, MAP_RAM);
		}
		ZetMapMemory(DrvBGSprRAM,		0x8800, 0x88ff, (game_select == CANNONB) ? MAP_WRITE : MAP_RAM);
		ZetMapMemory(DrvZ80RAM2,		0x8900, 0x8bff, MAP_RAM);
	}
	
	ZetMapMemory(DrvVidRAM,		    0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		    0x9400, 0x97ff, MAP_RAM); // mirror
	ZetMapMemory(DrvSprRAM,		    0x9800, 0x9bff, MAP_RAM); // 9800 - 981f colscroll, 9880 989f sprram, 98dc - 98df bigspritecontrol
	ZetMapMemory(DrvColRAM,		    0x9c00, 0x9fff, MAP_READ); // special write in handler

	if (game_select == TOPROLLR) {
		ZetMapMemory(DrvBgVidRAM,		    0x8c00, 0x8fff, MAP_RAM);
		ZetMapMemory(DrvBgColRAM,		    0x9400, 0x97ff, MAP_RAM);
		ZetMapMemory(DrvZ80ROM + 0xc000,	0xc000, 0xffff, MAP_ROM);
	}
	ZetSetWriteHandler(cclimber_write);
	ZetSetReadHandler(cclimber_read);
	ZetSetOutHandler(cclimber_out);
	ZetSetInHandler(cclimber_in);
	ZetClose();

	if (uses_sub) {
		ZetInit(1);
		ZetOpen(1);
		if (game_select == YAMATO) { // yamato
			ZetMapMemory(DrvSndROM,		    0x0000, 0x07ff, MAP_ROM);
			ZetMapMemory(DrvZ80RAM1_0,		0x5000, 0x53ff, MAP_RAM);
			ZetSetOutHandler(sub_out);
			ZetSetInHandler(sub_in);
		}
		if (game_select == GUZZLER || game_select == AU) { // swimmer/guzzler
			ZetMapMemory(DrvSndROM,		    0x0000, 0x0fff, MAP_ROM);
			ZetMapMemory(DrvZ80RAM1_0,		0x2000, 0x23ff, MAP_RAM);
			ZetMapMemory(DrvSndROM + 0x1000,0x4000, 0xffff, MAP_RAM);
			ZetSetReadHandler(swimmer_sub_read);
			ZetSetOutHandler(sub_out);
		}
		if (game_select == TANGRAMQ) { // tangramq
			ZetMapMemory(DrvSndROM,		    0x0000, 0x1fff, MAP_ROM);
			ZetMapMemory(DrvZ80RAM1_0,		0xe000, 0xe3ff, MAP_RAM);
			ZetSetReadHandler(tangramq_sub_read);
			ZetSetWriteHandler(tangramq_sub_write);
		}
		ZetClose();
	}

	AY8910Init(0, (game_select == GUZZLER || game_select == TANGRAMQ) ? 2000000 : 1536000, 0);
	AY8910SetPorts(0, NULL, NULL, &cclimber_sample_select_w, NULL);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910Init(1, (game_select == GUZZLER || game_select == TANGRAMQ) ? 2000000 : 1536000, 1);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, (game_select == TANGRAMQ) ? 4000000 : ((game_select == GUZZLER) ? 2000000 : 3072000));

	GenericTilesInit();

	switch (game_select)
	{
		case GUZZLER:
		case AU:
			bigsprite_index = 0xfc;
		break;

		case TOPROLLR:
			bigsprite_index = 0x1dc;
		break;
		
		default:
			bigsprite_index = 0xdc;
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFreeMemIndex();

	game_select = CCLIMBER;
	uses_sub = 0;
	uses_samples = 0;

	return 0;
}

static void cclimber_draw_bigsprite()
{
	UINT8 x = 136 - DrvSprRAM[bigsprite_index + 3];
	UINT8 y = 128 - DrvSprRAM[bigsprite_index + 2];
	INT32 flipx = (DrvSprRAM[bigsprite_index + 1] & 0x10) >> 4;
	INT32 flipy = (DrvSprRAM[bigsprite_index + 1] & 0x20) >> 5;
	INT32 bits = ((game_select == GUZZLER) || (game_select == AU)) ? 3 : 2;
	INT32 palindex = (game_select == GUZZLER) ? 0x100 : ((game_select == AU) ? 0 : 0x40);

	if (flipscreen[0] && !(game_select == CKONG || game_select == CKONGB || game_select == TANGRAMQ)) { // flipx
		flipx = !flipx;
	}

	if (flipscreen[1]) { // flipy
		y = 128 - y;
		flipy = !flipy;
	}

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		if ((offs & 0x210) != 0x210) continue; // everything else is transparent, apparently

		INT32 ofst = ((offs & 0x1e0) >> 1) | (offs & 0x0f);

		INT32 sx = ofst % 0x10;
		INT32 sy = ofst / 0x10;
		if (flipx) sx = 15 - sx;
		if (flipy) sy = 15 - sy;

		sx = (x+8*sx) & 0xff;
		sy = (y+8*sy) & 0xff;

		sy -= 16; //offsets

		INT32 code =  ((DrvSprRAM[bigsprite_index + 1] & 0x18) << 5) | DrvBGSprRAM[ofst];
		INT32 color = DrvSprRAM[bigsprite_index + 1] & 0x07;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, bits, 0, palindex, DrvGfxROM2);
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx-256, sy, color, bits, 0, palindex, DrvGfxROM2);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, bits, 0, palindex, DrvGfxROM2);
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx-256, sy, color, bits, 0, palindex, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, bits, 0, palindex, DrvGfxROM2);
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx-256, sy, color, bits, 0, palindex, DrvGfxROM2);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, bits, 0, palindex, DrvGfxROM2);
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx-256, sy, color, bits, 0, palindex, DrvGfxROM2);
			}
		}

	}
}

static void draw_playfield(INT32)
{
	INT32 bits = (game_select == GUZZLER || game_select == AU) ? 3 : 2;

	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		sy -= DrvSprRAM[sx >> 3]; // col scroll
		if (game_select == CKONG || game_select == CKONGB || game_select == TANGRAMQ) sy += 16; else sy -= 16; //offsets
		if (sy < -7) sy += 256;
		if (sx < -7) sx += 256;

		INT32 flipy = (DrvColRAM[offs] & 0x80) >> 7;
		INT32 flipx = (DrvColRAM[offs] & 0x40) >> 6;

		INT32 tile_offs = offs ^ (flipy << 5);
		
		if (flipscreen[0]) { // flipx
			sx = 248 - sx;
			flipx ^= 1;
		}

		if (flipscreen[1]) { // flipy
			sy = 248 - sy;
			flipy ^= 1;
		}

		INT32 code = DrvVidRAM[tile_offs];
		INT32 color = DrvColRAM[tile_offs] & 0x0f;

		if (game_select == GUZZLER || game_select == AU) {
			code  |= ((DrvColRAM[tile_offs] & 0x30) << 4);
			color |= (swimmer_palettebank << 4);
		} else {
			code  |= ((DrvColRAM[tile_offs] & 0x10) << 5) + ((DrvColRAM[tile_offs] & 0x20) << 3);
		}

		if (sx > nScreenWidth || sy > nScreenHeight) continue;

		Draw8x8MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, bits, 0, 0, DrvGfxROM0);
	}
}

inline static UINT32 CalcCol(UINT16 nColour)
{
	INT32 r, g, b;

	r = pal3bit(nColour >> 0);
	g = pal3bit(nColour >> 4);
	b = pal3bit(nColour >> 8);

	return BurnHighCol(r, g, b, 0);
}

static void AuPaletteUpdate()
{
	for (INT32 i = 0; i < 0x80; i += 2)
	{
		DrvPalette[i/2] = CalcCol(DrvPalRAM[i | 1] | (DrvPalRAM[i & ~1] << 8));
	}
}

static void draw_sprites()
{
	INT32 bits = (game_select == GUZZLER || game_select == AU) ? 3 : 2;
	INT32 start = (game_select == TOPROLLR) ? 0x15c : 0x09c;

	for (INT32 offs = start; offs >= 0x80; offs -= 4)
	{
		INT32 x = DrvSprRAM[offs + 3];
		INT32 y = 240 - DrvSprRAM[offs + 2];

		if (game_select == CKONG || game_select == CKONGB || game_select == TANGRAMQ) y += 16; else y -= 16; //offsets

		INT32 code =  DrvSprRAM[offs + 0] & 0x3f;
		INT32 color = DrvSprRAM[offs + 1] & 0x0f;

		if (game_select == GUZZLER || game_select == AU) {
			code  |= ((DrvSprRAM[offs + 1] & 0x30) << 2);
			color |= (swimmer_palettebank << 4);
		} else {
			code  |= ((DrvSprRAM[offs + 1] & 0x10) << 3) | ((DrvSprRAM[offs + 1] & 0x20) << 1);
		}

		INT32 flipx = DrvSprRAM[offs + 0] & 0x40;
		INT32 flipy = DrvSprRAM[offs + 0] & 0x80;

		if (flipscreen[0] & 1)
		{
			x = 240 - x;
			flipx = !flipx;
		}

		if (flipscreen[1] & 1)
		{
			y = 240 - y;
			flipy = !flipy;
		}

		Draw16x16MaskTile(pTransDraw, code, x, y, flipx, flipy, color, bits, 0, 0, DrvGfxROM1);
	}
}

#define SWIMMER_SIDE_BG_PEN (0x120)
#define SWIMMER_BG_PEN      (0x00)
#define SWIMMER_BG_SPLIT    (0x18 * 8)

static void swimmer_draw_backdrop() // background effects for swimmer/guzzler
{
	swimmer_set_background_pen();

	if (swimmer_sidebg & 1) {
		for (INT32 y = 0; y < nScreenHeight; y++)
			for (INT32 x = 0; x < nScreenWidth; x++) {
				if (flipscreen[0] & 1) {
					if (x <= 0xff - SWIMMER_BG_SPLIT) {
						pTransDraw[(y*nScreenWidth) + x] = SWIMMER_SIDE_BG_PEN;
					} else {
						pTransDraw[(y*nScreenWidth) + x] = SWIMMER_BG_PEN;
					}
				} else {
					if (x <= SWIMMER_BG_SPLIT - 1) {
						pTransDraw[(y*nScreenWidth) + x] = SWIMMER_BG_PEN;
					} else {
						pTransDraw[(y*nScreenWidth) + x] = SWIMMER_SIDE_BG_PEN;
					}
				}
			}
	} else { // just fill bg
		for (INT32 y = 0; y < nScreenHeight; y++)
			for (INT32 x = 0; x < nScreenWidth; x++) {
				pTransDraw[(y*nScreenWidth) + x] = SWIMMER_BG_PEN;
			}
	}
}

static void yamato_draw_backdrop()
{
	UINT16 bank = (yamato_bg[2] << 2 | yamato_bg[1] << 1 | yamato_bg[0]) << 8;
	bank |= (flipscreen[0] ? 0x80 : 0);

	for (INT32 i = 0; i < 0x80; i++) {
		UINT8 data0 = DrvUser1[0x0000 | bank | i];
		UINT8 data1 = DrvUser1[0x1000 | bank | i];

		UINT8 r = pal5bit(data0 & 0x1f);
		UINT8 g = pal5bit(data0 >> 5 | (data1 << 3 & 0x18));
		UINT8 b = pal6bit(data1 >> 2);

		DrvPalette[i + 0x60] = BurnHighCol(r, g, b, 0);

		for (int y = 0; y < nScreenHeight; y++) {
			int start = (i * 2 - 8) & 0xff;
			for (int x = start; x < start + 2; x++) {
				if (x >= 0 && x < nScreenWidth) {
					pTransDraw[y * nScreenWidth + x] = 0x60 + i;
				}
			}
		}
	}
}

static void toprollr_draw_backdrop()
{
	for (INT32 offs = 64; offs < 32 * 30; offs++)
	{
		INT32 sx = ((offs & 0x1f) * 8) - DrvBgVidRAM[0];
		INT32 sy = ((offs >> 5) * 8) - 16;
		
		if (sx < -7) sx += 256;
	
		INT32 code = ((DrvBgColRAM[offs] & 0x40) << 2) | DrvBgVidRAM[offs];
		INT32 color = DrvBgColRAM[offs] & 0xf;
		
		Draw8x8Tile(pTransDraw, code, sx, sy, 1, 0, color, 2, 0x60, DrvGfxROM3);
		if (sx < 0) Draw8x8Tile(pTransDraw, code, sx + 256, sy, 1, 0, color, 2, 0x60, DrvGfxROM3);
	}
}

static INT32 ToprollrDraw()
{
	if (DrvRecalc) {
		toprollrPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	toprollr_draw_backdrop();

	if (DrvSprRAM[bigsprite_index + 1] & 0x20) {
		if (nBurnLayer & 4) draw_sprites();
		if (nBurnLayer & 2) cclimber_draw_bigsprite();
	} else {
		if (nBurnLayer & 2) cclimber_draw_bigsprite();
		if (nBurnLayer & 4) draw_sprites();
	}

	for (INT32 y = 0; y < 224; y++) {
		memset (pTransDraw + y * nScreenWidth, 0, 32*2);
		memset (pTransDraw + y * nScreenWidth + (nScreenWidth - 24), 0, 24*2);
	}

	if (nBurnLayer & 1) draw_playfield(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		switch (game_select)
		{
			case GUZZLER:  SwimmerPaletteInit(); break;
			case YAMATO:   YamatoPaletteInit(); break;
			default: DrvPaletteInit();
		}
		DrvRecalc = 0;
	}

	if (game_select == AU) {
		AuPaletteUpdate();
	}

	BurnTransferClear();

	switch (game_select)
	{
		case GUZZLER:  swimmer_draw_backdrop(); break;
		case YAMATO:   yamato_draw_backdrop(); break;
	}

	if (nBurnLayer & 1) draw_playfield(0);

	if (DrvSprRAM[bigsprite_index] & 0x01) {
		if (nBurnLayer & 2) cclimber_draw_bigsprite();
		if (nBurnLayer & 4) draw_sprites();
	} else {
		if (nBurnLayer & 4) draw_sprites();
		if (nBurnLayer & 2) cclimber_draw_bigsprite();
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

	{
		UINT32 JoyInit[4] = { 0x00, 0x00, 0x00, 0x00 };
		UINT8 *DrvJoys[4] = { DrvJoy1, DrvJoy2, DrvJoy3, DrvJoy4 };

		CompileInput(DrvJoys, (void*)DrvInputs, 4, 8, JoyInit);

		if (game_select == CKONG || game_select == CKONGB) {
			DrvInputs[2] = ~DrvInputs[2];
		}

		if (game_select == TANGRAMQ) {
			// tangramq: these 2 are active low
			DrvInputs[2] = ~DrvInputs[2];
			DrvInputs[3] = ~DrvInputs[3];
		}

		if (game_select == TOPROLLR) {
			DrvInputs[2] = ~DrvInputs[2];
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3072000 / 60, ((game_select == TANGRAMQ) ? 4000000 : ((game_select == GUZZLER) ? 2000000 : 3072000)) / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == nInterleave - 1 && interrupt_enable) {
			if (game_select == BAGMANF)
				ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
			else
				ZetNmi();
		}
		ZetClose();

		if (uses_sub) {
			ZetOpen(1);
			CPU_RUN(1, Zet);
			if ((game_select == GUZZLER || game_select == TANGRAMQ) && (i%63==0)) // 4x per frame
				ZetNmi();
			if (i == nInterleave - 1 && game_select == AU)
				ZetNmi();
			ZetClose();
		}
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		cclimber_render(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029735;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(interrupt_enable);
		SCAN_VAR(yamato_p0);
		SCAN_VAR(yamato_p1);
		SCAN_VAR(yamato_bg);
		SCAN_VAR(swimmer_background_color);
		SCAN_VAR(swimmer_sidebg);
		SCAN_VAR(swimmer_palettebank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(sample_num);
		SCAN_VAR(sample_freq);
		SCAN_VAR(sample_vol);
		SCAN_VAR(sample_len);
		SCAN_VAR(sample_pos);
		SCAN_VAR(bankdata);
	}
	
	if (nAction & ACB_WRITE) {
		if (game_select == TOPROLLR) {
			INT32 data = bankdata;
			ZetOpen(0);
			bankswitch(5, data & 1);
			bankswitch(6, (data >> 1) & 1);	
			ZetClose();
		}
	}

	return 0;
}


// Crazy Climber (US set 1)

static struct BurnRomInfo cclimberRomDesc[] = {
	{ "cc11",			0x1000, 0x217ec4ff, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "cc10",			0x1000, 0xb3c26cef, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cc09",			0x1000, 0x6db0879c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cc08",			0x1000, 0xf48c5fe3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cc07",			0x1000, 0x3e873baf, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "cc06",			0x0800, 0x481b64cc, 2 | BRF_GRA },	     	 //  5 Sprites & Tiles
	{ "cc05",			0x0800, 0x2c33b760, 2 | BRF_GRA },	     	 //  6
	{ "cc04",			0x0800, 0x332347cb, 2 | BRF_GRA },	     	 //  7
	{ "cc03",			0x0800, 0x4e4b3658, 2 | BRF_GRA },	     	 //  8

	{ "cc02",			0x0800, 0x14f3ecc9, 3 | BRF_GRA },	     	 //  9 Big Sprites
	{ "cc01",			0x0800, 0x21c0f9fb, 3 | BRF_GRA },	     	 // 10

	{ "cclimber.pr1", 	0x0020, 0x751c3325, 6 | BRF_GRA },	     	 // 11 Color Proms
	{ "cclimber.pr2", 	0x0020, 0xab1940fa, 6 | BRF_GRA },	     	 // 12
	{ "cclimber.pr3", 	0x0020, 0x71317756, 6 | BRF_GRA },	     	 // 13 

	{ "cc13",			0x1000, 0xe0042f75, 7 | BRF_SND },	     	 // 14 Samples
	{ "cc12",			0x1000, 0x5da13aaa, 7 | BRF_SND },	     	 // 15
	
	{ "dm7052.cpu",		0x0100, 0xf4179117, 9 | BRF_PRG | BRF_ESS }, // 16 Decryption Table
};

STD_ROM_PICK(cclimber)
STD_ROM_FN(cclimber)

static void cclimber_decrypt()
{
	for (INT32 A = 0; A < 0x6000; A++)
	{
		UINT8 src = DrvZ80ROM[A];

		// pick the offset in the table from bit 0 of the address and bits 0 1 2 4 6 7 of the source data
		INT32 j = (src & 0x01) | ((src & 0x04) >> 1) | ((src & 0x10) >> 1) | ((src & 0x40) >> 4) | ((A & 1) << 6) | ((src & 0x02) << 3) | ((src & 0x80) >> 2);

		UINT8 prm = DrvDecodePROM[j];

		// decode the opcodes
		DrvZ80OPS[A] = (src & 0xaa) | (prm & 0x01) | ((prm & 0x02) << 1) | ((prm & 0x04) << 4) | ((prm & 0x08) << 1);
	}

	ZetOpen(0);
	ZetMapArea(0x0000, 0x5fff, 2, DrvZ80OPS, DrvZ80ROM);
	ZetClose();
}

static INT32 cclimberInit()
{
	game_select = CCLIMBER;

	INT32 nRet = DrvInit();

	if (nRet == 0) {
		cclimber_decrypt();
	}

	return nRet;
}

struct BurnDriver BurnDrvCclimber = {
	"cclimber", NULL, NULL, NULL, "1980",
	"Crazy Climber (US set 1)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, cclimberRomInfo, cclimberRomName, NULL, NULL, NULL, NULL, CclimberInputInfo, CclimberDIPInfo,
	cclimberInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	256, 224, 4, 3
};


// Crazy Climber (US set 2)

static struct BurnRomInfo cclimberaRomDesc[] = {
	{ "cc11",			0x1000, 0x217ec4ff, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "10",				0x1000, 0x983d0bab, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cc9",			0x1000, 0x6db0879c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cc8",			0x1000, 0xf48c5fe3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "7",				0x1000, 0xc2e06606, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "cc6",			0x0800, 0x481b64cc, 2 | BRF_GRA },	     	 //  5 Sprites & Tiles
	{ "cc5",			0x0800, 0x2c33b760, 2 | BRF_GRA },	     	 //  6
	{ "cc4",			0x0800, 0x332347cb, 2 | BRF_GRA },	     	 //  7
	{ "cc3",			0x0800, 0x4e4b3658, 2 | BRF_GRA },	     	 //  8

	{ "cc2",			0x0800, 0x14f3ecc9, 3 | BRF_GRA },	     	 //  9 Big Sprites
	{ "cc1",			0x0800, 0x21c0f9fb, 3 | BRF_GRA },	     	 // 10

	{ "cclimber.pr1", 	0x0020, 0x751c3325, 6 | BRF_GRA },	     	 // 11 Color Proms
	{ "cclimber.pr2", 	0x0020, 0xab1940fa, 6 | BRF_GRA },	     	 // 12
	{ "cclimber.pr3", 	0x0020, 0x71317756, 6 | BRF_GRA },	     	 // 13 

	{ "cc13",			0x1000, 0xe0042f75, 7 | BRF_SND },	     	 // 14 Samples
	{ "cc12",			0x1000, 0x5da13aaa, 7 | BRF_SND },	     	 // 15
	
	{ "dm7052.cpu",		0x0100, 0xf4179117, 9 | BRF_PRG | BRF_ESS }, // 16 Decryption Table
};

STD_ROM_PICK(cclimbera)
STD_ROM_FN(cclimbera)

struct BurnDriver BurnDrvCclimbera = {
	"cclimbera", "cclimber", NULL, NULL, "1980",
	"Crazy Climber (US set 2)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, cclimberaRomInfo, cclimberaRomName, NULL, NULL, NULL, NULL, CclimberInputInfo, CclimberDIPInfo,
	cclimberInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	256, 224, 4, 3
};


// Crazy Climber (Japan)

static struct BurnRomInfo cclimberjRomDesc[] = {
	{ "cc11j.bin",		0x1000, 0x89783959, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "cc10j.bin",		0x1000, 0x14eda506, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cc09j.bin",		0x1000, 0x26489069, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cc08j.bin",		0x1000, 0xb33c96f8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cc07j.bin",		0x1000, 0xfbc9626c, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "cc06",			0x0800, 0x481b64cc, 2 | BRF_GRA },	     	 //  5 Sprites & Tiles
	{ "cc05",			0x0800, 0x2c33b760, 2 | BRF_GRA },	     	 //  6
	{ "cc04",			0x0800, 0x332347cb, 2 | BRF_GRA },	     	 //  7
	{ "cc03",			0x0800, 0x4e4b3658, 2 | BRF_GRA },	     	 //  8

	{ "cc02",			0x0800, 0x14f3ecc9, 3 | BRF_GRA },	     	 //  9 Big Sprites
	{ "cc01",			0x0800, 0x21c0f9fb, 3 | BRF_GRA },	     	 // 10

	{ "cclimber.pr1", 	0x0020, 0x751c3325, 6 | BRF_GRA },	     	 // 11 Color Proms
	{ "cclimber.pr2", 	0x0020, 0xab1940fa, 6 | BRF_GRA },	     	 // 12
	{ "cclimber.pr3", 	0x0020, 0x71317756, 6 | BRF_GRA },	     	 // 13 

	{ "cc13j.bin",		0x1000, 0x5f0bcdfb, 7 | BRF_SND },	     	 // 14 Samples
	{ "cc12j.bin",		0x1000, 0x9003ffbd, 7 | BRF_SND },	     	 // 15
	
	{ "ccboot.prm", 	0x0100, 0x9e11550d, 9 | BRF_PRG | BRF_ESS }, // 16 Decryption Table
};

STD_ROM_PICK(cclimberj)
STD_ROM_FN(cclimberj)

struct BurnDriver BurnDrvCclimberj = {
	"cclimberj", "cclimber", NULL, NULL, "1980",
	"Crazy Climber (Japan)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, cclimberjRomInfo, cclimberjRomName, NULL, NULL, NULL, NULL, CclimberInputInfo, CclimberjDIPInfo,
	cclimberInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	256, 224, 4, 3
};


// Crazy Climber (bootleg set 1)

static struct BurnRomInfo ccbootRomDesc[] = {
	{ "m11.bin",		0x1000, 0x5efbe180, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "m10.bin",		0x1000, 0xbe2748c7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cc09j.bin",		0x1000, 0x26489069, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "m08.bin",		0x1000, 0xe3c542d6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cc07j.bin",		0x1000, 0xfbc9626c, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "cc06",			0x0800, 0x481b64cc, 2 | BRF_GRA },           //  5 Sprites & Tiles
	{ "m05.bin",		0x0800, 0x056af36b, 2 | BRF_GRA },           //  6
	{ "m04.bin",		0x0800, 0x6fb80538, 2 | BRF_GRA },           //  7
	{ "m03.bin",		0x0800, 0x67127253, 2 | BRF_GRA },           //  8

	{ "m02.bin",		0x0800, 0x7f4877de, 3 | BRF_GRA },           //  9 Big Sprites
	{ "m01.bin",		0x0800, 0x49fab908, 3 | BRF_GRA },           // 10

	{ "cclimber.pr1",	0x0020, 0x751c3325, 6 | BRF_GRA },           // 11 Color Proms
	{ "cclimber.pr2",	0x0020, 0xab1940fa, 6 | BRF_GRA },           // 12
	{ "cclimber.pr3",	0x0020, 0x71317756, 6 | BRF_GRA },           // 13

	{ "cc13j.bin",		0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 14 Samples
	{ "cc12j.bin",		0x1000, 0x9003ffbd, 7 | BRF_SND },           // 15

	{ "ccboot.prm",		0x0100, 0x9e11550d, 9 | BRF_PRG | BRF_ESS }, // 16 Decryption Table
};

STD_ROM_PICK(ccboot)
STD_ROM_FN(ccboot)

struct BurnDriver BurnDrvCcboot = {
	"ccboot", "cclimber", NULL, NULL, "1980",
	"Crazy Climber (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ccbootRomInfo, ccbootRomName, NULL, NULL, NULL, NULL, CclimberInputInfo, CclimberjDIPInfo,
	cclimberInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	256, 224, 4, 3
};


// Crazy Climber (bootleg set 2)

static struct BurnRomInfo ccboot2RomDesc[] = {
	{ "11.4k",			0x1000, 0xb2b17e24, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "10.4j",			0x1000, 0x8382bc0f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cc09j.bin",		0x1000, 0x26489069, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "m08.bin",		0x1000, 0xe3c542d6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cc07j.bin",		0x1000, 0xfbc9626c, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "cc06",			0x0800, 0x481b64cc, 2 | BRF_GRA },           //  5 Sprites & Tiles
	{ "cc05",			0x0800, 0x2c33b760, 2 | BRF_GRA },           //  6
	{ "cc04",			0x0800, 0x332347cb, 2 | BRF_GRA },           //  7
	{ "cc03",			0x0800, 0x4e4b3658, 2 | BRF_GRA },           //  8

	{ "cc02",			0x0800, 0x14f3ecc9, 3 | BRF_GRA },           //  9 Big Sprites
	{ "cc01",			0x0800, 0x21c0f9fb, 3 | BRF_GRA },           // 10

	{ "cclimber.pr1",	0x0020, 0x751c3325, 6 | BRF_GRA },           // 11 Color Proms
	{ "cclimber.pr2",	0x0020, 0xab1940fa, 6 | BRF_GRA },           // 12
	{ "cclimber.pr3",	0x0020, 0x71317756, 6 | BRF_GRA },           // 13

	{ "cc13j.bin",		0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 14 Samples
	{ "cc12j.bin",		0x1000, 0x9003ffbd, 7 | BRF_SND },           // 15

	{ "ccboot.prm",		0x0100, 0x9e11550d, 9 | BRF_PRG | BRF_ESS }, // 16 Decryption Table
};

STD_ROM_PICK(ccboot2)
STD_ROM_FN(ccboot2)

struct BurnDriver BurnDrvCcboot2 = {
	"ccboot2", "cclimber", NULL, NULL, "1980",
	"Crazy Climber (bootleg set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ccboot2RomInfo, ccboot2RomName, NULL, NULL, NULL, NULL, CclimberInputInfo, CclimberjDIPInfo,
	cclimberInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	256, 224, 4, 3
};


// Crazy Climber (ManilaMatic bootleg)

static struct BurnRomInfo ccbootmmRomDesc[] = {
	{ "5_mm",			0x1000, 0xa7e3450a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "4_mm",			0x1000, 0xaa89d255, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3_mm",			0x1000, 0x26489069, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2_mm",			0x1000, 0x19205c51, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1_mm",			0x1000, 0x499c0625, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "13_mm",			0x0800, 0x2f303f40, 2 | BRF_GRA },           //  5 Sprites & Tiles
	{ "12_mm",			0x0800, 0x056af36b, 2 | BRF_GRA },           //  6
	{ "11_mm",			0x0800, 0x850dbb52, 2 | BRF_GRA },           //  7
	{ "10_mm",			0x0800, 0x71fb3ed9, 2 | BRF_GRA },           //  8

	{ "9_mm",			0x0800, 0x943858c2, 3 | BRF_GRA },           //  9 Big Sprites
	{ "8_mm",			0x0800, 0x76d75e83, 3 | BRF_GRA },           // 10

	{ "cclimber.pr1",	0x0020, 0x751c3325, 6 | BRF_GRA },           // 11 Color Proms
	{ "cclimber.pr2",	0x0020, 0xab1940fa, 6 | BRF_GRA },           // 12
	{ "cclimber.pr3",	0x0020, 0xb4e827a5, 6 | BRF_GRA },           // 13

	{ "mm_7",			0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 14 Samples
	{ "mm_6",			0x1000, 0x9003ffbd, 7 | BRF_SND },           // 15

	{ "ccboot.prm",		0x0100, 0x9e11550d, 9 | BRF_PRG | BRF_ESS }, // 16 Decryption Table
};

STD_ROM_PICK(ccbootmm)
STD_ROM_FN(ccbootmm)

struct BurnDriver BurnDrvCcbootmm = {
	"ccbootmm", "cclimber", NULL, NULL, "1980",
	"Crazy Climber (ManilaMatic bootleg)\0", NULL, "bootleg (ManilaMatic)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ccbootmmRomInfo, ccbootmmRomName, NULL, NULL, NULL, NULL, CclimberInputInfo, CclimberjDIPInfo,
	cclimberInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	256, 224, 4, 3
};


// Crazy Climber (Model Racing bootleg)

static struct BurnRomInfo ccbootmrRomDesc[] = {
	{ "211.k4",			0x1000, 0xb2b17e24, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "210.j4",			0x1000, 0x8382bc0f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "209.f4",			0x1000, 0x26489069, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "208.e4",			0x1000, 0xe3c542d6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "207.c4",			0x1000, 0xfbc9626c, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "cc06",			0x0800, 0x481b64cc, 2 | BRF_GRA },           //  5 Sprites & Tiles
	{ "cc05",			0x0800, 0x2c33b760, 2 | BRF_GRA },           //  6
	{ "cc04",			0x0800, 0x332347cb, 2 | BRF_GRA },           //  7
	{ "cc03",			0x0800, 0x4e4b3658, 2 | BRF_GRA },           //  8

	{ "202.c6",			0x0800, 0x5ec87c50, 3 | BRF_GRA },           //  9 Big Sprites
	{ "201.a6",			0x0800, 0x76d6d9a4, 3 | BRF_GRA },           // 10

	{ "cclimber.pr1",	0x0020, 0x751c3325, 6 | BRF_GRA },           // 11 Color Proms
	{ "cclimber.pr2",	0x0020, 0xab1940fa, 6 | BRF_GRA },           // 12
	{ "198-74288.c9",	0x0020, 0xb4e827a5, 6 | BRF_GRA },           // 13

	{ "213.r4",			0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 15 Samples
	{ "212.n4",			0x1000, 0x9003ffbd, 7 | BRF_SND },           // 16

	{ "214-74187.cpu",	0x0100, 0x9e11550d, 9 | BRF_PRG | BRF_ESS }, // 17 Decryption Table
};

STD_ROM_PICK(ccbootmr)
STD_ROM_FN(ccbootmr)

struct BurnDriver BurnDrvCcbootmr = {
	"ccbootmr", "cclimber", NULL, NULL, "1980",
	"Crazy Climber (Model Racing bootleg)\0", NULL, "bootleg (Model Racing)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ccbootmrRomInfo, ccbootmrRomName, NULL, NULL, NULL, NULL, CclimberInputInfo, CclimberjDIPInfo,
	cclimberInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	256, 224, 4, 3
};


// Crazy Climber (Spanish, Operamatic bootleg)

static struct BurnRomInfo cclimbroperRomDesc[] = {
	{ "cc5-2532.cpu",	0x1000, 0xf94b96e8, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "cc4-2532.cpu",	0x1000, 0x4b1abea6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cc3-2532.cpu",	0x1000, 0x5612bb3c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cc2-2532.cpu",	0x1000, 0x653cebc4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cc1-2532.cpu",	0x1000, 0x3fcf912b, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "cc13-2716.cpu",	0x0800, 0x9324846d, 2 | BRF_GRA },           //  5 Sprites & Tiles
	{ "cc12-2716.cpu",	0x0800, 0x6d15ba36, 2 | BRF_GRA },           //  6
	{ "cc11-2716.cpu",	0x0800, 0x25886f13, 2 | BRF_GRA },           //  7
	{ "cc10-2716.cpu",	0x0800, 0x437471ec, 2 | BRF_GRA },           //  8

	{ "cc9-2716.cpu",	0x0800, 0xa546a18f, 3 | BRF_GRA },           //  9 Big Sprites
	{ "cc8-2716.cpu",	0x0800, 0x0224e507, 3 | BRF_GRA },           // 10

	{ "cclimber.pr1",	0x0020, 0x751c3325, 6 | BRF_GRA },           // 11 Color Proms
	{ "cclimber.pr2",	0x0020, 0xab1940fa, 6 | BRF_GRA },           // 12
	{ "cclimber.pr3",	0x0020, 0x71317756, 6 | BRF_GRA },           // 13

	{ "cc7-2532.cpu",	0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 14 Samples
	{ "cc6-2532.cpu",	0x1000, 0x9003ffbd, 7 | BRF_SND },           // 15
};

STD_ROM_PICK(cclimbroper)
STD_ROM_FN(cclimbroper)

static INT32 cclimbroperInit()
{
	game_select = CCLIMBER;

	return DrvInit();
}

struct BurnDriver BurnDrvCclimbroper = {
	"cclimbroper", "cclimber", NULL, NULL, "1980",
	"Crazy Climber (Spanish, Operamatic bootleg)\0", NULL, "bootleg (Operamatic)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, cclimbroperRomInfo, cclimbroperRomName, NULL, NULL, NULL, NULL, CclimberInputInfo, CclimberjDIPInfo,
	cclimbroperInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	256, 224, 4, 3
};


// Crazy Climber (Spanish, Rodmar bootleg)

static struct BurnRomInfo cclimbrrodRomDesc[] = {
	{ "cc5.bin",		0x1000, 0xa67238e9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "cc4.bin",		0x1000, 0x4b1abea6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cc3.bin",		0x1000, 0x5612bb3c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cc2.bin",		0x1000, 0x653cebc4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cc1.bin",		0x1000, 0x3fcf912b, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "cc13.bin",		0x0800, 0x8e0299f5, 2 | BRF_GRA },           //  5 Sprites & Tiles
	{ "cc12.bin",		0x0800, 0xe8cd7b53, 2 | BRF_GRA },           //  6
	{ "cc11.bin",		0x0800, 0x921ebd9a, 2 | BRF_GRA },           //  7
	{ "cc10.bin",		0x0800, 0x8ab5fa6b, 2 | BRF_GRA },           //  8

	{ "cc9.bin",		0x0800, 0x6fb2afaf, 3 | BRF_GRA },           //  9 Big Sprites
	{ "cc8.bin",		0x0800, 0x227ee804, 3 | BRF_GRA },           // 10

	{ "cclimber.pr1",	0x0020, 0x751c3325, 6 | BRF_GRA },           // 11 Color Proms
	{ "cclimber.pr2",	0x0020, 0xab1940fa, 6 | BRF_GRA },           // 12
	{ "cclimber.pr3",	0x0020, 0x71317756, 6 | BRF_GRA },           // 13

	{ "cc7.cpu",		0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 14 Samples
	{ "cc6.cpu",		0x1000, 0x9003ffbd, 7 | BRF_SND },           // 15
};

STD_ROM_PICK(cclimbrrod)
STD_ROM_FN(cclimbrrod)

struct BurnDriver BurnDrvCclimbrrod = {
	"cclimbrrod", "cclimber", NULL, NULL, "1980",
	"Crazy Climber (Spanish, Rodmar bootleg)\0", NULL, "bootleg (Rodmar)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, cclimbrrodRomInfo, cclimbrrodRomName, NULL, NULL, NULL, NULL, CclimberInputInfo, CclimberjDIPInfo,
	cclimbroperInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	256, 224, 4, 3
};


// Crazy Kong Part II (set 1)

static struct BurnRomInfo ckongpt2RomDesc[] = {
	{ "7.5d",			0x1000, 0xb27df032, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "8.5e",			0x1000, 0x5dc1aaba, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "9.5h",			0x1000, 0xc9054c94, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "10.5k",			0x1000, 0x069c4797, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "11.5l",			0x1000, 0xae159192, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "12.5n",			0x1000, 0x966bc9ab, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "6.11n",			0x1000, 0x2dcedd12, 2 | BRF_GRA },           //  6 Sprites & Tiles
	{ "5.11l",			0x1000, 0xfa7cbd91, 2 | BRF_GRA },           //  7
	{ "4.11k",			0x1000, 0x3375b3bd, 2 | BRF_GRA },           //  8
	{ "3.11h",			0x1000, 0x5655cc11, 2 | BRF_GRA },           //  9

	{ "2.11c",			0x0800, 0xd1352c31, 3 | BRF_GRA },           // 10 Big Sprites
	{ "1.11a",			0x0800, 0xa7a2fdbd, 3 | BRF_GRA },           // 11

	{ "prom.v6",		0x0020, 0xb3fc1505, 6 | BRF_GRA },           // 12 Color PROMs
	{ "prom.u6",		0x0020, 0x26aada9e, 6 | BRF_GRA },           // 13
	{ "prom.t6",		0x0020, 0x676b3166, 6 | BRF_GRA },           // 14

	{ "14.5s",			0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 15 Samples
	{ "13.5p",			0x1000, 0x9003ffbd, 7 | BRF_SND },           // 16
};

STD_ROM_PICK(ckongpt2)
STD_ROM_FN(ckongpt2)

static INT32 ckongInit()
{
	game_select = CKONG;

	return DrvInit();
}

struct BurnDriver BurnDrvCkongpt2 = {
	"ckongpt2", NULL, NULL, NULL, "1981",
	"Crazy Kong Part II (set 1)\0", NULL, "Falcon", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongpt2RomInfo, ckongpt2RomName, NULL, NULL, NULL, NULL, CkongInputInfo, CkongDIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Crazy Kong Part II (set 2)

static struct BurnRomInfo ckongpt2aRomDesc[] = {
	{ "7.5d",			0x1000, 0xb27df032, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "8.5e",			0x1000, 0x5dc1aaba, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "9.5h",			0x1000, 0xc9054c94, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "10.dat",			0x1000, 0xc3beb501, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "11.5l",			0x1000, 0xae159192, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "12.5n",			0x1000, 0x966bc9ab, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "6.11n",			0x1000, 0x2dcedd12, 2 | BRF_GRA },           //  6 Sprites & Tiles
	{ "5.11l",			0x1000, 0xfa7cbd91, 2 | BRF_GRA },           //  7
	{ "4.11k",			0x1000, 0x3375b3bd, 2 | BRF_GRA },           //  8
	{ "3.11h",			0x1000, 0x5655cc11, 2 | BRF_GRA },           //  9

	{ "2.11c",			0x0800, 0xd1352c31, 3 | BRF_GRA },           // 10 Big Sprites
	{ "1.11a",			0x0800, 0xa7a2fdbd, 3 | BRF_GRA },           // 11

	{ "prom.v6",		0x0020, 0xb3fc1505, 6 | BRF_GRA },           // 12 Color PROMs
	{ "prom.u6",		0x0020, 0x26aada9e, 6 | BRF_GRA },           // 13
	{ "prom.t6",		0x0020, 0x676b3166, 6 | BRF_GRA },           // 14

	{ "14.5s",			0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 15 Samples
	{ "13.5p",			0x1000, 0x9003ffbd, 7 | BRF_SND },           // 16
};

STD_ROM_PICK(ckongpt2a)
STD_ROM_FN(ckongpt2a)

struct BurnDriver BurnDrvCkongpt2a = {
	"ckongpt2a", "ckongpt2", NULL, NULL, "1981",
	"Crazy Kong Part II (set 2)\0", NULL, "Falcon", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongpt2aRomInfo, ckongpt2aRomName, NULL, NULL, NULL, NULL, CkongInputInfo, CkongDIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Crazy Kong Part II (Japan)

static struct BurnRomInfo ckongpt2jRomDesc[] = {
	{ "7.5d",			0x1000, 0xb27df032, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "8.5e",			0x1000, 0x5dc1aaba, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "9.5h",			0x1000, 0xc9054c94, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "10.dat",			0x1000, 0xc3beb501, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "11.5l",			0x1000, 0x4164eb4d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "12.5n",			0x1000, 0x966bc9ab, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "6.11n",			0x1000, 0x2dcedd12, 2 | BRF_GRA },           //  6 Sprites & Tiles
	{ "5.11l",			0x1000, 0xfa7cbd91, 2 | BRF_GRA },           //  7
	{ "4.11k",			0x1000, 0x3375b3bd, 2 | BRF_GRA },           //  8
	{ "3.11h",			0x1000, 0x5655cc11, 2 | BRF_GRA },           //  9

	{ "2.11c",			0x0800, 0xd1352c31, 3 | BRF_GRA },           // 10 Big Sprites
	{ "1.11a",			0x0800, 0xa7a2fdbd, 3 | BRF_GRA },           // 11

	{ "prom.v6",		0x0020, 0xb3fc1505, 6 | BRF_GRA },           // 12 Color PROMs
	{ "prom.u6",		0x0020, 0x26aada9e, 6 | BRF_GRA },           // 13
	{ "prom.t6",		0x0020, 0x676b3166, 6 | BRF_GRA },           // 14

	{ "14.5s",			0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 15 Samples
	{ "13.5p",			0x1000, 0x9003ffbd, 7 | BRF_SND },           // 16
};

STD_ROM_PICK(ckongpt2j)
STD_ROM_FN(ckongpt2j)

struct BurnDriver BurnDrvCkongpt2j = {
	"ckongpt2j", "ckongpt2", NULL, NULL, "1981",
	"Crazy Kong Part II (Japan)\0", NULL, "Falcon", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongpt2jRomInfo, ckongpt2jRomName, NULL, NULL, NULL, NULL, CkongInputInfo, CkongDIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Crazy Kong Part II (Jeutel bootleg)

static struct BurnRomInfo ckongpt2jeuRomDesc[] = {
	{ "7.5d",			0x1000, 0xb27df032, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "8.5e",			0x1000, 0x5dc1aaba, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "9.5h",			0x1000, 0xc9054c94, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ckjeu10.dat",	0x1000, 0x7e6eeec4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "11.5l",			0x1000, 0xae159192, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ckjeu12.dat",	0x1000, 0x0532f270, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "6.11n",			0x1000, 0x2dcedd12, 2 | BRF_GRA },           //  6 Sprites & Tiles
	{ "5.11l",			0x1000, 0xfa7cbd91, 2 | BRF_GRA },           //  7
	{ "4.11k",			0x1000, 0x3375b3bd, 2 | BRF_GRA },           //  8
	{ "3.11h",			0x1000, 0x5655cc11, 2 | BRF_GRA },           //  9

	{ "2.11c",			0x0800, 0xd1352c31, 3 | BRF_GRA },           // 10 Big Sprites
	{ "1.11a",			0x0800, 0xa7a2fdbd, 3 | BRF_GRA },           // 11

	{ "prom.v6",		0x0020, 0xb3fc1505, 6 | BRF_GRA },           // 12 Color PROMs
	{ "prom.u6",		0x0020, 0x26aada9e, 6 | BRF_GRA },           // 13
	{ "prom.t6",		0x0020, 0x676b3166, 6 | BRF_GRA },           // 14

	{ "14.5s",			0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 15 Samples
	{ "13.5p",			0x1000, 0x9003ffbd, 7 | BRF_SND },           // 16
};

STD_ROM_PICK(ckongpt2jeu)
STD_ROM_FN(ckongpt2jeu)

struct BurnDriver BurnDrvCkongpt2jeu = {
	"ckongpt2jeu", "ckongpt2", NULL, NULL, "1981",
	"Crazy Kong Part II (Jeutel bootleg)\0", NULL, "bootleg (Jeutel)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongpt2jeuRomInfo, ckongpt2jeuRomName, NULL, NULL, NULL, NULL, CkongInputInfo, CkongDIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Crazy Kong Part II (alternative levels)

static struct BurnRomInfo ckongpt2bRomDesc[] = {
	{ "d05-7.rom",		0x1000, 0x5d96ee9a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "f05-8.rom",		0x1000, 0x74a8435b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "h05-9.rom",		0x1000, 0xe06ca575, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "k05-10.rom",		0x1000, 0x46d83a11, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "l05-11.rom",		0x1000, 0x07c30f3d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "n05-12.rom",		0x1000, 0x151de90a, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "6.11n",			0x1000, 0x2dcedd12, 2 | BRF_GRA },           //  6 Sprites & Tiles
	{ "5.11l",			0x1000, 0xfa7cbd91, 2 | BRF_GRA },           //  7
	{ "4.11k",			0x1000, 0x3375b3bd, 2 | BRF_GRA },           //  8
	{ "3.11h",			0x1000, 0x5655cc11, 2 | BRF_GRA },           //  9

	{ "2.11c",			0x0800, 0xd1352c31, 3 | BRF_GRA },           // 10 Big Sprites
	{ "1.11a",			0x0800, 0xa7a2fdbd, 3 | BRF_GRA },           // 11

	{ "prom.v6",		0x0020, 0xb3fc1505, 6 | BRF_GRA },           // 12 Color PROMs
	{ "prom.u6",		0x0020, 0x26aada9e, 6 | BRF_GRA },           // 13
	{ "prom.t6",		0x0020, 0x676b3166, 6 | BRF_GRA },           // 14

	{ "14.5s",			0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 15 Samples
	{ "13.5p",			0x1000, 0x9003ffbd, 7 | BRF_SND },           // 16
};

STD_ROM_PICK(ckongpt2b)
STD_ROM_FN(ckongpt2b)

static void ckongbCallback()
{
	for (INT32 i = 0; i < 0x6000; i++) {
		DrvZ80ROM[i] ^= 0xf0;
	}
}

static INT32 ckongbInit()
{
	game_select = CKONGB;

	INT32 nRet = DrvInit();

	if (nRet == 0) {
		ckongbCallback();
	}

	return nRet;
}

struct BurnDriver BurnDrvCkongpt2b = {
	"ckongpt2b", "ckongpt2", NULL, NULL, "1981",
	"Crazy Kong Part II (alternative levels)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongpt2bRomInfo, ckongpt2bRomName, NULL, NULL, NULL, NULL, CkongInputInfo, CkongbDIPInfo,
	ckongbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Crazy Kong Part II (bootleg)

static struct BurnRomInfo ckongpt2b2RomDesc[] = {
	{ "0.bin",			0x1000, 0x1c21386f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1.bin",			0x1000, 0x5dc1aaba, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.bin",			0x1000, 0xc9054c94, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.bin",			0x1000, 0x84903b9d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "4.bin",			0x1000, 0xae159192, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "5.bin",			0x1000, 0x966bc9ab, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "d.bin",			0x1000, 0x2dcedd12, 2 | BRF_GRA },           //  6 Sprites & Tiles
	{ "c.bin",			0x1000, 0xfa7cbd91, 2 | BRF_GRA },           //  7
	{ "b.bin",			0x1000, 0x3375b3bd, 2 | BRF_GRA },           //  8
	{ "a.bin",			0x1000, 0x5655cc11, 2 | BRF_GRA },           //  9

	{ "9.bin",			0x0800, 0xd1352c31, 3 | BRF_GRA },           // 10 Big Sprites
	{ "8.bin",			0x0800, 0xa7a2fdbd, 3 | BRF_GRA },           // 11

	{ "prom.bin",		0x0020, 0xd3b84067, 6 | BRF_GRA },           // 12 Color PROMs
	{ "prom.t6",		0x0020, 0x26aada9e, 6 | BRF_GRA },           // 13
	{ "prom.u6",		0x0020, 0x676b3166, 6 | BRF_GRA },           // 14
	
	{ "7.bin",			0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 15 Samples
	{ "6.bin",			0x1000, 0x9003ffbd, 7 | BRF_SND },           // 16
};

STD_ROM_PICK(ckongpt2b2)
STD_ROM_FN(ckongpt2b2)

struct BurnDriver BurnDrvCkongpt2b2 = {
	"ckongpt2b2", "ckongpt2", NULL, NULL, "1981",
	"Crazy Kong Part II (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongpt2b2RomInfo, ckongpt2b2RomName, NULL, NULL, NULL, NULL, CkongInputInfo, Ckongb2DIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Crazy Kong (SegaSA / Sonic bootleg)

static struct BurnRomInfo ckongpt2ssRomDesc[] = {
	{ "ck2_7.5d",		0x1000, 0xb27df032, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ck2_8.5e",		0x1000, 0x5dc1aaba, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ck2_9.5h",		0x1000, 0xc9054c94, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ck_10.5k",		0x1000, 0x923420cb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ck2_11.5l",		0x1000, 0xae159192, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ck_12.5n",		0x1000, 0x25a015d3, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ck2_6.11n",		0x1000, 0x2dcedd12, 2 | BRF_GRA },           //  6 Sprites & Tiles
	{ "ck2_5.11l",		0x1000, 0xfa7cbd91, 2 | BRF_GRA },           //  7
	{ "ck2_4.11k",		0x1000, 0x3375b3bd, 2 | BRF_GRA },           //  8
	{ "ck2_3.11h",		0x1000, 0x5655cc11, 2 | BRF_GRA },           //  9

	{ "ck2_2.11c",		0x0800, 0xd1352c31, 3 | BRF_GRA },           // 10 Big Sprites
	{ "ck2_1.11a",		0x0800, 0xa7a2fdbd, 3 | BRF_GRA },           // 11

	{ "mb7051.v6",		0x0020, 0xb3fc1505, 6 | BRF_GRA },           // 12 Color PROMs
	{ "mb7051.u6",		0x0020, 0x26aada9e, 6 | BRF_GRA },           // 13
	{ "mb7051.t6",		0x0020, 0x676b3166, 6 | BRF_GRA },           // 14

	{ "14.5s",			0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 15 Samples
	{ "13.5p",			0x1000, 0x9003ffbd, 7 | BRF_SND },           // 16
};

STD_ROM_PICK(ckongpt2ss)
STD_ROM_FN(ckongpt2ss)

struct BurnDriver BurnDrvCkongpt2ss = {
	"ckongpt2ss", "ckongpt2", NULL, NULL, "1982",
	"Crazy Kong (SegaSA / Sonic bootleg)\0", NULL, "bootleg (SegaSA / Sonic)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongpt2ssRomInfo, ckongpt2ssRomName, NULL, NULL, NULL, NULL, CkongInputInfo, Ckongb2DIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Crazy Kong

static struct BurnRomInfo ckongRomDesc[] = {
	{ "falcon7",		0x1000, 0x2171cac3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "falcon8",		0x1000, 0x88b83ff7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "falcon9",		0x1000, 0xcff2af47, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "falcon10",		0x1000, 0x6b2ecf23, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "falcon11",		0x1000, 0x327dcadf, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "falcon6",		0x1000, 0xa8916dc8, 2 | BRF_GRA },           //  6 Sprites & Tiles
	{ "falcon5",		0x1000, 0xcd3b5dde, 2 | BRF_GRA },           //  7
	{ "falcon4",		0x1000, 0xb62a0367, 2 | BRF_GRA },           //  8
	{ "falcon3",		0x1000, 0x61122c5e, 2 | BRF_GRA },           //  9

	{ "falcon2",		0x0800, 0xf67c80f1, 3 | BRF_GRA },           // 10 Big Sprites
	{ "falcon1",		0x0800, 0x80eb517d, 3 | BRF_GRA },           // 11

	{ "ck6v.bin",		0x0020, 0x751c3325, 6 | BRF_GRA },           // 12 Color PROMs
	{ "ck6u.bin",		0x0020, 0xab1940fa, 6 | BRF_GRA },           // 13
	{ "ck6t.bin",		0x0020, 0xb4e827a5, 6 | BRF_GRA },           // 14

	{ "falcon13",		0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 15 Samples
	{ "falcon12",		0x1000, 0x9003ffbd, 7 | BRF_SND },           // 16
};

STD_ROM_PICK(ckong)
STD_ROM_FN(ckong)

struct BurnDriver BurnDrvCkong = {
	"ckong", NULL, NULL, NULL, "1981",
	"Crazy Kong\0", NULL, "Kyoei / Falcon", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongRomInfo, ckongRomName, NULL, NULL, NULL, NULL, CkongInputInfo, CkongDIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Crazy Kong (Alca bootleg)

static struct BurnRomInfo ckongalcRomDesc[] = {
	{ "ck7.bin",		0x1000, 0x2171cac3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ck8.bin",		0x1000, 0x88b83ff7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ck9.bin",		0x1000, 0xcff2af47, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ck10.bin",		0x1000, 0x520fa4de, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ck11.bin",		0x1000, 0x327dcadf, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ck6.bin",		0x1000, 0xa8916dc8, 2 | BRF_GRA },           //  5 Sprites & Tiles
	{ "ck5.bin",		0x1000, 0xcd3b5dde, 2 | BRF_GRA },           //  6
	{ "ck4.bin",		0x1000, 0xb62a0367, 2 | BRF_GRA },           //  7
	{ "ck3.bin",		0x1000, 0x61122c5e, 2 | BRF_GRA },           //  8

	{ "alc_ck2.bin",	0x0800, 0xf67c80f1, 3 | BRF_GRA },           //  9 Big Sprites
	{ "alc_ck1.bin",	0x0800, 0x80eb517d, 3 | BRF_GRA },           // 10

	{ "ck6v.bin",		0x0020, 0x751c3325, 6 | BRF_GRA },           // 11 Color PROMs
	{ "ck6u.bin",		0x0020, 0xab1940fa, 6 | BRF_GRA },           // 12
	{ "ck6t.bin",		0x0020, 0xb4e827a5, 6 | BRF_GRA },           // 13

	{ "cc13j.bin",		0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 14 Samples
	{ "ck12.bin",		0x1000, 0x2eb23b60, 7 | BRF_SND },           // 15
};

STD_ROM_PICK(ckongalc)
STD_ROM_FN(ckongalc)

struct BurnDriver BurnDrvCkongalc = {
	"ckongalc", "ckong", NULL, NULL, "1981",
	"Crazy Kong (Alca bootleg)\0", NULL, "bootleg (Alca)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongalcRomInfo, ckongalcRomName, NULL, NULL, NULL, NULL, CkongInputInfo, Ckongb2DIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Monkey Donkey

static struct BurnRomInfo monkeydRomDesc[] = {
	{ "ck7.bin",		0x1000, 0x2171cac3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ck8.bin",		0x1000, 0x88b83ff7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ck9.bin",		0x1000, 0xcff2af47, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ck10.bin",		0x1000, 0x520fa4de, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "md5l.bin",		0x1000, 0xd1db1bb0, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ck6.bin",		0x1000, 0xa8916dc8, 2 | BRF_GRA },           //  5 Sprites & Tiles
	{ "ck5.bin",		0x1000, 0xcd3b5dde, 2 | BRF_GRA },           //  6
	{ "ck4.bin",		0x1000, 0xb62a0367, 2 | BRF_GRA },           //  7
	{ "ck3.bin",		0x1000, 0x61122c5e, 2 | BRF_GRA },           //  8

	{ "md_ck2.bin",		0x0800, 0xf67c80f1, 3 | BRF_GRA },           //  9 Big Sprites
	{ "md_ck1.bin",		0x0800, 0x80eb517d, 3 | BRF_GRA },           // 10

	{ "ck6v.bin",		0x0020, 0x751c3325, 6 | BRF_GRA },           // 11 Color PROMs
	{ "ck6u.bin",		0x0020, 0xab1940fa, 6 | BRF_GRA },           // 12
	{ "ck6t.bin",		0x0020, 0xb4e827a5, 6 | BRF_GRA },           // 13

	{ "cc13j.bin",		0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 14 Samples
	{ "ck12.bin",		0x1000, 0x2eb23b60, 7 | BRF_SND },           // 15
};

STD_ROM_PICK(monkeyd)
STD_ROM_FN(monkeyd)

struct BurnDriver BurnDrvMonkeyd = {
	"monkeyd", "ckong", NULL, NULL, "1981",
	"Monkey Donkey\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, monkeydRomInfo, monkeydRomName, NULL, NULL, NULL, NULL, CkongInputInfo, Ckongb2DIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Donkey King

static struct BurnRomInfo dkingRomDesc[] = {
	{ "d11.r2",			0x1000, 0xf7cace41, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "d7.1n",			0x1000, 0xfe89dea4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "d9.2m",			0x1000, 0xb9c34e14, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "d10.2n",			0x1000, 0x243e458d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "d8.1r",			0x1000, 0x7c66fb5c, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "falcon6",		0x1000, 0xa8916dc8, 2 | BRF_GRA },           //  5 Sprites & Tiles
	{ "falcon5",		0x1000, 0xcd3b5dde, 2 | BRF_GRA },           //  6
	{ "falcon4",		0x1000, 0xb62a0367, 2 | BRF_GRA },           //  7
	{ "falcon3",		0x1000, 0x61122c5e, 2 | BRF_GRA },           //  8

	{ "falcon2",		0x0800, 0xf67c80f1, 3 | BRF_GRA },           //  9 Big Sprites
	{ "falcon1",		0x0800, 0x80eb517d, 3 | BRF_GRA },           // 10

	{ "ck6v.bin",		0x0020, 0x751c3325, 6 | BRF_GRA },           // 11 Color PROMs
	{ "ck6u.bin",		0x0020, 0xab1940fa, 6 | BRF_GRA },           // 12
	{ "ck6t.bin",		0x0020, 0xb4e827a5, 6 | BRF_GRA },           // 13
	{ "82s129.5g",		0x0100, 0x9e11550d, 0 | BRF_GRA },           // 14

	{ "falcon13",		0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 15 Samples
	{ "falcon12",		0x1000, 0x9003ffbd, 7 | BRF_SND },           // 16
};

STD_ROM_PICK(dking)
STD_ROM_FN(dking)

static void dkingCallback()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x1000);

	memcpy (tmp, DrvZ80ROM, 0x800);
	memcpy (DrvZ80ROM, DrvZ80ROM + 0x0800, 0x0800);
	memcpy (DrvZ80ROM + 0x0800, tmp, 0x0800);

	memcpy (tmp, DrvZ80ROM + 0x4000, 0x0800);
	memcpy (DrvZ80ROM + 0x4000, DrvZ80ROM + 0x4800, 0x0800);
	memcpy (DrvZ80ROM + 0x4800, tmp, 0x0800);

	for (INT32 j = 0; j < 0x5000; j += 0x800)
	{
		for (int i = 0x0500; i < 0x0800; i++)
			DrvZ80ROM[i + j] ^= 0xff;
	}
}

static INT32 dkingInit()
{
	INT32 nRet = ckongInit();
	
	if (nRet == 0)
	{
		dkingCallback();
	}

	return nRet;
}

struct BurnDriver BurnDrvDking = {
	"dking", "ckong", NULL, NULL, "1981",
	"Donkey King\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, dkingRomInfo, dkingRomName, NULL, NULL, NULL, NULL, CkongInputInfo, Ckongb2DIPInfo,
	dkingInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Donkey Kong (Spanish bootleg of Crazy Kong)

static struct BurnRomInfo ckongdksRomDesc[] = {
	{ "ck13.bin",		0x1000, 0xf97ba8ae, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ck09.bin",		0x1000, 0xfe89dea4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ck11.bin",		0x1000, 0xb3947d06, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ck12.bin",		0x1000, 0x23d0657d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ck10.bin",		0x1000, 0xc27a13f1, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ck06.bin",		0x1000, 0xa8916dc8, 2 | BRF_GRA },           //  5 Sprites & Tiles
	{ "ck05.bin",		0x1000, 0xcd3b5dde, 2 | BRF_GRA },           //  6
	{ "ck04.bin",		0x1000, 0xb62a0367, 2 | BRF_GRA },           //  7
	{ "ck03.bin",		0x1000, 0x61122c5e, 2 | BRF_GRA },           //  8

	{ "ck02.bin",		0x0800, 0x085b5f90, 3 | BRF_GRA },           //  9 Big Sprites
	{ "ck01.bin",		0x0800, 0x16fd47e2, 3 | BRF_GRA },           // 10

	{ "ck6v.bin",		0x0020, 0x751c3325, 6 | BRF_GRA },           // 11 Color PROMs
	{ "ck6u.bin",		0x0020, 0xab1940fa, 6 | BRF_GRA },           // 12
	{ "ck6t.bin",		0x0020, 0xb4e827a5, 6 | BRF_GRA },           // 13
	{ "82s129.5g",		0x0100, 0x9e11550d, 0 | BRF_GRA },           // 14

	{ "ck08.bin",		0x1000, 0x31c0a7de, 7 | BRF_SND },           // 15 Samples
	{ "ck07.bin",		0x1000, 0x9003ffbd, 7 | BRF_SND },           // 16
};

STD_ROM_PICK(ckongdks)
STD_ROM_FN(ckongdks)

struct BurnDriver BurnDrvCkongdks = {
	"ckongdks", "ckong", NULL, NULL, "1981",
	"Donkey Kong (Spanish bootleg of Crazy Kong)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongdksRomInfo, ckongdksRomName, NULL, NULL, NULL, NULL, CkongInputInfo, Ckongb2DIPInfo,
	dkingInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Crazy Kong (Orca bootleg)

static struct BurnRomInfo ckongoRomDesc[] = {
	{ "o55a-1",			0x1000, 0x8bfb4623, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "o55a-2",			0x1000, 0x9ae8089b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o55a-3",			0x1000, 0xe82b33c8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "o55a-4",			0x1000, 0xf038f941, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "o55a-5",			0x1000, 0x5182db06, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "o50b-1",			0x1000, 0xcae9e2bf, 2 | 0x20 | BRF_GRA },    //  5 Sprites & Tiles
	{ "o50b-2",			0x1000, 0xfba82114, 2 | 0x20 | BRF_GRA },    //  6
	{ "o50b-3",			0x1000, 0x1714764b, 2 | 0x20 | BRF_GRA },    //  7
	{ "o50b-4",			0x1000, 0xb7008b57, 2 | 0x20 | BRF_GRA },    //  8

	{ "c11-02.bin",		0x0800, 0xd1352c31, 3 | BRF_GRA },           //  9 Big Sprites
	{ "a11-01.bin",		0x0800, 0xa7a2fdbd, 3 | BRF_GRA },           // 10

	{ "prom.v6",		0x0020, 0xb3fc1505, 6 | BRF_GRA },           // 11 Color PROMs
	{ "prom.u6",		0x0020, 0x26aada9e, 6 | BRF_GRA },           // 12
	{ "prom.t6",		0x0020, 0x676b3166, 6 | BRF_GRA },           // 13

	{ "cc13j.bin",		0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 14 Samples
	{ "cc12j.bin",		0x1000, 0x9003ffbd, 7 | BRF_SND },           // 15
};

STD_ROM_PICK(ckongo)
STD_ROM_FN(ckongo)

struct BurnDriver BurnDrvCkongo = {
	"ckongo", "ckong", NULL, NULL, "1981",
	"Crazy Kong (Orca bootleg)\0", NULL, "bootleg (Orca)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongoRomInfo, ckongoRomName, NULL, NULL, NULL, NULL, CkongInputInfo, Ckongb2DIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Big Kong

static struct BurnRomInfo bigkongRomDesc[] = {
	{ "dk01f7_2532.d5",	0x1000, 0x4c9102f1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "dk02f8_2532.f5",	0x1000, 0x1683e9ae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dk03f9_2532.h5",	0x1000, 0x073eea32, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dk04f10_2532.k5",0x1000, 0x0aab0334, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "dk05f11_2532.l5",0x1000, 0x45be1c6a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "n05-12.bin",		0x1000, 0x966bc9ab, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "n11-06.bin",		0x1000, 0x2dcedd12, 2 | BRF_GRA },           //  6 Sprites & Tiles
	{ "l11-05.bin",		0x1000, 0xfa7cbd91, 2 | BRF_GRA },           //  7
	{ "k11-04.bin",		0x1000, 0x3375b3bd, 2 | BRF_GRA },           //  8
	{ "h11-03.bin",		0x1000, 0x5655cc11, 2 | BRF_GRA },           //  9

	{ "c11-02.bin",		0x0800, 0xd1352c31, 3 | BRF_GRA },           // 10 Big Sprites
	{ "a11-01.bin",		0x0800, 0xa7a2fdbd, 3 | BRF_GRA },           // 11

	{ "prom.v6",		0x0020, 0xb3fc1505, 6 | BRF_GRA },           // 12 Color PROMs
	{ "prom.u6",		0x0020, 0x26aada9e, 6 | BRF_GRA },           // 13
	{ "prom.t6",		0x0020, 0x676b3166, 6 | BRF_GRA },           // 14

	{ "cc13j.bin",		0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 15 Samples
	{ "cc12j.bin",		0x1000, 0x9003ffbd, 7 | BRF_SND },           // 16
};

STD_ROM_PICK(bigkong)
STD_ROM_FN(bigkong)

struct BurnDriver BurnDrvBigkong = {
	"bigkong", "ckong", NULL, NULL, "1981",
	"Big Kong\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, bigkongRomInfo, bigkongRomName, NULL, NULL, NULL, NULL, CkongInputInfo, Ckongb2DIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Yamato (set 1)

static struct BurnRomInfo yamatoRomDesc[] = {
	{ "2.5de",			0x2000, 0xe796fbce, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #1 Code (Encrypted)
	{ "3.5f",			0x2000, 0xde50e4e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.5jh",			0x2000, 0x4f831d4b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1.5v",			0x0800, 0x3aad9e3c, 7 | BRF_PRG | BRF_ESS }, //  3 Z80 #2 Code

	{ "10.11k",			0x2000, 0x161121f5, 2 | BRF_GRA }, 			 //  4 Tiles & Sprites
	{ "9.11h",			0x2000, 0x56e84cc4, 2 | BRF_GRA }, 			 //  5

	{ "8.11c",			0x1000, 0x28024d9a, 3 | BRF_GRA }, 			 //  6 Big Sprites
	{ "7.11a",			0x1000, 0x4a179790, 3 | BRF_GRA }, 			 //  7

	{ "5.5lm",			0x1000, 0x7761ad24, 4 | BRF_GRA }, 			 //  8 Bank Data
	{ "6.5n",			0x1000, 0xda48444c, 4 | BRF_GRA }, 			 //  9

	{ "1.bpr",			0x0020, 0xef2053ab, 6 | BRF_GRA }, 			 // 10 Color PROMs
	{ "2.bpr",			0x0020, 0x2281d39f, 6 | BRF_GRA }, 			 // 11
	{ "3.bpr",			0x0020, 0x9e6341e3, 6 | BRF_GRA }, 			 // 12
	{ "4.bpr",			0x0020, 0x1c97dc0b, 6 | BRF_GRA }, 			 // 13
	{ "5.bpr",			0x0020, 0xedd6c05f, 6 | BRF_GRA }, 			 // 14
};

STD_ROM_PICK(yamato)
STD_ROM_FN(yamato)

static void sega_decode(const UINT8 convtable[32][4], UINT8 *rom, UINT8 *decrypted, INT32 length, INT32 cryptlen)
{
	INT32 A;

	//INT32 length = 0x10000;
	//INT32 cryptlen = 0x8000;
	//UINT8 *rom = DrvZ80ROM;
	//UINT8 *decrypted = DrvZ80OPS;
	
	for (A = 0x0000;A < cryptlen;A++)
	{
		INT32 xorval = 0;

		UINT8 src = rom[A];

		// pick the translation table from bits 0, 4, 8 and 12 of the address
		INT32 row = (A & 1) + (((A >> 4) & 1) << 1) + (((A >> 8) & 1) << 2) + (((A >> 12) & 1) << 3);

		// pick the offset in the table from bits 3 and 5 of the source data
		INT32 col = ((src >> 3) & 1) + (((src >> 5) & 1) << 1);
		// the bottom half of the translation table is the mirror image of the top
		if (src & 0x80)
		{
			col = 3 - col;
			xorval = 0xa8;
		}

		// decode the opcodes
		decrypted[A] = (src & ~0xa8) | (convtable[2*row][col] ^ xorval);

		// decode the data
		rom[A] = (src & ~0xa8) | (convtable[2*row+1][col] ^ xorval);

		if (convtable[2*row][col] == 0xff)	// table incomplete! (for development)
			decrypted[A] = 0xee;
		if (convtable[2*row+1][col] == 0xff)	// table incomplete! (for development)
			rom[A] = 0xee;
	}

	// this is a kludge to catch anyone who has code that crosses the encrypted
	// decrypted boundary. ssanchan does it
	if (length > 0x8000)
	{
		INT32 bytes = 0x4000;
		memcpy(&decrypted[0x8000], &rom[0x8000], bytes);
	}
}

static INT32 yamatoInit()
{
	static const UINT8 convtable[32][4] =
	{
		//       opcode                   data                     address      
		//  A    B    C    D         A    B    C    D                           
		{ 0x88,0xa8,0x08,0x28 }, { 0x88,0xa8,0x80,0xa0 },   // ...0...0...0...0 
		{ 0x20,0xa0,0x28,0xa8 }, { 0x88,0xa8,0x80,0xa0 },   // ...0...0...0...1 
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },   // ...0...0...1...0
		{ 0x88,0xa8,0x80,0xa0 }, { 0x20,0xa0,0x28,0xa8 },   // ...0...0...1...1
		{ 0x88,0xa8,0x08,0x28 }, { 0x88,0xa8,0x08,0x28 },   // ...0...1...0...0
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },   // ...0...1...0...1
		{ 0x20,0xa0,0x28,0xa8 }, { 0x20,0xa0,0x28,0xa8 },   // ...0...1...1...0
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },   // ...0...1...1...1
		{ 0x20,0xa0,0x28,0xa8 }, { 0x88,0xa8,0x08,0x28 },   // ...1...0...0...0
		{ 0x20,0xa0,0x28,0xa8 }, { 0x28,0x20,0xa8,0xa0 },   // ...1...0...0...1
		{ 0xa0,0x20,0x80,0x00 }, { 0x20,0xa0,0x28,0xa8 },   // ...1...0...1...0
		{ 0x28,0x20,0xa8,0xa0 }, { 0x20,0xa0,0x28,0xa8 },   // ...1...0...1...1
		{ 0x20,0xa0,0x28,0xa8 }, { 0x88,0xa8,0x08,0x28 },   // ...1...1...0...0
		{ 0x88,0xa8,0x08,0x28 }, { 0x88,0xa8,0x08,0x28 },   // ...1...1...0...1
		{ 0xa0,0x20,0x80,0x00 }, { 0x88,0x08,0x80,0x00 },   // ...1...1...1...0
		{ 0x20,0xa0,0x28,0xa8 }, { 0x00,0x08,0x20,0x28 }    // ...1...1...1...1
	};

	game_select = YAMATO;

	INT32 rc = DrvInit();
	if (rc == 0) {
		memmove(DrvZ80ROM+0x7000, DrvZ80ROM+0x6000, 0x1000);
		memset(DrvZ80ROM+0x6000, 0, 0x1000);
		
		sega_decode(convtable, DrvZ80ROM, DrvZ80OPS, 0x10000, 0x8000);

		ZetOpen(0);
		ZetMapArea(0x0000, 0x5fff, 2, DrvZ80OPS, DrvZ80ROM);
		ZetMapMemory(DrvZ80ROM+0x7000, 0x7000, 0x7fff, MAP_ROM);
		ZetMapArea(0x7000, 0x7fff, 2, DrvZ80OPS+0x7000, DrvZ80ROM+0x7000);

		ZetClose();
	}

	return rc;
}

struct BurnDriver BurnDrvYamato = {
	"yamato", NULL, NULL, NULL, "1983",
	"Yamato (set 1)\0", NULL, "Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, yamatoRomInfo, yamatoRomName, NULL, NULL, NULL, NULL, YamatoInputInfo, YamatoDIPInfo,
	yamatoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Yamato (US)

static struct BurnRomInfo yamatouRomDesc[] = {
	{ "2.5de",			0x2000, 0x20895096, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #1 Code (Encrypted)
	{ "3.5f",			0x2000, 0x57a696f9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.5jh",			0x2000, 0x59a468e8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "11.5a",			0x1000, 0x35987485, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "1.5v",			0x0800, 0x3aad9e3c, 7 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "10.11k",			0x2000, 0x161121f5, 2 | BRF_GRA }, 			 //  5 Tiles & Sprites
	{ "9.11h",			0x2000, 0x56e84cc4, 2 | BRF_GRA }, 			 //  6

	{ "8.11c",			0x1000, 0x28024d9a, 3 | BRF_GRA }, 			 //  7 Big Sprites
	{ "7.11a",			0x1000, 0x4a179790, 3 | BRF_GRA }, 			 //  8

	{ "5.5lm",			0x1000, 0x7761ad24, 4 | BRF_GRA }, 			 //  9 Bank data
	{ "6.5n",			0x1000, 0xda48444c, 4 | BRF_GRA }, 			 // 10

	{ "1.bpr",			0x0020, 0xef2053ab, 6 | BRF_GRA }, 			 // 11 Color PROMs
	{ "2.bpr",			0x0020, 0x2281d39f, 6 | BRF_GRA }, 			 // 12
	{ "3.bpr",			0x0020, 0x9e6341e3, 6 | BRF_GRA },			 // 13
	{ "4.bpr",			0x0020, 0x1c97dc0b, 6 | BRF_GRA }, 			 // 14
	{ "5.bpr",			0x0020, 0xedd6c05f, 6 | BRF_GRA }, 			 // 15
};

STD_ROM_PICK(yamatou)
STD_ROM_FN(yamatou)

struct BurnDriver BurnDrvYamatou = {
	"yamatou", "yamato", NULL, NULL, "1983",
	"Yamato (US)\0", NULL, "Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, yamatouRomInfo, yamatouRomName, NULL, NULL, NULL, NULL, YamatoInputInfo, YamatoDIPInfo,
	yamatoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Yamato (set 2)

static struct BurnRomInfo yamatoaRomDesc[] = {
	{ "2-2.5de",		0x2000, 0x93da1d52, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #1 Code (Encrypted)
	{ "3-2.5f",			0x2000, 0x31e73821, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4-2.5jh",		0x2000, 0xfd7bcfc3, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1.5v",			0x0800, 0x3aad9e3c, 7 | BRF_PRG | BRF_ESS }, //  3 Z80 #2 Code

	{ "10.11k",			0x2000, 0x161121f5, 2 | BRF_GRA }, 			 //  4 Tiles & Sprites
	{ "9.11h",			0x2000, 0x56e84cc4, 2 | BRF_GRA }, 			 //  5

	{ "8.11c",			0x1000, 0x28024d9a, 3 | BRF_GRA }, 			 //  6 Big Sprites
	{ "7.11a",			0x1000, 0x4a179790, 3 | BRF_GRA }, 			 //  7

	{ "5.5lm",			0x1000, 0x7761ad24, 4 | BRF_GRA }, 			 //  8 Bank Data
	{ "6.5n",			0x1000, 0xda48444c, 4 | BRF_GRA }, 			 //  9

	{ "1.bpr",			0x0020, 0xef2053ab, 6 | BRF_GRA }, 			 // 10 Color PROMs
	{ "2.bpr",			0x0020, 0x2281d39f, 6 | BRF_GRA }, 			 // 11
	{ "3.bpr",			0x0020, 0x9e6341e3, 6 | BRF_GRA }, 			 // 12
	{ "4.bpr",			0x0020, 0x1c97dc0b, 6 | BRF_GRA }, 			 // 13
	{ "5.bpr",			0x0020, 0xedd6c05f, 6 | BRF_GRA }, 			 // 14
};

STD_ROM_PICK(yamatoa)
STD_ROM_FN(yamatoa)

struct BurnDriver BurnDrvYamatoa = {
	"yamatoa", "yamato", NULL, NULL, "1983",
	"Yamato (set 2)\0", NULL, "Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, yamatoaRomInfo, yamatoaRomName, NULL, NULL, NULL, NULL, YamatoInputInfo, YamatoDIPInfo,
	yamatoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Guzzler

static struct BurnRomInfo guzzlerRomDesc[] = {
	{ "guzz-01.bin",	0x2000, 0x58aaa1e9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #1 Code
	{ "guzz-02.bin",	0x2000, 0xf80ceb17, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "guzz-03.bin",	0x2000, 0xe63c65a2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "guzz-04.bin",	0x2000, 0x45be42f5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "guzz-16.bin",	0x2000, 0x61ee00b7, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "guzz-12.bin",	0x1000, 0xf3754d9e, 7 | BRF_PRG | BRF_ESS }, //  5 Z80 #2 Code

	{ "guzz-13.bin",	0x1000, 0xafc464e2, 2 | BRF_GRA },           //  6 Tiles & Sprites
	{ "guzz-14.bin",	0x1000, 0xacbdfe1f, 2 | BRF_GRA },           //  7
	{ "guzz-15.bin",	0x1000, 0x66978c05, 2 | BRF_GRA },           //  8

	{ "guzz-11.bin",	0x1000, 0xec2e9d86, 3 | BRF_GRA },           //  9 Big Sprites
	{ "guzz-10.bin",	0x1000, 0xbd3f0bf7, 3 | BRF_GRA },           // 10
	{ "guzz-09.bin",	0x1000, 0x18927579, 3 | BRF_GRA },           // 11

	{ "guzzler.003",	0x0100, 0xf86930c1, 6 | BRF_GRA },           // 12 Color PROMs
	{ "guzzler.002",	0x0100, 0xb566ea9e, 6 | BRF_GRA },           // 13
	{ "guzzler.001",	0x0020, 0x69089495, 6 | BRF_GRA },           // 14
};

STD_ROM_PICK(guzzler)
STD_ROM_FN(guzzler)

static INT32 guzzlerInit()
{
	game_select = GUZZLER;

	return DrvInit();
}

struct BurnDriver BurnDrvGuzzler = {
	"guzzler", NULL, NULL, NULL, "1983",
	"Guzzler\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, guzzlerRomInfo, guzzlerRomName, NULL, NULL, NULL, NULL, GuzzlerInputInfo, GuzzlerDIPInfo,
	guzzlerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Guzzler (Swimmer conversion)

static struct BurnRomInfo guzzlersRomDesc[] = {
	{ "guzz1.l9",		0x1000, 0x48f751ee, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #1 Code
	{ "guzz2.k9",		0x1000, 0xc13f23e6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "guzz3.j9",		0x1000, 0x7a523fd8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "guzz4.f9",		0x1000, 0xd2bb2204, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "guzz5.e9",		0x1000, 0x09856fd0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "guzz6.d9",		0x1000, 0x80990d1e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "guzz7.c9",		0x1000, 0xfe37b99d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "guzz8.a9",		0x1000, 0x8d44f5f8, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "guzz-16.bin",	0x2000, 0x61ee00b7, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "guzz-12.bin",	0x1000, 0xf3754d9e, 7 | BRF_PRG | BRF_ESS }, //  9 Z80 #2 Code

	{ "guzz-13.bin",	0x1000, 0xafc464e2, 2 | BRF_GRA },           // 10 Tiles & Sprites
	{ "guzz-14.bin",	0x1000, 0xacbdfe1f, 2 | BRF_GRA },           // 11
	{ "guzz-15.bin",	0x1000, 0x66978c05, 2 | BRF_GRA },           // 12

	{ "guzz-11.bin",	0x1000, 0xec2e9d86, 3 | BRF_GRA },           // 13 Big Sprites
	{ "guzz-10.bin",	0x1000, 0xbd3f0bf7, 3 | BRF_GRA },           // 14
	{ "guzz-09.bin",	0x1000, 0x18927579, 3 | BRF_GRA },           // 15

	{ "guzzler.003",	0x0100, 0xf86930c1, 6 | BRF_GRA },           // 16 Color PROMs
	{ "guzzler.002",	0x0100, 0xb566ea9e, 6 | BRF_GRA },           // 17
	{ "c.c12",			0x0020, 0x51cd9980, 6 | BRF_GRA },           // 18

	{ "tk01.bin",		0x0104, 0xaf082b3c, 0 | BRF_OPT },           // 19 CPU PAL
};

STD_ROM_PICK(guzzlers)
STD_ROM_FN(guzzlers)

struct BurnDriver BurnDrvGuzzlers = {
	"guzzlers", "guzzler", NULL, NULL, "1983",
	"Guzzler (Swimmer conversion)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, guzzlersRomInfo, guzzlersRomName, NULL, NULL, NULL, NULL, GuzzlerInputInfo, GuzzlerDIPInfo,
	guzzlerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Swimmer (set 1)

static struct BurnRomInfo swimmerRomDesc[] = {
	{ "sw1",			0x1000, 0xf12481e7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #1 Code
	{ "sw2",			0x1000, 0xa0b6fdd2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sw3",			0x1000, 0xec93d7de, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sw4",			0x1000, 0x0107927d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sw5",			0x1000, 0xebd8a92c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sw6",			0x1000, 0xf8539821, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sw7",			0x1000, 0x37efb64e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sw8",			0x1000, 0x33d6001e, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "sw12.4k",		0x1000, 0x2eee9bcb, 7 | BRF_PRG | BRF_ESS }, //  8 Z80 #2 Code

	{ "sw15.18k",		0x1000, 0x4f3608cb, 2 | BRF_GRA },           //  9 Sprites & Tiles
	{ "sw14.18l",		0x1000, 0x7181c8b4, 2 | BRF_GRA },           // 10
	{ "sw13.18m",		0x1000, 0x2eb1af5c, 2 | BRF_GRA },           // 11

	{ "sw23.6c",		0x0800, 0x9ca67e24, 3 | BRF_GRA },           // 12 Big Sprites
	{ "sw22.5c",		0x0800, 0x02c10992, 3 | BRF_GRA },           // 13
	{ "sw21.4c",		0x0800, 0x7f4993c1, 3 | BRF_GRA },           // 14

	{ "24s10.13b",		0x0100, 0x8e35b97d, 6 | BRF_GRA },           // 15 Color PROMs
	{ "24s10.13a",		0x0100, 0xc5f24909, 6 | BRF_GRA },           // 16
	{ "18s030.12c",		0x0020, 0x3b2deb3a, 6 | BRF_GRA },           // 17
};

STD_ROM_PICK(swimmer)
STD_ROM_FN(swimmer)

struct BurnDriver BurnDrvSwimmer = {
	"swimmer", NULL, NULL, NULL, "1982",
	"Swimmer (set 1)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, swimmerRomInfo, swimmerRomName, NULL, NULL, NULL, NULL, SwimmerInputInfo, SwimmerDIPInfo,
	guzzlerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Swimmer (set 2)

static struct BurnRomInfo swimmeraRomDesc[] = {
	{ "swa1",			0x1000, 0x42c2b6c5, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #1 Code
	{ "swa2",			0x1000, 0x49bac195, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "swa3",			0x1000, 0xa6d8cb01, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "swa4",			0x1000, 0x7be75182, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "swa5",			0x1000, 0x78f79573, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "swa6",			0x1000, 0xfda9b311, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "swa7",			0x1000, 0x7090e5ee, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "swa8",			0x1000, 0xab86efa9, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "sw12.4k",		0x1000, 0x2eee9bcb, 7 | BRF_PRG | BRF_ESS }, //  8 Z80 #2 Code

	{ "sw15.18k",		0x1000, 0x4f3608cb, 2 | BRF_GRA },           //  9 Sprites & Tiles
	{ "sw14.18l",		0x1000, 0x7181c8b4, 2 | BRF_GRA },           // 10
	{ "sw13.18m",		0x1000, 0x2eb1af5c, 2 | BRF_GRA },           // 11

	{ "sw23.6c",		0x0800, 0x9ca67e24, 3 | BRF_GRA },           // 12 Big Sprites
	{ "sw22.5c",		0x0800, 0x02c10992, 3 | BRF_GRA },           // 13
	{ "sw21.4c",		0x0800, 0x7f4993c1, 3 | BRF_GRA },           // 14

	{ "24s10.13b",		0x0100, 0x8e35b97d, 6 | BRF_GRA },           // 15 Color PROMs
	{ "24s10.13a",		0x0100, 0xc5f24909, 6 | BRF_GRA },           // 16
	{ "18s030.12c",		0x0020, 0x3b2deb3a, 6 | BRF_GRA },           // 17
};

STD_ROM_PICK(swimmera)
STD_ROM_FN(swimmera)

struct BurnDriver BurnDrvSwimmera = {
	"swimmera", "swimmer", NULL, NULL, "1982",
	"Swimmer (set 2)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, swimmeraRomInfo, swimmeraRomName, NULL, NULL, NULL, NULL, SwimmerInputInfo, SwimmerDIPInfo,
	guzzlerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Swimmer (set 3)

static struct BurnRomInfo swimmerbRomDesc[] = {
	{ "sw1.9l",			0x1000, 0xb045be08, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #1 Code
	{ "sw2.9k",			0x1000, 0x163d65e5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sw3.9j",			0x1000, 0x631d74e9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sw4.9f",			0x1000, 0xd62634db, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sw5.9e",			0x1000, 0x922d5d87, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sw6.9d",			0x1000, 0x85478209, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sw7.9c",			0x1000, 0x88266f2e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sw8.9a",			0x1000, 0x191a16e4, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "sw12.4k",		0x1000, 0x2eee9bcb, 7 | BRF_PRG | BRF_ESS }, //  8 Z80 #2 Code

	{ "sw15.18k",		0x1000, 0x4f3608cb, 3 | BRF_GRA },           //  9 Sprites & Tiles
	{ "sw14.18l",		0x1000, 0x7181c8b4, 3 | BRF_GRA },           // 10
	{ "sw13.18m",		0x1000, 0x2eb1af5c, 3 | BRF_GRA },           // 11

	{ "sw23.6c",		0x0800, 0x9ca67e24, 4 | BRF_GRA },           // 12 Big Sprites
	{ "sw22.5c",		0x0800, 0x02c10992, 4 | BRF_GRA },           // 13
	{ "sw21.4c",		0x0800, 0x7f4993c1, 4 | BRF_GRA },           // 14

	{ "24s10.13b",		0x0100, 0x8e35b97d, 6 | BRF_GRA },           // 15 Color PROMs
	{ "24s10.13a",		0x0100, 0xc5f24909, 6 | BRF_GRA },           // 16
	{ "18s030.12c",		0x0020, 0x3b2deb3a, 6 | BRF_GRA },           // 17
};

STD_ROM_PICK(swimmerb)
STD_ROM_FN(swimmerb)

struct BurnDriver BurnDrvSwimmerb = {
	"swimmerb", "swimmer", NULL, NULL, "1982",
	"Swimmer (set 3)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, swimmerbRomInfo, swimmerbRomName, NULL, NULL, NULL, NULL, SwimmerInputInfo, SwimmerDIPInfo,
	guzzlerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// River Patrol (Japan)

static struct BurnRomInfo rpatrolRomDesc[] = {
	{ "1.1h",			0x1000, 0x065197f0, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2.1f",			0x1000, 0x3614b820, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.1d",			0x1000, 0xba428bbf, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.1c",			0x1000, 0x41497a94, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.1a",			0x1000, 0xe20ee7e7, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "6.6l.2732",		0x1000, 0xb38d8aca, 2 | 0x10 | BRF_GRA },    //  5 Sprites & Tiles
	{ "7.6p.2732",		0x1000, 0xbc2bddf9, 2 | 0x10 | BRF_GRA },    //  6

	{ "8.2s",			0x0800, 0x59747c31, 3 | BRF_GRA },           //  7 Big Sprites
	{ "9.2t",			0x0800, 0x065651a5, 3 | BRF_GRA },           //  8

	{ "mb7051.1b",		0x0020, 0xf9a2383b, 6 | BRF_GRA },           //  9 Color PROMs
	{ "mb7051.1c",		0x0020, 0x1743bd26, 6 | BRF_GRA },           // 10
	{ "mb7051.1u",		0x0020, 0xee03bc96, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(rpatrol)
STD_ROM_FN(rpatrol)

static void rpatrolCallback()
{
	for (INT32 i = 0; i < 0x5000; i++)	{
		DrvZ80ROM[i] ^= (i & 1) ? 0x5b : 0x79;
	}
}

static INT32 rpatrolInit()
{
	game_select = CCLIMBER;

	INT32 nRet = DrvInit();

	if (nRet == 0)
	{
		rpatrolCallback();
	}

	return nRet;
}

struct BurnDriver BurnDrvRpatrol = {
	"rpatrol", NULL, NULL, NULL, "1981",
	"River Patrol (Japan)\0", NULL, "Orca", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, rpatrolRomInfo, rpatrolRomName, NULL, NULL, NULL, NULL, RpatrolInputInfo, RpatrolDIPInfo,
	rpatrolInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// River Patrol (Japan, unprotected)
/* located original ORCA OVG-51A PCB */

static struct BurnRomInfo rpatrolnRomDesc[] = {
	{ "1_2.3k",			0x1000, 0x33b01c90, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2_2.3l",			0x1000, 0x03f53340, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3_2.3n",			0x1000, 0x8fa300df, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4_2.3p",			0x1000, 0x74a8f1f4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5_2.3r",			0x1000, 0xd7ef6c87, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "6.6l.2732",		0x1000, 0xb38d8aca, 2 | 0x10 | BRF_GRA },    //  5 Sprites & Tiles
	{ "7.6p.2732",		0x1000, 0xbc2bddf9, 2 | 0x10 | BRF_GRA },    //  6

	{ "8.2s",			0x0800, 0x59747c31, 3 | BRF_GRA },           //  7 Big Sprites
	{ "9.2t",			0x0800, 0x065651a5, 3 | BRF_GRA },           //  8

	{ "mb7051.1b",		0x0020, 0xf9a2383b, 6 | BRF_GRA },           //  9 Color PROMs
	{ "mb7051.1c",		0x0020, 0x1743bd26, 6 | BRF_GRA },           // 10
	{ "mb7051.1u",		0x0020, 0xee03bc96, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(rpatroln)
STD_ROM_FN(rpatroln)

static INT32 rpatrolnInit()
{
	game_select = CCLIMBER;

	return DrvInit();
}

struct BurnDriver BurnDrvRpatroln = {
	"rpatroln", "rpatrol", NULL, NULL, "1981",
	"River Patrol (Japan, unprotected)\0", NULL, "Orca", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, rpatrolnRomInfo, rpatrolnRomName, NULL, NULL, NULL, NULL, RpatrolInputInfo, RpatrolDIPInfo,
	rpatrolnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// River Patrol (bootleg)

static struct BurnRomInfo rpatrolbRomDesc[] = {
	{ "rp1.4l",			0x1000, 0xbfd7ae7a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "rp2.4j",			0x1000, 0x03f53340, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rp3.4f",			0x1000, 0x8fa300df, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rp4.4e",			0x1000, 0x74a8f1f4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rp5.4c",			0x1000, 0xd7ef6c87, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "rp6.6n",			0x0800, 0x19f18e9e, 2 | BRF_GRA },           //  5 Sprites & Tiles
	{ "rp7.6l",			0x0800, 0x07f2070d, 2 | BRF_GRA },           //  6
	{ "rp8.6k",			0x0800, 0x008738c7, 2 | BRF_GRA },           //  7
	{ "rp9.6h",			0x0800, 0xea5aafca, 2 | BRF_GRA },           //  8

	{ "rp11.6c",		0x0800, 0x065651a5, 3 | BRF_GRA },           //  9 Big Sprites
	{ "rp10.6a",		0x0800, 0x59747c31, 3 | BRF_GRA },           // 10

	{ "bprom1.9n",		0x0020, 0xf9a2383b, 6 | BRF_GRA },           //  9 Color PROMs
	{ "bprom2.9p",		0x0020, 0x1743bd26, 6 | BRF_GRA },           // 10
	{ "bprom3.9c",		0x0020, 0xee03bc96, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(rpatrolb)
STD_ROM_FN(rpatrolb)

static INT32 rpatrolbInit()
{
	game_select = CCLIMBER;

	return DrvInit();
}

struct BurnDriver BurnDrvRpatrolb = {
	"rpatrolb", "rpatrol", NULL, NULL, "1981",
	"River Patrol (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, rpatrolbRomInfo, rpatrolbRomName, NULL, NULL, NULL, NULL, RpatrolInputInfo, RpatrolDIPInfo,
	rpatrolbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Silver Land (hack of River Patrol)

static struct BurnRomInfo silvlandRomDesc[] = {
	{ "7.2r",			0x1000, 0x57e6be62, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "8.1n",			0x1000, 0xbbb2b287, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rp3.4f",			0x1000, 0x8fa300df, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "10.2n",			0x1000, 0x5536a65d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "11.1r",			0x1000, 0x6f23f66f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "12.2k",			0x1000, 0x26f1537c, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "6.6n",			0x0800, 0xaffb804f, 2 | BRF_GRA },           //  6 Sprites & Tiles
	{ "5.6l",			0x0800, 0xad4642e5, 2 | BRF_GRA },           //  7
	{ "4.6k",			0x0800, 0xe487579d, 2 | BRF_GRA },           //  8
	{ "3.6h",			0x0800, 0x59125a1a, 2 | BRF_GRA },           //  9

	{ "2.6c",			0x0800, 0xc8d32b8e, 3 | BRF_GRA },           // 10 Big Sprites
	{ "1.6a",			0x0800, 0xee333daf, 3 | BRF_GRA },           // 11

	{ "mb7051.1v",		0x0020, 0x1d2343b1, 6 | BRF_GRA },           // 12 Color PROMs
	{ "mb7051.1u",		0x0020, 0xc174753c, 6 | BRF_GRA },           // 13
	{ "mb7051.1t",		0x0020, 0x04a1be01, 6 | BRF_GRA },           // 14
};

STD_ROM_PICK(silvland)
STD_ROM_FN(silvland)

static INT32 silvlandInit()
{
	game_select = SILVLAND;

	return DrvInit();
}

struct BurnDriver BurnDrvSilvland = {
	"silvland", "rpatrol", NULL, NULL, "1981",
	"Silver Land (hack of River Patrol)\0", NULL, "Falcon", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, silvlandRomInfo, silvlandRomName, NULL, NULL, NULL, NULL, RpatrolInputInfo, RpatrolDIPInfo,
	silvlandInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Tangram Q

static struct BurnRomInfo tangramqRomDesc[] = {
	{ "m1.k5",			0x2000, 0xdff92169, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #1 Code
	{ "m2.k4",			0x2000, 0x1cbade75, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "s1.a6",			0x2000, 0x05af38f6, 7 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code

	{ "f1.h4",			0x2000, 0xc7c3ffe1, 2 | BRF_GRA },           //  3 Sprites & Tiles
	{ "f2.h2",			0x2000, 0xdbc13c1f, 2 | BRF_GRA },           //  4

	{ "b2.e17",			0x1000, 0x77d21b84, 3 | BRF_GRA },           //  5 Big Sprites
	{ "b1.e19",			0x1000, 0xf3ec2562, 3 | BRF_GRA },           //  6

	{ "mb7051_m02.m6",	0x0020, 0xb3fc1505, 6 | BRF_GRA },           //  7 Color PROMs
	{ "mb7051_m02.m7",	0x0020, 0x26aada9e, 6 | BRF_GRA },           //  8
	{ "mb7051_m02.m8",	0x0020, 0x676b3166, 6 | BRF_GRA },           //  9
};

STD_ROM_PICK(tangramq)
STD_ROM_FN(tangramq)

static INT32 tangramqInit()
{
	game_select = TANGRAMQ;

	return DrvInit();
}

struct BurnDriver BurnDrvTangramq = {
	"tangramq", NULL, NULL, NULL, "1983",
	"Tangram Q\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, tangramqRomInfo, tangramqRomName, NULL, NULL, NULL, NULL, TangramqInputInfo, TangramqDIPInfo,
	tangramqInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Au (location test)

static struct BurnRomInfo auRomDesc[] = {
	{ "program0.e8",	0x2000, 0x04c7ebc9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #1 Code
	{ "program1.b8",	0x2000, 0xd3820146, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "program2.d8",	0x2000, 0xda85cf0f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "program3.a8",	0x2000, 0xfa4bc959, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sound0.c4",		0x1000, 0x0315f0a1, 7 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "tile2.bin",		0x2000, 0xfaa24ff4, 2 | BRF_GRA },           //  5 Sprites & Tiles
	{ "tile1.bin",		0x2000, 0x2bd7aa4e, 2 | BRF_GRA },           //  6
	{ "tile0.bin",		0x2000, 0xd5a8bf00, 2 | BRF_GRA },           //  7

	{ "big2.g7",		0x1000, 0x19d65322, 3 | BRF_GRA },           //  8 Big Sprites
	{ "big1.e7",		0x1000, 0xdd2bf0ba, 3 | BRF_GRA },           //  9
	{ "big0.c7",		0x1000, 0x4a22394e, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(au)
STD_ROM_FN(au)

static INT32 auInit()
{
	game_select = AU;

	return DrvInit();
}

struct BurnDriver BurnDrvAu = {
	"au", NULL, NULL, NULL, "1983",
	"Au (location test)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, auRomInfo, auRomName, NULL, NULL, NULL, NULL, AuInputInfo, AuDIPInfo,
	auInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Cannon Ball (bootleg on Crazy Kong hardware) (set 1, buggy)

static struct BurnRomInfo cannonbRomDesc[] = {
	{ "canballs.5d",	0x1000, 0x43ad0d16, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "canballs.5f",	0x1000, 0x3e0dacdd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "canballs.5h",	0x1000, 0xe18a836b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "canballs.5k",	0x0800, 0x6ed3cbf4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "cb10.bin",		0x1000, 0x602a6c2d, 2 | 0x10 | BRF_GRA },    //  4 Sprites & Tiles
	{ "cb9.bin",		0x1000, 0x2d036026, 2 | 0x10 | BRF_GRA },    //  5

	{ "canballs.11c",	0x0800, 0xd1352c31, 3 | BRF_GRA },           //  6 Big Sprites
	{ "canballs.11a",	0x0800, 0xa7a2fdbd, 3 | BRF_GRA },           //  7

	{ "prom.v6",		0x0020, 0xb3fc1505, 6 | BRF_GRA },           //  8 Color PROMs
	{ "prom.u6",		0x0020, 0x26aada9e, 6 | BRF_GRA },           //  9
	{ "prom.t6",		0x0020, 0x676b3166, 6 | BRF_GRA },           // 10

	{ "canballs.5s",	0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 11 Samples
	{ "canballs.5p",	0x1000, 0x9003ffbd, 7 | BRF_SND },           // 12

	{ "canballs.11n",	0x1000, 0xa95b8e03, 0 | BRF_OPT },           // 13 Unused
	{ "canballs.11k",	0x1000, 0xdbbe8263, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(cannonb)
STD_ROM_FN(cannonb)

static void CannonbCallback()
{
	static const UINT8 xor_tab[4] = { 0x92, 0x82, 0x12, 0x10 };

	for (INT32 i = 0; i < 0x1000; i++)
	{
		DrvZ80ROM[i] ^= xor_tab[((i & 0x200) >> 8) | ((i & 0x80) >> 7)];
	}

	//ZetReset(0);
}

static INT32 CannonbInit()
{
	game_select = CANNONB;

	INT32 nRet = DrvInit();
	
	if (nRet == 0)
	{
		CannonbCallback();
	}
	
	return nRet;
}

struct BurnDriver BurnDrvCannonb = {
	"cannonb", "cannonbp", NULL, NULL, "1985",
	"Cannon Ball (bootleg on Crazy Kong hardware) (set 1, buggy)\0", NULL, "bootleg (Soft)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, cannonbRomInfo, cannonbRomName, NULL, NULL, NULL, NULL, CannonbInputInfo, CannonbDIPInfo,
	CannonbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Cannon Ball (bootleg on Crazy Kong hardware) (set 2, buggy)

static struct BurnRomInfo cannonb2RomDesc[] = {
	{ "cb1.bin",		0x1000, 0x7a3cba7c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "cb2.bin",		0x1000, 0x58ef3118, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cb3.bin",		0x1000, 0xe18a836b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cb4.bin",		0x1000, 0x696ebdb0, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "cb10.bin",		0x1000, 0x602a6c2d, 2 | 0x10 | BRF_GRA },    //  4 Sprites & Tiles
	{ "cb9.bin",		0x1000, 0x2d036026, 2 | 0x10 | BRF_GRA },    //  5

	{ "cb7.bin",		0x0800, 0x80eb517d, 3 | BRF_GRA },           //  6 bigsprite
	{ "cb8.bin",		0x0800, 0xf67c80f1, 3 | BRF_GRA },           //  7

	{ "v6.bin",			0x0020, 0x751c3325, 6 | BRF_GRA },           //  8 Color PROMs
	{ "u6.bin",			0x0020, 0xc0539747, 6 | BRF_GRA },           //  9
	{ "t6.bin",			0x0020, 0xb4e827a5, 6 | BRF_GRA },           // 10

	{ "cb6.bin",		0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 11 Samples
	{ "cb5.bin",		0x1000, 0x9003ffbd, 7 | BRF_SND },           // 12
};

STD_ROM_PICK(cannonb2)
STD_ROM_FN(cannonb2)

static INT32 Cannonb2Init()
{
	game_select = CANNONB;

	return DrvInit();
}

struct BurnDriver BurnDrvCannonb2 = {
	"cannonb2", "cannonbp", NULL, NULL, "1985",
	"Cannon Ball (bootleg on Crazy Kong hardware) (set 2, buggy)\0", NULL, "bootleg (TV Game Gruenberg)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, cannonb2RomInfo, cannonb2RomName, NULL, NULL, NULL, NULL, CannonbInputInfo, CannonbDIPInfo,
	Cannonb2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Cannon Ball (bootleg on Crazy Kong hardware) (set 3, no bonus game)

static struct BurnRomInfo cannonb3RomDesc[] = {
	{ "1 pos e5  2532.bin",		0x1000, 0x4b482e80, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2 pos f5  2532.bin",		0x1000, 0x1fa050af, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3 pos h5  2532.bin",		0x1000, 0xe18a836b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4 pos k5  2716.bin",		0x0800, 0x58e04f41, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "b pos n11  2532.bin",	0x1000, 0x602a6c2d, 2 | 0x10 | BRF_GRA },    //  4 Sprites & Tiles
	{ "a pos k11  2532.bin",	0x1000, 0x2d036026, 2 | 0x10 | BRF_GRA },    //  5

	{ "ck2 pos c11  2716.bin",	0x0800, 0xd1352c31, 3 | BRF_GRA },           //  6 Big Sprites
	{ "ck1 pos a11  2716.bin",	0x0800, 0xa7a2fdbd, 3 | BRF_GRA },           //  7

	{ "c pos v6  n82s123n.bin",	0x0020, 0xb3fc1505, 6 | BRF_GRA },           //  8 Color PROMs
	{ "b pos u6  n82s123n.bin",	0x0020, 0xa758b567, 6 | BRF_GRA },           //  9
	{ "a pos t6  n82s123n.bin",	0x0020, 0x676b3166, 6 | BRF_GRA },           // 10

	{ "ck14 pos s5  2532.bin",	0x1000, 0x5f0bcdfb, 7 | BRF_SND },           // 11 Samples
	{ "ck13 pos r5  2532.bin",	0x1000, 0x9003ffbd, 7 | BRF_SND },           // 12
};

STD_ROM_PICK(cannonb3)
STD_ROM_FN(cannonb3)

struct BurnDriver BurnDrvCannonb3 = {
	"cannonb3", "cannonbp", NULL, NULL, "1985",
	"Cannon Ball (bootleg on Crazy Kong hardware) (set 3, no bonus game)\0", NULL, "bootleg (Soft)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, cannonb3RomInfo, cannonb3RomName, NULL, NULL, NULL, NULL, CannonbInputInfo, CannonbDIPInfo,
	Cannonb2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Top Roller

static struct BurnRomInfo toprollrRomDesc[] = {
	{ "2.f5",			0x2000, 0xef789f00, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "8.f3",			0x2000, 0x94371cfb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.k5",			0x2000, 0x1cb48ea0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.h5",			0x2000, 0xd45494ba, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "9.h3",			0x2000, 0x8a8032a7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1.d5",			0x2000, 0x9894374d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.d3",			0x2000, 0x904fffb6, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "10.k3",			0x2000, 0x1e8914a6, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "11.l3",			0x2000, 0xb20a9fa2, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "16.j4",			0x2000, 0xce3afe26, 2 | BRF_GRA },           //  9 Sprites & Tiles
	{ "15.h4",			0x2000, 0xb6fe97f2, 2 | BRF_GRA },           // 10

	{ "14.c4",			0x2000, 0x7a945733, 3 | BRF_GRA },           // 11 Big Sprites
	{ "13.a4",			0x2000, 0x89327329, 3 | BRF_GRA },           // 12

	{ "6.m5",			0x1000, 0xe30c1dd8, 8 | BRF_GRA },           // 13 Big Sprites
	{ "5.l5",			0x1000, 0x84139f46, 8 | BRF_GRA },           // 14

	{ "12.p3",			0x2000, 0x7f989dc9, 7 | BRF_SND },           // 15 Samples

	{ "prom.p2",		0x0020, 0x42e828fa, 6 | BRF_GRA },           // 16 Color PROMs
	{ "prom.r2",		0x0020, 0x99b87eed, 6 | BRF_GRA },           // 17
	{ "prom.a1",		0x0020, 0x7d626d6c, 6 | BRF_GRA },           // 18
	{ "prom.p9",		0x0020, 0xeb399c02, 6 | BRF_GRA },           // 19
	{ "prom.n9",		0x0020, 0xfb03ea99, 6 | BRF_GRA },           // 20

	{ "prom.s9",		0x0100, 0xabf4c5fb, 0 | BRF_OPT },           // 21 Unused
};

STD_ROM_PICK(toprollr)
STD_ROM_FN(toprollr)

static void ToprollrCallback()
{
	static const UINT8 convtable[32][4] =
	{
		//       opcode                   data                     address      
		//  A    B    C    D         A    B    C    D                           
		{ 0x88,0xa8,0x08,0x28 }, { 0x88,0xa8,0x80,0xa0 },   // ...0...0...0...0
		{ 0x20,0xa0,0x28,0xa8 }, { 0x88,0xa8,0x80,0xa0 },   // ...0...0...0...1
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },   // ...0...0...1...0
		{ 0x88,0xa8,0x80,0xa0 }, { 0x20,0xa0,0x28,0xa8 },   // ...0...0...1...1
		{ 0x88,0xa8,0x08,0x28 }, { 0x88,0xa8,0x08,0x28 },   // ...0...1...0...0
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },   // ...0...1...0...1
		{ 0x20,0xa0,0x28,0xa8 }, { 0x20,0xa0,0x28,0xa8 },   // ...0...1...1...0
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },   // ...0...1...1...1
		{ 0x20,0xa0,0x28,0xa8 }, { 0x88,0xa8,0x08,0x28 },   // ...1...0...0...0
		{ 0x20,0xa0,0x28,0xa8 }, { 0x28,0x20,0xa8,0xa0 },   // ...1...0...0...1
		{ 0xa0,0x20,0x80,0x00 }, { 0x20,0xa0,0x28,0xa8 },   // ...1...0...1...0
		{ 0x28,0x20,0xa8,0xa0 }, { 0x20,0xa0,0x28,0xa8 },   // ...1...0...1...1
		{ 0x20,0xa0,0x28,0xa8 }, { 0x88,0xa8,0x08,0x28 },   // ...1...1...0...0
		{ 0x88,0xa8,0x08,0x28 }, { 0x88,0xa8,0x08,0x28 },   // ...1...1...0...1
		{ 0xa0,0x20,0x80,0x00 }, { 0x88,0x08,0x80,0x00 },   // ...1...1...1...0
		{ 0x20,0xa0,0x28,0xa8 }, { 0x00,0x08,0x20,0x28 }    // ...1...1...1...1
	};

	UINT8 *tmp = (UINT8*)BurnMalloc(0x30000);

	memcpy (tmp + 0x00000, DrvZ80ROM + 0x00000, 0x06000); // bank 0
	memcpy (tmp + 0x10000, DrvZ80ROM + 0x06000, 0x04000); // bank 1
	memcpy (tmp + 0x14000, DrvZ80ROM + 0x04000, 0x02000);
	memcpy (tmp + 0x20000, DrvZ80ROM + 0x0a000, 0x04000); // bank 2
	memcpy (tmp + 0x24000, DrvZ80ROM + 0x04000, 0x02000);
	memcpy (tmp + 0x0c000, DrvZ80ROM + 0x0e000, 0x04000); // fixed
	memcpy (DrvZ80ROM, tmp, 0x30000);
	
	BurnFree (tmp);

	sega_decode(convtable, DrvZ80ROM + 0x00000, DrvZ80OPS + 0x00000, 0x10000, 0x8000);
	sega_decode(convtable, DrvZ80ROM + 0x10000, DrvZ80OPS + 0x10000, 0x10000, 0x8000);
	sega_decode(convtable, DrvZ80ROM + 0x20000, DrvZ80OPS + 0x20000, 0x10000, 0x8000);

	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM, 0x0000, 0x5fff, MAP_ROM);
	ZetMapArea(0x0000, 0x5fff, 2, DrvZ80OPS, DrvZ80ROM);
	ZetReset(0);
	ZetClose();
}

static INT32 toprollrInit()
{
	game_select = TOPROLLR;

	INT32 nRet = DrvInit();
	if (nRet == 0) {
		ToprollrCallback();
	}

	return nRet;
}

struct BurnDriver BurnDrvToprollr = {
	"toprollr", NULL, NULL, NULL, "1983",
	"Top Roller\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, toprollrRomInfo, toprollrRomName, NULL, NULL, NULL, NULL, ToprollrInputInfo, ToprollrDIPInfo,
	toprollrInit, DrvExit, DrvFrame, ToprollrDraw, DrvScan, &DrvRecalc, 0xa0,
	224, 256, 3, 4
};


// Le Bagnard (bootleg on Crazy Kong hardware)

static struct BurnRomInfo bagmanfRomDesc[] = {
	{ "01.d5.bin",		0x1000, 0xe0156191, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "02.f5.bin",		0x1000, 0x7b758982, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03.h5.bin",		0x1000, 0x302a077b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "04.k5.bin",		0x1000, 0xb704d761, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "05.l5.bin",		0x1000, 0x4f0088ab, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "2732 05 pos dboard.bin",	0x1000, 0x91570033, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "10.n11.bin",		0x1000, 0xc680ef04, 2 | BRF_GRA },           //  6 Sprites & Tiles
	{ "09.l11.bin",		0x1000, 0x060b044c, 2 | BRF_GRA },           //  7
	{ "12.k11.bin",		0x1000, 0x4a0a6b55, 2 | BRF_GRA },           //  8
	{ "11.h11.bin",		0x1000, 0x8043bc1a, 2 | BRF_GRA },           //  9

	{ "v6",				0x0020, 0xb3fc1505, 6 | BRF_GRA },           // 10 Color PROMs
	{ "u6",				0x0020, 0x26aada9e, 6 | BRF_GRA },           // 11
	{ "t6",				0x0020, 0x676b3166, 6 | BRF_GRA },           // 12

	{ "daughterboard.prom",	0x0020, 0xc58a4f6a, 0 | BRF_OPT },           // 13 proms2

	{ "2732 07 pos dboard.bin",	0x1000, 0x2e0057ff, 7 | BRF_SND },           // 14 Speech data
	{ "2732 08 pos dboard.bin",	0x1000, 0xb2120edd, 7 | BRF_SND },           // 15
};

STD_ROM_PICK(bagmanf)
STD_ROM_FN(bagmanf)

struct BurnDriverD BurnDrvBagmanf = {
	"bagmanf", "bagman", NULL, NULL, "1982",
	"Le Bagnard (bootleg on Crazy Kong hardware)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, bagmanfRomInfo, bagmanfRomName, NULL, NULL, NULL, NULL, ToprollrInputInfo, ToprollrDIPInfo, //BagmanfInputInfo, BagmanfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	256, 224, 4, 3
};
