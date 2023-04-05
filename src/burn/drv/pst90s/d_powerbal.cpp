// FinalBurn Neo Playmark Power Ball / Hot Mind / Magic Sticks / Atom bootleg hardware
// Bssed on MAME driver by David Haywood and Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "eeprom.h"
#include "burn_pal.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvVideoRAM;
static UINT8 *DrvSprRAM;

static INT32 tilebank;
static INT32 soundbank;

static INT32 video_ram_offset;
static INT32 tilebank_shift;
static INT32 vblank;
static INT32 irq_line;
static INT32 use_vblank_eeprom;
static INT32 sprite_x_adjust;
static INT32 sprite_y_adjust;
static INT32 sprite_transpen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT16 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo PowerbalInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Powerbal)

static struct BurnInputInfo MagicstkInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Magicstk)

static struct BurnInputInfo HotmindaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Hotminda)

static struct BurnInputInfo AtombjtInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Atombjt)

static struct BurnDIPInfo PowerbalDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL						},
	{0x12, 0xff, 0xff, 0xef, NULL						},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x11, 0x01, 0x01, 0x01, "Off"						},
	{0x11, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x11, 0x01, 0x02, 0x00, "No"						},
	{0x11, 0x01, 0x02, 0x02, "Yes"						},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x11, 0x01, 0x1c, 0x08, "6 Coins 1 Credits"		},
	{0x11, 0x01, 0x1c, 0x0c, "5 Coins 1 Credits"		},
	{0x11, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x1c, 0x14, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x1c, 0x04, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0x1c, 0x00, "4 Coins 3 Credits"		},
	{0x11, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x11, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0xe0, 0x00, "3 Coins 4 Credits"		},
	{0x11, 0x01, 0xe0, 0x20, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0xe0, 0x60, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0xe0, 0x40, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x12, 0x01, 0x01, 0x00, "Off"						},
	{0x12, 0x01, 0x01, 0x01, "On"						},

	{0   , 0xfe, 0   ,    2, "Language"					},
	{0x12, 0x01, 0x08, 0x08, "English"					},
	{0x12, 0x01, 0x08, 0x00, "Italian"					},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0x30, 0x30, "1"						},
	{0x12, 0x01, 0x30, 0x20, "2"						},
	{0x12, 0x01, 0x30, 0x10, "3"						},
	{0x12, 0x01, 0x30, 0x00, "4"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x12, 0x01, 0xc0, 0xc0, "Easy"						},
	{0x12, 0x01, 0xc0, 0x80, "Normal"					},
	{0x12, 0x01, 0xc0, 0x40, "Hard"						},
	{0x12, 0x01, 0xc0, 0x00, "Very Hard"				},
};

STDDIPINFO(Powerbal)

static struct BurnDIPInfo MagicstkDIPList[]=
{
	{0x03, 0xff, 0xff, 0xff, NULL						},
	{0x04, 0xff, 0xff, 0x77, NULL						},

	{0   , 0xfe, 0   ,    1, "Coin Mode"				},
	{0x03, 0x01, 0x01, 0x01, "Mode 1"					},

