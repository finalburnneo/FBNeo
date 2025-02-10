// FB Neo Ghosts'n Goblins driver module
// Based on MAME driver by Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvFgVideoRAM;
static UINT8 *DrvBgVideoRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprRAMBuf;
static UINT8 *DrvPalRAM0;
static UINT8 *DrvPalRAM1;
static UINT8 *DrvChars;
static UINT8 *DrvTiles;
static UINT8 *DrvSprites;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[3];
static UINT8 DrvReset;

static UINT8 rom_bank;
static UINT8 scrollx[2];
static UINT8 scrolly[2];
static UINT8 soundlatch;

static INT32 nExtraCycles;

static INT32 is_game = 0; // 0 gng+etc, 1 diamrun

static struct BurnInputInfo GngInputList[] =
{
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Fire 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Fire 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Fire 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Fire 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip 1",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip 2",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip 3",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Gng)

static struct BurnInputInfo DiamrunInputList[] =
{
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},

	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Fire 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip 1",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip 2",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip 3",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Diamrun)


static struct BurnDIPInfo GngDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0xdf, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					}, // fake

	{0   , 0xfe, 0   , 16  , "Coinage"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Play"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Play"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Play"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Plays"		},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Plays"		},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Play"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Plays"		},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Plays"		},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Plays"		},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Plays"		},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Plays"		},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Plays"		},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Plays"		},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Plays"		},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Plays"		},
	{0x00, 0x01, 0x0f, 0x00, "Freeplay"				},

	{0   , 0xfe, 0   , 2   , "Coinage affects"		},
	{0x00, 0x01, 0x10, 0x10, "Coin A"				},
	{0x00, 0x01, 0x10, 0x00, "Coin B"				},

	{0   , 0xfe, 0   , 2   , "Demo Sounds"			},
	{0x00, 0x01, 0x20, 0x20, "Off"					},
	{0x00, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   , 2   , "Service Mode"			},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   , 2   , "Flip Screen"			},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   , 4   , "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "3"					},
	{0x01, 0x01, 0x03, 0x02, "4"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   , 2   , "Cabinet"				},
	{0x01, 0x01, 0x04, 0x00, "Upright"				},
	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   , 4   , "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x18, "20k 70k 70k"			},
	{0x01, 0x01, 0x18, 0x10, "30k 80k 80k"			},
	{0x01, 0x01, 0x18, 0x08, "20k 80k"				},
	{0x01, 0x01, 0x18, 0x00, "30k 80k"				},

	{0   , 0xfe, 0   , 4   , "Difficulty"			},
	{0x01, 0x01, 0x60, 0x40, "Easy"					},
	{0x01, 0x01, 0x60, 0x60, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Difficult"			},
	{0x01, 0x01, 0x60, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   , 3   , "Joy Type"				},
	{0x02, 0x01, 0x03, 0x00, "8-Way"				},
	{0x02, 0x01, 0x03, 0x01, "4-Way"				},
	{0x02, 0x01, 0x03, 0x02, "4-Way Alt."			},
};

STDDIPINFO(Gng)

static struct BurnDIPInfo MakaimurDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0xdf, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					}, // fake

	{0   , 0xfe, 0   , 16  , "Coinage"				},
	{0x00, 0x01, 0x0f, 0x02, "4 Coins 1 Play"		},
	{0x00, 0x01, 0x0f, 0x05, "3 Coins 1 Play"		},
	{0x00, 0x01, 0x0f, 0x08, "2 Coins 1 Play"		},
	{0x00, 0x01, 0x0f, 0x04, "3 Coins 2 Plays"		},
	{0x00, 0x01, 0x0f, 0x01, "4 Coins 3 Plays"		},
	{0x00, 0x01, 0x0f, 0x0f, "1 Coin  1 Play"		},
	{0x00, 0x01, 0x0f, 0x03, "3 Coins 4 Plays"		},
	{0x00, 0x01, 0x0f, 0x07, "2 Coins 3 Plays"		},
	{0x00, 0x01, 0x0f, 0x0e, "1 Coin  2 Plays"		},
	{0x00, 0x01, 0x0f, 0x06, "2 Coins 5 Plays"		},
	{0x00, 0x01, 0x0f, 0x0d, "1 Coin  3 Plays"		},
	{0x00, 0x01, 0x0f, 0x0c, "1 Coin  4 Plays"		},
	{0x00, 0x01, 0x0f, 0x0b, "1 Coin  5 Plays"		},
	{0x00, 0x01, 0x0f, 0x0a, "1 Coin  6 Plays"		},
	{0x00, 0x01, 0x0f, 0x09, "1 Coin  7 Plays"		},
	{0x00, 0x01, 0x0f, 0x00, "Freeplay"				},

	{0   , 0xfe, 0   , 2   , "Coinage affects"		},
	{0x00, 0x01, 0x10, 0x10, "Coin A"				},
	{0x00, 0x01, 0x10, 0x00, "Coin B"				},

	{0   , 0xfe, 0   , 2   , "Demo Sounds"			},
	{0x00, 0x01, 0x20, 0x20, "Off"					},
	{0x00, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   , 2   , "Service Mode"			},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   , 2   , "Flip Screen"			},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   , 4   , "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "3"					},
	{0x01, 0x01, 0x03, 0x02, "4"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   , 2   , "Cabinet"				},
	{0x01, 0x01, 0x04, 0x00, "Upright"				},
	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   , 4   , "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x18, "20k 70k 70k"			},
	{0x01, 0x01, 0x18, 0x10, "30k 80k 80k"			},
	{0x01, 0x01, 0x18, 0x08, "20k 80k"				},
	{0x01, 0x01, 0x18, 0x00, "30k 80k"				},

	{0   , 0xfe, 0   , 4   , "Difficulty"			},
	{0x01, 0x01, 0x60, 0x40, "Easy"					},
	{0x01, 0x01, 0x60, 0x60, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Difficult"			},
	{0x01, 0x01, 0x60, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   , 2   , "Invulnerability"		},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   , 3   , "Joy Type"				},
	{0x02, 0x01, 0x03, 0x00, "8-Way"				},
	{0x02, 0x01, 0x03, 0x01, "4-Way"				},
	{0x02, 0x01, 0x03, 0x02, "4-Way Alt."			},
};

STDDIPINFO(Makaimur)

static struct BurnDIPInfo DiamrunDIPList[]=
{
	DIP_OFFSET(0x0a)
	{0x00, 0xff, 0xff, 0x81, NULL					},
	{0x01, 0xff, 0xff, 0x07, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					}, // fake

	{0   , 0xfe, 0   , 4   , "Lives"				},
	{0x00, 0x01, 0x03, 0x00, "2"					},
	{0x00, 0x01, 0x03, 0x01, "3"					},
	{0x00, 0x01, 0x03, 0x02, "4"					},
	{0x00, 0x01, 0x03, 0x03, "5"					},

	{0   , 0xfe, 0   , 4   , "Credits A"			},
	{0x00, 0x01, 0x0c, 0x00, "x1"					},
	{0x00, 0x01, 0x0c, 0x04, "x2"					},
	{0x00, 0x01, 0x0c, 0x08, "x3"					},
	{0x00, 0x01, 0x0c, 0x0c, "x4"					},

	{0   , 0xfe, 0   , 4   , "Coinage"				},
	{0x00, 0x01, 0x30, 0x30, "4 Coins 1 Play"		},
	{0x00, 0x01, 0x30, 0x20, "3 Coins 1 Play"		},
	{0x00, 0x01, 0x30, 0x10, "2 Coins 1 Play"		},
	{0x00, 0x01, 0x30, 0x00, "1 Coin  1 Play"		},

