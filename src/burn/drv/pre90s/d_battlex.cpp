// FB Alpha Battle Cross / Dodge Man driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "burn_pal.h"
#include "bitswap.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;

static UINT8 *tmpbitmap;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;
static UINT8 scroll[2];
static UINT8 previous_irq_flip;
static INT32 timer = 0;

static INT32 nExtraCycles;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo BattlexInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Battlex)

static struct BurnInputInfo DodgemanInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Dodgeman)

static struct BurnDIPInfo BattlexDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xac, NULL			},
	{0x10, 0xff, 0xff, 0x10, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0f, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x03, 0x03, "2 Coins 3 Credits"	},
	{0x0f, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x0f, 0x01, 0x04, 0x00, "No"			},
	{0x0f, 0x01, 0x04, 0x04, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0f, 0x01, 0x08, 0x00, "Off"			},
	{0x0f, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x20, 0x20, "Upright"		},
	{0x0f, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0f, 0x01, 0x40, 0x00, "Off"			},
	{0x0f, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    0, "Coin B"		},
	{0x10, 0x01, 0x07, 0x07, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x07, 0x05, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x07, 0x06, "2 Coins 3 Credits"	},
	{0x10, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},
	{0x10, 0x01, 0x07, 0x04, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    8, "Lives"		},
	{0x10, 0x01, 0x18, 0x00, "1"			},
	{0x10, 0x01, 0x18, 0x08, "2"			},
	{0x10, 0x01, 0x18, 0x10, "3"			},
	{0x10, 0x01, 0x18, 0x18, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x60, 0x00, "5000"			},
	{0x10, 0x01, 0x60, 0x20, "10000"		},
	{0x10, 0x01, 0x60, 0x40, "15000"		},
	{0x10, 0x01, 0x60, 0x60, "20000"		},

	{0   , 0xfe, 0   ,    4, "Free Play"		},
	{0x10, 0x01, 0x80, 0x00, "Off"			},
	{0x10, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Battlex)

static struct BurnDIPInfo DodgemanDIPList[]=
{
	{0x11, 0xff, 0xff, 0xac, NULL			},
	{0x12, 0xff, 0xff, 0x10, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x11, 0x01, 0x04, 0x00, "No"			},
	{0x11, 0x01, 0x04, 0x04, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x08, 0x00, "Off"			},
	{0x11, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x20, 0x20, "Upright"		},
	{0x11, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x40, 0x00, "Off"			},
	{0x11, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    0, "Coin B"		},
	{0x12, 0x01, 0x07, 0x07, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    8, "Lives"		},
	{0x12, 0x01, 0x18, 0x00, "1"			},
	{0x12, 0x01, 0x18, 0x08, "2"			},
	{0x12, 0x01, 0x18, 0x10, "3"			},
	{0x12, 0x01, 0x18, 0x18, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x60, 0x00, "5000"			},
	{0x12, 0x01, 0x60, 0x20, "10000"		},
	{0x12, 0x01, 0x60, 0x40, "15000"		},
	{0x12, 0x01, 0x60, 0x60, "20000"		},

	{0   , 0xfe, 0   ,    4, "Free Play"		},
	{0x12, 0x01, 0x80, 0x00, "Off"			},
	{0x12, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Dodgeman)

static void do_pal(INT32 offset, UINT8 data)
{
	INT32 pal = offset / 8;
	INT32 color = offset & 7;

	DrvPalette[offset] = BurnHighCol(pal1bit(data >> 0), pal1bit(data >> 2), pal1bit(data >> 1), 0);

	DrvPalette[0x40 + pal * 0x10 + color] = BurnHighCol(pal1bit(data >> 0), pal1bit(data >> 2), pal1bit(data >> 1), 0);
	DrvPalette[0x40 + pal * 0x10 + color + 8] = BurnHighCol(pal2bit((data >> 0) & 1), pal2bit((data >> 2) & 1), pal2bit((data >> 1) & 1), 0);
}

static UINT8 __fastcall main_read(UINT16 address)
{
	if (address >= 0xe000 && address <= 0xe03f) {
		return DrvPalRAM[address & 0x3f];
	}

	return 0x00;
}

static void __fastcall main_write(UINT16 address, UINT8 data)
{
	if (address >= 0xe000 && address <= 0xe03f) {
		DrvPalRAM[address & 0x3f] = data;
		do_pal(address & 0x3f, data);
	}
}

static void __fastcall battlex_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x10:
			flipscreen = data & 0x80;
		return;

		case 0x22:
		case 0x23:
			AY8910Write(0, ~port & 1, data);
		return;

		case 0x30:
			// starfield (not emulated)
		return;

		case 0x32:
		case 0x33:
			scroll[port & 1] = data;
		return;

		case 0x26:
		case 0x27:
			AY8910Write(1, ~port & 1, data);
		return;
	}
}

