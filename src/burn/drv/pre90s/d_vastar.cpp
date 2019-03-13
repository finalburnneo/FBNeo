// FB Alpha Vastar driver module
// Based on MAME driver by Allard Van Der Bas

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvVidRAM2;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static UINT8 flipscreen;
static UINT8 nmi_mask;
static UINT8 sprite_priority;
static UINT8 sound_reset;

static INT32 watchdog;

static struct BurnInputInfo VastarInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Vastar)

static struct BurnInputInfo PprobeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pprobe)


static struct BurnDIPInfo VastarDIPList[]=
{
	{0x11, 0xff, 0xff, 0xbf, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x11, 0x01, 0x03, 0x03, "3"				},
	{0x11, 0x01, 0x03, 0x02, "4"				},
	{0x11, 0x01, 0x03, 0x01, "5"				},
	{0x11, 0x01, 0x03, 0x00, "6"				},

	{0   , 0xfe, 0   ,    2, "Show Author Credits"		},
	{0x11, 0x01, 0x08, 0x08, "No"				},
	{0x11, 0x01, 0x08, 0x00, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Slow Motion (Cheat)"		},
	{0x11, 0x01, 0x10, 0x10, "Off"				},
	{0x11, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x11, 0x01, 0x20, 0x20, "20000 50000"			},
	{0x11, 0x01, 0x20, 0x00, "40000 70000"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x11, 0x01, 0x40, 0x00, "Upright"			},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x11, 0x01, 0x80, 0x80, "Off"				},
	{0x11, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x12, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x01, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x12, 0x01, 0x38, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x38, 0x20, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
};

STDDIPINFO(Vastar)

static struct BurnDIPInfo Vastar4DIPList[]=
{
	{0x11, 0xff, 0xff, 0xbf, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x11, 0x01, 0x03, 0x03, "1"				},
	{0x11, 0x01, 0x03, 0x02, "2"				},
	{0x11, 0x01, 0x03, 0x01, "3"				},
	{0x11, 0x01, 0x03, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x11, 0x01, 0x04, 0x00, "Off"				},
	{0x11, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    2, "Show Author Credits"		},
	{0x11, 0x01, 0x08, 0x08, "No"				},
	{0x11, 0x01, 0x08, 0x00, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Slow Motion (Cheat)"		},
	{0x11, 0x01, 0x10, 0x10, "Off"				},
	{0x11, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x11, 0x01, 0x20, 0x20, "20000 50000"			},
	{0x11, 0x01, 0x20, 0x00, "40000 70000"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x11, 0x01, 0x40, 0x00, "Upright"			},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Freeze"			},
	{0x11, 0x01, 0x80, 0x80, "Off"				},
	{0x11, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x12, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x01, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x12, 0x01, 0x38, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x38, 0x20, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
};

STDDIPINFO(Vastar4)

static struct BurnDIPInfo PprobeDIPList[]=
{
	{0x12, 0xff, 0xff, 0xbd, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x03, 0x03, "1"				},
	{0x12, 0x01, 0x03, 0x02, "2"				},
	{0x12, 0x01, 0x03, 0x01, "3"				},
	{0x12, 0x01, 0x03, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Player Controls Demo (Cheat)"	},
	{0x12, 0x01, 0x04, 0x04, "Off"				},
	{0x12, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x12, 0x01, 0x08, 0x08, "Off"				},
	{0x12, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x12, 0x01, 0x20, 0x20, "20000 then every 40000"	},
	{0x12, 0x01, 0x20, 0x00, "30000 then every 70000"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x12, 0x01, 0x40, 0x00, "Upright"			},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Rom Test / STOP"		},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,   16, "Coin A"			},
	{0x13, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"		},
	{0x13, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"		},
	{0x13, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"		},
	{0x13, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"		},
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x13, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x13, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"		},
	{0x13, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,   16, "Coin B"			},
	{0x13, 0x01, 0xf0, 0x00, "5 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"		},
	{0x13, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"		},
	{0x13, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"		},
	{0x13, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"		},
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x13, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x13, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"		},
};

STDDIPINFO(Pprobe)

static void __fastcall vastar_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
			sprite_priority = data;
		return;

		case 0xe000:
			watchdog = 0;
		return;
	}
}

static UINT8 __fastcall vastar_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			watchdog = 0;
			return 0;
	}

	return 0;
}

static void __fastcall vastar_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0x0f)
	{
		case 0x00:
			nmi_mask = data & 0x01;
		return;

		case 0x01:
			flipscreen = data & 0x01;
		return;

		case 0x02:
			sound_reset = ~data & 0x01;
			if (sound_reset) {
				ZetReset(1);
			}
		return;
	}
}

static UINT8 __fastcall vastar_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8000:
			return DrvInputs[1];

		case 0x8040:
			return DrvInputs[0];

		case 0x8080:
			return DrvInputs[2];
	}

	return 0;
}

static void __fastcall vastar_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0x0f)
	{
		case 0x00:
		case 0x01:
			AY8910Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall vastar_sound_read_port(UINT16 port)
{
	switch (port & 0x0f)
	{
		case 0x02:
			return AY8910Read(0);
	}

	return 0;
}

static UINT8 vastar_ay8910_read_A(UINT32)
{
	return DrvDips[0];
}

static UINT8 vastar_ay8910_read_B(UINT32)
{
	return DrvDips[1];
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);

	watchdog = 0;
	sound_reset = 1;

	flipscreen = 0;
	nmi_mask = 0;
	sprite_priority = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x008000;
	DrvGfxROM3		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000300;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam			= Next;

	DrvShareRAM		= Next; Next += 0x000800;
	DrvVidRAM0		= Next; Next += 0x001000;
	DrvVidRAM1		= Next; Next += 0x001000;
	DrvVidRAM2		= Next; Next += 0x000c00;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Planes[2] = { 0, 4 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x200, 2,  8,  8, Planes, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x100, 2, 16, 16, Planes, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x2000);

	GfxDecode(0x200, 2,  8,  8, Planes, XOffs, YOffs, 0x080, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x2000);

	GfxDecode(0x200, 2,  8,  8, Planes, XOffs, YOffs, 0x080, tmp, DrvGfxROM3);

	BurnFree (tmp);
}

static inline void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
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

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvInit(INT32 load_type)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		switch (load_type)
		{
			case 0:
			{
				if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x01000,  1, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  2, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x03000,  3, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  4, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x05000,  5, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  6, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x07000,  7, 1)) return 1;

