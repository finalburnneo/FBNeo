// FB Alpha Battlezone / Bradley Tank Trainer / Red Baron driver module
// Based on MAME driver by Brad Oliver and Nicola Salmoria

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "burn_gun.h"
#include "mathbox.h"
#include "vector.h"
#include "avgdvg.h"
#include "pokey.h"
#include "watchdog.h"
#include "earom.h"
#include "redbaron.h" // audio custom
#include "bzone.h" // audio custom

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

static INT32 nExtraCycles;
static UINT8 analog_data = 0;
static INT32 input_select = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvFakeInput[4];
static UINT32 DrvFakeInputPrev;
static UINT8 DrvDips[4];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;
static INT16 DrvAnalogPort2 = 0;

static INT32 x_target, y_target; // for smooth moves between analog values
static INT32 x_adder, y_adder;

static INT32 bradley = 0;
static INT32 redbaron = 0;
static INT32 redbarona = 0;

static struct BurnInputInfo BzoneInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,		"p1 coin"	},
	{"P2 Coin",				BIT_DIGITAL,	DrvJoy1 + 1,		"p2 coin"	},
	{"Start 1",				BIT_DIGITAL,	DrvJoy2 + 5,		"p1 start"	},
	{"Start 2",				BIT_DIGITAL,	DrvJoy2 + 6,		"p2 start"	},
	{"Fire",				BIT_DIGITAL,	DrvJoy2 + 4,		"p1 fire 1"	},
	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy2 + 3,		"p1 fire 2"	},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy2 + 2,		"p1 fire 3"	},
	{"P1 Right Stick Up",	BIT_DIGITAL,	DrvJoy2 + 1,		"p1 fire 4"	},
	{"P1 Right Stick Down",	BIT_DIGITAL,	DrvJoy2 + 0,		"p1 fire 5"	},
	{"P1 Up (Fake)",		BIT_DIGITAL,	DrvFakeInput + 0,	"p1 up"		},
	{"P1 Down (Fake)",		BIT_DIGITAL,	DrvFakeInput + 1,	"p1 down"	},
	{"P1 Left (Fake)",		BIT_DIGITAL,	DrvFakeInput + 2,	"p1 left"	},
	{"P1 Right (Fake)",		BIT_DIGITAL,	DrvFakeInput + 3,	"p1 right"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,			"reset"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,		"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,		"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,		"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,		"dip"		},
};

