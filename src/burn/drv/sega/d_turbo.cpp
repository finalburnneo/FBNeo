// FB Alpha Sega Z80-3D system driver module
// Based on and large pieces copied from (video code & sound code) MAME driver by 
// Alex Pasadyn, Howie Cohen, Frank Palazzolo, Ernesto Corvi, and Aaron Giles

// done:
//  done: sample freq. hooked up to turbo. iq/dnk

// to do:
//  done: sample freq. hooked up to turbo.
//  collisions don't work (turbo)
//	add 9-seg support (i8279)
//	bug testing
//	fixing sounds (subroc3d is bad)
//	sound disabling
//	clean up

#include "tiles_generic.h"
#include "z80_intf.h"
#include "samples.h"
#include "8255ppi.h"
#include "math.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80Dec;
static UINT8 *DrvSubROM;
static UINT8 *DrvSprROM;
static UINT8 *DrvFgROM;
static UINT8 *DrvRoadROM;
static UINT8 *DrvBgColor;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprPOS;
static UINT8 *DrvScrRAM;
static UINT8 *DrvBmpRAM;
static UINT8 *DrvSubRAM;
static UINT16 *DrvBitmap;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 turbo_op[3];
static UINT8 turbo_ip[3];
static UINT8 turbo_fbpla;
static UINT8 turbo_fbcol;
static UINT8 turbo_last_analog;
static UINT8 turbo_collision;
static UINT8 turbo_bsel;
static UINT8 turbo_accel;

static UINT8 sound_data[3];

static UINT8 subroc3d_ply;
static UINT8 subroc3d_flip;
static UINT8 subroc3d_col;

static UINT8 buckrog_command;
static UINT8 buckrog_mov;
static UINT8 buckrog_fchg;
static UINT8 buckrog_obch;

static INT32 DrvDial; // turbo

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy4[8]; //fake
static UINT8 DrvDips[3];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo TurboInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 4,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Turbo)

static struct BurnInputInfo Subroc3dInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 5,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 4,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Subroc3d)

static struct BurnInputInfo BuckrogInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Buckrog)

static struct BurnDIPInfo TurboDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xfb, NULL					},
	{0x0b, 0xff, 0xff, 0xff, NULL					},
	{0x0c, 0xff, 0xff, 0xf0, NULL					},

	{0   , 0xfe, 0   ,    4, "Car On Extended Play"	},
	{0x0a, 0x01, 0x03, 0x00, "1"					},
	{0x0a, 0x01, 0x03, 0x01, "2"					},
	{0x0a, 0x01, 0x03, 0x02, "3"					},
	{0x0a, 0x01, 0x03, 0x03, "4"					},

	{0   , 0xfe, 0   ,    2, "Game Time"			},
	{0x0a, 0x01, 0x04, 0x04, "Fixed (55 sec)"		},
	{0x0a, 0x01, 0x04, 0x00, "Adjustable"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x0a, 0x01, 0x08, 0x08, "Easy"					},
	{0x0a, 0x01, 0x08, 0x00, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Game Mode"			},
	{0x0a, 0x01, 0x10, 0x00, "No Collisions (cheat)"},
	{0x0a, 0x01, 0x10, 0x10, "Normal"				},

	{0   , 0xfe, 0   ,    2, "Initial Entry"		},
	{0x0a, 0x01, 0x20, 0x00, "Off"					},
	{0x0a, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x0a, 0x01, 0x40, 0x40, "Off"					},
	{0x0a, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x0a, 0x01, 0x80, 0x80, "Off"					},
	{0x0a, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Game Time"			},
	{0x0b, 0x01, 0x03, 0x00, "60 seconds"			},
	{0x0b, 0x01, 0x03, 0x01, "70 seconds"			},
	{0x0b, 0x01, 0x03, 0x02, "80 seconds"			},
	{0x0b, 0x01, 0x03, 0x03, "90 seconds"			},

	{0   , 0xfe, 0   ,    7, "Coin B"				},
	{0x0b, 0x01, 0x1c, 0x18, "4 Coins 1 Credits"	},
	{0x0b, 0x01, 0x1c, 0x14, "3 Coins 1 Credits"	},
	{0x0b, 0x01, 0x1c, 0x10, "2 Coins 1 Credits"	},
	{0x0b, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0x1c, 0x04, "1 Coin  2 Credits"	},
	{0x0b, 0x01, 0x1c, 0x08, "1 Coin  3 Credits"	},
	{0x0b, 0x01, 0x1c, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    7, "Coin A"				},
	{0x0b, 0x01, 0xe0, 0xc0, "4 Coins 1 Credits"	},
	{0x0b, 0x01, 0xe0, 0xa0, "3 Coins 1 Credits"	},
	{0x0b, 0x01, 0xe0, 0x80, "2 Coins 1 Credits"	},
	{0x0b, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0xe0, 0x20, "1 Coin  2 Credits"	},
	{0x0b, 0x01, 0xe0, 0x40, "1 Coin  3 Credits"	},
	{0x0b, 0x01, 0xe0, 0x60, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Tachometer"			},
	{0x0c, 0x01, 0x40, 0x40, "Analog (Meter)"		},
	{0x0c, 0x01, 0x40, 0x00, "Digital (LED)"		},

	{0   , 0xfe, 0   ,    2, "Sound System"			},
	{0x0c, 0x01, 0x80, 0x80, "Upright"				},
	{0x0c, 0x01, 0x80, 0x00, "Cockpit"				},
};

STDDIPINFO(Turbo)

static struct BurnDIPInfo Subroc3dDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x40, NULL					},
	{0x0b, 0xff, 0xff, 0xbf, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x0a, 0x01, 0x07, 0x07, "5 Coins 1 Credits"	},
	{0x0a, 0x01, 0x07, 0x06, "4 Coins 1 Credits"	},
	{0x0a, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x0a, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x0a, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x0a, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x0a, 0x01, 0x38, 0x38, "5 Coins 1 Credits"	},
	{0x0a, 0x01, 0x38, 0x30, "4 Coins 1 Credits"	},
	{0x0a, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x0a, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0x38, 0x00, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0x38, 0x08, "1 Coin  2 Credits"	},
	{0x0a, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x0a, 0x01, 0x38, 0x18, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0a, 0x01, 0xc0, 0x00, "2"					},
	{0x0a, 0x01, 0xc0, 0x40, "3"					},
	{0x0a, 0x01, 0xc0, 0x80, "4"					},
	{0x0a, 0x01, 0xc0, 0xc0, "5"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x0b, 0x01, 0x03, 0x00, "20000"				},
	{0x0b, 0x01, 0x03, 0x01, "40000"				},
	{0x0b, 0x01, 0x03, 0x02, "60000"				},
	{0x0b, 0x01, 0x03, 0x03, "80000"				},

	{0   , 0xfe, 0   ,    2, "Initial Entry"		},
	{0x0b, 0x01, 0x04, 0x00, "Off"					},
	{0x0b, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x0b, 0x01, 0x08, 0x08, "Normal"				},
	{0x0b, 0x01, 0x08, 0x00, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x0b, 0x01, 0x10, 0x10, "Off"					},
	{0x0b, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Motion"				},
	{0x0b, 0x01, 0x20, 0x00, "Stop"					},
	{0x0b, 0x01, 0x20, 0x20, "Normal"				},

	{0   , 0xfe, 0   ,    2, "Screen"				},
	{0x0b, 0x01, 0x40, 0x00, "Mono"					},
	{0x0b, 0x01, 0x40, 0x40, "Stereo"				},

	{0   , 0xfe, 0   ,    2, "Game"					},
	{0x0b, 0x01, 0x80, 0x00, "Endless"				},
	{0x0b, 0x01, 0x80, 0x80, "Normal"				},
};

STDDIPINFO(Subroc3d)

static struct BurnDIPInfo BuckrogDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x00, NULL					},
	{0x0c, 0xff, 0xff, 0xc0, NULL					},
	{0x0d, 0xff, 0xff, 0x12, NULL					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0b, 0x01, 0x10, 0x00, "Off"					},
	{0x0b, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x0c, 0x01, 0x07, 0x07, "5 Coins 1 Credits"	},
	{0x0c, 0x01, 0x07, 0x06, "4 Coins 1 Credits"	},
	{0x0c, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x0c, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x0c, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x0c, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x0c, 0x01, 0x38, 0x38, "5 Coins 1 Credits"	},
	{0x0c, 0x01, 0x38, 0x30, "4 Coins 1 Credits"	},
	{0x0c, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x0c, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x38, 0x00, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0x38, 0x08, "1 Coin  2 Credits"	},
	{0x0c, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x0c, 0x01, 0x38, 0x18, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Collisions"			},
	{0x0d, 0x01, 0x01, 0x01, "Off"					},
	{0x0d, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Accel by"				},
	{0x0d, 0x01, 0x02, 0x00, "Pedal"				},
	{0x0d, 0x01, 0x02, 0x02, "Button"				},

	{0   , 0xfe, 0   ,    2, "Best 5 Scores"		},
	{0x0d, 0x01, 0x04, 0x04, "Off"					},
	{0x0d, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Score Display"		},
	{0x0d, 0x01, 0x08, 0x00, "Off"					},
	{0x0d, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x0d, 0x01, 0x10, 0x10, "Normal"				},
	{0x0d, 0x01, 0x10, 0x00, "Hard"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0d, 0x01, 0x60, 0x00, "3"					},
	{0x0d, 0x01, 0x60, 0x20, "4"					},
	{0x0d, 0x01, 0x60, 0x40, "5"					},
	{0x0d, 0x01, 0x60, 0x60, "6"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0d, 0x01, 0x80, 0x00, "Upright"				},
	{0x0d, 0x01, 0x80, 0x80, "Cockpit"				},
};

STDDIPINFO(Buckrog)

static void __fastcall turbo_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xa000) {
		address = (address & 7) | ((address >> 1) & 0x78);
		DrvSprRAM[address] = data;
		return;
	}

	if ((address & 0xf807) == 0xa800) address &= ~0x07f8;
	if ((address & 0xfc00) == 0xf800) address &= ~0x00fc;
	if ((address & 0xff00) == 0xfc00) address &= ~0x00fe;

	if ((address & 0xf800) == 0xb800) {
		turbo_last_analog = DrvDial;
		return;
	}

	if ((address & 0xf800) == 0xe800) {
		turbo_collision = 0;
		return;
	}

	switch (address)
	{
		case 0xa800: // coin counter 1
		case 0xa801: // coin counter 2
		case 0xa802: // lamps
		case 0xa803:
		case 0xa804:
		case 0xa805:
		case 0xa806:
		case 0xa807:
			data &= 1; // ls259
		return;

		case 0xf800:
		case 0xf801:
		case 0xf802:
		case 0xf803:
		case 0xf900:
		case 0xf901:
		case 0xf902:
		case 0xf903:
		case 0xfa00:
		case 0xfa01:
		case 0xfa02:
		case 0xfa03:
		case 0xfb00:
		case 0xfb01:
		case 0xfb02:
		case 0xfb03:
			ppi8255_w((address >> 8) & 3, address & 3, data);
		return;

		case 0xfc00:
		case 0xfc01: // i8279 write (handling segment output)
		return;
	}
}

static UINT8 __fastcall turbo_read(UINT16 address)
{
	if ((address & 0xf800) == 0xa000) {
		address = (address & 7) | ((address >> 1) & 0x78);
		return DrvSprRAM[address];
	}

	if ((address & 0xfc00) == 0xf800) address &= ~0x00fc;
	if ((address & 0xff00) == 0xfc00) address &= ~0x00fe;
	if ((address & 0xff00) == 0xfd00) address &= ~0x00ff;

	if ((address & 0xff00) == 0xfe00) {
		return (DrvDips[2] & 0xf0) | (turbo_collision & 0xf); // "dsw3"
	}

	switch (address)
	{
		case 0xf800:
		case 0xf801:
		case 0xf802:
		case 0xf803:
		case 0xf900:
		case 0xf901:
		case 0xf902:
		case 0xf903:
		case 0xfa00:
		case 0xfa01:
		case 0xfa02:
		case 0xfa03:
		case 0xfb00:
		case 0xfb01:
		case 0xfb02:
		case 0xfb03:
			return ppi8255_r((address >> 8) & 3, address & 3);

		case 0xfc00: // should use i8279 device
			return DrvDips[0];

		case 0xfc01:// i8279 read
			return 0x10;

		case 0xfd00:
			return DrvInputs[0]; // "input"
	}
	
	return 0;
}

static void __fastcall subroc3d_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xf800) address &= ~0x07fe;

	switch (address & ~0x7fc)
	{
		case 0xe800:
		case 0xe801:
		case 0xe802:
		case 0xe803:
			ppi8255_w(0, address & 3, data);
		return;

		case 0xf000:
		case 0xf001:
		case 0xf002:
		case 0xf003:
			ppi8255_w(1, address & 3, data);
		return;
	}
}

static UINT8 __fastcall subroc3d_read(UINT16 address)
{
	if ((address & 0xf800) == 0xf800) address &= ~0x07fe;

	switch (address & ~0x7fc)
	{
		case 0xa800:
			return DrvInputs[0];

		case 0xa801:
			return DrvInputs[1];

		case 0xa802:
			return DrvDips[1];

		case 0xa803:
			return DrvDips[2];

		case 0xe800:
		case 0xe801:
		case 0xe802:
		case 0xe803:
			return ppi8255_r(0, address & 3);

		case 0xf000:
		case 0xf001:
		case 0xf002:
		case 0xf003:
			return ppi8255_r(1, address & 3);

		case 0xf800: // should use i8279 device
			return DrvDips[0];

		case 0xf801:// i8279 read
			return 0x10;
	}
	
	return 0;
}

static void __fastcall buckrog_write(UINT16 address, UINT8 data)
{
	switch (address & ~0x7fc)
	{
		case 0xc800:
		case 0xc801:
		case 0xc802:
		case 0xc803: // sync before writing another command to the sub cpu
		{
			INT32 cycles = ZetTotalCycles();
			ZetClose();
			ZetOpen(1);
			ZetRun(cycles - ZetTotalCycles());
			ZetClose();
			ZetOpen(0);
			ppi8255_w(0, address & 3, data);
		}
		return;

		case 0xd000:
		case 0xd001:
		case 0xd002:
		case 0xd003:
			ppi8255_w(1, address & 3, data);
		return;

		case 0xf800: // ??
		return;
	}
}

static UINT8 buckrog_port_2_read()
{
	int inp1 = DrvDips[0];
	int inp2 = DrvDips[1];

	return  (((inp2 >> 6) & 1) << 7) |
			(((inp2 >> 4) & 1) << 6) |
			(((inp2 >> 3) & 1) << 5) |
			(((inp2 >> 0) & 1) << 4) |
			(((inp1 >> 6) & 1) << 3) |
			(((inp1 >> 4) & 1) << 2) |
			(((inp1 >> 3) & 1) << 1) |
			(((inp1 >> 0) & 1) << 0);
}

static UINT8 buckrog_port_3_read()
{
	int inp1 = DrvDips[0];
	int inp2 = DrvDips[1];

	return  (((inp2 >> 7) & 1) << 7) |
			(((inp2 >> 5) & 1) << 6) |
			(((inp2 >> 2) & 1) << 5) |
			(((inp2 >> 1) & 1) << 4) |
			(((inp1 >> 7) & 1) << 3) |
			(((inp1 >> 5) & 1) << 2) |
			(((inp1 >> 2) & 1) << 1) |
			(((inp1 >> 1) & 1) << 0);
}


