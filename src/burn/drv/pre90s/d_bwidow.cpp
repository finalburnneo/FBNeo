// FB Alpha Atari Black Widow driver module
// Based on MAME driver by Brad Oliver, Bernd Wiebelt, and Allard van der Bas

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "vector.h"
#include "avgdvg.h"
#include "pokey.h"
#include "watchdog.h"
#include "earom.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVectorRAM;
static UINT8 *DrvVectorROM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 avgletsgo = 0;
static INT32 nExtraCycles = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[5];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 irqcnt = 0;
static INT32 irqflip = 0;

static struct BurnInputInfo BwidowInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 start"	},

	{"Move Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"Move Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"Move Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"Move Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},

	{"Fire Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"Fire Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"Fire Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"Fire Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Diagnostic Step",	BIT_DIGITAL,	DrvJoy1 + 5,	"service2"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		}, // not used
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		}, // service mode
	{"Dip E",			BIT_DIPSWITCH,	DrvDips + 4,	"dip"		}, // fake resolution dips
};

STDINPUTINFO(Bwidow)

static struct BurnInputInfo GravitarInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Diagnostic Step",	BIT_DIGITAL,	DrvJoy1 + 5,	"service2"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		}, // not used
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		}, // service mode
	{"Dip E",			BIT_DIPSWITCH,	DrvDips + 4,	"dip"		}, // fake resolution dips
};

STDINPUTINFO(Gravitar)

static struct BurnInputInfo LunarbatInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		}, // not used
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		}, // not used
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		}, // not used
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		}, // not used
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		}, // service mode
	{"Dip E",			BIT_DIPSWITCH,	DrvDips + 4,	"dip"		}, // fake resolution dips
};

STDINPUTINFO(Lunarbat)

static struct BurnInputInfo SpacduelInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"	},
	{"P1 Select",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 coin"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Diagnostic Step",	BIT_DIGITAL,	DrvJoy1 + 5,	"service2"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		}, // service mode
	{"Dip E",			BIT_DIPSWITCH,	DrvDips + 4,	"dip"		}, // fake resolution dips
};

STDINPUTINFO(Spacduel)

#ifdef __LIBRETRO__
#define RESO_DIP
#else
#define RESO_DIP \
	{0   , 0xfe, 0   ,    2, "Hires Mode"			}, \
	{0x04, 0x01, 0x01, 0x00, "No"					}, \
	{0x04, 0x01, 0x01, 0x01, "Yes"					},
#endif

