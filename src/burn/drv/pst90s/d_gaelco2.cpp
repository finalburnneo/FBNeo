#include "tiles_generic.h"
#include "m68000_intf.h"
#include "eeprom.h"
#include "gaelco.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvPalRAM;
static UINT8 *Drv68KRAM;
static UINT8 *Drv68KRAM2;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 *DrvVidRegs;

static UINT32 snowboar_latch;

static void (*pIRQCallback)(INT32 line);

static UINT8 m_dual_monitor = 0;
static UINT32 gfxmask = ~0;

static INT32 nCPUClockSpeed = 0;

//static INT32 global_x_offset = 0;
static INT32 global_y_offset = -16;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT16 DrvInputs[4];

static struct BurnInputInfo AlighuntInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},

	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"service"	},

	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Alighunt)

static struct BurnInputInfo SnowboarInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 3,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Snowboar)

static struct BurnDIPInfo SnowboarDIPList[]=
{
	{0x14, 0xff, 0xff, 0x04, NULL		},

	{0   , 0xfe, 0   ,    1, "Service Mode (No Toggle)"		},
	{0x14, 0x01, 0x04, 0x04, "Off"		},
};

STDDIPINFO(Snowboar)

static struct BurnInputInfo ManiacsqInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},

	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"service"	},

	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Maniacsq)