static UINT8 __fastcall buckrog_read(UINT16 address)
{
	switch (address & ~0x7fc)
	{
		case 0xc800:
		case 0xc801:
		case 0xc802:
		case 0xc803:
			return ppi8255_r(0, address & 3);

		case 0xd000:
		case 0xd001:
		case 0xd002:
		case 0xd003:
			return ppi8255_r(1, address & 3);

		case 0xd800:
		case 0xd802:
			return DrvDips[0];

		case 0xd801:
		case 0xd803:
			return 0x10;

		case 0xe800:
			return DrvInputs[0];

		case 0xe801:
			return DrvInputs[1];

		case 0xe802:
			return buckrog_port_2_read();

		case 0xe803:
			return buckrog_port_3_read();

		case 0xf800: // ??
			return 0;
	}

	return 0;
}

static void __fastcall buckrog_sub_write(UINT16 address, UINT8 data)
{
	if (address < 0xe000) {
		DrvBmpRAM[address] = data & 1;
		return;
	}
}

static UINT8 __fastcall buckrog_sub_read_port(UINT16)
{
	ppi8255_set_portC(0,0);
	return buckrog_command;
}



static void turbo_ppi0a_write(UINT8 data)
{
	turbo_op[0] = data;
}

static void turbo_ppi0b_write(UINT8 data)
{
	turbo_op[1] = data;
}

static void turbo_ppi0c_write(UINT8 data)
{
	turbo_op[2] = data;
}

static void turbo_ppi1a_write(UINT8 data)
{
	turbo_ip[0] = data;
}

static void turbo_ppi1b_write(UINT8 data)
{
	turbo_ip[1] = data;
}

static void turbo_ppi1c_write(UINT8 data)
{
	turbo_ip[2] = data;
}

static void turbo_update_samples()
{
	if (turbo_bsel == 3 && BurnSampleGetStatus(7))
	{
		BurnSampleStop(7);
	}
	else if (turbo_bsel != 3 && !BurnSampleGetStatus(7))
	{
		BurnSamplePlay(7);
	}
	
	if (BurnSampleGetStatus(7)) {
		// my math sucks, there might be a better way to do this:  -dink
		INT32 percentyderp = (((nBurnSoundRate * ((turbo_accel & 0x3f) / 5.25 + 1)) - nBurnSoundRate) / nBurnSoundRate * 100) + 100;
		BurnSampleSetPlaybackRate(7, percentyderp);
	}
}

static void turbo_ppi2a_write(UINT8 data)
{
	UINT8 diff = data ^ sound_data[0];
	sound_data[0] = data;

	if ((diff & 0x01) && !(data & 0x01)) BurnSamplePlay(5);
	if ((diff & 0x02) && !(data & 0x02)) BurnSamplePlay(0);
	if ((diff & 0x04) && !(data & 0x04)) BurnSamplePlay(1);
	if ((diff & 0x08) && !(data & 0x08)) BurnSamplePlay(2);
	if ((diff & 0x10) && !(data & 0x10)) BurnSamplePlay(3);
	if ((diff & 0x40) && !(data & 0x40)) BurnSamplePlay(4);
	if ((diff & 0x80) && !(data & 0x80)) BurnSamplePlay(5);
	
	turbo_update_samples();
}

static void turbo_ppi2b_write(UINT8 data)
{
	UINT8 diff = data ^ sound_data[1];
	sound_data[1] = data;

	turbo_accel = data & 0x3f;

	if ((diff & 0x40) && !(data & 0x40) && !BurnSampleGetStatus(8)) BurnSamplePlay(8);
	if ((diff & 0x40) &&  (data & 0x40)) BurnSampleStop(8);
	if ((diff & 0x80) && !(data & 0x80)) BurnSamplePlay(6);

	turbo_update_samples();
}

static void turbo_ppi2c_write(UINT8 data)
{
	turbo_bsel = (data >> 2) & 3;
	turbo_update_samples();
}

static UINT8 turbo_ppi3a_read()
{
	return DrvDial - turbo_last_analog;
}

static UINT8 turbo_ppi3b_read()
{
	return DrvDips[1]; // "dsw2"
}

static void turbo_ppi3c_write(UINT8 data)
{
	turbo_fbpla = data & 0x0f;
	turbo_fbcol = (data >> 4) & 0x07;
}

static void subroc3d_ppi0a_write(UINT8 data)
{
	subroc3d_ply = data & 0x0f;
}

static void subroc3d_ppi0b_write(UINT8 data)
{
	// coin counter = data & 3
	// lame = data & 4;

	subroc3d_flip = (data >> 4) & 1;
}

static void subroc3d_ppi0c_write(UINT8 data)
{
	subroc3d_col = data & 0x0f;
}

#if 0
static void subroc3d_update_volume(int leftchan, UINT8 dis, UINT8 dir)
{
	float volume = (float)(15 - dis) / 16.0f;
	float lvol, rvol;

	/* compute the left/right volume from the data */
	if (dir != 7)
	{
		lvol = volume * (float)(6 - dir) / 6.0f;
		rvol = volume * (float)dir / 6.0f;
	}
	else
		lvol = rvol = 0;

	/* if the sample is playing, adjust it */
	sample_set_volume(leftchan + 0, lvol);
	sample_set_volume(leftchan + 1, rvol);
}
#endif

static void subroc3d_ppi1a_write(UINT8 data)
{
	sound_data[0] = data;
}

static void subroc3d_ppi1b_write(UINT8 data)
{
	UINT8 diff = data ^ sound_data[1];
	sound_data[1] = data;

	if ((diff & 0x01) && (data & 0x01))
	{
	//	subroc3d_mdis = sound_data[0] & 0x0f;
	//	subroc3d_mdir = (sound_data[0] >> 4) & 0x07;
		
		if (!BurnSampleGetStatus(0))
		{
			BurnSamplePlay(0);
		}
	//	subroc3d_update_volume(0, subroc3d_mdis, subroc3d_mdir);
	}

	if ((diff & 0x02) && (data & 0x02))
	{
	//	subroc3d_tdis = sound_data[0] & 0x0f;
	//	subroc3d_tdir = (sound_data[0] >> 4) & 0x07;
		if (!BurnSampleGetStatus(1))
		{
			BurnSamplePlay(1);
		}
	//	subroc3d_update_volume(2, subroc3d_tdis, subroc3d_tdir);
	}

	if ((diff & 0x04) && (data & 0x04))
	{
	//	subroc3d_fdis = sound_data[0] & 0x0f;
	//	subroc3d_fdir = (sound_data[0] >> 4) & 0x07;
		if (!BurnSampleGetStatus(2))
		{
			BurnSamplePlay(2);
		}
	//	subroc3d_update_volume(4, subroc3d_fdis, subroc3d_fdir);
	}

	if ((diff & 0x08) && (data & 0x08))
	{
	//	subroc3d_hdis = sound_data[0] & 0x0f;
	//	subroc3d_hdir = (sound_data[0] >> 4) & 0x07;
	//	subroc3d_update_volume(6, subroc3d_hdis, subroc3d_hdir);
	}
}

static void subroc3d_ppi1c_write(UINT8 data)
{
	UINT8 diff = data ^ sound_data[2];
	sound_data[2] = data;

	if ((diff & 0x01) && (data & 0x01))
		BurnSamplePlay((data & 0x02) ? 6 : 5);

	if ((diff & 0x04) && (data & 0x04))
		BurnSamplePlay(7);

	if ((diff & 0x08) && (data & 0x08))
	{
		BurnSamplePlay((sound_data[0] & 0x80) ? 4 : 3);
	}

	if ((diff & 0x10) && (data & 0x10))
		BurnSamplePlay((data & 0x20) ? 10 : 9);

	if (!BurnSampleGetStatus(8))
		BurnSamplePlay(8);

//	sample_set_volume(11, (data & 0x40) ? 0 : 1.0);

//	sound_global_enable(!(data & 0x80));
}

static void buckrog_ppi0a_write(UINT8 data)
{
	buckrog_command = data;
}

static void buckrog_ppi0b_write(UINT8 data)
{
	buckrog_mov = data & 0x3f;
}

static void buckrog_ppi0c_write(UINT8 data)
{
	buckrog_fchg = data & 0x07;
	ZetClose();
	ZetOpen(1);
	ZetSetIRQLine(0, (data & 0x80) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
	ZetClose();
	ZetOpen(0);
}

static void buckrog_update_samples()
{
	if (BurnSampleGetStatus(8))
	{
//		sample_set_freq(5, sample_get_base_freq(5) * (state->buckrog_myship / 100.25 + 1));	
	}
}

static void buckrog_ppi1a_write(UINT8 data)
{
	UINT8 diff = data ^ sound_data[0];
	sound_data[0] = data;

	//if ((diff & 0x10) && (data & 0x10))
	//	sample_set_volume(3, (float)(/*7 - */(data & 7)) / 7.0f);

	if ((diff & 0x20) && (data & 0x20))
	{
	//	buckrog_myship = data & 0x0f;
		buckrog_update_samples();
	}

	if ((diff & 0x40) && !(data & 0x40)) BurnSamplePlay(0);
	if ((diff & 0x80) && !(data & 0x80)) BurnSamplePlay(1);
}

static void buckrog_ppi1b_write(UINT8 data)
{
	UINT8 diff = data ^ sound_data[1];
	sound_data[1] = data;

	if ((diff & 0x01) && !(data & 0x01)) BurnSamplePlay(2);
	if ((diff & 0x02) && !(data & 0x02)) BurnSamplePlay(3);
	if ((diff & 0x04) && !(data & 0x04)) BurnSamplePlay(5);
	if ((diff & 0x08) && !(data & 0x08)) BurnSamplePlay(4);
	
	if ((diff & 0x10) && !(data & 0x10))
	{
		BurnSamplePlay(7);
		buckrog_update_samples();
	}

	if ((diff & 0x20) && !(data & 0x20)) BurnSamplePlay(6);
	if ((diff & 0x40) &&  (data & 0x40) && !BurnSampleGetStatus(8))
	{
		BurnSamplePlay(8);
		buckrog_update_samples();
	}
	if ((diff & 0x40) && !(data & 0x40) && BurnSampleGetStatus(8)) BurnSampleStop(8);

//	sound_global_enable(data & 0x80);
}

static void buckrog_ppi1c_write(UINT8 data)
{
	// coin counter = data & 0x30
	// lamp = data & 0x40
	buckrog_obch = data & 7;
}

static tilemap_callback( fg )
{
	INT32 code = DrvVidRAM[offs];

	TILE_SET_INFO(0, code, code >> 2, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();
	
	ppi8255_reset();

	BurnSampleReset();
	
	memset (turbo_op, 0, 3);
	memset (turbo_ip, 0, 3);
	turbo_fbpla = 0;
	turbo_fbcol = 0;
	turbo_last_analog = 0;
	turbo_collision = 0;

	turbo_bsel = 0;
	turbo_accel = 0;
	memset (sound_data, 0, 3);

	subroc3d_ply = 0;
	subroc3d_flip = 0;
	subroc3d_col = 0;

	buckrog_command = 0;
	buckrog_mov = 0;
	buckrog_fchg = 0;
	buckrog_obch = 0;

	DrvDial = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x00a000;
	DrvZ80Dec		= Next; Next += 0x00a000;
	DrvSubROM		= Next; Next += 0x002000;

	DrvSprROM		= Next; Next += 0x040000;
	DrvFgROM		= Next; Next += 0x004000;
	DrvRoadROM		= Next; Next += 0x008000;
	DrvBgColor		= Next; Next += 0x002000;

	DrvColPROM		= Next; Next += 0x001020;

	DrvBitmap		= (UINT16*)Next; Next += 256 * 256 * 2;

	DrvPalette		= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000400;
	DrvSprPOS		= Next; Next += 0x000400;
	DrvScrRAM		= Next; Next += 0x000800;
	DrvSubRAM		= Next; Next += 0x000800;
	DrvBmpRAM		= Next; Next += 0x00e000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void turbo_rom_decode()
{
	static const UINT8 xortable[4][32] = {
		{ 0x00,0x44,0x0c,0x48,0x00,0x44,0x0c,0x48,0xa0,0xe4,0xac,0xe8,0xa0,0xe4,0xac,0xe8,
		  0x60,0x24,0x6c,0x28,0x60,0x24,0x6c,0x28,0xc0,0x84,0xcc,0x88,0xc0,0x84,0xcc,0x88 },
		{ 0x00,0x44,0x18,0x5c,0x14,0x50,0x0c,0x48,0x28,0x6c,0x30,0x74,0x3c,0x78,0x24,0x60,
		  0x60,0x24,0x78,0x3c,0x74,0x30,0x6c,0x28,0x48,0x0c,0x50,0x14,0x5c,0x18,0x44,0x00 },
		{ 0x00,0x00,0x28,0x28,0x90,0x90,0xb8,0xb8,0x28,0x28,0x00,0x00,0xb8,0xb8,0x90,0x90,
		  0x00,0x00,0x28,0x28,0x90,0x90,0xb8,0xb8,0x28,0x28,0x00,0x00,0xb8,0xb8,0x90,0x90 },
		{ 0x00,0x14,0x88,0x9c,0x30,0x24,0xb8,0xac,0x24,0x30,0xac,0xb8,0x14,0x00,0x9c,0x88,
		  0x48,0x5c,0xc0,0xd4,0x78,0x6c,0xf0,0xe4,0x6c,0x78,0xe4,0xf0,0x5c,0x48,0xd4,0xc0 }
	};

	static const INT32 findtable[4*6] = {
		0,1,0,1,2,1,2,1,3,1,3,1,3,1,3,1,0,1,0,1,2,1,2,1
	};

	for (INT32 offs = 0; offs < 0x6000; offs++)
	{
		UINT8 src = DrvZ80ROM[offs];
		INT32 j = src >> 2;
		if (src & 0x80) j ^= 0x3f;
		DrvZ80ROM[offs] ^= xortable[findtable[offs >> 10]][j];
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { 0x800*8, 0 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x1000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvFgROM, 0x1000);

	GfxDecode(0x0100, 2, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvFgROM);

	BurnFree(tmp);

	return 0;
}

static INT32 TurboInit(INT32 encrypted)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if ((BurnDrvGetFlags() & BDF_BOOTLEG) == 0)
	{
		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x04000,  2, 1)) return 1;

		if (BurnLoadRom(DrvSprROM   + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x02000,  3, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x04000,  4, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x06000,  4, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x08000,  5, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x0a000,  5, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x0c000,  6, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x0e000,  7, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x10000,  8, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x12000,  9, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x14000, 10, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x16000, 11, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x18000, 12, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x1a000, 13, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x1c000, 14, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x1e000, 15, 1)) return 1;

		if (BurnLoadRom(DrvFgROM    + 0x00000, 16, 1)) return 1;
		if (BurnLoadRom(DrvFgROM    + 0x00800, 17, 1)) return 1;

		if (BurnLoadRom(DrvRoadROM  + 0x00000, 18, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM  + 0x00800, 19, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM  + 0x01000, 20, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM  + 0x01800, 21, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM  + 0x02000, 22, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM  + 0x02800, 23, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM  + 0x03000, 24, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM  + 0x03800, 25, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM  + 0x04000, 26, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 27, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 28, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00040, 29, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00060, 30, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00100, 31, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00200, 32, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00400, 33, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00600, 34, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00800, 35, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00c00, 36, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x01000, 37, 1)) return 1;

		if (encrypted) turbo_rom_decode();
		DrvGfxDecode();
	}
	else
	{
		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x04000,  2, 1)) return 1;

		if (BurnLoadRom(DrvSprROM   + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x02000,  3, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x04000,  4, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x06000,  4, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x08000,  5, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x0a000,  5, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x0c000,  6, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x0e000,  7, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x10000,  8, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x12000,  9, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x14000, 10, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x16000, 11, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x18000, 12, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x1a000, 13, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x1c000, 14, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x1e000, 15, 1)) return 1;

		if (BurnLoadRom(DrvFgROM    + 0x00000, 16, 1)) return 1;
		if (BurnLoadRom(DrvFgROM    + 0x00800, 17, 1)) return 1;

		if (BurnLoadRom(DrvRoadROM  + 0x00000, 18, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM  + 0x01000, 19, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM  + 0x02000, 20, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM  + 0x03000, 21, 1)) return 1;
		if (BurnLoadRom(DrvRoadROM  + 0x04000, 22, 1)) return 1;
		
		// unk prom position 23

		if (BurnLoadRom(DrvColPROM  + 0x00000, 24, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 25, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00040, 26, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00060, 27, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00100, 28, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00200, 29, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00400, 30, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00600, 31, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00800, 32, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00c00, 33, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x01000, 34, 1)) return 1;
		
		DrvGfxDecode();		
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvSprPOS,			0xb000, 0xb3ff, MAP_RAM);
	ZetMapMemory(DrvSprPOS,			0xb400, 0xb7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,			0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(turbo_write);
	ZetSetReadHandler(turbo_read);
	ZetClose();
	
	ZetInit(1); // so we can share reset w/buckrog

	ppi8255_init(4);
	ppi8255_set_write_ports(0, turbo_ppi0a_write, turbo_ppi0b_write, turbo_ppi0c_write);
	ppi8255_set_write_ports(1, turbo_ppi1a_write, turbo_ppi1b_write, turbo_ppi1c_write);
	ppi8255_set_write_ports(2, turbo_ppi2a_write, turbo_ppi2b_write, turbo_ppi2c_write);
	ppi8255_set_write_ports(3, NULL, NULL, turbo_ppi3c_write);
	ppi8255_set_read_ports(3, turbo_ppi3a_read, turbo_ppi3b_read, NULL);

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvFgROM, 2, 8, 8, 0x4000, 0, 0x3f);

	DrvDoReset();

	return 0;
}