static struct BurnDIPInfo BwidowDIPList[]=
{
	DIP_OFFSET(0x0e)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0x11, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					}, // unused
	{0x03, 0xff, 0xff, 0x10, NULL					}, // service mode
	{0x04, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x03, 0x02, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x00, 0x01, 0x0c, 0x00, "*1"					},
	{0x00, 0x01, 0x0c, 0x04, "*4"					},
	{0x00, 0x01, 0x0c, 0x08, "*5"					},
	{0x00, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Coin A"				},
	{0x00, 0x01, 0x10, 0x00, "*1"					},
	{0x00, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    6, "Bonus Coins"			},
	{0x00, 0x01, 0xe0, 0x80, "1 each 5"				},
	{0x00, 0x01, 0xe0, 0x60, "2 each 4"				},
	{0x00, 0x01, 0xe0, 0x40, "1 each 4"				},
	{0x00, 0x01, 0xe0, 0xa0, "1 each 3"				},
	{0x00, 0x01, 0xe0, 0x20, "1 each 2"				},
	{0x00, 0x01, 0xe0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Max Start"			},
	{0x01, 0x01, 0x03, 0x00, "Lev 13"				},
	{0x01, 0x01, 0x03, 0x01, "Lev 21"				},
	{0x01, 0x01, 0x03, 0x02, "Lev 37"				},
	{0x01, 0x01, 0x03, 0x03, "Lev 53"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x0c, 0x00, "3"					},
	{0x01, 0x01, 0x0c, 0x04, "4"					},
	{0x01, 0x01, 0x0c, 0x08, "5"					},
	{0x01, 0x01, 0x0c, 0x0c, "6"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x30, 0x00, "Easy"					},
	{0x01, 0x01, 0x30, 0x10, "Medium"				},
	{0x01, 0x01, 0x30, 0x20, "Hard"					},
	{0x01, 0x01, 0x30, 0x30, "Demo"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0xc0, 0x00, "20000"				},
	{0x01, 0x01, 0xc0, 0x40, "30000"				},
	{0x01, 0x01, 0xc0, 0x80, "40000"				},
	{0x01, 0x01, 0xc0, 0xc0, "None"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x03, 0x01, 0x10, 0x10, "Off"					},
	{0x03, 0x01, 0x10, 0x00, "On"					},

	RESO_DIP
};

STDDIPINFO(Bwidow)

static struct BurnDIPInfo GravitarDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0x04, NULL					},
	{0x01, 0xff, 0xff, 0x00, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					}, // unused
	{0x03, 0xff, 0xff, 0x10, NULL					}, // service mode
	{0x04, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x00, "3"					},
	{0x00, 0x01, 0x0c, 0x04, "4"					},
	{0x00, 0x01, 0x0c, 0x08, "5"					},
	{0x00, 0x01, 0x0c, 0x0c, "6"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x00, 0x01, 0x10, 0x00, "Easy"					},
	{0x00, 0x01, 0x10, 0x10, "Hard"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x00, 0x01, 0xc0, 0x00, "10000"				},
	{0x00, 0x01, 0xc0, 0x40, "20000"				},
	{0x00, 0x01, 0xc0, 0x80, "30000"				},
	{0x00, 0x01, 0xc0, 0xc0, "None"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x01, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x01, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x01, 0x01, 0x03, 0x03, "1 Coin  2 Credits"	},
	{0x01, 0x01, 0x03, 0x02, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x01, 0x01, 0x0c, 0x00, "*1"					},
	{0x01, 0x01, 0x0c, 0x04, "*4"					},
	{0x01, 0x01, 0x0c, 0x08, "*5"					},
	{0x01, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Coin A"				},
	{0x01, 0x01, 0x10, 0x00, "*1"					},
	{0x01, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    6, "Bonus Coins"			},
	{0x01, 0x01, 0xe0, 0x80, "1 each 5"				},
	{0x01, 0x01, 0xe0, 0x60, "2 each 4"				},
	{0x01, 0x01, 0xe0, 0x40, "1 each 4"				},
	{0x01, 0x01, 0xe0, 0xa0, "1 each 3"				},
	{0x01, 0x01, 0xe0, 0x20, "1 each 2"				},
	{0x01, 0x01, 0xe0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x03, 0x01, 0x10, 0x10, "Off"					},
	{0x03, 0x01, 0x10, 0x00, "On"					},

	RESO_DIP
};

STDDIPINFO(Gravitar)

static struct BurnDIPInfo SpacduelDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0x01, NULL					},
	{0x01, 0xff, 0xff, 0x00, NULL					},
	{0x02, 0xff, 0xff, 0x07, NULL					},
	{0x03, 0xff, 0xff, 0x10, NULL					}, // service mode
	{0x04, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x03, 0x01, "3"					},
	{0x00, 0x01, 0x03, 0x00, "4"					},
	{0x00, 0x01, 0x03, 0x03, "5"					},
	{0x00, 0x01, 0x03, 0x02, "6"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x0c, 0x04, "Easy"					},
	{0x00, 0x01, 0x0c, 0x00, "Normal"				},
	{0x00, 0x01, 0x0c, 0x0c, "Medium"				},
	{0x00, 0x01, 0x0c, 0x08, "Hard"					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x00, 0x01, 0x30, 0x00, "English"				},
	{0x00, 0x01, 0x30, 0x10, "German"				},
	{0x00, 0x01, 0x30, 0x20, "French"				},
	{0x00, 0x01, 0x30, 0x30, "Spanish"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x00, 0x01, 0xc0, 0xc0, "8000"					},
	{0x00, 0x01, 0xc0, 0x00, "10000"				},
	{0x00, 0x01, 0xc0, 0x40, "15000"				},
	{0x00, 0x01, 0xc0, 0x80, "None"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x01, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x01, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x01, 0x01, 0x03, 0x03, "1 Coin  2 Credits"	},
	{0x01, 0x01, 0x03, 0x02, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x01, 0x01, 0x0c, 0x00, "*1"					},
	{0x01, 0x01, 0x0c, 0x04, "*4"					},
	{0x01, 0x01, 0x0c, 0x08, "*5"					},
	{0x01, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Coin A"				},
	{0x01, 0x01, 0x10, 0x00, "*1"					},
	{0x01, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    6, "Bonus Coins"			},
	{0x01, 0x01, 0xe0, 0x80, "1 each 5"				},
	{0x01, 0x01, 0xe0, 0x60, "2 each 4"				},
	{0x01, 0x01, 0xe0, 0x40, "1 each 4"				},
	{0x01, 0x01, 0xe0, 0xa0, "1 each 3"				},
	{0x01, 0x01, 0xe0, 0x20, "1 each 2"				},
	{0x01, 0x01, 0xe0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Charge by ..."		},
	{0x02, 0x01, 0x01, 0x01, "player"				},
	{0x02, 0x01, 0x01, 0x00, "game"					},

	{0   , 0xfe, 0   ,    2, "2-credit minimum"		},
	{0x02, 0x01, 0x02, 0x02, "Off"					},
	{0x02, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "1-player game only"	},
	{0x02, 0x01, 0x04, 0x04, "Off"					},
	{0x02, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x03, 0x01, 0x10, 0x10, "Off"					},
	{0x03, 0x01, 0x10, 0x00, "On"					},

	RESO_DIP
};

STDDIPINFO(Spacduel)

static void bwidow_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x6000) {
		pokey_write((address / 0x800) & 1, address & 0xf, data);
		return;
	}

	if ((address & 0xffc0) == 0x8940) {
		earom_write(address & 0x3f, data);
		return;
	}

	if ((address & 0xff80) == 0x8980) {
		// watchdog clear (not hooked up in MAME/FBA)
		return;
	}

	switch (address)
	{
		case 0x8800:
			// coin counters, leds
		return;

		case 0x8840:
			avgdvg_go();
			avgletsgo = 1;
		return;

		case 0x8880:
			avgdvg_reset();
		return;

		case 0x88c0:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x8900:
			earom_ctrl_write(0, data);
		return;
	}
}