				if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  8, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM1 + 0x01000,  9, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM0 + 0x00000, 10, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM1 + 0x00000, 11, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM1 + 0x02000, 12, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM2 + 0x00000, 13, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM3 + 0x00000, 14, 1)) return 1;

				if (BurnLoadRom(DrvColPROM + 0x00000, 15, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x00100, 16, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x00200, 17, 1)) return 1;
			}
			break;

			case 1:
			{
				if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;

				if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  4, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM1 + 0x01000,  5, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM1 + 0x00000,  7, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM1 + 0x02000,  8, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM2 + 0x00000,  9, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM3 + 0x00000, 10, 1)) return 1;

				if (BurnLoadRom(DrvColPROM + 0x00000, 11, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x00100, 12, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x00200, 13, 1)) return 1;
			}
			break;

			case 2:
			{
				if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
				if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;

				if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  4, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 1)) return 1;
				if (BurnLoadRom(DrvGfxROM1 + 0x02000,  7, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM2 + 0x00000,  8, 1)) return 1;

				if (BurnLoadRom(DrvGfxROM3 + 0x00000,  9, 1)) return 1;

				if (BurnLoadRom(DrvColPROM + 0x00000, 10, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x00100, 11, 1)) return 1;
				if (BurnLoadRom(DrvColPROM + 0x00200, 12, 1)) return 1;
			}
			break;
		}

		DrvPaletteInit();
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM1,	0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,	0x9000, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,	0xa000, 0xafff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,	0xb000, 0xbfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM2,	0xc400, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(vastar_main_write);
	ZetSetReadHandler(vastar_main_read);
	ZetSetOutHandler(vastar_main_write_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,	0x4000, 0x47ff, MAP_RAM);
	ZetSetReadHandler(vastar_sound_read);
	ZetSetOutHandler(vastar_sound_write_port);
	ZetSetInHandler(vastar_sound_read_port);
	ZetClose();

	AY8910Init(0, 1536000, 0);
	AY8910SetPorts(0, &vastar_ay8910_read_A, &vastar_ay8910_read_B, NULL, NULL);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	AY8910Exit(0);

	BurnFree(AllMem);

	return 0;
}

