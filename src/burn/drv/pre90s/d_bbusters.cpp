// FB Alpha Beast Busters and Mechanized Attack driver module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2610.h"
#include "burn_ym2608.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvZoomTab;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvEeprom;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPfRAM0;
static UINT8 *DrvPfRAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvPfScroll0;
static UINT8 *DrvPfScroll1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 sound_status;
static UINT8 soundlatch;
static UINT8 gun_select;
static INT32 LethalGun0 = 0;
static INT32 LethalGun1 = 0;
static INT32 LethalGun2 = 0;
static INT32 LethalGun3 = 0;
static INT32 LethalGun4 = 0;
static INT32 LethalGun5 = 0;

static INT32 game_select = 0; // bbuster = 0, mechatt = 1

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[3];
static UINT8 DrvReset;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo BbustersInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &LethalGun0,    "mouse x-axis"	),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &LethalGun1,    "mouse y-axis"	),

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 2"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &LethalGun2,    "p2 x-axis"	),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &LethalGun3,    "p2 y-axis"	),

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p3 start"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p3 fire 2"	},
	A("P3 Gun X",    	BIT_ANALOG_REL, &LethalGun4,    "p2 x-axis"	),
	A("P3 Gun Y",   	 BIT_ANALOG_REL,&LethalGun5,    "p2 y-axis"	),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bbusters)

static struct BurnInputInfo Bbusters2pInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &LethalGun0,    "mouse x-axis"	),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &LethalGun1,    "mouse y-axis"	),

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 2"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &LethalGun2,    "p2 x-axis"	),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &LethalGun3,    "p2 y-axis"	),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bbusters2p)

static struct BurnInputInfo MechattInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 2"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &LethalGun0,    "mouse x-axis"	),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &LethalGun1,    "mouse y-axis"	),

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 2"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &LethalGun2,    "p2 x-axis"	),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &LethalGun3,    "p2 y-axis"	),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Mechatt)
#undef A

static struct BurnDIPInfo BbustersDIPList[]=
{
	{0x14, 0xff, 0xff, 0xf9, NULL				},
	{0x15, 0xff, 0xff, 0x8f, NULL				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x01, 0x00, "No"				},
	{0x14, 0x01, 0x01, 0x01, "Yes"				},