static UINT8 bwidow_read(UINT16 address)
{
	if ((address & 0xf000) == 0x6000) {
		return pokey_read((address / 0x800) & 1, address & 0xf);
	}

	switch (address)
	{
		case 0x7000:
			return earom_read(0);

		case 0x7800: {
			UINT8 temp = DrvInputs[0] & 0x3f;
			if (avgdvg_done()) temp |= 0x40;
			if (M6502TotalCycles() & 0x100) temp |= 0x80;
			return temp;
		}

		case 0x8000:
			return DrvInputs[1];

		case 0x8800:
			return DrvInputs[2];
	}

	return 0;
}

static void bwidowp_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x0800) {
		pokey_write((address / 0x10) & 1, address & 0xf, data);
		return;
	}

	if ((address & 0xffc0) == 0x8000) {
		earom_write(address & 0x3f, data);
		return;
	}

	switch (address)
	{
		case 0x2000:
			avgdvg_go();
			avgletsgo = 1;
		return;

		case 0x2800:
			avgdvg_reset();
		return;

		case 0x3000:
			BurnWatchdogWrite();
		return;

		case 0x3800:
			// coin counters, leds
		return;

		case 0x6000:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x8800:
			earom_ctrl_write(0, data);
		return;

		case 0x9800:
		return; // nop
	}
}

static UINT8 bwidowp_read(UINT16 address)
{
	if ((address & 0xffe0) == 0x0800) {
		return pokey_read((address / 0x10) & 1, address & 0xf);
	}

	switch (address)
	{
		case 0x1000:
			return (DrvInputs[0] << 4) | (DrvInputs[1] & 0xf);

		case 0x1800: {
			UINT8 temp = DrvInputs[0] & 0x3f;
			if (avgdvg_done()) temp |= 0x40;
			if (M6502TotalCycles() & 0x100) temp |= 0x80;
			return temp;
		}

		case 0x9000:
			return earom_read(0);
	}

	return 0;
}

static void spacduel_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfb00) == 0x1000) {
		pokey_write((address / 0x400) & 1, address & 0xf, data);
		return;
	}

	if ((address & 0xffc0) == 0x0f00) {
		earom_write(address & 0x3f, data);
		return;
	}

	switch (address)
	{
		case 0x0905:
		case 0x0906:
		return; // nop

		case 0x0c00:
			// coin counters, leds
		return;

		case 0x0c80:
			avgdvg_go();
			avgletsgo = 1;
		return;

		case 0x0d00:
			// watchdog
		return;

		case 0x0d80:
			avgdvg_reset();
		return;

		case 0x0e00:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x0e80:
			earom_ctrl_write(0, data);
		return;
	}
}

