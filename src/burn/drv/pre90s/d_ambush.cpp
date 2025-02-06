// FinalBurn Neo Ambush driver module
// Based on MAME driver by Zsolt Vasvari

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "watchdog.h"
#include "bitswap.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvScrRAM;

static UINT8 *color_bank;
static UINT8 *flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo AmbushInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 2"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 fire 2"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
};

STDINPUTINFO(Ambush)

static struct BurnInputInfo MarioblInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 1"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
};

STDINPUTINFO(Mariobl)

static struct BurnInputInfo Dkong3ablInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 1"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
};

STDINPUTINFO(Dkong3abl)

static struct BurnDIPInfo AmbushDIPList[]=
{
	{0x11, 0xf0, 0xff, 0xff, NULL										},
	{0x00, 0xff, 0xff, 0xc0, NULL										},

	{0   , 0xfe, 0   ,    4, "Lives"									},
	{0x00, 0x01, 0x03, 0x00, "3"										},
	{0x00, 0x01, 0x03, 0x01, "4"										},
	{0x00, 0x01, 0x03, 0x02, "5"										},
	{0x00, 0x01, 0x03, 0x03, "6"										},

	{0   , 0xfe, 0   ,    8, "Coinage"									},
	{0x00, 0x01, 0x1c, 0x10, "2 Coins 1 Credits"						},
	{0x00, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"						},
	{0x00, 0x01, 0x1c, 0x14, "2 Coins 3 Credits"						},
	{0x00, 0x01, 0x1c, 0x04, "1 Coin  2 Credits"						},
	{0x00, 0x01, 0x1c, 0x18, "2 Coins 5 Credits"						},
	{0x00, 0x01, 0x1c, 0x08, "1 Coin  3 Credits"						},
	{0x00, 0x01, 0x1c, 0x0c, "1 Coin  4 Credits"						},
	{0x00, 0x01, 0x1c, 0x1c, "Service Mode/Free Play"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"								},
	{0x00, 0x01, 0x20, 0x00, "Easy"										},
	{0x00, 0x01, 0x20, 0x20, "Hard"										},

	{0   , 0xfe, 0   ,    2, "Bonus Life"								},
	{0x00, 0x01, 0x40, 0x40, "80000"									},
	{0x00, 0x01, 0x40, 0x00, "120000"									},

	{0   , 0xfe, 0   ,    2, "Cabinet"									},
	{0x00, 0x01, 0x80, 0x80, "Upright"									},
	{0x00, 0x01, 0x80, 0x00, "Cocktail"									},
};

STDDIPINFO(Ambush)

static struct BurnDIPInfo AmbushtDIPList[]=
{
	{0x11, 0xf0, 0xff, 0xff, NULL										},
	{0x00, 0xff, 0xff, 0xc4, NULL										},

	{0   , 0xfe, 0   ,    4, "Lives"									},
	{0x00, 0x01, 0x03, 0x00, "3"										},
	{0x00, 0x01, 0x03, 0x01, "4"										},
	{0x00, 0x01, 0x03, 0x02, "5"										},
	{0x00, 0x01, 0x03, 0x03, "6"										},

	{0   , 0xfe, 0   ,    2, "Allow Continue"							},
	{0x00, 0x01, 0x04, 0x00, "No"										},
	{0x00, 0x01, 0x04, 0x04, "Yes"										},

	{0   , 0xfe, 0   ,    2, "Service Mode/Free Play"					},
	{0x00, 0x01, 0x08, 0x00, "Off"										},
	{0x00, 0x01, 0x08, 0x08, "On"										},

	{0   , 0xfe, 0   ,    2, "Coinage"									},
	{0x00, 0x01, 0x10, 0x00, "A 1 Coin/1 Credit B 1 Coin/2 Credits"		},
	{0x00, 0x01, 0x10, 0x10, "A 2 Coins/3 Credits B 1 Coin/3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Difficulty"								},
	{0x00, 0x01, 0x20, 0x00, "Easy"										},
	{0x00, 0x01, 0x20, 0x20, "Hard"										},

	{0   , 0xfe, 0   ,    2, "Bonus Life"								},
	{0x00, 0x01, 0x40, 0x40, "80000"									},
	{0x00, 0x01, 0x40, 0x00, "120000"									},

	{0   , 0xfe, 0   ,    2, "Cabinet"									},
	{0x00, 0x01, 0x80, 0x80, "Upright"									},
	{0x00, 0x01, 0x80, 0x00, "Cocktail"									},
};

STDDIPINFO(Ambusht)

static struct BurnDIPInfo MarioblDIPList[]=
{
	{0x0b, 0xf0, 0xff, 0xff, NULL										},
	{0x00, 0xff, 0xff, 0x20, NULL										},

	{0   , 0xfe, 0   ,    4, "Lives"									},
	{0x00, 0x01, 0x03, 0x00, "3"										},
	{0x00, 0x01, 0x03, 0x01, "4"										},
	{0x00, 0x01, 0x03, 0x02, "5"										},
	{0x00, 0x01, 0x03, 0x03, "6"										},

	{0   , 0xfe, 0   ,    8, "Coinage"									},
	{0x00, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"						},
	{0x00, 0x01, 0x1c, 0x10, "2 Coins 1 Credits"						},
	{0x00, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"						},
	{0x00, 0x01, 0x1c, 0x18, "1 Coin  2 Credits"						},
	{0x00, 0x01, 0x1c, 0x04, "1 Coin  3 Credits"						},
	{0x00, 0x01, 0x1c, 0x0c, "1 Coin  4 Credits"						},
	{0x00, 0x01, 0x1c, 0x14, "1 Coin  5 Credits"						},
	{0x00, 0x01, 0x1c, 0x1c, "1 Coin  6 Credits"						},

	{0   , 0xfe, 0   ,    2, "2 Players Game"							},
	{0x00, 0x01, 0x20, 0x00, "1 Credit"									},
	{0x00, 0x01, 0x20, 0x20, "2 Credits"								},

	{0   , 0xfe, 0   ,    4, "Bonus Life"								},
	{0x00, 0x01, 0xc0, 0x00, "20k 50k 30k+"								},
	{0x00, 0x01, 0xc0, 0x40, "30k 60k 30k+"								},
	{0x00, 0x01, 0xc0, 0x80, "40k 70k 30k+"								},
	{0x00, 0x01, 0xc0, 0xc0, "None"										},
};

STDDIPINFO(Mariobl)

static struct BurnDIPInfo Dkong3ablDIPList[]=
{
	{0x0f, 0xf0, 0xff, 0xff, NULL										},
	{0x00, 0xff, 0xff, 0x10, NULL										},

	{0   , 0xfe, 0   ,    4, "Bonus Life"								},
	{0x00, 0x01, 0x0c, 0x00, "30k"										},
	{0x00, 0x01, 0x0c, 0x04, "40k"										},
	{0x00, 0x01, 0x0c, 0x08, "50k"										},
	{0x00, 0x01, 0x0c, 0x0c, "None"										},

	{0   , 0xfe, 0   ,    2, "Unknown"									},
	{0x00, 0x01, 0x10, 0x10, "Off"										},
	{0x00, 0x01, 0x10, 0x00, "On"										},

	{0   , 0xfe, 0   ,    2, "Coinage"									},
	{0x00, 0x01, 0x20, 0x00, "1 Coin  1 Credits"						},
	{0x00, 0x01, 0x20, 0x20, "2 Coins 1 Credits"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"								},
	{0x00, 0x01, 0xc0, 0x00, "Easy"										},
	{0x00, 0x01, 0xc0, 0x40, "Medium"									},
	{0x00, 0x01, 0xc0, 0x80, "Hard"										},
	{0x00, 0x01, 0xc0, 0xc0, "Hardest"									},
};

STDDIPINFO(Dkong3abl)

static UINT8 __fastcall ambush_read_byte(UINT16 address)
{
	switch (address)
	{
		case 0xc800:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall ambush_write_byte(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xcc04:
			*flipscreen = data & 1;	
		break;

		case 0xcc05:
			*color_bank = (data & 3) << 4;
		break;
	}
}

static void __fastcall bootleg_write_byte(UINT16 address, UINT8 data)// mariobl, dkongabl
{
	switch (address)
	{
		case 0xa204:
			data &= 1; // coin counter
		return;

		case 0xa206:
			*color_bank = (data & 1) << 4;
		return;
	}
}

static void __fastcall mariobla_write_byte(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa205:
			*color_bank = (data & 1) << 4;
		return;
	}
}

static UINT8 __fastcall bootleg_read_byte(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return BurnWatchdogRead();

		case 0xa100:
			return DrvDips[0];
	}

	return 0;
}

static UINT8 __fastcall ambush_in_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x80:
			return AY8910Read((port >> 7) & 1); 
	}

	return 0;
}

static void __fastcall ambush_out_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x80:
		case 0x81:
			AY8910Write((port >> 7) & 1, port & 1, data);
		break;
	}
}