	{0   , 0xfe, 0   ,    4, "Magazine / Grenade"		},
	{0x14, 0x01, 0x06, 0x04, "5 / 2"			},
	{0x14, 0x01, 0x06, 0x06, "7 / 3"			},
	{0x14, 0x01, 0x06, 0x02, "9 / 4"			},
	{0x14, 0x01, 0x06, 0x00, "12 / 5"			},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x14, 0x01, 0x18, 0x00, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0x18, 0x08, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x18, 0x10, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x18, 0x18, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x14, 0x01, 0x60, 0x60, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x60, 0x40, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x60, 0x20, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x60, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x14, 0x01, 0x80, 0x80, "Common"			},
	{0x14, 0x01, 0x80, 0x00, "Individual"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x03, 0x02, "Easy"				},
	{0x15, 0x01, 0x03, 0x03, "Normal"			},
	{0x15, 0x01, 0x03, 0x01, "Hard"				},
	{0x15, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Game Mode"			},
	{0x15, 0x01, 0x0c, 0x08, "Demo Sounds Off"		},
	{0x15, 0x01, 0x0c, 0x0c, "Demo Sounds On"		},
	{0x15, 0x01, 0x0c, 0x04, "Infinite Energy (Cheat)"	},
	{0x15, 0x01, 0x0c, 0x00, "Freeze"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x15, 0x01, 0x80, 0x80, "Off"				},
	{0x15, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Bbusters)

static struct BurnDIPInfo Bbusters2pDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xf9, NULL				},
	{0x0f, 0xff, 0xff, 0x8f, NULL				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x0e, 0x01, 0x01, 0x00, "No"				},
	{0x0e, 0x01, 0x01, 0x01, "Yes"				},

	{0   , 0xfe, 0   ,    4, "Magazine / Grenade"		},
	{0x0e, 0x01, 0x06, 0x04, "5 / 2"			},
	{0x0e, 0x01, 0x06, 0x06, "7 / 3"			},
	{0x0e, 0x01, 0x06, 0x02, "9 / 4"			},
	{0x0e, 0x01, 0x06, 0x00, "12 / 5"			},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x0e, 0x01, 0x18, 0x00, "4 Coins 1 Credits"		},
	{0x0e, 0x01, 0x18, 0x08, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0x18, 0x10, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0x18, 0x18, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x0e, 0x01, 0x60, 0x60, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0x60, 0x40, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0x60, 0x20, "1 Coin  4 Credits"		},
	{0x0e, 0x01, 0x60, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x0e, 0x01, 0x80, 0x80, "Common"			},
	{0x0e, 0x01, 0x80, 0x00, "Individual"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0f, 0x01, 0x03, 0x02, "Easy"				},
	{0x0f, 0x01, 0x03, 0x03, "Normal"			},
	{0x0f, 0x01, 0x03, 0x01, "Hard"				},
	{0x0f, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Game Mode"			},
	{0x0f, 0x01, 0x0c, 0x08, "Demo Sounds Off"		},
	{0x0f, 0x01, 0x0c, 0x0c, "Demo Sounds On"		},
	{0x0f, 0x01, 0x0c, 0x04, "Infinite Energy (Cheat)"	},
	{0x0f, 0x01, 0x0c, 0x00, "Freeze"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x80, 0x80, "Off"				},
	{0x0f, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Bbusters2p)

static struct BurnDIPInfo MechattDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL				},
	{0x0f, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x0e, 0x01, 0x01, 0x01, "Common"			},
	{0x0e, 0x01, 0x01, 0x00, "Individual"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x0e, 0x01, 0x02, 0x00, "No"				},
	{0x0e, 0x01, 0x02, 0x02, "Yes"				},

	{0   , 0xfe, 0   ,    4, "Magazine / Grenade"		},
	{0x0e, 0x01, 0x0c, 0x08, "5 / 2"			},
	{0x0e, 0x01, 0x0c, 0x0c, "6 / 3"			},
	{0x0e, 0x01, 0x0c, 0x04, "7 / 4"			},
	{0x0e, 0x01, 0x0c, 0x00, "8 / 5"			},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x0e, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x0e, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x0e, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0xc0, 0x00, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0f, 0x01, 0x03, 0x02, "Easy"				},
	{0x0f, 0x01, 0x03, 0x03, "Normal"			},
	{0x0f, 0x01, 0x03, 0x01, "Hard"				},
	{0x0f, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Game Mode"			},
	{0x0f, 0x01, 0x0c, 0x08, "Demo Sounds Off"		},
	{0x0f, 0x01, 0x0c, 0x0c, "Demo Sounds On"		},
	{0x0f, 0x01, 0x0c, 0x04, "Infinite Energy (Cheat)"	},
	{0x0f, 0x01, 0x0c, 0x00, "Freeze"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x80, 0x80, "Off"				},
	{0x0f, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Mechatt)

static struct BurnDIPInfo MechattuDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL				},
	{0x0f, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x0e, 0x01, 0x01, 0x01, "Common"			},
	{0x0e, 0x01, 0x01, 0x00, "Individual"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x0e, 0x01, 0x02, 0x00, "No"				},
	{0x0e, 0x01, 0x02, 0x02, "Yes"				},

	{0   , 0xfe, 0   ,    4, "Magazine / Grenade"		},
	{0x0e, 0x01, 0x0c, 0x08, "5 / 2"			},
	{0x0e, 0x01, 0x0c, 0x0c, "6 / 3"			},
	{0x0e, 0x01, 0x0c, 0x04, "7 / 4"			},
	{0x0e, 0x01, 0x0c, 0x00, "8 / 5"			},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x0e, 0x01, 0x30, 0x00, "Free_Play"				},
	{0x0e, 0x01, 0x30, 0x10, "1 Coin/2 Credits first, then 1 Coin/1 Credit"		},
	{0x0e, 0x01, 0x30, 0x20, "2 Coins/1 Credit first, then 1 Coin/1 Credit"		},
	{0x0e, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x0e, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0xc0, 0x00, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0f, 0x01, 0x03, 0x02, "Easy"				},
	{0x0f, 0x01, 0x03, 0x03, "Normal"			},
	{0x0f, 0x01, 0x03, 0x01, "Hard"				},
	{0x0f, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Game Mode"			},
	{0x0f, 0x01, 0x0c, 0x08, "Demo Sounds Off"		},
	{0x0f, 0x01, 0x0c, 0x0c, "Demo Sounds On"		},
	{0x0f, 0x01, 0x0c, 0x04, "Infinite Energy (Cheat)"	},
	{0x0f, 0x01, 0x0c, 0x00, "Freeze"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x80, 0x80, "Off"				},
	{0x0f, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Mechattu)

static UINT16 control_3_r()
{
	UINT16 retdata = 0;
	// Dink's method of maddness for dealing with the creepage AKA. the farther the crosshair 
	// goes in a + direction, the more it creeps away from the crosshair.
	// NOTE: each gun needs different offsets _and_ has different creepage. odd?
	if (gun_select >> 1 == 0) {
		if (gun_select & 1) { // Player 1
			retdata = (BurnGunReturnX(gun_select >> 1) + 0xa0) + (BurnGunReturnX(gun_select >> 1) >> 4);
		} else {
			retdata = (BurnGunReturnY(gun_select >> 1) + 0x60+0x1a) - (BurnGunReturnY(gun_select >> 1) >> 2);
		}
	} else if (gun_select >> 1 == 1) {
		if (gun_select & 1) { // Player 2
			retdata = (BurnGunReturnX(gun_select >> 1) + 0xa0-0x1a) - (BurnGunReturnX(gun_select >> 1) >> 3);
		} else {
			retdata = (BurnGunReturnY(gun_select >> 1) + 0x60) + (0x40 - (BurnGunReturnY(gun_select >> 1) >> 2));
		}
	} else if (gun_select >> 1 == 2) {
		if (gun_select & 1) { // Player 3
			retdata = (BurnGunReturnX(gun_select >> 1) + 0xa0-0x8) - (BurnGunReturnX(gun_select >> 1) >> 5);
		} else {
			retdata = (BurnGunReturnY(gun_select >> 1) + 0x60+0x1a) + (0x40 - (BurnGunReturnY(gun_select >> 1) >> 2));
		}
	}

	return retdata >> 1;
}

static UINT16 mechatt_gun_r(UINT16 offset)
{
	INT32 x = BurnGunReturnX((offset) ? 1 : 0) + 0x18;
	INT32 y = BurnGunReturnY((offset) ? 1 : 0);

	//if (x > 0xff) x = 0xff;
	if (y > 0xef) y = 0xef;

	return x | (y << 8);
}

static UINT16 __fastcall bbusters_main_read_word(UINT32 address)
{
	if ((address & 0xffff00) == 0x0f8000) {
		return (DrvEeprom[(address >> 1) & 0x7f] | (DrvEeprom[(address >> 1) & 0x7f] << 8));
	}

	switch (address)
	{
		case 0x0e0000:
			return DrvInputs[2];

		case 0x0e0002:
			return DrvInputs[0];

		case 0x0e0004:
			return DrvInputs[1];

		case 0x0e0008:
			return DrvDips[0];

		case 0x0e000a:
			return DrvDips[1];

		case 0x0e0018:
			return sound_status;

		case 0x0e8000:
			return 0;

		case 0x0e8002:
			return control_3_r();
	}

	return 0;
}

static UINT8 __fastcall bbusters_main_read_byte(UINT32 address)
{
	if ((address & 0xffff00) == 0x0f8000) {
		return (DrvEeprom[(address >> 1) & 0x7f]);
	}

	return 0;
}

static void __fastcall bbusters_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff00) == 0x0f8000) {
		DrvEeprom[(address & 0xfe)>>1] = data;
		return;
	}

	switch (address)
	{
		case 0x0b8000:
		case 0x0b8002:
			*((UINT16*)(DrvPfScroll0 + (address & 2))) = data;
		return;

		case 0x0b8008:
		case 0x0b800a:
			*((UINT16*)(DrvPfScroll1 + (address & 2))) = data;
		return;

		case 0x0e8000: {
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
			gun_select = data & 0xff;
		}
		return;

		case 0x0f0008:
			// three_gun_output
		return;

		case 0x0f0018: {
			soundlatch = data;
			ZetNmi();
		}
		return;
	}
}

static void __fastcall bbusters_main_write_byte(UINT32 /*address*/, UINT8 /*data*/)
{
	//bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall mechatt_main_read_word(UINT32 address)
{
	//bprintf (0, _T("MRW: %5.5x\n"), address);

	switch (address)
	{
		case 0x0e0000:
			return DrvInputs[0];

		case 0x0e0002:
			return DrvDips[0] | (DrvDips[1] << 8);

		case 0x0e0004:
		case 0x0e0006:
			return mechatt_gun_r(address - 0xe0004);

		case 0x0e8000:
			return sound_status;
	}

	return 0;
}

static UINT8 __fastcall mechatt_main_read_byte(UINT32 /*address*/)
{
	//bprintf (0, _T("MRB: %5.5x\n"), address);

	return 0;
}

static void __fastcall mechatt_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xf0000) == 0xa0000) return; // nop

	switch (address)
	{
		case 0x0b8000:
		case 0x0b8002:
			*((UINT16*)(DrvPfScroll0 + (address & 2))) = data;
		return;

		case 0x0c8000:
		case 0x0c8002:
			*((UINT16*)(DrvPfScroll1 + (address & 2))) = data;
		return;

		case 0x0e4002:
			// two_gun_output
		return;

		case 0x0e8000: {
			soundlatch = data;
			ZetNmi();
		}
		return;
	}
}

static void __fastcall mechatt_main_write_byte(UINT32 /*address*/, UINT8 /*data*/)
{
	//bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static void __fastcall bbusters_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf800:
			sound_status = data;
		return;
	}
}

static UINT8 __fastcall bbusters_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			return soundlatch;
	}

	return 0;
}

static void __fastcall bbusters_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			if (game_select) {
				BurnYM2608Write(port & 3, data);
			} else {
				BurnYM2610Write(port & 3, data);
			}
		return;
	}
}