	{0   , 0xfe, 0   , 2   , "Flip Screen"			},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   , 16  , "Energy Loss"			},
	{0x01, 0x01, 0x0f, 0x00, "Slowest"				},
	{0x01, 0x01, 0x0f, 0x01, "-6 Slower"			},
	{0x01, 0x01, 0x0f, 0x02, "-5 Slower"			},
	{0x01, 0x01, 0x0f, 0x03, "-4 Slower"			},
	{0x01, 0x01, 0x0f, 0x04, "-3 Slower"			},
	{0x01, 0x01, 0x0f, 0x05, "-2 Slower"			},
	{0x01, 0x01, 0x0f, 0x06, "-1 Slower"			},
	{0x01, 0x01, 0x0f, 0x07, "Normal"				},
	{0x01, 0x01, 0x0f, 0x08, "+1 Faster"			},
	{0x01, 0x01, 0x0f, 0x09, "+2 Faster"			},
	{0x01, 0x01, 0x0f, 0x0a, "+3 Faster"			},
	{0x01, 0x01, 0x0f, 0x0b, "+4 Faster"			},
	{0x01, 0x01, 0x0f, 0x0c, "+5 Faster"			},
	{0x01, 0x01, 0x0f, 0x0d, "+6 Faster"			},
	{0x01, 0x01, 0x0f, 0x0e, "+7 Faster"			},
	{0x01, 0x01, 0x0f, 0x0f, "Fastest"				},

	{0   , 0xfe, 0   , 4   , "Credits B"			},
	{0x01, 0x01, 0x30, 0x00, "x1"					},
	{0x01, 0x01, 0x30, 0x10, "x2"					},
	{0x01, 0x01, 0x30, 0x20, "x3"					},
	{0x01, 0x01, 0x30, 0x30, "x4"					},
};

STDDIPINFO(Diamrun)

static void calc_color(INT32 index); // forward

static void bank_switch(UINT8 bank)
{
	if (bank == 4) {
		rom_bank = 4;
		M6809MapMemory(DrvM6809ROM,									0x4000, 0x5fff, MAP_ROM);
	} else {
		rom_bank = bank & 3;
		M6809MapMemory(DrvM6809ROM + 0xc000 + (rom_bank * 0x2000),	0x4000, 0x5fff, MAP_ROM);
	}
}

static UINT8 main_read(UINT16 address)
{
	switch (address) {
		case 0x3000:
		case 0x3001:
		case 0x3002:
			return DrvInputs[address & 3];

		case 0x3003:
		case 0x3004:
			return DrvDips[address - 0x3003];

		case 0x3c00:
		case 0x3d01: {
			// nop
			return 0;
		}
	}

	if (address >= 0x3005 && address <= 0x33ff)
		return 0; // nop (diamrun)

	bprintf(0, _T("mr %X\n"), address);

	return 0;
}

static void main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff00) == 0x3800) { // 0x38xx (second write)
		DrvPalRAM1[address & 0xff] = data;
		calc_color(address & 0xff);
		return;
	}
	if ((address & 0xff00) == 0x3900) { // 0x39xx (first write)
		DrvPalRAM0[address & 0xff] = data;
		return;
	}

	switch (address) {
		case 0x3a00:
			soundlatch = data;
			return;

		case 0x3b08:
		case 0x3b09:
			scrollx[address & 1] = data;
			return;

		case 0x3b0a:
		case 0x3b0b:
			scrolly[address & 1] = data;
			return;

		case 0x3c00: // nop
			return;

		case 0x3d00: // flipscreen
			return;

		case 0x3d01:
			if (data & 1 && is_game == 0) {
				BurnYM2203Reset();
				ZetReset();
			}
			return;

		case 0x3d02: // coin counter 1,2
		case 0x3d03:
			return;

		case 0x3e00:
			bank_switch(data);
			return;
	}
}

static UINT8 __fastcall sound_read(UINT16 address)
{
	switch (address) {
		case 0xc800:
			return soundlatch;
	}

	return 0;
}

static void __fastcall sound_write(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0xe000:
		case 0xe001:
		case 0xe002:
		case 0xe003:
			BurnYM2203Write((address >> 1) & 1, address & 1, data);
			return;
	}
}

static void DrvRandPalette()
{ // On first boot we fill the palette with some arbitrary values to see the boot-up messages
	DrvPalRAM0[0] = 0x00;
	DrvPalRAM1[0] = 0x00;
	for (INT32 i = 1; i < 0x100; i++) {
		DrvPalRAM0[i] = 0xaf;
		DrvPalRAM1[i] = 0x5a;
	}
}

static tilemap_callback( bg )
{
	INT32 Attr = DrvBgVideoRAM[offs + 0x400];
	INT32 Code = DrvBgVideoRAM[offs] + ((Attr & 0xc0) << 2);

	TILE_SET_INFO(0, Code, Attr/*&7*/, TILE_FLIPYX(Attr >> 4));
	sTile->category = (Attr >> 3) & 1;
}

static tilemap_callback( fg )
{
	INT32 Attr = DrvFgVideoRAM[offs + 0x400];
	INT32 Code = DrvFgVideoRAM[offs] + ((Attr & 0xc0) << 2);

	TILE_SET_INFO(1, Code, Attr/*&f*/, TILE_FLIPYX(Attr >> 4));
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM			= Next; Next += 0x14000;
	DrvZ80ROM			= Next; Next += 0x08000;

	AllRam				= Next;

	DrvM6809RAM			= Next; Next += 0x01e00;
	DrvZ80RAM			= Next; Next += 0x00800;
	DrvSprRAM			= Next; Next += 0x00200;
	DrvSprRAMBuf		= Next; Next += 0x00200;
	DrvFgVideoRAM		= Next; Next += 0x00800;
	DrvBgVideoRAM		= Next; Next += 0x00800;
	DrvPalRAM0			= Next; Next += 0x00100;
	DrvPalRAM1			= Next; Next += 0x00100;

	RamEnd				= Next;

	DrvChars			= Next; Next += 0x400 *  8 *  8;
	DrvTiles			= Next; Next += 0x400 * 16 * 16;
	DrvSprites			= Next; Next += 0x400 * 16 * 16;
	DrvPalette			= (UINT32*)Next; Next += 0x00100 * sizeof(UINT32);

	MemEnd				= Next;

	return 0;
}

static INT32 DrvDoReset()
{
	M6809Open(0);
	M6809Reset();
	M6809Close();

	ZetOpen(0);
	BurnYM2203Reset();
	ZetReset();
	ZetClose();

	HiscoreReset();

	rom_bank = 0;
	scrollx[0] = scrollx[1] = 0;
	scrolly[0] = scrolly[1] = 0;
	soundlatch = 0;
	nExtraCycles = 0;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 CharPlaneOffsets[2]   = { 4, 0 };
	INT32 CharXOffsets[8]       = { STEP4(0,1), STEP4(8,1) };
	INT32 CharYOffsets[8]       = { STEP8(0, 16) };
	INT32 TilePlaneOffsets[3]   = { 0x80000, 0x40000, 0 };
	INT32 TileXOffsets[16]      = { STEP8(0,1), STEP8(128,1) };
	INT32 TileYOffsets[16]      = { STEP16(0,8) };
	INT32 SpritePlaneOffsets[4] = { 0x80004, 0x80000, 4, 0 };
	INT32 SpriteXOffsets[16]    = { STEP4(0,1), STEP4(8,1), STEP4(256,1), STEP4(256+8,1) };
	INT32 SpriteYOffsets[16]    = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy(tmp, DrvChars, 0x4000);
	GfxDecode(0x400, 2,  8,  8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x80, tmp, DrvChars);

	memcpy(tmp, DrvTiles, 0x20000);
	GfxDecode(0x400, 3, 16, 16, TilePlaneOffsets, TileXOffsets, TileYOffsets, 0x100, tmp, DrvTiles);

	memcpy(tmp, DrvSprites, 0x20000);
	GfxDecode(0x400, 4, 16, 16, SpritePlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x200, tmp, DrvSprites);

	BurnFree(tmp);

	return 0;
}


