// FinalBurn Neo Kyugo hardware driver module
// Based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "watchdog.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvColPROM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvVidRAM[2];
static UINT8 *DrvSprRAM[2];
static UINT8 *DrvZ80RAM;

static INT32 scrollx;
static INT32 scrolly;
static INT32 fg_color;
static INT32 bg_color;
static INT32 flipscreen;
static INT32 nmi_mask;

static UINT8 *DrvColorLut;
static INT32 nGfxROMLen[3];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo KyugoInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Kyugo)

#define KyugoCommonDips()									\
	{0   , 0xfe, 0   ,    8, "Coin A"					},	\
	{0x13, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},	\
	{0x13, 0x01, 0x07, 0x01, "3 Coins 2 Credits"		},	\
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},	\
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},	\
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},	\
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},	\
	{0x13, 0x01, 0x07, 0x03, "1 Coin  6 Credits"		},	\
	{0x13, 0x01, 0x07, 0x00, "Free Play"				},	\
															\
	{0   , 0xfe, 0   ,    8, "Coin B"					},	\
	{0x13, 0x01, 0x38, 0x00, "5 Coins 1 Credits"		},	\
	{0x13, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},	\
	{0x13, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},	\
	{0x13, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},	\
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},	\
	{0x13, 0x01, 0x38, 0x20, "3 Coins 4 Credits"		},	\
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},	\
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},

static struct BurnDIPInfo GyrodineDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL						},
	{0x13, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0x03, 0x03, "3"						},
	{0x12, 0x01, 0x03, 0x02, "4"						},
	{0x12, 0x01, 0x03, 0x01, "5"						},
	{0x12, 0x01, 0x03, 0x00, "6"						},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x12, 0x01, 0x10, 0x10, "Easy"						},
	{0x12, 0x01, 0x10, 0x00, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x12, 0x01, 0x20, 0x20, "20000 50000"				},
	{0x12, 0x01, 0x20, 0x00, "40000 70000"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x12, 0x01, 0x40, 0x00, "Upright"					},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x12, 0x01, 0x80, 0x80, "Off"						},
	{0x12, 0x01, 0x80, 0x00, "On"						},

	KyugoCommonDips()
};

STDDIPINFO(Gyrodine)

static struct BurnDIPInfo RepulseDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL						},
	{0x13, 0xff, 0xff, 0xbf, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0x03, 0x03, "3"						},
	{0x12, 0x01, 0x03, 0x02, "4"						},
	{0x12, 0x01, 0x03, 0x01, "5"						},
	{0x12, 0x01, 0x03, 0x00, "6"						},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x12, 0x01, 0x04, 0x04, "Every 50000"				},
	{0x12, 0x01, 0x04, 0x00, "Every 70000"				},

	{0   , 0xfe, 0   ,    2, "Slow Motion"				},
	{0x12, 0x01, 0x08, 0x08, "Off"						},
	{0x12, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x12, 0x01, 0x10, 0x10, "Off"						},
	{0x12, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Sound Test"				},
	{0x12, 0x01, 0x20, 0x20, "Off"						},
	{0x12, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x12, 0x01, 0x40, 0x00, "Upright"					},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x12, 0x01, 0x80, 0x80, "Off"						},
	{0x12, 0x01, 0x80, 0x00, "On"						},

	KyugoCommonDips()

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x13, 0x01, 0xc0, 0xc0, "Easy"						},
	{0x13, 0x01, 0xc0, 0x80, "Normal"					},
	{0x13, 0x01, 0xc0, 0x40, "Hard"						},
	{0x13, 0x01, 0xc0, 0x00, "Hardest"					},
};

STDDIPINFO(Repulse)

static struct BurnDIPInfo AirwolfDIPList[]=
{
	{0x12, 0xff, 0xff, 0xfb, NULL						},
	{0x13, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0x03, 0x03, "4"						},
	{0x12, 0x01, 0x03, 0x02, "5"						},
	{0x12, 0x01, 0x03, 0x01, "6"						},
	{0x12, 0x01, 0x03, 0x00, "7"						},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x12, 0x01, 0x04, 0x04, "No"						},
	{0x12, 0x01, 0x04, 0x00, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Slow Motion"				},
	{0x12, 0x01, 0x08, 0x08, "Off"						},
	{0x12, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x12, 0x01, 0x10, 0x10, "Off"						},
	{0x12, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Sound Test"				},
	{0x12, 0x01, 0x20, 0x20, "Off"						},
	{0x12, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x12, 0x01, 0x40, 0x00, "Upright"					},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x12, 0x01, 0x80, 0x80, "Off"						},
	{0x12, 0x01, 0x80, 0x00, "On"						},

	KyugoCommonDips()
};

STDDIPINFO(Airwolf)

static struct BurnDIPInfo SkywolfDIPList[]=
{
	{0x12, 0xff, 0xff, 0xfb, NULL						},
	{0x13, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0x03, 0x03, "3"						},
	{0x12, 0x01, 0x03, 0x02, "4"						},
	{0x12, 0x01, 0x03, 0x01, "5"						},
	{0x12, 0x01, 0x03, 0x00, "6"						},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x12, 0x01, 0x04, 0x04, "No"						},
	{0x12, 0x01, 0x04, 0x00, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Slow Motion"				},
	{0x12, 0x01, 0x08, 0x08, "Off"						},
	{0x12, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x12, 0x01, 0x10, 0x10, "Off"						},
	{0x12, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Sound Test"				},
	{0x12, 0x01, 0x20, 0x20, "Off"						},
	{0x12, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x12, 0x01, 0x40, 0x00, "Upright"					},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x12, 0x01, 0x80, 0x80, "Off"						},
	{0x12, 0x01, 0x80, 0x00, "On"						},

	KyugoCommonDips()
};

STDDIPINFO(Skywolf)

static struct BurnDIPInfo FlashgalDIPList[]=
{
	{0x12, 0xff, 0xff, 0xef, NULL						},
	{0x13, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0x03, 0x03, "3"						},
	{0x12, 0x01, 0x03, 0x02, "4"						},
	{0x12, 0x01, 0x03, 0x01, "5"						},
	{0x12, 0x01, 0x03, 0x00, "6"						},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x12, 0x01, 0x04, 0x04, "Every 50000"				},
	{0x12, 0x01, 0x04, 0x00, "Every 70000"				},

	{0   , 0xfe, 0   ,    2, "Slow Motion"				},
	{0x12, 0x01, 0x08, 0x08, "Off"						},
	{0x12, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x12, 0x01, 0x10, 0x10, "No"						},
	{0x12, 0x01, 0x10, 0x00, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Sound Test"				},
	{0x12, 0x01, 0x20, 0x20, "Off"						},
	{0x12, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x12, 0x01, 0x40, 0x00, "Upright"					},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x12, 0x01, 0x80, 0x80, "Off"						},
	{0x12, 0x01, 0x80, 0x00, "On"						},

	KyugoCommonDips()
};

STDDIPINFO(Flashgal)

static struct BurnDIPInfo SrdmissnDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL						},
	{0x13, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0x03, 0x03, "3"						},
	{0x12, 0x01, 0x03, 0x02, "4"						},
	{0x12, 0x01, 0x03, 0x01, "5"						},
	{0x12, 0x01, 0x03, 0x00, "6"						},

	{0   , 0xfe, 0   ,    2, "Bonus Life/Continue"		},
	{0x12, 0x01, 0x04, 0x04, "Every 50000/No"			},
	{0x12, 0x01, 0x04, 0x00, "Every 70000/Yes"			},

	{0   , 0xfe, 0   ,    2, "Slow Motion"				},
	{0x12, 0x01, 0x08, 0x08, "Off"						},
	{0x12, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x12, 0x01, 0x10, 0x10, "Off"						},
	{0x12, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Sound Test"				},
	{0x12, 0x01, 0x20, 0x20, "Off"						},
	{0x12, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x12, 0x01, 0x40, 0x00, "Upright"					},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x12, 0x01, 0x80, 0x80, "Off"						},
	{0x12, 0x01, 0x80, 0x00, "On"						},

	KyugoCommonDips()
};

STDDIPINFO(Srdmissn)

static struct BurnDIPInfo LegendDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL						},
	{0x13, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0x03, 0x03, "3"						},
	{0x12, 0x01, 0x03, 0x02, "4"						},
	{0x12, 0x01, 0x03, 0x01, "5"						},
	{0x12, 0x01, 0x03, 0x00, "6"						},

	{0   , 0xfe, 0   ,    2, "Bonus Life/Continue"		},
	{0x12, 0x01, 0x04, 0x04, "Every 50000/No"			},
	{0x12, 0x01, 0x04, 0x00, "Every 70000/Yes"			},

	{0   , 0xfe, 0   ,    2, "Slow Motion"				},
	{0x12, 0x01, 0x08, 0x08, "Off"						},
	{0x12, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x12, 0x01, 0x10, 0x10, "Off"						},
	{0x12, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Sound Test"				},
	{0x12, 0x01, 0x20, 0x20, "Off"						},
	{0x12, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x12, 0x01, 0x40, 0x00, "Upright"					},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x12, 0x01, 0x80, 0x80, "Off"						},
	{0x12, 0x01, 0x80, 0x00, "On"						},

	KyugoCommonDips()
};

STDDIPINFO(Legend)