static UINT8 __fastcall bbusters_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		if (game_select) {
			return BurnYM2608Read(port & 2);
		} else {
			return BurnYM2610Read(port & 2);
		}
	}

	return 0;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	// Sometimes loading a savestate needs to kick off an irq, but since this
	// happens outside of DrvScan(), we have to handle opening of the cpu here.
	INT32 fromFMPostLoad = (ZetGetActive() == -1);

	if (fromFMPostLoad) {
		bprintf(0, _T("FM-PostLoad kicking irq!!! %X\n"), nStatus);
		ZetOpen(0);
	}

	ZetSetIRQLine(0, (nStatus) ?  CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);

	if (fromFMPostLoad)
		ZetClose();

}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 4000000;
}

static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 4000000.0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	if (game_select) {
		BurnYM2608Reset();
	} else {
		BurnYM2610Reset();
	}
	ZetClose();

	HiscoreReset();

	sound_status = 0;
	soundlatch = 0;
	gun_select = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x400000;
	DrvGfxROM2		= Next; Next += 0x400000;
	DrvGfxROM3		= Next; Next += 0x100000;
	DrvGfxROM4		= Next; Next += 0x100000;

	DrvZoomTab		= Next; Next += 0x010000;

	DrvSndROM0		= Next; Next += 0x080000;
	DrvSndROM1		= Next; Next += 0x080000;

	DrvEeprom		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	DrvZ80RAM		= Next; Next += 0x000800;

	DrvVidRAM		= Next; Next += 0x001000;
	DrvPfRAM0		= Next; Next += 0x004000;
	DrvPfRAM1		= Next; Next += 0x004000;
	DrvPalRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x010000;
	DrvSprBuf		= Next; Next += 0x002000;

	DrvPfScroll0		= Next; Next += 0x000004;
	DrvPfScroll1		= Next; Next += 0x000004;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes0[4] = { STEP4(0,1) };
	INT32 XOffs0[16] = { STEP8(0,4), STEP8(512, 4) };
	INT32 YOffs0[16] = { STEP16(0,32) };

	INT32 Planes1[4] = { 8, 12, 0, 4 };
	INT32 XOffs1[16] = { STEP4(0,1), STEP4(16,1), STEP4(256,1), STEP4(272,1) };
	INT32 YOffs1[16] = { STEP8(0,32), STEP8(512,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x020000);

	GfxDecode(((0x020000/4)*8)/( 8 *  8), 4,  8,  8, Planes0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x200000);

	GfxDecode(((0x200000/4)*8)/(16 * 16), 4, 16, 16, Planes1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x200000);

	GfxDecode(((0x200000/4)*8)/(16 * 16), 4, 16, 16, Planes1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x080000);

	GfxDecode(((0x080000/4)*8)/(16 * 16), 4, 16, 16, Planes0, XOffs0, YOffs0, 0x400, tmp, DrvGfxROM3);

	memcpy (tmp, DrvGfxROM4, 0x080000);

	GfxDecode(((0x080000/4)*8)/(16 * 16), 4, 16, 16, Planes0, XOffs0, YOffs0, 0x400, tmp, DrvGfxROM4);


	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	game_select = 0;

	BurnSetRefreshRate(56.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x180000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x180000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000, 14, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x000000, 15, 1)) return 1;

		if (BurnLoadRom(DrvZoomTab + 0x000000, 16, 1)) return 1;
	//	if (BurnLoadRom(DrvZoomTab + 0x000000, 17, 1)) return 1;
	//	if (BurnLoadRom(DrvZoomTab + 0x000000, 18, 1)) return 1;
	//	if (BurnLoadRom(DrvZoomTab + 0x000000, 19, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 20, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 21, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 22, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x080000, 0x08ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM,		0x090000, 0x090fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x0a0000, 0x0affff, MAP_RAM);
	SekMapMemory(DrvPfRAM0,		0x0b0000, 0x0b1fff, MAP_RAM);
	SekMapMemory(DrvPfRAM1,		0x0b2000, 0x0b5fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x0d0000, 0x0d0fff, MAP_RAM);
	SekSetWriteWordHandler(0,	bbusters_main_write_word);
	SekSetWriteByteHandler(0,	bbusters_main_write_byte);
	SekSetReadWordHandler(0,	bbusters_main_read_word);
	SekSetReadByteHandler(0,	bbusters_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(bbusters_sound_write);
	ZetSetReadHandler(bbusters_sound_read);
	ZetSetOutHandler(bbusters_sound_write_port);
	ZetSetInHandler(bbusters_sound_read_port);
	ZetClose();

	INT32 nSoundROMLen = 0x80000;
	BurnYM2610Init(8000000, DrvSndROM0, &nSoundROMLen, DrvSndROM1, &nSoundROMLen, &DrvFMIRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 2.00, BURN_SND_ROUTE_LEFT);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 2.00, BURN_SND_ROUTE_RIGHT);
	BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	BurnGunInit(3, true);

	DrvDoReset();

	return 0;
}