static INT32 DrvCommonInit(INT32 game) // 0 = gng, 1 = gnga, 2 = diamrun
{
	BurnAllocMemIndex();

	BurnSetRefreshRate(59.64);

	{
		if (game == 0 || game == 1) {
			INT32 k = 0;
			if (game == 1) {
				if (BurnLoadRom(DrvM6809ROM + 0x00000, k++, 1)) return 1;
				if (BurnLoadRom(DrvM6809ROM + 0x04000, k++, 1)) return 1;
				if (BurnLoadRom(DrvM6809ROM + 0x08000, k++, 1)) return 1;
				if (BurnLoadRom(DrvM6809ROM + 0x0c000, k++, 1)) return 1;
				if (BurnLoadRom(DrvM6809ROM + 0x10000, k++, 1)) return 1;
			} else {
				if (BurnLoadRom(DrvM6809ROM + 0x00000, k++, 1)) return 1;
				if (BurnLoadRom(DrvM6809ROM + 0x04000, k++, 1)) return 1;
				if (BurnLoadRom(DrvM6809ROM + 0x0c000, k++, 1)) return 1;
			}

			if (BurnLoadRom(DrvZ80ROM + 0x00000, k++, 1)) return 1;

			if (BurnLoadRom(DrvChars, k++, 1)) return 1;

			if (BurnLoadRom(DrvTiles + 0x00000, k++, 1)) return 1;
			if (BurnLoadRom(DrvTiles + 0x04000, k++, 1)) return 1;
			if (BurnLoadRom(DrvTiles + 0x08000, k++, 1)) return 1;
			if (BurnLoadRom(DrvTiles + 0x0c000, k++, 1)) return 1;
			if (BurnLoadRom(DrvTiles + 0x10000, k++, 1)) return 1;
			if (BurnLoadRom(DrvTiles + 0x14000, k++, 1)) return 1;

			memset(DrvSprites, 0xff, 0x20000);
			if (BurnLoadRom(DrvSprites + 0x00000, k++, 1)) return 1;
			if (BurnLoadRom(DrvSprites + 0x04000, k++, 1)) return 1;
			if (BurnLoadRom(DrvSprites + 0x08000, k++, 1)) return 1;
			if (BurnLoadRom(DrvSprites + 0x10000, k++, 1)) return 1;
			if (BurnLoadRom(DrvSprites + 0x14000, k++, 1)) return 1;
			if (BurnLoadRom(DrvSprites + 0x18000, k++, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvM6809ROM + 0x00000, 0, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x04000, 1, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x0c000, 2, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x14000, 3, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM + 0x00000, 4, 1)) return 1;

			if (BurnLoadRom(DrvChars, 5, 1)) return 1;

			if (BurnLoadRom(DrvTiles + 0x00000,  6, 1)) return 1;
			if (BurnLoadRom(DrvTiles + 0x04000,  7, 1)) return 1;
			if (BurnLoadRom(DrvTiles + 0x08000,  8, 1)) return 1;
			if (BurnLoadRom(DrvTiles + 0x0c000,  9, 1)) return 1;
			if (BurnLoadRom(DrvTiles + 0x10000, 10, 1)) return 1;
			if (BurnLoadRom(DrvTiles + 0x14000, 11, 1)) return 1;

			memset(DrvSprites, 0xff, 0x20000);
			if (BurnLoadRom(DrvSprites + 0x00000, 12, 1)) return 1;
			if (BurnLoadRom(DrvSprites + 0x10000, 13, 1)) return 1;

			DrvM6809ROM[0x2000] = 0x00; // crash patch (diamrun)
		}

		DrvGfxDecode();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,				0x0000, 0x1dff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,				0x1e00, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvFgVideoRAM,			0x2000, 0x27ff, MAP_RAM);
	M6809MapMemory(DrvBgVideoRAM,			0x2800, 0x2fff, MAP_RAM);
//	M6809MapMemory(DrvPalRAM1,				0x3800, 0x38ff, MAP_RAM); // handler
//	M6809MapMemory(DrvPalRAM0,				0x3900, 0x39ff, MAP_RAM); // handler
	M6809MapMemory(DrvM6809ROM,				0x4000, 0x5fff, MAP_ROM);
	M6809MapMemory(DrvM6809ROM + 0x2000,	0x6000, 0xffff, MAP_ROM);
	M6809SetReadHandler(main_read);
	M6809SetWriteHandler(main_write);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,					0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,					0xc000, 0xc7ff, MAP_RAM);
	ZetSetReadHandler(sound_read);
	ZetSetWriteHandler(sound_write);
	ZetClose();

	BurnYM2203Init(2, 1500000, NULL, 0);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.18, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.38, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.38, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.38, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE, 0.18, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.38, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.38, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.38, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvTiles, 3, 16, 16, 0x400 * 16*16, 0x00, 0x07);
	GenericTilemapSetGfx(1, DrvChars, 2,  8,  8, 0x400 *  8* 8, 0x80, 0x0f);
	GenericTilemapSetTransSplit(0, 0, 0xff, 0x00);
	GenericTilemapSetTransSplit(0, 1, 0x41, 0xbe);
	GenericTilemapSetTransparent(1, 3);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	if (game != 2) DrvRandPalette(); // only gng

	DrvDoReset();

	return 0;
}

static INT32 GngInit()
{
	return DrvCommonInit(0);
}

static INT32 GngaInit()
{
	return DrvCommonInit(1);
}

static INT32 DiamrunInit()
{
	is_game = 1;
	return DrvCommonInit(2);
}

static INT32 DrvExit()
{
	M6809Exit();
	ZetExit();

	BurnYM2203Exit();

	GenericTilesExit();

	BurnFreeMemIndex();

	scrollx[0] = scrollx[1] = 0;
	scrolly[0] = scrolly[1] = 0;
	rom_bank = 0;
	soundlatch = 0;

	is_game = 0;

	return 0;
}

static void calc_color(INT32 index)
{
	INT32 Val = DrvPalRAM0[index] + (DrvPalRAM1[index] << 8);

	INT32 r = pal4bit(Val >> 12);
	INT32 g = pal4bit(Val >>  8);
	INT32 b = pal4bit(Val >>  4);

	DrvPalette[index] = BurnHighCol(r, g, b, 0);
}

static void DrvRecalcPalette()
{
	for (INT32 i = 0; i < 0x100; i++) {
		calc_color(i);
	}
}