static struct BurnDIPInfo AlighuntDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL				},
	{0x16, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,   12, "Coinage"			},
	{0x15, 0x01, 0x0f, 0x07, "4 Coins 1 Credit"		},
	{0x15, 0x01, 0x0f, 0x08, "3 Coins 1 Credit"		},
	{0x15, 0x01, 0x0f, 0x09, "2 Coins 1 Credit"		},
	{0x15, 0x01, 0x0f, 0x05, "3 Coins 2 Credit"		},
	{0x15, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x15, 0x01, 0x0f, 0x06, "2 Coin  3 Credits"		},
	{0x15, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x0f, 0x0c, "1 Coins 4 Credits"		},
	{0x15, 0x01, 0x0f, 0x0b, "1 Coins 5 Credits"		},
	{0x15, 0x01, 0x0f, 0x0a, "1 Coins 6 Credits"		},
	{0x15, 0x01, 0x0f, 0x00, "Free Plan if Coin B too"	},

	{0   , 0xfe, 0   ,   12, "Coinage"			},
	{0x15, 0x01, 0xf0, 0x70, "4 Coins 1 Credit"		},
	{0x15, 0x01, 0xf0, 0x80, "3 Coins 1 Credit"		},
	{0x15, 0x01, 0xf0, 0x90, "2 Coins 1 Credit"		},
	{0x15, 0x01, 0xf0, 0x50, "3 Coins 2 Credit"		},
	{0x15, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x15, 0x01, 0xf0, 0x60, "2 Coin  3 Credits"		},
	{0x15, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0xf0, 0xc0, "1 Coins 4 Credits"		},
	{0x15, 0x01, 0xf0, 0xb0, "1 Coins 5 Credits"		},
	{0x15, 0x01, 0xf0, 0xa0, "1 Coins 6 Credits"		},
	{0x15, 0x01, 0xf0, 0x00, "Free Plan if Coin A too"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x16, 0x01, 0x03, 0x02, "Easy"				},
	{0x16, 0x01, 0x03, 0x03, "Normal"			},
	{0x16, 0x01, 0x03, 0x01, "Hard"				},
	{0x16, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x16, 0x01, 0x0c, 0x08, "1"				},
	{0x16, 0x01, 0x0c, 0x0c, "2"				},
	{0x16, 0x01, 0x0c, 0x04, "3"				},
	{0x16, 0x01, 0x0c, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Sound Type",			},
	{0x16, 0x01, 0x10, 0x00, "Mono"				},
	{0x16, 0x01, 0x10, 0x10, "Stereo"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x20, 0x00, "Off"				},
	{0x16, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Joystick"			},
	{0x16, 0x01, 0x40, 0x00, "Analog (Unemulated)"		},
	{0x16, 0x01, 0x40, 0x40, "Standard"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x16, 0x01, 0x80, 0x80, "Off"				},
	{0x16, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Alighunt)

static struct BurnDIPInfo ManiacsqDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,   12, "Coinage"			},
	{0x11, 0x01, 0x0f, 0x07, "4 Coins 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x08, "3 Coins 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x09, "2 Coins 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x05, "3 Coins 2 Credit"		},
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x11, 0x01, 0x0f, 0x06, "2 Coin  3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0c, "1 Coins 4 Credits"		},
	{0x11, 0x01, 0x0f, 0x0b, "1 Coins 5 Credits"		},
	{0x11, 0x01, 0x0f, 0x0a, "1 Coins 6 Credits"		},
	{0x11, 0x01, 0x0f, 0x00, "Free Plan if Coin B too"	},

	{0   , 0xfe, 0   ,   12, "Coinage"			},
	{0x11, 0x01, 0xf0, 0x70, "4 Coins 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x80, "3 Coins 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x90, "2 Coins 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x50, "3 Coins 2 Credit"		},
	{0x11, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x11, 0x01, 0xf0, 0x60, "2 Coin  3 Credits"		},
	{0x11, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0xf0, 0xc0, "1 Coins 4 Credits"		},
	{0x11, 0x01, 0xf0, 0xb0, "1 Coins 5 Credits"		},
	{0x11, 0x01, 0xf0, 0xa0, "1 Coins 6 Credits"		},
	{0x11, 0x01, 0xf0, 0x00, "Free Plan if Coin A too"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x16, 0x01, 0x03, 0x02, "Easy"				},
	{0x16, 0x01, 0x03, 0x03, "Normal"			},
	{0x16, 0x01, 0x03, 0x01, "Hard"				},
	{0x16, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x20, 0x00, "Off"				},
	{0x16, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "1P Continue"			},
	{0x16, 0x01, 0x40, 0x00, "Off"				},
	{0x16, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x16, 0x01, 0x80, 0x80, "Off"				},
	{0x16, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Maniacsq)

// Snowboard protection

static UINT32 rol(UINT32 x, UINT32 c)
{
	return (x << c) | (x >> (32 - c));
}
 
static UINT16 get_lo(UINT32 x)
{
	return ((x & 0x00000010) <<  1) |
			((x & 0x00000800) <<  3) |
			((x & 0x40000000) >> 27) |
			((x & 0x00000005) <<  6) |
			((x & 0x00000008) <<  8) |
			rol(x & 0x00800040, 9)   |
			((x & 0x04000000) >> 16) |
			((x & 0x00008000) >> 14) |
			((x & 0x00002000) >> 11) |
			((x & 0x00020000) >> 10) |
			((x & 0x00100000) >>  8) |
			((x & 0x00044000) >>  5) |
			((x & 0x00000020) >>  1);
}
 
static UINT16 get_hi(UINT32 x)
{
	return ((x & 0x00001400) >>  0) |
			((x & 0x10000000) >> 26) |
			((x & 0x02000000) >> 24) |
			((x & 0x08000000) >> 21) |
			((x & 0x00000002) << 12) |
			((x & 0x01000000) >> 19) |
			((x & 0x20000000) >> 18) |
			((x & 0x80000000) >> 16) |
			((x & 0x00200000) >> 13) |
			((x & 0x00010000) >> 12) |
			((x & 0x00080000) >> 10) |
			((x & 0x00000200) >>  9) |
			((x & 0x00400000) >>  8) |
			((x & 0x00000080) >>  4) |
			((x & 0x00000100) >>  1);
}
 
static UINT16 get_out(UINT16 x)
{
	return ((x & 0xc840) <<  0) |
			((x & 0x0080) <<  2) |
			((x & 0x0004) <<  3) |
			((x & 0x0008) <<  5) |
			((x & 0x0010) <<  8) |
			((x & 0x0002) <<  9) |
			((x & 0x0001) << 13) |
			((x & 0x0200) >>  9) |
			((x & 0x1400) >>  8) |
			((x & 0x0100) >>  7) |
			((x & 0x2000) >>  6) |
			((x & 0x0020) >>  2);
}
 
static UINT16 mangle(UINT32 x)
{
	UINT16 a = get_lo(x);
	UINT16 b = get_hi(x);
	return get_out(((a ^ 0x0010) - (b ^ 0x0024)) ^ 0x5496);
}

static UINT16 snowboar_protection_r()
{
	UINT16 ret  = mangle(snowboar_latch);
	ret = ((ret & 0xff00) >> 8) | ((ret & 0x00ff) << 8);
	return ret;

}

static void snowboar_protection_w(UINT16 offset, UINT16 data)
{
	//COMBINE_DATA(&m_snowboar_protection[offset]);

	snowboar_latch = (snowboar_latch << 16) | data;

	//logerror("%06x: protection write %04x to %04x\n", space.device().safe_pc(), data, offset*2);
}

static void __fastcall gaelco2_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x218004:
		case 0x218005:
		case 0x218006:
		case 0x218007:
		case 0x218008:
		case 0x218009: {
			UINT8 *r = (UINT8*)DrvVidRegs;
			r[(address - 0x218004) ^ 1] = data;
		}
		return;

		case 0x300008:
		case 0x300009:
			EEPROMWriteBit(data & 1);
		return;

		case 0x30000a:
		case 0x30000b:
			EEPROMSetClockLine(data & 0x01);
		return;

		case 0x30000c:
		case 0x30000d:
			EEPROMSetCSLine(data & 0x01);
		return;

		case 0x500000:
		case 0x500001:
			// coin counter
		return;

		case 0x30004a:
		case 0x30004b:
		return; // NOP - maniacsq

		case 0x500006:
		case 0x500007:
		return; // NOP - alligator
	}
}

static void __fastcall gaelco2_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xff0000) == 0x310000) {
		snowboar_protection_w(address - 0x310000, data);
		return;
	}

	switch (address)
	{
		case 0x218004:
		case 0x218006:
		case 0x218008:
			DrvVidRegs[(address - 0x218004) / 2] = data;
		return;

		case 0x500000:
			// coin counter
		return;

		case 0x300008:
			EEPROMWriteBit(data & 1);
		return;

		case 0x30000a:
			EEPROMSetClockLine(data & 0x01);
		return;

		case 0x30000c:
			EEPROMSetCSLine(data & 0x01);
		return;

		case 0x30004a:
		return; // NOP - maniacsq

		case 0x500006:
		return; // NOP - alligator
	}
}