static INT32 MechattInit()
{
	game_select = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x180000,  9, 1)) return 1;

		memset (DrvGfxROM2, 0xff, 0x200000);

		if (BurnLoadRom(DrvGfxROM3 + 0x000000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x000000, 11, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 12, 1)) return 1;

		if (BurnLoadRom(DrvZoomTab + 0x000000, 13, 1)) return 1;
	//	if (BurnLoadRom(DrvZoomTab + 0x000000, 14, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1,		0x80, 1)) return 1; // ym2608 rom

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x06ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x070000, 0x07ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM,		0x090000, 0x090fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x0a0000, 0x0a0fff, MAP_RAM);
	SekMapMemory(DrvPfRAM0,		0x0b0000, 0x0b3fff, MAP_RAM);
	SekMapMemory(DrvPfRAM1,		0x0c0000, 0x0c3fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x0d0000, 0x0d07ff, MAP_RAM);
	SekSetWriteWordHandler(0,	mechatt_main_write_word);
	SekSetWriteByteHandler(0,	mechatt_main_write_byte);
	SekSetReadWordHandler(0,	mechatt_main_read_word);
	SekSetReadByteHandler(0,	mechatt_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(bbusters_sound_write);
	ZetSetReadHandler(bbusters_sound_read);
	ZetSetOutHandler(bbusters_sound_write_port);
	ZetSetInHandler(bbusters_sound_read_port);
	ZetClose();

	INT32 nSndROMLen = 0x20000;
	BurnYM2608Init(8000000, DrvSndROM0, &nSndROMLen, DrvSndROM1, &DrvFMIRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2608SetRoute(BURN_SND_YM2608_YM2608_ROUTE_1, 0.45, BURN_SND_ROUTE_BOTH);
	BurnYM2608SetRoute(BURN_SND_YM2608_YM2608_ROUTE_2, 0.45, BURN_SND_ROUTE_BOTH);
	BurnYM2608SetRoute(BURN_SND_YM2608_AY8910_ROUTE,   0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	BurnGunInit(3, true);

	DrvDoReset();

	return 0;
}

static INT32 MechattjInit()
{
	game_select = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040000,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040001,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080001, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c0000, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c0001, 13, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000, 14, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100001, 15, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x140000, 16, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x140001, 17, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x180000, 18, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x180001, 19, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1c0000, 20, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1c0001, 21, 2)) return 1;

		memset (DrvGfxROM2, 0xff, 0x200000);

		if (BurnLoadRom(DrvGfxROM3 + 0x000000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x020000, 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x040000, 24, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x060000, 25, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x000000, 26, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x020000, 27, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x040000, 28, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x060000, 29, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 30, 1)) return 1;

		if (BurnLoadRom(DrvZoomTab + 0x000000, 31, 1)) return 1;
	//	if (BurnLoadRom(DrvZoomTab + 0x000000, 32, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1,		0x80, 1)) return 1; // ym2608 rom

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x06ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x070000, 0x07ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM,		0x090000, 0x090fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x0a0000, 0x0a0fff, MAP_RAM);
	SekMapMemory(DrvPfRAM0,		0x0b0000, 0x0b3fff, MAP_RAM);
	SekMapMemory(DrvPfRAM1,		0x0c0000, 0x0c3fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x0d0000, 0x0d07ff, MAP_RAM);
	SekSetWriteWordHandler(0,	mechatt_main_write_word);
	SekSetWriteByteHandler(0,	mechatt_main_write_byte);
	SekSetReadWordHandler(0,	mechatt_main_read_word);
	SekSetReadByteHandler(0,	mechatt_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(bbusters_sound_write);
	ZetSetReadHandler(bbusters_sound_read);
	ZetSetOutHandler(bbusters_sound_write_port);
	ZetSetInHandler(bbusters_sound_read_port);
	ZetClose();

	INT32 nSndROMLen = 0x20000;
	BurnYM2608Init(8000000, DrvSndROM0, &nSndROMLen, DrvSndROM1, &DrvFMIRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2608SetRoute(BURN_SND_YM2608_YM2608_ROUTE_1, 0.45, BURN_SND_ROUTE_BOTH);
	BurnYM2608SetRoute(BURN_SND_YM2608_YM2608_ROUTE_2, 0.45, BURN_SND_ROUTE_BOTH);
	BurnYM2608SetRoute(BURN_SND_YM2608_AY8910_ROUTE,   0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	BurnGunInit(3, true);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	ZetExit();
	if (game_select) {
		BurnYM2608Exit();
	} else {
		BurnYM2610Exit();
	}
	BurnGunExit();

	BurnFree (AllMem);

	game_select = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *ram = (UINT16*)DrvPalRAM;

	for (INT32 offs = 0; offs < BurnDrvGetPaletteEntries(); offs++)
	{
		INT32 r = ram[offs] >> 12;
		INT32 g = (ram[offs] >> 8) & 0xf;
		INT32 b = (ram[offs] >> 4) & 0xf;

		DrvPalette[offs] = BurnHighCol(r*16+r, g*16+g, b*16+b, 0);
	}
}

static void draw_text_layer()
{
	UINT16 *ram = (UINT16*)DrvVidRAM;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		Render8x8Tile_Mask_Clip(pTransDraw, ram[offs] & 0xfff, sx, sy - 16, ram[offs] >> 12, 4, 0xf, 0, DrvGfxROM0);
	}
}

static void draw_layer(UINT8 *rambase, UINT8 *gfx, INT32 color_offset, INT32 transp, UINT8 *scroll, INT32 wide)
{
	UINT16 *scr = (UINT16*)scroll;
	UINT16 *ram = (UINT16*)rambase;

	INT32 width = (wide) ? 256 : 128;

	INT32 scrollx = scr[0] & ((width * 16)-1);
	INT32 scrolly = (scr[1] + 16) & 0x1ff;

	for (INT32 offs = 0; offs < width * 32; offs++)
	{
		INT32 sx = (offs / 0x20) * 16;
		INT32 sy = (offs & 0x1f) * 16;

		sx -= scrollx;
		if (sx < -15) sx += width * 16;
		sy -= scrolly;
		if (sy < -15) sy += 0x200;
		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		if (transp) {
			Render16x16Tile_Mask_Clip(pTransDraw, ram[offs] & 0xfff, sx, sy, ram[offs]>>12, 4, 0xf, color_offset, gfx);
		} else {
			Render16x16Tile_Clip(pTransDraw, ram[offs] & 0xfff, sx, sy, ram[offs]>>12, 4, color_offset, gfx);
		}
	}
}

#define ADJUST_4x4 \
		if ((dx&0x10) && (dy&0x10)) code+=3;    \
		else if (dy&0x10) code+=2;              \
		else if (dx&0x10) code+=1

#define ADJUST_8x8 \
		if ((dx&0x20) && (dy&0x20)) code+=12;   \
		else if (dy&0x20) code+=8;              \
		else if (dx&0x20) code+=4

#define ADJUST_16x16 \
		if ((dx&0x40) && (dy&0x40)) code+=48;   \
		else if (dy&0x40) code+=32;             \
		else if (dx&0x40) code+=16

inline const UINT8 *get_source_ptr(UINT8 *gfx, UINT32 sprite, INT32 dx, INT32 dy, INT32 block)
{
	int code=0;

	switch (block)
	{
		case 0:
		break;

		case 1:
			ADJUST_4x4;
		break;

		case 2:
			ADJUST_4x4;
			ADJUST_8x8;
		break;

		case 3:
			ADJUST_4x4;
			ADJUST_8x8;
			ADJUST_16x16;
		break;
	}

	return gfx + (((sprite + code) & 0x3fff) * 0x100) + ((dy & 0xf) * 0x10);
}

static void draw_block(const UINT8 *scale_table_ptr,INT32 scale_line_count,INT32 x,INT32 y,INT32 size,INT32 flipx,INT32 flipy,UINT32 sprite,INT32 color,INT32 bank,INT32 block)
{
	UINT8 *gfx = (bank == 2) ? DrvGfxROM2 : DrvGfxROM1;
	INT32 pen_base = ((bank == 2) ? 0x200 : 0x100) + ((color & 0xf) << 4);
	UINT32 xinc=(scale_line_count * 0x10000 ) / size;
	UINT8 pixel;
	INT32 x_index;
	INT32 dy=y;
	INT32 sx, ex = scale_line_count;

	while (scale_line_count) {
		if (dy>=16 && dy<240) {
			UINT16 *destline = pTransDraw + (dy - 16) * nScreenWidth;
			UINT8 srcline=*scale_table_ptr;
			const UINT8 *srcptr=NULL;

			if (!flipy)
				srcline=size-srcline-1;

			if (flipx)
				x_index=(ex-1)*0x10000;
			else
				x_index=0;

			for (sx=0; sx<size; sx++) {
				if ((sx%16)==0)
					srcptr=get_source_ptr(gfx,sprite,sx,srcline,block);

				pixel=*srcptr++;
				if (pixel!=15 && ((x+(x_index>>16)) & 0x1ff) < nScreenWidth)
					destline[(x+(x_index>>16)) & 0x1ff]= pen_base + pixel;

				if (flipx)
					x_index-=xinc;
				else
					x_index+=xinc;
			}
		}

		dy++;
		scale_table_ptr--;
		scale_line_count--;
	}
}

static void draw_sprites(UINT8 *source8, INT32 bank, INT32 colval, INT32 colmask)
{
	UINT16 *source = (UINT16*)source8;
	const UINT8 *scale_table=DrvZoomTab;

	for (INT32 offs = 0; offs < 0x800; offs += 4) {
		INT32 scale;

		INT32 sprite=source[offs+1];
		INT32 colour=source[offs+0];

		if ((colour==0xf7 || colour==0xffff || colour == 0x43f9) && (sprite==0x3fff || sprite==0xffff || sprite==0x0001))
			continue; // sprite 1, color 0x43f9 is the dead sprite in the top-right of the screen in Mechanized Attack's High Score table.

		INT16 y=source[offs+3];
		INT32 x=source[offs+2];

		if (x&0x200) x=-(0x100-(x&0xff));

		if (y > 320 || y < -256) y &= 0x1ff; // fix for bbusters ending & "Zing!" attract-mode fullscreen zombie & Helicopter on the 3rd rotation of the attractmode sequence

		colour>>=12;
		INT32 block=(source[offs+0]>>8)&0x3;
		INT32 fy=source[offs+0]&0x400;
		INT32 fx=source[offs+0]&0x800;
		sprite&=0x3fff;

		if ((colour&colmask)!=colval)
			continue;

		switch ((source[offs+0]>>8)&0x3)
		{
			case 0: {
				scale=source[offs+0]&0x7;
				const UINT8 *scale_table_ptr = scale_table+0x387f+(0x80*scale);
				INT32 scale_line_count = 0x10-scale;
				draw_block(scale_table_ptr,scale_line_count,x,y,16,fx,fy,sprite,colour,bank,block);
			}
			break;
			case 1: { // 2 x 2
				scale=source[offs+0]&0xf;
				const UINT8 *scale_table_ptr = scale_table+0x707f+(0x80*scale);
				INT32 scale_line_count = 0x20-scale;
				draw_block(scale_table_ptr,scale_line_count,x,y,32,fx,fy,sprite,colour,bank,block);
			}
			break;
			case 2: { // 64 by 64 block (2 x 2) x 2
				scale=source[offs+0]&0x1f;
				const UINT8 *scale_table_ptr = scale_table+0xa07f+(0x80*scale);
				INT32 scale_line_count = 0x40-scale;
				draw_block(scale_table_ptr,scale_line_count,x,y,64,fx,fy,sprite,colour,bank,block);
			}
			break;
			case 3: { // 2 x 2 x 2 x 2
				scale=source[offs+0]&0x3f;
				const UINT8 *scale_table_ptr = scale_table+0xc07f+(0x80*scale);
				INT32 scale_line_count = 0x80-scale;
				draw_block(scale_table_ptr,scale_line_count,x,y,128,fx,fy,sprite,colour,bank,block);
			}
			break;
		}
	}
}

static INT32 BbustersDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer(DrvPfRAM1, DrvGfxROM4, 0x500, 0, DrvPfScroll1, 0);
	if (nBurnLayer & 2) draw_layer(DrvPfRAM0, DrvGfxROM3, 0x300, 1, DrvPfScroll0, 0);
	if (nSpriteEnable & 1) draw_sprites(DrvSprBuf + 0x1000, 2, 0, 0);
	if (nSpriteEnable & 2) draw_sprites(DrvSprBuf + 0x0000, 1, 0, 0);
	if (nBurnLayer & 4) draw_text_layer();

	BurnTransferCopy(DrvPalette);

	for (INT32 i = 0; i < nBurnGunNumPlayers; i++) {
		BurnGunDrawTarget(i, BurnGunX[i] >> 8, BurnGunY[i] >> 8);
	}

	return 0;
}