static void __fastcall kyugo_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa800:
			scrollx = (scrollx & 0x100) | data;
		return;

		case 0xb000:
			scrollx = (scrollx & 0x0ff) | ((data & 1) << 8);
			fg_color = (data >> 5) & 1;
			bg_color = (data >> 6) & 1;
		return;

		case 0xb800:
			scrolly = data;
		return;

		case 0xe000: // gyrodine
			BurnWatchdogWrite();
		return;
	}
}

static UINT8 __fastcall kyugo_main_read(UINT16 address)
{
	if ((address & 0xf800) == 0x9800) {
		return DrvSprRAM[1][address & 0x7ff] | 0xf0;
	}

	return 0;
}

static void __fastcall kyugo_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0x07)
	{
		case 0x00:
			nmi_mask = data & 1;
		return;

		case 0x01:
			flipscreen = data & 1;
		return;

		case 0x02:
			ZetSetHALT(1, (data & 1) ? 0 : 1);
		return;

		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		return;
	}
}

static UINT8 __fastcall kyugo_sub_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x02: // gyrodine, repulse
		case 0x42: // flashgala
		case 0x82: // srdmissn
			return AY8910Read(0);
	}

	return 0;
}

static UINT8 __fastcall gyrodine_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x8000: return DrvInputs[2]; // p2
		case 0x8040: return DrvInputs[1]; // p1
		case 0x8080: return DrvInputs[0]; // system
	}

	return 0;
}

static void __fastcall gyrodine_sub_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0xc0:
		case 0xc1:
			AY8910Write((port / 0x80) & 1, port & 1, data);
		return;
	}
}

static UINT8 __fastcall repulse_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000: return DrvInputs[2]; // p2
		case 0xc040: return DrvInputs[1]; // p1
		case 0xc080: return DrvInputs[0]; // system
	}

	return 0;
}

static void __fastcall repulse_sub_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x40:
		case 0x41:
			AY8910Write((port / 0x40) & 1, port & 1, data);
		return;

		case 0xc0:
		case 0xc1:
		return; // coin counter
	}
}

static UINT8 __fastcall srdmissn_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0xf400: return DrvInputs[0]; // system
		case 0xf401: return DrvInputs[1]; // p1
		case 0xF402: return DrvInputs[2]; // p2
	}

	return 0;
}

static void __fastcall srdmissin_sub_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x80:
		case 0x81:
		case 0x84:
		case 0x85:
			AY8910Write((port / 0x04) & 1, port & 1, data);
		return;

		case 0x90:
		case 0x91:
		return; // coin counter
	}
}

static UINT8 __fastcall legend_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800: return DrvInputs[0]; // system
		case 0xf801: return DrvInputs[1]; // p1
		case 0xF802: return DrvInputs[2]; // p2
	}

	return 0;
}

static UINT8 __fastcall flashgala_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0xc040: return DrvInputs[0]; // system
		case 0xc080: return DrvInputs[1]; // p1
		case 0xc0c0: return DrvInputs[2]; // p2
	}

	return 0;
}

static void __fastcall flashgala_sub_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x40:
		case 0x41:
		case 0x80:
		case 0x81:
			AY8910Write((port / 0x80) & 1, port & 1, data);
		return;

		case 0xc0:
		case 0xc1:
		return; // coin counter
	}
}

static UINT8 AY8910_0_portA(UINT32)
{
	return DrvDips[0];
}

static UINT8 AY8910_0_portB(UINT32)
{
	return DrvDips[1];
}

static tilemap_callback( fg )
{
	INT32 code = DrvVidRAM[1][offs];

	TILE_SET_INFO(0, code, fg_color + (DrvColorLut[code >> 3] << 1), 0);
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[0][0x800 + offs];
	INT32 code = DrvVidRAM[0][offs] + (attr << 8);

	TILE_SET_INFO(1, code, (attr >> 4) | (bg_color << 4), TILE_FLIPYX(attr >> 2));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetReset(0);
	ZetReset(1);
	ZetSetHALT(1, 1);

	AY8910Reset(0);
	AY8910Reset(1);

	BurnWatchdogReset();

	scrollx = 0;
	scrolly = 0;
	bg_color = 0;
	fg_color = 0;
	nmi_mask = 0;
	flipscreen = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x008000;
	DrvZ80ROM[1]		= Next; Next += 0x008000;

	DrvGfxROM[0]		= Next; Next += 0x008000;
	DrvGfxROM[1]		= Next; Next += 0x010000;
	DrvGfxROM[2]		= Next; Next += 0x040000;

	DrvColPROM			= Next; Next += 0x000300;
	DrvColorLut			= Next; Next += 0x000020;

	BurnPalette			= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam				= Next;

	DrvShareRAM			= Next; Next += 0x000800;
	DrvVidRAM[0]		= Next; Next += 0x001000;
	DrvVidRAM[1]		= Next; Next += 0x000800;
	DrvSprRAM[0]		= Next; Next += 0x000800;
	DrvSprRAM[1]		= Next; Next += 0x000800;
	DrvZ80RAM			= Next; Next += 0x000800;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *pLoad[7] = { DrvZ80ROM[0], DrvZ80ROM[1], DrvGfxROM[0], DrvGfxROM[1], DrvGfxROM[2], DrvColPROM, DrvColorLut };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if (ri.nType & 7) {
			if (BurnLoadRom(pLoad[(ri.nType & 7) - 1], i, 1)) return 1;
			pLoad[(ri.nType & 7) - 1] += ((ri.nType & 7) == 5 && ri.nLen < 0x4000) ? 0x4000 : ri.nLen;
			continue;
		}
	}

	for (INT32 i = 0; i < 3; i++) {
		nGfxROMLen[i] = pLoad[2 + i] - DrvGfxROM[i];
	}

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 0, 4 };
	INT32 Plane1[3]  = { (nGfxROMLen[1] / 3) * 8 * 0, (nGfxROMLen[1] / 3) * 8 * 1, (nGfxROMLen[1] / 3) * 8 * 2 };
	INT32 Plane2[3]  = { (nGfxROMLen[2] / 3) * 8 * 0, (nGfxROMLen[2] / 3) * 8 * 1, (nGfxROMLen[2] / 3) * 8 * 2 };
	INT32 XOffs0[8]  = { STEP4(0,1), STEP4(64,1) };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16]  = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(nGfxROMLen[2]);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], nGfxROMLen[0]);

	GfxDecode(((nGfxROMLen[0] * 8) / 2) / ( 8 *  8), 2,  8,  8, Plane0, XOffs0, YOffs, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], nGfxROMLen[1]);

	GfxDecode(((nGfxROMLen[1] * 8) / 3) / ( 8 *  8), 3,  8,  8, Plane1, XOffs1, YOffs, 0x040, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], nGfxROMLen[2]);

	GfxDecode(((nGfxROMLen[2] * 8) / 3) / (16 * 16), 3, 16, 16, Plane2, XOffs1, YOffs, 0x100, tmp, DrvGfxROM[2]);

	BurnFree(tmp);

	return 0;
}