static UINT8 __fastcall gaelco2_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x218004:
		case 0x218005:
		case 0x218006:
		case 0x218007:
		case 0x218008:
		case 0x218009: {
			UINT8 *r = (UINT8*)DrvVidRegs;
			return r[(address - 0x218004) ^ 1];
		}

		case 0x300000:
		case 0x300001:
			return DrvInputs[0] >> ((~address & 1) * 8);

		case 0x300002:
		case 0x300003:
		case 0x300010:
		case 0x300011:
			return DrvInputs[1] >> ((~address & 1) * 8);


		case 0x300020:
		case 0x300021:
		case 0x320000:
		case 0x320001:
			return DrvInputs[2] >> ((~address & 1) * 8);
	}

	return 0;
}

static UINT16 __fastcall gaelco2_main_read_word(UINT32 address)
{
	if ((address & 0xff0000) == 0x310000) {
		return snowboar_protection_r();
	}

	switch (address)
	{
		case 0x218004:
		case 0x218006:
		case 0x218008:
			return DrvVidRegs[(address - 0x218004) / 2];

		case 0x300000:
			return DrvInputs[0];

		case 0x300002:
		case 0x300010:
			return DrvInputs[1];

		case 0x320000:
		case 0x300020:
			return (DrvInputs[2] & ~0x40) | (EEPROMRead() ? 0x40 : 0);
	}

	return 0;
}

static void __fastcall gaelco2_sound_write_byte(UINT32 address, UINT8 data)
{
	DrvSprRAM[(address & 0xffff) ^ 1] = data;

	if (address >= 0x202890 && address <= 0x2028ff) {
		// not used.
		return;
	}
}