static INT32 MechattDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer(DrvPfRAM1, DrvGfxROM4, 0x300, 0, DrvPfScroll1, 1);
	if (nBurnLayer & 2) draw_layer(DrvPfRAM0, DrvGfxROM3, 0x200, 1, DrvPfScroll0, 1);
	if (nSpriteEnable & 2) draw_sprites(DrvSprBuf + 0x0000, 1, 0, 0);
	if (nBurnLayer & 4) draw_text_layer();

	BurnTransferCopy(DrvPalette);

	for (INT32 i = 0; i < nBurnGunNumPlayers; i++) {
		BurnGunDrawTarget(i, BurnGunX[i] >> 8, BurnGunY[i] >> 8);
	}

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset(DrvInputs, 0xff, 3 * sizeof(INT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		BurnGunMakeInputs(0, (INT16)LethalGun0, (INT16)LethalGun1);
		BurnGunMakeInputs(1, (INT16)LethalGun2, (INT16)LethalGun3);
		BurnGunMakeInputs(2, (INT16)LethalGun4, (INT16)LethalGun5);

	}

	INT32 nInterleave = 30;
	INT32 nCyclesTotal[2] = { 12000000 / 56, 4000000 / 56 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		SekRun(nCyclesTotal[0] / nInterleave);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));
	}
	SekSetIRQLine((game_select) ? 4 : 6, CPU_IRQSTATUS_AUTO);

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		if (game_select) {
			BurnYM2608Update(pBurnSoundOut, nBurnSoundLen);
		} else {
			BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
		}
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf + 0x00000, DrvSprRAM + 0x00000, 0x01000);
	memcpy (DrvSprBuf + 0x01000, DrvSprRAM + 0x08000, 0x01000);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029719;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_DRIVER_DATA) {

		SekScan(nAction);
		ZetScan(nAction);
		BurnGunScan();

		ZetOpen(0);
		if (game_select) {
			BurnYM2608Scan(nAction, pnMin);
		} else {
			BurnYM2610Scan(nAction, pnMin);
		}
		ZetClose();

		SCAN_VAR(sound_status);
		SCAN_VAR(soundlatch);
		SCAN_VAR(gun_select);

		DrvRecalc = 1;
	}

	return 0;
}


// Beast Busters (World)

static struct BurnRomInfo bbustersRomDesc[] = {
	{ "bb-3.k10",			0x20000, 0x04da1820, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "bb-5.k12",			0x20000, 0x777e0611, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bb-2.k8",			0x20000, 0x20141805, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bb-4.k11",			0x20000, 0xd482e0e9, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bb-1.e6",			0x10000, 0x4360f2ee, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 code
	
	{ "bb-10.l9",			0x20000, 0x490c0d9b, 3 | BRF_GRA },           //  5 Characters

	{ "bb-f11.m16",			0x80000, 0x39fdf9c0, 4 | BRF_GRA },           //  6 Sprite Chip 0
	{ "bb-f12.m13",			0x80000, 0x69ee046b, 4 | BRF_GRA },           //  7
	{ "bb-f13.m12",			0x80000, 0xf5ef840e, 4 | BRF_GRA },           //  8
	{ "bb-f14.m11",			0x80000, 0x1a7df3bb, 4 | BRF_GRA },           //  9

	{ "bb-f21.l10",			0x80000, 0x530f595b, 5 | BRF_GRA },           // 10 Sprite Chip 1
	{ "bb-f22.l12",			0x80000, 0x889c562e, 5 | BRF_GRA },           // 11
	{ "bb-f23.l13",			0x80000, 0xc89fe0da, 5 | BRF_GRA },           // 12
	{ "bb-f24.l15",			0x80000, 0xe0d81359, 5 | BRF_GRA },           // 13

	{ "bb-back1.m4",		0x80000, 0xb5445313, 6 | BRF_GRA },           // 14 Background Tiles

	{ "bb-back2.m6",		0x80000, 0x8be996f6, 7 | BRF_GRA },           // 15 Foreground Tiles