static void DrvRenderSprites()
{
	for (INT32 offs = 0x200 - 4; offs >= 0; offs -= 4)
	{
		UINT8 attr	= DrvSprRAMBuf[offs + 1];
		INT32 sx	= DrvSprRAMBuf[offs + 3] - 0x100 * (attr & 0x01);
		INT32 sy	= DrvSprRAMBuf[offs + 2];
		INT32 flipx	= attr & 0x04;
		INT32 flipy	= attr & 0x08;
		INT32 code	= DrvSprRAMBuf[offs + 0] + ((attr << 2) & 0x300);
		INT32 color	= (attr >> 4) & 3;

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 16, flipx, flipy, color, 4, 0xf, 0x40, DrvSprites);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollX(0, scrollx[0] | (scrollx[1] << 8));
	GenericTilemapSetScrollY(0, scrolly[0] | (scrolly[1] << 8));

	BurnTransferClear();

	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1);

	if (nSpriteEnable & 1) DrvRenderSprites();

	if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0);

	if (nBurnLayer & 8) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) DrvDoReset();

	ZetNewFrame();

	{
		UINT8 *DrvJoy[3] = { DrvJoy1, DrvJoy2, DrvJoy3 };
		UINT32 DrvJoyInit[3] = { 0xff, 0xff, 0xff };

		UINT8 joy_type = 0;
		switch (DrvDips[2]) {
			case 1: joy_type = INPUT_4WAY; break;
			case 2: joy_type = INPUT_4WAY_ALT; break;
		}

		CompileInput(DrvJoy, (void*)DrvInputs, 3, 8, DrvJoyInit);
		ProcessJoystick(&DrvInputs[1], 0, 3,2,1,0, INPUT_CLEAROPPOSITES | INPUT_ISACTIVELOW | joy_type);
		ProcessJoystick(&DrvInputs[2], 1, 3,2,1,0, INPUT_CLEAROPPOSITES | INPUT_ISACTIVELOW | joy_type);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { (INT32)(1500000 / 59.637405), (INT32)(3000000 / 59.637405) };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	M6809Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, M6809);
		if (i == nInterleave-1) {
			if (pBurnDraw) DrvDraw();

			memcpy(DrvSprRAMBuf, DrvSprRAM, 0x200);
			M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));
		if (i % 64 == 63) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}

	M6809Close();

	BurnTimerEndFrame(nCyclesTotal[1]);

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029696;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(AllRam, RamEnd-AllRam, "All Ram");
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);
		ZetScan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(nExtraCycles);
		SCAN_VAR(rom_bank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		bank_switch(rom_bank);
		M6809Close();
	}

	return 0;
}


static struct BurnRomInfo GngRomDesc[] = {
	{ "gg4.bin",       0x04000, 0x66606beb, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "gg3.bin",       0x08000, 0x9e01c65e, BRF_ESS | BRF_PRG }, //	 1
	{ "gg5.bin",       0x08000, 0xd6397b2b, BRF_ESS | BRF_PRG }, //	 2

	{ "gg2.bin",       0x08000, 0x615f5b6f, BRF_ESS | BRF_PRG }, //  3	Z80 Program

	{ "gg1.bin",       0x04000, 0xecfccf07, BRF_GRA },	     //  4	Characters

	{ "gg11.bin",      0x04000, 0xddd56fa9, BRF_GRA },	     //  5	Tiles
	{ "gg10.bin",      0x04000, 0x7302529d, BRF_GRA },	     //  6
	{ "gg9.bin",       0x04000, 0x20035bda, BRF_GRA },	     //  7
	{ "gg8.bin",       0x04000, 0xf12ba271, BRF_GRA },	     //  8
	{ "gg7.bin",       0x04000, 0xe525207d, BRF_GRA },	     //  9
	{ "gg6.bin",       0x04000, 0x2d77e9b2, BRF_GRA },	     //  10

	{ "gg17.bin",      0x04000, 0x93e50a8f, BRF_GRA },	     //  11	Sprites
	{ "gg16.bin",      0x04000, 0x06d7e5ca, BRF_GRA },	     //  12
	{ "gg15.bin",      0x04000, 0xbc1fe02d, BRF_GRA },	     //  13
	{ "gg14.bin",      0x04000, 0x6aaf12f9, BRF_GRA },	     //  14
	{ "gg13.bin",      0x04000, 0xe80c3fca, BRF_GRA },	     //  15
	{ "gg12.bin",      0x04000, 0x7780a925, BRF_GRA },	     //  16

	{ "tbp24s10.14k",  0x00100, 0x0eaf5158, BRF_GRA | BRF_OPT },	     //  17	PROMs
	{ "63s141.2e",     0x00100, 0x4a1285a4, BRF_GRA | BRF_OPT },	     //  18

	{ "gg-pal10l8.bin",0x0002c, 0x87f1b7e0, BRF_GRA | BRF_OPT },	     //  19	PLDs
};

STD_ROM_PICK(Gng)
STD_ROM_FN(Gng)