static void __fastcall gaelco2_sound_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvSprRAM + (address & 0xfffe))) = data;

	if (address >= 0x202890 && address <= 0x2028ff) {
		gaelcosnd_w((address - 0x202890) >> 1, data);
		return;
	}
}

static UINT8 __fastcall gaelco2_sound_read_byte(UINT32 address)
{
	if (address >= 0x202890 && address <= 0x2028ff) {
		// not used.
	}
	return DrvSprRAM[(address & 0xffff) ^ 1];
}

static UINT16 __fastcall gaelco2_sound_read_word(UINT32 address)
{
	if (address >= 0x202890 && address <= 0x2028ff) {
		return gaelcosnd_r((address - 0x202890) >> 1);
	}
	return *((UINT16*)(DrvSprRAM + (address & 0xfffe)));
}

static void palette_update(INT32 offset)
{
	static const int pen_color_adjust[16] = {
		0, -8, -16, -24, -32, -40, -48, -56, 64, 56, 48, 40, 32, 24, 16, 8
	};

	offset = (offset & 0x1ffe);

	UINT16 color = *((UINT16*)(DrvPalRAM + offset));

	INT32 r = (color >> 10) & 0x1f;
	INT32 g = (color >>  5) & 0x1f;
	INT32 b = (color >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r,g,b,0);

	if (offset >= 0x211fe0) return;

#define ADJUST_COLOR(c) ((c < 0) ? 0 : ((c > 255) ? 255 : c))

	for (INT32 i = 1; i < 16; i++) {
		INT32 auxr = ADJUST_COLOR(r + pen_color_adjust[i]);
		INT32 auxg = ADJUST_COLOR(g + pen_color_adjust[i]);
		INT32 auxb = ADJUST_COLOR(b + pen_color_adjust[i]);

		DrvPalette[0x1000 + (offset/2) * i] = BurnHighCol(auxr,auxg,auxb,0);
	}
}

static void __fastcall gaelco2_palette_write_byte(UINT32 address, UINT8 data)
{
	DrvPalRAM[(address & 0x1fff) ^ 1] = data;
	palette_update(address);
}

static void __fastcall gaelco2_palette_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvPalRAM + (address & 0x1ffe))) = data;
	palette_update(address);
}

static void pIRQLine6Callback(INT32 line)
{
	if (line == 255) // ??
		SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
}