	{0   , 0xfe, 0   ,    15, "Coinage Mode 1"			},
	{0x03, 0x01, 0x1e, 0x14, "6 Coins 1 Credits"		},
	{0x03, 0x01, 0x1e, 0x16, "5 Coins 1 Credits"		},
	{0x03, 0x01, 0x1e, 0x18, "4 Coins 1 Credits"		},
	{0x03, 0x01, 0x1e, 0x1a, "3 Coins 1 Credits"		},
	{0x03, 0x01, 0x1e, 0x02, "8 Coins 3 Credits"		},
	{0x03, 0x01, 0x1e, 0x1c, "2 Coins 1 Credits"		},
	{0x03, 0x01, 0x1e, 0x04, "5 Coins 3 Credits"		},
	{0x03, 0x01, 0x1e, 0x06, "3 Coins 2 Credits"		},
	{0x03, 0x01, 0x1e, 0x1e, "1 Coin  1 Credits"		},
	{0x03, 0x01, 0x1e, 0x08, "2 Coins 3 Credits"		},
	{0x03, 0x01, 0x1e, 0x12, "1 Coin  2 Credits"		},
	{0x03, 0x01, 0x1e, 0x10, "1 Coin  3 Credits"		},
	{0x03, 0x01, 0x1e, 0x0e, "1 Coin  4 Credits"		},
	{0x03, 0x01, 0x1e, 0x0c, "1 Coin  5 Credits"		},
	{0x03, 0x01, 0x1e, 0x0a, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Clear Counters"			},
	{0x04, 0x01, 0x01, 0x01, "Off"						},
	{0x04, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Ticket"					},
	{0x04, 0x01, 0x02, 0x02, "Off"						},
	{0x04, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Hopper"					},
	{0x04, 0x01, 0x04, 0x04, "Off"						},
	{0x04, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x04, 0x01, 0x08, 0x08, "Off"						},
	{0x04, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x07, 0x01, 0x10, 0x10, "Off"						},
	{0x07, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    8, "Difficulty"				},
	{0x04, 0x01, 0xe0, 0xa0, "Hard 7"					},
	{0x04, 0x01, 0xe0, 0x20, "Very Hard 6"				},
	{0x04, 0x01, 0xe0, 0xc0, "Very Hard 5"				},
	{0x04, 0x01, 0xe0, 0x80, "Very Hard 4"				},
	{0x04, 0x01, 0xe0, 0x40, "Very Hard 4"				},
	{0x04, 0x01, 0xe0, 0x00, "Very Hard 4"				},
	{0x04, 0x01, 0xe0, 0x60, "Normal 8"					},
	{0x04, 0x01, 0xe0, 0xe0, "Easy 9"					},
};

STDDIPINFO(Magicstk)


static struct BurnDIPInfo HotmindaDIPList[]=
{
	{0x06, 0xff, 0xff, 0xff, NULL						},
	{0x07, 0xff, 0xff, 0x77, NULL						},

	{0   , 0xfe, 0   ,    4, "Premio"					},
	{0x06, 0x01, 0x06, 0x00, "1 Premio ogni 10 Vincite"	},
	{0x06, 0x01, 0x06, 0x04, "1 Premio ogni 10 Vincite"	},
	{0x06, 0x01, 0x06, 0x02, "1 Premio ogni 5 Vincite"	},
	{0x06, 0x01, 0x06, 0x06, "Paga 1 Premio ogni Vincita"},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x06, 0x01, 0x18, 0x18, "1 Coin  2 Credits"		},
	{0x06, 0x01, 0x18, 0x08, "1 Coin  3 Credits"		},
	{0x06, 0x01, 0x18, 0x10, "1 Coin  5 Credits"		},
	{0x06, 0x01, 0x18, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Clear Counters"			},
	{0x07, 0x01, 0x01, 0x01, "Off"						},
	{0x07, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Ticket"					},
	{0x07, 0x01, 0x02, 0x02, "Off"						},
	{0x07, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Hopper"					},
	{0x07, 0x01, 0x04, 0x04, "Off"						},
	{0x07, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x07, 0x01, 0x08, 0x08, "Off"						},
	{0x07, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x07, 0x01, 0x10, 0x10, "Off"						},
	{0x07, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    8, "Difficulty"				},
	{0x07, 0x01, 0xe0, 0xa0, "Hard 7"					},
	{0x07, 0x01, 0xe0, 0x20, "Very Hard 6"				},
	{0x07, 0x01, 0xe0, 0xc0, "Very Hard 5"				},
	{0x07, 0x01, 0xe0, 0x40, "Very Hard 4"				},
	{0x07, 0x01, 0xe0, 0x80, "Very Hard 3"				},
	{0x07, 0x01, 0xe0, 0x00, "Very Hard 2"				},
	{0x07, 0x01, 0xe0, 0x60, "Normal 8"					},
	{0x07, 0x01, 0xe0, 0xe0, "Easy 9"					},
};

STDDIPINFO(Hotminda)

static struct BurnDIPInfo AtombjtDIPList[]=
{
	{0x12, 0xff, 0xff, 0xfe, NULL						},
	{0x13, 0xff, 0xff, 0xfe, NULL						},

	{0   , 0xfe, 0   ,    8, "Starting level"			},
	{0x12, 0x01, 0x0e, 0x08, "Germany"					},
	{0x12, 0x01, 0x0e, 0x04, "Thailand"					},
	{0x12, 0x01, 0x0e, 0x0c, "Nevada"					},
	{0x12, 0x01, 0x0e, 0x0e, "Japan"					},
	{0x12, 0x01, 0x0e, 0x06, "Korea"					},
	{0x12, 0x01, 0x0e, 0x0a, "England"					},
	{0x12, 0x01, 0x0e, 0x02, "Hong Kong"				},
	{0x12, 0x01, 0x0e, 0x00, "China"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x12, 0x01, 0x30, 0x20, "Easy"						},
	{0x12, 0x01, 0x30, 0x30, "Normal"					},
	{0x12, 0x01, 0x30, 0x10, "Hard"						},
	{0x12, 0x01, 0x30, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0xc0, 0x00, "1"						},
	{0x12, 0x01, 0xc0, 0x40, "2"						},
	{0x12, 0x01, 0xc0, 0xc0, "3"						},
	{0x12, 0x01, 0xc0, 0x80, "4"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x13, 0x01, 0x02, 0x00, "Off"						},
	{0x13, 0x01, 0x02, 0x02, "On"						},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x1c, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0xe0, 0x00, "Free Play"				},
};

STDDIPINFO(Atombjt)

static void oki_bankswitch(INT32 data)
{
	soundbank = data & 3;
	MSM6295SetBank(0, DrvSndROM, 0x00000, 0x1ffff);
	MSM6295SetBank(0, DrvSndROM + (soundbank * 0x20000), 0x20000, 0x3ffff);
}

static void __fastcall magicstk_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x094000: // atombj
			if (tilebank_shift == 0) {
				tilebank = (data >> tilebank_shift) & 0x0f;
			}
		return;

		case 0x094002:
		return;

		case 0x094004:
			if (tilebank_shift != 0) {
				tilebank = (data >> tilebank_shift) & 0x0f;
			}
		return;

		case 0xc2014:
		{
			if (!use_vblank_eeprom) return;

			EEPROMSetCSLine((~data & 8) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
			EEPROMWriteBit((data & 2) >> 1);
			EEPROMSetClockLine((~data & 4) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);

		}
		return;

		case 0xc201c:
			oki_bankswitch(data);
		return;

		case 0xc201e:
			MSM6295Write(0, data & 0xff);
		return;

		case 0xc4000:
		return;
	}
}

static void __fastcall magicstk_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x094000:
			if (tilebank_shift == 0) {
				tilebank = (data >> (tilebank_shift & 7)) & 0x0f;
			}
		return;

		case 0x094002:
		return;

		case 0x094004:
			if (tilebank_shift != 0) {
				tilebank = (data >> (tilebank_shift & 7)) & 0x0f;
			}
		return;

		case 0xc2015:
		{
			if (!use_vblank_eeprom) return;
			EEPROMSetCSLine((~data & 8) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
			EEPROMWriteBit((data & 2) >> 1);
			EEPROMSetClockLine((~data & 4) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		}
		return;

		case 0xc201d:
			oki_bankswitch(data);
		return;

		case 0xc201f:
			MSM6295Write(0, data & 0xff);
		return;

		case 0xc4000:
		return;
	}
}

static UINT16 __fastcall magicstk_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xc2010:
			return DrvInputs[0];

		case 0xc2012:
			return DrvInputs[1];

		case 0xc2014:
		{
			INT32 ret = DrvInputs[2];
			if (use_vblank_eeprom) {
				ret &= ~0x41;
				ret |= EEPROMRead() ? 0x01 : 0;
				ret |= vblank ? 0x40 : 0;
			}
			return ret;
		}

		case 0xc2016:
			return DrvDips[0];

		case 0xc2018:
			return DrvDips[1];

		case 0xc201e:
			return MSM6295Read(0);
	}
	return 0;
}

static UINT8 __fastcall magicstk_main_read_byte(UINT32 address)
{
	return SekReadWord(address & ~1);
}

static tilemap_callback( bg )
{
	UINT16 *ram = (UINT16*)(DrvVideoRAM + video_ram_offset);

	INT32 attr = ram[offs];

	TILE_SET_INFO(0, (attr & 0x7ff) | (tilebank * 0x800) | ((attr & 0x800) << 4), attr >> 12, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset(0);
	oki_bankswitch(1);

	if (use_vblank_eeprom) EEPROMReset();

	tilebank = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;

	DrvGfxROM[0]	= Next; Next += 0x400000;
	DrvGfxROM[1]	= Next; Next += 0x400000;

	DrvSndROM		= Next; Next += 0x080000;

	BurnPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x020000;
	BurnPalRAM		= Next; Next += 0x000400;
	DrvVideoRAM		= Next; Next += 0x001400;
	DrvSprRAM		= Next; Next += 0x003000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 chr_len, INT32 spr_len)
{
	INT32 Plane0[4] = { (chr_len/4)*8*3, (chr_len/4)*8*2, (chr_len/4)*8*1, (chr_len/4)*8*0 };
	INT32 Plane1[4] = { (spr_len/4)*8*3, (spr_len/4)*8*2, (spr_len/4)*8*1, (spr_len/4)*8*0 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc((spr_len > chr_len) ? spr_len : chr_len);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], chr_len);
	GfxDecode((chr_len  * 2) / (8 * 8),  4,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], spr_len);
	GfxDecode((spr_len * 2) / (16 * 16), 4, 16, 16, Plane1, XOffs, YOffs, 0x100, tmp, DrvGfxROM[1]);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvLoadRoms(INT32 chr_len, INT32 spr_len)
{
	INT32 k = 0;

	if (BurnLoadRom(Drv68KROM    + 0x00001, k++, 2)) return 1;
	if (BurnLoadRom(Drv68KROM    + 0x00000, k++, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM[0] + ((chr_len / 4) * 0), k++, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + ((chr_len / 4) * 1), k++, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + ((chr_len / 4) * 2), k++, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[0] + ((chr_len / 4) * 3), k++, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM[1] + ((spr_len / 4) * 0), k++, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + ((spr_len / 4) * 1), k++, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + ((spr_len / 4) * 2), k++, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM[1] + ((spr_len / 4) * 3), k++, 1)) return 1;

	if (BurnLoadRom(DrvSndROM    + 0x00000, k++, 1)) return 1;

	DrvGfxDecode(chr_len, spr_len);

	return 0;
}

static INT32 MagicstkInit()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms(0x80000, 0x80000)) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(BurnPalRAM,			0x088000, 0x0883ff, MAP_RAM);
	SekMapMemory(DrvVideoRAM,			0x098000, 0x0993ff, MAP_RAM); // 180-117f
	SekMapMemory(Drv68KRAM,				0x0e0000, 0x0fffff, MAP_RAM);
	SekMapMemory(DrvSprRAM,				0x100000, 0x100fff, MAP_RAM);
	SekSetWriteWordHandler(0,			magicstk_main_write_word);
	SekSetWriteByteHandler(0,			magicstk_main_write_byte);
	SekSetReadWordHandler(0,			magicstk_main_read_word);
	SekSetReadByteHandler(0,			magicstk_main_read_byte);
	SekClose();

	EEPROMInit(&eeprom_interface_93C46);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x100000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x100000, 0x100, 0xf);

	video_ram_offset = 0x180;
	tilebank_shift = 12;
	use_vblank_eeprom = 1;
	irq_line = 2;
	sprite_x_adjust = -16;
	sprite_y_adjust = -10;
	sprite_transpen = 0;

	DrvDoReset();

	return 0;
}

