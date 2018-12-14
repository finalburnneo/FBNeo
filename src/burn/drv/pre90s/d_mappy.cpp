// FB Alpha Mappy driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "namco_snd.h"
#include "namcoio.h"
#include "bitswap.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *DrvM6809ROM2;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndPROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvM6809RAM2;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 main_irq_mask;
static UINT8 sub_irq_mask;
static UINT8 sub2_irq_mask;
static UINT8 flipscreen;
static UINT8 sub_cpu_in_reset;
static UINT8 sub2_cpu_in_reset;
static UINT8 scroll;
static UINT8 out_mux;

static INT32 watchdog; // not hooked up

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[7];
static UINT8 DrvReset;

static INT32 fourwaymode = 0;
static INT32 grobdamode = 0;

static struct BurnInputInfo MappyInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Mappy)

static struct BurnInputInfo Digdug2InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy5 + 0,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy5 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Digdug2)

static struct BurnInputInfo TodruagaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Todruaga)

static struct BurnInputInfo MotosInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy5 + 0,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy5 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Motos)

static struct BurnInputInfo SuperpacInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Superpac)

static struct BurnInputInfo PacnpalInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Pacnpal)

static struct BurnInputInfo GrobdaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy5 + 0,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy5 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Grobda)

static struct BurnInputInfo PhozonInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Phozon)

static struct BurnDIPInfo MappyDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x0f, NULL				},
	{0x0d, 0xff, 0xff, 0xff, NULL				},
	{0x0e, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0c, 0x01, 0x04, 0x04, "Upright"			},
	{0x0c, 0x01, 0x04, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Service"			},
	{0x0c, 0x01, 0x08, 0x08, "Off"				},
	{0x0c, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x0d, 0x01, 0x07, 0x07, "Rank A"			},
	{0x0d, 0x01, 0x07, 0x06, "Rank B"			},
	{0x0d, 0x01, 0x07, 0x05, "Rank C"			},
	{0x0d, 0x01, 0x07, 0x04, "Rank D"			},
	{0x0d, 0x01, 0x07, 0x03, "Rank E"			},
	{0x0d, 0x01, 0x07, 0x02, "Rank F"			},
	{0x0d, 0x01, 0x07, 0x01, "Rank G"			},
	{0x0d, 0x01, 0x07, 0x00, "Rank H"			},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x0d, 0x01, 0x18, 0x00, "2 Coins 1 Credits"		},
	{0x0d, 0x01, 0x18, 0x18, "1 Coin  1 Credits"		},
	{0x0d, 0x01, 0x18, 0x10, "1 Coin  5 Credits"		},
	{0x0d, 0x01, 0x18, 0x08, "1 Coin  7 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0d, 0x01, 0x20, 0x00, "Off"				},
	{0x0d, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Rack Test (Cheat)"		},
	{0x0d, 0x01, 0x40, 0x40, "Off"				},
	{0x0d, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x0d, 0x01, 0x80, 0x80, "Off"				},
	{0x0d, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x0e, 0x01, 0x07, 0x01, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0x07, 0x00, "3 Coins 2 Credits"		},
	{0x0e, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0x07, 0x02, "2 Coins 3 Credits"		},
	{0x0e, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0x07, 0x04, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x0e, 0x01, 0x38, 0x18, "20k Only"			},
	{0x0e, 0x01, 0x38, 0x30, "20k & 60k Only"		},
	{0x0e, 0x01, 0x38, 0x38, "20k & 70k Only"		},
	{0x0e, 0x01, 0x38, 0x10, "20k, 70k & Every 70k"		},
	{0x0e, 0x01, 0x38, 0x28, "20k & 80k Only"		},
	{0x0e, 0x01, 0x38, 0x08, "20k, 80k & Every 80k"		},
	{0x0e, 0x01, 0x38, 0x20, "30k & 100k Only"		},
#if 0
	{0x0e, 0x01, 0x38, 0x20, "30k Only"			},
	{0x0e, 0x01, 0x38, 0x38, "30k & 80k Only"		},
	{0x0e, 0x01, 0x38, 0x30, "30k & 100k Only"		},
	{0x0e, 0x01, 0x38, 0x10, "30k, 100k & Every 100k"	},
	{0x0e, 0x01, 0x38, 0x28, "30k & 120k Only"		},
	{0x0e, 0x01, 0x38, 0x18, "40k Only"			},
	{0x0e, 0x01, 0x38, 0x08, "40k, 120k & Every 120k"	},
#endif
	{0x0e, 0x01, 0x38, 0x00, "None"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0e, 0x01, 0xc0, 0x40, "1"				},
	{0x0e, 0x01, 0xc0, 0x00, "2"				},
	{0x0e, 0x01, 0xc0, 0xc0, "3"				},
	{0x0e, 0x01, 0xc0, 0x80, "5"				},
};

STDDIPINFO(Mappy)

static struct BurnDIPInfo Digdug2DIPList[]=
{
	{0x12, 0xff, 0xff, 0x0f, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},
	{0x14, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x12, 0x01, 0x04, 0x04, "Upright"			},
	{0x12, 0x01, 0x04, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Service"			},
	{0x12, 0x01, 0x08, 0x08, "Off"				},
	{0x12, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x01, 0x01, "Off"				},
	{0x13, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x13, 0x01, 0x02, 0x02, "3"				},
	{0x13, 0x01, 0x02, 0x00, "5"				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x13, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x13, 0x01, 0x30, 0x30, "30k 80k and ..."		},
	{0x13, 0x01, 0x30, 0x20, "30k 100k and ..."		},
	{0x13, 0x01, 0x30, 0x10, "30k 120k and ..."		},
	{0x13, 0x01, 0x30, 0x00, "30k 150k and..."		},

	{0   , 0xfe, 0   ,    2, "Level Select"			},
	{0x13, 0x01, 0x40, 0x40, "Off"				},
	{0x13, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Digdug2)

static struct BurnDIPInfo TodruagaDIPList[]=
{
	{0x10, 0xff, 0xff, 0x0f, NULL				},
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x10, 0x01, 0x04, 0x04, "Upright"			},
	{0x10, 0x01, 0x04, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x11, 0x01, 0x03, 0x01, "1"				},
	{0x11, 0x01, 0x03, 0x02, "2"				},
	{0x11, 0x01, 0x03, 0x03, "3"				},
	{0x11, 0x01, 0x03, 0x00, "5"				},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x11, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x11, 0x01, 0x10, 0x10, "Off"				},
	{0x11, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x20, 0x20, "Off"				},
	{0x11, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x11, 0x01, 0xc0, 0x00, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
};

STDDIPINFO(Todruaga)

static struct BurnDIPInfo MotosDIPList[]=
{
	{0x12, 0xff, 0xff, 0x0f, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},
	{0x14, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x12, 0x01, 0x04, 0x04, "Upright"			},
	{0x12, 0x01, 0x04, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x01, 0x01, "Off"				},
	{0x13, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x13, 0x01, 0x06, 0x00, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x06, 0x02, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x06, 0x06, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x06, 0x04, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x13, 0x01, 0x08, 0x08, "3"				},
	{0x13, 0x01, 0x08, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x13, 0x01, 0x10, 0x10, "Rank A"			},
	{0x13, 0x01, 0x10, 0x00, "Rank B"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x13, 0x01, 0x60, 0x60, "10k 30k and every 50k"	},
	{0x13, 0x01, 0x60, 0x40, "20k and every 50k"		},
	{0x13, 0x01, 0x60, 0x20, "30k and every 70k"		},
	{0x13, 0x01, 0x60, 0x00, "20k 70k"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x80, 0x00, "Off"				},
	{0x13, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Motos)

static struct BurnDIPInfo SuperpacDIPList[]=
{
	{0x10, 0xff, 0xff, 0x0f, NULL				},
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x10, 0x01, 0x04, 0x04, "Upright"			},
	{0x10, 0x01, 0x04, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,   16, "Difficulty"			},
	{0x11, 0x01, 0x0f, 0x0f, "Rank 0-Normal"		},
	{0x11, 0x01, 0x0f, 0x0e, "Rank 1-Easiest"		},
	{0x11, 0x01, 0x0f, 0x0d, "Rank 2"			},
	{0x11, 0x01, 0x0f, 0x0c, "Rank 3"			},
	{0x11, 0x01, 0x0f, 0x0b, "Rank 4"			},
	{0x11, 0x01, 0x0f, 0x0a, "Rank 5"			},
	{0x11, 0x01, 0x0f, 0x09, "Rank 6-Medium"		},
	{0x11, 0x01, 0x0f, 0x08, "Rank 7"			},
	{0x11, 0x01, 0x0f, 0x07, "Rank 8-Default"		},
	{0x11, 0x01, 0x0f, 0x06, "Rank 9"			},
	{0x11, 0x01, 0x0f, 0x05, "Rank A"			},
	{0x11, 0x01, 0x0f, 0x04, "Rank B-Hardest"		},
	{0x11, 0x01, 0x0f, 0x03, "Rank C-Easy Auto"		},
	{0x11, 0x01, 0x0f, 0x02, "Rank D-Auto"			},
	{0x11, 0x01, 0x0f, 0x01, "Rank E-Auto"			},
	{0x11, 0x01, 0x0f, 0x00, "Rank F-Hard Auto"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x11, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x30, 0x00, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x40, 0x00, "Off"				},
	{0x11, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x11, 0x01, 0x80, 0x80, "Off"				},
	{0x11, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x12, 0x01, 0x07, 0x00, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x07, 0x01, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  7 Credits"		},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x12, 0x01, 0x38, 0x08, "30k Only"			},
	{0x12, 0x01, 0x38, 0x30, "30k & 80k Only"		},
	{0x12, 0x01, 0x38, 0x20, "30k, 80k & Every 80k"		},
	{0x12, 0x01, 0x38, 0x38, "30k & 100k Only"		},
	{0x12, 0x01, 0x38, 0x18, "30k, 100k & Every 100k"	},
	{0x12, 0x01, 0x38, 0x28, "30k & 120k Only"		},
	{0x12, 0x01, 0x38, 0x10, "30k, 120k & Every 120k"	},
	{0x12, 0x01, 0x38, 0x00, "None"				},
#if 0
	{0x12, 0x01, 0x38, 0x10, "30k Only"			},
	{0x12, 0x01, 0x38, 0x38, "30k & 100k Only"		},
	{0x12, 0x01, 0x38, 0x20, "30k, 100k & Every 100k"	},
	{0x12, 0x01, 0x38, 0x30, "30k & 120k Only"		},
	{0x12, 0x01, 0x38, 0x08, "40k Only"			},
	{0x12, 0x01, 0x38, 0x28, "40k & 120k Only"		},
	{0x12, 0x01, 0x38, 0x18, "40k, 120k & Every 120k"	},
#endif

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0xc0, 0x80, "1"				},
	{0x12, 0x01, 0xc0, 0x40, "2"				},
	{0x12, 0x01, 0xc0, 0xc0, "3"				},
	{0x12, 0x01, 0xc0, 0x00, "5"				},
};

STDDIPINFO(Superpac)

static struct BurnDIPInfo PacnpalDIPList[]=
{
	{0x10, 0xff, 0xff, 0x0f, NULL				},
	{0x11, 0xff, 0xff, 0x77, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x10, 0x01, 0x04, 0x04, "Upright"			},
	{0x10, 0x01, 0x04, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x11, 0x01, 0x07, 0x00, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x01, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  6 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "1 Coin  7 Credits"		},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x11, 0x01, 0x38, 0x20, "20k & 70k Only"		},
	{0x11, 0x01, 0x38, 0x30, "20k, 70k & Every 70k"		},
	{0x11, 0x01, 0x38, 0x00, "30k Only"			},
	{0x11, 0x01, 0x38, 0x18, "30k & 70k Only"		},
	{0x11, 0x01, 0x38, 0x10, "30k & 80k Only"		},
	{0x11, 0x01, 0x38, 0x28, "30k, 100k & Every 80k"	},
	{0x11, 0x01, 0x38, 0x08, "30k & 100k Only"		},
#if 0
	{0x11, 0x01, 0x38, 0x08, "30k Only"			},
	{0x11, 0x01, 0x38, 0x00, "40k Only"			},
	{0x11, 0x01, 0x38, 0x20, "30k & 80k Only"		},
	{0x11, 0x01, 0x38, 0x30, "30k, 80k & Every 80k"		},
	{0x11, 0x01, 0x38, 0x18, "30k & 100k Only"		},
	{0x11, 0x01, 0x38, 0x10, "40k & 120k Only"		},
	{0x11, 0x01, 0x38, 0x28, "40k, 100k & Every 100k"	},
#endif
	{0x11, 0x01, 0x38, 0x38, "None"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x11, 0x01, 0xc0, 0xc0, "1"				},
	{0x11, 0x01, 0xc0, 0x80, "2"				},
	{0x11, 0x01, 0xc0, 0x40, "3"				},
	{0x11, 0x01, 0xc0, 0x00, "5"				},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x12, 0x01, 0x03, 0x01, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x03, 0x00, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x0c, 0x0c, "Rank A"			},
	{0x12, 0x01, 0x0c, 0x08, "Rank B"			},
	{0x12, 0x01, 0x0c, 0x04, "Rank C"			},
	{0x12, 0x01, 0x0c, 0x00, "Rank D"			},
};