/*
static void pBangIRQLineCallback(INT32 line)
{

}
*/

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	EEPROMReset();

	HiscoreReset();

	gaelcosnd_reset();

	snowboar_latch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x0100000;

	DrvGfxROM0	= Next; Next += 0x1400000;
	DrvGfxROM	= Next; Next += 0x2000000;

	DrvPalette	= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);

	AllRam		= Next;

	DrvSprRAM	= Next; Next += 0x0010000;
	DrvSprBuf	= Next; Next += 0x0010000;
	DrvPalRAM	= Next; Next += 0x0002000;
	Drv68KRAM	= Next; Next += 0x0020000;
	Drv68KRAM2	= Next; Next += 0x0020000;

	DrvVidRegs	= (UINT16*)Next; Next += 0x00003 * sizeof(UINT16);

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void DrvGfxDecode(INT32 size)
{
	INT32 Planes[5] = { (size/5)*8*4, (size/5)*8*3, (size/5)*8*2, (size/5)*8*1, (size/5)*8*0 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	GfxDecode(((size*8)/5)/(16*16), 5, 16, 16, Planes, XOffs, YOffs, 0x100, DrvGfxROM0, DrvGfxROM);

	gfxmask = (((size * 8) / 5) / (16 * 16)) - 1;
}

static void gaelco2_split_gfx(UINT8 *src, UINT8 *dst, INT32 start, INT32 length, INT32 dest1, INT32 dest2)
{
	for (INT32 i = 0; i < length/2; i++){
		dst[dest1 + i] = src[start + i*2 + 0];
		dst[dest2 + i] = src[start + i*2 + 1];
	}
}


static const eeprom_interface gaelco2_eeprom_interface =
{
	8,				/* address bits */
	16,				/* data bits */
	"*110",			/* read command */
	"*101",			/* write command */
	"*111",			/* erase command */
	"*10000xxxxxx",	/* lock command */
	"*10011xxxxxx", /* unlock command */
	0,//  "*10001xxxxxx", /* write all */
	0//  "*10010xxxxxx", /* erase all */
};

static INT32 DrvInit(INT32 game_select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	switch (game_select)
	{
		case 0:	// aligatorun
		{
			if (BurnLoadRom(Drv68KROM  + 0x000001, 0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x000000, 1, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000, 2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x400000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x800000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0xc00000, 5, 1)) return 1;

			memset (DrvGfxROM0, 0, 0x1400000);

			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0000000, 0x0400000, 0x0000000, 0x0400000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0400000, 0x0400000, 0x0200000, 0x0600000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0800000, 0x0400000, 0x0800000, 0x0c00000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0c00000, 0x0400000, 0x0a00000, 0x0e00000);

			memset (DrvGfxROM, 0, 0x2000000);

			DrvGfxDecode(0x1400000);

			nCPUClockSpeed = 13000000;
			pIRQCallback = pIRQLine6Callback;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0400000, 1 * 0x0400000, 2 * 0x0400000, 3 * 0x0400000);
		}
		break;

		case 1:	// maniacsq
		{
			if (BurnLoadRom(Drv68KROM  + 0x000001, 0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x000000, 1, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x000000, 2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x080000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x100000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x180000, 5, 1)) return 1;

			DrvGfxDecode(0x280000);

			nCPUClockSpeed = 13000000;
			pIRQCallback = pIRQLine6Callback;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0080000, 1 * 0x0080000, 0, 0);
		}
		break;

		case 2:	// snowboara
		{
			if (BurnLoadRom(Drv68KROM  + 0x000001, 0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x000000, 1, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x1000000, 2, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x000000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x400000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x800000, 5, 1)) return 1;

			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0000000, 0x0400000, 0x0000000, 0x0400000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0400000, 0x0400000, 0x0200000, 0x0600000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0800000, 0x0400000, 0x0800000, 0x0c00000);

			DrvGfxDecode(0x1400000);

			nCPUClockSpeed = 15000000;
			pIRQCallback = pIRQLine6Callback;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0400000, 1 * 0x0400000, 0, 0);
		}
		break;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x210000, 0x211fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xfe0000, 0xffffff, MAP_RAM);
	if (game_select == 2) // snowboard
		SekMapMemory(Drv68KRAM2, 0x212000, 0x213fff, MAP_RAM); // ??? -dink

	SekSetWriteWordHandler(0,	gaelco2_main_write_word);
	SekSetWriteByteHandler(0,	gaelco2_main_write_byte);
	SekSetReadWordHandler(0,	gaelco2_main_read_word);
	SekSetReadByteHandler(0,	gaelco2_main_read_byte);

	SekMapHandler(1,		0x202800, 0x202bff, MAP_WRITE | MAP_READ);
	SekSetWriteWordHandler(1,	gaelco2_sound_write_word);
	SekSetWriteByteHandler(1,	gaelco2_sound_write_byte);
	SekSetReadWordHandler(1,	gaelco2_sound_read_word);
	SekSetReadByteHandler(1,	gaelco2_sound_read_byte);

	SekMapHandler(2,		0x210000, 0x211fff, MAP_WRITE);
	SekSetWriteWordHandler(2,	gaelco2_palette_write_word);
	SekSetWriteByteHandler(2,	gaelco2_palette_write_byte);
	SekClose();

	EEPROMInit(&gaelco2_eeprom_interface);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	EEPROMExit();

	BurnFree (AllMem);

	gaelcosnd_exit();

	return 0;
}

