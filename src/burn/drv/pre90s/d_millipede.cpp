// Millipede emu-layer for FB Alpha by dink, based on Ivan Mackintosh's Millipede/Centipede emulator and MAME driver.
// Todo:
//   Screen flip needs fixing (2p coctail mode) [move joystick <- -> or press OK to continue!]

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "burn_gun.h"
#include "pokey.h"
#include "earom.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv6502ROM;
static UINT8 *Drv6502RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSpriteRAM;

static UINT8 *DrvBGGFX;
static UINT8 *DrvSpriteGFX;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 dip_select;
static UINT8 control_select;
static UINT32 flipscreen = 0;
static UINT32 vblank;
// transmask stuff
static UINT8 penmask[64];
// trackball stuff
static UINT8 oldpos[4];
static UINT8 sign[4];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoyT[8];
static UINT8 DrvDip[6] = {0, 0, 0, 0, 0, 0};
static UINT8 DrvInput[5];
static UINT8 DrvReset;

static INT16 Analog0PortX = 0;
static INT16 Analog0PortY = 0;
static INT16 Analog1PortX = 0;
static INT16 Analog1PortY = 0;

static UINT32 centipedemode = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo MillipedInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",		    BIT_DIGITAL,	DrvJoy3 + 3,	"p1 up"		},
	{"P1 Down",		    BIT_DIGITAL,	DrvJoy3 + 2,	"p1 down"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy3 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog0PortX,  "p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog0PortY,  "p1 y-axis"),

	{"P2 Coin",		    BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",		    BIT_DIGITAL,	DrvJoy4 + 3,	"p2 up"		},
	{"P2 Down",		    BIT_DIGITAL,	DrvJoy4 + 2,	"p2 down"	},
	{"P2 Left",		    BIT_DIGITAL,	DrvJoy4 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	A("P2 Trackball X", BIT_ANALOG_REL, &Analog1PortX,  "p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &Analog1PortY,  "p2 y-axis"),

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		    BIT_DIGITAL,	DrvJoy3 + 7,	"service"	},
	{"Tilt",		    BIT_DIGITAL,	DrvJoy3 + 4,	"tilt"		},
	{"Dip A",		    BIT_DIPSWITCH,	DrvDip + 0,	    "dip"		},
	{"Dip B",		    BIT_DIPSWITCH,	DrvDip + 1,	    "dip"		},
	{"Dip C",		    BIT_DIPSWITCH,	DrvDip + 2,	    "dip"		},
	{"Dip D",		    BIT_DIPSWITCH,	DrvDip + 3,	    "dip"		},
	{"Dip E",		    BIT_DIPSWITCH,	DrvDip + 4,	    "dip"		},
};

STDINPUTINFO(Milliped)

static struct BurnInputInfo CentipedInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",		    BIT_DIGITAL,	DrvJoy4 + 4,	"p1 up"		},
	{"P1 Down",		    BIT_DIGITAL,	DrvJoy4 + 5,	"p1 down"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy4 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog0PortX,  "p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog0PortY,  "p1 y-axis"),

	{"P2 Coin",		    BIT_DIGITAL,	DrvJoy2 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",		    BIT_DIGITAL,	DrvJoy4 + 1,	"p2 up"		},
	{"P2 Down",		    BIT_DIGITAL,	DrvJoy4 + 0,	"p2 down"	},
	{"P2 Left",		    BIT_DIGITAL,	DrvJoy4 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &Analog1PortX,  "p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &Analog1PortY,  "p2 y-axis"),

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		    BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Tilt",		    BIT_DIGITAL,	DrvJoy2 + 4,	"tilt"		},
	{"Dip A",		    BIT_DIPSWITCH,	DrvDip + 0,	    "dip"		},
	{"Dip B",		    BIT_DIPSWITCH,	DrvDip + 4,	    "dip"		},
	{"Dip C",		    BIT_DIPSWITCH,	DrvDip + 5,	    "dip"		},
};
#undef A

STDINPUTINFO(Centiped)


static struct BurnDIPInfo CentipedDIPList[]=
{
	{0x15, 0xff, 0xff, 0x30, NULL					},  // dip0
	{0x16, 0xff, 0xff, 0x54, NULL					},  // dip4
	{0x17, 0xff, 0xff, 0x02, NULL					},  // dip5

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x15, 0x01, 0x10, 0x00, "Upright"				},
	{0x15, 0x01, 0x10, 0x10, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x15, 0x01, 0x20, 0x20, "Off"					},
	{0x15, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x16, 0x01, 0x03, 0x00, "English"				},
	{0x16, 0x01, 0x03, 0x01, "German"				},
	{0x16, 0x01, 0x03, 0x02, "French"				},
	{0x16, 0x01, 0x03, 0x03, "Spanish"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x16, 0x01, 0x0c, 0x00, "2"					},
	{0x16, 0x01, 0x0c, 0x04, "3"					},
	{0x16, 0x01, 0x0c, 0x08, "4"					},
	{0x16, 0x01, 0x0c, 0x0c, "5"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x16, 0x01, 0x30, 0x00, "10000"				},
	{0x16, 0x01, 0x30, 0x10, "12000"				},
	{0x16, 0x01, 0x30, 0x20, "15000"				},
	{0x16, 0x01, 0x30, 0x30, "20000"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x16, 0x01, 0x40, 0x40, "Easy"					},
	{0x16, 0x01, 0x40, 0x00, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Credit Minimum"		},
	{0x16, 0x01, 0x80, 0x00, "1"					},
	{0x16, 0x01, 0x80, 0x80, "2"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x17, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{0x17, 0x01, 0x03, 0x02, "1 Coin  1 Credits"	},
	{0x17, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x17, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Right Coin"			},
	{0x17, 0x01, 0x0c, 0x00, "*1"					},
	{0x17, 0x01, 0x0c, 0x04, "*4"					},
	{0x17, 0x01, 0x0c, 0x08, "*5"					},
	{0x17, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Left Coin"			},
	{0x17, 0x01, 0x10, 0x00, "*1"					},
	{0x17, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    6, "Bonus Coins"			},
	{0x17, 0x01, 0xe0, 0x00, "None"					},
	{0x17, 0x01, 0xe0, 0x20, "3 credits/2 coins"	},
	{0x17, 0x01, 0xe0, 0x40, "5 credits/4 coins"	},
	{0x17, 0x01, 0xe0, 0x60, "6 credits/4 coins"	},
	{0x17, 0x01, 0xe0, 0x80, "6 credits/5 coins"	},
	{0x17, 0x01, 0xe0, 0xa0, "4 credits/3 coins"	},
};

STDDIPINFO(Centiped)


static struct BurnDIPInfo MillipedDIPList[]=
{
	{0x15, 0xff, 0xff, 0x04, NULL					},
	{0x16, 0xff, 0xff, 0x00, NULL					},
	{0x17, 0xff, 0xff, 0x80, NULL					},
	{0x18, 0xff, 0xff, 0x04, NULL					},
	{0x19, 0xff, 0xff, 0x02, NULL					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x15, 0x01, 0x03, 0x00, "English"				},
	{0x15, 0x01, 0x03, 0x01, "German"				},
	{0x15, 0x01, 0x03, 0x02, "French"				},
	{0x15, 0x01, 0x03, 0x03, "Spanish"				},

	{0   , 0xfe, 0   ,    4, "Bonus"				},
	{0x15, 0x01, 0x0c, 0x00, "0"					},
	{0x15, 0x01, 0x0c, 0x04, "0 1x"					},
	{0x15, 0x01, 0x0c, 0x08, "0 1x 2x"				},
	{0x15, 0x01, 0x0c, 0x0c, "0 1x 2x 3x"			},

	{0   , 0xfe, 0   ,    2, "Credit Minimum"		},
	{0x16, 0x01, 0x04, 0x00, "1"					},
	{0x16, 0x01, 0x04, 0x04, "2"					},

	{0   , 0xfe, 0   ,    2, "Coin Counters"		},
	{0x16, 0x01, 0x08, 0x00, "1"					},
	{0x16, 0x01, 0x08, 0x08, "2"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x17, 0x01, 0x20, 0x20, "Upright"				},
	{0x17, 0x01, 0x20, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x17, 0x01, 0x80, 0x80, "Off"					},
	{0x17, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Millipede Head"		},
	{0x18, 0x01, 0x01, 0x00, "Easy"					},
	{0x18, 0x01, 0x01, 0x01, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Beetle"				},
	{0x18, 0x01, 0x02, 0x00, "Easy"					},
	{0x18, 0x01, 0x02, 0x02, "Hard"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x18, 0x01, 0x0c, 0x00, "2"					},
	{0x18, 0x01, 0x0c, 0x04, "3"					},
	{0x18, 0x01, 0x0c, 0x08, "4"					},
	{0x18, 0x01, 0x0c, 0x0c, "5"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x18, 0x01, 0x30, 0x00, "12000"				},
	{0x18, 0x01, 0x30, 0x10, "15000"				},
	{0x18, 0x01, 0x30, 0x20, "20000"				},
	{0x18, 0x01, 0x30, 0x30, "None"					},

	{0   , 0xfe, 0   ,    2, "Spider"				},
	{0x18, 0x01, 0x40, 0x00, "Easy"					},
	{0x18, 0x01, 0x40, 0x40, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Starting Score Select"},
	{0x18, 0x01, 0x80, 0x80, "Off"					},
	{0x18, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x19, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{0x19, 0x01, 0x03, 0x02, "1 Coin  1 Credits"	},
	{0x19, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x19, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Right Coin"			},
	{0x19, 0x01, 0x0c, 0x00, "*1"					},
	{0x19, 0x01, 0x0c, 0x04, "*4"					},
	{0x19, 0x01, 0x0c, 0x08, "*5"					},
	{0x19, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Left Coin"			},
	{0x19, 0x01, 0x10, 0x00, "*1"					},
	{0x19, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    7, "Bonus Coins"			},
	{0x19, 0x01, 0xe0, 0x00, "None"					},
	{0x19, 0x01, 0xe0, 0x20, "3 credits/2 coins"	},
	{0x19, 0x01, 0xe0, 0x40, "5 credits/4 coins"	},
	{0x19, 0x01, 0xe0, 0x60, "6 credits/4 coins"	},
	{0x19, 0x01, 0xe0, 0x80, "6 credits/5 coins"	},
	{0x19, 0x01, 0xe0, 0xa0, "4 credits/3 coins"	},
	{0x19, 0x01, 0xe0, 0xc0, "Demo Mode"			},
};

STDDIPINFO(Milliped)

static void milliped_set_color(UINT16 offset, UINT8 data)
{
	INT32 bit0, bit1, bit2;

	bit0 = (~data >> 5) & 0x01;
	bit1 = (~data >> 6) & 0x01;
	bit2 = (~data >> 7) & 0x01;
	INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	bit0 = 0;
	bit1 = (~data >> 3) & 0x01;
	bit2 = (~data >> 4) & 0x01;
	INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	bit0 = (~data >> 0) & 0x01;
	bit1 = (~data >> 1) & 0x01;
	bit2 = (~data >> 2) & 0x01;
	INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	UINT32 color = BurnHighCol(r, g, b, 0);

	if (offset < 0x10) // chars
		DrvPalette[offset] = color;
	else
	{
		INT32 base = offset & 0x0c; // sprites

		offset = offset & 0x03;

		for (INT32 i = (base << 6); i < (base << 6) + 0x100; i += 4)
		{
			if (offset == ((i >> 2) & 0x03))
				DrvPalette[i + 0x100 + 1] = color;

			if (offset == ((i >> 4) & 0x03))
				DrvPalette[i + 0x100 + 2] = color;

			if (offset == ((i >> 6) & 0x03))
				DrvPalette[i + 0x100 + 3] = color;
		}
	}
}

static void millipede_recalcpalette()
{
	for (INT32 i = 0; i <= 0x1f; i++) {
		milliped_set_color(i, DrvPalRAM[i]);
	}
}

static void centipede_set_color(UINT16 offset, UINT8 data)
{
	if (offset & 4)	{
		INT32 r = 0xff * ((~data >> 0) & 1);
		INT32 g = 0xff * ((~data >> 1) & 1);
		INT32 b = 0xff * ((~data >> 2) & 1);

		if (~data & 0x08) {
			if (b) b = 0xc0;
			else if (g) g = 0xc0;
		}

		UINT32 color = BurnHighCol(r, g, b, 0);

		if ((offset & 0x08) == 0) // chars
			DrvPalette[offset & 0x03] = color;
		else
		{
			offset = offset & 0x03; // sprites

			for (INT32 i = 0; i < 0x100; i += 4)
			{
				if (offset == ((i >> 2) & 0x03))
					DrvPalette[i + 0x100 + 1] = color;

				if (offset == ((i >> 4) & 0x03))
					DrvPalette[i + 0x100 + 2] = color;

				if (offset == ((i >> 6) & 0x03))
					DrvPalette[i + 0x100 + 3] = color;
			}
		}
	}
}

static void centipede_recalcpalette()
{
	for (INT32 i = 0; i <= 0x0f; i++) {
		centipede_set_color(i, DrvPalRAM[i]);
	}
}

static void millipede_write(UINT16 address, UINT8 data)
{
	address &= 0x7fff; // 15bit addressing
	if (address >= 0x1000 && address <= 0x13bf) { // Video Ram
		DrvVidRAM[address - 0x1000] = data;
		return;
	}
	if (address >= 0x13c0 && address <= 0x13ff) { // Sprite Ram
		DrvSpriteRAM[address - 0x13c0] = data;
		return;
	}
	if (address >= 0x2480 && address <= 0x249f) { // Palette Ram
		DrvPalRAM[address - 0x2480] = data;
		milliped_set_color(address - 0x2480, data);
		return;
	}

	if (address >= 0x400 && address <= 0x40f) { // Pokey 1
		pokey1_w(address - 0x400, data);
		return;
	}

	if (address >= 0x800 && address <= 0x80f) { // Pokey 2
		pokey2_w(address - 0x800, data);
		return;
	}

	if (address >= 0x2780 && address <= 0x27bf) { // EAROM Write
		earom_write(address - 0x2780, data);
		return;
	}

	switch (address)
	{
		case 0x2505:
			dip_select = (~data >> 7) & 1;
		return;

		case 0x2506:
			flipscreen = data >> 7;
		return;
		case 0x2507:
			control_select = (data >> 7) & 1;
		return;
		case 0x2600:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
		case 0x2700:
			earom_ctrl_write(0x2700, data);
		return;
	}
}

static void centipede_write(UINT16 address, UINT8 data)
{
	address &= 0x3fff; // 14bit addressing
	if (address >= 0x400 && address <= 0x7bf) { // Video Ram
		DrvVidRAM[address - 0x400] = data;
		return;
	}
	if (address >= 0x7c0 && address <= 0x7ff) { // Sprite Ram
		DrvSpriteRAM[address - 0x7c0] = data;
		return;
	}
	if (address >= 0x1400 && address <= 0x140f) { // Palette Ram
		DrvPalRAM[address - 0x1400] = data;
		centipede_set_color(address - 0x1400, data);
		return;
	}

	if (address >= 0x1000 && address <= 0x100f) { // Pokey #1
		pokey1_w(address - 0x1000, data);
		return;
	}

	if (address >= 0x1600 && address <= 0x163f) { // EAROM Write
		earom_write(address - 0x1600, data);
		return;
	}

	switch (address)
	{
		case 0x2000: // watchdog
		return;
		case 0x1c07:
			flipscreen = data >> 7;
		return;
		case 0x1680:
			earom_ctrl_write(0x1680, data);
		return;
		case 0x2507:
			control_select = (data >> 7) & 1;
		return;
		case 0x1800:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}

}

static INT32 read_trackball(INT32 idx, INT32 switch_port)
{
	UINT8 newpos;

	if (flipscreen)
		idx += 2;

	if (dip_select) {
		return ((DrvInput[switch_port] | DrvDip[switch_port]) & 0x7f) | sign[idx];
	}

	UINT8 trackie[4] = { BurnTrackballRead(0, 0), BurnTrackballRead(0, 1), BurnTrackballRead(1, 0), BurnTrackballRead(1, 1) };

	newpos = trackie[idx];
	if (newpos != oldpos[idx]) {
		sign[idx] = (newpos - oldpos[idx]) & 0x80;
		oldpos[idx] = newpos;
	}

	return ((DrvInput[switch_port] | DrvDip[switch_port]) & 0x70) | (oldpos[idx] & 0x0f) | sign[idx];
}

static UINT8 silly_milli()
{
	UINT8 inpt = DrvInput[2];
	if (control_select) {
		// p2 select
		UINT8 swappy = DrvInput[3] & 0xf;
		swappy = ((swappy >> 1) & 1) | ((swappy << 1) & 2) |
				 ((swappy >> 1) & 4) | ((swappy << 1) & 8);
		inpt = (inpt&0xf0) | swappy;
	}
	return inpt;
}

static UINT8 millipede_read(UINT16 address)
{
	address &= 0x7fff; // 15bit addressing
	if (address >= 0x1000 && address <= 0x13bf) { // Video Ram
		return DrvVidRAM[address - 0x1000];
	}
	if (address >= 0x13c0 && address <= 0x13ff) { // Sprite Ram
		return DrvSpriteRAM[address - 0x13c0];
	}
	if (address >= 0x2480 && address <= 0x249f) { // Palette Ram
		return DrvPalRAM[address - 0x2480];
	}
	if (address >= 0x4000 && address <= 0x7fff) { // ROM
		return Drv6502ROM[address];
	}

	switch (address)
	{
		case 0x2000: return (read_trackball(0, 0) & ~0x40) | ((vblank) ? 0x40 : 0x00);
		case 0x2001: return read_trackball(1, 1);
		case 0x2010: return silly_milli();
		case 0x2011: return 0x5f | DrvDip[2];
		case 0x2030: return earom_read(address);
		case 0x0400:
		case 0x0401:
		case 0x0402:
		case 0x0403:
		case 0x0404:
		case 0x0405:
		case 0x0406:
		case 0x0407: return pokey1_r(address);
		case 0x0408: return DrvDip[3];
		case 0x0409:
		case 0x040a:
		case 0x040b:
		case 0x040c:
		case 0x040d:
		case 0x040e:
		case 0x040f: return pokey1_r(address);
		case 0x0800:
		case 0x0801:
		case 0x0802:
		case 0x0803:
		case 0x0804:
		case 0x0805:
		case 0x0806:
		case 0x0807: return pokey2_r(address);
		case 0x0808: return DrvDip[4];
		case 0x0809:
		case 0x080a:
		case 0x080b:
		case 0x080c:
		case 0x080d:
		case 0x080e:
		case 0x080f: return pokey2_r(address);
	}

	//bprintf(0, _T("mr %X,"), address);

	return 0;
}

static UINT8 centipede_read(UINT16 address)
{
	address &= 0x3fff; // 14bit addressing
	if (address >= 0x400 && address <= 0x7bf) { // Video Ram
		return DrvVidRAM[address - 0x400];
	}
	if (address >= 0x7c0 && address <= 0x7ff) { // Sprite Ram
		return DrvSpriteRAM[address - 0x7c0];
	}
	if (address >= 0x1400 && address <= 0x140f) { // Palette Ram
		return DrvPalRAM[address - 0x1400];
	}
	if (address >= 0x2000 && address <= 0x3fff) { // ROM
		return Drv6502ROM[address];
	}
	if (address >= 0x1700 && address <= 0x173f) { // EAROM
		return earom_read(address);
	}

	switch (address)
	{
		case 0x0c00: return (read_trackball(0, 0) & ~0x40) | ((vblank) ? 0x40 : 0x00);
		case 0x0c01: return DrvInput[1];
		case 0x0c02: return read_trackball(1, 2);
		case 0x0c03: return DrvInput[3];
		case 0x0800: return DrvDip[4];
		case 0x0801: return DrvDip[5];
		case 0x1000:
		case 0x1001:
		case 0x1002:
		case 0x1003:
		case 0x1004:
		case 0x1005:
		case 0x1006:
		case 0x1007: return pokey1_r(address);
		case 0x1008:
		case 0x1009:
		case 0x100a:
		case 0x100b:
		case 0x100c:
		case 0x100d:
		case 0x100e:
		case 0x100f: return pokey1_r(address);
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	dip_select = 0;
	flipscreen = 0;

	memset(&DrvJoyT, 0, sizeof(DrvJoyT));

	M6502Open(0);
	M6502Reset();
	M6502Close();

	earom_reset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv6502ROM		= Next; Next += 0x08000;

	DrvPalette		= (UINT32*)Next; Next += 0x0600 * sizeof(UINT32);
	DrvBGGFX        = Next; Next += 0x10000;
	DrvSpriteGFX    = Next; Next += 0x10000;

	AllRam			= Next;

	Drv6502RAM		= Next; Next += 0x00400;
	DrvVidRAM		= Next; Next += 0x01000;
	DrvSpriteRAM	= Next; Next += 0x01000;
	DrvPalRAM		= Next; Next += 0x01000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void init_penmask()
{
	for (INT32 i = 0; i < 0x40; i++) {
		penmask[i] = ((!(i & 0x03)) << 1) |
			         ((!(i & 0x0c)) << 2) |
			         ((!(i & 0x30)) << 3) | 1;
	}
}

static INT32 PlaneOffsets[2] = { 0x800 * 8, 0 };
static INT32 CharXOffsets[8] = { STEP8(0, 1) };
static INT32 CharYOffsets[8] = { STEP8(0, 8) };

static INT32 SpriteXOffsets[8] = { STEP8(0, 1) };
static INT32 SpriteYOffsets[16] = { STEP16(0, 8) };

static INT32 DrvInit() // millipede
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv6502ROM + 0x4000, 0, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x5000, 1, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x6000, 2, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x7000, 3, 1)) return 1;

		UINT8 *DrvTempRom = (UINT8 *)BurnMalloc(0x10000);
		memset(DrvTempRom, 0, 0x10000);
		if (BurnLoadRom(DrvTempRom         , 4, 1)) return 1;
		if (BurnLoadRom(DrvTempRom+0x800   , 5, 1)) return 1;
		GfxDecode(0x100, 2, 8, 8, PlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvBGGFX);
		GfxDecode(0x80, 2, 8, 16, PlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x80, DrvTempRom, DrvSpriteGFX);
		BurnFree(DrvTempRom);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(Drv6502RAM,	0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(Drv6502ROM + 0x4000,	0x4000, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(millipede_write);
	M6502SetReadHandler(millipede_read);
	M6502SetReadOpArgHandler(millipede_read);
	M6502SetReadOpHandler(millipede_read);
	M6502Close();

	PokeyInit(1512000, 2, 1.00, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);

	init_penmask();

	GenericTilesInit();

	earom_init();

	BurnTrackballInit(2);

	DrvDoReset();

	return 0;
}

static INT32 DrvInitcentiped()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv6502ROM + 0x2000, 0, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x2800, 1, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x3000, 2, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x3800, 3, 1)) return 1;

		UINT8 *DrvTempRom = (UINT8 *)BurnMalloc(0x10000);
		memset(DrvTempRom, 0, 0x10000);
		if (BurnLoadRom(DrvTempRom         , 4, 1)) return 1;
		if (BurnLoadRom(DrvTempRom+0x800   , 5, 1)) return 1;
		GfxDecode(0x100, 2, 8, 8, PlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvBGGFX);
		GfxDecode(0x80, 2, 8, 16, PlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x80, DrvTempRom, DrvSpriteGFX);
		BurnFree(DrvTempRom);
	}

	centipedemode = 1;

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x3fff);
	M6502MapMemory(Drv6502RAM,	0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(Drv6502ROM + 0x2000,	0x2000, 0x3fff, MAP_ROM);
	M6502SetWriteHandler(centipede_write);
	M6502SetReadHandler(centipede_read);
	M6502SetReadOpArgHandler(centipede_read);
	M6502SetReadOpHandler(centipede_read);
	M6502Close();

	PokeyInit(1512000, 2, 2.40, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);

	init_penmask();

	GenericTilesInit();

	earom_init();

	BurnTrackballInit(2);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	PokeyExit();

	M6502Exit();

	earom_exit();

	BurnFree(AllMem);

	BurnTrackballExit();

	centipedemode = 0;
	dip_select = 0;
	flipscreen = 0;

	memset(&DrvDip, 0, sizeof(DrvDip));

	return 0;
}

static void draw_bg()
{
	UINT8 *videoram = DrvVidRAM;

	for (INT32 offs = 0; offs <= 0x3bf; offs++)
	{
		INT32 flip_tiles;
		INT32 sx = offs % 32;
		INT32 sy = offs / 32;

		INT32 data = videoram[offs];
		INT32 bank = ((data >> 6) & 1);
		INT32 color = (data >> 6) & 3;
		// Flip both x and y if flipscreen is non-zero
		flip_tiles = (flipscreen) ? 0x03 : 0;

		if (centipedemode) {
			bank = 0;
			color = 0;
			flip_tiles = data >> 6;
		}

		INT32 code = (data & 0x3f) + 0x40 + (bank * 0x80);

		sx = 8 * sx;
		sy = 8 * sy;
		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		if (flipscreen) {
			sx = nScreenWidth - 8 - sx;
			sy = nScreenHeight - 8 - sy;
			flip_tiles ^= 3;
		}

		Draw8x8Tile(pTransDraw, code, sx, sy, flip_tiles & 1, flip_tiles & 2, color, 2, 0, DrvBGGFX);
	}
}

static void RenderTileCPMP(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height)
{
	UINT16 *dest = pTransDraw;
	UINT8 *gfx = DrvSpriteGFX;

	INT32 flip = 0;
	if (flipy) flip |= (height - 1) * width;
	if (flipx) flip |= width - 1;

	gfx += code * width * height;

	for (INT32 y = 0; y < height; y++, sy++) {
		if (sy < 0 || sy >= nScreenHeight) continue;

		for (INT32 x = 0; x < width; x++, sx++) {
			if (sx < 0 || sx >= nScreenWidth - 8) continue; // clip top 8px of sprites (top of screen)

			INT32 pxl = gfx[((y * width) + x) ^ flip];

			if (penmask[color & 0x3f] & (1 << pxl) || !pxl) continue;
			dest[sy * nScreenWidth + sx] = pxl + (color << 2) + 0x100;
		}
		sx -= width;
	}
}


static void draw_sprites()
{
	UINT8 *spriteram = DrvSpriteRAM;

	for (INT32 offs = 0; offs < 0x10; offs++) {
		INT32 code = (((spriteram[offs] & 0x3e) >> 1) | ((spriteram[offs] & 0x01) << 6)) & 0x7f;
		INT32 color = spriteram[offs + 0x30] & ((centipedemode) ? 0x3f : 0xff);
		INT32 flipx = (centipedemode) ? (spriteram[offs] >> 6) & 1 : flipscreen;
		INT32 flipy = (centipedemode) ? (spriteram[offs] >> 7) & 1 : (spriteram[offs] & 0x80);
		INT32 x = spriteram[offs + 0x20];
		INT32 y = 240 - spriteram[offs + 0x10];

		if (flipx && !centipedemode) {
			flipy = !flipy;
		}

		if (flipscreen) {
			x = nScreenWidth - 8 - x;
			y = nScreenHeight - 16 - y;
			flipx = !flipx;
			flipy = !flipy;
		}

		RenderTileCPMP(code, color, x, y, flipx, flipy, 8, 16);
	}
}

static INT32 DrvDraw()
{
	BurnTransferClear();

	if (nBurnLayer & 1) draw_bg();
	if (nBurnLayer & 2) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void DrvMakeInputs()
{
	// Reset Inputs - bring active-LOW stuff HIGH
	DrvInput[0] = (centipedemode) ? 0x00 : 0x10 + 0x20;
	DrvInput[1] = (centipedemode) ? 0xff : 0x01 + 0x02 + 0x10 + 0x20 + 0x40;
	DrvInput[2] = 0xff;
	DrvInput[3] = (centipedemode) ? 0xff : 0x01 + 0x02 + 0x04 + 0x08 + 0x10 + 0x40;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInput[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInput[2] ^= (DrvJoy3[i] & 1) << i;
		DrvInput[3] ^= (DrvJoy4[i] & 1) << i;
	}

}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	if(DrvRecalc) {
		if (centipedemode)
			centipede_recalcpalette();
		else
			millipede_recalcpalette();
		DrvRecalc = 0;
	}

	{
		DrvMakeInputs();

		BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
		BurnTrackballFrame(0, Analog0PortX, Analog0PortY, 1, 7);
		BurnTrackballUpdate(0);

		BurnTrackballConfig(1, AXIS_NORMAL, AXIS_REVERSED);
		BurnTrackballFrame(1, Analog1PortX, Analog1PortY, 1, 7);
		BurnTrackballUpdate(1);
	}

	INT32 nCyclesTotal[1] = { 1512000 / 60 };
	INT32 nCyclesDone[1] = { 0 };
	INT32 nInterleave = 4;

	vblank = 0;

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, M6502);
		M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);

		if (i == 1) { BurnTrackballUpdate(0); BurnTrackballUpdate(1); }
		if (i == 2)
		    vblank = 1;
	}

	M6502Close();
	if (pBurnSoundOut) {
		pokey_update(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		pokey_scan(nAction, pnMin);

		BurnTrackballScan();

		SCAN_VAR(dip_select);
		SCAN_VAR(control_select);
		SCAN_VAR(flipscreen);
		SCAN_VAR(oldpos);
		SCAN_VAR(sign);
	}

	earom_scan(nAction, pnMin);

	return 0;
}


// Millipede

static struct BurnRomInfo millipedRomDesc[] = {
	{ "136013-104.mn1",	0x1000, 0x40711675, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136013-103.l1",	0x1000, 0xfb01baf2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136013-102.jk1",	0x1000, 0x62e137e0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136013-101.h1",	0x1000, 0x46752c7d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136013-107.r5",	0x0800, 0x68c3437a, 2 | BRF_PRG | BRF_ESS }, //  4 gfx1
	{ "136013-106.p5",	0x0800, 0xf4468045, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "136001-213.e7",	0x0100, 0x6fa3093a, 3 | BRF_PRG | BRF_ESS }, //  6 proms
};

STD_ROM_PICK(milliped)
STD_ROM_FN(milliped)

struct BurnDriver BurnDrvMilliped = {
	"milliped", NULL, NULL, NULL, "1982",
	"Millipede\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION, 0,
	NULL, millipedRomInfo, millipedRomName, NULL, NULL, NULL, NULL, MillipedInputInfo, MillipedDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	240, 256, 3, 4
};

// Centipede (revision 4)
static struct BurnRomInfo centipedRomDesc[] = {
	{ "136001-407.d1",	0x0800, 0xc4d995eb, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136001-408.e1",	0x0800, 0xbcdebe1b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136001-409.fh1",	0x0800, 0x66d7b04a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136001-410.j1",	0x0800, 0x33ce4640, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136001-211.f7",	0x0800, 0x880acfb9, 2 | BRF_PRG | BRF_ESS }, //  4 gfx1
	{ "136001-212.hj7",	0x0800, 0xb1397029, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "136001-213.p4",	0x0100, 0x6fa3093a, 3 | BRF_PRG | BRF_ESS }, //  6 proms
};


STD_ROM_PICK(centiped)
STD_ROM_FN(centiped)

struct BurnDriver BurnDrvCentiped = {
	"centiped", NULL, NULL, NULL, "1980",
	"Centipede (revision 4)\0", "1 Player version", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 1, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION, 0,
	NULL, centipedRomInfo, centipedRomName, NULL, NULL, NULL, NULL, CentipedInputInfo, CentipedDIPInfo,
	DrvInitcentiped, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	240, 256, 3, 4
};

// Centipede (revision 3)

static struct BurnRomInfo centiped3RomDesc[] = {
	{ "136001-307.d1",	0x0800, 0x5ab0d9de, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136001-308.e1",	0x0800, 0x4c07fd3e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136001-309.fh1",	0x0800, 0xff69b424, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136001-310.j1",	0x0800, 0x44e40fa4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136001-211.f7",	0x0800, 0x880acfb9, 2 | BRF_PRG | BRF_ESS }, //  4 gfx1
	{ "136001-212.hj7",	0x0800, 0xb1397029, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "136001-213.p4",	0x0100, 0x6fa3093a, 3 | BRF_PRG | BRF_ESS }, //  6 proms
};

STD_ROM_PICK(centiped3)
STD_ROM_FN(centiped3)

struct BurnDriver BurnDrvCentiped3 = {
	"centiped3", "centiped", NULL, NULL, "1980",
	"Centipede (revision 3)\0", "2 Player version", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION, 0,
	NULL, centiped3RomInfo, centiped3RomName, NULL, NULL, NULL, NULL, CentipedInputInfo, CentipedDIPInfo,
	DrvInitcentiped, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	240, 256, 3, 4
};