static INT32 TurboEncInit() { return TurboInit(1); }
static INT32 TurboDecInit() { return TurboInit(0); }

static INT32 Subroc3dInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x04000,  2, 1)) return 1;

		if (BurnLoadRom(DrvSprROM   + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x02000,  4, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x08000,  5, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x0a000,  6, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x10000,  7, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x12000,  8, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x16000,  9, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x18000, 10, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x1a000, 11, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x1e000, 12, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x20000, 13, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x22000, 14, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x26000, 15, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x28000, 16, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x2a000, 17, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x2e000, 18, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x30000, 19, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x32000, 20, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x34000, 21, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x36000, 22, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x38000, 23, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x3a000, 24, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x3c000, 25, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x3e000, 26, 1)) return 1;

		if (BurnLoadRom(DrvFgROM    + 0x00000, 27, 1)) return 1;
		if (BurnLoadRom(DrvFgROM    + 0x00800, 28, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 29, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00200, 30, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00300, 31, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00500, 32, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00700, 33, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00900, 34, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00920, 35, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvSprPOS,			0xa000, 0xa3ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xa400, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,			0xb000, 0xb7ff, MAP_RAM);
	ZetMapMemory(DrvScrRAM,			0xb800, 0xbfff, MAP_RAM); // map(0xb800, 0xbfff);  b800-bfff HANDLE CL ?? wtf
	ZetMapMemory(DrvVidRAM,			0xe000, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(subroc3d_write);
	ZetSetReadHandler(subroc3d_read);
	ZetClose();

	ZetInit(1); // so we can share reset w/buckrog
	
	ppi8255_init(2);
	ppi8255_set_write_ports(0, subroc3d_ppi0a_write, subroc3d_ppi0b_write, subroc3d_ppi0c_write);
	ppi8255_set_write_ports(1, subroc3d_ppi1a_write, subroc3d_ppi1b_write, subroc3d_ppi1c_write);

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvFgROM, 2, 8, 8, 0x4000, 0, 0x3f);

	DrvDoReset();

	return 0;
}

static void sega_decode(UINT8 *rom, UINT8 *decrypted, INT32 len, const UINT8 convtable[32][4])
{
	for (INT32 A = 0x0000; A < 0x8000;A++)
	{
		int xor1 = 0;

		UINT8 src = rom[A];

		/* pick the translation table from bits 0, 4, 8 and 12 of the address */
		int row = (A & 1) + (((A >> 4) & 1) << 1) + (((A >> 8) & 1) << 2) + (((A >> 12) & 1) << 3);

		/* pick the offset in the table from bits 3 and 5 of the source data */
		int col = ((src >> 3) & 1) + (((src >> 5) & 1) << 1);
		/* the bottom half of the translation table is the mirror image of the top */
		if (src & 0x80)
		{
			col = 3 - col;
			xor1 = 0xa8;
		}

		/* decode the opcodes */
		decrypted[A] = (src & ~0xa8) | (convtable[2*row][col] ^ xor1);

		/* decode the data */
		rom[A] = (src & ~0xa8) | (convtable[2*row+1][col] ^ xor1);

		if (convtable[2*row][col] == 0xff)	/* table incomplete! (for development) */
			decrypted[A] = 0x00;
		if (convtable[2*row+1][col] == 0xff)	/* table incomplete! (for development) */
			rom[A] = 0xee;
	}

	if (len > 0x8000) memcpy(&decrypted[0x8000], &rom[0x8000], len - 0x8000);
}

static void buckrog_decode(UINT8 *rom, UINT8 *dec, INT32 len)
{
	static const unsigned char convtable[32][4] = {
		{ 0x80,0x00,0x88,0x08 }, { 0x28,0x20,0xa8,0xa0 },	/* ...0...0...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0xa0,0x80,0x20,0x00 },	/* ...0...0...0...1 */
		{ 0x28,0xa8,0x08,0x88 }, { 0xa8,0xa0,0x88,0x80 },	/* ...0...0...1...0 */
		{ 0x80,0x00,0x88,0x08 }, { 0x28,0x20,0xa8,0xa0 },	/* ...0...0...1...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0xa0,0x80,0x20,0x00 },	/* ...0...1...0...0 */
		{ 0x80,0x00,0x88,0x08 }, { 0x28,0x20,0xa8,0xa0 },	/* ...0...1...0...1 */
		{ 0x28,0xa8,0x08,0x88 }, { 0xa8,0xa0,0x88,0x80 },	/* ...0...1...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0xa0,0x80,0x20,0x00 },	/* ...0...1...1...1 */
		{ 0x28,0xa8,0x08,0x88 }, { 0xa8,0xa0,0x88,0x80 },	/* ...1...0...0...0 */
		{ 0x80,0x00,0x88,0x08 }, { 0x28,0x20,0xa8,0xa0 },	/* ...1...0...0...1 */
		{ 0x80,0x00,0x88,0x08 }, { 0x28,0x20,0xa8,0xa0 },	/* ...1...0...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...0...1...1 */
		{ 0x80,0x00,0x88,0x08 }, { 0x28,0x20,0xa8,0xa0 },	/* ...1...1...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...1...0...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...1...1...0 */
		{ 0x28,0xa8,0x08,0x88 }, { 0xa8,0xa0,0x88,0x80 }	/* ...1...1...1...1 */
	};

	sega_decode(rom, dec, len, convtable);
}

static INT32 BuckrogInit(INT32 encrypted)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x04000,  1, 1)) return 1;

		if (BurnLoadRom(DrvSubROM   + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvSprROM   + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x08000,  4, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x10000,  5, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x18000,  6, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x20000,  7, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x28000,  8, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x2c000,  9, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x30000, 10, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x34000, 11, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x38000, 12, 1)) return 1;
		if (BurnLoadRom(DrvSprROM   + 0x3c000, 13, 1)) return 1;

		if (BurnLoadRom(DrvFgROM    + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvFgROM    + 0x00800, 15, 1)) return 1;

		if (BurnLoadRom(DrvBgColor  + 0x00000, 16, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 17, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 18, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00100, 19, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00300, 20, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00500, 21, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00700, 22, 1)) return 1;

		if (encrypted) {
			buckrog_decode(DrvZ80ROM, DrvZ80Dec, 0x8000);
		} else {
			memcpy (DrvZ80Dec, DrvZ80ROM, 0x8000);
		}
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80Dec,			0x0000, 0x7fff, MAP_FETCHOP);
	ZetMapMemory(DrvVidRAM,			0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvSprPOS,			0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xe400, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,			0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(buckrog_write);
	ZetSetReadHandler(buckrog_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvSubROM,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvSubRAM,			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvSubRAM,			0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSubRAM,			0xf000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvSubRAM,			0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(buckrog_sub_write);
	ZetSetInHandler(buckrog_sub_read_port);
	ZetClose();

	ppi8255_init(2);
	ppi8255_set_write_ports(0, buckrog_ppi0a_write, buckrog_ppi0b_write, buckrog_ppi0c_write);
	ppi8255_set_write_ports(1, buckrog_ppi1a_write, buckrog_ppi1b_write, buckrog_ppi1c_write);

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvFgROM, 2, 8, 8, 0x4000, 0, 0x3f);

	DrvDoReset();

	return 0;
}

static INT32 BuckrogEncInit()
{
	return BuckrogInit(1);
}

static INT32 BuckrogDecInit()
{
	return BuckrogInit(0);
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	ppi8255_exit();
	BurnSampleExit();

	BurnFree(AllMem);

	return 0;
}