static void draw_layer(INT32 layer)
{
	INT32 offset = ((DrvVidRegs[layer] >> 9) & 0x07) * 0x1000;

	UINT16 *ram = (UINT16*)DrvSprRAM;

	INT32 scrolly = (ram[(0x2800 + (layer * 4))/2] + 0x01 - global_y_offset) & 0x1ff;

	if ((DrvVidRegs[layer] & 0x8000) == 0)
	{
		INT32 scrollx = (ram[(0x2802 + (layer * 4))/2] + 0x10 + (layer ? 0 : 4)) & 0x3ff;

		for (INT32 offs = 0; offs < 64 * 32; offs++)
		{
			INT32 sx = (offs & 0x3f) * 16;
			INT32 sy = (offs / 0x40) * 16;

			sx -= scrollx;
			if (sx < -15) sx += 1024;
			sy -= scrolly;
			if (sy < -15) sy += 512;

			if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

			INT32 attr0 = ram[offset + (offs * 2) + 0];
			INT32 attr1 = ram[offset + (offs * 2) + 1];

			INT32 code  = (attr1 + ((attr0 & 0x07) << 16)) & gfxmask;

			INT32 color = (attr0 & 0xfe00) >> 9;
			INT32 flipx = (attr0 & 0x0080) ? 0xf : 0;
			INT32 flipy = (attr0 & 0x0040) ? 0xf : 0;

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 5, 0, 0, DrvGfxROM);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 5, 0, 0, DrvGfxROM);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 5, 0, 0, DrvGfxROM);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 5, 0, 0, DrvGfxROM);
				}
			}
		}
	}
	else
	{
		for (INT32 sy = 0; sy < nScreenHeight; sy++)
		{
			INT32 yy = (sy + scrolly) & 0x1ff;

			INT32 scrollx = (ram[(0x2802 + (layer * 4))/2] + 0x10 + (layer ? 0 : 4)) & 0x3ff;
			if (DrvVidRegs[layer] & 0x8000) {
				if (layer) {
					scrollx = (ram[(0x2400/2) + sy] + 0x10) & 0x3ff;
				} else {
					scrollx = (ram[(0x2000/2) + sy] + 0x14) & 0x3ff;
				}
			}

			UINT16 *dst = pTransDraw + sy * nScreenWidth;

			for (INT32 sx = 0; sx < nScreenWidth + 16; sx += 16)
			{
				INT32 sxx = (sx + scrollx) & 0x3ff;
				INT32 xx = sx - (scrollx & 0xf);

				INT32 index = ((sxx / 16) + ((yy / 16) * 64)) * 2;

				INT32 attr0 = ram[offset + index + 0];
				INT32 attr1 = ram[offset + index + 1];

				INT32 code  = (attr1 + ((attr0 & 0x07) << 16)) & gfxmask;

				INT32 color = ((attr0 >> 9) & 0x7f) * 0x20;
				INT32 flipx = (attr0 & 0x80) ? 0xf : 0;
				INT32 flipy = (attr0 & 0x40) ? 0xf : 0;

				UINT8 *gfx = DrvGfxROM + (code * 256) + (((yy & 0x0f) ^ flipy) * 16);

				for (INT32 x = 0; x < 16; x++, xx++)
				{
					if (xx >= 0 && xx < nScreenWidth) {
						if (gfx[x^flipx]) {
							dst[xx] = gfx[x^flipx] + color;
						}
					}
				}
			}
		}
	}
}