static UINT8 __fastcall battlex_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00: {
			UINT8 ret = previous_irq_flip;
			if (previous_irq_flip) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
				previous_irq_flip = 0;
			}
			return (DrvDips[0] & ~0x10) | (ret << 4);
		}

		case 0x01:
			return DrvInputs[0]; // system

		case 0x02:
			return DrvInputs[1]; // inputs

		case 0x03:
			return DrvDips[1];
	}

	return 0;
}

static tilemap_callback( battlex )
{
	UINT16 code = DrvVidRAM[offs * 2] | (((DrvVidRAM[offs * 2 + 1] & 0x01)) << 8);
	UINT8 color = (DrvVidRAM[offs * 2 + 1] & 0x0e) >> 1;

	TILE_SET_INFO(0, code, color, 0);
}

static tilemap_callback( dodgeman )
{
	UINT16 code = DrvVidRAM[offs * 2] | (((DrvVidRAM[offs * 2 + 1] & 0x03)) << 8);
	UINT8 color = (DrvVidRAM[offs * 2 + 1] & 0x0c) >> 2;

	TILE_SET_INFO(0, code, color, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);
	HiscoreReset();

	scroll[0] = scroll[1] = 0;
	flipscreen = 0;
	previous_irq_flip = 0;
	timer = 0;
	nExtraCycles = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x006000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x00c2 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x000200;
	DrvPalRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	tmpbitmap		= Next; Next += 256 * 224;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 select)
{
	INT32 size = (select) ? 0x6000 : 0x3000;
	INT32 Plane0[4] = { STEP4(0, 1) };
	INT32 XOffs0[8] = { STEP8(0, 4) };
	INT32 YOffs0[8] = { STEP8(0, 32) };

	INT32 Plane1[3] = { RGN_FRAC(size, 0, 3), RGN_FRAC(size, 1, 3), RGN_FRAC(size, 2, 3) };
	INT32 XOffs1[16] = { STEP8(7,-1), STEP8(15,-1) };
	INT32 YOffs1[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x8000);

	GfxDecode(0x0400, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x6000);

	GfxDecode(0x0100, 3, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static void DrvGfxExpand(INT32 len, INT32 game_select)
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);

	memcpy (tmp, DrvGfxROM0, 0x4000);
	memset (DrvGfxROM0, 0, 0x10000);

	UINT8 *colormask = tmp;
	UINT8 *gfxdata = tmp + 0x2000;
	UINT8 *dest = DrvGfxROM0;
	INT32 tile_size = len / 32;
	INT32 offset = 0;

	for (INT32 tile = 0; tile < tile_size; tile++)
	{
		for (INT32 line = 0; line < 8; line ++)
		{
			for (INT32 bit = 0; bit < 8; bit ++)
			{
				INT32 color = colormask[(tile << 3) | ((line & 0x6) + (bit > 3 ? 1 : 0))];
				INT32 data = BIT(gfxdata[(tile << 3) | line], bit);

				if (!data)
					color >>= 4;

				color &= 0x0f;

				if (offset&1)
					dest[offset >> 1] |= color;
				else
					dest[offset >> 1] = color<<4;

				offset++;
			}
		}
	}

	BurnFree(tmp);
}

static void init_stars() // hack
{
	UINT8 *dst = tmpbitmap;

	for (INT32 y = 0; y < 224; y++)
	{
		for (INT32 x = 0; x < 256; x++)
		{
			UINT16 z = rand() & 0x1ff;

			if (z==0xf6) dst[x] = 0xc1;
		}

		dst += 256;
	}
}

static INT32 DrvInit(INT32 select)
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x3000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x5000,  5, 1)) return 1;

		if (select) {
			if (BurnLoadRom(DrvGfxROM1 + 0x0000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x2000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x4000,  8, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvGfxROM1 + 0x0000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x1000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x2000,  8, 1)) return 1;
		}

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000, 10, 1)) return 1;

		DrvGfxExpand(((select) ? 0x8000 : 0x4000), select);
		DrvGfxDecode(select);
		if (select == 0) init_stars();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0x9000, 0x91ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0xa000, 0xa3ff, MAP_RAM);
	//ZetMapMemory(DrvPalRAM,		0xe000, 0xe0ff, MAP_RAM); // in handler
	ZetSetReadHandler(main_read);
	ZetSetWriteHandler(main_write);
	ZetSetOutHandler(battlex_write_port);
	ZetSetInHandler(battlex_read_port);
	ZetClose();

	AY8910Init(0, 1250000, 0);
	AY8910Init(1, 1250000, 1);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 2500000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, select ? dodgeman_map_callback : battlex_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x10000, 64, 7);
	GenericTilemapSetTransparent(0,0);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x40; i++) {
		do_pal(i, DrvPalRAM[i]);
	}

	DrvPalette[0xc0] = BurnHighCol(0xff, 0xff, 0xff, 0);
	DrvPalette[0xc1] = BurnHighCol(0x2c, 0x2c, 0x2c, 0); // stars!
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x200; i+=4)
	{
		INT32 sx    =(DrvSprRAM[i] & 0x7f) * 2 - (DrvSprRAM[i] & 0x80) * 2;
		INT32 sy    = DrvSprRAM[i+3];
		INT32 code  = DrvSprRAM[i+2] & 0xff;
		INT32 color = DrvSprRAM[i+1] & 0x07;
		INT32 flipy = DrvSprRAM[i+1] & 0x80;
		INT32 flipx = DrvSprRAM[i+1] & 0x40;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		Draw16x16MaskTile(pTransDraw, code, sx, sy-16, flipx, flipy, color, 3, 0, 0, DrvGfxROM1);
	}
}