static INT32 CommonInit(void (*romdecodecb)(), UINT16 sub_rom_offset, UINT16 share_ram_offset, UINT8 extra_ram, UINT8 (__fastcall *sub_read_cb)(UINT16), void (__fastcall *sub_writeport_cb)(UINT16,UINT8))
{
	BurnAllocMemIndex();

	{
		if (DrvLoadRoms()) return 1;

		if (romdecodecb) romdecodecb();

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],				0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM[0],				0x8000, 0x8fff, MAP_RAM); // 0-7ff bgvid, 800+ bg attr
	ZetMapMemory(DrvVidRAM[1],				0x9000, 0x97ff, MAP_RAM); // 0-7ff fgvid
	ZetMapMemory(DrvSprRAM[1],				0x9800, 0x9fff, MAP_WRITE);
	ZetMapMemory(DrvSprRAM[0],				0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,				0xf000, 0xf7ff, MAP_RAM);
	if (extra_ram) {
		ZetMapMemory(DrvShareRAM,			0xe000, 0xe7ff, MAP_RAM);
	}
	ZetSetWriteHandler(kyugo_main_write);
	ZetSetReadHandler(kyugo_main_read);
	ZetSetOutHandler(kyugo_main_write_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],				0x0000, sub_rom_offset, MAP_ROM);
	ZetMapMemory(DrvShareRAM,				share_ram_offset, share_ram_offset + 0x7ff, MAP_RAM);
	if (extra_ram) {
		ZetMapMemory(DrvZ80RAM,             0x8800, 0x8fff, MAP_RAM);
	}
	ZetSetReadHandler(sub_read_cb);
	ZetSetOutHandler(sub_writeport_cb);
	ZetSetInHandler(kyugo_sub_read_port);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	AY8910Init(0, 1536000, 0);
	AY8910Init(1, 1536000, 0);
	AY8910SetPorts(0, &AY8910_0_portA, &AY8910_0_portB, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3072000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 2,  8,  8, (nGfxROMLen[0] * 8) / 2, 0, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 3,  8,  8, (nGfxROMLen[1] * 8) / 3, 0, 0x1f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 3, 16, 16, (nGfxROMLen[2] * 8) / 3, 0, 0x1f);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(0, -32, -16, 288+32, -16);
	GenericTilemapSetOffsets(1, 0, -16, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnWatchdogExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 256; i++) {

		INT32 bit0 = (DrvColPROM[0x000 + i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[0x000 + i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[0x000 + i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[0x000 + i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[0x100 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x100 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x100 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x100 + i] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[0x200 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x200 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x200 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x200 + i] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		BurnPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_sprites()
{
	UINT8 *ram1 = DrvSprRAM[0] + 0x28;
	UINT8 *ram2 = DrvSprRAM[1] + 0x28;
	UINT8 *ram3 = DrvVidRAM[1] + 0x28;

	for (INT32 n = 0; n < 24; n++)
	{
		INT32 offs	= 2 * (n % 12) + 64 * (n / 12);
		INT32 sx	= ram3[offs + 1] + 256 * (ram2[offs + 1] & 1);
		INT32 sy	= 255 - ram1[offs] + 2;
		INT32 color = ram1[offs + 1] & 0x1f;

		if (sx > 320) sx -= 512;
		if (sy > 240) sy -= 256;

		if (flipscreen) sy = 240 - sy;

		for (INT32 y = 0; y < 16; y++)
		{
			INT32 attr = ram2[offs + 128 * y];
			INT32 code = ram3[offs + 128 * y] | ((attr & 0x01) << 9) | ((attr & 0x02) << 7);
			INT32 flipx =  attr & 0x08;
			INT32 flipy =  attr & 0x04;

			if (flipscreen)
			{
				flipx = !flipx;
				flipy = !flipy;
			}

			DrawGfxMaskTile(0, 2, code, sx, (flipscreen ? (sy - 16 * y) : (sy + 16 * y)) - 16, flipx, flipy, color, 0);
		}
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteInit();
		BurnRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? (TMAP_FLIPX | TMAP_FLIPY) : 0);
	GenericTilemapSetScrollX(0, flipscreen ? -scrollx : scrollx);
	GenericTilemapSetScrollY(0, scrolly);

	if (~nBurnLayer & 1) BurnTransferClear(0);
	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);

	if (flipscreen) {
		BurnTransferFlip(1, 1);
	}

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

    ZetNewFrame();

	{
		memset (DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == nInterleave-1 && nmi_mask) {
			ZetNmi();
		}
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if ((i % 64) == 63) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // 4x / frame
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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
		AY8910Scan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		SCAN_VAR(flipscreen);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(bg_color);
		SCAN_VAR(fg_color);
		SCAN_VAR(nmi_mask);
	}

	return 0;
}


// Gyrodine

static struct BurnRomInfo gyrodineRomDesc[] = {
	{ "rom2",			0x2000, 0x85ddea38, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a21.03",			0x2000, 0x4e9323bd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a21.04",			0x2000, 0x57e659d4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a21.05",			0x2000, 0x1e7293f3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a21.01",			0x2000, 0xb2ce0aa2, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "a21.15",			0x1000, 0xadba18d0, 3 | BRF_GRA },           //  5 Foreground Tiles

	{ "a21.08",			0x2000, 0xa57df1c9, 4 | BRF_GRA },           //  6 Background Tiles
	{ "a21.07",			0x2000, 0x63623ba3, 4 | BRF_GRA },           //  7
	{ "a21.06",			0x2000, 0x4cc969a9, 4 | BRF_GRA },           //  8

	{ "a21.14",			0x2000, 0x9c5c4d5b, 5 | BRF_GRA },           //  9 Sprites
	{ "a21.13",			0x2000, 0xd36b5aad, 5 | BRF_GRA },           // 10
	{ "a21.12",			0x2000, 0xf387aea2, 5 | BRF_GRA },           // 11
	{ "a21.11",			0x2000, 0x87967d7d, 5 | BRF_GRA },           // 12
	{ "a21.10",			0x2000, 0x59640ab4, 5 | BRF_GRA },           // 13
	{ "a21.09",			0x2000, 0x22ad88d8, 5 | BRF_GRA },           // 14

	{ "a21.16",			0x0100, 0xcc25fb56, 6 | BRF_GRA },           // 15 Color Data
	{ "a21.17",			0x0100, 0xca054448, 6 | BRF_GRA },           // 16
	{ "a21.18",			0x0100, 0x23c0c449, 6 | BRF_GRA },           // 17
	{ "a21.20",			0x0020, 0xefc4985e, 7 | BRF_GRA },           // 18 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 19
};

STD_ROM_PICK(gyrodine)
STD_ROM_FN(gyrodine)

static INT32 GyrodineInit()
{
	return CommonInit(NULL, 0x1fff, 0x4000, 0, gyrodine_sub_read, gyrodine_sub_write_port);
}

struct BurnDriver BurnDrvGyrodine = {
	"gyrodine", NULL, NULL, NULL, "1984",
	"Gyrodine\0", NULL, "Crux", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, gyrodineRomInfo, gyrodineRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, GyrodineDIPInfo,
	GyrodineInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	224, 288, 3, 4
};


// Gyrodine (Taito Corporation license)

static struct BurnRomInfo gyrodinetRomDesc[] = {
	{ "a21.02",			0x2000, 0xc5ec4a50, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a21.03",			0x2000, 0x4e9323bd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a21.04",			0x2000, 0x57e659d4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a21.05",			0x2000, 0x1e7293f3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a21.01",			0x2000, 0xb2ce0aa2, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "a21.15",			0x1000, 0xadba18d0, 3 | BRF_GRA },           //  5 Foreground Tiles

	{ "a21.08",			0x2000, 0xa57df1c9, 4 | BRF_GRA },           //  6 Background Tiles
	{ "a21.07",			0x2000, 0x63623ba3, 4 | BRF_GRA },           //  7
	{ "a21.06",			0x2000, 0x4cc969a9, 4 | BRF_GRA },           //  8

	{ "a21.14",			0x2000, 0x9c5c4d5b, 5 | BRF_GRA },           //  9 Sprites
	{ "a21.13",			0x2000, 0xd36b5aad, 5 | BRF_GRA },           // 10
	{ "a21.12",			0x2000, 0xf387aea2, 5 | BRF_GRA },           // 11
	{ "a21.11",			0x2000, 0x87967d7d, 5 | BRF_GRA },           // 12
	{ "a21.10",			0x2000, 0x59640ab4, 5 | BRF_GRA },           // 13
	{ "a21.09",			0x2000, 0x22ad88d8, 5 | BRF_GRA },           // 14

	{ "a21.16",			0x0100, 0xcc25fb56, 6 | BRF_GRA },           // 15 Color Data
	{ "a21.17",			0x0100, 0xca054448, 6 | BRF_GRA },           // 16
	{ "a21.18",			0x0100, 0x23c0c449, 6 | BRF_GRA },           // 17
	{ "a21.20",			0x0020, 0xefc4985e, 7 | BRF_GRA },           // 18 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 19
};

STD_ROM_PICK(gyrodinet)
STD_ROM_FN(gyrodinet)

struct BurnDriver BurnDrvGyrodinet = {
	"gyrodinet", "gyrodine", NULL, NULL, "1984",
	"Gyrodine (Taito Corporation license)\0", NULL, "Crux (Taito Corporation license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, gyrodinetRomInfo, gyrodinetRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, GyrodineDIPInfo,
	GyrodineInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	224, 288, 3, 4
};


// Buzzard

static struct BurnRomInfo buzzardRomDesc[] = {
	{ "rom2",			0x2000, 0x85ddea38, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a21.03",			0x2000, 0x4e9323bd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a21.04",			0x2000, 0x57e659d4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a21.05",			0x2000, 0x1e7293f3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a21.01",			0x2000, 0xb2ce0aa2, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "buzl01.bin",		0x1000, 0x65d728d0, 3 | BRF_GRA },           //  5 Foreground Tiles

	{ "a21.08",			0x2000, 0xa57df1c9, 4 | BRF_GRA },           //  6 Background Tiles
	{ "a21.07",			0x2000, 0x63623ba3, 4 | BRF_GRA },           //  7
	{ "a21.06",			0x2000, 0x4cc969a9, 4 | BRF_GRA },           //  8

	{ "a21.14",			0x2000, 0x9c5c4d5b, 5 | BRF_GRA },           //  9 Sprites
	{ "a21.13",			0x2000, 0xd36b5aad, 5 | BRF_GRA },           // 10
	{ "a21.12",			0x2000, 0xf387aea2, 5 | BRF_GRA },           // 11
	{ "a21.11",			0x2000, 0x87967d7d, 5 | BRF_GRA },           // 12
	{ "a21.10",			0x2000, 0x59640ab4, 5 | BRF_GRA },           // 13
	{ "a21.09",			0x2000, 0x22ad88d8, 5 | BRF_GRA },           // 14

	{ "a21.16",			0x0100, 0xcc25fb56, 6 | BRF_GRA },           // 15 Color Data
	{ "a21.17",			0x0100, 0xca054448, 6 | BRF_GRA },           // 16
	{ "a21.18",			0x0100, 0x23c0c449, 6 | BRF_GRA },           // 17
	{ "a21.20",			0x0020, 0xefc4985e, 7 | BRF_GRA },           // 18 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 19
};

STD_ROM_PICK(buzzard)
STD_ROM_FN(buzzard)

struct BurnDriver BurnDrvBuzzard = {
	"buzzard", "gyrodine", NULL, NULL, "1984",
	"Buzzard\0", NULL, "Crux", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, buzzardRomInfo, buzzardRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, GyrodineDIPInfo,
	GyrodineInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	224, 288, 3, 4
};


// Repulse

static struct BurnRomInfo repulseRomDesc[] = {
	{ "repulse.b5",		0x2000, 0xfb2b7c9d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "repulse.b6",		0x2000, 0x99129918, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "7.j4",			0x2000, 0x57a8e900, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1.f2",			0x2000, 0xc485c621, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "2.h2",			0x2000, 0xb3c6a886, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "3.j2",			0x2000, 0x197e314c, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "repulse.b4",		0x2000, 0x86b267f3, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "repulse.a11",	0x1000, 0x8e1de90a, 3 | BRF_GRA },           //  7 Foreground Tiles

	{ "15.9h",			0x2000, 0xc9213469, 4 | BRF_GRA },           //  8 Background Tiles
	{ "16.10h",			0x2000, 0x7de5d39e, 4 | BRF_GRA },           //  9
	{ "17.11h",			0x2000, 0x0ba5f72c, 4 | BRF_GRA },           // 10

	{ "8.6a",			0x4000, 0x0e9f757e, 5 | BRF_GRA },           // 11 Sprites
	{ "9.7a",			0x4000, 0xf7d2e650, 5 | BRF_GRA },           // 12
	{ "10.8a",			0x4000, 0xe717baf4, 5 | BRF_GRA },           // 13
	{ "11.9a",			0x4000, 0x04b2250b, 5 | BRF_GRA },           // 14
	{ "12.10a",			0x4000, 0xd110e140, 5 | BRF_GRA },           // 15
	{ "13.11a",			0x4000, 0x8fdc713c, 5 | BRF_GRA },           // 16

	{ "b.1j",			0x0100, 0x3ea35431, 6 | BRF_GRA },           // 17 Color Data
	{ "g.1h",			0x0100, 0xacd7a69e, 6 | BRF_GRA },           // 18
	{ "r.1f",			0x0100, 0xb7f48b41, 6 | BRF_GRA },           // 19
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 20 Character Color Look-up Table
};

STD_ROM_PICK(repulse)
STD_ROM_FN(repulse)

static INT32 RepulseInit()
{
	return CommonInit(NULL, 0x7fff, 0xa000, 0, repulse_sub_read, repulse_sub_write_port);
}

struct BurnDriver BurnDrvRepulse = {
	"repulse", NULL, NULL, NULL, "1985",
	"Repulse\0", NULL, "Crux / Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, repulseRomInfo, repulseRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, RepulseDIPInfo,
	RepulseInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	224, 288, 3, 4
};


// '99: The Last War (set 1)

static struct BurnRomInfo lstwar99RomDesc[] = {
	{ "1999.4f",		0x2000, 0xe3cfc09f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "1999.4h",		0x2000, 0xfd58c6e1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "7.j4",			0x2000, 0x57a8e900, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1.f2",			0x2000, 0xc485c621, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "2.h2",			0x2000, 0xb3c6a886, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "3.j2",			0x2000, 0x197e314c, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "repulse.b4",		0x2000, 0x86b267f3, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "1999.4a",		0x1000, 0x49a2383e, 3 | BRF_GRA },           //  7 Foreground Tiles

	{ "15.9h",			0x2000, 0xc9213469, 4 | BRF_GRA },           //  8 Background Tiles
	{ "16.10h",			0x2000, 0x7de5d39e, 4 | BRF_GRA },           //  9
	{ "17.11h",			0x2000, 0x0ba5f72c, 4 | BRF_GRA },           // 10

	{ "8.6a",			0x4000, 0x0e9f757e, 5 | BRF_GRA },           // 11 Sprites
	{ "9.7a",			0x4000, 0xf7d2e650, 5 | BRF_GRA },           // 12
	{ "10.8a",			0x4000, 0xe717baf4, 5 | BRF_GRA },           // 13
	{ "11.9a",			0x4000, 0x04b2250b, 5 | BRF_GRA },           // 14
	{ "12.10a",			0x4000, 0xd110e140, 5 | BRF_GRA },           // 15
	{ "13.11a",			0x4000, 0x8fdc713c, 5 | BRF_GRA },           // 16

	{ "b.1j",			0x0100, 0x3ea35431, 6 | BRF_GRA },           // 17 Color Data
	{ "g.1h",			0x0100, 0xacd7a69e, 6 | BRF_GRA },           // 18
	{ "r.1f",			0x0100, 0xb7f48b41, 6 | BRF_GRA },           // 19
	{ "n82s123n.5j",	0x0020, 0xcce2e29f, 7 | BRF_GRA },           // 20 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 21

	{ "pal12l6.4m",		0x0117, 0xb52fbcc0, 0 | BRF_OPT },           // 22 PLDs
	{ "pal12l6.5n",		0x0117, 0x453ce64a, 0 | BRF_OPT },           // 23
};

STD_ROM_PICK(lstwar99)
STD_ROM_FN(lstwar99)

struct BurnDriver BurnDrvlstwar99 = {
	"99lstwar", "repulse", NULL, NULL, "1985",
	"'99: The Last War (set 1)\0", NULL, "Crux / Proma", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, lstwar99RomInfo, lstwar99RomName, NULL, NULL, NULL, NULL, KyugoInputInfo, RepulseDIPInfo,
	RepulseInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	224, 288, 3, 4
};


// '99: The Last War (set 2)

static struct BurnRomInfo lstwar99aRomDesc[] = {
	{ "4f.bin",			0x2000, 0xefe2908d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "4h.bin",			0x2000, 0x5b79c342, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4j.bin",			0x2000, 0xd2a62c1b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "2f.bin",			0x2000, 0xcb9d8291, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "2h.bin",			0x2000, 0x24dbddc3, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "2j.bin",			0x2000, 0x16879c4c, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "repulse.b4",		0x2000, 0x86b267f3, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "1999.4a",		0x1000, 0x49a2383e, 3 | BRF_GRA },           //  7 Foreground Tiles

	{ "9h.bin",			0x2000, 0x59993c27, 4 | BRF_GRA },           //  8 Background Tiles
	{ "10h.bin",		0x2000, 0xdfbf0280, 4 | BRF_GRA },           //  9
	{ "11h.bin",		0x2000, 0xe4f29fc0, 4 | BRF_GRA },           // 10

	{ "6a.bin",			0x4000, 0x98d44410, 5 | BRF_GRA },           // 11 Sprites
	{ "7a.bin",			0x4000, 0x4c54d281, 5 | BRF_GRA },           // 12
	{ "8a.bin",			0x4000, 0x81018101, 5 | BRF_GRA },           // 13
	{ "9a.bin",			0x4000, 0x347b91fd, 5 | BRF_GRA },           // 14
	{ "10a.bin",		0x4000, 0xf07de4fa, 5 | BRF_GRA },           // 15
	{ "11a.bin",		0x4000, 0x34a04f48, 5 | BRF_GRA },           // 16

	{ "b.1j",			0x0100, 0x3ea35431, 6 | BRF_GRA },           // 17 Color Data
	{ "g.1h",			0x0100, 0xacd7a69e, 6 | BRF_GRA },           // 18
	{ "r.1f",			0x0100, 0xb7f48b41, 6 | BRF_GRA },           // 19
	{ "n82s123n.5j",	0x0020, 0xcce2e29f, 7 | BRF_GRA },           // 20 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 21

	{ "pal12l6.4m",		0x0117, 0xb52fbcc0, 0 | BRF_OPT },           // 22 PLDs
	{ "pal12l6.5n",		0x0117, 0x453ce64a, 0 | BRF_OPT },           // 23
};

STD_ROM_PICK(lstwar99a)
STD_ROM_FN(lstwar99a)

struct BurnDriver BurnDrvlstwar99a = {
	"99lstwara", "repulse", NULL, NULL, "1985",
	"'99: The Last War (set 2)\0", NULL, "Crux / Proma", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, lstwar99aRomInfo, lstwar99aRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, RepulseDIPInfo,
	RepulseInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	224, 288, 3, 4
};


// '99: The Last War (Kyugo)

static struct BurnRomInfo lstwar99kRomDesc[] = {
	{ "88.4f",			0x2000, 0xe3cfc09f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "89.4h",			0x2000, 0xfd58c6e1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "90.j4",			0x2000, 0x57a8e900, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "84.f2",			0x2000, 0xc485c621, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "85.h2",			0x2000, 0xb3c6a886, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "86.j2",			0x2000, 0x197e314c, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "87.k2",			0x2000, 0x86b267f3, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "97.4a",			0x2000, 0x15ad6867, 3 | BRF_GRA },           //  7 Foreground Tiles

	{ "98.9h",			0x2000, 0xc9213469, 4 | BRF_GRA },           //  8 Background Tiles
	{ "99.10h",			0x2000, 0x7de5d39e, 4 | BRF_GRA },           //  9
	{ "00.11h",			0x2000, 0x0ba5f72c, 4 | BRF_GRA },           // 10

	{ "91.6a",			0x4000, 0x0e9f757e, 5 | BRF_GRA },           // 11 Sprites
	{ "92.7a",			0x4000, 0xf7d2e650, 5 | BRF_GRA },           // 12
	{ "93.8a",			0x4000, 0xe717baf4, 5 | BRF_GRA },           // 13
	{ "94.9a",			0x4000, 0x04b2250b, 5 | BRF_GRA },           // 14
	{ "95.10a",			0x4000, 0xd110e140, 5 | BRF_GRA },           // 15
	{ "96.11a",			0x4000, 0x8fdc713c, 5 | BRF_GRA },           // 16

	{ "b.1j",			0x0100, 0x3ea35431, 6 | BRF_GRA },           // 17 Color Data
	{ "g.1h",			0x0100, 0xacd7a69e, 6 | BRF_GRA },           // 18
	{ "r.1f",			0x0100, 0xb7f48b41, 6 | BRF_GRA },           // 19
	{ "n82s123n.5j",	0x0020, 0xcce2e29f, 7 | BRF_GRA },           // 20 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 21

	{ "1999-00.rom",	0x0800, 0x0c0c449f, 0 | BRF_OPT },           // 22 Unknown

	{ "pal12l6.4m",		0x0117, 0xb52fbcc0, 0 | BRF_OPT },           // 23 PLDs
	{ "pal12l6.5n",		0x0117, 0x453ce64a, 0 | BRF_OPT },           // 24
};

STD_ROM_PICK(lstwar99k)
STD_ROM_FN(lstwar99k)

struct BurnDriver BurnDrv99lstwark = {
	"99lstwark", "repulse", NULL, NULL, "1985",
	"'99: The Last War (Kyugo)\0", NULL, "Crux / Kyugo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, lstwar99kRomInfo, lstwar99kRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, RepulseDIPInfo,
	RepulseInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	224, 288, 3, 4
};


// '99: The Last War (bootleg)

static struct BurnRomInfo lstwar99bRomDesc[] = {
	{ "15.2764",		0x2000, 0xf9367b9d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "16.2764",		0x2000, 0x04c3316a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "17.2764",		0x2000, 0x02aa4de5, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "11.2764",		0x2000, 0xaa3e0996, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "12.2764",		0x2000, 0xa59d3d1b, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "13.2764",		0x2000, 0xfe31975e, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "14.2764",		0x2000, 0x683481a5, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "1.2732",			0x1000, 0x8ed6855b, 3 | BRF_GRA },           //  7 Foreground Tiles

	{ "8.2764",			0x2000, 0xb161c853, 4 | BRF_GRA },           //  8 Background Tiles
	{ "9.2764",			0x2000, 0x44fd4c31, 4 | BRF_GRA },           //  9
	{ "10.2764",		0x2000, 0xb3dbc16b, 4 | BRF_GRA },           // 10

	{ "2.27128",		0x4000, 0x34dba8f9, 5 | BRF_GRA },           // 11 Sprites
	{ "3.27128",		0x4000, 0x8bd7d5b6, 5 | BRF_GRA },           // 12
	{ "4.27128",		0x4000, 0x64036ea0, 5 | BRF_GRA },           // 13
	{ "5.27128",		0x4000, 0x2f7352e4, 5 | BRF_GRA },           // 14
	{ "6.27128",		0x4000, 0x7d9e1e7e, 5 | BRF_GRA },           // 15
	{ "7.27128",		0x4000, 0x8b6fa1c4, 5 | BRF_GRA },           // 16

	{ "b.1j",			0x0100, 0x3ea35431, 6 | BRF_GRA },           // 17 Color Data
	{ "g.1h",			0x0100, 0xacd7a69e, 6 | BRF_GRA },           // 18
	{ "r.1f",			0x0100, 0xb7f48b41, 6 | BRF_GRA },           // 19
	{ "n82s123n.5j",	0x0020, 0xcce2e29f, 7 | BRF_GRA },           // 20 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 21

	{ "pal12l6.4m",		0x0117, 0xb52fbcc0, 0 | BRF_OPT },           // 22 PLDs
	{ "pal12l6.5n",		0x0117, 0x453ce64a, 0 | BRF_OPT },           // 23
};

STD_ROM_PICK(lstwar99b)
STD_ROM_FN(lstwar99b)

struct BurnDriver BurnDrvlstwar99b = {
	"99lstwarb", "repulse", NULL, NULL, "1985",
	"'99: The Last War (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, lstwar99bRomInfo, lstwar99bRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, RepulseDIPInfo,
	RepulseInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	224, 288, 3, 4
};


// Son of Phoenix (bootleg of Repulse)

static struct BurnRomInfo sonofphxRomDesc[] = {
	{ "5.f4",			0x2000, 0xe0d2c6cf, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "6.h4",			0x2000, 0x3a0d0336, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "7.j4",			0x2000, 0x57a8e900, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1.f2",			0x2000, 0xc485c621, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "2.h2",			0x2000, 0xb3c6a886, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "3.j2",			0x2000, 0x197e314c, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "4.k2",			0x2000, 0x4f3695a1, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "14.4a",			0x1000, 0xb3859b8b, 3 | BRF_GRA },           //  7 Foreground Tiles

	{ "15.9h",			0x2000, 0xc9213469, 4 | BRF_GRA },           //  8 Background Tiles
	{ "16.10h",			0x2000, 0x7de5d39e, 4 | BRF_GRA },           //  9
	{ "17.11h",			0x2000, 0x0ba5f72c, 4 | BRF_GRA },           // 10

	{ "8.6a",			0x4000, 0x0e9f757e, 5 | BRF_GRA },           // 11 Sprites
	{ "9.7a",			0x4000, 0xf7d2e650, 5 | BRF_GRA },           // 12
	{ "10.8a",			0x4000, 0xe717baf4, 5 | BRF_GRA },           // 13
	{ "11.9a",			0x4000, 0x04b2250b, 5 | BRF_GRA },           // 14
	{ "12.10a",			0x4000, 0xd110e140, 5 | BRF_GRA },           // 15
	{ "13.11a",			0x4000, 0x8fdc713c, 5 | BRF_GRA },           // 16

	{ "b.1j",			0x0100, 0x3ea35431, 6 | BRF_GRA },           // 17 Color Data
	{ "g.1h",			0x0100, 0xacd7a69e, 6 | BRF_GRA },           // 18
	{ "r.1f",			0x0100, 0xb7f48b41, 6 | BRF_GRA },           // 19
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 20 Character Color Look-up Table
};

STD_ROM_PICK(sonofphx)
STD_ROM_FN(sonofphx)

struct BurnDriver BurnDrvSonofphx = {
	"sonofphx", "repulse", NULL, NULL, "1985",
	"Son of Phoenix (bootleg of Repulse)\0", NULL, "bootleg (Associated Overseas MFR, Inc.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sonofphxRomInfo, sonofphxRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, RepulseDIPInfo,
	RepulseInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	224, 288, 3, 4
};


// Flashgal (set 1)

static struct BurnRomInfo flashgalRomDesc[] = {
	{ "epr-7167.4f",	0x2000, 0xcf5ad733, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "epr-7168.4h",	0x2000, 0x00c4851f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-7169.4j",	0x2000, 0x1ef0b8f7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-7170.4k",	0x2000, 0x885d53de, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-7163.2f",	0x2000, 0xeee2134d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "epr-7164.2h",	0x2000, 0xe5e0cd22, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "epr-7165.2j",	0x2000, 0x4cd3fe5e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "epr-7166.2k",	0x2000, 0x552ca339, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "epr-7177.4a",	0x1000, 0xdca9052f, 3 | BRF_GRA },           //  8 Foreground Tiles

	{ "epr-7178.9h",	0x2000, 0x2f5b62c0, 4 | BRF_GRA },           //  9 Background Tiles
	{ "epr-7179.10h",	0x2000, 0x8fbb49b5, 4 | BRF_GRA },           // 10
	{ "epr-7180.11h",	0x2000, 0x26a8e5c3, 4 | BRF_GRA },           // 11

	{ "epr-7171.6a",	0x4000, 0x62caf2a1, 5 | BRF_GRA },           // 12 Sprites
	{ "epr-7172.7a",	0x4000, 0x10f78a10, 5 | BRF_GRA },           // 13
	{ "epr-7173.8a",	0x4000, 0x36ea1d59, 5 | BRF_GRA },           // 14
	{ "epr-7174.9a",	0x4000, 0xf527d837, 5 | BRF_GRA },           // 15
	{ "epr-7175.10a",	0x4000, 0xba76e4c1, 5 | BRF_GRA },           // 16
	{ "epr-7176.11a",	0x4000, 0xf095d619, 5 | BRF_GRA },           // 17

	{ "7161.1j",		0x0100, 0x02c4043f, 6 | BRF_GRA },           // 18 Color Data
	{ "7160.1h",		0x0100, 0x225938d1, 6 | BRF_GRA },           // 19
	{ "7159.1f",		0x0100, 0x1e0a1cd3, 6 | BRF_GRA },           // 20
	{ "7162.5j",		0x0020, 0xcce2e29f, 7 | BRF_GRA },           // 21 Character Color Look-up Table
	{ "bpr.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 22

	{ "pal12l6.4m",		0x0117, 0xb52fbcc0, 0 | BRF_OPT },           // 23 PLDs
	{ "pal12l6.5n",		0x0117, 0x453ce64a, 0 | BRF_OPT },           // 24
};

STD_ROM_PICK(flashgal)
STD_ROM_FN(flashgal)

struct BurnDriver BurnDrvFlashgal = {
	"flashgal", NULL, NULL, NULL, "1985",
	"Flashgal (set 1)\0", NULL, "Kyugo / Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, flashgalRomInfo, flashgalRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, FlashgalDIPInfo,
	RepulseInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	288, 224, 4, 3
};


// Flashgal (set 1, Kyugo logo)

static struct BurnRomInfo flashgalkRomDesc[] = {
	{ "epr-7167.4f",	0x2000, 0xcf5ad733, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "epr-7168.4h",	0x2000, 0x00c4851f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-7169.4j",	0x2000, 0x1ef0b8f7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-7170.4k",	0x2000, 0x885d53de, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-7163.2f",	0x2000, 0xeee2134d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "epr-7164.2h",	0x2000, 0xe5e0cd22, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "epr-7165.2j",	0x2000, 0x4cd3fe5e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "epr-7166.2k",	0x2000, 0x552ca339, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "4a.bin",			0x1000, 0x83a30785, 3 | BRF_GRA },           //  8 Foreground Tiles

	{ "epr-7178.9h",	0x2000, 0x2f5b62c0, 4 | BRF_GRA },           //  9 Background Tiles
	{ "epr-7179.10h",	0x2000, 0x8fbb49b5, 4 | BRF_GRA },           // 10
	{ "epr-7180.11h",	0x2000, 0x26a8e5c3, 4 | BRF_GRA },           // 11

	{ "epr-7171.6a",	0x4000, 0x62caf2a1, 5 | BRF_GRA },           // 12 Sprites
	{ "epr-7172.7a",	0x4000, 0x10f78a10, 5 | BRF_GRA },           // 13
	{ "epr-7173.8a",	0x4000, 0x36ea1d59, 5 | BRF_GRA },           // 14
	{ "epr-7174.9a",	0x4000, 0xf527d837, 5 | BRF_GRA },           // 15
	{ "epr-7175.10a",	0x4000, 0xba76e4c1, 5 | BRF_GRA },           // 16
	{ "epr-7176.11a",	0x4000, 0xf095d619, 5 | BRF_GRA },           // 17

	{ "7161.1j",		0x0100, 0x02c4043f, 6 | BRF_GRA },           // 18 Color Data
	{ "7160.1h",		0x0100, 0x225938d1, 6 | BRF_GRA },           // 19
	{ "7159.1f",		0x0100, 0x1e0a1cd3, 6 | BRF_GRA },           // 20
	{ "7162.5j",		0x0020, 0xcce2e29f, 7 | BRF_GRA },           // 21 Character Color Look-up Table
	{ "bpr.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 22

	{ "pal12l6.4m",		0x0117, 0xb52fbcc0, 0 | BRF_OPT },           // 23 PLDs
	{ "pal12l6.5n",		0x0117, 0x453ce64a, 0 | BRF_OPT },           // 24
};

STD_ROM_PICK(flashgalk)
STD_ROM_FN(flashgalk)

struct BurnDriver BurnDrvFlashgalk = {
	"flashgalk", "flashgal", NULL, NULL, "1985",
	"Flashgal (set 1, Kyugo logo)\0", NULL, "Kyugo / Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, flashgalkRomInfo, flashgalkRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, FlashgalDIPInfo,
	RepulseInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	288, 224, 4, 3
};


// Flashgal (set 2)

static struct BurnRomInfo flashgalaRomDesc[] = {
	{ "flashgal.5",		0x2000, 0xaa889ace, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "epr-7168.4h",	0x2000, 0x00c4851f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-7169.4j",	0x2000, 0x1ef0b8f7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-7170.4k",	0x2000, 0x885d53de, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "flashgal.1",		0x2000, 0x55171cc1, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "flashgal.2",		0x2000, 0x3fd21aac, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "flashgal.3",		0x2000, 0xa1223b74, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "flashgal.4",		0x2000, 0x04d2a05f, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "epr-7177.4a",	0x1000, 0xdca9052f, 3 | BRF_GRA },           //  8 Foreground Tiles

	{ "epr-7178.9h",	0x2000, 0x2f5b62c0, 4 | BRF_GRA },           //  9 Background Tiles
	{ "epr-7179.10h",	0x2000, 0x8fbb49b5, 4 | BRF_GRA },           // 10
	{ "epr-7180.11h",	0x2000, 0x26a8e5c3, 4 | BRF_GRA },           // 11

	{ "epr-7171.6a",	0x4000, 0x62caf2a1, 5 | BRF_GRA },           // 12 Sprites
	{ "epr-7172.7a",	0x4000, 0x10f78a10, 5 | BRF_GRA },           // 13
	{ "epr-7173.8a",	0x4000, 0x36ea1d59, 5 | BRF_GRA },           // 14
	{ "epr-7174.9a",	0x4000, 0xf527d837, 5 | BRF_GRA },           // 15
	{ "epr-7175.10a",	0x4000, 0xba76e4c1, 5 | BRF_GRA },           // 16
	{ "epr-7176.11a",	0x4000, 0xf095d619, 5 | BRF_GRA },           // 17

	{ "7161.1j",		0x0100, 0x02c4043f, 6 | BRF_GRA },           // 18 Color Data
	{ "7160.1h",		0x0100, 0x225938d1, 6 | BRF_GRA },           // 19
	{ "7159.1f",		0x0100, 0x1e0a1cd3, 6 | BRF_GRA },           // 20
	{ "7162.5j",		0x0020, 0xcce2e29f, 7 | BRF_GRA },           // 21 Character Color Look-up Table
	{ "bpr.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 22

	{ "pal12l6.4m",		0x0117, 0xb52fbcc0, 0 | BRF_OPT },           // 23 PLDs
	{ "pal12l6.5n",		0x0117, 0x453ce64a, 0 | BRF_OPT },           // 24
};

STD_ROM_PICK(flashgala)
STD_ROM_FN(flashgala)

static INT32 FlashgalaInit()
{
	return CommonInit(NULL, 0x7fff, 0xe000, 0, flashgala_sub_read, flashgala_sub_write_port);
}

struct BurnDriver BurnDrvFlashgala = {
	"flashgala", "flashgal", NULL, NULL, "1985",
	"Flashgal (set 2)\0", NULL, "Kyugo / Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, flashgalaRomInfo, flashgalaRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, FlashgalDIPInfo,
	FlashgalaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	288, 224, 4, 3
};


// S.R.D. Mission

static struct BurnRomInfo srdmissnRomDesc[] = {
	{ "5.t2",			0x4000, 0xa682b48c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "7.t3",			0x4000, 0x1719c58c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.t7",			0x4000, 0xdc48595e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "3.t8",			0x4000, 0x216be1e8, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "15.4a",			0x1000, 0x4961f7fd, 3 | BRF_GRA },           //  4 Foreground Tiles

	{ "17.9h",			0x2000, 0x41211458, 4 | BRF_GRA },           //  5 Background Tiles
	{ "18.10h",			0x2000, 0x740eccd4, 4 | BRF_GRA },           //  6
	{ "16.11h",			0x2000, 0xc1f4a5db, 4 | BRF_GRA },           //  7

	{ "14.6a",			0x4000, 0x3d4c0447, 5 | BRF_GRA },           //  8 Sprites
	{ "13.7a",			0x4000, 0x22414a67, 5 | BRF_GRA },           //  9
	{ "12.8a",			0x4000, 0x61e34283, 5 | BRF_GRA },           // 10
	{ "11.9a",			0x4000, 0xbbbaffef, 5 | BRF_GRA },           // 11
	{ "10.10a",			0x4000, 0xde564f97, 5 | BRF_GRA },           // 12
	{ "9.11a",			0x4000, 0x890dc815, 5 | BRF_GRA },           // 13

	{ "mr.1j",			0x0100, 0x110a436e, 6 | BRF_GRA },           // 14 Color Data
	{ "mg.1h",			0x0100, 0x0fbfd9f0, 6 | BRF_GRA },           // 15
	{ "mb.1f",			0x0100, 0xa342890c, 6 | BRF_GRA },           // 16
	{ "m2.5j",			0x0020, 0x190a55ad, 7 | BRF_GRA },           // 17 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 18
};

STD_ROM_PICK(srdmissn)
STD_ROM_FN(srdmissn)

static INT32 SrdmissnInit()
{
	return CommonInit(NULL, 0x7fff, 0x8000, 1, srdmissn_sub_read, srdmissin_sub_write_port);
}

struct BurnDriver BurnDrvSrdmissn = {
	"srdmissn", NULL, NULL, NULL, "1986",
	"S.R.D. Mission\0", NULL, "Kyugo / Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, srdmissnRomInfo, srdmissnRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, SrdmissnDIPInfo,
	SrdmissnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	224, 288, 3, 4
};


// F-X (bootleg of S.R.D. Mission)

static struct BurnRomInfo fxRomDesc[] = {
	{ "fx.01",			0x4000, 0xb651754b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "fx.02",			0x4000, 0xf3d2dcc1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "fx.03",			0x4000, 0x8907df6b, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "fx.04",			0x4000, 0xc665834f, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "fx.05",			0x1000, 0x4a504286, 3 | BRF_GRA },           //  4 Foreground Tiles

	{ "17.9h",			0x2000, 0x41211458, 4 | BRF_GRA },           //  5 Background Tiles
	{ "18.10h",			0x2000, 0x740eccd4, 4 | BRF_GRA },           //  6
	{ "16.11h",			0x2000, 0xc1f4a5db, 4 | BRF_GRA },           //  7

	{ "14.6a",			0x4000, 0x3d4c0447, 5 | BRF_GRA },           //  8 Sprites
	{ "13.7a",			0x4000, 0x22414a67, 5 | BRF_GRA },           //  9
	{ "12.8a",			0x4000, 0x61e34283, 5 | BRF_GRA },           // 10
	{ "11.9a",			0x4000, 0xbbbaffef, 5 | BRF_GRA },           // 11
	{ "10.10a",			0x4000, 0xde564f97, 5 | BRF_GRA },           // 12
	{ "9.11a",			0x4000, 0x890dc815, 5 | BRF_GRA },           // 13

	{ "mr.1j",			0x0100, 0x110a436e, 6 | BRF_GRA },           // 14 Color Data
	{ "mg.1h",			0x0100, 0x0fbfd9f0, 6 | BRF_GRA },           // 15
	{ "mb.1f",			0x0100, 0xa342890c, 6 | BRF_GRA },           // 16
	{ "m2.5j",			0x0020, 0x190a55ad, 7 | BRF_GRA },           // 17 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 18

	{ "pal16l8.1",		0x0104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 19 PLDs
	{ "pal16l8.2",		0x0104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 20
	{ "pal16l8.3",		0x0104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 21
};

STD_ROM_PICK(fx)
STD_ROM_FN(fx)

struct BurnDriver BurnDrvFx = {
	"fx", "srdmissn", NULL, NULL, "1986",
	"F-X (bootleg of S.R.D. Mission)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, fxRomInfo, fxRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, SrdmissnDIPInfo,
	SrdmissnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	224, 288, 3, 4
};


// Airwolf

static struct BurnRomInfo airwolfRomDesc[] = {
	{ "b.2s",			0x8000, 0x8c993cce, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "a.7s",			0x8000, 0xa3c7af5c, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "f.4a",			0x1000, 0x4df44ce9, 3 | BRF_GRA },           //  2 Foreground Tiles

	{ "09h_14.bin",		0x2000, 0x25e57e1f, 4 | BRF_GRA },           //  3 Background Tiles
	{ "10h_13.bin",		0x2000, 0xcf0de5e9, 4 | BRF_GRA },           //  4
	{ "11h_12.bin",		0x2000, 0x4050c048, 4 | BRF_GRA },           //  5

	{ "e.6a",			0x8000, 0xe8fbc7d2, 5 | BRF_GRA },           //  6 Sprites
	{ "d.8a",			0x8000, 0xc5d4156b, 5 | BRF_GRA },           //  7
	{ "c.10a",			0x8000, 0xde91dfb1, 5 | BRF_GRA },           //  8

	{ "01j.bin",		0x0100, 0x6a94b2a3, 6 | BRF_GRA },           //  9 Color Data
	{ "01h.bin",		0x0100, 0xec0923d3, 6 | BRF_GRA },           // 10
	{ "01f.bin",		0x0100, 0xade97052, 6 | BRF_GRA },           // 11
	{ "74s288-2.bin",	0x0020, 0x190a55ad, 7 | BRF_GRA },           // 12 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 13

	{ "pal16l8a.2j",	0x0104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 14 PLDs
	{ "epl12p6a.9j",	0x0034, 0x19808f14, 0 | BRF_OPT },           // 15
	{ "epl12p6a.9k",	0x0034, 0xf5acad85, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(airwolf)
STD_ROM_FN(airwolf)

static void airwolf_callback()
{
	UINT8 *dst = (UINT8*)BurnMalloc(0x18000);

	for (INT32 i = 0; i < 0x18000; i++)
	{
		dst[i] = DrvGfxROM[2][(i & 0x19fff) | ((i & 0x4000) >> 1) | ((i & 0x2000) << 1)];
	}

	memcpy (DrvGfxROM[2], dst, 0x18000);

	BurnFree (dst);
}

static INT32 AirwolfInit()
{
	return CommonInit(airwolf_callback, 0x7fff, 0x8000, 1, srdmissn_sub_read, srdmissin_sub_write_port);
}

struct BurnDriver BurnDrvAirwolf = {
	"airwolf", NULL, NULL, NULL, "1987",
	"Airwolf\0", NULL, "Kyugo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, airwolfRomInfo, airwolfRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, AirwolfDIPInfo,
	AirwolfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	288, 224, 4, 3
};


// Airwolf (US)

static struct BurnRomInfo airwolfaRomDesc[] = {
	{ "airwolf.2",		0x8000, 0xbc1a8587, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "airwolf.1",		0x8000, 0xa3c7af5c, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "airwolf.6",		0x2000, 0x5b0a01e9, 3 | BRF_GRA },           //  2 Foreground Tiles

	{ "airwolf.9",		0x2000, 0x25e57e1f, 4 | BRF_GRA },           //  3 Background Tiles
	{ "airwolf.8",		0x2000, 0xcf0de5e9, 4 | BRF_GRA },           //  4
	{ "airwolf.7",		0x2000, 0x4050c048, 4 | BRF_GRA },           //  5

	{ "airwolf.5",		0x8000, 0xe8fbc7d2, 5 | BRF_GRA },           //  6 Sprites
	{ "airwolf.4",		0x8000, 0xc5d4156b, 5 | BRF_GRA },           //  7
	{ "airwolf.3",		0x8000, 0xde91dfb1, 5 | BRF_GRA },           //  8

	{ "01j.bin",		0x0100, 0x6a94b2a3, 6 | BRF_GRA },           //  9 Color Data
	{ "01h.bin",		0x0100, 0xec0923d3, 6 | BRF_GRA },           // 10
	{ "01f.bin",		0x0100, 0xade97052, 6 | BRF_GRA },           // 11
	{ "74s288-2.bin",	0x0020, 0x190a55ad, 7 | BRF_GRA },           // 12 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 13

	{ "pal16l8a.2j",	0x0104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 14 PLDs
	{ "epl12p6a.9j",	0x0034, 0x19808f14, 0 | BRF_OPT },           // 15
	{ "epl12p6a.9k",	0x0034, 0xf5acad85, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(airwolfa)
STD_ROM_FN(airwolfa)

struct BurnDriver BurnDrvAirwolfa = {
	"airwolfa", "airwolf", NULL, NULL, "1987",
	"Airwolf (US)\0", NULL, "Kyugo (United Amusements license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, airwolfaRomInfo, airwolfaRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, AirwolfDIPInfo,
	AirwolfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	288, 224, 4, 3
};


// Sky Wolf (set 1)

static struct BurnRomInfo skywolfRomDesc[] = {
	{ "02s_03.bin",		0x4000, 0xa0891798, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "03s_04.bin",		0x4000, 0x5f515d46, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "07s_01.bin",		0x4000, 0xc680a905, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "08s_02.bin",		0x4000, 0x3d66bf26, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "04a_11.bin",		0x1000, 0x219de9aa, 3 | BRF_GRA },           //  4 Foreground Tiles

	{ "09h_14.bin",		0x2000, 0x25e57e1f, 4 | BRF_GRA },           //  5 Background Tiles
	{ "10h_13.bin",		0x2000, 0xcf0de5e9, 4 | BRF_GRA },           //  6
	{ "11h_12.bin",		0x2000, 0x4050c048, 4 | BRF_GRA },           //  7

	{ "06a_10.bin",		0x4000, 0x1c809383, 5 | BRF_GRA },           //  8 Sprites
	{ "07a_09.bin",		0x4000, 0x5665d774, 5 | BRF_GRA },           //  9
	{ "08a_08.bin",		0x4000, 0x6dda8f2a, 5 | BRF_GRA },           // 10
	{ "09a_07.bin",		0x4000, 0x6a21ddb8, 5 | BRF_GRA },           // 11
	{ "10a_06.bin",		0x4000, 0xf2e548e0, 5 | BRF_GRA },           // 12
	{ "11a_05.bin",		0x4000, 0x8681b112, 5 | BRF_GRA },           // 13

	{ "01j.bin",		0x0100, 0x6a94b2a3, 6 | BRF_GRA },           // 14 Color Data
	{ "01h.bin",		0x0100, 0xec0923d3, 6 | BRF_GRA },           // 15
	{ "01f.bin",		0x0100, 0xade97052, 6 | BRF_GRA },           // 16
	{ "74s288-2.bin",	0x0020, 0x190a55ad, 7 | BRF_GRA },           // 17 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 18
};

STD_ROM_PICK(skywolf)
STD_ROM_FN(skywolf)

struct BurnDriver BurnDrvSkywolf = {
	"skywolf", "airwolf", NULL, NULL, "1987",
	"Sky Wolf (set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, skywolfRomInfo, skywolfRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, SkywolfDIPInfo,
	SrdmissnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	288, 224, 4, 3
};


// Sky Wolf (set 2)

static struct BurnRomInfo skywolf2RomDesc[] = {
	{ "z80_2.bin",		0x8000, 0x34db7bda, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "07s_01.bin",		0x4000, 0xc680a905, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code
	{ "08s_02.bin",		0x4000, 0x3d66bf26, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "04a_11.bin",		0x1000, 0x219de9aa, 3 | BRF_GRA },           //  3 Foreground Tiles

	{ "09h_14.bin",		0x2000, 0x25e57e1f, 4 | BRF_GRA },           //  4 Background Tiles
	{ "10h_13.bin",		0x2000, 0xcf0de5e9, 4 | BRF_GRA },           //  5
	{ "11h_12.bin",		0x2000, 0x4050c048, 4 | BRF_GRA },           //  6

	{ "06a_10.bin",		0x4000, 0x1c809383, 5 | BRF_GRA },           //  7 Sprites
	{ "07a_09.bin",		0x4000, 0x5665d774, 5 | BRF_GRA },           //  8
	{ "08a_08.bin",		0x4000, 0x6dda8f2a, 5 | BRF_GRA },           //  9
	{ "09a_07.bin",		0x4000, 0x6a21ddb8, 5 | BRF_GRA },           // 10
	{ "10a_06.bin",		0x4000, 0xf2e548e0, 5 | BRF_GRA },           // 11
	{ "11a_05.bin",		0x4000, 0x8681b112, 5 | BRF_GRA },           // 12

	{ "01j.bin",		0x0100, 0x6a94b2a3, 6 | BRF_GRA },           // 13 Color Data
	{ "01h.bin",		0x0100, 0xec0923d3, 6 | BRF_GRA },           // 14
	{ "01f.bin",		0x0100, 0xade97052, 6 | BRF_GRA },           // 15
	{ "74s288-2.bin",	0x0020, 0x190a55ad, 7 | BRF_GRA },           // 16 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 17
};

STD_ROM_PICK(skywolf2)
STD_ROM_FN(skywolf2)

struct BurnDriver BurnDrvSkywolf2 = {
	"skywolf2", "airwolf", NULL, NULL, "1987",
	"Sky Wolf (set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, skywolf2RomInfo, skywolf2RomName, NULL, NULL, NULL, NULL, KyugoInputInfo, AirwolfDIPInfo,
	SrdmissnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	288, 224, 4, 3
};


// Sky Wolf (set 3)

static struct BurnRomInfo skywolf3RomDesc[] = {
	{ "1.bin",			0x8000, 0x74a86ec8, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.bin",			0x8000, 0xf02143de, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "3.bin",			0x8000, 0x787cdd0a, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "4.bin",			0x8000, 0x07a2c814, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "8.bin",			0x8000, 0xb86d3dac, 3 | BRF_GRA },           //  4 Foreground Tiles

	{ "11.bin",			0x8000, 0xfc7bbf7a, 4 | BRF_GRA },           //  5 Background Tiles
	{ "10.bin",			0x8000, 0x1a3710ab, 4 | BRF_GRA },           //  6
	{ "9.bin",			0x8000, 0xa184349a, 4 | BRF_GRA },           //  7

	{ "7.bin",			0x8000, 0x086612e8, 5 | BRF_GRA },           //  8 Sprites
	{ "6.bin",			0x8000, 0x3a9beabd, 5 | BRF_GRA },           //  9
	{ "5.bin",			0x8000, 0xbd83658e, 5 | BRF_GRA },           // 10

	{ "82s129-1.bin",	0x0100, 0x6a94b2a3, 6 | BRF_GRA },           // 11 Color Data
	{ "82s129-2.bin",	0x0100, 0xec0923d3, 6 | BRF_GRA },           // 12
	{ "82s129-3.bin",	0x0100, 0xade97052, 6 | BRF_GRA },           // 13
	{ "74s288-2.bin",	0x0020, 0x190a55ad, 7 | BRF_GRA },           // 14 Character Color Look-up Table
	{ "74s288-1.bin",	0x0020, 0x5ddb2d15, 0 | BRF_OPT },           // 15

	{ "pal16l8nc.1",	0x0104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 16 PLDs
	{ "pal16l8nc.2",	0x0104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 17
	{ "pal16l8nc.3",	0x0104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 18
};

STD_ROM_PICK(skywolf3)
STD_ROM_FN(skywolf3)

static void skywolf3_callback()
{
	nGfxROMLen[0] = 0x1000;
	memcpy (DrvGfxROM[0] + 0x00000, DrvGfxROM[0] + 0x07000, 0x01000);

	nGfxROMLen[1] = 0x6000;
	memcpy (DrvGfxROM[1] + 0x02000, DrvGfxROM[1] + 0x08000, 0x02000);
	memcpy (DrvGfxROM[1] + 0x04000, DrvGfxROM[1] + 0x10000, 0x02000);
}

static INT32 Skywolf3Init()
{
	return CommonInit(skywolf3_callback, 0x7fff, 0x8000, 1, srdmissn_sub_read, srdmissin_sub_write_port);
}

struct BurnDriver BurnDrvSkywolf3 = {
	"skywolf3", "airwolf", NULL, NULL, "1987",
	"Sky Wolf (set 3)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, skywolf3RomInfo, skywolf3RomName, NULL, NULL, NULL, NULL, KyugoInputInfo, AirwolfDIPInfo,
	Skywolf3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	288, 224, 4, 3
};


// Legend

static struct BurnRomInfo legendRomDesc[] = {
	{ "a_r2.rom",		0x4000, 0x0cc1c4f4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a_r3.rom",		0x4000, 0x4b270c6b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "a_r7.rom",		0x2000, 0xabfe5eb4, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "a_r8.rom",		0x2000, 0x7e7b9ba9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "a_r9.rom",		0x2000, 0x66737f1e, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "a_n7.rom",		0x2000, 0x13915a53, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "b_a4.rom",		0x1000, 0xc7dd3cf7, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "b_h9.rom",		0x2000, 0x1fe8644a, 4 | BRF_GRA },           //  7 Background Tiles
	{ "b_h10.rom",		0x2000, 0x5f7dc82e, 4 | BRF_GRA },           //  8
	{ "b_h11.rom",		0x2000, 0x46741643, 4 | BRF_GRA },           //  9

	{ "b_a6.rom",		0x4000, 0x1689f21c, 5 | BRF_GRA },           // 10 Sprites
	{ "b_a7.rom",		0x4000, 0xf527c909, 5 | BRF_GRA },           // 11
	{ "b_a8.rom",		0x4000, 0x8d618629, 5 | BRF_GRA },           // 12
	{ "b_a9.rom",		0x4000, 0x7d7e2d55, 5 | BRF_GRA },           // 13
	{ "b_a10.rom",		0x4000, 0xf12232fe, 5 | BRF_GRA },           // 14
	{ "b_a11.rom",		0x4000, 0x8c09243d, 5 | BRF_GRA },           // 15

	{ "82s129.1j",		0x0100, 0x40590ac0, 6 | BRF_GRA },           // 16 Color Data
	{ "82s129.1h",		0x0100, 0xe542b363, 6 | BRF_GRA },           // 17
	{ "82s129.1f",		0x0100, 0x75536fc8, 6 | BRF_GRA },           // 18
	{ "82s123.5j",		0x0020, 0xc98f0651, 7 | BRF_GRA },           // 19 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 20

	{ "epl10p8.2j",		0x002c, 0x8abc03bf, 0 | BRF_OPT },           // 21 PLDs
	{ "epl12p6.9k",		0x0034, 0x9b0bd6f8, 0 | BRF_OPT },           // 22
	{ "epl12p6.9j",		0x0034, 0xdcae870d, 0 | BRF_OPT },           // 23
};

STD_ROM_PICK(legend)
STD_ROM_FN(legend)

static INT32 LegendInit()
{
	return CommonInit(NULL, 0x7fff, 0xc000, 0, legend_sub_read, srdmissin_sub_write_port);
}

struct BurnDriver BurnDrvLegend = {
	"legend", NULL, NULL, NULL, "1986",
	"Legend\0", NULL, "Kyugo / Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, legendRomInfo, legendRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, LegendDIPInfo,
	LegendInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	288, 224, 4, 3
};


// Legion (bootleg of Legend)

static struct BurnRomInfo legendbRomDesc[] = {
	{ "06.s02",			0x2000, 0x227f3e88, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "07.s03",			0x2000, 0x9352e9dc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "08.s04",			0x2000, 0x41cee2b2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "05.n03",			0x2000, 0xd8fd4e37, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "02.s07",			0x2000, 0xabfe5eb4, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "03.s08",			0x2000, 0x7e7b9ba9, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "04.s09",			0x2000, 0x0dd50aa7, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "01.n07",			0x2000, 0x13915a53, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "15.b05",			0x1000, 0x6c879f76, 3 | BRF_GRA },           //  8 Foreground Tiles

	{ "18.j09",			0x2000, 0x3bdcd028, 4 | BRF_GRA },           //  9 Background Tiles
	{ "17.j10",			0x2000, 0x105c5b53, 4 | BRF_GRA },           // 10
	{ "16.j11",			0x2000, 0xb9ca4efd, 4 | BRF_GRA },           // 11

	{ "14.b06",			0x4000, 0x1689f21c, 5 | BRF_GRA },           // 12 Sprites
	{ "13.b07",			0x4000, 0xf527c909, 5 | BRF_GRA },           // 13
	{ "12.b08",			0x4000, 0x8d618629, 5 | BRF_GRA },           // 14
	{ "11.b09",			0x4000, 0x7d7e2d55, 5 | BRF_GRA },           // 15
	{ "10.b10",			0x4000, 0xf12232fe, 5 | BRF_GRA },           // 16
	{ "09.b11",			0x4000, 0x8c09243d, 5 | BRF_GRA },           // 17

	{ "82s129.1j",		0x0100, 0x40590ac0, 6 | BRF_GRA },           // 18 Color Data
	{ "82s129.1h",		0x0100, 0xe542b363, 6 | BRF_GRA },           // 19
	{ "82s129.1f",		0x0100, 0x75536fc8, 6 | BRF_GRA },           // 20
	{ "82s123.5j",		0x0020, 0xc98f0651, 7 | BRF_GRA },           // 21 Character Color Look-up Table
	{ "m1.2c",			0x0020, 0x83a39201, 0 | BRF_OPT },           // 22

	{ "epl10p8.2j",		0x002c, 0x8abc03bf, 0 | BRF_OPT },           // 23 PLDs
	{ "epl12p6.9k",		0x0034, 0x9b0bd6f8, 0 | BRF_OPT },           // 24
	{ "epl12p6.9j",		0x0034, 0xdcae870d, 0 | BRF_OPT },           // 25
};

STD_ROM_PICK(legendb)
STD_ROM_FN(legendb)

struct BurnDriver BurnDrvLegendb = {
	"legendb", "legend", NULL, NULL, "1986",
	"Legion (bootleg of Legend)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, legendbRomInfo, legendbRomName, NULL, NULL, NULL, NULL, KyugoInputInfo, LegendDIPInfo,
	LegendInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	288, 224, 4, 3
};