static INT32 PowerbalInit()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms(0x200000, 0x200000)) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(BurnPalRAM,			0x088000, 0x0883ff, MAP_RAM);
	SekMapMemory(DrvVideoRAM,			0x098000, 0x098fff, MAP_RAM);
	SekMapMemory(Drv68KRAM + 0x10000,	0x099000, 0x09bfff, MAP_RAM);
	SekMapMemory(Drv68KRAM,				0x0f0000, 0x0fffff, MAP_RAM);
	SekMapMemory(DrvSprRAM,				0x101000, 0x103fff, MAP_RAM);	// only 1000-1fff sprite ram!
	SekSetWriteWordHandler(0,			magicstk_main_write_word);
	SekSetWriteByteHandler(0,			magicstk_main_write_byte);
	SekSetReadWordHandler(0,			magicstk_main_read_word);
	SekSetReadByteHandler(0,			magicstk_main_read_byte);
	SekClose();

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x400000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x400000, 0x100, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	video_ram_offset = 0;
	tilebank_shift = 12;
	use_vblank_eeprom = 0;
	irq_line = 2;
	sprite_x_adjust = -16;
	sprite_y_adjust = -10;
	sprite_transpen = 0;

	DrvDoReset();

	return 0;
}

static INT32 AtombjtInit()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms(0x200000, 0x100000)) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(BurnPalRAM,			0x088000, 0x0883ff, MAP_RAM);
	SekMapMemory(DrvVideoRAM,			0x09c000, 0x09cfff, MAP_RAM);
	SekMapMemory(DrvVideoRAM,			0x09d000, 0x09dfff, MAP_RAM);
	SekMapMemory(Drv68KRAM,				0x0f0000, 0x100fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,				0x101000, 0x103fff, MAP_RAM);	// only 1000-1fff sprite ram!
	SekSetWriteWordHandler(0,			magicstk_main_write_word);
	SekSetWriteByteHandler(0,			magicstk_main_write_byte);
	SekSetReadWordHandler(0,			magicstk_main_read_word);
	SekSetReadByteHandler(0,			magicstk_main_read_byte);
	SekClose();

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x400000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x200000, 0x100, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 64, -16);

	video_ram_offset = 0;
	tilebank_shift = 0;
	use_vblank_eeprom = 0;
	irq_line = 6;
	sprite_x_adjust = 32 + 8;
	sprite_y_adjust = -16 + 9;
	sprite_transpen = 0xf;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);
	if (use_vblank_eeprom) EEPROMExit();
	SekExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 4; offs < 0x1000 / 2; offs += 4)
	{
		INT32 attr	= ram[offs - 1];
		if (attr & 0x8000) break;

		INT32 sx	= ram[offs + 1] & 0x01ff;
		INT32 code	= ram[offs + 2];
		INT32 color	= ram[offs + 1] >> 12;
		INT32 flipx	= attr & 0x4000;
		INT32 sy	= (232 - attr) & 0xff;

		DrawGfxMaskTile(0, 1, code, sx - 23 + (sprite_x_adjust - 4), sy + sprite_y_adjust, flipx, 0, color, sprite_transpen);
	}
}