static UINT8 AY8910_0_port0(UINT32)
{
	return DrvInputs[0];
}

static UINT8 AY8910_1_port0(UINT32)
{
	return DrvInputs[1];
}

static tilemap_callback( ambush )
{
	INT32 attr = DrvColRAM[((offs >> 2) & 0xe0) | (offs & 0x1f)];

	INT32 code = ((attr & 0x60) << 3) | DrvVidRAM[offs];
	INT32 color = (attr & 0x0f) | *color_bank;

	TILE_SET_INFO(0, code, color, TILE_GROUP((attr & 0x10) >> 4));
}

static tilemap_callback( mariobl )
{
	INT32 attr0 = DrvColRAM[((offs >> 2) & 0xe0) | (offs & 0x1f)];
	INT32 attr1 = DrvVidRAM[offs];

	INT32 code = ((attr0 & 0x40) << 2) | attr1;
	INT32 color =((attr1 & 0xe0) >> 5) | ((attr0 & 0x40) >> 2) | 8;

	TILE_SET_INFO(0, code, color, 0);
}

static tilemap_callback( dkong3abl )
{
	INT32 attr0 = DrvColRAM[((offs >> 2) & 0xe0) | (offs & 0x1f)];
	INT32 attr1 = DrvVidRAM[offs];

	INT32 code = ((attr0 & 0x40) << 2) | attr1;
	INT32 color =((attr0 & 0x07) >> 0) | ((attr0 & 0x40) >> 1) | ((attr0 & 0x40) >> 2);

	TILE_SET_INFO(0, code, color, 0);
}