static void TurboPaletteInit()
{
	for (INT32 i = 0; i < 256; i++)
	{
		INT32 bit0 = (i >> 0) & 1;
		INT32 bit1 = (i >> 1) & 1;
		INT32 bit2 = (i >> 2) & 1;
		INT32 r = (((bit0 * 220) + (bit1 * 470) + (bit2 * 1000)) * 255) / 1690;

		bit0 = (i >> 3) & 1;
		bit1 = (i >> 4) & 1;
		bit2 = (i >> 5) & 1;
		INT32 g = (((bit0 * 220) + (bit1 * 470) + (bit2 * 1000)) * 255) / 1690;

		bit0 = (i >> 6) & 1;
		bit1 = (i >> 7) & 1;
		INT32 b = (((bit0 * 220) + (bit1 * 470)) * 255) / 690;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}


#define TURBO_X_SCALE       2

struct sprite_info
{
	UINT16  ve;                 /* VE0-15 signals for this row */
	UINT8   lst;                /* LST0-7 signals for this row */
	UINT32  latched[8];         /* latched pixel data */
	UINT8   plb[8];             /* latched PLB state */
	UINT32  offset[8];          /* current offset for this row */
	UINT32  frac[8];            /* leftover fraction */
	UINT32  step[8];            /* stepping value */
};

static const UINT32 sprite_expand[16] =
{
	0x00000000, 0x00000001, 0x00000100, 0x00000101,
	0x00010000, 0x00010001, 0x00010100, 0x00010101,
	0x01000000, 0x01000001, 0x01000100, 0x01000101,
	0x01010000, 0x01010001, 0x01010100, 0x01010101
};


static UINT32 sprite_xscale(UINT8 dacinput, double vr1, double vr2, double cext)
{
	double iref = 5.0 / (1.5e3 + vr2);
	double iout = iref * ((double)dacinput / 256.0);
	double vref = 5.0 * 1e3 / (3.8e3 + 1e3 + vr1);
	double vco_cv = (2.2e3 * iout) + vref;

	double vco_freq;
	if (vco_cv > 5.0)
		vco_cv = 5.0;
	if (vco_cv < 0.0)
		vco_cv = 0.0;
	if (cext < 1e-11)
	{
		if (vco_cv < 1.33)
			vco_freq = (0.68129 + pow(vco_cv + 0.6, 1.285)) * 1e6;
		else if (vco_cv < 4.3)
			vco_freq = (3 + (8 - 3) * ((vco_cv - 1.33) / (4.3 - 1.33))) * 1e6;
		else
			vco_freq = (-1.560279 + pow(vco_cv - 4.3 + 6, 1.26)) * 1e6;

		vco_freq *= 50e-12 / cext;
	}
	else
	{
		vco_freq = -0.9892942 * log10(cext) - 0.0309697 * vco_cv * vco_cv
						+   0.344079975 * vco_cv - 4.086395841;
		vco_freq = pow(10.0, vco_freq);
	}

	return (UINT32)((vco_freq / (5e6 * TURBO_X_SCALE)) * 16777216.0);
}

static void turbo_prepare_sprites(UINT8 y, sprite_info *info)
{
	const UINT8 *pr1119 = &DrvColPROM[0x200];
	int sprnum;

	/* initialize the line enable signals to 0 */
	info->ve = 0;
	info->lst = 0;

	INT32 vr1 = 31; // pots! iq_132
	INT32 vr2 = 91; // pot!

	/* compute the sprite information, which was done on the previous scanline during HBLANK */
	for (sprnum = 0; sprnum < 16; sprnum++)
	{
		UINT8 *rambase = &DrvSprRAM[sprnum * 8];
		int level = sprnum & 7;
		UINT8 clo, chi;
		UINT32 sum;

		/* perform the first ALU to see if we are within the scanline */
		sum = y + (rambase[0] ^ 0xff);
		clo = (sum >> 8) & 1;
		sum += (y << 8) + ((rambase[1] ^ 0xff) << 8);
		chi = (sum >> 16) & 1;

		/* the AND of the low carry and the inverse of the high carry clocks an enable bit */
		/* for this sprite; note that the logic in the Turbo schematics is reversed here */
		if (clo & (chi ^ 1))
		{
			int xscale = rambase[2] ^ 0xff;
			int yscale = rambase[3];// ^ 0xff;
			UINT16 offset = rambase[6] + (rambase[7] << 8);
			int offs;

			/* mark this entry enabled */
			info->ve |= 1 << sprnum;

			/* look up the low byte of the sum plus the yscale value in */
			/* IC50/PR1119 to determine if we write back the sum of the */
			/* offset and the rowbytes this scanline (p. 138) */
			offs = (sum & 0xff) |           /* A0-A7 = AL0-AL7 */
					((yscale & 0x08) << 5); /* A8-A9 = /RO11-/RO12 */

			/* one of the bits is selected based on the low 7 bits of yscale */
			if (!((pr1119[offs] >> (yscale & 0x07)) & 1))
			{
				offset += rambase[4] + (rambase[5] << 8);
				rambase[6] = offset;
				rambase[7] = offset >> 8;
			}

			/* the output of the ALU here goes to the individual level counter */
			info->latched[level] = 0;
			info->plb[level] = 0;
			info->offset[level] = offset;
			info->frac[level] = 0;

			info->step[level] = sprite_xscale(xscale, 1.0e3 * vr1 / 100.0, 1.0e3 * vr2 / 100.0, 100e-12);
		}
	}
}


static UINT32 turbo_get_sprite_bits(UINT8 road, sprite_info *sprinfo)
{
	UINT8 sprlive = sprinfo->lst;
	UINT32 sprdata = 0;
	int level;

	/* if we haven't left the road yet, prites 3-7 are disabled */
	if (!road)
		sprlive &= 0x07;

	/* loop over all live levels */
	for (level = 0; level < 8; level++)
		if (sprlive & (1 << level))
		{
			/* latch the data and advance the offset */
			sprdata |= sprinfo->latched[level];
			sprinfo->frac[level] += sprinfo->step[level];

			/* if we're live and we've clocked more data, advance */
			while (sprinfo->frac[level] >= 0x1000000)
			{
				UINT16 offs = sprinfo->offset[level];
				UINT8 pixdata;

				/* bit 0 controls which half of the byte to use */
				/* bits 1-13 go to address lines */
				/* bit 14 selects which of the two ROMs to read from */
				pixdata = DrvSprROM[(level << 14) | ((offs >> 1) & 0x3fff)] >> ((~offs & 1) * 4);
				sprinfo->latched[level] = sprite_expand[pixdata & 0x0f] << level;

				/* if bit 3 is 0 and bit 2 is 1, the enable flip/flip is reset */
				if ((pixdata & 0x0c) == 0x04)
				{
					sprinfo->lst &= ~(1 << level);
					sprlive &= ~(1 << level);
				}

				/* if bit 15 is set, we decrement instead of increment */
				sprinfo->offset[level] += (offs & 0x8000) ? -1 : 1;
				sprinfo->frac[level] -= 0x1000000;
			}
		}

	return sprdata;
}

static void screen_update_turbo()
{
	const UINT8 *pr1114 = &DrvColPROM[0x000];
	const UINT8 *pr1115 = &DrvColPROM[0x020];
	const UINT8 *pr1116 = &DrvColPROM[0x040];
	const UINT8 *pr1117 = &DrvColPROM[0x060];
	const UINT8 *pr1118 = &DrvColPROM[0x100];
	const UINT8 *pr1121 = &DrvColPROM[0x600];
	const UINT8 *pr1122 = &DrvColPROM[0x800];
	const UINT8 *pr1123 = &DrvColPROM[0xc00];
	int x, y;
	
	/* loop over rows */
	for (y = 0; y < nScreenHeight; y++)
	{
		const UINT16 *fore = DrvBitmap + y * 256;
		UINT16 *dest = pTransDraw + y * nScreenWidth;
		int sel, coch, babit, slipar_acciar, area, offs, areatmp, road = 0;
		sprite_info sprinfo;

		/* compute the Y sum between opa and the current scanline (p. 141) */
		int va = (y + turbo_op[0]) & 0xff;

		/* the upper bit of OPC inverts the road (p. 141) */
		if (!(turbo_op[2] & 0x80))
			va ^= 0xff;

		/* compute the sprite information; we use y-1 since this info was computed during HBLANK */
		/* on the previous scanline */
		turbo_prepare_sprites(y, &sprinfo);

		/* loop over columns */
		for (x = 0; x < nScreenWidth; x += TURBO_X_SCALE)
		{
			int bacol, red, grn, blu, priority, foreraw, forebits, mx, ix;
			int xx = x / TURBO_X_SCALE;
			UINT8 carry;
			UINT32 sprbits;
			UINT16 he;

			/* load the bitmask from the sprite position for both halves of the sprites (p. 139) */
			he = DrvSprPOS[xx] | (DrvSprPOS[xx + 0x100] << 8);

			/* the AND of the line enable and horizontal enable is clocked and held in LST0-7 (p. 143) */
			he &= sprinfo.ve;
			sprinfo.lst |= he | (he >> 8);

			/* compute the X sum between opb and the current column; only the carry matters (p. 141) */
			carry = (xx + turbo_op[1]) >> 8;

			/* the carry selects which inputs to use (p. 141) */
			if (carry)
			{
				sel  = turbo_ip[1];
				coch = turbo_ip[2] >> 4;
			}
			else
			{
				sel  = turbo_ip[0];
				coch = turbo_ip[2] & 15;
			}

			/* look up AREA1 and AREA2 (p. 142) */
			offs = va |                         /*  A0- A7 = VA0-VA7 */
					((sel & 0x0f) << 8);            /*  A8-A11 = SEL0-3 */

			areatmp = DrvRoadROM[0x0000 | offs];
			areatmp = ((areatmp + xx) >> 8) & 0x01;
			area = areatmp << 0;

			areatmp = DrvRoadROM[0x1000 | offs];
			areatmp = ((areatmp + xx) >> 8) & 0x01;
			area |= areatmp << 1;

			/* look up AREA3 and AREA4 (p. 142) */
			offs = va |                         /*  A0- A7 = VA0-VA7 */
					((sel & 0xf0) << 4);            /*  A8-A11 = SEL4-7 */

			areatmp = DrvRoadROM[0x2000 | offs];
			areatmp = ((areatmp + xx) >> 8) & 0x01;
			area |= areatmp << 2;

			areatmp = DrvRoadROM[0x3000 | offs];
			areatmp = ((areatmp + xx) >> 8) & 0x01;
			area |= areatmp << 3;

			/* look up AREA5 (p. 141) */
			offs = (xx >> 3) |                          /*  A0- A4 = H3-H7 */
					((turbo_op[2] & 0x3f) << 5);    /*  A5-A10 = OPC0-5 */

			areatmp = DrvRoadROM[0x4000 | offs];
			areatmp = (areatmp << (xx & 7)) & 0x80;
			area |= areatmp >> 3;

			/* compute the final area value and look it up in IC18/PR1115 (p. 144) */
			/* note: SLIPAR is 0 on the road surface only */
			/*       ACCIAR is 0 on the road surface and the striped edges only */
			babit = pr1115[area];
			slipar_acciar = babit & 0x30;
			if (!road && (slipar_acciar & 0x20))
				road = 1;

			/* also use the coch value to look up color info in IC13/PR1114 and IC21/PR1117 (p. 144) */
			offs = (coch & 0x0f) |                      /* A0-A3: CONT0-3 = COCH0-3 */
					((turbo_fbcol & 0x01) << 4);  /*    A4: COL0 */
			bacol = pr1114[offs] | (pr1117[offs] << 8);

			/* at this point, do the character lookup; due to the shift register loading in */
			/* the sync PROM, we latch character 0 during pixel 6 and start clocking in pixel */
			/* 8, effectively shifting the display by 8; at pixel 0x108, the color latch is */
			/* forced clear and isn't touched until the next shift register load */
			foreraw = (xx < 8 || xx >= 0x108) ? 0 : fore[xx - 8];

			/* perform the foreground color table lookup in IC99/PR1118 (p. 137) */
			forebits = pr1118[foreraw];

			/* now that we have done all the per-5MHz pixel work, mix the Sprites at the scale factor */
			for (ix = 0; ix < TURBO_X_SCALE; ix++)
			{
				/* iterate over live Sprites and update them */
				/* the final 32-bit value is: */
				/*    CDB0-7 = D0 -D7  */
				/*    CDG0-7 = D8 -D15 */
				/*    CDR0-7 = D16-D23 */
				/*    PLB0-7 = D24-D31 */
				sprbits = turbo_get_sprite_bits(road, &sprinfo);

				/* perform collision detection here via lookup in IC20/PR1116 (p. 144) */
				turbo_collision |= pr1116[((sprbits >> 24) & 7) | (slipar_acciar >> 1)];

				/* look up the sprite priority in IC11/PR1122 (p. 144) */
				priority = ((sprbits & 0xfe000000) >> 25) |     /* A0-A6: PLB1-7 */
							((turbo_fbpla & 0x07) << 7);  /* A7-A9: PLA0-2 */
				priority = pr1122[priority];

				/* use that to look up the overall priority in IC12/PR1123 (p. 144) */
				mx = (priority & 7) |                       /* A0-A2: PR-1122 output, bits 0-2 */
						((sprbits & 0x01000000) >> 21) |        /*    A3: PLB0 */
						((foreraw & 0x80) >> 3) |               /*    A4: PLBE */
						((forebits & 0x08) << 2) |          /*    A5: PLBF */
						((babit & 0x07) << 6) |             /* A6-A8: BABIT1-3 */
						((turbo_fbpla & 0x08) << 6);  /*    A9: PLA3 */
				mx = pr1123[mx];

				/* the MX output selects one of 16 inputs; build up a 16-bit pattern to match */
				/* these in red, green, and blue (p. 144) */
				red = ((sprbits & 0x0000ff) >> 0) |     /*  D0- D7: CDR0-CDR7 */
						((forebits & 0x01) << 8) |      /*      D8: CDRF */
						((bacol & 0x001f) << 9) |           /*  D9-D13: BAR0-BAR4 */
						(1 << 14) |                     /*     D14: 1 */
						(0 << 15);                      /*     D15: 0 */

				grn = ((sprbits & 0x00ff00) >> 8) |     /*  D0- D7: CDG0-CDG7 */
						((forebits & 0x02) << 7) |      /*      D8: CDGF */
						((bacol & 0x03e0) << 4) |           /*  D9-D13: BAG0-BAG4 */
						(1 << 14) |                     /*     D14: 1 */
						(0 << 15);                      /*     D15: 0 */

				blu = ((sprbits & 0xff0000) >> 16) |    /*  D0- D7: CDB0-CDB7 */
						((forebits & 0x04) << 6) |      /*      D8: CDBF */
						((bacol & 0x7c00) >> 1) |           /*  D9-D13: BAB0-BAB4 */
						(1 << 14) |                     /*     D14: 1 */
						(0 << 15);                      /*     D15: 0 */

				/* we then go through a muxer to select one of the 16 outputs computed above (p. 144) */
				offs = mx |                             /* A0-A3: MX0-MX3 */
						(((~red >> mx) & 1) << 4) |     /*    A4: CDR */
						(((~grn >> mx) & 1) << 5) |     /*    A5: CDG */
						(((~blu >> mx) & 1) << 6) |     /*    A6: CDB */
						((turbo_fbcol & 6) << 6); /* A7-A8: COL1-2 */
				dest[x + ix] = pr1121[offs];
			}
		}
	}
}

static INT32 TurboDraw()
{
	if (DrvRecalc) {
		TurboPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilesSetClipRaw(0, 256, 0, 256);
	GenericTilemapDraw(0, DrvBitmap, 0);
	GenericTilesClearClipRaw();

	screen_update_turbo();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void subroc3d_prepare_sprites(UINT8 y, sprite_info *info)
{
	const UINT8 *pr1449 = &DrvColPROM[0x300];
	int sprnum;

	/* initialize the line enable signals to 0 */
	info->ve = 0;
	info->lst = 0;

	/* compute the sprite information, which was done on the previous scanline during HBLANK */
	for (sprnum = 0; sprnum < 16; sprnum++)
	{
		UINT8 *rambase = &DrvSprRAM[sprnum * 8];
		int level = sprnum & 7;
		UINT8 clo, chi;
		UINT32 sum;

		/* perform the first ALU to see if we are within the scanline */
		sum = y + (rambase[0]/* ^ 0xff*/);
		clo = (sum >> 8) & 1;
		sum += (y << 8) + ((rambase[1]/* ^ 0xff*/) << 8);
		chi = (sum >> 16) & 1;

		/* the AND of the low carry and the inverse of the high carry clocks an enable bit */
		/* for this sprite; note that the logic in the Turbo schematics is reversed here */
		if (clo & (chi ^ 1))
		{
			int xscale = rambase[2] ^ 0xff;
			int yscale = rambase[3];// ^ 0xff;
			UINT16 offset = rambase[6] + (rambase[7] << 8);
			int offs;

			/* mark this entry enabled */
			info->ve |= 1 << sprnum;

			/* look up the low byte of the sum plus the yscale value in */
			/* IC50/PR1119 to determine if we write back the sum of the */
			/* offset and the rowbytes this scanline (p. 138) */
			offs = (sum & 0xff) |           /* A0-A7 = AL0-AL7 */
					((yscale & 0x08) << 5); /* A8-A9 = /RO11-/RO12 */

			/* one of the bits is selected based on the low 7 bits of yscale */
			if (!((pr1449[offs] >> (yscale & 0x07)) & 1))
			{
				offset += rambase[4] + (rambase[5] << 8);
				rambase[6] = offset;
				rambase[7] = offset >> 8;
			}

			/* the output of the ALU here goes to the individual level counter */
			info->latched[level] = 0;
			info->plb[level] = 0;
			info->offset[level] = offset << 1;
			info->frac[level] = 0;
			info->step[level] = sprite_xscale(xscale, 1.2e3, 1.2e3, 220e-12);
		}
	}
}

static UINT32 subroc3d_get_sprite_bits(sprite_info *sprinfo, UINT8 *plb)
{
	/* see logic on each sprite:
	    END = (CDA == 1 && (CDA ^ CDB) == 0 && (CDC ^ CDD) == 0)
	    PLB = END ^ (CDA == 1 && (CDC ^ CDD) == 0)
	   end is in bit 1, plb in bit 0
	*/
	static const UINT8 plb_end[16] = { 0,1,1,2, 1,1,1,1, 1,1,1,1, 0,1,1,2 };
	UINT32 sprdata = 0;
	int level;

	*plb = 0;

	/* loop over all live levels */
	for (level = 0; level < 8; level++)
		if (sprinfo->lst & (1 << level))
		{
			/* latch the data and advance the offset */
			sprdata |= sprinfo->latched[level];
			*plb |= sprinfo->plb[level];
			sprinfo->frac[level] += sprinfo->step[level];

			/* if we're live and we've clocked more data, advance */
			while (sprinfo->frac[level] >= 0x800000)
			{
				UINT32 offs = sprinfo->offset[level];
				UINT8 pixdata;

				/* bit 0 controls which half of the byte to use */
				/* bits 1-13 go to address lines */
				/* bit 14 selects which of the two ROMs to read from */
				pixdata = DrvSprROM[(level << 15) | ((offs >> 1) & 0x7fff)] >> ((~offs & 1) * 4);
				sprinfo->latched[level] = sprite_expand[pixdata & 0x0f] << level;
				sprinfo->plb[level] = (plb_end[pixdata & 0x0f] & 1) << level;

				/* if bit 3 is 0 and bit 2 is 1, the enable flip/flip is reset */
				if (plb_end[pixdata & 0x0f] & 2)
					sprinfo->lst &= ~(1 << level);

				/* if bit 15 is set, we decrement instead of increment */
				sprinfo->offset[level] += (offs & 0x10000) ? -1 : 1;
				sprinfo->frac[level] -= 0x800000;
			}
		}

	return sprdata;
}

static void screen_update_subroc3d()
{
	const UINT8 *pr1419 = &DrvColPROM[0x000];
	const UINT8 *pr1620 = &DrvColPROM[0x200];
	const UINT8 *pr1450 = &DrvColPROM[0x500];
	const UINT8 *pr1454 = &DrvColPROM[0x920];
	int x, y;

	for (y = 0; y < nScreenHeight; y++)
	{
		const UINT16 *fore = DrvBitmap + y * 256;
		UINT16 *dest = pTransDraw + y * nScreenWidth;
		sprite_info sprinfo;

		subroc3d_prepare_sprites(y, &sprinfo);

		for (x = 0; x < nScreenWidth; x += TURBO_X_SCALE)
		{
			int offs, finalbits, ix;
			UINT8 xx = x / TURBO_X_SCALE;
			UINT8 foreraw, forebits, mux, cd, plb, mplb;
			UINT16 he;
			UINT32 sprbits;

			he = DrvSprPOS[xx * 2] | (DrvSprPOS[xx * 2 + 1] << 8);

			he &= sprinfo.ve;
			sprinfo.lst |= he | (he >> 8);

			if (!subroc3d_flip)
				foreraw = fore[xx];
			else
				foreraw = fore[(pr1454[(xx >> 3) & 0x1f] << 3) | (xx & 0x07)];

			forebits = pr1620[foreraw];

			mplb = (foreraw & 0x80) || ((forebits & 0x0f) == 0);

			for (ix = 0; ix < TURBO_X_SCALE; ix++)
			{
				sprbits = subroc3d_get_sprite_bits(&sprinfo, &plb);

				if (mplb)
				{
					offs = (plb ^ 0xff) |                       /* A0-A7: /PLB0-7 */
							((subroc3d_ply & 0x02) << 7); /*    A8: PLY1 */
					mux = pr1450[offs] >> ((subroc3d_ply & 0x01) * 4);
				}
				else
					mux = 0;

				sprbits = (sprbits >> (mux & 0x07)) & 0x01010101;
				cd = (sprbits >> (24-3)) | (sprbits >> (16-2)) | (sprbits >> (8-1)) | sprbits;

				if (mux & 0x08)
					finalbits = cd;
				else
					finalbits = forebits;

				offs = (finalbits & 0x0f) |                 /* A0-A3: CD0-CD3 */
						((mux & 0x08) << 1) |               /*    A4: MUX3 */
						(subroc3d_col << 5);          /* A5-A8: COL0-COL3 */
				dest[(x + ix) ^ 0x1ff] = pr1419[offs]; // iq_132 - added screen flip
			}
		}
	}
}

static INT32 Subroc3dDraw()
{
	if (DrvRecalc) {
		TurboPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilesSetClipRaw(0, 256, 0, 256);
	GenericTilemapDraw(0, DrvBitmap, 0);
	GenericTilesClearClipRaw();

	screen_update_subroc3d();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void BuckrogPaletteInit()
{
	for (INT32 i = 0; i < 1024; i++)
	{
		INT32 bit0 = (i >> 0) & 1;
		INT32 bit1 = (i >> 1) & 1;
		INT32 bit2 = (i >> 2) & 1;
		INT32 r = (((bit0 * 250) + (bit1 * 500) + (bit2 * 1000)) * 255) / 1750;

		bit0 = (i >> 3) & 1;
		bit1 = (i >> 4) & 1;
		bit2 = (i >> 5) & 1;
		INT32 g = (((bit0 * 250) + (bit1 * 500) + (bit2 * 1000)) * 255) / 1750;

		bit0 = (i >> 8) & 1;
		bit1 = (i >> 9) & 1;
		bit2 = (i >> 6) & 1;
		INT32 bit3 = (i >> 7) & 1;
		INT32 b = (((bit0 * 250) + (bit1 * 500) + (bit2 * 1000) + (bit3 * 2200)) * 255) / 3950;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void buckrog_prepare_sprites(UINT8 y, sprite_info *info)
{
	const UINT8 *pr5196 = &DrvColPROM[0x100];
	int sprnum;

	info->ve = 0;
	info->lst = 0;

	for (sprnum = 0; sprnum < 16; sprnum++)
	{
		UINT8 *rambase = &DrvSprRAM[sprnum * 8];
		int level = sprnum & 7;
		UINT8 clo, chi;
		UINT32 sum;

		sum = y + (rambase[0]/* ^ 0xff*/);
		clo = (sum >> 8) & 1;
		sum += (y << 8) + ((rambase[1]/* ^ 0xff*/) << 8);
		chi = (sum >> 16) & 1;

		if (clo & (chi ^ 1))
		{
			int xscale = rambase[2] ^ 0xff;
			int yscale = rambase[3];// ^ 0xff;
			UINT16 offset = rambase[6] + (rambase[7] << 8);
			int offs;

			info->ve |= 1 << sprnum;

			offs = (sum & 0xff) |           /* A0-A7 = AL0-AL7 */
					((yscale & 0x08) << 5); /* A8-A9 = /RO11-/RO12 */

			if (!((pr5196[offs] >> (yscale & 0x07)) & 1))
			{
				offset += rambase[4] + (rambase[5] << 8);
				rambase[6] = offset;
				rambase[7] = offset >> 8;
			}

			info->latched[level] = 0;
			info->plb[level] = 0;
			info->offset[level] = offset << 1;
			info->frac[level] = 0;

			info->step[level] = sprite_xscale(xscale, 1.2e3, 820, 220e-12);
		}
	}
}

static UINT32 buckrog_get_sprite_bits(sprite_info *sprinfo, UINT8 *plb)
{
	static const UINT8 plb_end[16] = { 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,2 };
	UINT32 sprdata = 0;
	int level;

	*plb = 0;

	/* loop over all live levels */
	for (level = 0; level < 8; level++)
		if (sprinfo->lst & (1 << level))
		{
			/* latch the data and advance the offset */
			sprdata |= sprinfo->latched[level];
			*plb |= sprinfo->plb[level];
			sprinfo->frac[level] += sprinfo->step[level];

			/* if we're live and we've clocked more data, advance */
			while (sprinfo->frac[level] >= 0x800000)
			{
				UINT32 offs = sprinfo->offset[level];
				UINT8 pixdata;

				/* bit 0 controls which half of the byte to use */
				/* bits 1-13 go to address lines */
				/* bit 14 selects which of the two ROMs to read from */
				pixdata = DrvSprROM[(level << 15) | ((offs >> 1) & 0x7fff)] >> ((~offs & 1) * 4);
				sprinfo->latched[level] = sprite_expand[pixdata & 0x0f] << level;
				sprinfo->plb[level] = (plb_end[pixdata & 0x0f] & 1) << level;

				/* if bit 3 is 0 and bit 2 is 1, the enable flip/flip is reset */
				if (plb_end[pixdata & 0x0f] & 2)
					sprinfo->lst &= ~(1 << level);

				/* if bit 15 is set, we decrement instead of increment */
				sprinfo->offset[level] += (offs & 0x10000) ? -1 : 1;
				sprinfo->frac[level] -= 0x800000;
			}
		}

	return sprdata;
}

static void screen_update_buckrog()
{
	const UINT8 *pr5194 = &DrvColPROM[0x000];
	const UINT8 *pr5198 = &DrvColPROM[0x500];
	const UINT8 *pr5199 = &DrvColPROM[0x700];
	int x, y;

	/* loop over rows */
	for (y = 0; y < nScreenHeight; y++)
	{
		const UINT16 *fore = DrvBitmap + y * 256;
		UINT16 *dest = pTransDraw + y * nScreenWidth;
		sprite_info sprinfo;

		/* compute the sprite information; we use y-1 since this info was computed during HBLANK */
		/* on the previous scanline */
		buckrog_prepare_sprites(y, &sprinfo);

		/* loop over columns */
		for (x = 0; x < nScreenWidth; x += TURBO_X_SCALE)
		{
			UINT8 foreraw, forebits, cd, plb, star, mux;
			UINT8 xx = x / TURBO_X_SCALE;
			UINT16 he;
			UINT32 sprbits;
			int palbits, offs, ix;

			/* load the bitmask from the sprite position for both halves of the Sprites (p. 143) */
			he = DrvSprPOS[xx * 2] | (DrvSprPOS[xx * 2 + 1] << 8);

			/* the AND of the line enable and horizontal enable is clocked and held in LST0-7 (p. 143) */
			he &= sprinfo.ve;
			sprinfo.lst |= he | (he >> 8);

			/* at this point, do the character lookup and the foreground color table lookup in IC93/PR1598 (SH 5/5)*/
			foreraw = fore[(pr5194[((xx >> 3) - 1) & 0x1f] << 3) | (xx & 0x07)];
			offs = ((foreraw & 0x03) << 0) |            /* A0-A1: BIT0-1 */
					((foreraw & 0xf8) >> 1) |           /* A2-A6: BANK3-7 */
					((buckrog_fchg & 0x03) << 7); /* A7-A9: FCHG0-2 */
			forebits = pr5198[offs];

			/* fetch the STAR bit */
			star = DrvBmpRAM[y * 256 + xx];

			/* now that we have done all the per-5MHz pixel work, mix the Sprites at the scale factor */
			for (ix = 0; ix < TURBO_X_SCALE; ix++)
			{
				/* iterate over live Sprites and update them */
				/* the final 32-bit value is: */
				/*    CDA0-7 = D0 -D7  */
				/*    CDB0-7 = D8 -D15 */
				/*    CDC0-7 = D16-D23 */
				/*    CDD0-7 = D24-D31 */
				sprbits = buckrog_get_sprite_bits(&sprinfo, &plb);

				/* the PLB bits go into an LS148 8-to-1 decoder and become MUX0-3 (PROM board SH 2/10) */
				if (plb == 0)
					mux = 8;
				else
				{
					mux = 7;
					while (!(plb & 0x80))
					{
						mux--;
						plb <<= 1;
					}
				}

				/* MUX then selects one of the Sprites and selects CD0-3 */
				sprbits = (sprbits >> (mux & 0x07)) & 0x01010101;
				cd = (sprbits >> (24-3)) | (sprbits >> (16-2)) | (sprbits >> (8-1)) | sprbits;

				/* this info goes into an LS148 8-to-3 decoder to determine the priorities (SH 5/5) */

				/* priority 7 is if bit 0x80 of the foreground color is 0; CHNG = 0 */
				if (!(forebits & 0x80))
				{
					palbits = ((forebits & 0x3c) << 2) |
								((forebits & 0x06) << 1) |
								((forebits & 0x01) << 0);
				}

				/* priority 6 is if MUX3 is 0; CHNG = 1 */
				else if (!(mux & 0x08))
				{
					offs = (cd & 0x0f) |                        /* A0-A3: CD0-3 */
							((mux & 0x07) << 4) |               /* A4-A6: MUX0-2 */
							((buckrog_obch & 0x07) << 7); /* A7-A9: OBCH0-2 */
					palbits = pr5199[offs];
				}

				/* priority 3 is if bit 0x40 of the foreground color is 0; CHNG = 0 */
				else if (!(forebits & 0x40))
				{
					palbits = ((forebits & 0x3c) << 2) |
								((forebits & 0x06) << 1) |
								((forebits & 0x01) << 0);
				}

				/* priority 1 is if the star is set; CHNG = 2 */
				else if (star)
				{
					palbits = 0xff;
				}

				/* otherwise, CHNG = 3 */
				else
				{
					palbits = DrvBgColor[y | ((buckrog_mov & 0x1f) << 8)];
					palbits = (palbits & 0xc0) | ((palbits & 0x30) << 4) | ((palbits & 0x0f) << 2);
				}
				
				/* store the final bits for this pixel */
				dest[x + ix] = palbits;
			}
		}
	}
}

static INT32 BuckrogDraw()
{
	if (DrvRecalc) {
		BuckrogPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilesSetClipRaw(0, 256, 0, 256);
	GenericTilemapDraw(0, DrvBitmap, 0);
	GenericTilesClearClipRaw();

	screen_update_buckrog();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 TurboFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		if (DrvJoy4[0]) DrvDial-=4;
		if (DrvJoy4[1]) DrvDial+=4;
		if (DrvDial > 0xff) DrvDial = 0;
		if (DrvDial < 0x00) DrvDial = 0xff;

	}

	INT32 nInterleave = 128; // 256/2
	INT32 nCyclesTotal = 4992000 / 60;
	INT32 nSoundBufferPos = 0;

	ZetOpen(0);
	
	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetRun(nCyclesTotal / nInterleave);
		if (i == 224/2) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

		if (pBurnSoundOut && i&1) { // samplizer needs less update-latency for the speed changes.
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave/2);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnSampleRender(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	ZetClose();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			BurnSampleRender(pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 BuckrogFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 128;
	INT32 nCyclesTotal = 4992000 / 60;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		ZetRun(nCyclesTotal / nInterleave);
		if (i == 224/2) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		INT32 nSegment = ZetTotalCycles();
		ZetClose();

		ZetOpen(1);
		ZetRun(nSegment - ZetTotalCycles());
		ZetClose();
	}

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
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

		ZetScan(nAction);
		ppi8255_scan();

		SCAN_VAR(turbo_op);
		SCAN_VAR(turbo_ip);
		SCAN_VAR(turbo_fbpla);
		SCAN_VAR(turbo_fbcol);
		SCAN_VAR(turbo_last_analog);
		SCAN_VAR(turbo_collision);

		SCAN_VAR(turbo_bsel);
		SCAN_VAR(turbo_accel);
		SCAN_VAR(sound_data);
		
		SCAN_VAR(subroc3d_ply);
		SCAN_VAR(subroc3d_flip);
		SCAN_VAR(subroc3d_col);

		SCAN_VAR(buckrog_command);
		SCAN_VAR(buckrog_mov);
		SCAN_VAR(buckrog_fchg);
		SCAN_VAR(buckrog_obch);
	}

	return 0;
}


// Turbo (program 1513-1515)

static struct BurnRomInfo turboRomDesc[] = {
	{ "epr-1513.cpu-ic76",	0x2000, 0x0326adfc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "epr-1514.cpu-ic89",	0x2000, 0x25af63b0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-1515.cpu-ic103",	0x2000, 0x059c1c36, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-1246.prom-ic84",	0x2000, 0x555bfe9a, 2 | BRF_GRA },           //  3 Sprites
	{ "epr-1247.prom-ic86",	0x2000, 0xc8c5e4d5, 2 | BRF_GRA },           //  4
	{ "epr-1248.prom-ic88",	0x2000, 0x82fe5b94, 2 | BRF_GRA },           //  5
	{ "epr-1249.prom-ic90",	0x2000, 0xe258e009, 2 | BRF_GRA },           //  6
	{ "epr-1250.prom-ic108",0x2000, 0xaee6e05e, 2 | BRF_GRA },           //  7
	{ "epr-1251.prom-ic92",	0x2000, 0x292573de, 2 | BRF_GRA },           //  8
	{ "epr-1252.prom-ic110",0x2000, 0xaee6e05e, 2 | BRF_GRA },           //  9
	{ "epr-1253.prom-ic94",	0x2000, 0x92783626, 2 | BRF_GRA },           // 10
	{ "epr-1254.prom-ic112",0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 11
	{ "epr-1255.prom-ic32",	0x2000, 0x485dcef9, 2 | BRF_GRA },           // 12
	{ "epr-1256.prom-ic47",	0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 13
	{ "epr-1257.prom-ic34",	0x2000, 0x4ca984ce, 2 | BRF_GRA },           // 14
	{ "epr-1258.prom-ic49",	0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 15

	{ "epr-1244.cpu-ic111",	0x0800, 0x17f67424, 3 | BRF_GRA },           // 16 Foreground Tiles
	{ "epr-1245.cpu-ic122",	0x0800, 0x2ba0b46b, 3 | BRF_GRA },           // 17

	{ "epr-1125.cpu-ic1",	0x0800, 0x65b5d44b, 4 | BRF_GRA },           // 18 Road Data
	{ "epr-1126.cpu-ic2",	0x0800, 0x685ace1b, 4 | BRF_GRA },           // 19
	{ "epr-1127.cpu-ic13",	0x0800, 0x9233c9ca, 4 | BRF_GRA },           // 20
	{ "epr-1238.cpu-ic14",	0x0800, 0xd94fd83f, 4 | BRF_GRA },           // 21
	{ "epr-1239.cpu-ic27",	0x0800, 0x4c41124f, 4 | BRF_GRA },           // 22
	{ "epr-1240.cpu-ic28",	0x0800, 0x371d6282, 4 | BRF_GRA },           // 23
	{ "epr-1241.cpu-ic41",	0x0800, 0x1109358a, 4 | BRF_GRA },           // 24
	{ "epr-1242.cpu-ic42",	0x0800, 0x04866769, 4 | BRF_GRA },           // 25
	{ "epr-1243.cpu-ic74",	0x0800, 0x29854c48, 4 | BRF_GRA },           // 26

	{ "pr-1114.prom-ic13",	0x0020, 0x78aded46, 5 | BRF_GRA },           // 27 Color / Video PROMs
	{ "pr-1115.prom-ic18",	0x0020, 0x5394092c, 5 | BRF_GRA },           // 28
	{ "pr-1116.prom-ic20",	0x0020, 0x3956767d, 5 | BRF_GRA },           // 29
	{ "pr-1117.prom-ic21",	0x0020, 0xf06d9907, 5 | BRF_GRA },           // 30
	{ "pr-1118.cpu-ic99",	0x0100, 0x07324cfd, 5 | BRF_GRA },           // 31
	{ "pr-1119.cpu-ic50",	0x0200, 0x57ebd4bc, 5 | BRF_GRA },           // 32
	{ "pr-1120.cpu-ic62",	0x0200, 0x8dd4c8a8, 5 | BRF_GRA },           // 33
	{ "pr-1121.prom-ic29",	0x0200, 0x7692f497, 5 | BRF_GRA },           // 34
	{ "pr-1122.prom-ic11",	0x0400, 0x1a86ce70, 5 | BRF_GRA },           // 35
	{ "pr-1123.prom-ic12",	0x0400, 0x02d2cb52, 5 | BRF_GRA },           // 36
	{ "pr-1279.sound-ic40",	0x0020, 0xb369a6ae, 5 | BRF_GRA },           // 37
};

STD_ROM_PICK(turbo)
STD_ROM_FN(turbo)

static struct BurnSampleInfo turboSampleDesc[] = {
	{ "01", 		SAMPLE_NOLOOP },	/* 0: Trig1 */
	{ "02", 		SAMPLE_NOLOOP },	/* 1: Trig2 */
	{ "03", 		SAMPLE_NOLOOP },	/* 2: Trig3 */
	{ "04", 		SAMPLE_NOLOOP },	/* 3: Trig4 */
	{ "05", 		SAMPLE_NOLOOP },	/* 4: Screech */
	{ "06", 		SAMPLE_NOLOOP },	/* 5: Crash */
	{ "skidding",	SAMPLE_NOLOOP },	/* 6: Spin */
	{ "idle",		SAMPLE_NOLOOP },	/* 7: Idle */
	{ "ambulanc",	SAMPLE_NOLOOP },	/* 8: Ambulance */
	{ "", 0 }
};

STD_SAMPLE_PICK(turbo)
STD_SAMPLE_FN(turbo)

struct BurnDriver BurnDrvTurbo = {
	"turbo", NULL, NULL, "turbo", "1981",
	"Turbo (program 1513-1515)\0", NULL, "Sega", "Z80-3D",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, turboRomInfo, turboRomName, NULL, NULL, turboSampleInfo, turboSampleName, TurboInputInfo, TurboDIPInfo,
	TurboDecInit, DrvExit, TurboFrame, TurboDraw, DrvScan, &DrvRecalc, 0x100,
	224, 512, 3, 4
};


// Turbo (encrypted, program 1262-1264)

static struct BurnRomInfo turboaRomDesc[] = {
	{ "epr-1262.cpu-ic76",	0x2000, 0x1951b83a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "epr-1263.cpu-ic89",	0x2000, 0x45e01608, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-1264.cpu-ic103",	0x2000, 0x1802f6c7, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-1246.prom-ic84",	0x2000, 0x555bfe9a, 2 | BRF_GRA },           //  3 Sprites
	{ "epr-1247.prom-ic86",	0x2000, 0xc8c5e4d5, 2 | BRF_GRA },           //  4
	{ "epr-1248.prom-ic88",	0x2000, 0x82fe5b94, 2 | BRF_GRA },           //  5
	{ "epr-1249.prom-ic90",	0x2000, 0xe258e009, 2 | BRF_GRA },           //  6
	{ "epr-1250.prom-ic108",0x2000, 0xaee6e05e, 2 | BRF_GRA },           //  7
	{ "epr-1251.prom-ic92",	0x2000, 0x292573de, 2 | BRF_GRA },           //  8
	{ "epr-1252.prom-ic110",0x2000, 0xaee6e05e, 2 | BRF_GRA },           //  9
	{ "epr-1253.prom-ic94",	0x2000, 0x92783626, 2 | BRF_GRA },           // 10
	{ "epr-1254.prom-ic112",0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 11
	{ "epr-1255.prom-ic32",	0x2000, 0x485dcef9, 2 | BRF_GRA },           // 12
	{ "epr-1256.prom-ic47",	0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 13
	{ "epr-1257.prom-ic34",	0x2000, 0x4ca984ce, 2 | BRF_GRA },           // 14
	{ "epr-1258.prom-ic49",	0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 15

	{ "epr-1244.cpu-ic111",	0x0800, 0x17f67424, 3 | BRF_GRA },           // 16 Foreground Tiles
	{ "epr-1245.cpu-ic122",	0x0800, 0x2ba0b46b, 3 | BRF_GRA },           // 17

	{ "epr-1125.cpu-ic1",	0x0800, 0x65b5d44b, 4 | BRF_GRA },           // 18 Road Data
	{ "epr-1126.cpu-ic2",	0x0800, 0x685ace1b, 4 | BRF_GRA },           // 19
	{ "epr-1127.cpu-ic13",	0x0800, 0x9233c9ca, 4 | BRF_GRA },           // 20
	{ "epr-1238.cpu-ic14",	0x0800, 0xd94fd83f, 4 | BRF_GRA },           // 21
	{ "epr-1239.cpu-ic27",	0x0800, 0x4c41124f, 4 | BRF_GRA },           // 22
	{ "epr-1240.cpu-ic28",	0x0800, 0x371d6282, 4 | BRF_GRA },           // 23
	{ "epr-1241.cpu-ic41",	0x0800, 0x1109358a, 4 | BRF_GRA },           // 24
	{ "epr-1242.cpu-ic42",	0x0800, 0x04866769, 4 | BRF_GRA },           // 25
	{ "epr-1243.cpu-ic74",	0x0800, 0x29854c48, 4 | BRF_GRA },           // 26

	{ "pr-1114.prom-ic13",	0x0020, 0x78aded46, 5 | BRF_GRA },           // 27 proms
	{ "pr-1115.prom-ic18",	0x0020, 0x5394092c, 5 | BRF_GRA },           // 28
	{ "pr-1116.prom-ic20",	0x0020, 0x3956767d, 5 | BRF_GRA },           // 29
	{ "pr-1117.prom-ic21",	0x0020, 0xf06d9907, 5 | BRF_GRA },           // 30
	{ "pr-1118.cpu-ic99",	0x0100, 0x07324cfd, 5 | BRF_GRA },           // 31
	{ "pr-1119.cpu-ic50",	0x0200, 0x57ebd4bc, 5 | BRF_GRA },           // 32
	{ "pr-1120.cpu-ic62",	0x0200, 0x8dd4c8a8, 5 | BRF_GRA },           // 33
	{ "pr-1121.prom-ic29",	0x0200, 0x7692f497, 5 | BRF_GRA },           // 34
	{ "pr-1122.prom-ic11",	0x0400, 0x1a86ce70, 5 | BRF_GRA },           // 35
	{ "pr-1123.prom-ic12",	0x0400, 0x02d2cb52, 5 | BRF_GRA },           // 36
	{ "pr-1279.sound-ic40",	0x0020, 0xb369a6ae, 5 | BRF_GRA },           // 37
};

STD_ROM_PICK(turboa)
STD_ROM_FN(turboa)

struct BurnDriver BurnDrvTurboa = {
	"turboa", "turbo", NULL, "turbo", "1981",
	"Turbo (encrypted, program 1262-1264)\0", NULL, "Sega", "Z80-3D",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, turboaRomInfo, turboaRomName, NULL, NULL, turboSampleInfo, turboSampleName, TurboInputInfo, TurboDIPInfo,
	TurboEncInit, DrvExit, TurboFrame, TurboDraw, DrvScan, &DrvRecalc, 0x100,
	224, 512, 3, 4
};


// Turbo (encrypted, program 1363-1365 rev B)

static struct BurnRomInfo turbobRomDesc[] = {
	{ "epr-1363_t5b.ic76",	0x2000, 0xf7f28149, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "epr-1364_t5a.ic89",	0x2000, 0x6a341693, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-1365_t5a.ic103",	0x2000, 0x3b6b0dc8, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-1246.prom-ic84",	0x2000, 0x555bfe9a, 2 | BRF_GRA },           //  3 Sprites
	{ "epr-1247.prom-ic86",	0x2000, 0xc8c5e4d5, 2 | BRF_GRA },           //  4
	{ "epr-1248.prom-ic88",	0x2000, 0x82fe5b94, 2 | BRF_GRA },           //  5
	{ "epr-1249.prom-ic90",	0x2000, 0xe258e009, 2 | BRF_GRA },           //  6
	{ "epr-1250.prom-ic108",0x2000, 0xaee6e05e, 2 | BRF_GRA },           //  7
	{ "epr-1251.prom-ic92",	0x2000, 0x292573de, 2 | BRF_GRA },           //  8
	{ "epr-1252.prom-ic110",0x2000, 0xaee6e05e, 2 | BRF_GRA },           //  9
	{ "epr-1253.prom-ic94",	0x2000, 0x92783626, 2 | BRF_GRA },           // 10
	{ "epr-1254.prom-ic112",0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 11
	{ "epr-1255.prom-ic32",	0x2000, 0x485dcef9, 2 | BRF_GRA },           // 12
	{ "epr-1256.prom-ic47",	0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 13
	{ "epr-1257.prom-ic34",	0x2000, 0x4ca984ce, 2 | BRF_GRA },           // 14
	{ "epr-1258.prom-ic49",	0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 15

	{ "epr-1244.cpu-ic111",	0x0800, 0x17f67424, 3 | BRF_GRA },           // 16 Foreground Tiles
	{ "epr-1245.cpu-ic122",	0x0800, 0x2ba0b46b, 3 | BRF_GRA },           // 17

	{ "epr-1125.cpu-ic1",	0x0800, 0x65b5d44b, 4 | BRF_GRA },           // 18 Road Data
	{ "epr-1126.cpu-ic2",	0x0800, 0x685ace1b, 4 | BRF_GRA },           // 19
	{ "epr-1127.cpu-ic13",	0x0800, 0x9233c9ca, 4 | BRF_GRA },           // 20
	{ "epr-1238.cpu-ic14",	0x0800, 0xd94fd83f, 4 | BRF_GRA },           // 21
	{ "epr-1239.cpu-ic27",	0x0800, 0x4c41124f, 4 | BRF_GRA },           // 22
	{ "epr-1240.cpu-ic28",	0x0800, 0x371d6282, 4 | BRF_GRA },           // 23
	{ "epr-1241.cpu-ic41",	0x0800, 0x1109358a, 4 | BRF_GRA },           // 24
	{ "epr-1242.cpu-ic42",	0x0800, 0x04866769, 4 | BRF_GRA },           // 25
	{ "epr-1243.cpu-ic74",	0x0800, 0x29854c48, 4 | BRF_GRA },           // 26

	{ "pr-1114.prom-ic13",	0x0020, 0x78aded46, 5 | BRF_GRA },           // 27 Color / Video PROMs
	{ "pr-1115.prom-ic18",	0x0020, 0x5394092c, 5 | BRF_GRA },           // 28
	{ "pr-1116.prom-ic20",	0x0020, 0x3956767d, 5 | BRF_GRA },           // 29
	{ "pr-1117.prom-ic21",	0x0020, 0xf06d9907, 5 | BRF_GRA },           // 30
	{ "pr-1118.cpu-ic99",	0x0100, 0x07324cfd, 5 | BRF_GRA },           // 31
	{ "pr-1119.cpu-ic50",	0x0200, 0x57ebd4bc, 5 | BRF_GRA },           // 32
	{ "pr-1120.cpu-ic62",	0x0200, 0x8dd4c8a8, 5 | BRF_GRA },           // 33
	{ "pr-1121.prom-ic29",	0x0200, 0x7692f497, 5 | BRF_GRA },           // 34
	{ "pr-1122.prom-ic11",	0x0400, 0x1a86ce70, 5 | BRF_GRA },           // 35
	{ "pr-1123.prom-ic12",	0x0400, 0x02d2cb52, 5 | BRF_GRA },           // 36
	{ "pr-1279.sound-ic40",	0x0020, 0xb369a6ae, 5 | BRF_GRA },           // 37
};

STD_ROM_PICK(turbob)
STD_ROM_FN(turbob)

struct BurnDriver BurnDrvTurbob = {
	"turbob", "turbo", NULL, "turbo", "1981",
	"Turbo (encrypted, program 1363-1365 rev B)\0", NULL, "Sega", "Z80-3D",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, turbobRomInfo, turbobRomName, NULL, NULL, turboSampleInfo, turboSampleName, TurboInputInfo, TurboDIPInfo,
	TurboEncInit, DrvExit, TurboFrame, TurboDraw, DrvScan, &DrvRecalc, 0x100,
	224, 512, 3, 4
};


// Turbo (encrypted, program 1363-1365 rev A)

static struct BurnRomInfo turbocRomDesc[] = {
	{ "epr-1363_t5a.ic76",	0x2000, 0x5c110fb6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "epr-1364_t5a.ic89",	0x2000, 0x6a341693, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-1365_t5a.ic103",	0x2000, 0x3b6b0dc8, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-1246.prom-ic84",	0x2000, 0x555bfe9a, 2 | BRF_GRA },           //  3 Sprites
	{ "epr-1247.prom-ic86",	0x2000, 0xc8c5e4d5, 2 | BRF_GRA },           //  4
	{ "epr-1248.prom-ic88",	0x2000, 0x82fe5b94, 2 | BRF_GRA },           //  5
	{ "epr-1249.prom-ic90",	0x2000, 0xe258e009, 2 | BRF_GRA },           //  6
	{ "epr-1250.prom-ic108",0x2000, 0xaee6e05e, 2 | BRF_GRA },           //  7
	{ "epr-1251.prom-ic92",	0x2000, 0x292573de, 2 | BRF_GRA },           //  8
	{ "epr-1252.prom-ic110",0x2000, 0xaee6e05e, 2 | BRF_GRA },           //  9
	{ "epr-1253.prom-ic94",	0x2000, 0x92783626, 2 | BRF_GRA },           // 10
	{ "epr-1254.prom-ic112",0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 11
	{ "epr-1255.prom-ic32",	0x2000, 0x485dcef9, 2 | BRF_GRA },           // 12
	{ "epr-1256.prom-ic47",	0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 13
	{ "epr-1257.prom-ic34",	0x2000, 0x4ca984ce, 2 | BRF_GRA },           // 14
	{ "epr-1258.prom-ic49",	0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 15

	{ "epr-1244.cpu-ic111",	0x0800, 0x17f67424, 3 | BRF_GRA },           // 16 Foreground Tiles
	{ "epr-1245.cpu-ic122",	0x0800, 0x2ba0b46b, 3 | BRF_GRA },           // 17

	{ "epr-1125.cpu-ic1",	0x0800, 0x65b5d44b, 4 | BRF_GRA },           // 18 Road Data
	{ "epr-1126.cpu-ic2",	0x0800, 0x685ace1b, 4 | BRF_GRA },           // 19
	{ "epr-1127.cpu-ic13",	0x0800, 0x9233c9ca, 4 | BRF_GRA },           // 20
	{ "epr-1238.cpu-ic14",	0x0800, 0xd94fd83f, 4 | BRF_GRA },           // 21
	{ "epr-1239.cpu-ic27",	0x0800, 0x4c41124f, 4 | BRF_GRA },           // 22
	{ "epr-1240.cpu-ic28",	0x0800, 0x371d6282, 4 | BRF_GRA },           // 23
	{ "epr-1241.cpu-ic41",	0x0800, 0x1109358a, 4 | BRF_GRA },           // 24
	{ "epr-1242.cpu-ic42",	0x0800, 0x04866769, 4 | BRF_GRA },           // 25
	{ "epr-1243.cpu-ic74",	0x0800, 0x29854c48, 4 | BRF_GRA },           // 26

	{ "pr-1114.prom-ic13",	0x0020, 0x78aded46, 5 | BRF_GRA },           // 27 Color / Video PROMs
	{ "pr-1115.prom-ic18",	0x0020, 0x5394092c, 5 | BRF_GRA },           // 28
	{ "pr-1116.prom-ic20",	0x0020, 0x3956767d, 5 | BRF_GRA },           // 29
	{ "pr-1117.prom-ic21",	0x0020, 0xf06d9907, 5 | BRF_GRA },           // 30
	{ "pr-1118.cpu-ic99",	0x0100, 0x07324cfd, 5 | BRF_GRA },           // 31
	{ "pr-1119.cpu-ic50",	0x0200, 0x57ebd4bc, 5 | BRF_GRA },           // 32
	{ "pr-1120.cpu-ic62",	0x0200, 0x8dd4c8a8, 5 | BRF_GRA },           // 33
	{ "pr-1121.prom-ic29",	0x0200, 0x7692f497, 5 | BRF_GRA },           // 34
	{ "pr-1122.prom-ic11",	0x0400, 0x1a86ce70, 5 | BRF_GRA },           // 35
	{ "pr-1123.prom-ic12",	0x0400, 0x02d2cb52, 5 | BRF_GRA },           // 36
	{ "pr-1279.sound-ic40",	0x0020, 0xb369a6ae, 5 | BRF_GRA },           // 37
};

STD_ROM_PICK(turboc)
STD_ROM_FN(turboc)

struct BurnDriver BurnDrvTurboc = {
	"turboc", "turbo", NULL, "turbo", "1981",
	"Turbo (encrypted, program 1363-1365 rev A)\0", NULL, "Sega", "Z80-3D",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, turbocRomInfo, turbocRomName, NULL, NULL, turboSampleInfo, turboSampleName, TurboInputInfo, TurboDIPInfo,
	TurboEncInit, DrvExit, TurboFrame, TurboDraw, DrvScan, &DrvRecalc, 0x100,
	224, 512, 3, 4
};


// Turbo (encrypted, program 1363-1365)

static struct BurnRomInfo turbodRomDesc[] = {
	{ "1363.ic76",			0x2000, 0xb6329a00, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "1364.ic89",			0x2000, 0x3192f83b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1365.ic103",			0x2000, 0x23a3303a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-1246.prom-ic84",	0x2000, 0x555bfe9a, 2 | BRF_GRA },           //  3 Sprites
	{ "epr-1247.prom-ic86",	0x2000, 0xc8c5e4d5, 2 | BRF_GRA },           //  4
	{ "epr-1248.prom-ic88",	0x2000, 0x82fe5b94, 2 | BRF_GRA },           //  5
	{ "epr-1249.prom-ic90",	0x2000, 0xe258e009, 2 | BRF_GRA },           //  6
	{ "epr-1250.prom-ic108",0x2000, 0xaee6e05e, 2 | BRF_GRA },           //  7
	{ "epr-1251.prom-ic92",	0x2000, 0x292573de, 2 | BRF_GRA },           //  8
	{ "epr-1252.prom-ic110",0x2000, 0xaee6e05e, 2 | BRF_GRA },           //  9
	{ "epr-1253.prom-ic94",	0x2000, 0x92783626, 2 | BRF_GRA },           // 10
	{ "epr-1254.prom-ic112",0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 11
	{ "epr-1255.prom-ic32",	0x2000, 0x485dcef9, 2 | BRF_GRA },           // 12
	{ "epr-1256.prom-ic47",	0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 13
	{ "epr-1257.prom-ic34",	0x2000, 0x4ca984ce, 2 | BRF_GRA },           // 14
	{ "epr-1258.prom-ic49",	0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 15

	{ "epr-1244.cpu-ic111",	0x0800, 0x17f67424, 3 | BRF_GRA },           // 16 Foreground Tiles
	{ "epr-1245.cpu-ic122",	0x0800, 0x2ba0b46b, 3 | BRF_GRA },           // 17

	{ "epr-1125.cpu-ic1",	0x0800, 0x65b5d44b, 4 | BRF_GRA },           // 18 Road Data
	{ "epr-1126.cpu-ic2",	0x0800, 0x685ace1b, 4 | BRF_GRA },           // 19
	{ "epr-1127.cpu-ic13",	0x0800, 0x9233c9ca, 4 | BRF_GRA },           // 20
	{ "epr-1238.cpu-ic14",	0x0800, 0xd94fd83f, 4 | BRF_GRA },           // 21
	{ "epr-1239.cpu-ic27",	0x0800, 0x4c41124f, 4 | BRF_GRA },           // 22
	{ "epr-1240.cpu-ic28",	0x0800, 0x371d6282, 4 | BRF_GRA },           // 23
	{ "epr-1241.cpu-ic41",	0x0800, 0x1109358a, 4 | BRF_GRA },           // 24
	{ "epr-1242.cpu-ic42",	0x0800, 0x04866769, 4 | BRF_GRA },           // 25
	{ "epr-1243.cpu-ic74",	0x0800, 0x29854c48, 4 | BRF_GRA },           // 26

	{ "pr-1114.prom-ic13",	0x0020, 0x78aded46, 5 | BRF_GRA },           // 27 Color / Video PROMs
	{ "pr-1115.prom-ic18",	0x0020, 0x5394092c, 5 | BRF_GRA },           // 28
	{ "pr-1116.prom-ic20",	0x0020, 0x3956767d, 5 | BRF_GRA },           // 29
	{ "pr-1117.prom-ic21",	0x0020, 0xf06d9907, 5 | BRF_GRA },           // 30
	{ "pr-1118.cpu-ic99",	0x0100, 0x07324cfd, 5 | BRF_GRA },           // 31
	{ "pr-1119.cpu-ic50",	0x0200, 0x57ebd4bc, 5 | BRF_GRA },           // 32
	{ "pr-1120.cpu-ic62",	0x0200, 0x8dd4c8a8, 5 | BRF_GRA },           // 33
	{ "pr-1121.prom-ic29",	0x0200, 0x7692f497, 5 | BRF_GRA },           // 34
	{ "pr-1122.prom-ic11",	0x0400, 0x1a86ce70, 5 | BRF_GRA },           // 35
	{ "pr-1123.prom-ic12",	0x0400, 0x02d2cb52, 5 | BRF_GRA },           // 36
	{ "pr-1279.sound-ic40",	0x0020, 0xb369a6ae, 5 | BRF_GRA },           // 37
};

STD_ROM_PICK(turbod)
STD_ROM_FN(turbod)

struct BurnDriver BurnDrvTurbod = {
	"turbod", "turbo", NULL, "turbo", "1981",
	"Turbo (encrypted, program 1363-1365)\0", NULL, "Sega", "Z80-3D",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, turbodRomInfo, turbodRomName, NULL, NULL, turboSampleInfo, turboSampleName, TurboInputInfo, TurboDIPInfo,
	TurboEncInit, DrvExit, TurboFrame, TurboDraw, DrvScan, &DrvRecalc, 0x100,
	224, 512, 3, 4
};


// Indianapolis (bootleg of Turbo)

static struct BurnRomInfo turboblRomDesc[] = {
	{ "ic76.bin",			0x2000, 0xc208373b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "ic89.bin",			0x2000, 0x93ebc86a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic103.bin",			0x2000, 0x71876f74, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a-ic84.bin",			0x2000, 0x555bfe9a, 2 | BRF_GRA },           //  3 Sprites
	{ "b-ic86.bin",			0x2000, 0x82fe5b94, 2 | BRF_GRA },           //  4
	{ "c-ic88.bin",			0x2000, 0x95182020, 2 | BRF_GRA },           //  5
	{ "e-ic90.bin",			0x2000, 0x0e857f82, 2 | BRF_GRA },           //  6
	{ "d-ic99.bin",			0x2000, 0xaee6e05e, 2 | BRF_GRA },           //  7
	{ "g-ic92.bin",			0x2000, 0x292573de, 2 | BRF_GRA },           //  8
	{ "f-ic100.bin",		0x2000, 0xaee6e05e, 2 | BRF_GRA },           //  9
	{ "k-ic94.bin",			0x2000, 0x92783626, 2 | BRF_GRA },           // 10
	{ "h-ic101.bin",		0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 11
	{ "p-ic32.bin",			0x2000, 0x485dcef9, 2 | BRF_GRA },           // 12
	{ "n-ic47.bin",			0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 13
	{ "m-ic34.bin",			0x2000, 0x4ca984ce, 2 | BRF_GRA },           // 14
	{ "l-ic49.bin",			0x2000, 0xaee6e05e, 2 | BRF_GRA },           // 15

	{ "ic111.bin",			0x0800, 0xfab3899b, 3 | BRF_GRA },           // 16 Foreground Tiles
	{ "ic122.bin",			0x0800, 0xe5fab290, 3 | BRF_GRA },           // 17

	{ "ic1.bin",			0x1000, 0xc2f649a6, 4 | BRF_GRA },           // 18 Road Data
	{ "ic13.bin",			0x1000, 0xfefcf3be, 4 | BRF_GRA },           // 19
	{ "ic27.bin",			0x1000, 0x83195ee5, 4 | BRF_GRA },           // 20
	{ "ic41.bin",			0x1000, 0x3158a549, 4 | BRF_GRA },           // 21
	{ "ic74.bin",			0x0800, 0x29854c48, 4 | BRF_GRA },           // 22

	{ "ic90.bin",			0x0100, 0xeb2fd7a2, 5 | BRF_GRA },           // 23 unkproms

	{ "74s288.ic13",		0x0020, 0x78aded46, 6 | BRF_GRA },           // 24 Color / Video PROMs
	{ "74s288.ic18",		0x0020, 0x172d0835, 6 | BRF_GRA },           // 25
	{ "74s288.ic20",		0x0020, 0x3956767d, 6 | BRF_GRA },           // 26
	{ "74s288.ic21",		0x0020, 0xf06d9907, 6 | BRF_GRA },           // 27
	{ "ic99.bin",			0x0100, 0x59f36e1c, 6 | BRF_GRA },           // 28
	{ "pr-1119.cpu-ic50",	0x0200, 0x57ebd4bc, 6 | BRF_GRA },           // 29
	{ "pr-1120.cpu-ic62",	0x0200, 0x8dd4c8a8, 6 | BRF_GRA },           // 30
	{ "pr-1121.prom-ic29",	0x0200, 0x7692f497, 6 | BRF_GRA },           // 31
	{ "pr-1122.prom-ic11",	0x0400, 0x1a86ce70, 6 | BRF_GRA },           // 32
	{ "pr-1123.prom-ic12",	0x0400, 0x02d2cb52, 6 | BRF_GRA },           // 33
	{ "pr-1279.sound-ic40",	0x0020, 0xb369a6ae, 6 | BRF_GRA },           // 34
};

STD_ROM_PICK(turbobl)
STD_ROM_FN(turbobl)

struct BurnDriver BurnDrvTurbobl = {
	"turbobl", "turbo", NULL, "turbo", "1981",
	"Indianapolis (bootleg of Turbo)\0", NULL, "bootleg", "Z80-3D",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, turboblRomInfo, turboblRomName, NULL, NULL, turboSampleInfo, turboSampleName, TurboInputInfo, TurboDIPInfo,
	TurboDecInit, DrvExit, TurboFrame, TurboDraw, DrvScan, &DrvRecalc, 0x100,
	224, 512, 3, 4
};


// Subroc-3D

static struct BurnRomInfo subroc3dRomDesc[] = {
	{ "epr-1614a.cpu-ic88",	0x2000, 0x0ed856b4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "epr-1615.cpu-ic87",	0x2000, 0x6281eb2e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-1616.cpu-ic86",	0x2000, 0xcc7b0c9b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "epr-1417.prom-ic29",	0x2000, 0x2aaff4e0, 2 | BRF_GRA },           //  3 Sprites
	{ "epr-1418.prom-ic30",	0x2000, 0x41ff0f15, 2 | BRF_GRA },           //  4
	{ "epr-1419.prom-ic55",	0x2000, 0x37ac818c, 2 | BRF_GRA },           //  5
	{ "epr-1420.prom-ic56",	0x2000, 0x41ff0f15, 2 | BRF_GRA },           //  6
	{ "epr-1422.prom-ic81",	0x2000, 0x0221db58, 2 | BRF_GRA },           //  7
	{ "epr-1423.prom-ic82",	0x2000, 0x08b1a4b8, 2 | BRF_GRA },           //  8
	{ "epr-1421.prom-ic80",	0x2000, 0x1db33c09, 2 | BRF_GRA },           //  9
	{ "epr-1425.prom-ic107",0x2000, 0x0221db58, 2 | BRF_GRA },           // 10
	{ "epr-1426.prom-ic108",0x2000, 0x08b1a4b8, 2 | BRF_GRA },           // 11
	{ "epr-1424.prom-ic106",0x2000, 0x1db33c09, 2 | BRF_GRA },           // 12
	{ "epr-1664.prom-ic116",0x2000, 0x6c93ece7, 2 | BRF_GRA },           // 13
	{ "epr-1427.prom-ic115",0x2000, 0x2f8cfc2d, 2 | BRF_GRA },           // 14
	{ "epr-1429.prom-ic117",0x2000, 0x80e649c7, 2 | BRF_GRA },           // 15
	{ "epr-1665.prom-ic90",	0x2000, 0x6c93ece7, 2 | BRF_GRA },           // 16
	{ "epr-1430.prom-ic89",	0x2000, 0x2f8cfc2d, 2 | BRF_GRA },           // 17
	{ "epr-1432.prom-ic91",	0x2000, 0xd9cd98d0, 2 | BRF_GRA },           // 18
	{ "epr-1666.prom-ic64",	0x2000, 0x6c93ece7, 2 | BRF_GRA },           // 19
	{ "epr-1433.prom-ic63",	0x2000, 0x2f8cfc2d, 2 | BRF_GRA },           // 20
	{ "epr-1436.prom-ic66",	0x2000, 0xfc4ad926, 2 | BRF_GRA },           // 21
	{ "epr-1435.prom-ic65",	0x2000, 0x40662eef, 2 | BRF_GRA },           // 22
	{ "epr-1438.prom-ic38",	0x2000, 0xd563d4c1, 2 | BRF_GRA },           // 23
	{ "epr-1437.prom-ic37",	0x2000, 0x18ba6aad, 2 | BRF_GRA },           // 24
	{ "epr-1440.prom-ic40",	0x2000, 0x3a0e659c, 2 | BRF_GRA },           // 25
	{ "epr-1439.prom-ic39",	0x2000, 0x3d051668, 2 | BRF_GRA },           // 26

	{ "epr-1618.cpu-ic82",	0x0800, 0xa25fea71, 3 | BRF_GRA },           // 27 Foreground TIles
	{ "epr-1617.cpu-ic83",	0x0800, 0xf70c678e, 3 | BRF_GRA },           // 28

	{ "pr-1419.cpu-ic108",	0x0200, 0x2cfa2a3f, 4 | BRF_GRA },           // 29 Color / Video PROMs
	{ "pr-1620.cpu-ic62",	0x0100, 0x0ab7ef09, 4 | BRF_GRA },           // 30
	{ "pr-1449.cpu-ic5",	0x0200, 0x5eb9ff47, 4 | BRF_GRA },           // 31
	{ "pr-1450.cpu-ic21",	0x0200, 0x66bdb00c, 4 | BRF_GRA },           // 32
	{ "pr-1451.cpu-ic58",	0x0200, 0x6a575261, 4 | BRF_GRA },           // 33
	{ "pr-1453.cpu-ic39",	0x0020, 0x181c6d23, 4 | BRF_GRA },           // 34
	{ "pr-1454.cpu-ic67",	0x0020, 0xdc683440, 4 | BRF_GRA },           // 35
};

STD_ROM_PICK(subroc3d)
STD_ROM_FN(subroc3d)

static struct BurnSampleInfo subroc3dSampleDesc[] = {
	{ "01",	SAMPLE_NOLOOP },   /*  0: enemy missile */
	{ "02",	SAMPLE_NOLOOP },   /*  1: enemy torpedo */
	{ "03",	SAMPLE_NOLOOP },   /*  2: enemy fighter */
	{ "04",	SAMPLE_NOLOOP },   /*  3: explosion in sky */
	{ "05",	SAMPLE_NOLOOP },   /*  4: explosion on sea */
	{ "06",	SAMPLE_NOLOOP },   /*  5: missile shoot */
	{ "07",	SAMPLE_NOLOOP },   /*  6: torpedo shoot */
	{ "08",	SAMPLE_NOLOOP },   /*  7: my ship expl */
	{ "09",	SAMPLE_NOLOOP },   /*  8: prolog sound */
	{ "11",	SAMPLE_NOLOOP },   /*  9: alarm 0 */
	{ "12",	SAMPLE_NOLOOP },   /* 10: alarm 1 */
	{ "", 0 }
};

STD_SAMPLE_PICK(subroc3d)
STD_SAMPLE_FN(subroc3d)

struct BurnDriver BurnDrvSubroc3d = {
	"subroc3d", NULL, NULL, "subroc3d", "1982",
	"Subroc-3D\0", NULL, "Sega", "Z80-3D",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, subroc3dRomInfo, subroc3dRomName, NULL, NULL, subroc3dSampleInfo, subroc3dSampleName, Subroc3dInputInfo, Subroc3dDIPInfo,
	Subroc3dInit, DrvExit, TurboFrame, Subroc3dDraw, DrvScan, &DrvRecalc, 0x100,
	512, 224, 4, 3
};


// Buck Rogers: Planet of Zoom

static struct BurnRomInfo buckrogRomDesc[] = {
	{ "epr-5265.cpu-ic3",	0x4000, 0xf0055e97, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (encrypted)
	{ "epr-5266.cpu-ic4",	0x4000, 0x7d084c39, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-5200.cpu-ic66",	0x1000, 0x0d58b154, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "epr-5216.prom-ic100",0x2000, 0x8155bd73, 3 | BRF_GRA },           //  3 Sprites
	{ "epr-5213.prom-ic84",	0x2000, 0xfd78dda4, 3 | BRF_GRA },           //  4
	{ "epr-5262.prom-ic68",	0x4000, 0x2a194270, 3 | BRF_GRA },           //  5
	{ "epr-5260.prom-ic52",	0x4000, 0xb31a120f, 3 | BRF_GRA },           //  6
	{ "epr-5259.prom-ic43",	0x4000, 0xd3584926, 3 | BRF_GRA },           //  7
	{ "epr-5261.prom-ic59",	0x4000, 0xd83c7fcf, 3 | BRF_GRA },           //  8
	{ "epr-5208.prom-ic58",	0x2000, 0xd181fed2, 3 | BRF_GRA },           //  9
	{ "epr-5263.prom-ic75",	0x4000, 0x1bd6e453, 3 | BRF_GRA },           // 10
	{ "epr-5237.prom-ic74",	0x2000, 0xc34e9b82, 3 | BRF_GRA },           // 11
	{ "epr-5264.prom-ic91",	0x4000, 0x221f4ced, 3 | BRF_GRA },           // 12
	{ "epr-5238.prom-ic90",	0x2000, 0x7aff0886, 3 | BRF_GRA },           // 13

	{ "epr-5201.cpu-ic102",	0x0800, 0x7f21b0a4, 4 | BRF_GRA },           // 14 Foreground Tiles
	{ "epr-5202.cpu-ic103",	0x0800, 0x43f3e5a7, 4 | BRF_GRA },           // 15

	{ "epr-5203.cpu-ic91",	0x2000, 0x631f5b65, 5 | BRF_GRA },           // 16 Background Color Data

	{ "pr-5194.cpu-ic39",	0x0020, 0xbc88cced, 6 | BRF_GRA },           // 17 Color / Video PROMs
	{ "pr-5195.cpu-ic53",	0x0020, 0x181c6d23, 6 | BRF_GRA },           // 18
	{ "pr-5196.cpu-ic10",	0x0200, 0x04204bcf, 6 | BRF_GRA },           // 19
	{ "pr-5197.cpu-ic78",	0x0200, 0xa42674af, 6 | BRF_GRA },           // 20
	{ "pr-5198.cpu-ic93",	0x0200, 0x32e74bc8, 6 | BRF_GRA },           // 21
	{ "pr-5233.cpu-ic95",	0x0400, 0x1cd08c4e, 6 | BRF_GRA },           // 22
};

STD_ROM_PICK(buckrog)
STD_ROM_FN(buckrog)

static struct BurnSampleInfo BuckrogSampleDesc[] = {
	{ "alarm0",		SAMPLE_NOLOOP },	/* 0 */
	{ "alarm1",		SAMPLE_NOLOOP },	/* 1 */
	{ "alarm2",		SAMPLE_NOLOOP },	/* 2 */
	{ "alarm3",		SAMPLE_NOLOOP },	/* 3 */
	{ "exp",		SAMPLE_NOLOOP },	/* 4 */
	{ "fire",		SAMPLE_NOLOOP },	/* 5 */
	{ "rebound",	SAMPLE_NOLOOP },	/* 6 */
	{ "hit",		SAMPLE_NOLOOP },	/* 7 */
	{ "shipsnd1",	SAMPLE_NOLOOP },	/* 8 */
	{ "shipsnd2",	SAMPLE_NOLOOP },	/* 9 */
	{ "shipsnd3",	SAMPLE_NOLOOP },	/* 10 */
	{ "", 0 }
};

STD_SAMPLE_PICK(Buckrog)
STD_SAMPLE_FN(Buckrog)

struct BurnDriver BurnDrvBuckrog = {
	"buckrog", NULL, NULL, "buckrog", "1982",
	"Buck Rogers: Planet of Zoom\0", NULL, "Sega", "Z80-3D",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, buckrogRomInfo, buckrogRomName, NULL, NULL, BuckrogSampleInfo, BuckrogSampleName, BuckrogInputInfo, BuckrogDIPInfo,
	BuckrogEncInit, DrvExit, BuckrogFrame, BuckrogDraw, DrvScan, &DrvRecalc, 0x400,
	512, 224, 4, 3
};


// Buck Rogers: Planet of Zoom (not encrypted, set 1)

static struct BurnRomInfo buckrognRomDesc[] = {
	{ "cpu-ic3.bin",		0x4000, 0x7f1910af, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "cpu-ic4.bin",		0x4000, 0x5ecd393b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-5200.cpu-ic66",	0x1000, 0x0d58b154, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "epr-5216.prom-ic100",0x2000, 0x8155bd73, 3 | BRF_GRA },           //  3 Sprites
	{ "epr-5213.prom-ic84",	0x2000, 0xfd78dda4, 3 | BRF_GRA },           //  4
	{ "epr-5262.prom-ic68",	0x4000, 0x2a194270, 3 | BRF_GRA },           //  5
	{ "epr-5260.prom-ic52",	0x4000, 0xb31a120f, 3 | BRF_GRA },           //  6
	{ "epr-5259.prom-ic43",	0x4000, 0xd3584926, 3 | BRF_GRA },           //  7
	{ "epr-5261.prom-ic59",	0x4000, 0xd83c7fcf, 3 | BRF_GRA },           //  8
	{ "epr-5208.prom-ic58",	0x2000, 0xd181fed2, 3 | BRF_GRA },           //  9
	{ "epr-5263.prom-ic75",	0x4000, 0x1bd6e453, 3 | BRF_GRA },           // 10
	{ "epr-5237.prom-ic74",	0x2000, 0xc34e9b82, 3 | BRF_GRA },           // 11
	{ "epr-5264.prom-ic91",	0x4000, 0x221f4ced, 3 | BRF_GRA },           // 12
	{ "epr-5238.prom-ic90",	0x2000, 0x7aff0886, 3 | BRF_GRA },           // 13

	{ "epr-5201.cpu-ic102",	0x0800, 0x7f21b0a4, 4 | BRF_GRA },           // 14 Foreground Tiles
	{ "epr-5202.cpu-ic103",	0x0800, 0x43f3e5a7, 4 | BRF_GRA },           // 15

	{ "epr-5203.cpu-ic91",	0x2000, 0x631f5b65, 5 | BRF_GRA },           // 16 Background Color Data

	{ "pr-5194.cpu-ic39",	0x0020, 0xbc88cced, 6 | BRF_GRA },           // 17 Color / Video PROMs
	{ "pr-5195.cpu-ic53",	0x0020, 0x181c6d23, 6 | BRF_GRA },           // 18
	{ "pr-5196.cpu-ic10",	0x0200, 0x04204bcf, 6 | BRF_GRA },           // 19
	{ "pr-5197.cpu-ic78",	0x0200, 0xa42674af, 6 | BRF_GRA },           // 20
	{ "pr-5198.cpu-ic93",	0x0200, 0x32e74bc8, 6 | BRF_GRA },           // 21
	{ "pr-5199.cpu-ic95",	0x0400, 0x45e997a8, 6 | BRF_GRA },           // 22
};

STD_ROM_PICK(buckrogn)
STD_ROM_FN(buckrogn)

struct BurnDriver BurnDrvBuckrogn = {
	"buckrogn", "buckrog", NULL, "buckrog", "1982",
	"Buck Rogers: Planet of Zoom (not encrypted, set 1)\0", NULL, "Sega", "Z80-3D",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, buckrognRomInfo, buckrognRomName, NULL, NULL, BuckrogSampleInfo, BuckrogSampleName, BuckrogInputInfo, BuckrogDIPInfo,
	BuckrogDecInit, DrvExit, BuckrogFrame, BuckrogDraw, DrvScan, &DrvRecalc, 0x400,
	512, 224, 4, 3
};


// Buck Rogers: Planet of Zoom (not encrypted, set 2)

static struct BurnRomInfo buckrogn2RomDesc[] = {
	{ "epr-5204.cpu-ic3",	0x4000, 0xc2d43741, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "epr-5205.cpu-ic4",	0x4000, 0x648f3546, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-5200.cpu-ic66",	0x1000, 0x0d58b154, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "epr-5216.prom-ic100",0x2000, 0x8155bd73, 3 | BRF_GRA },           //  3 Sprites
	{ "epr-5213.prom-ic84",	0x2000, 0xfd78dda4, 3 | BRF_GRA },           //  4
	{ "epr-5210.prom-ic68",	0x4000, 0xc25b7b9e, 3 | BRF_GRA },           //  5
	{ "epr-5235.prom-ic52",	0x4000, 0x0ba5dac1, 3 | BRF_GRA },           //  6
	{ "epr-5234.prom-ic43",	0x4000, 0x6b773a81, 3 | BRF_GRA },           //  7
	{ "epr-5236.prom-ic59",	0x4000, 0xd11ce162, 3 | BRF_GRA },           //  8
	{ "epr-5208.prom-ic58",	0x2000, 0xd181fed2, 3 | BRF_GRA },           //  9
	{ "epr-5212.prom-ic75",	0x4000, 0x9359ec4f, 3 | BRF_GRA },           // 10
	{ "epr-5237.prom-ic74",	0x2000, 0xc34e9b82, 3 | BRF_GRA },           // 11
	{ "epr-5215.prom-ic91",	0x4000, 0xf5dacc53, 3 | BRF_GRA },           // 12
	{ "epr-5238.prom-ic90",	0x2000, 0x7aff0886, 3 | BRF_GRA },           // 13

	{ "epr-5201.cpu-ic102",	0x0800, 0x7f21b0a4, 4 | BRF_GRA },           // 14 Foreground Tiles
	{ "epr-5202.cpu-ic103",	0x0800, 0x43f3e5a7, 4 | BRF_GRA },           // 15

	{ "epr-5203.cpu-ic91",	0x2000, 0x631f5b65, 5 | BRF_GRA },           // 16 Background Color Data

	{ "pr-5194.cpu-ic39",	0x0020, 0xbc88cced, 6 | BRF_GRA },           // 17 Color / Video PROMs
	{ "pr-5195.cpu-ic53",	0x0020, 0x181c6d23, 6 | BRF_GRA },           // 18
	{ "pr-5196.cpu-ic10",	0x0200, 0x04204bcf, 6 | BRF_GRA },           // 19
	{ "pr-5197.cpu-ic78",	0x0200, 0xa42674af, 6 | BRF_GRA },           // 20
	{ "pr-5198.cpu-ic93",	0x0200, 0x32e74bc8, 6 | BRF_GRA },           // 21
	{ "pr-5233.cpu-ic95",	0x0400, 0x1cd08c4e, 6 | BRF_GRA },           // 22
};

STD_ROM_PICK(buckrogn2)
STD_ROM_FN(buckrogn2)

struct BurnDriver BurnDrvBuckrogn2 = {
	"buckrogn2", "buckrog", NULL, "buckrog", "1982",
	"Buck Rogers: Planet of Zoom (not encrypted, set 2)\0", NULL, "Sega", "Z80-3D",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, buckrogn2RomInfo, buckrogn2RomName, NULL, NULL, BuckrogSampleInfo, BuckrogSampleName, BuckrogInputInfo, BuckrogDIPInfo,
	BuckrogDecInit, DrvExit, BuckrogFrame, BuckrogDraw, DrvScan, &DrvRecalc, 0x400,
	512, 224, 4, 3
};


// Zoom 909

static struct BurnRomInfo zoom909RomDesc[] = {
	{ "epr-5217b.cpu-ic3",	0x4000, 0x1b56e7dd, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "epr-5218b.cpu-ic4",	0x4000, 0x77dfd911, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "epr-5200.cpu-ic66",	0x1000, 0x0d58b154, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "epr-5216.prom-ic100",0x2000, 0x8155bd73, 3 | BRF_GRA },           //  3 Sprites
	{ "epr-5213.prom-ic84",	0x2000, 0xfd78dda4, 3 | BRF_GRA },           //  4
	{ "epr-5231.prom-ic68",	0x4000, 0xf00385fc, 3 | BRF_GRA },           //  5
	{ "epr-5207.prom-ic52",	0x4000, 0x644f29d8, 3 | BRF_GRA },           //  6
	{ "epr-5206.prom-ic43",	0x4000, 0x049dc998, 3 | BRF_GRA },           //  7
	{ "epr-5209.prom-ic59",	0x4000, 0x0ff9ff71, 3 | BRF_GRA },           //  8
	{ "epr-5208.prom-ic58",	0x2000, 0xd181fed2, 3 | BRF_GRA },           //  9
	{ "epr-5212.prom-ic75",	0x4000, 0x9359ec4f, 3 | BRF_GRA },           // 10
	{ "epr-5211.prom-ic74",	0x2000, 0xd181fed2, 3 | BRF_GRA },           // 11
	{ "epr-5215.prom-ic91",	0x4000, 0xf5dacc53, 3 | BRF_GRA },           // 12
	{ "epr-5214.prom-ic90",	0x2000, 0x68306dd6, 3 | BRF_GRA },           // 13

	{ "epr-5201.cpu-ic102",	0x0800, 0x7f21b0a4, 4 | BRF_GRA },           // 14 Foreground Tiles
	{ "epr-5202.cpu-ic103",	0x0800, 0x43f3e5a7, 4 | BRF_GRA },           // 15

	{ "epr-5203.cpu-ic91",	0x2000, 0x631f5b65, 5 | BRF_GRA },           // 16 Background Color Data

	{ "pr-5194.cpu-ic39",	0x0020, 0xbc88cced, 6 | BRF_GRA },           // 17 Color / Video PROMs
	{ "pr-5195.cpu-ic53",	0x0020, 0x181c6d23, 6 | BRF_GRA },           // 18
	{ "pr-5196.cpu-ic10",	0x0200, 0x04204bcf, 6 | BRF_GRA },           // 19
	{ "pr-5197.cpu-ic78",	0x0200, 0xa42674af, 6 | BRF_GRA },           // 20
	{ "pr-5198.cpu-ic93",	0x0200, 0x32e74bc8, 6 | BRF_GRA },           // 21
	{ "pr-5199.cpu-ic95",	0x0400, 0x45e997a8, 6 | BRF_GRA },           // 22
};

STD_ROM_PICK(zoom909)
STD_ROM_FN(zoom909)

struct BurnDriver BurnDrvZoom909 = {
	"zoom909", "buckrog", NULL, "buckrog", "1982",
	"Zoom 909\0", NULL, "Sega", "Z80-3D",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, zoom909RomInfo, zoom909RomName, NULL, NULL, BuckrogSampleInfo, BuckrogSampleName, BuckrogInputInfo, BuckrogDIPInfo,
	BuckrogEncInit, DrvExit, BuckrogFrame, BuckrogDraw, DrvScan, &DrvRecalc, 0x400,
	512, 224, 4, 3
};