struct BurnDriver BurnDrvGng = {
	"gng", NULL, NULL, NULL, "1985",
	"Ghosts'n Goblins (World? set 1)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, GngRomInfo, GngRomName, NULL, NULL, NULL, NULL, GngInputInfo, GngDIPInfo,
	GngInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo GngaRomDesc[] = {
	{ "gng.n10",       0x04000, 0x60343188, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "gng.n9",        0x04000, 0xb6b91cfb, BRF_ESS | BRF_PRG }, //	 1
	{ "gng.n8",        0x04000, 0xa5cfa928, BRF_ESS | BRF_PRG }, //	 2
	{ "gng.n13",       0x04000, 0xfd9a8dda, BRF_ESS | BRF_PRG }, //	 3
	{ "gng.n12",       0x04000, 0x13cf6238, BRF_ESS | BRF_PRG }, //	 4

	{ "gg2.bin",       0x08000, 0x615f5b6f, BRF_ESS | BRF_PRG }, //  5	Z80 Program

	{ "gg1.bin",       0x04000, 0xecfccf07, BRF_GRA },	     //  6	Characters

	{ "gg11.bin",      0x04000, 0xddd56fa9, BRF_GRA },	     //  7	Tiles
	{ "gg10.bin",      0x04000, 0x7302529d, BRF_GRA },	     //  8
	{ "gg9.bin",       0x04000, 0x20035bda, BRF_GRA },	     //  9
	{ "gg8.bin",       0x04000, 0xf12ba271, BRF_GRA },	     //  10
	{ "gg7.bin",       0x04000, 0xe525207d, BRF_GRA },	     //  11
	{ "gg6.bin",       0x04000, 0x2d77e9b2, BRF_GRA },	     //  12

	{ "gg17.bin",      0x04000, 0x93e50a8f, BRF_GRA },	     //  13	Sprites
	{ "gg16.bin",      0x04000, 0x06d7e5ca, BRF_GRA },	     //  14
	{ "gg15.bin",      0x04000, 0xbc1fe02d, BRF_GRA },	     //  15
	{ "gg14.bin",      0x04000, 0x6aaf12f9, BRF_GRA },	     //  16
	{ "gg13.bin",      0x04000, 0xe80c3fca, BRF_GRA },	     //  17
	{ "gg12.bin",      0x04000, 0x7780a925, BRF_GRA },	     //  18

	{ "tbp24s10.14k",  0x00100, 0x0eaf5158, BRF_GRA | BRF_OPT },	     //  19	PROMs
	{ "63s141.2e",     0x00100, 0x4a1285a4, BRF_GRA | BRF_OPT },	     //  20
};

STD_ROM_PICK(Gnga)
STD_ROM_FN(Gnga)

struct BurnDriver BurnDrvGnga = {
	"gnga", "gng", NULL, NULL, "1985",
	"Ghosts'n Goblins (World? set 2)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, GngaRomInfo, GngaRomName, NULL, NULL, NULL, NULL, GngInputInfo, GngDIPInfo,
	GngaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo GngblRomDesc[] = {
	{ "5.84490.10n",   0x04000, 0x66606beb, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "4.84490.9n",    0x04000, 0x527f5c39, BRF_ESS | BRF_PRG }, //	 1
	{ "3.84490.8n",    0x04000, 0x1c5175d5, BRF_ESS | BRF_PRG }, //	 2
	{ "7.84490.13n",   0x04000, 0xfd9a8dda, BRF_ESS | BRF_PRG }, //	 3
	{ "6.84490.12n",   0x04000, 0xc83dbd10, BRF_ESS | BRF_PRG }, //	 4

	{ "2.8529.13h",    0x08000, 0x55cfb196, BRF_ESS | BRF_PRG }, //  5	Z80 Program

	{ "1.84490.11e",   0x04000, 0xecfccf07, BRF_GRA },	     //  6	Characters

	{ "13.84490.3e",   0x04000, 0xddd56fa9, BRF_GRA },	     //  7	Tiles
	{ "12.84490.1e",   0x04000, 0x7302529d, BRF_GRA },	     //  8
	{ "11.84490.3c",   0x04000, 0x20035bda, BRF_GRA },	     //  9
	{ "10.84490.1c",   0x04000, 0xf12ba271, BRF_GRA },	     //  10
	{ "9.84490.3b",    0x04000, 0xe525207d, BRF_GRA },	     //  11
	{ "8.84490.1b",    0x04000, 0x2d77e9b2, BRF_GRA },	     //  12

	{ "19.84472.4n",   0x04000, 0x4613afdc, BRF_GRA },	     //  13	Sprites
	{ "18.84472.3n",   0x04000, 0x06d7e5ca, BRF_GRA },	     //  14
	{ "17.84472.1n",   0x04000, 0xbc1fe02d, BRF_GRA },	     //  15
	{ "16.84472.4l",   0x04000, 0x608d68d5, BRF_GRA },	     //  16
	{ "15.84490.3l",   0x04000, 0xe80c3fca, BRF_GRA },	     //  17
	{ "14.84490.1l",   0x04000, 0x7780a925, BRF_GRA },	     //  18
};

STD_ROM_PICK(Gngbl)
STD_ROM_FN(Gngbl)

struct BurnDriver BurnDrvGngbl = {
	"gngbl", "gng", NULL, NULL, "1985",
	"Ghosts'n Goblins (bootleg with Cross)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, GngblRomInfo, GngblRomName, NULL, NULL, NULL, NULL, GngInputInfo, GngDIPInfo,
	GngaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo GngblaRomDesc[] = {
	{ "3.bin",         0x04000, 0x4859d068, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "4.bin",         0x04000, 0x08322bef, BRF_ESS | BRF_PRG }, //	 1
	{ "5.bin",         0x04000, 0x888d7764, BRF_ESS | BRF_PRG }, //	 2
	{ "gng.n13",       0x04000, 0xfd9a8dda, BRF_ESS | BRF_PRG }, //	 3
	{ "2.bin",         0x04000, 0xf32c2e55, BRF_ESS | BRF_PRG }, //	 4

	{ "gg2.bin",       0x08000, 0x615f5b6f, BRF_ESS | BRF_PRG }, //  5	Z80 Program

	{ "gg1.bin",       0x04000, 0xecfccf07, BRF_GRA },	     //  5	Characters

	{ "gg11.bin",      0x04000, 0xddd56fa9, BRF_GRA },	     //  7	Tiles
	{ "gg10.bin",      0x04000, 0x7302529d, BRF_GRA },	     //  8
	{ "gg9.bin",       0x04000, 0x20035bda, BRF_GRA },	     //  9
	{ "gg8.bin",       0x04000, 0xf12ba271, BRF_GRA },	     //  10
	{ "gg7.bin",       0x04000, 0xe525207d, BRF_GRA },	     //  11
	{ "gg6.bin",       0x04000, 0x2d77e9b2, BRF_GRA },	     //  12

	{ "19.84472.4n",   0x04000, 0x4613afdc, BRF_GRA },	     //  13	Sprites
	{ "18.84472.3n",   0x04000, 0x06d7e5ca, BRF_GRA },	     //  14
	{ "17.84472.1n",   0x04000, 0xbc1fe02d, BRF_GRA },	     //  15
	{ "16.84472.4l",   0x04000, 0x608d68d5, BRF_GRA },	     //  16
	{ "15.84490.3l",   0x04000, 0xe80c3fca, BRF_GRA },	     //  17
	{ "14.84490.1l",   0x04000, 0x7780a925, BRF_GRA },	     //  18
};

STD_ROM_PICK(Gngbla)
STD_ROM_FN(Gngbla)

struct BurnDriver BurnDrvGngbla = {
	"gngbla", "gng", NULL, NULL, "1985",
	"Ghosts'n Goblins (bootleg, harder)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, GngblaRomInfo, GngblaRomName, NULL, NULL, NULL, NULL, GngInputInfo, GngDIPInfo,
	GngaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo GngblitaRomDesc[] = {
	{ "3",             0x04000, 0x4859d068, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "4-5",           0x08000, 0x233a4589, BRF_ESS | BRF_PRG }, //	 1
	{ "1-2",           0x08000, 0xed28e86e, BRF_ESS | BRF_PRG }, //	 2

	{ "gg2.bin",       0x08000, 0x615f5b6f, BRF_ESS | BRF_PRG }, //  3	Z80 Program

	{ "gg1.bin",       0x04000, 0xecfccf07, BRF_GRA },	     //  4	Characters

	{ "gg11.bin",      0x04000, 0xddd56fa9, BRF_GRA },	     //  5	Tiles
	{ "gg10.bin",      0x04000, 0x7302529d, BRF_GRA },	     //  6
	{ "gg9.bin",       0x04000, 0x20035bda, BRF_GRA },	     //  7
	{ "gg8.bin",       0x04000, 0xf12ba271, BRF_GRA },	     //  8
	{ "gg7.bin",       0x04000, 0xe525207d, BRF_GRA },	     //  9
	{ "gg6.bin",       0x04000, 0x2d77e9b2, BRF_GRA },	     //  10

	{ "gg17.bin",      0x04000, 0x93e50a8f, BRF_GRA },	     //  11	Sprites
	{ "gg16.bin",      0x04000, 0x06d7e5ca, BRF_GRA },	     //  12
	{ "gg15.bin",      0x04000, 0xbc1fe02d, BRF_GRA },	     //  13
	{ "gg14.bin",      0x04000, 0x6aaf12f9, BRF_GRA },	     //  14
	{ "gg13.bin",      0x04000, 0xe80c3fca, BRF_GRA },	     //  15
	{ "gg12.bin",      0x04000, 0x7780a925, BRF_GRA },	     //  16

	{ "tbp24s10.14k",  0x00100, 0x0eaf5158, BRF_GRA | BRF_OPT },	     //  17	PROMs
	{ "63s141.2e",     0x00100, 0x4a1285a4, BRF_GRA | BRF_OPT },	     //  18

	{ "gg-pal10l8.bin",0x0002c, 0x87f1b7e0, BRF_GRA | BRF_OPT },	     //  19	PLDs
};

STD_ROM_PICK(Gngblita)
STD_ROM_FN(Gngblita)

struct BurnDriver BurnDrvGngblita = {
	"gngblita", "gng", NULL, NULL, "1985",
	"Ghosts'n Goblins (Italian bootleg, harder)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, GngblitaRomInfo, GngblitaRomName, NULL, NULL, NULL, NULL, GngInputInfo, GngDIPInfo,
	GngInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo GngprotRomDesc[] = {
	{ "gg10n.bin",     0x04000, 0x5d2a2c90, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "gg9n.bin",      0x04000, 0x30eb183d, BRF_ESS | BRF_PRG }, //	 1
	{ "gg8n.bin",      0x04000, 0x4b5e2145, BRF_ESS | BRF_PRG }, //	 2
	{ "gg13n.bin",     0x04000, 0x2664aae6, BRF_ESS | BRF_PRG }, //	 3
	{ "gg12n.bin",     0x04000, 0xc7ef4ae8, BRF_ESS | BRF_PRG }, //	 4

	{ "gg14h.bin",     0x08000, 0x55cfb196, BRF_ESS | BRF_PRG }, //  5	Z80 Program

	{ "gg11e.bin",	   0x04000, 0xccea9365, BRF_GRA },	     //  6	Characters

	{ "gg3e.bin",      0x04000, 0x68db22c8, BRF_GRA },	     //  7	Tiles
	{ "gg1e.bin",      0x04000, 0xdad8dd2f, BRF_GRA },	     //  8
	{ "gg3c.bin",      0x04000, 0x7a158323, BRF_GRA },	     //  9
	{ "gg1c.bin",      0x04000, 0x7314d095, BRF_GRA },	     //  10
	{ "gg3b.bin",      0x04000, 0x03a96d9b, BRF_GRA },	     //  11
	{ "gg1b.bin",      0x04000, 0x7b9899bc, BRF_GRA },	     //  12

	{ "gg4l.bin",      0x04000, 0x49cf81b4, BRF_GRA },	     //  13	Sprites
	{ "gg3l.bin",      0x04000, 0xe61437b1, BRF_GRA },	     //  14
	{ "gg1l.bin",      0x04000, 0xbc1fe02d, BRF_GRA },	     //  15
	{ "gg4n.bin",      0x04000, 0xd5aff5a7, BRF_GRA },	     //  16
	{ "gg3n.bin",      0x04000, 0xd589caeb, BRF_GRA },	     //  17
	{ "gg1n.bin",      0x04000, 0x7780a925, BRF_GRA },	     //  18
};

STD_ROM_PICK(Gngprot)
STD_ROM_FN(Gngprot)

struct BurnDriver BurnDrvGngprot = {
	"gngprot", "gng", NULL, NULL, "1985",
	"Ghosts'n Goblins (prototype)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, GngprotRomInfo, GngprotRomName, NULL, NULL, NULL, NULL, GngInputInfo, GngDIPInfo,
	GngaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo GngtRomDesc[] = {
	{ "mmt04d.10n",    0x04000, 0x652406f6, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "mmt03d.8n",     0x08000, 0xfb040b42, BRF_ESS | BRF_PRG }, //	 1
	{ "mmt05d.13n",    0x08000, 0x8f7cff61, BRF_ESS | BRF_PRG }, //	 2

	{ "mm02.14h",      0x08000, 0x615f5b6f, BRF_ESS | BRF_PRG }, //  3	Z80 Program

	{ "mm01.11e",      0x04000, 0xecfccf07, BRF_GRA },	     //  4	Characters

	{ "mm11.3e",       0x04000, 0xddd56fa9, BRF_GRA },	     //  5	Tiles
	{ "mm10.1e",       0x04000, 0x7302529d, BRF_GRA },	     //  6
	{ "mm09.3c",       0x04000, 0x20035bda, BRF_GRA },	     //  7
	{ "mm08.1c",       0x04000, 0xf12ba271, BRF_GRA },	     //  8
	{ "mm07.3b",       0x04000, 0xe525207d, BRF_GRA },	     //  9
	{ "mm06.1b",       0x04000, 0x2d77e9b2, BRF_GRA },	     //  10

	{ "mm17.4n",       0x04000, 0x93e50a8f, BRF_GRA },	     //  11	Sprites
	{ "mm16.3n",       0x04000, 0x06d7e5ca, BRF_GRA },	     //  12
	{ "mm15.1n",       0x04000, 0xbc1fe02d, BRF_GRA },	     //  13
	{ "mm14.4l",       0x04000, 0x6aaf12f9, BRF_GRA },	     //  14
	{ "mm13.3l",       0x04000, 0xe80c3fca, BRF_GRA },	     //  15
	{ "mm12.1l",       0x04000, 0x7780a925, BRF_GRA },	     //  16

	{ "m-02.14k",      0x00100, 0x0eaf5158, BRF_GRA | BRF_OPT },	     //  17	PROMs
	{ "m-01.2e",       0x00100, 0x4a1285a4, BRF_GRA | BRF_OPT },	     //  18
};

STD_ROM_PICK(Gngt)
STD_ROM_FN(Gngt)

struct BurnDriver BurnDrvGngt = {
	"gngt", "gng", NULL, NULL, "1985",
	"Ghosts'n Goblins (US)\0", NULL, "Capcom (Taito America license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, GngtRomInfo, GngtRomName, NULL, NULL, NULL, NULL, GngInputInfo, GngDIPInfo,
	GngInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo GngcRomDesc[] = {
	{ "mm_c_04",       0x04000, 0x4f94130f, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "mm_c_03",       0x08000, 0x1def138a, BRF_ESS | BRF_PRG }, //	 1
	{ "mm_c_05",       0x08000, 0xed28e86e, BRF_ESS | BRF_PRG }, //	 2

	{ "gg2.bin",       0x08000, 0x615f5b6f, BRF_ESS | BRF_PRG }, //  3	Z80 Program

	{ "gg1.bin",       0x04000, 0xecfccf07, BRF_GRA },	     //  4	Characters

	{ "gg11.bin",      0x04000, 0xddd56fa9, BRF_GRA },	     //  5	Tiles
	{ "gg10.bin",      0x04000, 0x7302529d, BRF_GRA },	     //  6
	{ "gg9.bin",       0x04000, 0x20035bda, BRF_GRA },	     //  7
	{ "gg8.bin",       0x04000, 0xf12ba271, BRF_GRA },	     //  8
	{ "gg7.bin",       0x04000, 0xe525207d, BRF_GRA },	     //  9
	{ "gg6.bin",       0x04000, 0x2d77e9b2, BRF_GRA },	     //  10

	{ "gg17.bin",      0x04000, 0x93e50a8f, BRF_GRA },	     //  11	Sprites
	{ "gg16.bin",      0x04000, 0x06d7e5ca, BRF_GRA },	     //  12
	{ "gg15.bin",      0x04000, 0xbc1fe02d, BRF_GRA },	     //  13
	{ "gg14.bin",      0x04000, 0x6aaf12f9, BRF_GRA },	     //  14
	{ "gg13.bin",      0x04000, 0xe80c3fca, BRF_GRA },	     //  15
	{ "gg12.bin",      0x04000, 0x7780a925, BRF_GRA },	     //  16

	{ "tbp24s10.14k",  0x00100, 0x0eaf5158, BRF_GRA | BRF_OPT },	     //  17	PROMs
	{ "63s141.2e",     0x00100, 0x4a1285a4, BRF_GRA | BRF_OPT },	     //  18
};

STD_ROM_PICK(Gngc)
STD_ROM_FN(Gngc)

struct BurnDriver BurnDrvGngc = {
	"gngc", "gng", NULL, NULL, "1985",
	"Ghosts'n Goblins (World? set 3)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, GngcRomInfo, GngcRomName, NULL, NULL, NULL, NULL, GngInputInfo, GngDIPInfo,
	GngInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo MakaimurRomDesc[] = {
	{ "10n.rom",       0x04000, 0x81e567e0, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "8n.rom",        0x08000, 0x9612d66c, BRF_ESS | BRF_PRG }, //	 1
	{ "12n.rom",       0x08000, 0x65a6a97b, BRF_ESS | BRF_PRG }, //	 2

	{ "gg2.bin",       0x08000, 0x615f5b6f, BRF_ESS | BRF_PRG }, //  3	Z80 Program

	{ "gg1.bin",       0x04000, 0xecfccf07, BRF_GRA },	     //  4	Characters

	{ "gg11.bin",      0x04000, 0xddd56fa9, BRF_GRA },	     //  5	Tiles
	{ "gg10.bin",      0x04000, 0x7302529d, BRF_GRA },	     //  6
	{ "gg9.bin",       0x04000, 0x20035bda, BRF_GRA },	     //  7
	{ "gg8.bin",       0x04000, 0xf12ba271, BRF_GRA },	     //  8
	{ "gg7.bin",       0x04000, 0xe525207d, BRF_GRA },	     //  9
	{ "gg6.bin",       0x04000, 0x2d77e9b2, BRF_GRA },	     //  10

	{ "gng13.n4",      0x04000, 0x4613afdc, BRF_GRA },	     //  11	Sprites
	{ "gg16.bin",      0x04000, 0x06d7e5ca, BRF_GRA },	     //  12
	{ "gg15.bin",      0x04000, 0xbc1fe02d, BRF_GRA },	     //  13
	{ "gng16.l4",      0x04000, 0x608d68d5, BRF_GRA },	     //  14
	{ "gg13.bin",      0x04000, 0xe80c3fca, BRF_GRA },	     //  15
	{ "gg12.bin",      0x04000, 0x7780a925, BRF_GRA },	     //  16

	{ "tbp24s10.14k",  0x00100, 0x0eaf5158, BRF_GRA | BRF_OPT },	     //  17	PROMs
	{ "63s141.2e",     0x00100, 0x4a1285a4, BRF_GRA | BRF_OPT },	     //  18
};

STD_ROM_PICK(Makaimur)
STD_ROM_FN(Makaimur)

struct BurnDriver BurnDrvMakaimur = {
	"makaimur", "gng", NULL, NULL, "1985",
	"Makaimura (Japan)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, MakaimurRomInfo, MakaimurRomName, NULL, NULL, NULL, NULL, GngInputInfo, MakaimurDIPInfo,
	GngInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo MakaimubRomDesc[] = {
	{ "mj04b.10n",     0x04000, 0xf8bda78f, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "mj03b.8n",      0x08000, 0x0ba14114, BRF_ESS | BRF_PRG }, //	 1
	{ "mj05b.12n",     0x08000, 0x3040a574, BRF_ESS | BRF_PRG }, //	 2

	{ "mm02.14h",      0x08000, 0x615f5b6f, BRF_ESS | BRF_PRG }, //  3	Z80 Program

	{ "mj01.11e",      0x04000, 0x178366b4, BRF_GRA },	     //  4	Characters

	{ "mm11.3e",       0x04000, 0xddd56fa9, BRF_GRA },	     //  5	Tiles
	{ "mm10.1e",       0x04000, 0x7302529d, BRF_GRA },	     //  6
	{ "mm09.3c",       0x04000, 0x20035bda, BRF_GRA },	     //  7
	{ "mm08.1c",       0x04000, 0xf12ba271, BRF_GRA },	     //  8
	{ "mm07.3b",       0x04000, 0xe525207d, BRF_GRA },	     //  9
	{ "mm06.1b",       0x04000, 0x2d77e9b2, BRF_GRA },	     //  10

	{ "mj17.4n",       0x04000, 0x4613afdc, BRF_GRA },	     //  11	Sprites
	{ "mj16.3n",       0x04000, 0x06d7e5ca, BRF_GRA },	     //  12
	{ "mj15.1n",       0x04000, 0xbc1fe02d, BRF_GRA },	     //  13
	{ "mj14.4l",       0x04000, 0x608d68d5, BRF_GRA },	     //  14
	{ "mj13.3l",       0x04000, 0xe80c3fca, BRF_GRA },	     //  15
	{ "mj12.1l",       0x04000, 0x7780a925, BRF_GRA },	     //  16

	{ "tbp24s10.14k",  0x00100, 0x0eaf5158, BRF_GRA | BRF_OPT },	     //  17	PROMs
	{ "63s141.2e",     0x00100, 0x4a1285a4, BRF_GRA | BRF_OPT },	     //  18
};

STD_ROM_PICK(Makaimub)
STD_ROM_FN(Makaimub)

struct BurnDriver BurnDrvMakaimub = {
	"makaimurb", "gng", NULL, NULL, "1985",
	"Makaimura (Japan revision B)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, MakaimubRomInfo, MakaimubRomName, NULL, NULL, NULL, NULL, GngInputInfo, MakaimurDIPInfo,
	GngInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo MakaimubblRomDesc[] = {
	{ "gg5.bin",       0x04000, 0xf8bda78f, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "gg4.bin",       0x04000, 0xac0b25fb, BRF_ESS | BRF_PRG }, //	 1
	{ "gg3.bin",       0x04000, 0x762b5af0, BRF_ESS | BRF_PRG }, //	 2
	{ "gg7.bin",       0x04000, 0xfd9a8dda, BRF_ESS | BRF_PRG }, //	 3
	{ "gg6.bin",       0x04000, 0x2e44634f, BRF_ESS | BRF_PRG }, //	 4

	{ "gg2.bin",       0x08000, 0x615f5b6f, BRF_ESS | BRF_PRG }, //  5	Z80 Program

	{ "gg1.bin",       0x04000, 0xecfccf07, BRF_GRA },	         //  6	Characters

	{ "gg13.bin",      0x04000, 0xddd56fa9, BRF_GRA },	         //  7	Tiles
	{ "gg12.bin",      0x04000, 0x7302529d, BRF_GRA },	         //  8
	{ "gg11.bin",      0x04000, 0x20035bda, BRF_GRA },	         //  9
	{ "gg10.bin",      0x04000, 0xf12ba271, BRF_GRA },	         //  10
	{ "gg9.bin",       0x04000, 0xe525207d, BRF_GRA },	         //  11
	{ "gg8.bin",       0x04000, 0x2d77e9b2, BRF_GRA },	         //  12

	{ "gg19.bin",      0x04000, 0x93e50a8f, BRF_GRA },	         //  13	Sprites
	{ "gg18.bin",      0x04000, 0x06d7e5ca, BRF_GRA },	         //  14
	{ "gg17.bin",      0x04000, 0xbc1fe02d, BRF_GRA },	         //  15
	{ "gg16.bin",      0x04000, 0x6aaf12f9, BRF_GRA },	         //  16
	{ "gg15.bin",      0x04000, 0xe80c3fca, BRF_GRA },	         //  17
	{ "gg14.bin",      0x04000, 0x7780a925, BRF_GRA },	         //  18

	{ "prom1",         0x00100, 0x00000000, BRF_GRA | BRF_NODUMP },	     //  19	PROMs
	{ "prom2",         0x00100, 0x00000000, BRF_GRA | BRF_NODUMP },	     //  20
};

STD_ROM_PICK(Makaimubbl)
STD_ROM_FN(Makaimubbl)

struct BurnDriver BurnDrvMakaimubbl = {
	"makaimurbbl", "gng", NULL, NULL, "1985",
	"Makaimura (Japan revision B bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, MakaimubblRomInfo, MakaimubblRomName, NULL, NULL, NULL, NULL, GngInputInfo, MakaimurDIPInfo,
	GngaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo MakaimucRomDesc[] = {
	{ "mj04c.bin",     0x04000, 0x1294edb1, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "mj03c.bin",     0x08000, 0xd343332d, BRF_ESS | BRF_PRG }, //	 1
	{ "mj05c.bin",     0x08000, 0x535342c2, BRF_ESS | BRF_PRG }, //	 2

	{ "gg2.bin",       0x08000, 0x615f5b6f, BRF_ESS | BRF_PRG }, //  3	Z80 Program

	{ "gg1.bin",       0x04000, 0xecfccf07, BRF_GRA },	     //  4	Characters

	{ "gg11.bin",      0x04000, 0xddd56fa9, BRF_GRA },	     //  5	Tiles
	{ "gg10.bin",      0x04000, 0x7302529d, BRF_GRA },	     //  6
	{ "gg9.bin",       0x04000, 0x20035bda, BRF_GRA },	     //  7
	{ "gg8.bin",       0x04000, 0xf12ba271, BRF_GRA },	     //  8
	{ "gg7.bin",       0x04000, 0xe525207d, BRF_GRA },	     //  9
	{ "gg6.bin",       0x04000, 0x2d77e9b2, BRF_GRA },	     //  10

	{ "gng13.n4",      0x04000, 0x4613afdc, BRF_GRA },	     //  11	Sprites
	{ "gg16.bin",      0x04000, 0x06d7e5ca, BRF_GRA },	     //  12
	{ "gg15.bin",      0x04000, 0xbc1fe02d, BRF_GRA },	     //  13
	{ "gng16.l4",      0x04000, 0x608d68d5, BRF_GRA },	     //  14
	{ "gg13.bin",      0x04000, 0xe80c3fca, BRF_GRA },	     //  15
	{ "gg12.bin",      0x04000, 0x7780a925, BRF_GRA },	     //  16

	{ "tbp24s10.14k",  0x00100, 0x0eaf5158, BRF_GRA | BRF_OPT },	     //  17	PROMs
	{ "63s141.2e",     0x00100, 0x4a1285a4, BRF_GRA | BRF_OPT },	     //  18
};

STD_ROM_PICK(Makaimuc)
STD_ROM_FN(Makaimuc)

struct BurnDriver BurnDrvMakaimuc = {
	"makaimurc", "gng", NULL, NULL, "1985",
	"Makaimura (Japan revision C)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, MakaimucRomInfo, MakaimucRomName, NULL, NULL, NULL, NULL, GngInputInfo, MakaimurDIPInfo,
	GngInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo MakaimugRomDesc[] = {
	{ "mj04g.bin",     0x04000, 0x757c94d3, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "mj03g.bin",     0x08000, 0x61b043bb, BRF_ESS | BRF_PRG }, //	 1
	{ "mj05g.bin",     0x08000, 0xf2fdccf5, BRF_ESS | BRF_PRG }, //	 2

	{ "gg2.bin",       0x08000, 0x615f5b6f, BRF_ESS | BRF_PRG }, //  3	Z80 Program

	{ "gg1.bin",       0x04000, 0xecfccf07, BRF_GRA },	     //  4	Characters

	{ "gg11.bin",      0x04000, 0xddd56fa9, BRF_GRA },	     //  5	Tiles
	{ "gg10.bin",      0x04000, 0x7302529d, BRF_GRA },	     //  6
	{ "gg9.bin",       0x04000, 0x20035bda, BRF_GRA },	     //  7
	{ "gg8.bin",       0x04000, 0xf12ba271, BRF_GRA },	     //  8
	{ "gg7.bin",       0x04000, 0xe525207d, BRF_GRA },	     //  9
	{ "gg6.bin",       0x04000, 0x2d77e9b2, BRF_GRA },	     //  10

	{ "gng13.n4",      0x04000, 0x4613afdc, BRF_GRA },	     //  11	Sprites
	{ "gg16.bin",      0x04000, 0x06d7e5ca, BRF_GRA },	     //  12
	{ "gg15.bin",      0x04000, 0xbc1fe02d, BRF_GRA },	     //  13
	{ "gng16.l4",      0x04000, 0x608d68d5, BRF_GRA },	     //  14
	{ "gg13.bin",      0x04000, 0xe80c3fca, BRF_GRA },	     //  15
	{ "gg12.bin",      0x04000, 0x7780a925, BRF_GRA },	     //  16

	{ "tbp24s10.14k",  0x00100, 0x0eaf5158, BRF_GRA | BRF_OPT },	     //  17	PROMs
	{ "63s141.2e",     0x00100, 0x4a1285a4, BRF_GRA | BRF_OPT },	     //  18
};

STD_ROM_PICK(Makaimug)
STD_ROM_FN(Makaimug)

struct BurnDriver BurnDrvMakaimug = {
	"makaimurg", "gng", NULL, NULL, "1985",
	"Makaimura (Japan revision G)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, MakaimugRomInfo, MakaimugRomName, NULL, NULL, NULL, NULL, GngInputInfo, MakaimurDIPInfo,
	GngInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static struct BurnRomInfo DiamrunRomDesc[] = {
	{ "d3o",           0x04000, 0xba4bf9f1, BRF_ESS | BRF_PRG }, //  0	M6809 Program Code
	{ "d3",            0x08000, 0xf436d6fa, BRF_ESS | BRF_PRG }, //	 1
	{ "d5o",           0x08000, 0xae58bd3a, BRF_ESS | BRF_PRG }, //	 2
	{ "d5",            0x08000, 0x453f3f9e, BRF_ESS | BRF_PRG }, //	 3

	{ "d2",            0x08000, 0x615f5b6f, BRF_ESS | BRF_PRG }, //  4	Z80 Program

	{ "d1",            0x04000, 0x3a24e504, BRF_GRA },	     //  5	Characters

	{ "d11",           0x04000, 0x754357d7, BRF_GRA },	     //  6	Tiles
	{ "d10",           0x04000, 0x7531edcd, BRF_GRA },	     //  7
	{ "d9",            0x04000, 0x22eeca08, BRF_GRA },	     //  8
	{ "d8",            0x04000, 0x6b61be60, BRF_GRA },	     //  9
	{ "d7",            0x04000, 0xfd595274, BRF_GRA },	     //  10
	{ "d6",            0x04000, 0x7f51dcd2, BRF_GRA },	     //  11

	{ "d17",           0x04000, 0x8164b005, BRF_GRA },	     //  12	Sprites
	{ "d14",           0x04000, 0x6f132163, BRF_GRA },	     //  13

	{ "prom1",         0x00100, 0x0eaf5158, BRF_GRA | BRF_OPT },	     //  14	PROMs
	{ "prom2",         0x00100, 0x4a1285a4, BRF_GRA | BRF_OPT },	     //  15
};

STD_ROM_PICK(Diamrun)
STD_ROM_FN(Diamrun)

struct BurnDriver BurnDrvDiamrun = {
	"diamrun", NULL, NULL, NULL, "1989",
	"Diamond Run\0", NULL, "KH Video", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM, 0,
	NULL, DiamrunRomInfo, DiamrunRomName, NULL, NULL, NULL, NULL, DiamrunInputInfo, DiamrunDIPInfo,
	DiamrunInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};