static INT32 DrvDoReset(INT32 nClearMem)
{
	if (nClearMem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetReset(0);
	AY8910Reset(0);
	AY8910Reset(1);
	BurnWatchdogReset();

	HiscoreReset();

	return 0;
}

static void DrvPaletteInit(INT32 type)
{
	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		INT32 c = i;
		if (type == 1) c = BITSWAP16(i, 15, 14, 13, 12, 11, 10, 9, 2, 7, 6, 8, 5, 4, 3, 1, 0);
		INT32 d = DrvColPROM[c];

		INT32 bit0 = (d >> 0) & 0x01;
		INT32 bit1 = (d >> 1) & 0x01;
		INT32 bit2 = (d >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (d >> 3) & 0x01;
		bit1 = (d >> 4) & 0x01;
		bit2 = (d >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit1 = (d >> 6) & 0x01;
		bit2 = (d >> 7) & 0x01;
		INT32 b = 0x47 * bit1 + 0x97 * bit2;

		BurnPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void DrvDkong3PaletteInit()
{
	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		INT32 d = DrvColPROM[i] | (DrvColPROM[i + 0x200] << 8);

		INT32 bit0 = (d >> 4) & 0x01;
		INT32 bit1 = (d >> 5) & 0x01;
		INT32 bit2 = (d >> 6) & 0x01;
		INT32 bit3 = (d >> 7) & 0x01;
		INT32 r = 255 - (0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3);

		bit0 = (d >>  0) & 0x01;
		bit1 = (d >>  1) & 0x01;
		bit2 = (d >>  2) & 0x01;
		bit3 = (d >>  3) & 0x01;
		INT32 g = 255 - (0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3);

		bit0 = (d >>  8) & 0x01;
		bit1 = (d >>  9) & 0x01;
		bit2 = (d >> 10) & 0x01;
		bit3 = (d >> 11) & 0x01;
		INT32 b = 255 - (0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3);

		BurnPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void DrvGfxDecode(UINT8 *src, UINT8 *dst, INT32 type)
{
	static INT32 Planes[3] = { 0x2000 * 8 * 2, 0x2000 * 8 * 1, 0 };
	static INT32 XOffs[16] = { STEP8(0,1), STEP8(64,1) };
	static INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8 *)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, src, 0x6000);

	switch (type)
	{
		case 0: GfxDecode(0x400, 2,  8,  8, Planes+1, XOffs, YOffs, 0x040, tmp, dst); break;
		case 1: GfxDecode(0x100, 2, 16, 16, Planes+1, XOffs, YOffs, 0x100, tmp, dst); break;
		case 2: GfxDecode(0x100, 3, 16, 16, Planes+0, XOffs, YOffs, 0x100, tmp, dst); break; // mariobl/a
	}

	BurnFree (tmp);
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x010000;
	DrvGfxROM1	= Next; Next += 0x010000;

	DrvColPROM	= Next; Next += 0x000400;

	BurnPalette	= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM	= Next; Next += 0x001400;
	DrvVidRAM	= Next; Next += 0x000400;
	DrvSprRAM	= Next; Next += 0x000200;
	DrvColRAM	= Next; Next += 0x000100;
	DrvScrRAM	= Next; Next += 0x000100;

	flipscreen 	= Next; Next += 0x000001;
	color_bank	= Next; Next += 0x000001;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000, 2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x6000, 3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x2000, 4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0000, 5, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 6, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, DrvGfxROM1, 1);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, 0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvScrRAM,		0xc000, 0xc0ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0xc100, 0xc1ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xc200, 0xc3ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xc400, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(ambush_write_byte);
	ZetSetReadHandler(ambush_read_byte);
	ZetSetOutHandler(ambush_out_port);
	ZetSetInHandler(ambush_in_port);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, -1); // not on non-bootleg hardware

	AY8910Init(0, 1536000, 0);
	AY8910Init(1, 1536000, 1);
	AY8910SetPorts(0, &AY8910_0_port0, NULL, NULL, NULL);
	AY8910SetPorts(1, &AY8910_1_port0, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.33, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.33, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 4000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, ambush_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2,  8,  8, 0x10000, 0, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 2, 16, 16, 0x10000, 0, 0x3f);
	GenericTilemapSetScrollCols(0, 32);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset(1);

	return 0;
}

static void BootlegCommonInit(void (__fastcall *write_byte)(UINT16,UINT8), INT32 gfxbpp)
{
	DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, 0);
	DrvGfxDecode(DrvGfxROM1, DrvGfxROM1, (gfxbpp == 3) ? 2 : 1);

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0x6000, 0x73ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0x7000, 0x71ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,				0x7200, 0x72ff, MAP_RAM);
//	ZetMapMemory(DrvScrRAM,				0x7380, 0x739f, MAP_RAM);
	ZetMapMemory(DrvVidRAM,				0x7400, 0x77ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0x8000,	0x8000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM + 0xb000,	0xb000, 0xffff, MAP_ROM);
	ZetSetWriteHandler(write_byte);
	ZetSetReadHandler(bootleg_read_byte);
	ZetSetOutHandler(ambush_out_port);
	ZetSetInHandler(ambush_in_port);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	AY8910Init(0, 1536000, 0);
	AY8910Init(1, 1536000, 1);
	AY8910SetPorts(0, &AY8910_0_port0, NULL, NULL, NULL);
	AY8910SetPorts(1, &AY8910_1_port0, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.33, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.33, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 4000000);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM0, gfxbpp,  8,  8, 0x10000, 0, 0x3f >> (gfxbpp - 2));
	GenericTilemapSetGfx(1, DrvGfxROM1, gfxbpp, 16, 16, 0x10000, 0, 0x1f);

	DrvDoReset(1);
}