STDDIPINFO(Pacnpal)

static struct BurnDIPInfo GrobdaDIPList[]=
{
	{0x12, 0xff, 0xff, 0x0f, NULL				},
	{0x13, 0xff, 0xff, 0xc9, NULL				},
	{0x14, 0xff, 0xff, 0x3f, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x12, 0x01, 0x04, 0x04, "Upright"			},
	{0x12, 0x01, 0x04, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x01, 0x01, "Off"				},
	{0x13, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0x0e, 0x00, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x0e, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x0e, 0x06, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x0e, 0x08, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x0e, 0x04, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0x0e, 0x0a, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x0e, 0x0e, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x0e, 0x0c, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x70, 0x00, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x70, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x70, 0x30, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x70, 0x40, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x70, 0x20, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0x70, 0x50, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x70, 0x70, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x70, 0x60, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x13, 0x01, 0x80, 0x80, "Off"				},
	{0x13, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x14, 0x01, 0x03, 0x02, "1"				},
	{0x14, 0x01, 0x03, 0x01, "2"				},
	{0x14, 0x01, 0x03, 0x03, "3"				},
	{0x14, 0x01, 0x03, 0x00, "5"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x0c, 0x0c, "Rank A"			},
	{0x14, 0x01, 0x0c, 0x08, "Rank B"			},
	{0x14, 0x01, 0x0c, 0x04, "Rank C"			},
	{0x14, 0x01, 0x0c, 0x00, "Rank D"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x10, 0x00, "Off"				},
	{0x14, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Level Select"			},
	{0x14, 0x01, 0x20, 0x00, "Off"				},
	{0x14, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x14, 0x01, 0xc0, 0x00, "10k, 50k & Every 50k"		},
	{0x14, 0x01, 0xc0, 0x40, "10k & 30k Only"		},
	{0x14, 0x01, 0xc0, 0xc0, "10k Only"			},
	{0x14, 0x01, 0xc0, 0x80, "None"				},
};

STDDIPINFO(Grobda)

static struct BurnDIPInfo PhozonDIPList[]=
{
	{0x10, 0xff, 0xff, 0x0f, NULL				},
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x10, 0x01, 0x04, 0x04, "Upright"			},
	{0x10, 0x01, 0x04, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x11, 0x01, 0x01, 0x01, "Off"				},
	{0x11, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x11, 0x01, 0x0e, 0x0e, "Rank 0"			},
	{0x11, 0x01, 0x0e, 0x0c, "Rank 1"			},
	{0x11, 0x01, 0x0e, 0x0a, "Rank 2"			},
	{0x11, 0x01, 0x0e, 0x08, "Rank 3"			},
	{0x11, 0x01, 0x0e, 0x06, "Rank 4"			},
	{0x11, 0x01, 0x0e, 0x04, "Rank 5"			},
	{0x11, 0x01, 0x0e, 0x02, "Rank 6"			},
	{0x11, 0x01, 0x0e, 0x00, "Rank 7"			},

	{0   , 0xfe, 0   ,    2, "Level Select (Cheat)"		},
	{0x11, 0x01, 0x10, 0x10, "Off"				},
	{0x11, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Stop Mode (Cheat)"		},
	{0x11, 0x01, 0x20, 0x20, "Off"				},
	{0x11, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x11, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x03, 0x02, "1"				},
	{0x12, 0x01, 0x03, 0x03, "3"				},
	{0x12, 0x01, 0x03, 0x01, "4"				},
	{0x12, 0x01, 0x03, 0x00, "5"				},

	{0   , 0xfe, 0   ,    7, "Bonus Life"			},
	{0x12, 0x01, 0x1c, 0x08, "20k & 80k Only"		},
	{0x12, 0x01, 0x1c, 0x10, "20k, 80k & Every 80k"		},
	{0x12, 0x01, 0x1c, 0x04, "30k Only"			},
	{0x12, 0x01, 0x1c, 0x18, "30k & 60k Only"		},
	{0x12, 0x01, 0x1c, 0x1c, "30k & 100k Only"		},
	{0x12, 0x01, 0x1c, 0x0c, "30k, 120k & Every 120k"	},
#if 0
	{0x12, 0x01, 0x1c, 0x0c, "20k & 80k Only"		},
	{0x12, 0x01, 0x1c, 0x08, "30k"				},
	{0x12, 0x01, 0x1c, 0x10, "30k, 100k & Every 100k"	},
	{0x12, 0x01, 0x1c, 0x1c, "30k & 100k Only"		},
	{0x12, 0x01, 0x1c, 0x18, "40k & 80k Only"		},
	{0x12, 0x01, 0x1c, 0x04, "100k Only"			},
#endif
	{0x12, 0x01, 0x1c, 0x00, "None"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x12, 0x01, 0xe0, 0x00, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x40, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xe0, 0x20, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xe0, 0x80, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0xe0, 0x60, "1 Coin  7 Credits"		},
};

STDDIPINFO(Phozon)

static void mappy_latch_write(INT32 cpu, INT32 offset)
{
	INT32 bit = offset & 1;

	switch (offset & 0x0e)
	{
		case 0x00:
			sub_irq_mask = bit;
			if (bit == 0) {
				if (cpu == 0) {
					M6809Close();
					M6809Open(1);
					M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
					M6809Close();
					M6809Open(0);
				} else {
					M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
				}
			}
		break;

		case 0x02:
			main_irq_mask = bit;
			if (bit == 0) {
				if (cpu == 1) {
					M6809Close();
					M6809Open(0);
					M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
					M6809Close();
					M6809Open(1);
				} else {
					M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
				}
			}
		break;

		case 0x04:
			flipscreen = bit;
		break;

		case 0x06:
			namco_15xx_sound_enable(bit);
		break;

		case 0x08:
			namcoio_set_reset_line(0, ~bit & 1);
			namcoio_set_reset_line(1, ~bit & 1);
		break;

		case 0x0a:
			sub_cpu_in_reset = ~bit & 1;
			if (bit == 0)
			{
				if (cpu == 0) {
					M6809Close();
					M6809Open(1);
					M6809Reset();
					M6809Close();
					M6809Open(0);
				} else {
					M6809Reset();
				}
			}
		break;
	}
}

static void phozon_latch_write(INT32 cpu, INT32 offset)
{
	INT32 bit = offset & 1;

	switch (offset & 0x0e)
	{
		case 0x04:
			if (bit == 0) {
				M6809Close();
				M6809Open(2);
				M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
				M6809Close();
				M6809Open(0);
			}
			sub2_irq_mask = bit;
		return;

		case 0x06:
		return;

		case 0x0c:
			sub2_cpu_in_reset = ~bit & 1;
			if (bit == 0)
			{
				M6809Close();
				M6809Open(2);
				M6809Reset();
				M6809Close();
				M6809Open(0);
			}
		return;
	}

	mappy_latch_write(cpu, offset);
}

static void mappy_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0x3800) {
		scroll = (address & 0x7f8) / 8;
		return;
	}

	if ((address & 0xfc00) == 0x4000) {
		namco_15xx_sharedram_write(address,data);
		return;
	}

	if ((address & 0xfff0) == 0x4800) {
		namcoio_write(0, address & 0xf, data);
		return;
	}

	if ((address & 0xfff0) == 0x4810) {
		namcoio_write(1, address & 0xf, data);
		return;
	}

	if ((address & 0xfff0) == 0x5000) {
		mappy_latch_write(0, address);
		return;
	}

	if (address == 0x8000) {
		watchdog = 0;
		return;
	}
}

