// FB Alpha M52 (Moon Patrol) driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "irem_sound.h"
#include "bitswap.h"
#include "resnet.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvM6803ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvTransTab;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 scrollx = 0;
static UINT8 bgcontrol = 0;
static UINT8 bg1xpos = 0;
static UINT8 bg1ypos = 0;
static UINT8 bg2xpos = 0;
static UINT8 bg2ypos = 0;
static UINT8 flipscreen = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 nExtraCycles[2];

static INT32 is_game; // 0 mpatrol/mranger, 1 alpha1v

static struct BurnInputInfo MpatrolInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Mpatrol)

static struct BurnInputInfo Alpha1vInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Alpha1v)

static struct BurnDIPInfo Alpha1vDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xea, NULL						},
	{0x01, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x03, "2"						},
	{0x00, 0x01, 0x03, 0x02, "3"						},
	{0x00, 0x01, 0x03, 0x01, "4"						},
	{0x00, 0x01, 0x03, 0x00, "5"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x04, 0x04, "Upright"					},
	{0x00, 0x01, 0x04, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x00, 0x01, 0x10, 0x10, "No"						},
	{0x00, 0x01, 0x10, 0x00, "Yes"						},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x01, 0x01, 0x0f, 0x0b, "4 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x0c, "3 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x0d, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    7, "Coin B"					},
	{0x01, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0xf0, 0xd0, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0xf0, 0xc0, "1 Coin  3 Credits"		},
	{0x01, 0x01, 0xf0, 0xb0, "1 Coin  4 Credits"		},
	{0x01, 0x01, 0xf0, 0xa0, "1 Coin  5 Credits"		},
	{0x01, 0x01, 0xf0, 0x90, "1 Coin  6 Credits"		},
	{0x01, 0x01, 0xf0, 0x80, "1 Coin  7 Credits"		},
};

STDDIPINFO(Alpha1v)

static struct BurnDIPInfo MpatrolDIPList[]=
{
	DIP_OFFSET(0x0e)
	{0x00, 0xff, 0xff, 0xfe, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x00, "1"						},
	{0x00, 0x01, 0x03, 0x01, "2"						},
	{0x00, 0x01, 0x03, 0x02, "3"						},
	{0x00, 0x01, 0x03, 0x03, "5"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x00, 0x01, 0x0c, 0x0c, "10000 30000 50000"		},
	{0x00, 0x01, 0x0c, 0x08, "20000 40000 60000"		},
	{0x00, 0x01, 0x0c, 0x04, "10000"					},
	{0x00, 0x01, 0x0c, 0x00, "None"						},