static INT32 MarioblInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0xe000, 2, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x0000, 3, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x2000, 4, 1, LD_INVERT)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000, 5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000, 6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000, 7, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 8, 1)) return 1;
	}

	BootlegCommonInit(bootleg_write_byte, 3);

	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, mariobl_map_callback, 8, 8, 32, 32);
	GenericTilemapSetOffsets(0, 0, -16);

	return 0;
}

static INT32 MarioblaInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0xe000, 2, 1)) return 1;

		{
			UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
			
			if (BurnLoadRom(tmp, 3, 1)) return 1;

			memcpy (DrvGfxROM1 + 0x0000, tmp + 0x0000, 0x2000);
			memcpy (DrvGfxROM0 + 0x0000, tmp + 0x2800, 0x0800);
			memcpy (DrvGfxROM0 + 0x0800, tmp + 0x3800, 0x0800);

			if (BurnLoadRom(DrvGfxROM1 + 0x2000, 4, 1)) return 1;
			if (BurnLoadRom(tmp, 5, 1)) return 1;

			memcpy (DrvGfxROM1 + 0x4000, tmp + 0x0000, 0x2000);
			memcpy (DrvGfxROM0 + 0x2000, tmp + 0x2800, 0x0800);
			memcpy (DrvGfxROM0 + 0x2800, tmp + 0x3800, 0x0800);

			for (INT32 i = 0; i < 0x4000; i++) {
				DrvGfxROM0[i] ^= 0xff;
			}

			BurnFree (tmp);
		}

		if (BurnLoadRom(DrvColPROM + 0x0000, 6, 1)) return 1;
	}

	BootlegCommonInit(mariobla_write_byte, 3);

	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, mariobl_map_callback, 8, 8, 32, 32);
	GenericTilemapSetOffsets(0, 0, -16);

	return 0;
}