static void draw_sprites(INT32 mask, INT32 xoffs)
{
	UINT16 *buffered_spriteram16 = (UINT16*)DrvSprBuf;
	int j, x, y, ex, ey, px, py;

	INT32 start_offset = (DrvVidRegs[1] & 0x10)*0x100;
	INT32 end_offset = start_offset + 0x1000;

	INT32 spr_x_adjust = ((nScreenWidth-1) - 320 + 1) - (511 - 320 - 1) - ((DrvVidRegs[0] >> 4) & 0x01) + xoffs;

	for (j = start_offset; j < end_offset; j += 8)
	{
		int data = buffered_spriteram16[(j/2) + 0];
		int data2 = buffered_spriteram16[(j/2) + 1];
		int data3 = buffered_spriteram16[(j/2) + 2];
		int data4 = buffered_spriteram16[(j/2) + 3];

		int sx = data3 & 0x3ff;
		int sy = data2 & 0x1ff;

		int xflip = data2 & 0x800;
		int yflip = data2 & 0x400;

		int xsize = ((data3 >> 12) & 0x0f) + 1;
		int ysize = ((data2 >> 12) & 0x0f) + 1;

		if (m_dual_monitor && ((data & 0x8000) != mask)) continue;

		if ((data2 & 0x0200) != 0)
		{
			for (y = 0; y < ysize; y++)
			{
				for (x = 0; x < xsize; x++)
				{
					int data5 = buffered_spriteram16[((data4/2) + (y*xsize + x)) & 0x7fff];
					int number = (((data & 0x1ff) << 10) + (data5 & 0x0fff)) & gfxmask;
					int color = ((data >> 9) & 0x7f) + ((data5 >> 12) & 0x0f);
					int color_effect = m_dual_monitor ? ((color & 0x3f) == 0x3f) : (color == 0x7f);

					ex = xflip ? (xsize - 1 - x) : x;
					ey = yflip ? (ysize - 1 - y) : y;

					if (color_effect == 0) // iq_132
					{
						if (yflip) {
							if (xflip) {
								Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, number, ((sx + ex*16) & 0x3ff) + spr_x_adjust, ((sy + ey*16) & 0x1ff) + global_y_offset, color, 5, 0, 0, DrvGfxROM);
							} else {
								Render16x16Tile_Mask_FlipY_Clip(pTransDraw, number, ((sx + ex*16) & 0x3ff) + spr_x_adjust, ((sy + ey*16) & 0x1ff) + global_y_offset, color, 5, 0, 0, DrvGfxROM);
							}
						} else {
							if (xflip) {
								Render16x16Tile_Mask_FlipX_Clip(pTransDraw, number, ((sx + ex*16) & 0x3ff) + spr_x_adjust, ((sy + ey*16) & 0x1ff) + global_y_offset, color, 5, 0, 0, DrvGfxROM);
							} else {
								Render16x16Tile_Mask_Clip(pTransDraw, number, ((sx + ex*16) & 0x3ff) + spr_x_adjust, ((sy + ey*16) & 0x1ff) + global_y_offset, color, 5, 0, 0, DrvGfxROM);
							}
						}
					} else {
						const UINT8 *gfx_src = DrvGfxROM + number * 0x100;

						for (py = 0; py < 16; py++)
						{
							int ypos = ((sy + ey*16 + py) & 0x1ff) + global_y_offset;
							if ((ypos < 0) || (ypos >= nScreenHeight)) continue;

							UINT16 *srcy = pTransDraw  + ypos * nScreenWidth;

							int gfx_py = yflip ? (16 - 1 - py) : py;

							for (px = 0; px < 16; px++)
							{
								int xpos = (((sx + ex*16 + px) & 0x3ff) + spr_x_adjust) & 0x3ff;
								UINT16 *pixel = srcy + xpos;
								int src_color = *pixel;

								int gfx_px = xflip ? (16 - 1 - px) : px;

								int gfx_pen = gfx_src[16*gfx_py + gfx_px];

								if ((gfx_pen == 0) || (gfx_pen >= 16)) continue;

								if ((xpos < 0) || (xpos >= nScreenWidth)) continue;

								*pixel = src_color + 4096*gfx_pen;
							}
						}
					}
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x2000; i+=2) {
			palette_update(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer(1);
	if (nBurnLayer & 2) draw_layer(0);
	if (nBurnLayer & 4) draw_sprites(0,0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, 4 * sizeof(UINT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		DrvInputs[0] = (DrvInputs[0] & 0xff) | (DrvDips[0] << 8);
		DrvInputs[1] = (DrvInputs[1] & 0xff) | (DrvDips[1] << 8);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { (nCPUClockSpeed * 10) / 591 }; // ?? MHZ @ 59.1 HZ
	INT32 nCyclesDone[1] = { 0 };
	INT32 nSegment = 0;

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = (nCyclesTotal[0] - nCyclesDone[0]) / (nInterleave - i);
		nCyclesDone[0] += SekRun(nSegment);

		pIRQCallback(i);

	}

	if (pBurnSoundOut) {
		gaelcosnd_update(pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x10000);

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

		EEPROMScan(nAction, pnMin);

		SCAN_VAR(snowboar_latch);
		gaelcosnd_scan();
	}

	return 0;
}


// Maniac Square (unprotected)

static struct BurnRomInfo maniacsqRomDesc[] = {
	{ "d8-d15.1m",		0x020000, 0x9121d1b6, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "d0-d7.1m",		0x020000, 0xa95cfd2a, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "d0-d7.4m",		0x080000, 0xd8551b2f, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "d8-d15.4m",		0x080000, 0xb269c427, 1 | BRF_GRA },           //  3
	{ "d16-d23.1m",		0x020000, 0xaf4ea5e7, 1 | BRF_GRA },           //  4
	{ "d24-d31.1m",		0x020000, 0x578c3588, 1 | BRF_GRA },           //  5
};

STD_ROM_PICK(maniacsq)
STD_ROM_FN(maniacsq)

static INT32 maniacsqInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvManiacsq = {
	"maniacsq", NULL, NULL, NULL, "1996",
	"Maniac Square (unprotected)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, maniacsqRomInfo, maniacsqRomName, NULL, NULL, ManiacsqInputInfo, ManiacsqDIPInfo,
	maniacsqInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Alligator Hunt

static struct BurnRomInfo aligatorRomDesc[] = {
	{ "u45",  		0x080000, 0x61c47c56, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u44",  		0x080000, 0xf0be007a, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "u48",		0x400000, 0x19e03bf1, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "u47",		0x400000, 0x74a5a29f, 1 | BRF_GRA },           //  3
	{ "u50",		0x400000, 0x85daecf9, 1 | BRF_GRA },           //  4
	{ "u49",		0x400000, 0x70a4ee0b, 1 | BRF_GRA },           //  5
	
	{ "aligator_ds5002fp.bin", 0x080000, 0x00000000, BRF_OPT | BRF_NODUMP },
};

STD_ROM_PICK(aligator)
STD_ROM_FN(aligator)

static INT32 aligatorInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvAligator = {
	"aligator", NULL, NULL, NULL, "1994",
	"Alligator Hunt\0", "Play the unprotected clone, instead!", "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, aligatorRomInfo, aligatorRomName, NULL, NULL, AlighuntInputInfo, AlighuntDIPInfo,
	aligatorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Alligator Hunt (unprotected)

static struct BurnRomInfo aligatorunRomDesc[] = {
	{ "ahntu45n.040",	0x080000, 0xfc02cb2d, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ahntu44n.040",	0x080000, 0x7fbea3a3, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "u48",		0x400000, 0x19e03bf1, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "u47",		0x400000, 0x74a5a29f, 1 | BRF_GRA },           //  3
	{ "u50",		0x400000, 0x85daecf9, 1 | BRF_GRA },           //  4
	{ "u49",		0x400000, 0x70a4ee0b, 1 | BRF_GRA },           //  5
};

STD_ROM_PICK(aligatorun)
STD_ROM_FN(aligatorun)

struct BurnDriver BurnDrvAligatorun = {
	"aligatorun", "aligator", NULL, NULL, "1994",
	"Alligator Hunt (unprotected)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, aligatorunRomInfo, aligatorunRomName, NULL, NULL, AlighuntInputInfo, AlighuntDIPInfo,
	aligatorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};



// Snow Board Championship (Version 2.0)

static struct BurnRomInfo snowboaraRomDesc[] = {
	{ "sb53",	0x080000, 0xe4eaefd4, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "sb55",	0x080000, 0xe2476994, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "sb43",	0x200000, 0xafce54ed, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "sb44",	0x400000, 0x1bbe88bc, 1 | BRF_GRA },           //  3
	{ "sb45",	0x400000, 0x373983d9, 1 | BRF_GRA },           //  4
	{ "sb46",	0x400000, 0x22e7c648, 1 | BRF_GRA },           //  5
};

STD_ROM_PICK(snowboara)
STD_ROM_FN(snowboara)

static INT32 snowboaraInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvSnowboara = {
	"snowboara", NULL, NULL, NULL, "1994",
	"Snow Board Championship (Version 2.0)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, snowboaraRomInfo, snowboaraRomName, NULL, NULL, SnowboarInputInfo, SnowboarDIPInfo,
	snowboaraInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};