static void draw_layer(UINT8 *ram, UINT8 *gfx, UINT8 *cscroll, INT32 ofst0, INT32 ofst1, INT32 opaque)
{
	INT32 trans = (opaque) ? 0xff : 0;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		if (cscroll) {
			sy -= (cscroll[sx/8] + 16) & 0xff;
			if (sy < -7) sy += 256;
		} else {
			sy -= 16;
			if (sy < -7) sy += 256;
		}

		if (sy >= nScreenHeight) continue;

		INT32 attr  = ram[offs + ofst0];
		INT32 code  = ram[offs + 0x800] + ((attr & 0x01) * 256);
		INT32 color = ram[offs + ofst1] & 0x3f;
		INT32 flipx = attr & 0x08;
		INT32 flipy = attr & 0x04;

		if (flipscreen) {
			sy = 216 - sy;
			sx = 248 - sx;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, trans, 0, gfx);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, trans, 0, gfx);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, trans, 0, gfx);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, trans, 0, gfx);
			}
		}
	}
}

static void draw_sprite_tile(INT32 code, INT32 sx, INT32 sy, INT32 color, INT32 flipx, INT32 flipy)
{
	sy -= 16;

	if (flipy) {
		if (flipx) {
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
		} else {
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
		}
	} else {
		if (flipx) {
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
		} else {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1);
		}
	}
}

static void draw_sprites()
{
	UINT8 *spriteram   = DrvVidRAM2 + 0x000;
	UINT8 *spriteram_2 = DrvVidRAM2 + 0x400;
	UINT8 *spriteram_3 = DrvVidRAM2 + 0x800;

	for (INT32 offs = 0x40-2; offs >=0; offs -= 2)
	{
		INT32 code, sx, sy, color, flipx, flipy;

		code = ((spriteram_3[offs] & 0xfc) >> 2) + ((spriteram_2[offs] & 0x01) << 6)
				+ ((offs & 0x20) << 2);

		sx = spriteram_3[offs + 1];
		sy = spriteram[offs];
		color = spriteram[offs + 1] & 0x3f;
		flipx = spriteram_3[offs] & 0x02;
		flipy = spriteram_3[offs] & 0x01;

		if (flipscreen)
		{
			flipx = !flipx;
			flipy = !flipy;
		}

		if (spriteram_2[offs] & 0x08)
		{
			if (!flipscreen)
				sy = 224 - sy;

			if (flipy) {
				draw_sprite_tile((code & ~1) | 1, sx, sy,            color, flipx, flipy);
				draw_sprite_tile((code & ~1) | 0, sx, sy + 16,       color, flipx, flipy);
				draw_sprite_tile((code & ~1) | 1, sx, sy + 256,      color, flipx, flipy);
				draw_sprite_tile((code & ~1) | 0, sx, sy + 16 + 256, color, flipx, flipy);
			} else {
				draw_sprite_tile((code & ~1) | 0, sx, sy,            color, flipx, flipy);
				draw_sprite_tile((code & ~1) | 1, sx, sy+ 16,        color, flipx, flipy);
				draw_sprite_tile((code & ~1) | 0, sx, sy + 256,      color, flipx, flipy);
				draw_sprite_tile((code & ~1) | 1, sx, sy + 16 + 256, color, flipx, flipy);
			}
		}
		else
		{
			if (!flipscreen)
				sy = 240 - sy;

			draw_sprite_tile(code, sx, sy, color, flipx, flipy);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) draw_layer(DrvVidRAM0, DrvGfxROM3, DrvVidRAM2 + 0x3c0, 0, 0xc00, 1);

	switch (sprite_priority)
	{
		case 0:
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 2) draw_layer(DrvVidRAM1, DrvGfxROM2, DrvVidRAM2 + 0x3e0, 0, 0xc00, 0);
			if (nBurnLayer & 4) draw_layer(DrvVidRAM2, DrvGfxROM0, NULL, 0x400, 0, 0);
		break;

		case 1:
			if (nBurnLayer & 2) draw_layer(DrvVidRAM1, DrvGfxROM2, DrvVidRAM2 + 0x3e0, 0, 0xc00, 0);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 4) draw_layer(DrvVidRAM2, DrvGfxROM0, NULL, 0x400, 0, 0);
		break;

		case 2:
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 1) draw_layer(DrvVidRAM0, DrvGfxROM3, DrvVidRAM2 + 0x3c0, 0, 0xc00, 0);
			if (nBurnLayer & 2) draw_layer(DrvVidRAM1, DrvGfxROM2, DrvVidRAM2 + 0x3e0, 0, 0xc00, 0);
			if (nBurnLayer & 4) draw_layer(DrvVidRAM2, DrvGfxROM0, NULL, 0x400, 0, 0);
		break;

		case 3:
			if (nBurnLayer & 2) draw_layer(DrvVidRAM1, DrvGfxROM2, DrvVidRAM2 + 0x3e0, 0, 0xc00, 0);
			if (nBurnLayer & 4) draw_layer(DrvVidRAM2, DrvGfxROM0, NULL, 0x400, 0, 0);
			if (nSpriteEnable & 1) draw_sprites();
		break;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 120) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2]  = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(((nCyclesTotal[0] * (i + 1)) / nInterleave) - nCyclesDone[0]);
		if (i == (nInterleave - 1) && nmi_mask) ZetNmi();
		ZetClose();

		if (sound_reset)
		{
			nCyclesDone[1] += ((nCyclesTotal[1] * (i + 1)) / nInterleave) - nCyclesDone[1];
		}
		else
		{
			ZetOpen(1);
			nCyclesDone[1] += ZetRun(((nCyclesTotal[1] * (i + 1)) / nInterleave) - nCyclesDone[1]);
			if ((i % (nInterleave / 4)) == ((nInterleave / 4) - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
			ZetClose();
		}
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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

		SCAN_VAR(flipscreen);
		SCAN_VAR(nmi_mask);
		SCAN_VAR(sprite_priority);
		SCAN_VAR(sound_reset);
	}

	return 0;
}