static INT32 Dkong3ablInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000, 1, 1)) return 1;
		memcpy (DrvZ80ROM + 0x8000, DrvZ80ROM + 0x6000, 0x2000);
		if (BurnLoadRom(DrvZ80ROM  + 0xb000, 2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000, 3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000, 4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000, 5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000, 6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0200, 8, 1)) return 1;
	}

	BootlegCommonInit(bootleg_write_byte, 2);

	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, dkong3abl_map_callback, 8, 8, 32, 32);
	GenericTilemapSetOffsets(0, 0, -16);

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

static void draw_sprites()
{
	for (INT32 offs = 0x200 - 4; offs >= 0; offs -= 4)
	{
		INT32 sy = DrvSprRAM[offs + 0];
		INT32 sx = DrvSprRAM[offs + 3];

		if (sy == 0 || sy == 0xff || (sx < 0x40 && DrvSprRAM[offs + 2] & 0x10) || (sx >= 0xc0 && ~DrvSprRAM[offs + 2] & 0x10))
			continue;

		INT32 code  = (DrvSprRAM[offs + 1] & 0x3f) | ((DrvSprRAM[offs + 2] & 0x60) << 1);
		INT32 color =(DrvSprRAM[offs + 2] & 0x0f) | *color_bank;
		INT32 flipx = DrvSprRAM[offs + 1] & 0x40;
		INT32 flipy = DrvSprRAM[offs + 1] & 0x80;
		INT32 size  =(DrvSprRAM[offs + 2] & 0x80) ? 16 : 8;

		if (*flipscreen) {
			flipx = !flipx;
			flipy = !flipy;
			sx = (256 - size) - sx;
		} else {
			sy = (256 - size) - sy;
		}

		DrawGfxMaskTile(0, size/16, code << ((size == 8) ? 2 : 0), sx, sy - 16, flipx, flipy, color, 0);
	}
}