STDINPUTINFO(Bzone)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo RedbaronInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 start"	},
	{"P1 Up",				BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",				BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 fire 1"	},

	A("P1 Stick X",         BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis" ),
	A("P1 Stick Y",         BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis" ),

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Diagnostic Step",		BIT_DIGITAL,	DrvJoy1 + 5,	"service2"	},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};
#undef A

STDINPUTINFO(Redbaron)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo BradleyInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 6"	},
	{"P1 Button 7",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 fire 7"	},
	{"P1 Button 8",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 fire 8"	},
	{"P1 Button 9",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 9"	},
	{"P1 Button 10",	BIT_DIGITAL,	DrvJoy4 + 4,	"p1 fire 10"},

	A("P1 Stick X",     BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis" ),
	A("P1 Stick Y",     BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis" ),
	A("P1 Stick Z",     BIT_ANALOG_REL, &DrvAnalogPort2,"p1 z-axis" ),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};
#undef A

STDINPUTINFO(Bradley)

#ifdef __LIBRETRO__
#define RESO_DIP
#else
#define RESO_DIP \
	{0   , 0xfe, 0   ,    2, "Hires Mode"			}, \
	{0x03, 0x01, 0x01, 0x00, "No"					}, \
	{0x03, 0x01, 0x01, 0x01, "Yes"					},
#endif

static struct BurnDIPInfo BzoneDIPList[]=
{
	DIP_OFFSET(0x0e)

	{0x00, 0xff, 0xff, 0x15, NULL					},
	{0x01, 0xff, 0xff, 0x02, NULL					},
	{0x02, 0xff, 0xff, 0x10, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x03, 0x00, "2"					},
	{0x00, 0x01, 0x03, 0x01, "3"					},
	{0x00, 0x01, 0x03, 0x02, "4"					},
	{0x00, 0x01, 0x03, 0x03, "5"					},

	{0   , 0xfe, 0   ,    4, "Missile appears at"	},
	{0x00, 0x01, 0x0c, 0x00, "5000"					},
	{0x00, 0x01, 0x0c, 0x04, "10000"				},
	{0x00, 0x01, 0x0c, 0x08, "20000"				},
	{0x00, 0x01, 0x0c, 0x0c, "30000"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x00, 0x01, 0x30, 0x10, "15k and 100k"			},
	{0x00, 0x01, 0x30, 0x20, "25k and 100k"			},
	{0x00, 0x01, 0x30, 0x30, "50k and 100k"			},
	{0x00, 0x01, 0x30, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x00, 0x01, 0xc0, 0x00, "English"				},
	{0x00, 0x01, 0xc0, 0x40, "German"				},
	{0x00, 0x01, 0xc0, 0x80, "French"				},
	{0x00, 0x01, 0xc0, 0xc0, "Spanish"				},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x01, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{0x01, 0x01, 0x03, 0x02, "1 Coin  1 Credits"	},
	{0x01, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x01, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x01, 0x01, 0x0c, 0x00, "*1"					},
	{0x01, 0x01, 0x0c, 0x04, "*4"					},
	{0x01, 0x01, 0x0c, 0x08, "*5"					},
	{0x01, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Coin A"				},
	{0x01, 0x01, 0x10, 0x00, "*1"					},
	{0x01, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    5, "Bonus Coins"			},
	{0x01, 0x01, 0xe0, 0x00, "None"					},
	{0x01, 0x01, 0xe0, 0x20, "3 credits/2 coins"	},
	{0x01, 0x01, 0xe0, 0x40, "5 credits/4 coins"	},
	{0x01, 0x01, 0xe0, 0x60, "6 credits/4 coins"	},
	{0x01, 0x01, 0xe0, 0x80, "6 credits/5 coins"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x10, 0x00, "On"					},
	{0x02, 0x01, 0x10, 0x10, "Off"					},

	RESO_DIP
};

STDDIPINFO(Bzone)

static struct BurnDIPInfo RedbaronDIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0xfd, NULL					},
	{0x01, 0xff, 0xff, 0xe7, NULL					},
	{0x02, 0xff, 0xff, 0x10, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    1, "Coinage"				},
	{0x00, 0x01, 0xff, 0xfd, "Normal"				},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x01, 0x01, 0x03, 0x00, "German"				},
	{0x01, 0x01, 0x03, 0x01, "French"				},
	{0x01, 0x01, 0x03, 0x02, "Spanish"				},
	{0x01, 0x01, 0x03, 0x03, "English"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x0c, 0x0c, "2k 10k 30k"			},
	{0x01, 0x01, 0x0c, 0x08, "4k 15k 40k"			},
	{0x01, 0x01, 0x0c, 0x04, "6k 20k 50k"			},
	{0x01, 0x01, 0x0c, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x30, 0x30, "2"					},
	{0x01, 0x01, 0x30, 0x20, "3"					},
	{0x01, 0x01, 0x30, 0x10, "4"					},
	{0x01, 0x01, 0x30, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "One Play Minimum"		},
	{0x01, 0x01, 0x40, 0x40, "Off"					},
	{0x01, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Self Adjust Diff"		},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x10, 0x00, "On"					},
	{0x02, 0x01, 0x10, 0x10, "Off"					},

	RESO_DIP
};

STDDIPINFO(Redbaron)

static struct BurnDIPInfo BradleyDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0x15, NULL					},
	{0x01, 0xff, 0xff, 0x02, NULL					},
	{0x02, 0xff, 0xff, 0x10, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x03, 0x00, "2"					},
	{0x00, 0x01, 0x03, 0x01, "3"					},
	{0x00, 0x01, 0x03, 0x02, "4"					},
	{0x00, 0x01, 0x03, 0x03, "5"					},

	{0   , 0xfe, 0   ,    4, "Missile appears at"	},
	{0x00, 0x01, 0x0c, 0x00, "5000"					},
	{0x00, 0x01, 0x0c, 0x04, "10000"				},
	{0x00, 0x01, 0x0c, 0x08, "20000"				},
	{0x00, 0x01, 0x0c, 0x0c, "30000"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x00, 0x01, 0x30, 0x10, "15k and 100k"			},
	{0x00, 0x01, 0x30, 0x20, "25k and 100k"			},
	{0x00, 0x01, 0x30, 0x30, "50k and 100k"			},
	{0x00, 0x01, 0x30, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x00, 0x01, 0xc0, 0x00, "English"				},
	{0x00, 0x01, 0xc0, 0x40, "German"				},
	{0x00, 0x01, 0xc0, 0x80, "French"				},
	{0x00, 0x01, 0xc0, 0xc0, "Spanish"				},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x01, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{0x01, 0x01, 0x03, 0x02, "1 Coin  1 Credits"	},
	{0x01, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x01, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x01, 0x01, 0x0c, 0x00, "*1"					},
	{0x01, 0x01, 0x0c, 0x04, "*4"					},
	{0x01, 0x01, 0x0c, 0x08, "*5"					},
	{0x01, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Coin A"				},
	{0x01, 0x01, 0x10, 0x00, "*1"					},
	{0x01, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    5, "Bonus Coins"			},
	{0x01, 0x01, 0xe0, 0x00, "None"					},
	{0x01, 0x01, 0xe0, 0x20, "3 credits/2 coins"	},
	{0x01, 0x01, 0xe0, 0x40, "5 credits/4 coins"	},
	{0x01, 0x01, 0xe0, 0x60, "6 credits/4 coins"	},
	{0x01, 0x01, 0xe0, 0x80, "6 credits/5 coins"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x10, 0x00, "On"					},
	{0x02, 0x01, 0x10, 0x10, "Off"					},

	RESO_DIP
};

STDDIPINFO(Bradley)


static UINT8 bzone_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x1820) {
		return pokey_read(0, address & 0x0f);
	}

	if ((address & 0xffe0) == 0x1860) {
		return 0; // reads a lot from here.. why? writes are mathbox_go_write (same as tempest)
	}

	switch (address)
	{
		case 0x0800: {
			UINT8 ret = DrvInputs[0] ^ 0xff;
			ret &= ~0x10;
			ret |= DrvDips[2] & 0x10; // 0x10 as a test!
			ret &= ~0x40;
			ret |= (avgdvg_done() ? 0x40 : 0);
			ret &= ~0x80;
			ret |= (M6502TotalCycles() & 0x100) ? 0x80 : 0;
			return ret;
		}

		case 0x0a00:
			return DrvDips[0];

		case 0x0c00:
			return DrvDips[1];

		case 0x1800:
			return mathbox_status_read();

		case 0x1808:
			return DrvInputs[2]; // bradley

		case 0x1809:
			return DrvInputs[3]; // bradley

		case 0x180a:
			return analog_data; // bradley

		case 0x1810:
			return mathbox_lo_read();

		case 0x1818:
			return mathbox_hi_read();
			
		case 0x1848:  // bradley (?)
		case 0x1849:
		case 0x184a:
			return 0; 
	}

	return 0;
}

static void bzone_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x1820) {
		pokey_write(0, address & 0x0f, data);
		return;
	}

	if ((address & 0xffe0) == 0x1860) {
		mathbox_go_write(address & 0x1f, data);
		return;
	}

	switch (address)
	{
		case 0x1000: 
			// coin counter
		return;

		case 0x1200:
			avgdvg_go();
		return;

		case 0x1400:
			BurnWatchdogWrite();
		return;

		case 0x1600:
			avgdvg_reset();
		return;

		case 0x1808: // bradley (?)
		return;

		case 0x1840:
			bzone_sound_write(data);
		return;

		// bradley analog select
		case 0x1848: analog_data = x_adder; break;
		case 0x1849: analog_data = y_adder; break;
		case 0x184a: analog_data = ProcessAnalog(DrvAnalogPort2, 1, 1, 0x10, 0xf0); break;

		case 0x184b:
		case 0x184c:
		case 0x184d:
		case 0x184e:
		case 0x184f:
		case 0x1850:
		return; // nop's
	}
}