// Vastar (set 1)

static struct BurnRomInfo vastarRomDesc[] = {
	{ "e_f4.rom",		0x1000, 0x45fa5075, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "e_k4.rom",		0x1000, 0x84531982, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "e_h4.rom",		0x1000, 0x94a4f778, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "e_l4.rom",		0x1000, 0x40e4d57b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "e_j4.rom",		0x1000, 0xbd607651, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "e_n4.rom",		0x1000, 0x7a3779a4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "e_n7.rom",		0x1000, 0x31b6be39, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "e_n5.rom",		0x1000, 0xf63f0e78, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "e_f2.rom",		0x1000, 0x713478d8, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code
	{ "e_j2.rom",		0x1000, 0xe4535442, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "c_c9.rom",		0x2000, 0x34f067b6, 3 | BRF_GRA },           // 10 Foreground Tiles

	{ "c_f7.rom",		0x2000, 0xedbf3b13, 4 | BRF_GRA },           // 11 Sprites
	{ "c_f9.rom",		0x2000, 0x8f309e22, 4 | BRF_GRA },           // 12

	{ "c_n4.rom",		0x2000, 0xb5f9c866, 5 | BRF_GRA },           // 13 Background #2 Tiles

	{ "c_s4.rom",		0x2000, 0xc9fbbfc9, 6 | BRF_GRA },           // 14 Background #1 Tiles

	{ "tbp24s10.6p",	0x0100, 0xa712d73a, 7 | BRF_GRA },           // 15 Color PROMs
	{ "tbp24s10.6s",	0x0100, 0x0a7d48ec, 7 | BRF_GRA },           // 16
	{ "tbp24s10.6m",	0x0100, 0x4c3db907, 7 | BRF_GRA },           // 17