static UINT8 mappy_main_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x4000) {
		return namco_15xx_sharedram_read(address);
	}

	if ((address & 0xfff0) == 0x4800) {
		return namcoio_read(0, address & 0xf);
	}

	if ((address & 0xfff0) == 0x4810) {
		return namcoio_read(1, address & 0xf);
	}

	return 0;
}

static void phozon_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x4000) {
		namco_15xx_sharedram_write(address,data);
		return;
	}

	if ((address & 0xfff0) == 0x4800) {
		namcoio_write(0, address & 0xf, data);
		return;
	}

	if ((address & 0xfff0) == 0x4810) {
		namcoio_write(1, address & 0xf, data);
		return;
	}

	if ((address & 0xfff0) == 0x5000) {
		phozon_latch_write(0, address);
		return;
	}

	if (address == 0x7000) {
		watchdog = 0;
		return;
	}
}

static UINT8 phozon_main_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x4000) {
		return namco_15xx_sharedram_read(address);
	}

	if ((address & 0xfff0) == 0x4800) {
		return namcoio_read(0, address & 0xf);
	}

	if ((address & 0xfff0) == 0x4810) {
		return namcoio_read(1, address & 0xf);
	}

	return 0;
}

static void superpac_main_write(UINT16 address, UINT8 data)
{
	if (address == 0x2000) {
		flipscreen = data & 1;
		return;
	}

	if ((address & 0xfff0) == 0x5000) {
		mappy_latch_write(0, address);
		return;
	}

	mappy_main_write(address,data);
}

static UINT8 superpac_main_read(UINT16 address)
{
	if (address == 0x2000) {
		flipscreen = 1;
		return 0xff;
	}

	return mappy_main_read(address);
}

static void mappy_sub_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x0000) {
		namco_15xx_sharedram_write(address,data);
		return;
	}

	if ((address & 0xfff0) == 0x2000) {
		mappy_latch_write(1, address);
		return;
	}
}

static UINT8 mappy_sub_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x0000) {
		return namco_15xx_sharedram_read(address);
	}

	return 0;
}

static void superpac_sub_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x2000) {
		mappy_latch_write(1, address);
		return;
	}

	mappy_sub_write(address, data);
}

static void grobda_sub_write(UINT16 address, UINT8 data)
{
	if (address == 0x0002) {
		DACWrite(0, data);
	}

	if ((address & 0xfff0) == 0x2000) {
		mappy_latch_write(1, address);
		return;
	}

	mappy_sub_write(address, data);
}


static UINT8 nio0_i0(UINT8) { return DrvInputs[3] & 0xf; } // COINS
static UINT8 nio0_i1(UINT8) { return DrvInputs[0] & 0xf; } // P1
static UINT8 nio0_i2(UINT8) { return DrvInputs[1] & 0xf; } // P2
static UINT8 nio0_i3(UINT8) { return DrvInputs[2] & 0xf; } // BUTTONS

static UINT8 nio1_i0(UINT8) { return DrvInputs[6] >> (out_mux * 4); }
static UINT8 nio1_i0b(UINT8) { return BITSWAP08(DrvInputs[6], 6,4,2,0,7,5,3,1) >> (out_mux * 4); }
static UINT8 nio1_i1(UINT8) { return DrvInputs[5] & 0xf; }
static UINT8 nio1_i2(UINT8) { return DrvInputs[5] >> 4; }
static UINT8 nio1_i3(UINT8) { return DrvInputs[4]; }
static void nio1_o0(UINT8, UINT8 data) { out_mux = data & 1; }

static tilemap_scan( mappy_bg )
{
	INT32 offs;

	col -= 2;
	if (col & 0x20)
	{
		if (row & 0x20)
			offs = 0x7ff;   // outside visible area
		else
			offs = ((row + 2) & 0x0f) + (row & 0x10) + ((col & 3) << 5) + 0x780;
	}
	else
		offs = col + (row << 5);

	return offs;
}

static tilemap_scan( superpac_bg )
{
	INT32 offs;

	row += 2;
	col -= 2;
	if (col & 0x20)
		offs = row + ((col & 0x1f) << 5);
	else
		offs = col + (row << 5);

	return offs;
}

static tilemap_callback( mappy_bg )
{
	UINT8 code = DrvVidRAM[offs];
	UINT8 attr = DrvVidRAM[offs + 0x800];

	TILE_SET_INFO(0, code, attr, TILE_GROUP((attr >> 6) & 1));
	*category = attr & 0x3f;
}

static tilemap_callback( superpac_bg )
{
	UINT8 code = DrvVidRAM[offs];
	UINT8 attr = DrvVidRAM[offs + 0x400];

	TILE_SET_INFO(0, code + ((attr & 0x80) << 1), attr, TILE_GROUP((attr >> 6) & 1));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	for (INT32 i = 0; i < 0x10; i+=2) {
		M6809WriteRom(0x5000 + i, 0); // send through the game's write handler
	}
	M6809Close();

	M6809Open(1);
	M6809Reset();
	NamcoSoundReset();
	DACReset(); // grobda
	M6809Close();

	M6809Open(2); // phozon
	M6809Reset();
	M6809Close();

	namcoio_reset(0);
	namcoio_reset(1);

	HiscoreReset();

	scroll = 0;
	out_mux = 0;
	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0		= Next; Next += 0x008000;
	DrvM6809ROM1		= Next; Next += 0x002000;
	DrvM6809ROM2		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000520;

	NamcoSoundProm		= Next;
	DrvSndPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x0500 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x001800;
	DrvM6809RAM2		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 decode_type)
{
	INT32 Plane[4]   = { 0, 4, 0x4000*8, 0x4000*8+4 };
	INT32 XOffs0[8]  = { STEP4(64,1), STEP4(0,1) };
	INT32 XOffs1[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs[16]  = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs0, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x8000);

	if (decode_type) { // phozon
		GfxDecode(0x0200, 2,  8,  8, Plane, XOffs1, YOffs, 0x080, tmp, DrvGfxROM1);
	} else {
		GfxDecode(0x0100, 4, 16, 16, Plane, XOffs1, YOffs, 0x200, tmp, DrvGfxROM1);
	}

	BurnFree(tmp);

	return 0;
}

static INT32 MappyInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM0 + 0x2000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x4000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x6000,  2, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x0000,  3, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0   + 0x0000,  4, 1, LD_INVERT)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x4000,  6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0020,  8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0120,  9, 1)) return 1;

		if (BurnLoadRom(DrvSndPROM   + 0x0000, 10, 1)) return 1;

		DrvGfxDecode(0);
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM,		0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x1000, 0x27ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(mappy_main_write);
	M6809SetReadHandler(mappy_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809ROM1,		0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(mappy_sub_write);
	M6809SetReadHandler(mappy_sub_read);
	M6809Close();

	M6809Init(2);
	// not used

	NamcoSoundInit(24000, 8, 0);
	NacmoSoundSetAllRoutes(0.50 * 10.0 / 16.0, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6809TotalCycles, 1536000); // not used on this hardware
	DACSetRoute(0, 0.0, BURN_SND_ROUTE_BOTH);

	namcoio_init(0, NAMCO58xx, nio0_i0, nio0_i1, nio0_i2, nio0_i3, NULL,    NULL);
	namcoio_init(1, NAMCO58xx, nio1_i0, nio1_i1, nio1_i2, nio1_i3, nio1_o0, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, mappy_bg_map_scan, mappy_bg_map_callback, 8, 8, 36, 60);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x1000*4, 0, 0x3f);
	GenericTilemapSetScrollCols(0, 36);
	GenericTilemapCategoryConfig(0, 0x40);
	for (INT32 i = 0; i < 0x40 * 4; i++) {
		GenericTilemapSetCategoryEntry(0, i / 4, i % 4, ((DrvColPROM[i + 0x020] & 0xf) == 0xf) ? 1 : 0);
	}

	DrvDoReset();

	return 0;
}