static UINT8 redbaron_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x1810) {
		return pokey_read(0, address & 0x0f);
	}

	if (address >= 0x1820 && address <= 0x185f) {
		return earom_read(address - 0x1820);
	}

	if ((address & 0xffe0) == 0x1860) {
		return 0; // reads a lot from here.. why? writes are mathbox_go_write (tempest does this, too)
	}

	switch (address)
	{
		case 0x0800: {
			UINT8 ret = DrvInputs[0] ^ 0xff;
			ret &= ~0x10;
			ret |= DrvDips[2] & 0x10; // 0x10 as a test!
			ret &= ~0x40;
			ret |= (avgdvg_done() ? 0x40 : 0);
			ret &= ~0x80;
			ret |= (M6502TotalCycles() & 0x100) ? 0x80 : 0;
			return ret;
		}

		case 0x0a00:
			return DrvDips[0];

		case 0x0c00:
			return DrvDips[1];

		case 0x1800:
			return mathbox_status_read();

		case 0x1802:
			return DrvInputs[2]; // in4

		case 0x1804:
			return mathbox_lo_read();

		case 0x1806:
			return mathbox_hi_read();
	}

	return 0;
}

static void redbaron_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x1810) {
		pokey_write(0, address & 0x0f, data);
		return;
	}

	if (address >= 0x1820 && address <= 0x185f) {
		earom_write(address - 0x1820, data);
		return;
	}

	if ((address & 0xffe0) == 0x1860) {
		mathbox_go_write(address & 0x1f, data);
		return;
	}

	switch (address)
	{
		case 0x1000:
		return;

		case 0x1200:
			avgdvg_go();
		return;

		case 0x1400:
			BurnWatchdogWrite();
		return;

		case 0x1600:
			avgdvg_reset();
		return;

		case 0x1808:
			redbaron_sound_write(data);
			input_select = data & 1;
		return;

		case 0x180a:
		return; // nop

		case 0x180c:
			earom_ctrl_write(address, data);
		return;
	}
}