	{ "tbp24s10.8n",	0x0100, 0xb5297a3b, 8 | BRF_GRA },           // 18 Unknown PROM
};

STD_ROM_PICK(vastar)
STD_ROM_FN(vastar)

static INT32 vastarInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvVastar = {
	"vastar", NULL, NULL, NULL, "1983",
	"Vastar (set 1)\0", NULL, "Sesame Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, vastarRomInfo, vastarRomName, NULL, NULL, NULL, NULL, VastarInputInfo, VastarDIPInfo,
	vastarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Vastar (set 2)

static struct BurnRomInfo vastar2RomDesc[] = {
	{ "3.4f",		0x1000, 0x6741ff9c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "6.4k",		0x1000, 0x5027619b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.4h",		0x1000, 0xfdaa44e6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "7.4l",		0x1000, 0x29bef91c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.4j",		0x1000, 0xc17c2458, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "8.4n",		0x1000, 0x8ca25c37, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "10.6n",		0x1000, 0x80df74ba, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "9.5n",		0x1000, 0x239ec84e, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "e_f2.rom",		0x1000, 0x713478d8, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code
	{ "e_j2.rom",		0x1000, 0xe4535442, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "c_c9.rom",		0x2000, 0x34f067b6, 3 | BRF_GRA },           // 10 Foreground Tiles

	{ "c_f7.rom",		0x2000, 0xedbf3b13, 4 | BRF_GRA },           // 11 Sprites
	{ "c_f9.rom",		0x2000, 0x8f309e22, 4 | BRF_GRA },           // 12

	{ "c_n4.rom",		0x2000, 0xb5f9c866, 5 | BRF_GRA },           // 13 Background #2 Tiles

	{ "c_s4.rom",		0x2000, 0xc9fbbfc9, 6 | BRF_GRA },           // 14 Background #1 Tiles

	{ "tbp24s10.6p",	0x0100, 0xa712d73a, 7 | BRF_GRA },           // 15 Color PROMs
	{ "tbp24s10.6s",	0x0100, 0x0a7d48ec, 7 | BRF_GRA },           // 16
	{ "tbp24s10.6m",	0x0100, 0x4c3db907, 7 | BRF_GRA },           // 17

	{ "tbp24s10.8n",	0x0100, 0xb5297a3b, 8 | BRF_OPT },           // 18 Unknown PROM
};

STD_ROM_PICK(vastar2)
STD_ROM_FN(vastar2)

struct BurnDriver BurnDrvVastar2 = {
	"vastar2", "vastar", NULL, NULL, "1983",
	"Vastar (set 2)\0", NULL, "Sesame Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, vastar2RomInfo, vastar2RomName, NULL, NULL, NULL, NULL, VastarInputInfo, VastarDIPInfo,
	vastarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Vastar (set 3)

static struct BurnRomInfo vastar3RomDesc[] = {
	{ "vst_2.4f",		0x2000, 0xad4e512a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "vst_3.4h",		0x2000, 0x2276c5d0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vst_4.4j",		0x2000, 0xdeca2aa1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "vst_5.6n",		0x2000, 0x743ed1c7, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "vst_0.2f",		0x1000, 0x713478d8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "vst_1.2j",		0x1000, 0xe4535442, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "c_c9.rom",		0x2000, 0x34f067b6, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "c_f7.rom",		0x2000, 0xedbf3b13, 4 | BRF_GRA },           //  7 Sprites
	{ "c_f9.rom",		0x2000, 0x8f309e22, 4 | BRF_GRA },           //  8

	{ "c_n4.rom",		0x2000, 0xb5f9c866, 5 | BRF_GRA },           //  9 Background #2 Tiles

	{ "c_s4.rom",		0x2000, 0xc9fbbfc9, 6 | BRF_GRA },           // 10 Background #1 Tiles

	{ "tbp24s10.6p",	0x0100, 0xa712d73a, 7 | BRF_GRA },           // 11 Color PROMs
	{ "tbp24s10.6s",	0x0100, 0x0a7d48ec, 7 | BRF_GRA },           // 12
	{ "tbp24s10.6m",	0x0100, 0x4c3db907, 7 | BRF_GRA },           // 13

	{ "tbp24s10.8n",	0x0100, 0xb5297a3b, 8 | BRF_OPT },           // 14 Unknown PROM
};

STD_ROM_PICK(vastar3)
STD_ROM_FN(vastar3)

static INT32 vastar3Init()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvVastar3 = {
	"vastar3", "vastar", NULL, NULL, "1983",
	"Vastar (set 3)\0", NULL, "Sesame Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, vastar3RomInfo, vastar3RomName, NULL, NULL, NULL, NULL, VastarInputInfo, VastarDIPInfo,
	vastar3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Vastar (set 4)

static struct BurnRomInfo vastar4RomDesc[] = {
	{ "3.bin",		0x1000, 0xd2b8f177, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "e_k4.rom",		0x1000, 0x84531982, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "e_h4.rom",		0x1000, 0x94a4f778, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "e_l4.rom",		0x1000, 0x40e4d57b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "e_j4.rom",		0x1000, 0xbd607651, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "e_n4.rom",		0x1000, 0x7a3779a4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "e_n7.rom",		0x1000, 0x31b6be39, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "e_n5.rom",		0x1000, 0xf63f0e78, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "e_f2.rom",		0x1000, 0x713478d8, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code
	{ "e_j2.rom",		0x1000, 0xe4535442, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "c_c9.rom",		0x2000, 0x34f067b6, 3 | BRF_GRA },           // 10 Foreground Tiles

	{ "c_f7.rom",		0x2000, 0xedbf3b13, 4 | BRF_GRA },           // 11 Sprites
	{ "c_f9.rom",		0x2000, 0x8f309e22, 4 | BRF_GRA },           // 12

	{ "c_n4.rom",		0x2000, 0xb5f9c866, 5 | BRF_GRA },           // 13 Background #2 Tiles

	{ "c_s4.rom",		0x2000, 0xc9fbbfc9, 6 | BRF_GRA },           // 14 Background #1 Tiles

	{ "tbp24s10.6p",	0x0100, 0xa712d73a, 7 | BRF_GRA },           // 15 Color PROMs
	{ "tbp24s10.6s",	0x0100, 0x0a7d48ec, 7 | BRF_GRA },           // 16
	{ "tbp24s10.6m",	0x0100, 0x4c3db907, 7 | BRF_GRA },           // 17

	{ "tbp24s10.8n",	0x0100, 0xb5297a3b, 8 | BRF_OPT },           // 18 Unknown PROM
};

STD_ROM_PICK(vastar4)
STD_ROM_FN(vastar4)

struct BurnDriver BurnDrvVastar4 = {
	"vastar4", "vastar", NULL, NULL, "1983",
	"Vastar (set 4)\0", NULL, "Sesame Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, vastar4RomInfo, vastar4RomName, NULL, NULL, NULL, NULL, VastarInputInfo, Vastar4DIPInfo,
	vastarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Planet Probe (prototype?)

static struct BurnRomInfo pprobeRomDesc[] = {
	{ "pb2.bin",		0x2000, 0xa88592aa, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pb3.bin",		0x2000, 0xe4e20f74, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pb4.bin",		0x2000, 0x4e40e3fe, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pb5.bin",		0x2000, 0xb26ff0fd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pb1.bin",		0x2000, 0xcd624df9, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "pb9.bin",		0x2000, 0x82294dd6, 3 | BRF_GRA },           //  5 Foreground Tiles

	{ "pb8.bin",		0x2000, 0x8d809e45, 4 | BRF_GRA },           //  6 Sprites
	{ "pb10.bin",		0x2000, 0x895f9dd3, 4 | BRF_GRA },           //  7

	{ "pb6.bin",		0x2000, 0xff309239, 5 | BRF_GRA },           //  8 Background #2 Tiles

	{ "pb7.bin",		0x2000, 0x439978f7, 6 | BRF_GRA },           //  9 Background #1 Tiles

	{ "n82s129.3",		0x0100, 0xdfb6b97c, 7 | BRF_GRA },           // 10 Color PROMs
	{ "n82s129.1",		0x0100, 0x3cc696a2, 7 | BRF_GRA },           // 11
	{ "dm74s287.2",		0x0100, 0x64fea033, 7 | BRF_GRA },           // 12

	{ "mmi6301-1.bin",	0x0100, 0xb5297a3b, 8 | BRF_OPT },           // 13 Unknown PROM
};

STD_ROM_PICK(pprobe)
STD_ROM_FN(pprobe)

static INT32 pprobeInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvPprobe = {
	"pprobe", NULL, NULL, NULL, "1985",
	"Planet Probe (prototype?)\0", NULL, "Crux / Kyugo?", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, pprobeRomInfo, pprobeRomName, NULL, NULL, NULL, NULL, PprobeInputInfo, PprobeDIPInfo,
	pprobeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