static INT32 Digdug2Init()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x4000,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x0000,  2, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0   + 0x0000,  3, 1, LD_INVERT)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x4000,  5, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0020,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0120,  8, 1)) return 1;

		if (BurnLoadRom(DrvSndPROM   + 0x0000,  9, 1)) return 1;

		DrvGfxDecode(0);
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM,		0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x1000, 0x27ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(mappy_main_write);
	M6809SetReadHandler(mappy_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809ROM1,		0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(mappy_sub_write);
	M6809SetReadHandler(mappy_sub_read);
	M6809Close();

	M6809Init(2); // not used

	NamcoSoundInit(24000, 8, 0);
	NacmoSoundSetAllRoutes(0.50 * 10.0 / 16.0, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6809TotalCycles, 1536000); // not used on this hardware
	DACSetRoute(0, 0.0, BURN_SND_ROUTE_BOTH);

	namcoio_init(0, NAMCO58xx, nio0_i0, nio0_i1, nio0_i2, nio0_i3, NULL,    NULL);
	namcoio_init(1, NAMCO56xx, nio1_i0, nio1_i1, nio1_i2, nio1_i3, nio1_o0, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, mappy_bg_map_scan, mappy_bg_map_callback, 8, 8, 36, 60);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x1000*4, 0, 0x3f);
	GenericTilemapSetScrollCols(0, 36);
	GenericTilemapCategoryConfig(0, 0x40);
	for (INT32 i = 0; i < 0x40 * 4; i++) {
		GenericTilemapSetCategoryEntry(0, i / 4, i % 4, ((DrvColPROM[i + 0x020] & 0xf) == 0xf) ? 1 : 0);
	}

	fourwaymode = 1;

	DrvDoReset();

	return 0;
}

static INT32 MotosInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x4000,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x0000,  2, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0   + 0x0000,  3, 1, LD_INVERT)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x4000,  5, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0020,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0120,  8, 1)) return 1;

		if (BurnLoadRom(DrvSndPROM   + 0x0000,  9, 1)) return 1;

		DrvGfxDecode(0);
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM,		0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x1000, 0x27ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(mappy_main_write);
	M6809SetReadHandler(mappy_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809ROM1,		0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(mappy_sub_write);
	M6809SetReadHandler(mappy_sub_read);
	M6809Close();

	M6809Init(2); // not used

	NamcoSoundInit(24000, 8, 0);
	NacmoSoundSetAllRoutes(0.50 * 10.0 / 16.0, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6809TotalCycles, 1536000); // not used on this hardware
	DACSetRoute(0, 0.0, BURN_SND_ROUTE_BOTH);

	namcoio_init(0, NAMCO56xx, nio0_i0, nio0_i1, nio0_i2, nio0_i3, NULL,    NULL);
	namcoio_init(1, NAMCO56xx, nio1_i0, nio1_i1, nio1_i2, nio1_i3, nio1_o0, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, mappy_bg_map_scan, mappy_bg_map_callback, 8, 8, 36, 60);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x1000*4, 0, 0x3f);
	GenericTilemapSetScrollCols(0, 36);
	GenericTilemapCategoryConfig(0, 0x40);
	for (INT32 i = 0; i < 0x40 * 4; i++) {
		GenericTilemapSetCategoryEntry(0, i / 4, i % 4, ((DrvColPROM[i + 0x020] & 0xf) == 0xf) ? 1 : 0);
	}

	DrvDoReset();

	return 0;
}

static INT32 SuperpacInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM0 + 0x2000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x4000,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x1000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x4000,  4, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000,  5, 1)) return 1;
		if (BurnLoadRomExt(DrvColPROM   + 0x0020,  6, 1, LD_INVERT)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0120,  7, 1)) return 1;

		if (BurnLoadRom(DrvSndPROM   + 0x0000,  8, 1)) return 1;

		DrvGfxDecode(0);
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x0800, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,		0xa000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(superpac_main_write);
	M6809SetReadHandler(superpac_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809ROM1,		0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(superpac_sub_write);
	M6809SetReadHandler(mappy_sub_read);
	M6809Close();

	M6809Init(2); // not used

	NamcoSoundInit(24000, 8, 0);
	NacmoSoundSetAllRoutes(0.50 * 10.0 / 16.0, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6809TotalCycles, 1536000); // not used on this hardware
	DACSetRoute(0, 0.0, BURN_SND_ROUTE_BOTH);

	namcoio_init(0, NAMCO56xx, nio0_i0, nio0_i1, nio0_i2, nio0_i3, NULL,    NULL);
	namcoio_init(1, NAMCO56xx, nio1_i0, nio1_i1, nio1_i2, nio1_i3, nio1_o0, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, superpac_bg_map_scan, superpac_bg_map_callback, 8, 8, 36, 28);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x1000*4, 0, 0x3f);

	fourwaymode = 1;

	DrvDoReset();

	return 0;
}

static INT32 PacnpalInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x4000,  2, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x1000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x4000,  5, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000,  6, 1)) return 1;
		if (BurnLoadRomExt(DrvColPROM   + 0x0020,  7, 1, LD_INVERT)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0120,  8, 1)) return 1;

		if (BurnLoadRom(DrvSndPROM   + 0x0000,  9, 1)) return 1;

		DrvGfxDecode(0);
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x0800, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,		0xa000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(superpac_main_write);
	M6809SetReadHandler(superpac_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809ROM1,		0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(superpac_sub_write);
	M6809SetReadHandler(mappy_sub_read);
	M6809Close();

	M6809Init(2); // not used

	NamcoSoundInit(24000, 8, 0);
	NacmoSoundSetAllRoutes(0.50 * 10.0 / 16.0, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6809TotalCycles, 1536000); // not used on this hardware
	DACSetRoute(0, 0.0, BURN_SND_ROUTE_BOTH);

	namcoio_init(0, NAMCO56xx, nio0_i0, nio0_i1, nio0_i2, nio0_i3, NULL,    NULL);
	namcoio_init(1, NAMCO59xx, nio1_i0, nio1_i1, nio1_i2, nio1_i3, nio1_o0, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, superpac_bg_map_scan, superpac_bg_map_callback, 8, 8, 36, 28);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x1000*4, 0, 0x3f);

	fourwaymode = 1;

	DrvDoReset();

	return 0;
}

static INT32 GrobdaInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x4000,  2, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x4000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x6000,  6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000,  7, 1)) return 1;
		if (BurnLoadRomExt(DrvColPROM   + 0x0020,  8, 1, LD_INVERT)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0120,  9, 1)) return 1;

		if (BurnLoadRom(DrvSndPROM   + 0x0000, 10, 1)) return 1;

		DrvGfxDecode(0);
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x0800, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,		0xa000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(superpac_main_write);
	M6809SetReadHandler(superpac_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809ROM1,		0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(grobda_sub_write);
	M6809SetReadHandler(mappy_sub_read);
	M6809Close();

	M6809Init(2); // not used

	NamcoSoundInit(24000, 8, 0);
	NacmoSoundSetAllRoutes(0.50 * 10.0 / 16.0, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6809TotalCycles, 1536000);
	DACSetRoute(0, 2.6, BURN_SND_ROUTE_BOTH);

	grobdamode = 1; // for dc filter

	namcoio_init(0, NAMCO58xx, nio0_i0, nio0_i1, nio0_i2, nio0_i3, NULL,    NULL);
	namcoio_init(1, NAMCO56xx, nio1_i0, nio1_i1, nio1_i2, nio1_i3, nio1_o0, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, superpac_bg_map_scan, superpac_bg_map_callback, 8, 8, 36, 28);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x1000*4, 0, 0x3f);

	DrvDoReset();

	return 0;
}

static INT32 PhozonInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM2 + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x1000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x0000,  8, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0100, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0200, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0300, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0400, 13, 1)) return 1;
//		if (BurnLoadRom(DrvColPROM   + 0x0500, 14, 1)) return 1;

		if (BurnLoadRom(DrvSndPROM   + 0x0000, 15, 1)) return 1;

		DrvGfxDecode(1);
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x0800, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(phozon_main_write);
	M6809SetReadHandler(phozon_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809ROM1,		0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(mappy_sub_write);
	M6809SetReadHandler(mappy_sub_read);
	M6809Close();

	M6809Init(2);
	M6809Open(2);
	M6809MapMemory(DrvVidRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x0800, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM2,		0xa000, 0xa7ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM2,		0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(mappy_main_write);
	M6809SetReadHandler(mappy_main_read);
	M6809Close();

	NamcoSoundInit(24000, 8, 0);
	NacmoSoundSetAllRoutes(0.50 * 10.0 / 16.0, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6809TotalCycles, 1536000); // not used on this hardware
	DACSetRoute(0, 0.0, BURN_SND_ROUTE_BOTH);

	namcoio_init(0, NAMCO58xx, nio0_i0, nio0_i1, nio0_i2, nio0_i3, NULL,    NULL);
	namcoio_init(1, NAMCO56xx, nio1_i0b, nio1_i1, nio1_i2, nio1_i3, nio1_o0, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, superpac_bg_map_scan, superpac_bg_map_callback, 8, 8, 36, 28);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x2000*4, 0, 0x3f);
	GenericTilemapSetTransparent(0, 0x00);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();

	NamcoSoundExit();
	NamcoSoundProm = NULL;

	DACExit();

	BurnFree(AllMem);

	fourwaymode = 0;
	grobdamode = 0;

	return 0;
}

static void MappyPaletteInit()
{
	UINT32 pal[32];

	for (INT32 i = 0; i < 32; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = (((bit0 * 220) + (bit1 * 470) + (bit2 * 1000)) * 255) / 1690;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = (((bit0 * 220) + (bit1 * 470) + (bit2 * 1000)) * 255) / 1690;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = (((bit0 * 470) + (bit1 * 1000)) * 255) / 1470;

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 256; i++)
	{
		DrvPalette[i + 0x000] = pal[(DrvColPROM[i + 0x020] & 0xf) + 0x10];
	}

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries() - 0x100; i++)
	{
		DrvPalette[i + 0x100] = pal[(DrvColPROM[i + 0x120] & 0xf)];
	}
}