	{0   , 0xfe, 0   ,    15, "Coinage"					},
	{0x00, 0x01, 0xf0, 0x90, "7 Coins 1 Credits"		},
	{0x00, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"		},
	{0x00, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"		},
	{0x00, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"		},
	{0x00, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"		},
	{0x00, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0xf0, 0x60, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0xf0, 0x50, "1 Coin  4 Credits"		},
	{0x00, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
	{0x00, 0x01, 0xf0, 0x30, "1 Coin  6 Credits"		},
	{0x00, 0x01, 0xf0, 0x20, "1 Coin  7 Credits"		},
	{0x00, 0x01, 0xf0, 0x10, "1 Coin  8 Credits"		},
	{0x00, 0x01, 0xf0, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x01, 0x01, 0x01, 0x01, "Off"						},
	{0x01, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x02, 0x00, "Upright"					},
	{0x01, 0x01, 0x02, 0x02, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Coin Mode"				},
	{0x01, 0x01, 0x04, 0x04, "Mode 1"					},
	{0x01, 0x01, 0x04, 0x00, "Mode 2"					},

	{0   , 0xfe, 0   ,    2, "Stop Mode (Cheat)"		},
	{0x01, 0x01, 0x10, 0x10, "Off"						},
	{0x01, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Sector Selection (Cheat)"	},
	{0x01, 0x01, 0x20, 0x20, "Off"						},
	{0x01, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x01, 0x01, 0x40, 0x40, "Off"						},
	{0x01, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x01, 0x01, 0x80, 0x80, "Off"						},
	{0x01, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Mpatrol)

static struct BurnDIPInfo MpatrolwDIPList[]=
{
	DIP_OFFSET(0x0e)
	{0x00, 0xff, 0xff, 0xfe, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x00, "2"						},
	{0x00, 0x01, 0x03, 0x01, "3"						},
	{0x00, 0x01, 0x03, 0x02, "4"						},
	{0x00, 0x01, 0x03, 0x03, "5"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x00, 0x01, 0x0c, 0x0c, "10000 30000 50000"		},
	{0x00, 0x01, 0x0c, 0x08, "20000 40000 60000"		},
	{0x00, 0x01, 0x0c, 0x04, "10000"					},
	{0x00, 0x01, 0x0c, 0x00, "None"						},

	{0   , 0xfe, 0   ,    15, "Coinage"					},
	{0x00, 0x01, 0xf0, 0x90, "7 Coins 1 Credits"		},
	{0x00, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"		},
	{0x00, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"		},
	{0x00, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"		},
	{0x00, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"		},
	{0x00, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x00, 0x01, 0xf0, 0x60, "1 Coin  3 Credits"		},
	{0x00, 0x01, 0xf0, 0x50, "1 Coin  4 Credits"		},
	{0x00, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
	{0x00, 0x01, 0xf0, 0x30, "1 Coin  6 Credits"		},
	{0x00, 0x01, 0xf0, 0x20, "1 Coin  7 Credits"		},
	{0x00, 0x01, 0xf0, 0x10, "1 Coin  8 Credits"		},
	{0x00, 0x01, 0xf0, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x01, 0x01, 0x01, 0x01, "Off"						},
	{0x01, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x02, 0x00, "Upright"					},
	{0x01, 0x01, 0x02, 0x02, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Coin Mode"				},
	{0x01, 0x01, 0x04, 0x04, "Mode 1"					},
	{0x01, 0x01, 0x04, 0x00, "Mode 2"					},

	{0   , 0xfe, 0   ,    2, "Stop Mode (Cheat)"		},
	{0x01, 0x01, 0x10, 0x10, "Off"						},
	{0x01, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Sector Selection (Cheat)"	},
	{0x01, 0x01, 0x20, 0x20, "Off"						},
	{0x01, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x01, 0x01, 0x40, 0x40, "Off"						},
	{0x01, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x01, 0x01, 0x80, 0x80, "Off"						},
	{0x01, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Mpatrolw)

static void __fastcall m52_main_write(UINT16 address, UINT8 data)
{
	switch (address & ~0x7fc)
	{
		case 0xd000:
			IremSoundWrite(data);
		return;

		case 0xd001:
			if (is_game == 0) {
				flipscreen = (data & 1) ^ (~DrvDips[1] & 0x01);
			} else {
				flipscreen = data & 1;
			}
		return;
	}
}

static UINT8 __fastcall m52_main_read(UINT16 address)
{
	if ((address & 0xf800) == 0x8800) {
		INT32 count = 0;
		for (INT32 i = bg1xpos & 0x7f; i != 0; i >>= 1) {
			count += i & 1;
		}
		return count ^ (bg1xpos >> 7);
	}

	switch (address & ~0x07f8)
	{
		case 0xd000:
		case 0xd001:
		case 0xd002:
			return DrvInputs[address & 3];

		case 0xd003:
		case 0xd004:
			return DrvDips[address - 0xd003];
	}

	return 0;
}

static void __fastcall m52_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xe0)
	{
		case 0x00:
			scrollx = data;
		return;

		case 0x40:
			bg1xpos = data;
		return;

		case 0x60:
			bg1ypos = data;
		return;

		case 0x80:
			bg2xpos = data;
		return;

		case 0xa0:
			bg2ypos = data;
		return;

		case 0xc0:
			bgcontrol = data;
		return;
	}
}

static tilemap_callback( bg )
{
	INT32 color = DrvColRAM[offs];
	INT32 code  = DrvVidRAM[offs] | ((color & 0x80) << 1);

	TILE_SET_INFO(0, code, color, ((offs/0x20) < 7) ? TILE_OPAQUE : 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	IremSoundReset();

	bgcontrol = 0;
	bg1xpos = 0;
	bg1ypos = 0;
	bg2xpos = 0;
	bg2ypos = 0;
	flipscreen = 0;
	scrollx = 0;

	memset(nExtraCycles, 0, sizeof(nExtraCycles));

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x007000;

	DrvM6803ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x008000;
	DrvGfxROM2		= Next; Next += 0x010000;
	DrvGfxROM3		= Next; Next += 0x010000;
	DrvGfxROM4		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000340;
	DrvTransTab     = Next; Next += 0x000100; // for sprites

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2] = { 0, 0x1000*8 };
	INT32 XOffs0[16] = { STEP8(0,1), STEP8(16*8,1) };
	INT32 YOffs0[16] = { STEP16(0,8) };

	INT32 PlaneS[3] = { 0x2000*8, 0, 0x1000*8 };
	INT32 XOffsS[16] = { STEP8(0,1), STEP8(16*8,1) };
	INT32 YOffsS[16] = { STEP16(0,8) };

	INT32 Plane1[2] = { 4, 0 };
	INT32 XOffs1[256];
	INT32 YOffs1[128] = { STEP32(0x0000, 0x200), STEP32(0x4000, 0x200), STEP32(0x8000, 0x200), STEP32(0xc000, 0x200) };
	for (INT32 i = 0; i < 0x100; i++) XOffs1[i] = ((i & 0xfc) << 1) | (i & 3);

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2,   8,  8, Plane0, XOffs0, YOffs0, 0x0040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x3000);

	GfxDecode(0x0080, 3,  16, 16, PlaneS, XOffsS, YOffsS, 0x0100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x2000);

	GfxDecode(0x0001, 2, 256, 128, Plane1, XOffs1, YOffs1, 0x8000, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x2000);

	GfxDecode(0x0001, 2, 256, 128, Plane1, XOffs1, YOffs1, 0x8000, tmp, DrvGfxROM3);

	memcpy (tmp, DrvGfxROM4, 0x2000);

	GfxDecode(0x0001, 2, 256, 128, Plane1, XOffs1, YOffs1, 0x8000, tmp, DrvGfxROM4);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 game)
{
	is_game = game;

	BurnSetRefreshRate(56.737589);

	BurnAllocMemIndex();

	memset(DrvGfxROM2, 0xff, 0x4000);
	memset(DrvGfxROM3, 0xff, 0x4000);
	memset(DrvGfxROM4, 0xff, 0x4000);

	{
		INT32 k = 0;
		if (is_game == 0) { // mpatrol
			if (BurnLoadRom(DrvZ80ROM  + 0x0000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x1000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x2000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x3000,  k++, 1)) return 1;

			if (BurnLoadRom(DrvM6803ROM+ 0x7000,  k++, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x0000,  k++, 1)) return 1; // txt
			if (BurnLoadRom(DrvGfxROM0 + 0x1000,  k++, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x0000,  k++, 1)) return 1; // sprites
			if (BurnLoadRom(DrvGfxROM1 + 0x1000,  k++, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x0000,  k++, 1)) return 1; // bg 0

			if (BurnLoadRom(DrvGfxROM3 + 0x0000,  k++, 1)) return 1; // bg 1

			if (BurnLoadRom(DrvGfxROM4 + 0x0000,  k++, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x0000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0200,  k++, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0220,  k++, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0240,  k++, 1)) return 1;
		} else { // alpha1
			if (BurnLoadRom(DrvZ80ROM  + 0x0000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x1000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x2000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x3000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x4000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x5000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x6000,  k++, 1)) return 1;

			if (BurnLoadRom(DrvM6803ROM+ 0x7000,  k++, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x0000,  k++, 1)) return 1; // txt
			if (BurnLoadRom(DrvGfxROM0 + 0x1000,  k++, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x0000,  k++, 1)) return 1; // sprites
			if (BurnLoadRom(DrvGfxROM1 + 0x1000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x2000,  k++, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x0000,  k++, 1)) return 1; // bg 0
			if (BurnLoadRom(DrvGfxROM2 + 0x1000,  k++, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM3 + 0x0000,  k++, 1)) return 1; // bg 1
			if (BurnLoadRom(DrvGfxROM3 + 0x1000,  k++, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x0000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0200,  k++, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0220,  k++, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0240,  k++, 1)) return 1;
		}
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x6fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,			0x8000, 0x83ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,			0x8400, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xc800, 0xcbff, MAP_WRITE);
	ZetMapMemory(DrvSprRAM,			0xcc00, 0xcfff, MAP_WRITE);
	ZetMapMemory(DrvZ80RAM,			0xe000, 0xefff, MAP_RAM);
	ZetSetWriteHandler(m52_main_write);
	ZetSetReadHandler(m52_main_read);
	ZetSetOutHandler(m52_main_write_port);
	ZetClose();

	IremSoundInit(DrvM6803ROM, 0, 3072000);
	AY8910SetAllRoutes(0, 0.07, BURN_SND_ROUTE_BOTH);
    AY8910SetAllRoutes(1, 0.07, BURN_SND_ROUTE_BOTH);
    AY8910SetBuffered(ZetTotalCycles, 3072000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x08000, 0, 0x7f);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetScrollRows(0, 4);
	GenericTilemapSetOffsets(0, -8, (is_game == 0) ? -6 : 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	IremSoundExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 tmp[64];

	static const INT32 resistances_3[3] = { 1000, 470, 220 };
	static const INT32 resistances_2[2]  = { 470, 220 };
	double weights_r[3], weights_g[3], weights_b[3], scale;

	/* compute palette information for characters/backgrounds */
	scale = compute_resistor_weights(0, 255, -1.0,
			3, resistances_3, weights_r, 0, 0,
			3, resistances_3, weights_g, 0, 0,
			2, resistances_2, weights_b, 0, 0);

	/* character palette */
	for (INT32 i = 0; i < 512; i++)
	{
		UINT8 promval = DrvColPROM[0x000 + i];
		INT32 r = combine_3_weights(weights_r, BIT(promval,0), BIT(promval,1), BIT(promval,2));
		INT32 g = combine_3_weights(weights_g, BIT(promval,3), BIT(promval,4), BIT(promval,5));
		INT32 b = combine_2_weights(weights_b, BIT(promval,6), BIT(promval,7));

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}

	/* background palette */
	for (INT32 i = 0; i < 32; i++)
	{
		UINT8 promval = DrvColPROM[0x200 + i];
		INT32 r = combine_3_weights(weights_r, BIT(promval,0), BIT(promval,1), BIT(promval,2));
		INT32 g = combine_3_weights(weights_g, BIT(promval,3), BIT(promval,4), BIT(promval,5));
		INT32 b = combine_2_weights(weights_b, BIT(promval,6), BIT(promval,7));

		tmp[i] = BurnHighCol(r,g,b,0);
	}

	/* compute palette information for sprites */
	compute_resistor_weights(0, 255, scale,
			2, resistances_2, weights_r, 470, 0,
			3, resistances_3, weights_g, 470, 0,
			3, resistances_3, weights_b, 470, 0);

	/* sprite palette */
	for (INT32 i = 0; i < 32; i++)
	{
		UINT8 promval = DrvColPROM[0x220 + i];
		INT32 r = combine_2_weights(weights_r, BIT(promval,6), BIT(promval,7));
		INT32 g = combine_3_weights(weights_g, BIT(promval,3), BIT(promval,4), BIT(promval,5));
		INT32 b = combine_3_weights(weights_b, BIT(promval,0), BIT(promval,1), BIT(promval,2));

		tmp[32 + i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 256; i++)
	{
		UINT8 promval = DrvColPROM[0x240 + i];
		DrvPalette[0x200+i] = tmp[32 + promval];
		DrvTransTab[i] = promval;
	}

	DrvPalette[0x300+0*4+0] = tmp[0];
	DrvPalette[0x300+0*4+1] = tmp[4];
	DrvPalette[0x300+0*4+2] = tmp[8];
	DrvPalette[0x300+0*4+3] = tmp[12];
	DrvPalette[0x300+1*4+0] = tmp[0];
	DrvPalette[0x300+1*4+1] = tmp[1];
	DrvPalette[0x300+1*4+2] = tmp[2];
	DrvPalette[0x300+1*4+3] = tmp[3];

	DrvPalette[0x300+2*4+0] = tmp[0];
	DrvPalette[0x300+2*4+1] = tmp[16+1];
	DrvPalette[0x300+2*4+2] = tmp[16+2];
	DrvPalette[0x300+2*4+3] = tmp[16+3];
}

static void draw_background(int xpos, int ypos, UINT8 *gfx, int color_offset)
{
	const INT32 BGHEIGHT = 128;

	if (flipscreen)
	{
		xpos = 264 - xpos;
		ypos = 264 - ypos - BGHEIGHT;
		ypos -= (is_game == 1) ? 16 : -4;
	}

	xpos -= 12;
	ypos -= (is_game == 0) ? 8 : 0;

	GenericTilesSetClip(-1, -1, 2, 250);
	DrawCustomMaskTile(pTransDraw, 256, 128, 0, xpos, ypos, flipscreen, flipscreen, 0, 2, 0, color_offset, gfx);
	DrawCustomMaskTile(pTransDraw, 256, 128, 0, xpos - 256, ypos, flipscreen, flipscreen, 0, 2, 0, color_offset, gfx);
	GenericTilesClearClip();

#if 0
	// this below is probably not needed any longer w/ the change to 128px height tiles.
	INT32 min_y, max_y;

	if (flipscreen)
	{
		min_y = ypos - BGHEIGHT;
		max_y = ypos - 1;
	}
	else
	{
		min_y = ypos + BGHEIGHT;
		max_y = ypos + 2 * BGHEIGHT - 1;
	}

	if (is_game == 0) { // only for mpatrol!
		// fill in all the stuff below the image with a solid color
		for (INT32 y = min_y; y < max_y; y++)
		{
			if (y >= 0 && y < nScreenHeight) {
				UINT16 *dst = pTransDraw + y * nScreenWidth;

				for (INT32 x = 0; x < nScreenWidth; x++)
				{
					dst[x] = color_offset + 3;
				}
			}
		}
	}
#endif
}

static void draw_sprites(INT32 spr_offs)
{
	for (INT32 offs = spr_offs; offs >= (spr_offs & 0xc0); offs -= 4)
	{
		INT32 sy    = 257 - DrvSprRAM[offs];
		INT32 color = DrvSprRAM[offs + 1] & 0x3f;
		INT32 flipx = DrvSprRAM[offs + 1] & 0x40;
		INT32 flipy = DrvSprRAM[offs + 1] & 0x80;
		INT32 code  = DrvSprRAM[offs + 2];
		INT32 sx    = DrvSprRAM[offs + 3];

		if (flipscreen)
		{
			if (offs & 0x80)
				GenericTilesSetClip(-1, -1, 0, 128);
			else
				GenericTilesSetClip(-1, -1, 128, 255);

			flipx = !flipx;
			flipy = !flipy;

			sx = (238 + 2) - sx;
			sy = (282 - ((is_game == 1) ? 4 : 2)) - sy;
		}
		else
		{
			// center fire-plants in mpatrol
			sy += (is_game == 1) ? 6 : 0;
			if (~offs & 0x80)
				GenericTilesSetClip(-1, -1, 0, 128);
			else
				GenericTilesSetClip(-1, -1, 128, 255);
		}
		// When using *Transtab(): BPP needs to be added to color
		RenderTileTranstabOffset(pTransDraw, DrvGfxROM1, code, color << 3, 0, sx - 8, sy - 22, flipx, flipy, 16, 16, DrvTransTab, 0x200);
	}

	GenericTilesClearClip();
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(0, (flipscreen) ? TMAP_FLIPXY : 0);

	BurnTransferClear();

	if (~bgcontrol & 0x20)
	{
		if (~bgcontrol & 0x10 && (nBurnLayer & 1))
			draw_background(bg2xpos, bg2ypos, DrvGfxROM2, 0x300 + 0);

		if (~bgcontrol & 0x02) {
			if (nBurnLayer & 2)	draw_background(bg1xpos, bg1ypos, DrvGfxROM3, 0x300 + 4);
		} else if (~bgcontrol & 0x04) {
			if (nBurnLayer & 4)	draw_background(bg1xpos, bg1ypos, DrvGfxROM4, 0x300 + 8);
		}
	}

	if (is_game == 1) {
		GenericTilemapSetScrollRow(0, 1, -scrollx);
		GenericTilemapSetScrollRow(0, 2, -scrollx);
	}
	GenericTilemapSetScrollRow(0, 3, -scrollx);

	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) {
		for (INT32 offs = 0x3c; offs <= ((is_game == 0) ? 0xfc : 0x1fc); offs += 0x40) {
			draw_sprites(offs);
		}
	}

	BurnTransferFlip(flipscreen, flipscreen); // unflip coctail for netplay/etc
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6803NewFrame();
	ZetNewFrame();

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = MSM5205CalcInterleave(0, 3072000);
	INT32 nCyclesTotal[2] = { 3072000 / 60, 894886 / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles[0], nExtraCycles[1] };

	ZetOpen(0);
	M6803Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
        CPU_RUN(0, Zet);

        if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

        CPU_RUN(1, M6803);

		MSM5205Update();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	M6803Close();
	ZetClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

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
		IremSoundScan(nAction, pnMin);

		SCAN_VAR(bgcontrol);
		SCAN_VAR(bg1xpos);
		SCAN_VAR(bg1ypos);
		SCAN_VAR(bg2xpos);
		SCAN_VAR(bg2ypos);
		SCAN_VAR(flipscreen);
		SCAN_VAR(scrollx);

		SCAN_VAR(nExtraCycles);
	}

	return 0;
}


// Moon Patrol

static struct BurnRomInfo mpatrolRomDesc[] = {
	{ "mpa-1.3m",		0x1000, 0x5873a860, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "mpa-2.3l",		0x1000, 0xf4b85974, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpa-3.3k",		0x1000, 0x2e1a598c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mpa-4.3j",		0x1000, 0xdd05b587, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mp-s1.1a",		0x1000, 0x561d3108, 2 | BRF_GRA },           //  4 M6803 Code

	{ "mpe-5.3e",		0x1000, 0xe3ee7f75, 3 | BRF_GRA },           //  5 Characters
	{ "mpe-4.3f",		0x1000, 0xcca6d023, 3 | BRF_GRA },           //  6

	{ "mpb-2.3m",		0x1000, 0x707ace5e, 4 | BRF_GRA },           //  7 Sprites
	{ "mpb-1.3n",		0x1000, 0x9b72133a, 4 | BRF_GRA },           //  8

	{ "mpe-1.3l",		0x1000, 0xc46a7f72, 5 | BRF_GRA },           //  9 Mountain Layer

	{ "mpe-2.3k",		0x1000, 0xc7aa1fb0, 6 | BRF_GRA },           // 10 Hill Layer

	{ "mpe-3.3h",		0x1000, 0xa0919392, 7 | BRF_GRA },           // 11 Cityscape Layer

	{ "mpc-4.2a",		0x0200, 0x07f99284, 8 | BRF_GRA },           // 12 Color Data
	{ "mpc-3.1m",		0x0020, 0x6a57eff2, 8 | BRF_GRA },           // 13
	{ "mpc-1.1f",		0x0020, 0x26979b13, 8 | BRF_GRA },           // 14
	{ "mpc-2.2h",		0x0100, 0x7ae4cd97, 8 | BRF_GRA },           // 15

	{ "mp_7621-5.7h",	0x0200, 0xcf1fd9d0, 0 | BRF_OPT },			 // 16 unkprom
};

STD_ROM_PICK(mpatrol)
STD_ROM_FN(mpatrol)

static INT32 mpatrolInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvMpatrol = {
	"mpatrol", NULL, NULL, NULL, "1982",
	"Moon Patrol\0", NULL, "Irem", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M62, GBF_HORSHOOT, 0,
	NULL, mpatrolRomInfo, mpatrolRomName, NULL, NULL, NULL, NULL, MpatrolInputInfo, MpatrolDIPInfo,
	mpatrolInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x42c,
	240, 252, 4, 3
};


// Moon Patrol (Williams)

static struct BurnRomInfo mpatrolwRomDesc[] = {
	{ "mpa-1w.3m",		0x1000, 0xbaa1a1d4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "mpa-2w.3l",		0x1000, 0x52459e51, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mpa-3w.3k",		0x1000, 0x9b249fe5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mpa-4w.3j",		0x1000, 0xfee76972, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mp-s1.1a",		0x1000, 0x561d3108, 2 | BRF_GRA },           //  4 M6803 Code

	{ "mpe-5w.3e",		0x1000, 0xf56e01fe, 3 | BRF_GRA },           //  5 Characters
	{ "mpe-4w.3f",		0x1000, 0xcaaba2d9, 3 | BRF_GRA },           //  6

	{ "mpb-2.3m",		0x1000, 0x707ace5e, 4 | BRF_GRA },           //  7 Sprites
	{ "mpb-1.3n",		0x1000, 0x9b72133a, 4 | BRF_GRA },           //  8

	{ "mpe-1.3l",		0x1000, 0xc46a7f72, 5 | BRF_GRA },           //  9 Mountain Layer

	{ "mpe-2.3k",		0x1000, 0xc7aa1fb0, 6 | BRF_GRA },           // 10 Hill Layer

	{ "mpe-3.3h",		0x1000, 0xa0919392, 7 | BRF_GRA },           // 11 Cityscape Layer

	{ "mpc-4a.2a",		0x0200, 0xcb0a5ff3, 8 | BRF_GRA },           // 12 Color Data
	{ "mpc-3.1m",		0x0020, 0x6a57eff2, 8 | BRF_GRA },           // 13
	{ "mpc-1.1f",		0x0020, 0x26979b13, 8 | BRF_GRA },           // 14
	{ "mpc-2.2h",		0x0100, 0x7ae4cd97, 8 | BRF_GRA },           // 15

	{ "mp_7621-5.7h",	0x0200, 0xcf1fd9d0, 0 | BRF_OPT },			 // 16 unkprom
};

STD_ROM_PICK(mpatrolw)
STD_ROM_FN(mpatrolw)

struct BurnDriver BurnDrvMpatrolw = {
	"mpatrolw", "mpatrol", NULL, NULL, "1982",
	"Moon Patrol (Williams)\0", NULL, "Irem (Williams license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M62, GBF_HORSHOOT, 0,
	NULL, mpatrolwRomInfo, mpatrolwRomName, NULL, NULL, NULL, NULL, MpatrolInputInfo, MpatrolwDIPInfo,
	mpatrolInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x42c,
	240, 252, 4, 3
};


// Moon Ranger (bootleg of Moon Patrol)

static struct BurnRomInfo mrangerRomDesc[] = {
	{ "mpa-1.3m",	0x1000, 0x5873a860, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "mra-2.3l",	0x1000, 0x217dd431, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mra-3.3k",	0x1000, 0x9f0af7b2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mra-4.3j",	0x1000, 0x7fe8e2cd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mp-s1.1a",	0x1000, 0x561d3108, 2 | BRF_GRA },           //  4 M6803 Code

	{ "mpe-5.3e",	0x1000, 0xe3ee7f75, 3 | BRF_GRA },           //  5 Characters
	{ "mpe-4.3f",	0x1000, 0xcca6d023, 3 | BRF_GRA },           //  6

	{ "mpb-2.3m",	0x1000, 0x707ace5e, 4 | BRF_GRA },           //  7 Sprites
	{ "mpb-1.3n",	0x1000, 0x9b72133a, 4 | BRF_GRA },           //  8

	{ "mpe-1.3l",	0x1000, 0xc46a7f72, 5 | BRF_GRA },           //  9 Mountain Layer

	{ "mpe-2.3k",	0x1000, 0xc7aa1fb0, 6 | BRF_GRA },           // 10 Hill Layer

	{ "mpe-3.3h",	0x1000, 0xa0919392, 7 | BRF_GRA },           // 11 Cityscape Layer

	{ "mpc-4.2a",	0x0200, 0x07f99284, 8 | BRF_GRA },           // 12 Color Data
	{ "mpc-3.1m",	0x0020, 0x6a57eff2, 8 | BRF_GRA },           // 13
	{ "mpc-1.1f",	0x0020, 0x26979b13, 8 | BRF_GRA },           // 14
	{ "mpc-2.2h",	0x0100, 0x7ae4cd97, 8 | BRF_GRA },           // 15
};

STD_ROM_PICK(mranger)
STD_ROM_FN(mranger)

struct BurnDriver BurnDrvMranger = {
	"mranger", "mpatrol", NULL, NULL, "1982",
	"Moon Ranger (bootleg of Moon Patrol)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_M62, GBF_HORSHOOT, 0,
	NULL, mrangerRomInfo, mrangerRomName, NULL, NULL, NULL, NULL, MpatrolInputInfo, MpatrolDIPInfo,
	mpatrolInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x42c,
	240, 252, 4, 3
};

// Alpha One (Vision Electronics)

static struct BurnRomInfo alpha1vRomDesc[] = {
	{ "2-m3",		0x1000, 0x3a679d34, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "3-l3",		0x1000, 0x2f09df64, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4-k3",		0x1000, 0x64fb9c8a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "5-j3",		0x1000, 0xd1643d18, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "6-h3",		0x1000, 0xcf34ab51, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "7-f3",		0x1000, 0x99db9781, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7a e3",		0x1000, 0x3b0b4b0d, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "1-a1",		0x1000, 0x9e07fdd5, 2 | BRF_GRA },           //  7 M6803 Code

	{ "14-e3",		0x1000, 0xcf00c737, 3 | BRF_GRA },           //  8 Characters
	{ "13-f3",		0x1000, 0x4b799229, 3 | BRF_GRA },           //  9

	{ "15-n3",		0x1000, 0xdc26df76, 4 | BRF_GRA },           // 10 Sprites
	{ "16-l3",		0x1000, 0x39b9863b, 4 | BRF_GRA },           // 11
	{ "17-k3",		0x1000, 0xcfd90773, 4 | BRF_GRA },           // 12

	{ "11-k3",		0x1000, 0x7659440a, 5 | BRF_GRA },           // 13 gfx3
	{ "12-jh3",		0x1000, 0x7659440a, 5 | BRF_GRA },           // 14

	{ "9-n3",		0x1000, 0x0fdb7d13, 6 | BRF_GRA },           // 15 gfx4

	{ "10-lm3",		0x1000, 0x9dde3a75, 7 | BRF_GRA },           // 16 gfx5

	{ "63s481-a2",	0x0200, 0x58678ea8, 8 | BRF_GRA },           // 17 Color Data
	{ "18s030-m1",	0x0020, 0x6a57eff2, 8 | BRF_GRA },           // 18
	{ "mb7051-f1",	0x0020, 0xd8bdd0df, 8 | BRF_GRA },           // 19
	{ "mb7052-h2",	0x0100, 0xce9f0ef9, 8 | BRF_GRA },           // 20
};

STD_ROM_PICK(alpha1v)
STD_ROM_FN(alpha1v)

static INT32 alpha1vInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvAlpha1v = {
	"alpha1v", NULL, NULL, NULL, "1988",
	"Alpha One (Vision Electronics)\0", NULL, "Vision Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_IREM_M62, GBF_HORSHOOT, 0,
	NULL, alpha1vRomInfo, alpha1vRomName, NULL, NULL, NULL, NULL, Alpha1vInputInfo, Alpha1vDIPInfo,
	alpha1vInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x42c,
	240, 256, 4, 3
};