static void bootleg_draw_sprites()
{
	for (INT32 offs = 0x200 - 4; offs >= 0; offs -= 4)
	{
		INT32 sx = DrvSprRAM[offs + 0];
		INT32 sy = DrvSprRAM[offs + 3];

		if (sy == 0x00 || sy == 0xff)
			continue;

		INT32 code = ((DrvSprRAM[offs + 1] & 0x40) << 1) | (DrvSprRAM[offs + 2] & 0x7f);
		INT32 color = (*color_bank) | (DrvSprRAM[offs + 1] & 0x0f);
		INT32 flipx = DrvSprRAM[offs + 1] & 0x80;
		INT32 flipy = DrvSprRAM[offs + 2] & 0x80;

		DrawGfxMaskTile(0, 1, code, sx, (240 - sy) - 16, flipx, flipy, color, 0);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteInit(0);
		BurnRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetFlip(0, (*flipscreen) ? TMAP_FLIPXY : 0);

	for (INT32 i = 0; i < 32; i++) {
		GenericTilemapSetScrollCol(0, i, DrvScrRAM[i + 0x80] + 1);
	}

	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, TMAP_FORCEOPAQUE);
	if (nSpriteEnable & 1) draw_sprites();
	if (nBurnLayer & 2) GenericTilemapDraw(0, 0, TMAP_SET_GROUP(1));

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 MarioblDraw()
{
	if (BurnRecalc) {
		DrvPaletteInit(1);
		BurnRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);
	if (nSpriteEnable & 1) bootleg_draw_sprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 Dkong3ablDraw()
{
	if (BurnRecalc) {
		DrvDkong3PaletteInit();
		BurnRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);
	if (nSpriteEnable & 1) bootleg_draw_sprites();

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
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	ZetOpen(0);
	ZetRun(4000000 / 60);
	ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	ZetClose();

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
	}

	return 0;
}


// Ambush

static struct BurnRomInfo ambushRomDesc[] = {
	{ "a1.i7",			0x2000, 0x31b85d9d, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "a2.g7",			0x2000, 0x8328d88a, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "a3.f7",			0x2000, 0x8db57ab5, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "a4.e7",			0x2000, 0x4a34d2a4, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "fa2.n4",			0x2000, 0xe7f134ba, 2 | BRF_GRA },	   	     //  4 Graphics tiles
	{ "fa1.m4",			0x2000, 0xad10969e, 2 | BRF_GRA },	         //  5

	{ "a.bpr",			0x0100, 0x5f27f511, 4 | BRF_GRA },	         //  6 color PROMs

	{ "b.bpr",			0x0100, 0x1b03fd3b, 0 | BRF_OPT },	         //  7 Proms - Not used
	{ "13.bpr",			0x0100, 0x547e970f, 0 | BRF_OPT },	         //  8
	{ "14.bpr",			0x0100, 0x622a8ce7, 0 | BRF_OPT },	         //  9
};

STD_ROM_PICK(ambush)
STD_ROM_FN(ambush)

struct BurnDriver BurnDrvAmbush = {
	"ambush", NULL, NULL, NULL, "1983",
	"Ambush\0", NULL, "Tecfri", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ambushRomInfo, ambushRomName, NULL, NULL, NULL, NULL, AmbushInputInfo, AmbushtDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 224, 4, 3
};


// Ambush

static struct BurnRomInfo ambushjRomDesc[] = {
	{ "ambush.h7",		0x2000, 0xce306563, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "ambush.g7",		0x2000, 0x90291409, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "ambush.f7",		0x2000, 0xd023ca29, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "ambush.e7",		0x2000, 0x6cc2d3ee, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "ambush.n4",		0x2000, 0xecc0dc85, 2 | BRF_GRA },	         //  4 Graphics tiles
	{ "ambush.m4",		0x2000, 0xe86ca98a, 2 | BRF_GRA },	         //  5

	{ "a.bpr",			0x0100, 0x5f27f511, 4 | BRF_GRA },	         //  6 color PROMs

	{ "b.bpr",			0x0100, 0x1b03fd3b, 0 | BRF_OPT },	         //  7 Proms - Not used
	{ "13.bpr",			0x0100, 0x547e970f, 0 | BRF_OPT },	         //  8
	{ "14.bpr",			0x0100, 0x622a8ce7, 0 | BRF_OPT },	         //  9
};

STD_ROM_PICK(ambushj)
STD_ROM_FN(ambushj)

struct BurnDriver BurnDrvAmbushj = {
	"ambushj", "ambush", NULL, NULL, "1983",
	"Ambush (Japan)\0", NULL, "Tecfri (Nippon Amuse license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ambushjRomInfo, ambushjRomName, NULL, NULL, NULL, NULL, AmbushInputInfo, AmbushtDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 224, 4, 3
};


// Ambush (hack?)

static struct BurnRomInfo ambushhRomDesc[] = {
	{ "a1_hack.i7",		0x2000, 0xa7cd149d, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "a2.g7",			0x2000, 0x8328d88a, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "a3.f7",			0x2000, 0x8db57ab5, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "a4.e7",			0x2000, 0x4a34d2a4, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "fa2.n4",			0x2000, 0xe7f134ba, 2 | BRF_GRA },	   	     //  4 Graphics tiles
	{ "fa1.m4",			0x2000, 0xad10969e, 2 | BRF_GRA },	         //  5

	{ "a.bpr",			0x0100, 0x5f27f511, 4 | BRF_GRA },	         //  6 color PROMs

	{ "b.bpr",			0x0100, 0x1b03fd3b, 0 | BRF_OPT },	         //  7 Proms - Not used
	{ "13.bpr",			0x0100, 0x547e970f, 0 | BRF_OPT },	         //  8
	{ "14.bpr",			0x0100, 0x622a8ce7, 0 | BRF_OPT },	         //  9
};

STD_ROM_PICK(ambushh)
STD_ROM_FN(ambushh)

struct BurnDriver BurnDrvAmbushh = {
	"ambushh", "ambush", NULL, NULL, "1983",
	"Ambush (hack?)\0", NULL, "Tecfri", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ambushhRomInfo, ambushhRomName, NULL, NULL, NULL, NULL, AmbushInputInfo, AmbushDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 224, 4, 3
};


// Ambush (Volt Electronics)

static struct BurnRomInfo ambushvRomDesc[] = {
	{ "n1.h7",			0x2000, 0x3c0833b4, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "n2.g7",			0x2000, 0x90291409, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "n3.f7",			0x2000, 0xd023ca29, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "n4.e7",			0x2000, 0x6cc2d3ee, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "f2.n4",			0x2000, 0xecc0dc85, 2 | BRF_GRA },	         //  4 Graphics tiles
	{ "f1.m4",			0x2000, 0xe86ca98a, 2 | BRF_GRA },	         //  5

	{ "a.bpr",			0x0100, 0x5f27f511, 4 | BRF_GRA },	         //  6 color PROMs

	{ "b.bpr",			0x0100, 0x1b03fd3b, 0 | BRF_OPT },	         //  7 Proms - Not used
	{ "13.bpr",			0x0100, 0x547e970f, 0 | BRF_OPT },	         //  8
	{ "14.bpr",			0x0100, 0x622a8ce7, 0 | BRF_OPT },	         //  9
};

STD_ROM_PICK(ambushv)
STD_ROM_FN(ambushv)

struct BurnDriver BurnDrvAmbushv = {
	"ambushv", "ambush", NULL, NULL, "1983",
	"Ambush (Volt Electronics)\0", NULL, "Tecfri (Volt Electronics license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ambushvRomInfo, ambushvRomName, NULL, NULL, NULL, NULL, AmbushInputInfo, AmbushDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 224, 4, 3
};


// Mario Bros. (bootleg on Ambush Hardware, set 1)

static struct BurnRomInfo marioblRomDesc[] = {
	{ "mbjba-8.7h",		0x4000, 0x344c959d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "mbjba-7.7g",		0x2000, 0x06faf308, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mbjba-6.7f",		0x2000, 0x761dd670, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mbjba-5.4n",		0x1000, 0x9bbcf9fb, 2 | BRF_GRA },           //  3 Background Tiles
	{ "mbjba-4.4l",		0x1000, 0x9379e836, 2 | BRF_GRA },           //  4

	{ "mbjba-3.3ns",	0x2000, 0x3981adb2, 3 | BRF_GRA },           //  5 Sprites
	{ "mbjba-2.3ls",	0x2000, 0x7b58c92e, 3 | BRF_GRA },           //  6
	{ "mbjba-1.3l",		0x2000, 0xc772cb8f, 3 | BRF_GRA },           //  7

	{ "n82s147n.15",	0x0200, 0x6a109f4b, 4 | BRF_GRA },           //  8 Color PROM

	{ "n82s147n.13",	0x0200, 0xa334e4f3, 0 | BRF_OPT },           //  9 Other PROM

	{ "82s153.2",		0x00eb, 0x3b6ec269, 0 | BRF_OPT },           //  10 PLDs
	{ "82s153.4",		0x00eb, 0x8e227b3e, 0 | BRF_OPT },           //  11
	{ "82s153.11",		0x00eb, 0x9da5e80d, 0 | BRF_OPT },           //  12
	{ "pal16x4cj.10",	0x0104, 0xd2731879, 0 | BRF_OPT },           //  13
};

STD_ROM_PICK(mariobl)
STD_ROM_FN(mariobl)

struct BurnDriver BurnDrvMariobl = {
	"mariobl", "mario", NULL, NULL, "1983",
	"Mario Bros. (bootleg on Ambush Hardware, set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, marioblRomInfo, marioblRomName, NULL, NULL, NULL, NULL, MarioblInputInfo, MarioblDIPInfo,
	MarioblInit, DrvExit, DrvFrame, MarioblDraw, DrvScan, &BurnRecalc, 0x100,
	256, 224, 4, 3
};


// Mario Bros. (bootleg on Ambush Hardware, set 2)

static struct BurnRomInfo marioblaRomDesc[] = {
	{ "4.7i",			0x4000, 0x9a364905, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "3.7g",			0x2000, 0x5c50209c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.7f",			0x2000, 0x5718fe0e, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "5.4n",			0x4000, 0x903615df, 2 | BRF_GRA },           //  3 Tiles & Sprites
	{ "6.4l",			0x2000, 0x7b58c92e, 2 | BRF_GRA },           //  4
	{ "7.4n",			0x4000, 0x04ef8165, 2 | BRF_GRA },           //  5

	{ "6349-2n.8h",		0x0200, 0x6a109f4b, 4 | BRF_GRA },           //  6 Color PROM

	{ "6349-2n.2m",		0x0200, 0xa334e4f3, 0 | BRF_OPT },           //  7 Proms - Not used
	{ "6349-2n-cpu.5b",	0x0200, 0x7250ad28, 0 | BRF_OPT },           //  8
	{ "82s153.7n",		0x00eb, 0x9da5e80d, 0 | BRF_OPT },           //  9
};

STD_ROM_PICK(mariobla)
STD_ROM_FN(mariobla)

struct BurnDriver BurnDrvMariobla = {
	"mariobla", "mario", NULL, NULL, "1983",
	"Mario Bros. (bootleg on Ambush Hardware, set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, marioblaRomInfo, marioblaRomName, NULL, NULL, NULL, NULL, MarioblInputInfo, MarioblDIPInfo,
	MarioblaInit, DrvExit, DrvFrame, MarioblDraw, DrvScan, &BurnRecalc, 0x100,
	256, 224, 4, 3
};


// Donkey Kong 3 (bootleg on Ambush hardware)

static struct BurnRomInfo dkong3ablRomDesc[] = {
	{ "dk3ba-7.7i",		0x4000, 0xa9263275, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "dk3ba-6.7g",		0x4000, 0x31b8401d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dk3ba-5.7f",		0x1000, 0x07d3fd88, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "dk3ba-3.4l",		0x1000, 0x67ac65d4, 2 | BRF_GRA },           //  3 Background Tiles
	{ "dk3ba-4.4n",		0x1000, 0x84b319d6, 2 | BRF_GRA },           //  4

	{ "dk3ba-1.3l",		0x2000, 0xd4a88e04, 3 | BRF_GRA },           //  5 Sprites
	{ "dk3ba-2.3m",		0x2000, 0xf71185ee, 3 | BRF_GRA },           //  6

	// These are from donkey kong 3
	{ "dkc1-c.1d",		0x0200, 0xdf54befc, 4 | BRF_GRA },           //  7 Color PROMs
	{ "dkc1-c.1c",		0x0200, 0x66a77f40, 4 | BRF_GRA },           //  8
	// Correct ones are undumped
	{ "a.bpr",			0x0100, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           //  9
	{ "b.bpr",			0x0100, 0x00000000, 0 | BRF_NODUMP | BRF_GRA },           // 10
};

STD_ROM_PICK(dkong3abl)
STD_ROM_FN(dkong3abl)

struct BurnDriver BurnDrvDkong3abl = {
	"dkong3abl", "dkong3", NULL, NULL, "1983",
	"Donkey Kong 3 (bootleg on Ambush hardware)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, dkong3ablRomInfo, dkong3ablRomName, NULL, NULL, NULL, NULL, Dkong3ablInputInfo, Dkong3ablDIPInfo,
	Dkong3ablInit, DrvExit, DrvFrame, Dkong3ablDraw, DrvScan, &BurnRecalc, 0x100,
	224, 256, 3, 4
};