static UINT8 spacduel_input_read(UINT8 offset)
{
	INT32 res1 = DrvInputs[1]^0xff;
	INT32 res2 = DrvInputs[2]^0xff;
	INT32 res3 = DrvDips[2];
	INT32 res = 0x00;

	switch (offset & 0x07)
	{
		case 0:
			res = (res1 & 0x0c) << 4;
		break;
		case 1:
			res = (res2 & 0x0c) << 4;
		break;
		case 2:
			if ( res1 & 0x01) res |= 0x80;
			if ( res1 & 0x02) res |= 0x40;
		break;
		case 3:
			if ( res2 & 0x01) res |= 0x80;
			if ( res2 & 0x02) res |= 0x40;
		break;
		case 4:
			if ( res1 & 0x10) res |= 0x80;
			if ( res1 & 0x20) res |= 0x40;
		break;
		case 5:
			if ( res2 & 0x10) res |= 0x80;
			if (~res3 & 0x01) res |= 0x40;
		break;
		case 6:
			if ( res1 & 0x40) res |= 0x80;
			if (~res3 & 0x02) res |= 0x40;
		break;
		case 7:
			if (~res3 & 0x04) res |= 0x40;
		break;
	}

	return res;
}

static UINT8 spacduel_read(UINT16 address)
{
	if ((address & 0xfb00) == 0x1000) {
		return pokey_read((address / 0x400) & 1, address & 0xf);
	}

	switch (address)
	{
		case 0x0800: {
			UINT8 temp = DrvInputs[0] & 0x3f;
			if (avgdvg_done()) temp |= 0x40;
			if (M6502TotalCycles() & 0x100) temp |= 0x80;
			return temp;
		}

		case 0x0900:
		case 0x0901:
		case 0x0902:
		case 0x0903:
		case 0x0904:
		case 0x0905:
		case 0x0906:
		case 0x0907:
			return spacduel_input_read(address);

		case 0x0a00:
			return earom_read(0);
	}

	return 0;
}

static INT32 port1_read(INT32 /*offset*/)
{
	return DrvDips[0];
}

static INT32 port2_read(INT32 /*offset*/)
{
	return DrvDips[1];
}

static INT32 res_check()
{
#ifdef __LIBRETRO__
	return 0;
#endif
	if (DrvDips[4] & 1) {
		INT32 Width, Height;
		BurnDrvGetVisibleSize(&Width, &Height);

		if (Height != 1080) {
			vector_rescale((1080*800/600), 1080);
			return 1;
		}
	} else {
		INT32 Width, Height;
		BurnDrvGetVisibleSize(&Width, &Height);

		if (Height != 600) {
			vector_rescale(800, 600);
			return 1;
		}
	}
	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	M6502Reset();
	M6502Close();

	BurnWatchdogReset();
	earom_reset();

	PokeyReset();

	avgdvg_reset();

	avgletsgo = 0;
	nExtraCycles = 0;

	irqcnt = 0;
	irqflip = 0;

	res_check();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM			= Next; Next += 0x010000;

	DrvPalette			= (UINT32*)Next; Next += 0x0020 * 256 * sizeof(UINT32);

	AllRam				= Next;

	DrvM6502RAM			= Next; Next += 0x000800;
	DrvVectorRAM		= Next; Next += 0x000800;

	RamEnd				= Next;

	DrvVectorROM 		= Next; Next += 0x004000; // must(!) come after Vector RAM!

	MemEnd				= Next;

	return 0;
}

static INT32 BwidowInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvVectorROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x0800,  1, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x1800,  2, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x2800,  3, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM  + 0x9000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0xa000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0xb000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0xc000,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0xd000,  8, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0xe000,  9, 1)) return 1;
		memcpy (DrvM6502ROM + 0xf000, DrvM6502ROM + 0xe000, 0x1000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,				0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,			0x2000, 0x27ff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,			0x2800, 0x5fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x9000,	0x9000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(bwidow_write);
	M6502SetReadHandler(bwidow_read);
	M6502Close();

	earom_init();

	BurnWatchdogInit(DrvDoReset, 180); // not on bwidow

	PokeyInit(12096000/8, 2, 0.50, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, port1_read);
	PokeyAllPotCallback(1, port2_read);

	avgdvg_init(USE_AVG, DrvVectorRAM, 0x4000, M6502TotalCycles, 480, 440);

	earom_init();

	DrvDoReset(1);

	return 0;
}