static INT32 DrvDraw()
{
	BurnPaletteUpdate_RRRRGGGGBBBBRGBx();

	BurnTransferClear();

	if (nBurnLayer & 1)		GenericTilemapDraw(0, 0, 0);
	if (nSpriteEnable & 1)	draw_sprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	SekOpen(0);
	vblank = 1;  // start in vblank
	SekSetIRQLine(irq_line, CPU_IRQSTATUS_AUTO);
	SekRun(((12000000 / 60) * 16)/256);
	vblank = 0;
	SekRun(((12000000 / 60) * 240)/256);
	SekClose();  // end at vblank

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

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
		SekScan(nAction);

		MSM6295Scan(nAction, pnMin);
		if (use_vblank_eeprom) EEPROMScan(nAction, pnMin);
		SCAN_VAR(tilebank);
		SCAN_VAR(soundbank);
	}

	if (nAction & ACB_WRITE) {
		oki_bankswitch(soundbank);
	}

	return 0;
}


// Power Balls

static struct BurnRomInfo powerbalRomDesc[] = {
	{ "3.u67",				0x40000, 0x3aecdde4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2.u66",				0x40000, 0xa4552a19, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "4.u38",				0x80000, 0xa60aa981, 2 | BRF_GRA },           //  2 Background Tiles
	{ "5.u42",				0x80000, 0x966c71df, 2 | BRF_GRA },           //  3
	{ "6.u39",				0x80000, 0x668957b9, 2 | BRF_GRA },           //  4
	{ "7.u45",				0x80000, 0xf5721c66, 2 | BRF_GRA },           //  5