static void PhozonPaletteInit()
{
	UINT32 pal[32];

	for (INT32 i = 0; i < 32; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i] >> 3) & 0x01;
		INT32 r = (((bit0 * 220) + (bit1 * 470) + (bit2 * 1000) + (bit3 * 2200)) * 255) / 3890;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = (((bit0 * 220) + (bit1 * 470) + (bit2 * 1000) + (bit3 * 2200)) * 255) / 3890;

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		INT32 b = (((bit0 * 220) + (bit1 * 470) + (bit2 * 1000) + (bit3 * 2200)) * 255) / 3890;

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 256; i++)
	{
		DrvPalette[i + 0x000] = pal[(DrvColPROM[i + 0x300] & 0xf)];
		DrvPalette[i + 0x100] = pal[(DrvColPROM[i + 0x400] & 0xf) + 0x10];
	}
}

static void mappy_draw_sprites(INT32 depth, INT32 priority)
{
	UINT8 *ram0 = DrvSprRAM + 0x780;
	UINT8 *ram1 = ram0 + 0x800;
	UINT8 *ram2 = ram1 + 0x800;

	for (INT32 offs = 0; offs < 0x80; offs += 2)
	{
		if ((ram2[offs+1] & 2) == 0)
		{
			static const UINT8 gfx_offs[2][2] =
			{
				{ 0, 1 },
				{ 2, 3 }
			};
			INT32 sprite = ram0[offs];
			INT32 color = (ram0[offs+1] << depth) & 0x3ff;
			INT32 sx = ram1[offs+1] + 0x100 * (ram2[offs+1] & 1) - 40;
			INT32 sy = 256 - ram1[offs] + 1;
			INT32 flipx = (ram2[offs] & 0x01);
			INT32 flipy = (ram2[offs] & 0x02) >> 1;
			INT32 sizex = (ram2[offs] & 0x04) >> 2;
			INT32 sizey = (ram2[offs] & 0x08) >> 3;

			sprite &= ~sizex;
			sprite &= ~(sizey << 1);

			sy = ((sy - (16 * sizey)) & 0xff) - 32;

			if (flipscreen)
			{
				flipx ^= 1;
				flipy ^= 1;
			}

			for (INT32 y = 0; y <= sizey; y++)
			{
				for (INT32 x = 0; x <= sizex; x++)
				{
					INT32 code = (sprite + gfx_offs[y ^ (sizey * flipy)][x ^ (sizex * flipx)]);

					if (priority) // pens 0 & 1 are over the char layer in superpac hardware
					{
						UINT8 *gfx = DrvGfxROM1 + (code * 0x100);
						INT32 flip = (flipy ? 0xf0 : 0) | (flipx ? 0x0f : 0);

						INT32 dy = sy + 16 * y;
						INT32 dx = sx + 16 * x;

						for (INT32 yy = 0; yy < 16; yy++) {
							if ((yy+dy) < 0 || (yy+dy) >= nScreenHeight) continue;

							for (INT32 xx = 0; xx < 16; xx++)
							{
								if ((xx+dx) < 0 || (xx+dx) >= nScreenWidth) continue;

								INT32 pxl = gfx[((yy*16)+xx)^flip] + color;

								INT32 col = DrvColPROM[0x120 + pxl];

								if (pxl && col < 2) {
									pTransDraw[(dy + yy) * nScreenWidth + dx + xx] = pxl + 0x100;
								}
							}
						}
					} else {
						RenderTileTranstab(pTransDraw, DrvGfxROM1, code, color+0x100, 0xf, sx + 16 * x, sy + 16 * y, flipx, flipy, 16, 16, DrvColPROM + 0x020);
					}
				}
			}
		}
	}
}

static void phozon_draw_sprites()
{
	UINT8 *ram0 = DrvSprRAM + 0x780;
	UINT8 *ram1 = ram0 + 0x800;
	UINT8 *ram2 = ram1 + 0x800;

	for (INT32 offs = 0; offs < 0x80; offs += 2)
	{
		if ((ram2[offs+1] & 2) == 0)
		{
			static const UINT8 size[4] = { 1, 0, 3, 0 };
			static const UINT8 gfx_offs[4][4] =
			{
				{ 0, 1, 4, 5 },
				{ 2, 3, 6, 7 },
				{ 8, 9,12,13 },
				{10,11,14,15 }
			};
			INT32 sprite = (ram0[offs] << 2) | ((ram2[offs] & 0xc0) >> 6);
			INT32 color = ram0[offs+1] & 0x3f;
			INT32 sx = ram1[offs+1] + 0x100 * (ram2[offs+1] & 1) - 69;
			INT32 sy = 256 - ram1[offs];
			INT32 flipx = (ram2[offs] & 0x01);
			INT32 flipy = (ram2[offs] & 0x02) >> 1;
			INT32 sizex = size[(ram2[offs] & 0x0c) >> 2];
			INT32 sizey = size[(ram2[offs] & 0x30) >> 4];

			sy = ((sy - (8 * sizey)) & 0xff) - 32;

			if (flipscreen)
			{
				flipx ^= 1;
				flipy ^= 1;
			}

			for (INT32 y = 0; y <= sizey; y++)
			{
				for (INT32 x = 0; x <= sizex; x++)
				{
					INT32 code = sprite + gfx_offs[y ^ (sizey * flipy)][x ^ (sizex * flipx)];

					RenderTileTranstab(pTransDraw, DrvGfxROM1, code, (color*4)+0x100, 0x0f, sx + 8 * x, sy + 8 * y, flipx, flipy, 8, 8, DrvColPROM + 0x300);
				}
			}
		}
	}
}