static INT32 GravitarInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvVectorROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x0800,  1, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x1800,  2, 1)) return 1;
		
		if (BurnDrvGetFlags() & BDF_PROTOTYPE) // lunarbat (has 1 less vector rom)
		{
			if (BurnLoadRom(DrvM6502ROM  + 0x9000,  3, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0xa000,  4, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0xb000,  5, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0xc000,  6, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0xd000,  7, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0xe000,  8, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvVectorROM + 0x2800,  3, 1)) return 1;

			if (BurnLoadRom(DrvM6502ROM  + 0x9000,  4, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0xa000,  5, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0xb000,  6, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0xc000,  7, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0xd000,  8, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0xe000,  9, 1)) return 1;
		}

		memcpy (DrvM6502ROM + 0xf000, DrvM6502ROM + 0xe000, 0x1000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,				0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,			0x2000, 0x27ff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,			0x2800, 0x5fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x9000,	0x9000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(bwidow_write);
	M6502SetReadHandler(bwidow_read);
	M6502Close();

	earom_init();

	BurnWatchdogInit(DrvDoReset, 180); // not on bwidow

	PokeyInit(12096000/8, 2, 0.50, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, port1_read);
	PokeyAllPotCallback(1, port2_read);

	avgdvg_init(USE_AVG, DrvVectorRAM, 0x4000, M6502TotalCycles, 420, 440);

	earom_init();

	DrvDoReset(1);

	return 0;
}

static INT32 BwidowpInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvVectorROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x0800,  1, 1)) return 1;
		memcpy (DrvVectorROM + 0x1800, DrvVectorROM + 0x0800, 0x1000);

		if (BurnLoadRom(DrvM6502ROM  + 0xa000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0xb000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0xc000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0xd000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0xe000,  6, 1)) return 1;
		memcpy (DrvM6502ROM + 0xf000, DrvM6502ROM + 0xe000, 0x1000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,				0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,			0x4000, 0x47ff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,			0x4800, 0x6fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0xa000,	0x9000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(bwidowp_write);
	M6502SetReadHandler(bwidowp_read);
	M6502Close();

	earom_init();

	BurnWatchdogInit(DrvDoReset, 180);

	PokeyInit(12096000/8, 2, 0.50, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, port1_read);
	PokeyAllPotCallback(1, port2_read);

	avgdvg_init(USE_AVG, DrvVectorRAM, 0x3000, M6502TotalCycles, 480, 440);

	earom_init();

	DrvDoReset(1);

	return 0;
}