static void update_analog()
{
    #define XY_RATE 8
	if (x_adder != x_target) {
		if (x_adder+XY_RATE <= x_target) x_adder += XY_RATE;
		else if (x_adder+1 <= x_target) x_adder += 1;

		else if (x_adder-XY_RATE >= x_target) x_adder -= XY_RATE;
		else if (x_adder-1 >= x_target) x_adder -= 1;
	}

	if (y_adder != y_target) {
		if (y_adder+XY_RATE <= y_target) y_adder += XY_RATE;
		else if (y_adder+1 <= y_target) y_adder += 1;

		else if (y_adder-XY_RATE >= y_target) y_adder -= XY_RATE;
		else if (y_adder-1 >= y_target) y_adder -= 1;
	}
}

static INT32 bzone_port0_read(INT32 /*offset*/)
{
	return DrvInputs[1];
}

static INT32 redbaron_port0_read(INT32 /*offset*/)
{
	update_analog(); // pseudo-simulate the counters according to schematics

	INT32 analog[2] = { (y_adder-8) & 0xff, (x_adder+12) & 0xff};
	return analog[input_select];
}

static INT32 res_check()
{
#ifdef __LIBRETRO__
	return 0;
#endif
	if (DrvDips[3] & 1) {
		INT32 Width, Height;
		BurnDrvGetVisibleSize(&Width, &Height);

		if (Height != 1080) {
			vector_rescale((1080*640/480), 1080);
			return 1;
		}
	} else {
		INT32 Width, Height;
		BurnDrvGetVisibleSize(&Width, &Height);

		if (Height != 480) {
			vector_rescale(640, 480);
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

	if (redbaron) {
		redbaron_sound_reset();
	} else {
		bzone_sound_reset();
	}

	PokeyReset();

	BurnWatchdogReset();

	mathbox_reset();
	avgdvg_reset();

	earom_reset();

	HiscoreReset();

	analog_data = 0;
	nExtraCycles = 0;
	input_select = 0;

	x_target = y_target = 0x80;
	x_adder = y_adder = 0x80;

	res_check();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x008000;

	DrvPalette		= (UINT32*)Next; Next += 0x20 * 256 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x000800;
	DrvVectorRAM	= Next; Next += 0x001000;

	RamEnd			= Next;

	DrvVectorROM	= Next; Next += 0x001000; // needs to be after DrvVectorRAM(!)

	MemEnd			= Next;

	return 0;
}

// Driver "NewFrame" support
// AVG and Pokey needs linear cycles for their timers, so we use this for the
// buffered sound customs.
static INT32 drv_cycles = 0;

static INT32 DrvM6502TotalCycles()
{
	return M6502TotalCycles() - drv_cycles;
}

static void DrvM6502NewFrame()
{
	drv_cycles = M6502TotalCycles();
}

static UINT32 bzone_pix_cb(INT32 x, INT32 y, UINT32 color)
{
	const INT32 hud_end[2] = { 92, 92 * 1080 / 480 }; // hud_end[1] is 100pix scaled to the new size
	INT32 hud = hud_end[DrvDips[3] & 1];

	if (y < hud) {
		color &= 0x00ff0000;    // mask out all but red
	}
	if (y > hud) {
		color &= 0x0000ff00;    // mask out all but green
	}
	return color;
}

static INT32 BzoneInit()
{
	BurnSetRefreshRate(60.00);

	BurnAllocMemIndex();

	{
		INT32 k = 0;

		if (strstr(BurnDrvGetTextA(DRV_NAME), "bzonec")) {
			if (BurnLoadRom(DrvM6502ROM  + 0x4800,  k++, 1)) return 1;
		}

		if (BurnLoadRom(DrvM6502ROM  + 0x5000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x5800,  k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x6000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x6800,  k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x7000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x7800,  k++, 1)) return 1;

		if (BurnLoadRom(DrvVectorROM + 0x0000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x0800,  k++, 1)) return 1;
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(DrvM6502RAM,		        0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,		    0x2000, 0x2fff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,            0x3000, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x4000,	0x4000, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(bzone_write);
	M6502SetReadHandler(bzone_read);
	M6502Close();

	earom_init(); // not used in bzone

	BurnWatchdogInit(DrvDoReset, -1);

	PokeyInit(12096000/8, 2, 2.40, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, bzone_port0_read);

	bzone_sound_init(DrvM6502TotalCycles, 1512000);

	avgdvg_init(USE_AVG_BZONE, DrvVectorRAM, 0x5000, M6502TotalCycles, 580, 400);
	vector_set_pix_cb(bzone_pix_cb);

	DrvDoReset(1);

	return 0;
}

static INT32 BradleyInit()
{
	BurnSetRefreshRate(60.00);

	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM  + 0x4000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x4800,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x5000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x5800,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x6000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x6800,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x7000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x7800,  7, 1)) return 1;

		if (BurnLoadRom(DrvVectorROM + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x0800,  9, 1)) return 1;
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(DrvM6502RAM,		        0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,		    0x2000, 0x2fff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,            0x3000, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x4000,	0x4000, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(bzone_write);
	M6502SetReadHandler(bzone_read);
	M6502Close();

	earom_init(); // not used in bzone

	BurnWatchdogInit(DrvDoReset, -1);

	PokeyInit(12096000/8, 2, 2.40, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, bzone_port0_read);

	bzone_sound_init(DrvM6502TotalCycles, 1512000);

	avgdvg_init(USE_AVG_BZONE, DrvVectorRAM, 0x5000, M6502TotalCycles, 580, 400);

	bradley = 1;

	DrvDoReset(1);

	return 0;
}

static INT32 RedbaronInit()
{
	BurnSetRefreshRate(60.00);

	BurnAllocMemIndex();

	{
		if (redbarona) {
			if (BurnLoadRom(DrvM6502ROM  + 0x4800,  0, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0x5000,  1, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0x5800,  2, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0x6000,  3, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0x6800,  4, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0x7000,  5, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0x7800,  6, 1)) return 1;
			if (BurnLoadRom(DrvVectorROM + 0x0000,  7, 1)) return 1;
			if (BurnLoadRom(DrvVectorROM + 0x0800,  8, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvM6502ROM  + 0x4800,  0, 1)) return 1;
			memcpy (DrvM6502ROM + 0x5800, DrvM6502ROM + 0x5000, 0x0800);
			if (BurnLoadRom(DrvM6502ROM  + 0x5000,  1, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0x6000,  2, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0x6800,  3, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0x7000,  4, 1)) return 1;
			if (BurnLoadRom(DrvM6502ROM  + 0x7800,  5, 1)) return 1;
			if (BurnLoadRom(DrvVectorROM + 0x0000,  6, 1)) return 1;
			if (BurnLoadRom(DrvVectorROM + 0x0800,  7, 1)) return 1;
		}
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(DrvM6502RAM,		        0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,		    0x2000, 0x2fff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,            0x3000, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x4000,	0x4000, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(redbaron_write);
	M6502SetReadHandler(redbaron_read);
	M6502Close();

	earom_init();

	BurnWatchdogInit(DrvDoReset, 180); // why is this being triggered?

	PokeyInit(12096000/8, 2, 2.40, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, redbaron_port0_read);

	redbaron_sound_init(DrvM6502TotalCycles, 1512000);

	avgdvg_init(USE_AVG_RBARON, DrvVectorRAM, 0x5000, M6502TotalCycles, 520, 400);

	redbaron = 1;

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	avgdvg_exit();

	PokeyExit();
	if (redbaron) {
		redbaron_sound_exit();
		redbaron = 0;
	} else {
		bzone_sound_exit();
	}

	M6502Exit();

	earom_exit();

	BurnFreeMemIndex();

	redbarona = 0;
	bradley = 0;

	return 0;
}

static void DrvPaletteInit()
{
    for (INT32 i = 0; i < 0x20; i++) // color
	{
		for (INT32 j = 0; j < 256; j++) // intensity
		{
			INT32 r = (0xff * j) / 0xff;
			INT32 g = r;
			INT32 b = r;

			if (redbaron) {
				r = (0x27 * j) / 0xff;
				g = (0xa0 * j) / 0xff;
				b = (0xa0 * j) / 0xff;
			}

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

	vector_set_clip(0x20, nScreenWidth-0x20, 0, nScreenHeight);

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
		memset (DrvInputs, 0, 5);
		if (redbaron) DrvInputs[2] = 0x40; // active low
		if (bradley) {
			DrvInputs[2] = 0xff; // active low
			DrvInputs[3] = 0x04 + 0x08 + 0x10; // ""
		}

		// hack to map 8-ways to the 8 different combinations
		if      (DrvFakeInput[0] && DrvFakeInput[2]) { DrvJoy2[0] = 0; DrvJoy2[1] = 1; DrvJoy2[3] = 0; DrvJoy2[2] = 0; }
		else if (DrvFakeInput[0] && DrvFakeInput[3]) { DrvJoy2[3] = 1; DrvJoy2[2] = 0; DrvJoy2[0] = 0; DrvJoy2[1] = 0; }
		else if (DrvFakeInput[1] && DrvFakeInput[2]) { DrvJoy2[0] = 1; DrvJoy2[1] = 0; DrvJoy2[3] = 0; DrvJoy2[2] = 0; }
		else if (DrvFakeInput[1] && DrvFakeInput[3]) { DrvJoy2[3] = 0; DrvJoy2[2] = 1; DrvJoy2[0] = 0; DrvJoy2[1] = 0; }
		else if (DrvFakeInput[0]) { DrvJoy2[3] = 1; DrvJoy2[1] = 1; }
		else if (DrvFakeInput[1]) { DrvJoy2[2] = 1; DrvJoy2[0] = 1; }
		else if (DrvFakeInput[2]) { DrvJoy2[2] = 1; DrvJoy2[1] = 1; }
		else if (DrvFakeInput[3]) { DrvJoy2[3] = 1; DrvJoy2[0] = 1; }
		else if (DrvFakeInputPrev) { DrvJoy2[0] = DrvJoy2[1] = DrvJoy2[2] = DrvJoy2[3] = 0; }

		memcpy(&DrvFakeInputPrev, DrvFakeInput, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}

		if (redbaron) {
			//INT16 analog[2] = { DrvAnalogPort0, DrvAnalogPort1 };
			// UINT8 rc = ProcessAnalog(analog[input_select], 0, 1, 0x40, 0xc0);
			x_target = ProcessAnalog(DrvAnalogPort0, 0, 1, 0x50, 0xb0);
			y_target = ProcessAnalog(DrvAnalogPort1, 0, 1, 0x50, 0xb0);
			update_analog();
		}

		if (bradley) {
			x_target = ProcessAnalog(DrvAnalogPort0, 0, 1, 0x48, 0xc8);
			y_target = ProcessAnalog(DrvAnalogPort1, 0, 1, 0x46, 0xc6);
			update_analog();
		}
	}
	INT32 nCyclesTotal[1] = { 1512000 / ((redbaron) ? 61 : 41) };
	INT32 nInterleave = 256;
	INT32 nCyclesDone[1] = { nExtraCycles };
	INT32 nSoundBufferPos = 0;

	M6502Open(0);
	DrvM6502NewFrame(); // see comments above in DrvM6502NewFrame() for explanation.

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, M6502);
		if ((i % 64) == 63 && (DrvDips[2] & 0x10)) {
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		}

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
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

		if (redbaron) {
			redbaron_sound_update(pBurnSoundOut, nBurnSoundLen);
		} else {
			bzone_sound_update(pBurnSoundOut, nBurnSoundLen);
			if (!bzone_sound_enable)
				BurnSoundClear();
		}
		BurnSoundDCFilter();
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

		mathbox_scan(nAction, pnMin);
		avgdvg_scan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		pokey_scan(nAction, pnMin);
		redbaron_sound_scan(nAction, pnMin);
		bzone_sound_scan(nAction, pnMin);

		SCAN_VAR(nExtraCycles);
		SCAN_VAR(analog_data);
		SCAN_VAR(input_select);
		SCAN_VAR(x_target);
		SCAN_VAR(y_target);
		SCAN_VAR(x_adder);
		SCAN_VAR(y_adder);
	}

	earom_scan(nAction, pnMin); // here.

	return 0;
}


// Battle Zone (rev 2)

static struct BurnRomInfo bzoneRomDesc[] = {
	{ "036414-02.e1",	0x0800, 0x13de36d5, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "036413-01.h1",	0x0800, 0x5d9d9111, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "036412-01.j1",	0x0800, 0xab55cbd2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "036411-01.k1",	0x0800, 0xad281297, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "036410-01.lm1",	0x0800, 0x0b7bfaa4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "036409-01.n1",	0x0800, 0x1e14e919, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "036422-01.bc3",	0x0800, 0x7414177b, 2 | BRF_PRG | BRF_ESS }, //  6 Vector Data
	{ "036421-01.a3",	0x0800, 0x8ea8f939, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "036408-01.k7",	0x0100, 0x5903af03, 3 | BRF_GRA },           //  8 AVG PROM

	{ "036174-01.b1",	0x0020, 0x8b04f921, 4 | BRF_GRA },           //  9 Mathbox PROM

	{ "036175-01.m1",	0x0100, 0x2af82e87, 5 | BRF_GRA },           // 10 user3
	{ "036176-01.l1",	0x0100, 0xb31f6e24, 5 | BRF_GRA },           // 11
	{ "036177-01.k1",	0x0100, 0x8119b847, 5 | BRF_GRA },           // 12
	{ "036178-01.j1",	0x0100, 0x09f5a4d5, 5 | BRF_GRA },           // 13
	{ "036179-01.h1",	0x0100, 0x823b61ae, 5 | BRF_GRA },           // 14
	{ "036180-01.f1",	0x0100, 0x276eadd5, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(bzone)
STD_ROM_FN(bzone)

struct BurnDriver BurnDrvBzone = {
	"bzone", NULL, NULL, NULL, "1980",
	"Battle Zone (rev 2)\0", "GFX/Sound Issues", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, bzoneRomInfo, bzoneRomName, NULL, NULL, NULL, NULL, BzoneInputInfo, BzoneDIPInfo,
	BzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Battle Zone (rev 1)

static struct BurnRomInfo bzoneaRomDesc[] = {
	{ "036414-01.e1",	0x0800, 0xefbc3fa0, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "036413-01.h1",	0x0800, 0x5d9d9111, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "036412-01.j1",	0x0800, 0xab55cbd2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "036411-01.k1",	0x0800, 0xad281297, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "036410-01.lm1",	0x0800, 0x0b7bfaa4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "036409-01.n1",	0x0800, 0x1e14e919, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "036422-01.bc3",	0x0800, 0x7414177b, 1 | BRF_PRG | BRF_ESS }, //  6 Vector Data
	{ "036421-01.a3",	0x0800, 0x8ea8f939, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "036408-01.k7",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  8 AVG PROM

	{ "036174-01.b1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           //  9 Mathbox PROM

	{ "036175-01.m1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 10 user3
	{ "036176-01.l1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 11
	{ "036177-01.k1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 12
	{ "036178-01.j1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 13
	{ "036179-01.h1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 14
	{ "036180-01.f1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 15
};

STD_ROM_PICK(bzonea)
STD_ROM_FN(bzonea)

struct BurnDriver BurnDrvBzonea = {
	"bzonea", "bzone", NULL, NULL, "1980",
	"Battle Zone (rev 1)\0", "GFX/Sound Issues", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, bzoneaRomInfo, bzoneaRomName, NULL, NULL, NULL, NULL, BzoneInputInfo, BzoneDIPInfo,
	BzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Battle Zone (cocktail)

static struct BurnRomInfo bzonecRomDesc[] = {
	{ "bz1g4800",		0x0800, 0xe228dd64, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "bz1f5000",		0x0800, 0xdddfac9a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bz1e5800",		0x0800, 0x7e00e823, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bz1d6000",		0x0800, 0xc0f8c068, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bz1c6800",		0x0800, 0x5adc64bd, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bz1b7000",		0x0800, 0xed8a860e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "bz1a7800",		0x0800, 0x04babf45, 1 | BRF_PRG | BRF_ESS }, //  6
	
	{ "036422-01.bc3",	0x0800, 0x7414177b, 1 | BRF_PRG | BRF_ESS }, //  7 Vector Data
	{ "bz3b3800",		0x0800, 0x76cf57f6, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "036408-01.k7",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  9 AVG PROM

	{ "036174-01.b1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 10 Mathbox PROM

	{ "036175-01.m1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 11 user3
	{ "036176-01.l1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 12
	{ "036177-01.k1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 13
	{ "036178-01.j1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 14
	{ "036179-01.h1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 15
	{ "036180-01.f1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(bzonec)
STD_ROM_FN(bzonec)

struct BurnDriver BurnDrvBzonec = {
	"bzonec", "bzone", NULL, NULL, "1980",
	"Battle Zone (cocktail)\0", "GFX/Sound Issues", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, bzonecRomInfo, bzonecRomName, NULL, NULL, NULL, NULL, BzoneInputInfo, BzoneDIPInfo,
	BzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Bradley Trainer

static struct BurnRomInfo bradleyRomDesc[] = {
	{ "btc1.bin",		0x0800, 0x0bb8e049, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "btd1.bin",		0x0800, 0x9e0566d4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bte1.bin",		0x0800, 0x64ee6a42, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bth1.bin",		0x0800, 0xbaab67be, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "btj1.bin",		0x0800, 0x036adde4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "btk1.bin",		0x0800, 0xf5c2904e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "btlm.bin",		0x0800, 0x7d0313bf, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "btn1.bin",		0x0800, 0x182c8c64, 1 | BRF_PRG | BRF_ESS }, //  7
	
	{ "btb3.bin",		0x0800, 0x88206304, 1 | BRF_PRG | BRF_ESS }, //  8 Vector Data
	{ "bta3.bin",		0x0800, 0xd669d796, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "036408-01.k7",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 10 AVG PROM

	{ "036174-01.b1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 11 Mathbox PROM

	{ "036175-01.m1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 12 user3
	{ "036176-01.l1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 13
	{ "036177-01.k1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 14
	{ "036178-01.j1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 15
	{ "036179-01.h1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 16
	{ "036180-01.f1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 17
};

STD_ROM_PICK(bradley)
STD_ROM_FN(bradley)

struct BurnDriver BurnDrvBradley = {
	"bradley", NULL, NULL, NULL, "1980",
	"Bradley Trainer\0", "GFX/Sound Issues", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, bradleyRomInfo, bradleyRomName, NULL, NULL, NULL, NULL, BradleyInputInfo, BradleyDIPInfo,
	BradleyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Red Baron (revised hardware)

static struct BurnRomInfo redbaronRomDesc[] = {
	{ "037587-01.fh1",	0x1000, 0x60f23983, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "037000-01.e1",	0x0800, 0x69bed808, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "036998-01.j1",	0x0800, 0xd1104dd7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "036997-01.k1",	0x0800, 0x7434acb4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "036996-01.lm1",	0x0800, 0xc0e7589e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "036995-01.n1",	0x0800, 0xad81d1da, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "037006-01.bc3",	0x0800, 0x9fcffea0, 1 | BRF_PRG | BRF_ESS }, //  6 Vector Data
	{ "037007-01.a3",	0x0800, 0x60250ede, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "036408-01.k7",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  8 AVG PROM

	{ "036174-01.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           //  9 user2

	{ "036175-01.e1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 10 Mathbox PROM
	{ "036176-01.f1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 11
	{ "036177-01.h1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 12
	{ "036178-01.j1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 13
	{ "036179-01.k1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 14
	{ "036180-01.l1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 15

	{ "036464-01.a5",	0x0020, 0x42875b18, 5 | BRF_GRA },           // 16 prom
};

STD_ROM_PICK(redbaron)
STD_ROM_FN(redbaron)

struct BurnDriver BurnDrvRedbaron = {
	"redbaron", NULL, NULL, NULL, "1980",
	"Red Baron (revised hardware)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, redbaronRomInfo, redbaronRomName, NULL, NULL, NULL, NULL, RedbaronInputInfo, RedbaronDIPInfo,
	RedbaronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};

static INT32 RedbaronaInit()
{
	redbarona = 1;

	return RedbaronInit();
}

// Red Baron

static struct BurnRomInfo redbaronaRomDesc[] = {
	{ "037001-01e.e1",	0x0800, 0xb9486a6a, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "037000-01e.fh1",	0x0800, 0x69bed808, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "036999-01e.j1",	0x0800, 0x48d49819, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "036998-01e.k1",	0x0800, 0xd1104dd7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "036997-01e.lm1",	0x0800, 0x7434acb4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "036996-01e.n1",	0x0800, 0xc0e7589e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "036995-01e.p1",	0x0800, 0xad81d1da, 1 | BRF_PRG | BRF_ESS }, //  6
	
	{ "037006-01e.bc3",	0x0800, 0x9fcffea0, 1 | BRF_PRG | BRF_ESS }, //  7 Vector Data
	{ "037007-01e.a3",	0x0800, 0x60250ede, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "036408-01.k7",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  9 AVG PROM

	{ "036174-01.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 10 Mathbox PROM

	{ "036175-01.e1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 11 user3
	{ "036176-01.f1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 12
	{ "036177-01.h1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 13
	{ "036178-01.j1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 14
	{ "036179-01.k1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 15
	{ "036180-01.l1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 16

	{ "036464-01.a5",	0x0020, 0x42875b18, 5 | BRF_GRA },           // 17 Timing PROM
};

STD_ROM_PICK(redbarona)
STD_ROM_FN(redbarona)

struct BurnDriver BurnDrvRedbarona = {
	"redbarona", "redbaron", NULL, NULL, "1980",
	"Red Baron\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, redbaronaRomInfo, redbaronaRomName, NULL, NULL, NULL, NULL, RedbaronInputInfo, RedbaronDIPInfo,
	RedbaronaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};