static INT32 MappyDraw()
{
	if (DrvRecalc) {
		MappyPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetFlip(0, flipscreen);

	for (INT32 offs = 2; offs < 34; offs++) {
		GenericTilemapSetScrollCol(0, offs, scroll);
	}

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nSpriteEnable & 1) mappy_draw_sprites(4, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 SuperpacDraw()
{
	if (DrvRecalc) {
		MappyPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetFlip(0, flipscreen);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nSpriteEnable & 1) mappy_draw_sprites(2, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));
	if (nSpriteEnable & 2) mappy_draw_sprites(2, 1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 PhozonDraw()
{
	if (DrvRecalc) {
		PhozonPaletteInit();
		DrvRecalc = 0;
	}

	flipscreen = DrvSprRAM[0x1f7f-0x800] & 1;

	BurnTransferClear();

	GenericTilemapSetFlip(0, flipscreen);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0 | TMAP_FORCEOPAQUE);
	if (nSpriteEnable & 1) phozon_draw_sprites();
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();

	{
		memset (DrvInputs, 0xff, 4);

		DrvInputs[4] = DrvDips[0];
		DrvInputs[5] = DrvDips[1];
		DrvInputs[6] = DrvDips[2];

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}

		if (fourwaymode) {
			ProcessJoystick(&DrvInputs[0], 0, 0,2,3,1, INPUT_4WAY | INPUT_ISACTIVELOW);
			ProcessJoystick(&DrvInputs[1], 1, 0,2,3,1, INPUT_4WAY | INPUT_ISACTIVELOW);
		}
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[3] = { (INT32)((double)1536000 / 60.606061), (INT32)((double)1536000 / 60.606061), (INT32)((double)1536000 / 60.606061) };
	INT32 nCyclesDone[3] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = 0;

		M6809Open(0);
		nCyclesDone[0] += M6809Run(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		if (i == nInterleave-1) {
			if (main_irq_mask)
				M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);

			if (!namcoio_read_reset_line(0))
				namcoio_run(0);

			if (!namcoio_read_reset_line(1))
				namcoio_run(1);
		}
		nSegment = M6809TotalCycles();
		M6809Close();

		if (sub_cpu_in_reset) {
			nCyclesDone[1] += ((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1];
		} else {
			M6809Open(1);
			nCyclesDone[1] += M6809Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
			if (i == nInterleave-1 && sub_irq_mask) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6809Close();
		}

		if (sub2_cpu_in_reset) {
			nCyclesDone[2] += ((i + 1) * nCyclesTotal[2] / nInterleave) - nCyclesDone[2];
		} else {
			M6809Open(2);
			nCyclesDone[2] += M6809Run(((i + 1) * nCyclesTotal[2] / nInterleave) - nCyclesDone[2]);
			if (i == nInterleave-1 && sub2_irq_mask) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6809Close();
		}
	}

	if (pBurnSoundOut) {
		NamcoSoundUpdate(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		if (grobdamode) BurnSoundDCFilter();
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

		M6809Scan(nAction);
		NamcoSoundScan(nAction, pnMin);
		DACScan(nAction, pnMin);

		namcoio_scan(0);
		namcoio_scan(1);

		SCAN_VAR(sub_cpu_in_reset);
		SCAN_VAR(sub2_cpu_in_reset);
		SCAN_VAR(scroll);
		SCAN_VAR(main_irq_mask);
		SCAN_VAR(sub_irq_mask);
		SCAN_VAR(sub2_irq_mask);
		SCAN_VAR(flipscreen);
		SCAN_VAR(out_mux);
	}

	return 0;
}


// Mappy (US)

static struct BurnRomInfo mappyRomDesc[] = {
	{ "mpx_3.1d",		0x2000, 0x52e6c708, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "mp1_2.1c",		0x2000, 0xa958a61c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpx_1.1b",		0x2000, 0x203766d4, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mp1_4.1k",		0x2000, 0x8182dd5b, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code

	{ "mp1_5.3b",		0x1000, 0x16498b9f, 3 | BRF_GRA },           //  4 Background Tiles

	{ "mp1_6.3m",		0x2000, 0xf2d9647a, 4 | BRF_GRA },           //  5 Sprites
	{ "mp1_7.3n",		0x2000, 0x757cf2b6, 4 | BRF_GRA },           //  6

	{ "mp1-5.5b",		0x0020, 0x56531268, 5 | BRF_GRA },           //  7 Color Data
	{ "mp1-6.4c",		0x0100, 0x50765082, 5 | BRF_GRA },           //  8
	{ "mp1-7.5k",		0x0100, 0x5396bd78, 5 | BRF_GRA },           //  9

	{ "mp1-3.3m",		0x0100, 0x16a9166a, 6 | BRF_GRA },           // 10 Sound Prom
};

STD_ROM_PICK(mappy)
STD_ROM_FN(mappy)

struct BurnDriver BurnDrvMappy = {
	"mappy", NULL, NULL, NULL, "1983",
	"Mappy (US)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, mappyRomInfo, mappyRomName, NULL, NULL, NULL, NULL, MappyInputInfo, MappyDIPInfo,
	MappyInit, DrvExit, DrvFrame, MappyDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Mappy (Japan)

static struct BurnRomInfo mappyjRomDesc[] = {
	{ "mp1_3.1d",		0x2000, 0xdb9d5ab5, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "mp1_2.1c",		0x2000, 0xa958a61c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mp1_1.1b",		0x2000, 0x77c0b492, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mp1_4.1k",		0x2000, 0x8182dd5b, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code

	{ "mp1_5.3b",		0x1000, 0x16498b9f, 3 | BRF_GRA },           //  4 Background Tiles

	{ "mp1_6.3m",		0x2000, 0xf2d9647a, 4 | BRF_GRA },           //  5 Sprites
	{ "mp1_7.3n",		0x2000, 0x757cf2b6, 4 | BRF_GRA },           //  6

	{ "mp1-5.5b",		0x0020, 0x56531268, 5 | BRF_GRA },           //  7 Color Data
	{ "mp1-6.4c",		0x0100, 0x50765082, 5 | BRF_GRA },           //  8
	{ "mp1-7.5k",		0x0100, 0x5396bd78, 5 | BRF_GRA },           //  9

	{ "mp1-3.3m",		0x0100, 0x16a9166a, 6 | BRF_GRA },           // 10 Sound Prom
};

STD_ROM_PICK(mappyj)
STD_ROM_FN(mappyj)

struct BurnDriver BurnDrvMappyj = {
	"mappyj", "mappy", NULL, NULL, "1983",
	"Mappy (Japan)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, mappyjRomInfo, mappyjRomName, NULL, NULL, NULL, NULL, MappyInputInfo, MappyDIPInfo,
	MappyInit, DrvExit, DrvFrame, MappyDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Dig Dug II (New Ver.)

static struct BurnRomInfo digdug2RomDesc[] = {
	{ "d23_3.1d",		0x4000, 0xcc155338, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "d23_1.1b",		0x4000, 0x40e46af8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "d21_4.1k",		0x2000, 0x737443b1, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code

	{ "d21_5.3b",		0x1000, 0xafcb4509, 3 | BRF_GRA },           //  3 Background Tiles

	{ "d21_6.3m",		0x4000, 0xdf1f4ad8, 4 | BRF_GRA },           //  4 Sprites
	{ "d21_7.3n",		0x4000, 0xccadb3ea, 4 | BRF_GRA },           //  5

	{ "d21-5.5b",		0x0020, 0x9b169db5, 5 | BRF_GRA },           //  6 Color Data
	{ "d21-6.4c",		0x0100, 0x55a88695, 5 | BRF_GRA },           //  7
	{ "d21-7.5k",		0x0100, 0x9c55feda, 5 | BRF_GRA },           //  8

	{ "d21-3.3m",		0x0100, 0xe0074ee2, 6 | BRF_GRA },           //  9 Sound Prom
};

STD_ROM_PICK(digdug2)
STD_ROM_FN(digdug2)

struct BurnDriver BurnDrvDigdug2 = {
	"digdug2", NULL, NULL, NULL, "1985",
	"Dig Dug II (New Ver.)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, digdug2RomInfo, digdug2RomName, NULL, NULL, NULL, NULL, Digdug2InputInfo, Digdug2DIPInfo,
	Digdug2Init, DrvExit, DrvFrame, MappyDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Dig Dug II (Old Ver.)

static struct BurnRomInfo digdug2oRomDesc[] = {
	{ "d21_3.1d",		0x4000, 0xbe7ec80b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "d21_1.1b",		0x4000, 0x5c77c0d4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "d21_4.1k",		0x2000, 0x737443b1, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code

	{ "d21_5.3b",		0x1000, 0xafcb4509, 3 | BRF_GRA },           //  3 Background Tiles

	{ "d21_6.3m",		0x4000, 0xdf1f4ad8, 4 | BRF_GRA },           //  4 Sprites
	{ "d21_7.3n",		0x4000, 0xccadb3ea, 4 | BRF_GRA },           //  5

	{ "d21-5.5b",		0x0020, 0x9b169db5, 5 | BRF_GRA },           //  6 Color Data
	{ "d21-6.4c",		0x0100, 0x55a88695, 5 | BRF_GRA },           //  7
	{ "d2x-7.5k",		0x0100, 0x1525a4d1, 5 | BRF_GRA },           //  8

	{ "d21-3.3m",		0x0100, 0xe0074ee2, 6 | BRF_GRA },           //  9 Sound Prom
};

STD_ROM_PICK(digdug2o)
STD_ROM_FN(digdug2o)

struct BurnDriver BurnDrvDigdug2o = {
	"digdug2o", "digdug2", NULL, NULL, "1985",
	"Dig Dug II (Old Ver.)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, digdug2oRomInfo, digdug2oRomName, NULL, NULL, NULL, NULL, Digdug2InputInfo, Digdug2DIPInfo,
	Digdug2Init, DrvExit, DrvFrame, MappyDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// The Tower of Druaga (New Ver.)

static struct BurnRomInfo todruagaRomDesc[] = {
	{ "td2_3.1d",		0x4000, 0xfbf16299, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "td2_1.1b",		0x4000, 0xb238d723, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "td1_4.1k",		0x2000, 0xae9d06d9, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code

	{ "td1_5.3b",		0x1000, 0xd32b249f, 3 | BRF_GRA },           //  3 Background Tiles

	{ "td1_6.3m",		0x2000, 0xe827e787, 4 | BRF_GRA },           //  4 Sprites
	{ "td1_7.3n",		0x2000, 0x962bd060, 4 | BRF_GRA },           //  5

	{ "td1-5.5b",		0x0020, 0x122cc395, 5 | BRF_GRA },           //  6 Color Data
	{ "td1-6.4c",		0x0100, 0x8c661d6a, 5 | BRF_GRA },           //  7
	{ "td1-7.5k",		0x0400, 0xa86c74dd, 5 | BRF_GRA },           //  8

	{ "td1-3.3m",		0x0100, 0x07104c40, 6 | BRF_GRA },           //  9 Sound Prom
};

STD_ROM_PICK(todruaga)
STD_ROM_FN(todruaga)

struct BurnDriver BurnDrvTodruaga = {
	"todruaga", NULL, NULL, NULL, "1984",
	"The Tower of Druaga (New Ver.)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, todruagaRomInfo, todruagaRomName, NULL, NULL, NULL, NULL, TodruagaInputInfo, TodruagaDIPInfo,
	Digdug2Init, DrvExit, DrvFrame, MappyDraw, DrvScan, &DrvRecalc, 0x500,
	224, 288, 3, 4
};


// The Tower of Druaga (Old Ver.)

static struct BurnRomInfo todruagaoRomDesc[] = {
	{ "td1_3.1d",		0x4000, 0x7ab4f5b2, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "td1_1.1b",		0x4000, 0x8c20ef10, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "td1_4.1k",		0x2000, 0xae9d06d9, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code

	{ "td1_5.3b",		0x1000, 0xd32b249f, 3 | BRF_GRA },           //  3 Background Tiles

	{ "td1_6.3m",		0x2000, 0xe827e787, 4 | BRF_GRA },           //  4 Sprites
	{ "td1_7.3n",		0x2000, 0x962bd060, 4 | BRF_GRA },           //  5

	{ "td1-5.5b",		0x0020, 0x122cc395, 5 | BRF_GRA },           //  6 Color Data
	{ "td1-6.4c",		0x0100, 0x8c661d6a, 5 | BRF_GRA },           //  7
	{ "td1-7.5k",		0x0400, 0xa86c74dd, 5 | BRF_GRA },           //  8

	{ "td1-3.3m",		0x0100, 0x07104c40, 6 | BRF_GRA },           //  9 Sound Prom
};

STD_ROM_PICK(todruagao)
STD_ROM_FN(todruagao)

struct BurnDriver BurnDrvTodruagao = {
	"todruagao", "todruaga", NULL, NULL, "1984",
	"The Tower of Druaga (Old Ver.)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, todruagaoRomInfo, todruagaoRomName, NULL, NULL, NULL, NULL, TodruagaInputInfo, TodruagaDIPInfo,
	Digdug2Init, DrvExit, DrvFrame, MappyDraw, DrvScan, &DrvRecalc, 0x500,
	224, 288, 3, 4
};


// The Tower of Druaga (Sidam)

static struct BurnRomInfo todruagasRomDesc[] = {
	{ "3b.bin",		0x4000, 0x85d052d9, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "1b.bin",		0x4000, 0xa5db267a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "td1_4.1k",		0x2000, 0xae9d06d9, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code

	{ "td1_5.3b",		0x1000, 0xd32b249f, 3 | BRF_GRA },           //  3 Background Tiles

	{ "td1_6.3m",		0x2000, 0xe827e787, 4 | BRF_GRA },           //  4 Sprites
	{ "td1_7.3n",		0x2000, 0x962bd060, 4 | BRF_GRA },           //  5

	{ "td1-5.5b",		0x0020, 0x122cc395, 5 | BRF_GRA },           //  6 Color Data
	{ "td1-6.4c",		0x0100, 0x8c661d6a, 5 | BRF_GRA },           //  7
	{ "td1-7.5k",		0x0400, 0xa86c74dd, 5 | BRF_GRA },           //  8

	{ "td1-3.3m",		0x0100, 0x07104c40, 6 | BRF_GRA },           //  9 Sound Prom
};

STD_ROM_PICK(todruagas)
STD_ROM_FN(todruagas)

struct BurnDriver BurnDrvTodruagas = {
	"todruagas", "todruaga", NULL, NULL, "1984",
	"The Tower of Druaga (Sidam)\0", NULL, "bootleg? (Sidam)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, todruagasRomInfo, todruagasRomName, NULL, NULL, NULL, NULL, TodruagaInputInfo, TodruagaDIPInfo,
	Digdug2Init, DrvExit, DrvFrame, MappyDraw, DrvScan, &DrvRecalc, 0x500,
	224, 288, 3, 4
};


// Motos

static struct BurnRomInfo motosRomDesc[] = {
	{ "mo1_3.1d",		0x4000, 0x1104abb2, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "mo1_1.1b",		0x4000, 0x57b157e2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mo1_4.1k",		0x2000, 0x55e45d21, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code

	{ "mo1_5.3b",		0x1000, 0x5d4a2a22, 3 | BRF_GRA },           //  3 Background Tiles

	{ "mo1_6.3m",		0x4000, 0x2f0e396e, 4 | BRF_GRA },           //  4 Sprites
	{ "mo1_7.3n",		0x4000, 0xcf8a3b86, 4 | BRF_GRA },           //  5

	{ "mo1-5.5b",		0x0020, 0x71972383, 5 | BRF_GRA },           //  6 Color Data
	{ "mo1-6.4c",		0x0100, 0x730ba7fb, 5 | BRF_GRA },           //  7
	{ "mo1-7.5k",		0x0100, 0x7721275d, 5 | BRF_GRA },           //  8

	{ "mo1-3.3m",		0x0100, 0x2accdfb4, 6 | BRF_GRA },           //  9 Sound Prom
};

STD_ROM_PICK(motos)
STD_ROM_FN(motos)

struct BurnDriver BurnDrvMotos = {
	"motos", NULL, NULL, NULL, "1985",
	"Motos\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, motosRomInfo, motosRomName, NULL, NULL, NULL, NULL, MotosInputInfo, MotosDIPInfo,
	MotosInit, DrvExit, DrvFrame, MappyDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Super Pac-Man

static struct BurnRomInfo superpacRomDesc[] = {
	{ "sp1-2.1c",		0x2000, 0x4bb33d9c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "sp1-1.1b",		0x2000, 0x846fbb4a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "spc-3.1k",		0x1000, 0x04445ddb, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code

	{ "sp1-6.3c",		0x1000, 0x91c5935c, 3 | BRF_GRA },           //  3 Background Tiles

	{ "spv-2.3f",		0x2000, 0x670a42f2, 4 | BRF_GRA },           //  4 Sprites

	{ "superpac.4c",	0x0020, 0x9ce22c46, 5 | BRF_GRA },           //  5 Color Data
	{ "superpac.4e",	0x0100, 0x1253c5c1, 5 | BRF_GRA },           //  6
	{ "superpac.3l",	0x0100, 0xd4d7026f, 5 | BRF_GRA },           //  7

	{ "superpac.3m",	0x0100, 0xad43688f, 6 | BRF_GRA },           //  8 Sound Prom
};

STD_ROM_PICK(superpac)
STD_ROM_FN(superpac)

struct BurnDriver BurnDrvSuperpac = {
	"superpac", NULL, NULL, NULL, "1982",
	"Super Pac-Man\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, superpacRomInfo, superpacRomName, NULL, NULL, NULL, NULL, SuperpacInputInfo, SuperpacDIPInfo,
	SuperpacInit, DrvExit, DrvFrame, SuperpacDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Super Pac-Man (Midway)

static struct BurnRomInfo superpacmRomDesc[] = {
	{ "spc-2.1c",		0x2000, 0x1a38c30e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "spc-1.1b",		0x2000, 0x730e95a9, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "spc-3.1k",		0x1000, 0x04445ddb, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 #1 Code

	{ "spv-1.3c",		0x1000, 0x78337e74, 3 | BRF_GRA },           //  3 Background Tiles

	{ "spv-2.3f",		0x2000, 0x670a42f2, 4 | BRF_GRA },           //  4 Sprites

	{ "superpac.4c",	0x0020, 0x9ce22c46, 5 | BRF_GRA },           //  5 Color Data
	{ "superpac.4e",	0x0100, 0x1253c5c1, 5 | BRF_GRA },           //  6
	{ "superpac.3l",	0x0100, 0xd4d7026f, 5 | BRF_GRA },           //  7

	{ "superpac.3m",	0x0100, 0xad43688f, 6 | BRF_GRA },           //  8 Sound Prom
};

STD_ROM_PICK(superpacm)
STD_ROM_FN(superpacm)

struct BurnDriver BurnDrvSuperpacm = {
	"superpacm", "superpac", NULL, NULL, "1982",
	"Super Pac-Man (Midway)\0", NULL, "Namco (Bally Midway license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, superpacmRomInfo, superpacmRomName, NULL, NULL, NULL, NULL, SuperpacInputInfo, SuperpacDIPInfo,
	SuperpacInit, DrvExit, DrvFrame, SuperpacDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Pac & Pal

static struct BurnRomInfo pacnpalRomDesc[] = {
	{ "pap1-3b.1d",		0x2000, 0xed64a565, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "pap1-2b.1c",		0x2000, 0x15308bcf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pap3-1.1b",		0x2000, 0x3cac401c, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pap1-4.1k",		0x1000, 0x330e20de, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code

	{ "pap1-6.3c",		0x1000, 0xa36b96cb, 3 | BRF_GRA },           //  4 Background Tiles

	{ "pap1-5.3f",		0x2000, 0xfb6f56e3, 4 | BRF_GRA },           //  5 Sprites

	{ "pap1-6.4c",		0x0020, 0x52634b41, 5 | BRF_GRA },           //  6 Color Data
	{ "pap1-5.4e",		0x0100, 0xac46203c, 5 | BRF_GRA },           //  7
	{ "pap1-4.3l",		0x0100, 0x686bde84, 5 | BRF_GRA },           //  8

	{ "pap1-3.3m",		0x0100, 0x94782db5, 6 | BRF_GRA },           //  9 Sound Prom
};

STD_ROM_PICK(pacnpal)
STD_ROM_FN(pacnpal)

struct BurnDriver BurnDrvPacnpal = {
	"pacnpal", NULL, NULL, NULL, "1983",
	"Pac & Pal\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, pacnpalRomInfo, pacnpalRomName, NULL, NULL, NULL, NULL, PacnpalInputInfo, PacnpalDIPInfo,
	PacnpalInit, DrvExit, DrvFrame, SuperpacDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Pac & Pal (older)

static struct BurnRomInfo pacnpal2RomDesc[] = {
	{ "pap1-3.1d",		0x2000, 0xd7ec2719, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "pap1-2.1c",		0x2000, 0x0245396e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pap1-1.1b",		0x2000, 0x7f046b58, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pap1-4.1k",		0x1000, 0x330e20de, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code

	{ "pap1-6.3c",		0x1000, 0xa36b96cb, 3 | BRF_GRA },           //  4 Background Tiles

	{ "pap1-5.3f",		0x2000, 0xfb6f56e3, 4 | BRF_GRA },           //  5 Sprites

	{ "pap1-6.4c",		0x0020, 0x52634b41, 5 | BRF_GRA },           //  6 Color Data
	{ "pap1-5.4e",		0x0100, 0xac46203c, 5 | BRF_GRA },           //  7
	{ "pap1-4.3l",		0x0100, 0x686bde84, 5 | BRF_GRA },           //  8

	{ "pap1-3.3m",		0x0100, 0x94782db5, 6 | BRF_GRA },           //  9 Sound Prom
};

STD_ROM_PICK(pacnpal2)
STD_ROM_FN(pacnpal2)

struct BurnDriver BurnDrvPacnpal2 = {
	"pacnpal2", "pacnpal", NULL, NULL, "1983",
	"Pac & Pal (older)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, pacnpal2RomInfo, pacnpal2RomName, NULL, NULL, NULL, NULL, PacnpalInputInfo, PacnpalDIPInfo,
	PacnpalInit, DrvExit, DrvFrame, SuperpacDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Pac-Man & Chomp Chomp

static struct BurnRomInfo pacnchmpRomDesc[] = {
	{ "pap3-3.1d",		0x2000, 0x20a07d3d, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "pap3-2.1c",		0x2000, 0x505bae56, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pap3-1.1b",		0x2000, 0x3cac401c, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pap1-4.1k",		0x1000, 0x330e20de, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code

	{ "pap2-6.3c",		0x1000, 0x93d15c30, 3 | BRF_GRA },           //  4 Background Tiles

	{ "pap2-5.3f",		0x2000, 0x39f44aa4, 4 | BRF_GRA },           //  5 Sprites

	{ "pap2-6.4c",		0x0020, 0x18c3db79, 5 | BRF_GRA },           //  6 Color Data
	{ "pap2-5.4e",		0x0100, 0x875b49bb, 5 | BRF_GRA },           //  7
	{ "pap2-4.3l",		0x0100, 0x23701566, 5 | BRF_GRA },           //  8

	{ "pap1-3.3m",		0x0100, 0x94782db5, 6 | BRF_GRA },           //  9 Sound Prom
};

STD_ROM_PICK(pacnchmp)
STD_ROM_FN(pacnchmp)

struct BurnDriver BurnDrvPacnchmp = {
	"pacnchmp", "pacnpal", NULL, NULL, "1983",
	"Pac-Man & Chomp Chomp\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, pacnchmpRomInfo, pacnchmpRomName, NULL, NULL, NULL, NULL, PacnpalInputInfo, PacnpalDIPInfo,
	PacnpalInit, DrvExit, DrvFrame, SuperpacDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Grobda (New Ver.)

static struct BurnRomInfo grobdaRomDesc[] = {
	{ "gr2-3.1d",		0x2000, 0x8e3a23be, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gr2-2.1c",		0x2000, 0x19ffa83d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gr2-1.1b",		0x2000, 0x0089b13a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gr1-4.1k",		0x2000, 0x3fe78c08, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code

	{ "gr1-7.3c",		0x1000, 0x4ebfabfd, 3 | BRF_GRA },           //  4 Background Tiles

	{ "gr1-5.3f",		0x2000, 0xeed43487, 4 | BRF_GRA },           //  5 Sprites
	{ "gr1-6.3e",		0x2000, 0xcebb7362, 4 | BRF_GRA },           //  6

	{ "gr1-6.4c",		0x0020, 0xc65efa77, 5 | BRF_GRA },           //  7 Color Data
	{ "gr1-5.4e",		0x0100, 0xa0f66911, 5 | BRF_GRA },           //  8
	{ "gr1-4.3l",		0x0100, 0xf1f2c234, 5 | BRF_GRA },           //  9

	{ "gr1-3.3m",		0x0100, 0x66eb1467, 6 | BRF_GRA },           // 10 Sound Prom
};

STD_ROM_PICK(grobda)
STD_ROM_FN(grobda)

struct BurnDriver BurnDrvGrobda = {
	"grobda", NULL, NULL, NULL, "1984",
	"Grobda (New Ver.)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, grobdaRomInfo, grobdaRomName, NULL, NULL, NULL, NULL, GrobdaInputInfo, GrobdaDIPInfo,
	GrobdaInit, DrvExit, DrvFrame, SuperpacDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Grobda (Old Ver. set 1)

static struct BurnRomInfo grobda2RomDesc[] = {
	{ "gr1-3.1d",		0x2000, 0x4ef4a7c1, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gr2-2a.1c",		0x2000, 0xf93e82ae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gr1-1.1b",		0x2000, 0x32d42f22, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gr1-4.1k",		0x2000, 0x3fe78c08, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code

	{ "gr1-7.3c",		0x1000, 0x4ebfabfd, 3 | BRF_GRA },           //  4 Background Tiles

	{ "gr1-5.3f",		0x2000, 0xeed43487, 4 | BRF_GRA },           //  5 Sprites
	{ "gr1-6.3e",		0x2000, 0xcebb7362, 4 | BRF_GRA },           //  6

	{ "gr1-6.4c",		0x0020, 0xc65efa77, 5 | BRF_GRA },           //  7 Color Data
	{ "gr1-5.4e",		0x0100, 0xa0f66911, 5 | BRF_GRA },           //  8
	{ "gr1-4.3l",		0x0100, 0xf1f2c234, 5 | BRF_GRA },           //  9

	{ "gr1-3.3m",		0x0100, 0x66eb1467, 6 | BRF_GRA },           // 10 Sound Prom
};

STD_ROM_PICK(grobda2)
STD_ROM_FN(grobda2)

struct BurnDriver BurnDrvGrobda2 = {
	"grobda2", "grobda", NULL, NULL, "1984",
	"Grobda (Old Ver. set 1)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, grobda2RomInfo, grobda2RomName, NULL, NULL, NULL, NULL, GrobdaInputInfo, GrobdaDIPInfo,
	GrobdaInit, DrvExit, DrvFrame, SuperpacDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Grobda (Old Ver. set 2)

static struct BurnRomInfo grobda3RomDesc[] = {
	{ "gr1-3.1d",		0x2000, 0x4ef4a7c1, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gr1-2.1c",		0x2000, 0x7dcc6e8e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gr1-1.1b",		0x2000, 0x32d42f22, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gr1-4.1k",		0x2000, 0x3fe78c08, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code

	{ "gr1-7.3c",		0x1000, 0x4ebfabfd, 3 | BRF_GRA },           //  4 Background Tiles

	{ "gr1-5.3f",		0x2000, 0xeed43487, 4 | BRF_GRA },           //  5 Sprites
	{ "gr1-6.3e",		0x2000, 0xcebb7362, 4 | BRF_GRA },           //  6

	{ "gr1-6.4c",		0x0020, 0xc65efa77, 5 | BRF_GRA },           //  7 Color Data
	{ "gr1-5.4e",		0x0100, 0xa0f66911, 5 | BRF_GRA },           //  8
	{ "gr1-4.3l",		0x0100, 0xf1f2c234, 5 | BRF_GRA },           //  9

	{ "gr1-3.3m",		0x0100, 0x66eb1467, 6 | BRF_GRA },           // 10 Sound Prom
};

STD_ROM_PICK(grobda3)
STD_ROM_FN(grobda3)

struct BurnDriver BurnDrvGrobda3 = {
	"grobda3", "grobda", NULL, NULL, "1984",
	"Grobda (Old Ver. set 2)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, grobda3RomInfo, grobda3RomName, NULL, NULL, NULL, NULL, GrobdaInputInfo, GrobdaDIPInfo,
	GrobdaInit, DrvExit, DrvFrame, SuperpacDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Phozon (Japan)

static struct BurnRomInfo phozonRomDesc[] = {
	{ "6e.rom",		0x2000, 0xa6686af1, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "6h.rom",		0x2000, 0x72a65ba0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "6c.rom",		0x2000, 0xf1fda22e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6d.rom",		0x2000, 0xf40e6df0, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "3b.rom",		0x2000, 0x5a4b3a79, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code

	{ "9r.rom",		0x2000, 0x5d9f0a28, 3 | BRF_GRA },           //  5 M6809 #2 Code

	{ "7j.rom",		0x1000, 0x27f9db5b, 4 | BRF_GRA },           //  6 Background Tiles
	{ "8j.rom",		0x1000, 0x15b12ef8, 4 | BRF_GRA },           //  7

	{ "5t.rom",		0x2000, 0xd50f08f8, 5 | BRF_GRA },           //  8 Sprites

	{ "red.prm",		0x0100, 0xa2880667, 6 | BRF_GRA },           //  9 Color Data
	{ "green.prm",		0x0100, 0xd6e08bef, 6 | BRF_GRA },           // 10
	{ "blue.prm",		0x0100, 0xb2d69c72, 6 | BRF_GRA },           // 11
	{ "chr.prm",		0x0100, 0x429e8fee, 6 | BRF_GRA },           // 12
	{ "sprite.prm",		0x0100, 0x9061db07, 6 | BRF_GRA },           // 13
	{ "palette.prm",	0x0020, 0x60e856ed, 6 | BRF_GRA | BRF_OPT }, // 14

	{ "sound.prm",		0x0100, 0xad43688f, 7 | BRF_GRA },           // 15 Sound Prom
};

STD_ROM_PICK(phozon)
STD_ROM_FN(phozon)

struct BurnDriver BurnDrvPhozon = {
	"phozon", NULL, NULL, NULL, "1983",
	"Phozon (Japan)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, phozonRomInfo, phozonRomName, NULL, NULL, NULL, NULL, PhozonInputInfo, PhozonDIPInfo,
	PhozonInit, DrvExit, DrvFrame, PhozonDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Phozon (Sidam)

static struct BurnRomInfo phozonsRomDesc[] = {
	{ "6e.bin",		0x2000, 0xef822900, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "6h.bin",		0x2000, 0xacb7869e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "6c.bin",		0x2000, 0x8ffa3e0e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6d.bin",		0x2000, 0x8e6800b3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "3b.rom",		0x2000, 0x5a4b3a79, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code

	{ "9r.rom",		0x2000, 0x5d9f0a28, 3 | BRF_GRA },           //  5 M6809 #2 Code

	{ "7j.bin",		0x1000, 0x312b3ece, 4 | BRF_GRA },           //  6 Background Tiles
	{ "8j.bin",		0x1000, 0xd21422a2, 4 | BRF_GRA },           //  7

	{ "5t.rom",		0x2000, 0xd50f08f8, 5 | BRF_GRA },           //  8 Sprites

	{ "red.prm",		0x0100, 0xa2880667, 6 | BRF_GRA },           //  9 Color Data
	{ "green.prm",		0x0100, 0xd6e08bef, 6 | BRF_GRA },           // 10
	{ "blue.prm",		0x0100, 0xb2d69c72, 6 | BRF_GRA },           // 11
	{ "chr.prm",		0x0100, 0x429e8fee, 6 | BRF_GRA },           // 12
	{ "sprite.prm",		0x0100, 0x9061db07, 6 | BRF_GRA },           // 13
	{ "palette.prm",	0x0020, 0x60e856ed, 6 | BRF_GRA | BRF_OPT }, // 14

	{ "sound.prm",		0x0100, 0xad43688f, 7 | BRF_GRA },           // 15 Sound Prom
};

STD_ROM_PICK(phozons)
STD_ROM_FN(phozons)

struct BurnDriver BurnDrvPhozons = {
	"phozons", "phozon", NULL, NULL, "1983",
	"Phozon (Sidam)\0", NULL, "Namco (Sidam license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, phozonsRomInfo, phozonsRomName, NULL, NULL, NULL, NULL, PhozonInputInfo, PhozonDIPInfo,
	PhozonInit, DrvExit, DrvFrame, PhozonDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};