	{ "bb-6.e7",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 16 Zoom Tables
	{ "bb-7.h7",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 17
	{ "bb-8.a14",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 18
	{ "bb-9.c14",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 19

	{ "bb-pcma.l5",			0x80000, 0x44cd5bfe, 9 | BRF_GRA },           // 20 YM2610 Samples

	{ "bb-pcmb.l3",			0x80000, 0xc8d5dd53, 10 | BRF_GRA },          // 21 YM2610 Deltat Samples

	{ "bbusters-eeprom.bin",0x00100, 0xa52ebd66, 11 | BRF_GRA },          // 22 EEPROM
};

STD_ROM_PICK(bbusters)
STD_ROM_FN(bbusters)

struct BurnDriver BurnDrvBbusters = {
	"bbusters", NULL, NULL, NULL, "1989",
	"Beast Busters (World)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, bbustersRomInfo, bbustersRomName, NULL, NULL, BbustersInputInfo, BbustersDIPInfo,
	DrvInit, DrvExit, DrvFrame, BbustersDraw, DrvScan, &DrvRecalc, 0x600,
	256, 224, 4, 3
};

// Beast Busters (US, Version 3)

static struct BurnRomInfo bbustersuRomDesc[] = {
	{ "bb-ver3-u3.k10",		0x20000, 0xc80ec3bc, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "bb-ver3-u5.k12",		0x20000, 0x5ded86d1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bb-2.k8",			0x20000, 0x20141805, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bb-4.k11",			0x20000, 0xd482e0e9, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bb-1.e6",			0x10000, 0x4360f2ee, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 code

	{ "bb-10.l9",			0x20000, 0x490c0d9b, 3 | BRF_GRA },           //  5 Characters

	{ "bb-f11.m16",			0x80000, 0x39fdf9c0, 4 | BRF_GRA },           //  6 Sprite Chip 0
	{ "bb-f12.m13",			0x80000, 0x69ee046b, 4 | BRF_GRA },           //  7
	{ "bb-f13.m12",			0x80000, 0xf5ef840e, 4 | BRF_GRA },           //  8
	{ "bb-f14.m11",			0x80000, 0x1a7df3bb, 4 | BRF_GRA },           //  9

	{ "bb-f21.l10",			0x80000, 0x530f595b, 5 | BRF_GRA },           // 10 Sprite Chip 1
	{ "bb-f22.l12",			0x80000, 0x889c562e, 5 | BRF_GRA },           // 11
	{ "bb-f23.l13",			0x80000, 0xc89fe0da, 5 | BRF_GRA },           // 12
	{ "bb-f24.l15",			0x80000, 0xe0d81359, 5 | BRF_GRA },           // 13

	{ "bb-back1.m4",		0x80000, 0xb5445313, 6 | BRF_GRA },           // 14 Background Tiles

	{ "bb-back2.m6",		0x80000, 0x8be996f6, 7 | BRF_GRA },           // 15 Foreground Tiles

	{ "bb-6.e7",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 16 Zoom Tables
	{ "bb-7.h7",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 17
	{ "bb-8.a14",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 18
	{ "bb-9.c14",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 19

	{ "bb-pcma.l5",			0x80000, 0x44cd5bfe, 9 | BRF_GRA },           // 20 YM2610 Samples

	{ "bb-pcma.l5",			0x80000, 0x44cd5bfe, 10 | BRF_GRA },          // 21 YM2610 Deltat Samples

	{ "bbusters-eeprom.bin",0x00100, 0xa52ebd66, 11 | BRF_GRA },          // 22 EEPROM
};

STD_ROM_PICK(bbustersu)
STD_ROM_FN(bbustersu)

struct BurnDriver BurnDrvBbustersu = {
	"bbustersu", "bbusters", NULL, NULL, "1989",
	"Beast Busters (US, Version 3)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, bbustersuRomInfo, bbustersuRomName, NULL, NULL, BbustersInputInfo, BbustersDIPInfo,
	DrvInit, DrvExit, DrvFrame, BbustersDraw, DrvScan, &DrvRecalc, 0x600,
	256, 224, 4, 3
};

// Beast Busters (Japan, Version 2)

static struct BurnRomInfo bbustersjRomDesc[] = {
	{ "bb3_ver2_j2.k10",	0x20000, 0x605eb62f, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "bb5_ver2_j2.k12",	0x20000, 0x9deea26f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bb-2.k8",			0x20000, 0x20141805, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bb-4.k11",			0x20000, 0xd482e0e9, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bb-1.e6",			0x10000, 0x4360f2ee, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 code

	{ "bb-10.l9",			0x20000, 0x490c0d9b, 3 | BRF_GRA },           //  5 Characters

	{ "bb-f11.m16",			0x80000, 0x39fdf9c0, 4 | BRF_GRA },           //  6 Sprite Chip 0
	{ "bb-f12.m13",			0x80000, 0x69ee046b, 4 | BRF_GRA },           //  7
	{ "bb-f13.m12",			0x80000, 0xf5ef840e, 4 | BRF_GRA },           //  8
	{ "bb-f14.m11",			0x80000, 0x1a7df3bb, 4 | BRF_GRA },           //  9

	{ "bb-f21.l10",			0x80000, 0x530f595b, 5 | BRF_GRA },           // 10 Sprite Chip 1
	{ "bb-f22.l12",			0x80000, 0x889c562e, 5 | BRF_GRA },           // 11
	{ "bb-f23.l13",			0x80000, 0xc89fe0da, 5 | BRF_GRA },           // 12
	{ "bb-f24.l15",			0x80000, 0xe0d81359, 5 | BRF_GRA },           // 13

	{ "bb-back1.m4",		0x80000, 0xb5445313, 6 | BRF_GRA },           // 14 Background Tiles

	{ "bb-back2.m6",		0x80000, 0x8be996f6, 7 | BRF_GRA },           // 15 Foreground Tiles

	{ "bb-6.e7",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 16 Zoom Tables
	{ "bb-7.h7",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 17
	{ "bb-8.a14",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 18
	{ "bb-9.c14",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 19

	{ "bb-pcma.l5",			0x80000, 0x44cd5bfe, 9 | BRF_GRA },           // 20 YM2610 Samples

	{ "bb-pcmb.l3",			0x80000, 0xc8d5dd53, 10 | BRF_GRA },          // 21 YM2610 Deltat Samples

	{ "bbusters-eeprom.bin",0x00100, 0xa52ebd66, 11 | BRF_GRA },          // 22 EEPROM
};

STD_ROM_PICK(bbustersj)
STD_ROM_FN(bbustersj)

struct BurnDriver BurnDrvBbustersj = {
	"bbustersj", "bbusters", NULL, NULL, "1989",
	"Beast Busters (Japan, Version 2)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, bbustersjRomInfo, bbustersjRomName, NULL, NULL, Bbusters2pInputInfo, Bbusters2pDIPInfo,
	DrvInit, DrvExit, DrvFrame, BbustersDraw, DrvScan, &DrvRecalc, 0x600,
	256, 224, 4, 3
};

// Beast Busters (US, Version 2)

static struct BurnRomInfo bbustersuaRomDesc[] = {
	{ "bb-ver2-u3.k10",		0x20000, 0x6930088b, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "bb-ver2-u5.k12",		0x20000, 0xcfdb2c6c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bb-2.k8",			0x20000, 0x20141805, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bb-4.k11",			0x20000, 0xd482e0e9, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bb-1.e6",			0x10000, 0x4360f2ee, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 code

	{ "bb-10.l9",			0x20000, 0x490c0d9b, 3 | BRF_GRA },           //  5 Characters

	{ "bb-f11.m16",			0x80000, 0x39fdf9c0, 4 | BRF_GRA },           //  6 Sprite Chip 0
	{ "bb-f12.m13",			0x80000, 0x69ee046b, 4 | BRF_GRA },           //  7
	{ "bb-f13.m12",			0x80000, 0xf5ef840e, 4 | BRF_GRA },           //  8
	{ "bb-f14.m11",			0x80000, 0x1a7df3bb, 4 | BRF_GRA },           //  9

	{ "bb-f21.l10",			0x80000, 0x530f595b, 5 | BRF_GRA },           // 10 Sprite Chip 1
	{ "bb-f22.l12",			0x80000, 0x889c562e, 5 | BRF_GRA },           // 11
	{ "bb-f23.l13",			0x80000, 0xc89fe0da, 5 | BRF_GRA },           // 12
	{ "bb-f24.l15",			0x80000, 0xe0d81359, 5 | BRF_GRA },           // 13

	{ "bb-back1.m4",		0x80000, 0xb5445313, 6 | BRF_GRA },           // 14 Background Tiles

	{ "bb-back2.m6",		0x80000, 0x8be996f6, 7 | BRF_GRA },           // 15 Foreground Tiles

	{ "bb-6.e7",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 16 Zoom Tables
	{ "bb-7.h7",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 17
	{ "bb-8.a14",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 18
	{ "bb-9.c14",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 19

	{ "bb-pcma.l5",			0x80000, 0x44cd5bfe, 9 | BRF_GRA },           // 20 YM2610 Samples

	{ "bb-pcma.l5",			0x80000, 0x44cd5bfe, 10 | BRF_GRA },          // 21 YM2610 Deltat Samples

	{ "bbusters-eeprom.bin",0x00100, 0xa52ebd66, 11 | BRF_GRA },          // 22 EEPROM
};

STD_ROM_PICK(bbustersua)
STD_ROM_FN(bbustersua)

struct BurnDriver BurnDrvBbustersua = {
	"bbustersua", "bbusters", NULL, NULL, "1989",
	"Beast Busters (US, Version 2)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, bbustersuaRomInfo, bbustersuaRomName, NULL, NULL, BbustersInputInfo, BbustersDIPInfo,
	DrvInit, DrvExit, DrvFrame, BbustersDraw, DrvScan, &DrvRecalc, 0x600,
	256, 224, 4, 3
};

// YM2608 ROM for Mechanized Attack
static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

static struct BurnRomInfo Ym2608RomDesc[] = {
#if !defined (ROM_VERIFY)
	{ "ym2608_adpcm_rom.bin",  0x002000, 0x23c9e0d8, BRF_ESS | BRF_PRG | BRF_BIOS },
#else
	{ "",  0x000000, 0x00000000, BRF_ESS | BRF_PRG | BRF_BIOS },
#endif
};

// Mechanized Attack (World)

static struct BurnRomInfo mechattRomDesc[] = {
	{ "ma5-e.n12",			0x20000, 0x9bbb852a, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ma4.l12",			0x20000, 0x0d414918, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ma7.n13",			0x20000, 0x61d85e1b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ma6-f.l13",			0x20000, 0x4055fe8d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ma_3.e13",			0x10000, 0xc06cc8e1, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "ma_1.l2",			0x10000, 0x24766917, 3 | BRF_GRA },           //  5 Characters

	{ "mao89p13.bin",		0x80000, 0x8bcb16cf, 4 | BRF_GRA },           //  6 Sprites
	{ "ma189p15.bin",		0x80000, 0xb84d9658, 4 | BRF_GRA },           //  7
	{ "ma289p17.bin",		0x80000, 0x6cbe08ac, 4 | BRF_GRA },           //  8
	{ "ma389m15.bin",		0x80000, 0x34d4585e, 4 | BRF_GRA },           //  9

	{ "mab189a2.bin",		0x80000, 0xe1c8b4d0, 5 | BRF_GRA },           // 10 Background Tiles

	{ "mab289c2.bin",		0x80000, 0x14f97ceb, 6 | BRF_GRA },           // 11 Foreground Tiles

	{ "ma_2.d10",			0x20000, 0xea4cc30d, 7 | BRF_GRA },           // 12 YM2608 Samples

	{ "ma_8.f10",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 13 Zoom Tables
	{ "ma_9.f12",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 14
};

STDROMPICKEXT(mechatt, mechatt, Ym2608)
STD_ROM_FN(mechatt)

struct BurnDriver BurnDrvMechatt = {
	"mechatt", NULL, "ym2608", NULL, "1989",
	"Mechanized Attack (World)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, mechattRomInfo, mechattRomName, NULL, NULL, MechattInputInfo, MechattDIPInfo,
	MechattInit, DrvExit, DrvFrame, MechattDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};

// Mechanized Attack (Japan)

static struct BurnRomInfo mechattjRomDesc[] = {
	{ "ma5j.n12",			0x20000, 0xe6bb5952, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ma4j.l12",			0x20000, 0xc78baa62, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ma7j.n13",			0x20000, 0x12a68fc2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ma6j.l13",			0x20000, 0x332b2f54, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ma_3.e13",			0x10000, 0xc06cc8e1, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "ma_1.l2",			0x10000, 0x24766917, 3 | BRF_GRA },           //  5 Characters

	{ "s_9.a1",				0x20000, 0x6e8e194c, 4 | BRF_GRA },           //  6 Sprites
	{ "s_1.b1",				0x20000, 0xfd9161ed, 4 | BRF_GRA },           //  7
	{ "s_10.a2",			0x20000, 0xfad6a1ab, 4 | BRF_GRA },           //  8
	{ "s_2.b2",				0x20000, 0x549056f0, 4 | BRF_GRA },           //  9
	{ "s_11.a3",			0x20000, 0x3887a382, 4 | BRF_GRA },           // 10
	{ "s_3.b3",				0x20000, 0xcb99f565, 4 | BRF_GRA },           // 11
	{ "s_12.a4",			0x20000, 0x63417b49, 4 | BRF_GRA },           // 12
	{ "s_4.b4",				0x20000, 0xd739d48a, 4 | BRF_GRA },           // 13
	{ "s_13.a5",			0x20000, 0xeccd47b6, 4 | BRF_GRA },           // 14
	{ "s_5.b5",				0x20000, 0xe15244da, 4 | BRF_GRA },           // 15
	{ "s_14.a6",			0x20000, 0xbbbf0461, 4 | BRF_GRA },           // 16
	{ "s_6.b6",				0x20000, 0x4ee89f75, 4 | BRF_GRA },           // 17
	{ "s_15.a7",			0x20000, 0xcde29bad, 4 | BRF_GRA },           // 18
	{ "s_7.b7",				0x20000, 0x065ed221, 4 | BRF_GRA },           // 19
	{ "s_16.a8",			0x20000, 0x70f28040, 4 | BRF_GRA },           // 20
	{ "s_8.b8",				0x20000, 0xa6f8574f, 4 | BRF_GRA },           // 21

	{ "s_21.b3",			0x20000, 0x701a0072, 5 | BRF_GRA },           // 22 Background Tiles
	{ "s_22.b4",			0x20000, 0x34e6225c, 5 | BRF_GRA },           // 23
	{ "s_23.b5",			0x20000, 0x9a7399d3, 5 | BRF_GRA },           // 24
	{ "s_24.b6",			0x20000, 0xf097459d, 5 | BRF_GRA },           // 25

	{ "s_17.a3",			0x20000, 0xcc47c4a3, 6 | BRF_GRA },           // 26 Foreground Tiles
	{ "s_18.a4",			0x20000, 0xa04377e8, 6 | BRF_GRA },           // 27
	{ "s_19.a5",			0x20000, 0xb07f5289, 6 | BRF_GRA },           // 28
	{ "s_20.a6",			0x20000, 0xa9bb4fa9, 6 | BRF_GRA },           // 29

	{ "ma_2.d10",			0x20000, 0xea4cc30d, 7 | BRF_GRA },           // 30 YM2608 Samples

	{ "ma_8.f10",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 31 Zoom Tables
	{ "ma_9.f12",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 32
};

STDROMPICKEXT(mechattj, mechattj, Ym2608)
STD_ROM_FN(mechattj)

struct BurnDriver BurnDrvMechattj = {
	"mechattj", "mechatt", "ym2608", NULL, "1989",
	"Mechanized Attack (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, mechattjRomInfo, mechattjRomName, NULL, NULL, MechattInputInfo, MechattDIPInfo,
	MechattjInit, DrvExit, DrvFrame, MechattDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};

// Mechanized Attack (US)

static struct BurnRomInfo mechattuRomDesc[] = {
	{ "ma5u.n12",			0x20000, 0x485ea606, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ma4u.l12",			0x20000, 0x09fa31ec, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ma7u.n13",			0x20000, 0xf45b2c70, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ma6u.l13",			0x20000, 0xd5d68ce6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ma_3.e13",			0x10000, 0xc06cc8e1, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "ma_1.l2",			0x10000, 0x24766917, 3 | BRF_GRA },           //  5 Characters

	{ "mao89p13.bin",		0x80000, 0x8bcb16cf, 4 | BRF_GRA },           //  6 Sprites
	{ "ma189p15.bin",		0x80000, 0xb84d9658, 4 | BRF_GRA },           //  7
	{ "ma289p17.bin",		0x80000, 0x6cbe08ac, 4 | BRF_GRA },           //  8
	{ "ma389m15.bin",		0x80000, 0x34d4585e, 4 | BRF_GRA },           //  9

	{ "mab189a2.bin",		0x80000, 0xe1c8b4d0, 5 | BRF_GRA },           // 10 Background Tiles

	{ "mab289c2.bin",		0x80000, 0x14f97ceb, 6 | BRF_GRA },           // 11 Foreground Tiles

	{ "ma_2.d10",			0x20000, 0xea4cc30d, 7 | BRF_GRA },           // 12 YM2608 Samples

	{ "ma_8.f10",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 13 Zoom Tables
	{ "ma_9.f12",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 14
};

STDROMPICKEXT(mechattu, mechattu, Ym2608)
STD_ROM_FN(mechattu)

struct BurnDriver BurnDrvMechattu = {
	"mechattu", "mechatt", "ym2608", NULL, "1989",
	"Mechanized Attack (US)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, mechattuRomInfo, mechattuRomName, NULL, NULL, MechattInputInfo, MechattuDIPInfo,
	MechattInit, DrvExit, DrvFrame, MechattDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};

// Mechanized Attack (US, Version 1, Single Player)

static struct BurnRomInfo mechattu1RomDesc[] = {
	{ "ma_ver1_5u.n12",		0x20000, 0xdcd2e971, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ma_ver1_4u.l12",		0x20000, 0x69c8a85b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ma7u.n13",			0x20000, 0xf45b2c70, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ma6u.l13",			0x20000, 0xd5d68ce6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ma_3.e13",			0x10000, 0xc06cc8e1, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "ma_1.l2",			0x10000, 0x24766917, 3 | BRF_GRA },           //  5 Characters

	{ "s_9.a1",				0x20000, 0x6e8e194c, 4 | BRF_GRA },           //  6 Sprites
	{ "s_1.b1",				0x20000, 0xfd9161ed, 4 | BRF_GRA },           //  7
	{ "s_10.a2",			0x20000, 0xfad6a1ab, 4 | BRF_GRA },           //  8
	{ "s_2.b2",				0x20000, 0x549056f0, 4 | BRF_GRA },           //  9
	{ "s_11.a3",			0x20000, 0x3887a382, 4 | BRF_GRA },           // 10
	{ "s_3.b3",				0x20000, 0xcb99f565, 4 | BRF_GRA },           // 11
	{ "s_12.a4",			0x20000, 0x63417b49, 4 | BRF_GRA },           // 12
	{ "s_4.b4",				0x20000, 0xd739d48a, 4 | BRF_GRA },           // 13
	{ "s_13.a5",			0x20000, 0xeccd47b6, 4 | BRF_GRA },           // 14
	{ "s_5.b5",				0x20000, 0xe15244da, 4 | BRF_GRA },           // 15
	{ "s_14.a6",			0x20000, 0xbbbf0461, 4 | BRF_GRA },           // 16
	{ "s_6.b6",				0x20000, 0x4ee89f75, 4 | BRF_GRA },           // 17
	{ "s_15.a7",			0x20000, 0xcde29bad, 4 | BRF_GRA },           // 18
	{ "s_7.b7",				0x20000, 0x065ed221, 4 | BRF_GRA },           // 19
	{ "s_16.a8",			0x20000, 0x70f28040, 4 | BRF_GRA },           // 20
	{ "s_8.b8",				0x20000, 0xa6f8574f, 4 | BRF_GRA },           // 21

	{ "s_21.b3",			0x20000, 0x701a0072, 5 | BRF_GRA },           // 22 Background Tiles
	{ "s_22.b4",			0x20000, 0x34e6225c, 5 | BRF_GRA },           // 23
	{ "s_23.b5",			0x20000, 0x9a7399d3, 5 | BRF_GRA },           // 24
	{ "s_24.b6",			0x20000, 0xf097459d, 5 | BRF_GRA },           // 25

	{ "s_17.a3",			0x20000, 0xcc47c4a3, 6 | BRF_GRA },           // 26 Foreground Tiles
	{ "s_18.a4",			0x20000, 0xa04377e8, 6 | BRF_GRA },           // 27
	{ "s_19.a5",			0x20000, 0xb07f5289, 6 | BRF_GRA },           // 28
	{ "s_20.a6",			0x20000, 0xa9bb4fa9, 6 | BRF_GRA },           // 29

	{ "ma_2.d10",			0x20000, 0xea4cc30d, 7 | BRF_GRA },           // 30 YM2608 Samples

	{ "ma_8.f10",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 31 Zoom Tables
	{ "ma_9.f12",			0x10000, 0x61f3de03, 8 | BRF_GRA },           // 32
};

STDROMPICKEXT(mechattu1, mechattu1, Ym2608)
STD_ROM_FN(mechattu1)

struct BurnDriver BurnDrvMechattu1 = {
	"mechattu1", "mechatt", "ym2608", NULL, "1989",
	"Mechanized Attack (US, Version 1, Single Player)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, mechattu1RomInfo, mechattu1RomName, NULL, NULL, MechattInputInfo, MechattuDIPInfo,
	MechattjInit, DrvExit, DrvFrame, MechattDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};