static INT32 SpacduelInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvVectorROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x0800,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM  + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x5000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x6000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x7000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x8000,  6, 1)) return 1;
		memcpy (DrvM6502ROM + 0x9000, DrvM6502ROM + 0x8000, 0x1000);
		BurnLoadRom(DrvM6502ROM + 0x9000, 7, 1); // lunarba1
		memcpy (DrvM6502ROM + 0xa000, DrvM6502ROM + 0x9000, 0x1000);
		memcpy (DrvM6502ROM + 0xb000, DrvM6502ROM + 0xa000, 0x1000);
		memcpy (DrvM6502ROM + 0xc000, DrvM6502ROM + 0xb000, 0x1000);
		memcpy (DrvM6502ROM + 0xd000, DrvM6502ROM + 0xc000, 0x1000);
		memcpy (DrvM6502ROM + 0xe000, DrvM6502ROM + 0xd000, 0x1000);
		memcpy (DrvM6502ROM + 0xf000, DrvM6502ROM + 0xe000, 0x1000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,				0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,			0x2000, 0x27ff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,			0x2800, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(spacduel_write);
	M6502SetReadHandler(spacduel_read);
	M6502Close();

	earom_init();

	BurnWatchdogInit(DrvDoReset, 180); // not on spacduel

	PokeyInit(12096000/8, 2, 0.50, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, port1_read);
	PokeyAllPotCallback(1, port2_read);

	avgdvg_init(USE_AVG, DrvVectorRAM, 0x2000, M6502TotalCycles, 540, 440);

	earom_init();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	avgdvg_exit();

	PokeyExit();
	M6502Exit();

	earom_exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
    for (INT32 i = 0; i < 0x20; i++) // color
	{
		INT32 r0 = (i & 0x4) ? 0xff : 0;
		INT32 g0 = (i & 0x2) ? 0xff : 0;
		INT32 b0 = (i & 0x1) ? 0xff : 0;
		
		for (INT32 j = 0; j < 256; j++) // intensity
		{
			INT32 r = (r0 * j) / 255;
			INT32 g = (g0 * j) / 255;
			INT32 b = (b0 * j) / 255;

			DrvPalette[i * 256 + j] = (r << 16) | (g << 8) | b; // must be 32bit palette! -dink (see vector.cpp)
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (res_check()) return 0; // resolution was changed

	draw_vector(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		DrvInputs[0] = (DrvInputs[0] & ~0x10) | (DrvDips[3] & 0x10);
	}

	INT32 nCyclesTotal = 1512000 / 60;
	INT32 nInterleave = 256;
	INT32 nCyclesDone = nExtraCycles;
	INT32 nSoundBufferPos = 0;

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone += M6502Run((nCyclesTotal * (i + 1) / nInterleave) - nCyclesDone);

		if (irqcnt >= (62 + irqflip)) { // 6.1something irq's per frame logic
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			irqcnt = -1;
			irqflip ^= 1;
		}
		irqcnt++;

		// Render Sound Segment
		if (pBurnSoundOut && i%4 == 3) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 4);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			pokey_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	nExtraCycles = nCyclesDone - nCyclesTotal;

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			pokey_update(pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	M6502Close();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		avgdvg_scan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		pokey_scan(nAction, pnMin);

		SCAN_VAR(nExtraCycles);
		SCAN_VAR(irqcnt);
		SCAN_VAR(irqflip);
	}

	earom_scan(nAction, pnMin); // here.

	return 0;
}


// Black Widow

static struct BurnRomInfo bwidowRomDesc[] = {
	{ "136017-107.l7",	0x0800, 0x97f6000c, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "136017-108.mn7",	0x1000, 0x3da354ed, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136017-109.np7",	0x1000, 0x2fc4ce79, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136017-110.r7",	0x1000, 0x0dd52987, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136017-101.d1",	0x1000, 0xfe3febb7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136017-102.ef1",	0x1000, 0x10ad0376, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136017-103.h1",	0x1000, 0x8a1430ee, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136017-104.j1",	0x1000, 0x44f9943f, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136017-105.kl1",	0x1000, 0x1fdf801c, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136017-106.m1",	0x1000, 0xccc9b26c, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "136002-125.n4",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 10 AVG PROM
};

STD_ROM_PICK(bwidow)
STD_ROM_FN(bwidow)

struct BurnDriver BurnDrvBwidow = {
	"bwidow", NULL, NULL, NULL, "1982",
	"Black Widow\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION | GBF_VECTOR, 0,
	NULL, bwidowRomInfo, bwidowRomName, NULL, NULL, NULL, NULL, BwidowInputInfo, BwidowDIPInfo,
	BwidowInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Black Widow (prototype)

static struct BurnRomInfo bwidowpRomDesc[] = {
	{ "vg4800",			0x0800, 0x12c0e382, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "vg5000",			0x1000, 0x7009106a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a000",			0x1000, 0xebe0ace2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "b000",			0x1000, 0xb14f33e2, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "c000",			0x1000, 0x79b8af00, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "d000",			0x1000, 0x10ac77c3, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "e000",			0x1000, 0xdfdda385, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "avgsmr",			0x0100, 0x5903af03, 2 | BRF_GRA },           //  7 AVG PROM

	{ "negrom.lo",		0x0100, 0xaeb9cde1, 0 | BRF_OPT },           //  8 PROMs
	{ "negrom.hi",		0x0100, 0x08f0112b, 0 | BRF_OPT },           //  9
};

STD_ROM_PICK(bwidowp)
STD_ROM_FN(bwidowp)

struct BurnDriverD BurnDrvBwidowp = {
	"bwidowp", "bwidow", NULL, NULL, "1982",
	"Black Widow (prototype)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION | GBF_VECTOR, 0,
	NULL, bwidowpRomInfo, bwidowpRomName, NULL, NULL, NULL, NULL, BwidowInputInfo, BwidowDIPInfo,
	BwidowpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Gravitar (version 3)

static struct BurnRomInfo gravitarRomDesc[] = {
	{ "136010-210.l7",	0x0800, 0xdebcb243, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "136010-207.mn7",	0x1000, 0x4135629a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136010-208.np7",	0x1000, 0x358f25d9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136010-309.r7",	0x1000, 0x4ac78df4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136010-301.d1",	0x1000, 0xa2a55013, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136010-302.ef1",	0x1000, 0xd3700b3c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136010-303.h1",	0x1000, 0x8e12e3e0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136010-304.j1",	0x1000, 0x467ad5da, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136010-305.kl1",	0x1000, 0x840603af, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136010-306.m1",	0x1000, 0x3f3805ad, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "136002-125.n4",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 10 AVG PROM

	{ "136010-111.r1",	0x0020, 0x6bf2dc46, 0 | BRF_OPT },           // 11 PROMs
	{ "136010-112.r2",	0x0020, 0xb6af29d1, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(gravitar)
STD_ROM_FN(gravitar)

struct BurnDriver BurnDrvGravitar = {
	"gravitar", NULL, NULL, NULL, "1982",
	"Gravitar (version 3)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, gravitarRomInfo, gravitarRomName, NULL, NULL, NULL, NULL, GravitarInputInfo, GravitarDIPInfo,
	GravitarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Gravitar (version 2)

static struct BurnRomInfo gravitar2RomDesc[] = {
	{ "136010-210.l7",	0x0800, 0xdebcb243, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "136010-207.mn7",	0x1000, 0x4135629a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136010-208.np7",	0x1000, 0x358f25d9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136010-209.r7",	0x1000, 0x37034287, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136010-201.d1",	0x1000, 0x167315e4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136010-202.ef1",	0x1000, 0xaaa9e62c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136010-203.h1",	0x1000, 0xae437253, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136010-204.j1",	0x1000, 0x5d6bc29e, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136010-205.kl1",	0x1000, 0x0db1ff34, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136010-206.m1",	0x1000, 0x4521ca48, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "136002-125.n4",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 10 AVG PROM

	{ "136010-111.r1",	0x0020, 0x6bf2dc46, 0 | BRF_OPT },           // 11 PROMs
	{ "136010-112.r2",	0x0020, 0xb6af29d1, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(gravitar2)
STD_ROM_FN(gravitar2)

struct BurnDriver BurnDrvGravitar2 = {
	"gravitar2", "gravitar", NULL, NULL, "1982",
	"Gravitar (version 2)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, gravitar2RomInfo, gravitar2RomName, NULL, NULL, NULL, NULL, GravitarInputInfo, GravitarDIPInfo,
	GravitarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Gravitar (version 1)

static struct BurnRomInfo gravitar1RomDesc[] = {
	{ "136010-110.l7",	0x0800, 0x1da0d845, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "136010-107.mn7",	0x1000, 0x650ba31e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136010-108.np7",	0x1000, 0x5119c0b2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136010-109.r7",	0x1000, 0xdefa8cbc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136010-101.d1",	0x1000, 0xacbc0e2c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136010-102.ef1",	0x1000, 0x88f98f8f, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136010-103.h1",	0x1000, 0x68a85703, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136010-104.j1",	0x1000, 0x33d19ef6, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136010-105.kl1",	0x1000, 0x032b5806, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136010-106.m1",	0x1000, 0x47fe97a0, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "136002-125.n4",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 10 AVG PROM

	{ "136010-111.r1",	0x0020, 0x6bf2dc46, 0 | BRF_OPT },           // 11 PROMs
	{ "136010-112.r2",	0x0020, 0xb6af29d1, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(gravitar1)
STD_ROM_FN(gravitar1)

struct BurnDriver BurnDrvGravitar1 = {
	"gravitar1", "gravitar", NULL, NULL, "1982",
	"Gravitar (version 1)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, gravitar1RomInfo, gravitar1RomName, NULL, NULL, NULL, NULL, GravitarInputInfo, GravitarDIPInfo,
	GravitarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Lunar Battle (prototype, later)

static struct BurnRomInfo lunarbatRomDesc[] = {
	{ "136010-010.l7",	0x0800, 0x48fd38aa, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "136010-007.mn7",	0x1000, 0x9754830e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136010-008.np7",	0x1000, 0x084aa8db, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136010-001.d1",	0x1000, 0xcd7e1780, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136010-002.ef1",	0x1000, 0xdc813a54, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136010-003.h1",	0x1000, 0x8e1fecd3, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136010-004.j1",	0x1000, 0xc407764f, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136010-005.kl1",	0x1000, 0x4feb6f81, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136010-006.m1",	0x1000, 0xf8ad139d, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "136002-125.n4",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  9 AVG PROM

	{ "136010-011.r1",	0x0020, 0x6bf2dc46, 0 | BRF_OPT },           // 10 PROMs
	{ "136010-012.r2",	0x0020, 0xb6af29d1, 0 | BRF_OPT },           // 11
};

STD_ROM_PICK(lunarbat)
STD_ROM_FN(lunarbat)

struct BurnDriver BurnDrvLunarbat = {
	"lunarbat", "gravitar", NULL, NULL, "1982",
	"Lunar Battle (prototype, later)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, lunarbatRomInfo, lunarbatRomName, NULL, NULL, NULL, NULL, GravitarInputInfo, GravitarDIPInfo,
	GravitarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Lunar Battle (prototype, earlier)

static struct BurnRomInfo lunarba1RomDesc[] = {
	{ "vrom1.bin",		0x0800, 0xc60634d9, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "vrom2.bin",		0x1000, 0x53d9a8a2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom0.bin",		0x1000, 0xcc4691c6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom1.bin",		0x1000, 0x4df71d07, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rom2.bin",		0x1000, 0xc6ff04cb, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rom3.bin",		0x1000, 0xa7dc9d1b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rom4.bin",		0x1000, 0x788bf976, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "rom5.bin",		0x1000, 0x16121e13, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136002-125.n4",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  8 AVG PROM

	{ "136010-111.r1",	0x0020, 0x6bf2dc46, 0 | BRF_OPT },           //  9 PROMs
	{ "136010-112.r2",	0x0020, 0xb6af29d1, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(lunarba1)
STD_ROM_FN(lunarba1)

struct BurnDriver BurnDrvLunarba1 = {
	"lunarba1", "gravitar", NULL, NULL, "1982",
	"Lunar Battle (prototype, earlier)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, lunarba1RomInfo, lunarba1RomName, NULL, NULL, NULL, NULL, LunarbatInputInfo, NULL,
	SpacduelInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Space Duel (version 2)

static struct BurnRomInfo spacduelRomDesc[] = {
	{ "136006-106.r7",	0x0800, 0x691122fe, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "136006-107.np7",	0x1000, 0xd8dd0461, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136006-201.r1",	0x1000, 0xf4037b6e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136006-102.np1",	0x1000, 0x4c451e8a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136006-103.m1",	0x1000, 0xee72da63, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136006-104.kl1",	0x1000, 0xe41b38a3, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136006-105.j1",	0x1000, 0x5652710f, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "136002-125.n4",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  7 AVG PROM
};

STD_ROM_PICK(spacduel)
STD_ROM_FN(spacduel)

struct BurnDriver BurnDrvSpacduel = {
	"spacduel", NULL, NULL, NULL, "1980",
	"Space Duel (version 2)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, spacduelRomInfo, spacduelRomName, NULL, NULL, NULL, NULL, SpacduelInputInfo, SpacduelDIPInfo,
	SpacduelInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Space Duel (version 1)

static struct BurnRomInfo spacduel1RomDesc[] = {
	{ "136006-106.r7",	0x0800, 0x691122fe, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "136006-107.np7",	0x1000, 0xd8dd0461, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136006-101.r1",	0x1000, 0xcd239e6c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136006-102.np1",	0x1000, 0x4c451e8a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136006-103.m1",	0x1000, 0xee72da63, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136006-104.kl1",	0x1000, 0xe41b38a3, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136006-105.j1",	0x1000, 0x5652710f, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "136002-125.n4",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  7 AVG PROM
};

STD_ROM_PICK(spacduel1)
STD_ROM_FN(spacduel1)

struct BurnDriver BurnDrvSpacduel1 = {
	"spacduel1", "spacduel", NULL, NULL, "1980",
	"Space Duel (version 1)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, spacduel1RomInfo, spacduel1RomName, NULL, NULL, NULL, NULL, SpacduelInputInfo, SpacduelDIPInfo,
	SpacduelInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};


// Space Duel (prototype)

static struct BurnRomInfo spacduel0RomDesc[] = {
	{ "136006-006.r7",	0x0800, 0x691122fe, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "136006-007.np7",	0x1000, 0xd8dd0461, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136006-001.r1",	0x1000, 0x8f993ac8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136006-002.np1",	0x1000, 0x32cca051, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136006-003.m1",	0x1000, 0x36624d57, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136006-004.kl1",	0x1000, 0xb322bf0b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136006-005.j1",	0x1000, 0x0edb1242, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "136002-125.n4",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  7 AVG PROM
};

STD_ROM_PICK(spacduel0)
STD_ROM_FN(spacduel0)

struct BurnDriver BurnDrvSpacduel0 = {
	"spacduel0", "spacduel", NULL, NULL, "1980",
	"Space Duel (prototype)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, spacduel0RomInfo, spacduel0RomName, NULL, NULL, NULL, NULL, SpacduelInputInfo, SpacduelDIPInfo,
	SpacduelInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	800, 600, 4, 3
};