	{ "8.u86",				0x80000, 0x4130694c, 3 | BRF_GRA },           //  6 Sprites
	{ "9.u85",				0x80000, 0xe7bcd2e7, 3 | BRF_GRA },           //  7
	{ "10.u84",				0x80000, 0x90412135, 3 | BRF_GRA },           //  8
	{ "11.u83",				0x80000, 0x92d7d40a, 3 | BRF_GRA },           //  9

	{ "1.u16",				0x80000, 0x12776dbc, 4 | BRF_SND },           // 10 Samples

	{ "palce16v8h.u102",	0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 11 PLDs
	{ "palce22v10h.u183",	0x002dd, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 12
	{ "palce22v10h.u211",	0x002dd, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 13
	{ "palce22v10h.bin",	0x002dd, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 14
	{ "pal22v10a.bin",		0x002dd, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 15
};

STD_ROM_PICK(powerbal)
STD_ROM_FN(powerbal)

struct BurnDriver BurnDrvPowerbal = {
	"powerbal", NULL, NULL, NULL, "1994",
	"Power Balls\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC | GBF_BREAKOUT, 0,
	NULL, powerbalRomInfo, powerbalRomName, NULL, NULL, NULL, NULL, PowerbalInputInfo, PowerbalDIPInfo,
	PowerbalInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	320, 240, 4, 3
};


// Magic Sticks

static struct BurnRomInfo magicstkRomDesc[] = {
	{ "12.u67",				0x20000, 0x70a9c66f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "11.u66",				0x20000, 0xa9d7c90e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "13.u36",				0x20000, 0x31e52562, 2 | BRF_GRA },           //  2 Background Tiles
	{ "14.u42",				0x20000, 0xb0d35eda, 2 | BRF_GRA },           //  3
	{ "15.u39",				0x20000, 0xaf27004b, 2 | BRF_GRA },           //  4
	{ "16.u45",				0x20000, 0x0c980db3, 2 | BRF_GRA },           //  5

	{ "17.u86",				0x20000, 0xce238006, 3 | BRF_GRA },           //  6 Sprites
	{ "18.u85",				0x20000, 0x3dc88bf6, 3 | BRF_GRA },           //  7
	{ "19.u84",				0x20000, 0xee12d5b2, 3 | BRF_GRA },           //  8
	{ "20.u83",				0x20000, 0xa07f542b, 3 | BRF_GRA },           //  9

	{ "10.u16",				0x20000, 0x1e4a03ef, 4 | BRF_SND },           // 10 Samples

	{ "palce16v8.u33",		0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 11 PLDs
	{ "palce16v8.u58",		0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 12
	{ "gal22v10b.bin",		0x002e5, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 13
};

STD_ROM_PICK(magicstk)
STD_ROM_FN(magicstk)

struct BurnDriver BurnDrvMagicstk = {
	"magicstk", NULL, NULL, NULL, "1995",
	"Magic Sticks\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, magicstkRomInfo, magicstkRomName, NULL, NULL, NULL, NULL, MagicstkInputInfo, MagicstkDIPInfo,
	MagicstkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	320, 240, 4, 3
};


// Hot Mind (adjustable prize)

static struct BurnRomInfo hotmindaRomDesc[] = {
	// reverse order from MAME!
	{ "rom2.rom",			0x20000, 0xf5accd9f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "rom1.rom",			0x20000, 0x33aaceba, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rom13.rom",			0x20000, 0x18d22109, 2 | BRF_GRA },           //  2 Background Tiles
	{ "rom14.rom",			0x20000, 0xf95a1ff6, 2 | BRF_GRA },           //  3
	{ "rom15.rom",			0x20000, 0x8a9ea7ed, 2 | BRF_GRA },           //  4
	{ "rom16.rom",			0x20000, 0xdf63b642, 2 | BRF_GRA },           //  5

	{ "rom17.rom",			0x20000, 0x805002cf, 3 | BRF_GRA },           //  6 Sprites
	{ "rom18.rom",			0x20000, 0x6a9d896b, 3 | BRF_GRA },           //  7
	{ "rom19.rom",			0x20000, 0x223ad90f, 3 | BRF_GRA },           //  8
	{ "rom20.rom",			0x20000, 0xab37a273, 3 | BRF_GRA },           //  9

	{ "rom10.rom",			0x40000, 0x0bf3a3e5, 4 | BRF_SND },           // 10 Samples
};

STD_ROM_PICK(hotminda)
STD_ROM_FN(hotminda)

struct BurnDriver BurnDrvHotminda = {
	"hotminda", "hotmind", NULL, NULL, "1995",
	"Hot Mind (adjustable prize)\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, hotmindaRomInfo, hotmindaRomName, NULL, NULL, NULL, NULL, HotmindaInputInfo, HotmindaDIPInfo,
	MagicstkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	320, 240, 4, 3
};


// Atom (bootleg of Bombjack Twin)

static struct BurnRomInfo atombjtRomDesc[] = {
	{ "22.u67",				0x20000, 0xbead8c70, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "21.u66",				0x20000, 0x73e3d488, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "23.u36",				0x80000, 0xa3fb6b91, 2 | BRF_GRA },           //  2 Background Tiles
	{ "24.u42",				0x80000, 0x4c30e15f, 2 | BRF_GRA },           //  3
	{ "25.u39",				0x80000, 0xff1af60f, 2 | BRF_GRA },           //  4
	{ "26.u45",				0x80000, 0x6cc4e817, 2 | BRF_GRA },           //  5

	{ "27.u86",				0x40000, 0x5a853e5c, 3 | BRF_GRA },           //  6 Sprites
	{ "28.u85",				0x40000, 0x41970bf6, 3 | BRF_GRA },           //  7
	{ "29.u84",				0x40000, 0x59a7d610, 3 | BRF_GRA },           //  8
	{ "30.u83",				0x40000, 0x9b2dfebd, 3 | BRF_GRA },           //  9

	{ "20.u16",				0x80000, 0x71c74ff9, 4 | BRF_SND },           // 10 Samples
};

STD_ROM_PICK(atombjt)
STD_ROM_FN(atombjt)

struct BurnDriver BurnDrvAtombjt = {
	"atombjt", "bjtwin", NULL, NULL, "1993",
	"Atom (bootleg of Bombjack Twin)\0", NULL, "bootleg (Kyon K.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, atombjtRomInfo, atombjtRomName, NULL, NULL, NULL, NULL, AtombjtInputInfo, AtombjtDIPInfo,
	AtombjtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	224, 384, 3, 4
};