static void draw_stars()
{
	UINT8 *src = tmpbitmap;
	UINT16 *dst = pTransDraw;

	for (INT32 y = 0; y < 224; y++)
	{
		for (INT32 x = 0; x < 256; x++)
		{
			dst[x] = src[x];
		}

		dst += 256;
		src += 256;
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	//}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_stars();

	if (flipscreen)
		GenericTilemapSetScrollX(0, scroll[0] | (scroll[1] << 3));
	else
		GenericTilemapSetScrollX(0, scroll[0] | (scroll[1] << 8));

	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 2500000 / 60 };
	INT32 nCyclesDone[1]  = { nExtraCycles };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Zet);

		timer +=       26042; // 6.666667/256 = 0.026042

		if (timer >= 1000000)
		{
			timer -= 1000000;
			previous_irq_flip = 1;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		}
	}

	ZetClose();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

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
		SCAN_VAR(scroll);
		SCAN_VAR(previous_irq_flip);
		SCAN_VAR(timer);
		SCAN_VAR(nExtraCycles);
	}

	return 0;
}

// Battle Cross

static struct BurnRomInfo battlexRomDesc[] = {
	{ "p-rom1.6",	0x1000, 0xb00ae551, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "p-rom2.5",	0x1000, 0xe765bb11, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p-rom3.4",	0x1000, 0x21675a91, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p-rom4.3",	0x1000, 0xfff1ccc4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "p-rom5.2",	0x1000, 0xceb63d38, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "p-rom6.1",	0x1000, 0x6923f601, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "1a_f.6f",	0x1000, 0x2b69287a, 2 | BRF_GRA },           //  6 Sprites
	{ "1a_h.6h",	0x1000, 0x9f4c3bdd, 2 | BRF_GRA },           //  7
	{ "1a_j.6j",	0x1000, 0xc1345b05, 2 | BRF_GRA },           //  8

	{ "1a_d.6d",	0x1000, 0x750a46ef, 3 | BRF_GRA },           //  9 Characters
	{ "1a_e.6e",	0x1000, 0x126842b7, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(battlex)
STD_ROM_FN(battlex)

static INT32 battlexInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvBattlex = {
	"battlex", NULL, NULL, NULL, "1982",
	"Battle Cross\0", NULL, "Omori Electric Co., Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, battlexRomInfo, battlexRomName, NULL, NULL, NULL, NULL, BattlexInputInfo, BattlexDIPInfo,
	battlexInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x42,
	256, 224, 4, 3
};


// Dodge Man

static struct BurnRomInfo dodgemanRomDesc[] = {
	{ "dg0.7f",	0x1000, 0x1219b5db, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "dg1.5f",	0x1000, 0xfff9a086, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dg2.4f",	0x1000, 0x2ca9ac99, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dg3.3f",	0x1000, 0x55a51c0e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "dg4.2f",	0x1000, 0x14169361, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "dg5.1f",	0x1000, 0x8f83ae2f, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "f.6f",	0x2000, 0xdfaaf4c8, 2 | BRF_GRA },           //  6 Sprites
	{ "h.6h",	0x2000, 0xe2525ffe, 2 | BRF_GRA },           //  7
	{ "j.6j",	0x2000, 0x2731ee46, 2 | BRF_GRA },           //  8

	{ "d.6d",	0x2000, 0x451c1c3a, 3 | BRF_GRA },           //  9 Characters
	{ "e.6e",	0x2000, 0xc9a515df, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(dodgeman)
STD_ROM_FN(dodgeman)

static INT32 dodgemanInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvDodgeman = {
	"dodgeman", NULL, NULL, NULL, "1983",
	"Dodge Man\0", NULL, "Omori Electric Co., Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, dodgemanRomInfo, dodgemanRomName, NULL, NULL, NULL, NULL, DodgemanInputInfo, DodgemanDIPInfo,
	dodgemanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};
