// FB Alpha Bionic Commando driver module
// Based on MAME driver by Steven Frew, Phil Stroffolino, and Paul Leaman

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2151.h"

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;
static UINT8 DrvDips[2];

static UINT8 *Mem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *Drv68KRAM0;
static UINT8 *DrvTextRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvPalRAM;
static UINT8 *Drv68KRAM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSprBuf;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *soundlatch;

static INT32 flipscreen;

static INT32 fg_scroll_x; 
static INT32 fg_scroll_y;
static INT32 bg_scroll_x;
static INT32 bg_scroll_y;

static INT32 fg_enable;
static INT32 bg_enable;

static struct BurnInputInfo DrvInputList[] = {
	{"Coin 1"       , BIT_DIGITAL  , DrvJoy1 + 6,	"p1 coin"  },
	{"Coin 2"       , BIT_DIGITAL  , DrvJoy1 + 7,	"p2 coin"  },

	{"P1 Start"  ,    BIT_DIGITAL  , DrvJoy1 + 5,	"p1 start" },
	{"P1 Left"      , BIT_DIGITAL  , DrvJoy2 + 3, 	"p1 left"  },
	{"P1 Right"     , BIT_DIGITAL  , DrvJoy2 + 2, 	"p1 right" },
	{"P1 Down",	  BIT_DIGITAL  , DrvJoy2 + 4,   "p1 down", },
	{"P1 Up",	  BIT_DIGITAL  , DrvJoy2 + 5,   "p1 up",   },
	{"P1 Button 1"  , BIT_DIGITAL  , DrvJoy2 + 1,	"p1 fire 1"},
	{"P1 Button 2"  , BIT_DIGITAL  , DrvJoy2 + 0,	"p1 fire 2"},

	{"P2 Start"  ,    BIT_DIGITAL  , DrvJoy1 + 4,	"p2 start" },
	{"P2 Left"      , BIT_DIGITAL  , DrvJoy3 + 3, 	"p2 left"  },
	{"P2 Right"     , BIT_DIGITAL  , DrvJoy3 + 2, 	"p2 right" },
	{"P2 Down",	  BIT_DIGITAL,   DrvJoy3 + 4,   "p2 down", },
	{"P2 Up",	  BIT_DIGITAL,   DrvJoy3 + 5,   "p2 up",   },
	{"P2 Button 1"  , BIT_DIGITAL  , DrvJoy3 + 1,	"p2 fire 1"},
	{"P2 Button 2"  , BIT_DIGITAL  , DrvJoy3 + 0,	"p2 fire 2"},

	{"Reset",	  BIT_DIGITAL  , &DrvReset,	"reset"    },
	{"Dip 1",	  BIT_DIPSWITCH, DrvDips + 0,	"dip"	   },
	{"Dip 2",	  BIT_DIPSWITCH, DrvDips + 1,	"dip"	   },
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   , 8   , "Coin_A"		},
	{0x11, 0x01, 0x07, 0x00, "4 Coins 1 Credit "	},
	{0x11, 0x01, 0x07, 0x01, "3 Coins 1 Credit "	},
	{0x11, 0x01, 0x07, 0x02, "2 Coins 1 Credit "	},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credit "	},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   , 8   , "Coin_B"		},
	{0x11, 0x01, 0x38, 0x00, "4 Coins 1 Credit "	},
	{0x11, 0x01, 0x38, 0x08, "3 Coins 1 Credit "	},
	{0x11, 0x01, 0x38, 0x10, "2 Coins 1 Credit "	},
	{0x11, 0x01, 0x38, 0x38, "1 Coin  1 Credit "	},
	{0x11, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x38, 0x18, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   , 2   , "Service Mode"		},
	{0x11, 0x01, 0x40, 0x40, "Off"			},
	{0x11, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   , 2   , "Flip Screen"		},
	{0x11, 0x01, 0x80, 0x80, "Off"			},
	{0x11, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   , 4   , "Lives"		},
	{0x12, 0x01, 0x03, 0x03, "3"			},
	{0x12, 0x01, 0x03, 0x02, "4"			},
	{0x12, 0x01, 0x03, 0x01, "5"			},
	{0x12, 0x01, 0x03, 0x00, "7"			},

	{0   , 0xfe, 0   , 2   , "Cabinet"		},
	{0x12, 0x01, 0x04, 0x04, "Upright"		},
	{0x12, 0x01, 0x04, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   , 4   , "Bonus_Life"		},
	{0x12, 0x01, 0x18, 0x18, "20K, 40K, every 60K"	},
	{0x12, 0x01, 0x18, 0x10, "30K, 50K, every 70K"	},
	{0x12, 0x01, 0x18, 0x08, "20K and 60K only"	},
	{0x12, 0x01, 0x18, 0x00, "30K and 70K only"	},

	{0   , 0xfe, 0   , 4   , "Difficulty"		},
	{0x12, 0x01, 0x60, 0x40, "Easy"			},
	{0x12, 0x01, 0x60, 0x60, "Medium"		},
	{0x12, 0x01, 0x60, 0x20, "Hard"			},
	{0x12, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   , 2   , "Freeze"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Drv)

static void bionicc_palette_write(INT32 offset)
{
	offset >>= 1;

	INT32 r, g, b, bright;
	INT32 data =  *((UINT16*)(DrvPalRAM + (offset << 1)));

	bright = (data&0x0f);

	r = ((data>>12)&0x0f) * 0x11;
	g = ((data>>8 )&0x0f) * 0x11;
	b = ((data>>4 )&0x0f) * 0x11;

	if ((bright & 0x08) == 0)
	{
		r = r * (0x07 + bright) / 0x0e;
		g = g * (0x07 + bright) / 0x0e;
		b = b * (0x07 + bright) / 0x0e;
	}

	DrvPalette[offset] = BurnHighCol(r, g, b, 0);
}

void __fastcall bionicc_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffff800) == 0xff8000) {
		address &= 0x7ff;
		DrvPalRAM[address ^ 1] = data;
		bionicc_palette_write(address);
		return;
	}

	switch (address)
	{
		case 0xfe4000:
		case 0xfe4001:
			flipscreen = data & 0x01;
			fg_enable  = data & 0x10;
			bg_enable  = data & 0x20;
		return;
	}
}

void __fastcall bionicc_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffff800) == 0xff8000) {
		address &= 0x7ff;

		*((UINT16*)(DrvPalRAM + address)) = data;

		bionicc_palette_write(address);

		return;
	}

	switch (address)
	{
		case 0xfe8010:
			fg_scroll_x = data & 0x3ff;
		return;

		case 0xfe8012:
			fg_scroll_y = data & 0x3ff;
		return;

		case 0xfe8014:
			bg_scroll_x = data & 0x1ff;
		return;

		case 0xfe8016:
			bg_scroll_y = data & 0x1ff;
		return;

		case 0xfe801a:
			UINT16 *inp = (UINT16*)(Drv68KRAM1 + 0x3ffa);
			inp[0] = (DrvInputs[0] >> 4) ^ 0x0f;
			inp[1] = DrvInputs[2] ^ 0xff;
			inp[2] = DrvInputs[1] ^ 0xff;
		return;
	}
}

UINT8 __fastcall bionicc_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xfe4000:
			return DrvInputs[0];

		case 0xfe4001:
			return 0xff; 

		case 0xfe4002:
			return DrvDips[0];

		case 0xfe4003:
			return DrvDips[1];
	}

	return 0;
}

UINT16 __fastcall bionicc_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xfe4000:
			return 0x00ff | (DrvInputs[0] << 8);

		case 0xfe4002:
			return DrvDips[0] | (DrvDips[1] << 8);
	}

	return 0;
}

UINT8 __fastcall bionicc_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8001:
			return BurnYM2151ReadStatus();

		case 0xa000:
			return *soundlatch;
	}

	return 0;
}

void __fastcall bionicc_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
			BurnYM2151SelectRegister(data);
			return;

		case 0x8001:
			BurnYM2151WriteRegister(data);
			return;
	}
}

static tilemap_callback( background )
{
	UINT16 *ram = (UINT16*)DrvVidRAM1;

	INT32 attr  = ram[(offs * 2) + 1];
	INT32 code  = (ram[offs * 2] & 0xff) | ((attr & 0x07) * 256);

	TILE_SET_INFO(0, code, attr >> 3, TILE_FLIPXY(attr >> 6));
}

static tilemap_callback( foreground )
{
	UINT16 *ram = (UINT16*)DrvVidRAM0;

	INT32 attr  = ram[(offs * 2) + 1];
	INT32 code  = (ram[offs * 2] & 0xff) | ((attr & 0x07) * 256);

	INT32 flags = TILE_FLIPXY(attr >> 6);
	INT32 group = (attr >> 5) & 1;

	if (attr >= 0xc0) {
		flags ^= 3;
		group = 2;
	}

	TILE_SET_INFO(1, code, attr >> 3, flags | TILE_GROUP(group));
}

static tilemap_callback( text )
{
	UINT16 *ram = (UINT16*)DrvTextRAM;

	INT32 attr  = ram[offs + 0x400];
	INT32 code  = (ram[offs] & 0xff) | ((attr & 0xc0) << 2);

	TILE_SET_INFO(2, code, attr, 0);
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KROM	= Next; Next += 0x0040000;
	DrvZ80ROM	= Next; Next += 0x0008000;

	DrvGfxROM0	= Next; Next += 0x0020000;
	DrvGfxROM1	= Next; Next += 0x0020000;
	DrvGfxROM2	= Next; Next += 0x0080000;
	DrvGfxROM3	= Next; Next += 0x0080000;

	DrvPalette	= (UINT32*)Next; Next += 0x00400 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM0	= Next; Next += 0x0004000;
	Drv68KRAM1	= Next; Next += 0x0004000;
	DrvPalRAM	= Next; Next += 0x0000800;
	DrvTextRAM	= Next; Next += 0x0001000;
	DrvVidRAM0	= Next; Next += 0x0004000;
	DrvVidRAM1	= Next; Next += 0x0004000;

	DrvSprBuf	= Next; Next += 0x0000500;

	DrvZ80RAM	= Next; Next += 0x0000800;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnYM2151Reset();

	HiscoreReset();

	fg_scroll_x = 0;
	fg_scroll_y = 0;
	bg_scroll_x = 0;
	bg_scroll_y = 0;

	soundlatch = Drv68KRAM1 + 0x3ff8;
	flipscreen = 0;

	fg_enable = 0;
	bg_enable = 0;

	return 0;
}

static INT32 DrvGfxDecode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	static INT32 CharPlanes[2]    = { 0x000004, 0x000000 };
	static INT32 Tile0Planes[4]   = { 0x040004, 0x040000, 0x000004, 0x000000 };
	static INT32 Tile1Planes[4]   = { 0x100004, 0x100000, 0x000004, 0x000000 };
	static INT32 SpriPlanes[4]    = { 0x180000, 0x100000, 0x080000, 0x000000 };
	static INT32 CharXOffsets[16] = { STEP4(0,1), STEP4(8,1), STEP4(256,1), STEP4(256+8,1) };
	static INT32 SpriXOffsets[16] = { STEP8(0,1), STEP8(128,1) };
	static INT32 CharYOffsets[16] = { STEP16(0,16) };
	static INT32 SpriYOffsets[16] = { STEP16(0,8) };

	memcpy (tmp, DrvGfxROM0, 0x08000);

	GfxDecode(0x00400, 2,  8,  8, CharPlanes,  CharXOffsets,  CharYOffsets,  0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x10000);

	GfxDecode(0x00800, 4,  8,  8, Tile0Planes,  CharXOffsets, CharYOffsets,  0x080, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x40000);

	GfxDecode(0x00800, 4, 16, 16, Tile1Planes,  CharXOffsets, CharYOffsets,  0x200, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x40000);

	GfxDecode(0x00800, 4, 16, 16, SpriPlanes,  SpriXOffsets,  SpriYOffsets,  0x100, tmp, DrvGfxROM3);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	Mem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x00001, 0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x00000, 1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x20001, 2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x20000, 3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM, 	     4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0, 	     5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000, 6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x8000, 7, 1)) return 1;

		for (INT32 i = 0; i < 8; i++) {
			if (BurnLoadRom(DrvGfxROM2 + i * 0x8000, i +  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3 + i * 0x8000, i + 16, 1)) return 1;
		}

		if (DrvGfxDecode()) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM0,	0xfe0000, 0xfe3fff, MAP_RAM);
	SekMapMemory(DrvTextRAM,	0xfec000, 0xfecfff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,	0xff0000, 0xff3fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,	0xff4000, 0xff7fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0xff8000, 0xff87ff, MAP_ROM);
	SekMapMemory(Drv68KRAM1,	0xffc000, 0xffffff, MAP_RAM); 
	SekSetReadByteHandler(0,	bionicc_read_byte);
	SekSetReadWordHandler(0,	bionicc_read_word);
	SekSetWriteByteHandler(0,	bionicc_write_byte);
	SekSetWriteWordHandler(0,	bionicc_write_word);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80ROM);
	ZetMapArea(0xc000, 0xc7ff, 0, DrvZ80RAM);
	ZetMapArea(0xc000, 0xc7ff, 1, DrvZ80RAM);
	ZetMapArea(0xc000, 0xc7ff, 2, DrvZ80RAM);
	ZetSetWriteHandler(bionicc_sound_write);
	ZetSetReadHandler(bionicc_sound_read);
	ZetClose();

	BurnYM2151Init(3579545);
	BurnYM2151SetAllRoutes(0.60, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback,  8,  8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 16, 16, 64, 64);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, text_map_callback,        8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 4,  8,  8, 0x20000, 0x000, 0x03);
	GenericTilemapSetGfx(1, DrvGfxROM2, 4, 16, 16, 0x80000, 0x100, 0x03);
	GenericTilemapSetGfx(2, DrvGfxROM0, 2,  8,  8, 0x20000, 0x300, 0x3f);
	GenericTilemapSetTransparent(0, 0x0f);
	GenericTilemapSetTransparent(1, 0x0f);
	GenericTilemapSetTransparent(2, 0x03);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvbInit()
{
	Mem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x20001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x20000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM, 	       4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0, 	       5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x18000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x28000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x30000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x38000, 15, 1)) return 1;
		
		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x10000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x20000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x30000, 19, 1)) return 1;
		
		if (DrvGfxDecode()) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM0,	0xfe0000, 0xfe3fff, MAP_RAM);
	SekMapMemory(DrvTextRAM,	0xfec000, 0xfecfff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,	0xff0000, 0xff3fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,	0xff4000, 0xff7fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0xff8000, 0xff87ff, MAP_ROM);
	SekMapMemory(Drv68KRAM1,	0xffc000, 0xffffff, MAP_RAM); 
	SekSetReadByteHandler(0,	bionicc_read_byte);
	SekSetReadWordHandler(0,	bionicc_read_word);
	SekSetWriteByteHandler(0,	bionicc_write_byte);
	SekSetWriteWordHandler(0,	bionicc_write_word);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80ROM);
	ZetMapArea(0xc000, 0xc7ff, 0, DrvZ80RAM);
	ZetMapArea(0xc000, 0xc7ff, 1, DrvZ80RAM);
	ZetMapArea(0xc000, 0xc7ff, 2, DrvZ80RAM);
	ZetSetWriteHandler(bionicc_sound_write);
	ZetSetReadHandler(bionicc_sound_read);
	ZetClose();

	BurnYM2151Init(3579545);
	BurnYM2151SetAllRoutes(0.60, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback,  8,  8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 16, 16, 64, 64);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, text_map_callback,        8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 4,  8,  8, 0x20000, 0x000, 0x03);
	GenericTilemapSetGfx(1, DrvGfxROM2, 4, 16, 16, 0x80000, 0x100, 0x03);
	GenericTilemapSetGfx(2, DrvGfxROM0, 2,  8,  8, 0x20000, 0x300, 0x3f);
	GenericTilemapSetTransparent(0, 0x0f);
	GenericTilemapSetTransparent(1, 0x0f);
	GenericTilemapSetTransparent(2, 0x03);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	BurnYM2151Exit();
	SekExit();
	ZetExit();

	BurnFree (Mem);

	GenericTilesExit();

	return 0;
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprBuf;

	for (INT32 offs = (0x500-8)/2; offs >= 0; offs -= 4)
	{
		INT32 code = ram[offs] & 0x7ff;

		if (code != 0x7ff)
		{
			INT32 attr = ram[offs+1];
			INT32 color = (attr & 0x3c) >> 2;
			INT32 flipx = attr & 0x02;
			INT32 flipy = 0;
			INT32 sx = (INT16)ram[offs+3];
			INT32 sy = (INT16)ram[offs+2];
			if (sy > 496) sy -= 512;

			if (sx < -15 || sx > 255 || sy < 1 || sy > 239) continue;

			if (flipscreen) {
				flipx ^= 2;
				flipy = 1;
				sx = (256 - 16) - sx;
				sy = (256 - 16) - sy;
			}

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 15, 0x200, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 15, 0x200, DrvGfxROM3);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 4, 15, 0x200, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 4, 15, 0x200, DrvGfxROM3);
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i+=2) {
			bionicc_palette_write(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? (TMAP_FLIPY | TMAP_FLIPX) : 0);

	GenericTilemapSetScrollX(0, bg_scroll_x);
	GenericTilemapSetScrollY(0, bg_scroll_y);
	GenericTilemapSetScrollX(1, fg_scroll_x);
	GenericTilemapSetScrollY(1, fg_scroll_y);

	GenericTilemapSetEnable(0, bg_enable);
	GenericTilemapSetEnable(1, fg_enable);

	GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(2));
	GenericTilemapDraw(0, pTransDraw, 0);
	GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(0));

	draw_sprites();

	GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(1));
	GenericTilemapDraw(2, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = DrvInputs[1] = DrvInputs[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= DrvJoy1[i] << i;
			DrvInputs[1] ^= DrvJoy2[i] << i;
			DrvInputs[2] ^= DrvJoy3[i] << i;
		}
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 8;
	INT32 nTotalCycles[2] = { 12000000 / 60, 3579545 / 60 };

	ZetNewFrame();

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nTotalCycles[0] -= SekRun(nTotalCycles[0] / (nInterleave - i));
		if (i != (nInterleave - 1)) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		nTotalCycles[1] -= ZetRun(nTotalCycles[1] / (nInterleave - i));
		if ((i & 1) == 1) ZetNmi();

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		BurnYM2151Render(pSoundBuf, nSegmentLength);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	// Lag sprites 1 frame
 	memcpy (DrvSprBuf, Drv68KRAM0 + 0x800, 0x500);

	return 0;
}


static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029682;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		BurnYM2151Scan(nAction);

		SCAN_VAR(fg_scroll_x);
		SCAN_VAR(fg_scroll_y);
		SCAN_VAR(bg_scroll_x);
		SCAN_VAR(bg_scroll_y);
		SCAN_VAR(fg_enable);
		SCAN_VAR(bg_enable);
		SCAN_VAR(flipscreen);
	}

	return 0;
}

// Bionic Commando (Euro)

static struct BurnRomInfo bioniccRomDesc[] = {
	{ "tse_02.1a",		0x10000, 0xe4aeefaa, 1 | BRF_PRG | BRF_ESS },	//  0 68k Code
	{ "tse_04.1b",		0x10000, 0xd0c8ec75, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "tse_03.2a",		0x10000, 0xb2ac0a45, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "tse_05.2b",		0x10000, 0xa79cb406, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "ts_01b.4e",		0x08000, 0xa9a6cafa, 2 | BRF_PRG | BRF_ESS },	//  4 Z80 Code

	{ "tsu_08.8l",		0x08000, 0x9bf0b7a2, 3 | BRF_GRA },				//  5 Characters

	{ "tsu_07.5l",		0x08000, 0x9469efa4, 4 | BRF_GRA },				//  6 Background Tiles
	{ "tsu_06.4l",		0x08000, 0x40bf0eb4, 4 | BRF_GRA },				//  7

	{ "ts_12.17f",		0x08000, 0xe4b4619e, 5 | BRF_GRA },				//  8 Foreground Tiles
	{ "ts_11.15f",		0x08000, 0xab30237a, 5 | BRF_GRA },				//  9
	{ "ts_17.17g", 		0x08000, 0xdeb657e4, 5 | BRF_GRA },				// 10
	{ "ts_16.15g",		0x08000, 0xd363b5f9, 5 | BRF_GRA },				// 11 
	{ "ts_13.18f",		0x08000, 0xa8f5a004, 5 | BRF_GRA },				// 12
	{ "ts_18.18g",		0x08000, 0x3b36948c, 5 | BRF_GRA },				// 13
	{ "ts_23.18j",		0x08000, 0xbbfbe58a, 5 | BRF_GRA },				// 14
	{ "ts_24.18k",		0x08000, 0xf156e564, 5 | BRF_GRA },				// 15

	{ "tse_10.13f",		0x08000, 0xd28eeacc, 6 | BRF_GRA },				// 16 Sprites
	{ "tsu_09.11f",		0x08000, 0x6a049292, 6 | BRF_GRA },				// 17
	{ "tse_15.13g",		0x08000, 0x9b5593c0, 6 | BRF_GRA },				// 18
	{ "tsu_14.11g",		0x08000, 0x46b2ad83, 6 | BRF_GRA },				// 19
	{ "tse_20.13j",		0x08000, 0xb03db778, 6 | BRF_GRA },				// 20
	{ "tsu_19.11j",		0x08000, 0xb5c82722, 6 | BRF_GRA },				// 21
	{ "tse_22.17j",		0x08000, 0xd4dedeb3, 6 | BRF_GRA },				// 22
	{ "tsu_21.15j",		0x08000, 0x98777006, 6 | BRF_GRA },				// 23

	{ "63s141.18f",		0x00100, 0xb58d0023, 0 | BRF_OPT },				// 24 Priority (not used)
	
	{ "c8751h-88",          0x01000, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },
};

STD_ROM_PICK(bionicc)
STD_ROM_FN(bionicc)

struct BurnDriver BurnDrvbionicc = {
	"bionicc", NULL, NULL, NULL, "1987",
	"Bionic Commando (Euro)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM, 0,
	NULL, bioniccRomInfo, bioniccRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Bionic Commando (US set 1)

static struct BurnRomInfo bionicc1RomDesc[] = {
	{ "tsu_02b.1a",		0x10000, 0xcf965a0a, 1 | BRF_PRG | BRF_ESS },	//  0 68k Code
	{ "tsu_04b.1b",		0x10000, 0xc9884bfb, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "tsu_03b.2a",		0x10000, 0x4e157ae2, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "tsu_05b.2b",		0x10000, 0xe66ca0f9, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "ts_01b.4e",		0x08000, 0xa9a6cafa, 2 | BRF_PRG | BRF_ESS },	//  4 Z80 Code

	{ "tsu_08.8l",		0x08000, 0x9bf0b7a2, 3 | BRF_GRA },				//  5 Characters

	{ "tsu_07.5l",		0x08000, 0x9469efa4, 4 | BRF_GRA },				//  6 Background Tiles
	{ "tsu_06.4l",		0x08000, 0x40bf0eb4, 4 | BRF_GRA },				//  7

	{ "ts_12.17f",		0x08000, 0xe4b4619e, 5 | BRF_GRA },				//  8 Foreground Tiles
	{ "ts_11.15f",		0x08000, 0xab30237a, 5 | BRF_GRA },				//  9
	{ "ts_17.17g", 		0x08000, 0xdeb657e4, 5 | BRF_GRA },				// 10
	{ "ts_16.15g",		0x08000, 0xd363b5f9, 5 | BRF_GRA },				// 11 
	{ "ts_13.18f",		0x08000, 0xa8f5a004, 5 | BRF_GRA },				// 12
	{ "ts_18.18g",		0x08000, 0x3b36948c, 5 | BRF_GRA },				// 13
	{ "ts_23.18j",		0x08000, 0xbbfbe58a, 5 | BRF_GRA },				// 14
	{ "ts_24.18k",		0x08000, 0xf156e564, 5 | BRF_GRA },				// 15

	{ "tsu_10.13f",		0x08000, 0xf1180d02, 6 | BRF_GRA },				// 16 Sprites
	{ "tsu_09.11f",		0x08000, 0x6a049292, 6 | BRF_GRA },				// 17
	{ "tsu_15.13g",		0x08000, 0xea912701, 6 | BRF_GRA },				// 18
	{ "tsu_14.11g",		0x08000, 0x46b2ad83, 6 | BRF_GRA },				// 19
	{ "tsu_20.13j",		0x08000, 0x17857ad2, 6 | BRF_GRA },				// 20
	{ "tsu_19.11j",		0x08000, 0xb5c82722, 6 | BRF_GRA },				// 21
	{ "tsu_22.17j",		0x08000, 0x5ee1ae6a, 6 | BRF_GRA },				// 22
	{ "tsu_21.15j",		0x08000, 0x98777006, 6 | BRF_GRA },				// 23

	{ "63s141.18f",		0x00100, 0xb58d0023, 0 | BRF_OPT },				// 24 Priority (not used)
	
	{ "c8751h-88",      0x01000, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },
};

STD_ROM_PICK(bionicc1)
STD_ROM_FN(bionicc1)

struct BurnDriver BurnDrvbionicc1 = {
	"bionicc1", "bionicc", NULL, NULL, "1987",
	"Bionic Commando (US set 1)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM, 0,
	NULL, bionicc1RomInfo, bionicc1RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Bionic Commando (US set 2)

static struct BurnRomInfo bionicc2RomDesc[] = {
	{ "tsu_02.1a",		0x10000, 0xf2528f08, 1 | BRF_PRG | BRF_ESS },	//  0 68k Code
	{ "tsu_04.1b",		0x10000, 0x38b1c7e4, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "tsu_03.2a",		0x10000, 0x72c3b76f, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "tsu_05.2b",		0x10000, 0x70621f83, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "ts_01b.4e",		0x08000, 0xa9a6cafa, 2 | BRF_PRG | BRF_ESS },	//  4 Z80 Code

	{ "tsu_08.8l",		0x08000, 0x9bf0b7a2, 3 | BRF_GRA },				//  5 Characters

	{ "tsu_07.5l",		0x08000, 0x9469efa4, 4 | BRF_GRA },				//  6 Background Tiles
	{ "tsu_06.4l",		0x08000, 0x40bf0eb4, 4 | BRF_GRA },				//  7

	{ "ts_12.17f",		0x08000, 0xe4b4619e, 5 | BRF_GRA },				//  8 Foreground Tiles
	{ "ts_11.15f",		0x08000, 0xab30237a, 5 | BRF_GRA },				//  9
	{ "ts_17.17g", 		0x08000, 0xdeb657e4, 5 | BRF_GRA },				// 10
	{ "ts_16.15g",		0x08000, 0xd363b5f9, 5 | BRF_GRA },				// 11 
	{ "ts_13.18f",		0x08000, 0xa8f5a004, 5 | BRF_GRA },				// 12
	{ "ts_18.18g",		0x08000, 0x3b36948c, 5 | BRF_GRA },				// 13
	{ "ts_23.18j",		0x08000, 0xbbfbe58a, 5 | BRF_GRA },				// 14
	{ "ts_24.18k",		0x08000, 0xf156e564, 5 | BRF_GRA },				// 15

	{ "tsu_10.13f",		0x08000, 0xf1180d02, 6 | BRF_GRA },				// 16 Sprites
	{ "tsu_09.11f",		0x08000, 0x6a049292, 6 | BRF_GRA },				// 17
	{ "tsu_15.13g",		0x08000, 0xea912701, 6 | BRF_GRA },				// 18
	{ "tsu_14.11g",		0x08000, 0x46b2ad83, 6 | BRF_GRA },				// 19
	{ "tsu_20.13j",		0x08000, 0x17857ad2, 6 | BRF_GRA },				// 20
	{ "tsu_19.11j",		0x08000, 0xb5c82722, 6 | BRF_GRA },				// 21
	{ "tsu_22.17j",		0x08000, 0x5ee1ae6a, 6 | BRF_GRA },				// 22
	{ "tsu_21.15j",		0x08000, 0x98777006, 6 | BRF_GRA },				// 23

	{ "63s141.18f",		0x00100, 0xb58d0023, 0 | BRF_OPT },				// 24 Priority (not used)
	
	{ "c8751h-88",      0x01000, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },
};

STD_ROM_PICK(bionicc2)
STD_ROM_FN(bionicc2)

struct BurnDriver BurnDrvbionicc2 = {
	"bionicc2", "bionicc", NULL, NULL, "1987",
	"Bionic Commando (US set 2)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM, 0,
	NULL, bionicc2RomInfo, bionicc2RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Top Secret (Japan)

static struct BurnRomInfo topsecrtRomDesc[] = {
	{ "ts_02.1a",		0x10000, 0xb2fe1ddb, 1 | BRF_PRG | BRF_ESS },	//  0 68k Code
	{ "ts_04.1b",		0x10000, 0x427a003d, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "ts_03.2a",		0x10000, 0x27f04bb6, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "ts_05.2b",		0x10000, 0xc01547b1, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "ts_01.4e",		0x08000, 0x8ea07917, 2 | BRF_PRG | BRF_ESS },	//  4 Z80 Code

	{ "ts_08.8l",		0x08000, 0x96ad379e, 3 | BRF_GRA },				//  5 Characters

	{ "ts_07.5l",		0x08000, 0x25cdf8b2, 4 | BRF_GRA },				//  6 Background Tiles
	{ "ts_06.4l",		0x08000, 0x314fb12d, 4 | BRF_GRA },				//  7

	{ "ts_12.17f",		0x08000, 0xe4b4619e, 5 | BRF_GRA },				//  8 Foreground Tiles
	{ "ts_11.15f",		0x08000, 0xab30237a, 5 | BRF_GRA },				//  9
	{ "ts_17.17g", 		0x08000, 0xdeb657e4, 5 | BRF_GRA },				// 10
	{ "ts_16.15g",		0x08000, 0xd363b5f9, 5 | BRF_GRA },				// 11 
	{ "ts_13.18f",		0x08000, 0xa8f5a004, 5 | BRF_GRA },				// 12
	{ "ts_18.18g",		0x08000, 0x3b36948c, 5 | BRF_GRA },				// 13
	{ "ts_23.18j",		0x08000, 0xbbfbe58a, 5 | BRF_GRA },				// 14
	{ "ts_24.18k",		0x08000, 0xf156e564, 5 | BRF_GRA },				// 15

	{ "ts_10.13f",		0x08000, 0xc3587d05, 6 | BRF_GRA },				// 16 Sprites
	{ "ts_09.11f",		0x08000, 0x6b63eef2, 6 | BRF_GRA },				// 17
	{ "ts_15.13g",		0x08000, 0xdb8cebb0, 6 | BRF_GRA },				// 18
	{ "ts_14.11g",		0x08000, 0xe2e41abf, 6 | BRF_GRA },				// 19
	{ "ts_20.13j",		0x08000, 0xbfd1a695, 6 | BRF_GRA },				// 20
	{ "ts_19.11j",		0x08000, 0x928b669e, 6 | BRF_GRA },				// 21
	{ "ts_22.17j",		0x08000, 0x3fe05d9a, 6 | BRF_GRA },				// 22
	{ "ts_21.15j",		0x08000, 0x27a9bb7c, 6 | BRF_GRA },				// 23

	{ "63s141.18f",		0x00100, 0xb58d0023, 0 | BRF_OPT },				// 24 Priority (not used)
	
//	{ "c8751h-88",      0x01000, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },
	{ "d8751h.bin",     0x01000, 0x3ed7f0be, 0 | BRF_OPT },
};

STD_ROM_PICK(topsecrt)
STD_ROM_FN(topsecrt)

struct BurnDriver BurnDrvtopsecrt = {
	"topsecrt", "bionicc", NULL, NULL, "1987",
	"Top Secret (Japan)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM, 0,
	NULL, topsecrtRomInfo, topsecrtRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Bionic Commandos (bootleg, set 1)

static struct BurnRomInfo bioniccblRomDesc[] = {
	{ "02l.bin",		0x10000, 0xb2fe1ddb, 1 | BRF_PRG | BRF_ESS },	//  0 68k Code
	{ "03l.bin",		0x10000, 0x427a003d, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "02u.bin",		0x10000, 0x27f04bb6, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "03u.bin",		0x10000, 0xc01547b1, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "01.bin",			0x08000, 0x8ea07917, 2 | BRF_PRG | BRF_ESS },	//  4 Z80 Code

	{ "06.bin",			0x04000, 0x4e6b81d9, 3 | BRF_GRA },				//  5 Characters

	{ "05.bin",			0x08000, 0x9bf8dc7f, 4 | BRF_GRA },				//  6 Background Tiles
	{ "04.bin",			0x08000, 0x1b43bf63, 4 | BRF_GRA },				//  7

	{ "ts_12.17f",		0x08000, 0xe4b4619e, 5 | BRF_GRA },				//  8 Foreground Tiles
	{ "ts_11.15f",		0x08000, 0xab30237a, 5 | BRF_GRA },				//  9
	{ "ts_17.17g",		0x08000, 0xdeb657e4, 5 | BRF_GRA },				// 10
	{ "ts_16.15g",		0x08000, 0xd363b5f9, 5 | BRF_GRA },				// 11 
	{ "ts_13.18f",		0x08000, 0xa8f5a004, 5 | BRF_GRA },				// 12
	{ "ts_18.18g",		0x08000, 0x3b36948c, 5 | BRF_GRA },				// 13
	{ "ts_23.18j",		0x08000, 0xbbfbe58a, 5 | BRF_GRA },				// 14
	{ "ts_24.18k",		0x08000, 0xf156e564, 5 | BRF_GRA },				// 15

	{ "07.bin",			0x10000, 0xa0e78996, 6 | BRF_GRA },				// 16 Sprites 
	{ "11.bin",			0x10000, 0x37cb11c2, 6 | BRF_GRA },				// 17
	{ "15.bin",			0x10000, 0x4e0354ce, 6 | BRF_GRA },				// 18
	{ "16.bin",			0x10000, 0xac89e5cc, 6 | BRF_GRA },				// 19
	
	{ "63s141.18f",		0x00100, 0xb58d0023, 0 | BRF_OPT },				// 20 Priority (not used)
	
	{ "c8751h-88",     	0x01000, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },
};

STD_ROM_PICK(bioniccbl)
STD_ROM_FN(bioniccbl)

struct BurnDriver BurnDrvbioniccbl = {
	"bioniccbl", "bionicc", NULL, NULL, "1987",
	"Bionic Commandos (bootleg, set 1)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM, 0,
	NULL, bioniccblRomInfo, bioniccblRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Bionic Commandos (bootleg, set 2)

static struct BurnRomInfo bioniccbl2RomDesc[] = {
	{ "2",				0x10000, 0xc03d3424, 1 | BRF_PRG | BRF_ESS },	//  0 68k Code
	{ "4",				0x10000, 0x9f13eb9d, 1 | BRF_PRG | BRF_ESS },	//  1
	{ "3",				0x10000, 0xa909ec2c, 1 | BRF_PRG | BRF_ESS },	//  2
	{ "5",				0x10000, 0x4e6b75ce, 1 | BRF_PRG | BRF_ESS },	//  3

	{ "01.bin",			0x08000, 0x8ea07917, 2 | BRF_PRG | BRF_ESS },	//  4 Z80 Code

	{ "06.bin",			0x04000, 0x4e6b81d9, 3 | BRF_GRA },				//  5 Characters

	{ "05.bin",			0x08000, 0x9bf8dc7f, 4 | BRF_GRA },				//  6 Background Tiles
	{ "04.bin",			0x08000, 0x1b43bf63, 4 | BRF_GRA },				//  7

	{ "ts_12.17f",		0x08000, 0xe4b4619e, 5 | BRF_GRA },				//  8 Foreground Tiles
	{ "ts_11.15f",		0x08000, 0xab30237a, 5 | BRF_GRA },				//  9
	{ "ts_17.17g",		0x08000, 0xdeb657e4, 5 | BRF_GRA },				// 10
	{ "ts_16.15g",		0x08000, 0xd363b5f9, 5 | BRF_GRA },				// 11 
	{ "ts_13.18f",		0x08000, 0xa8f5a004, 5 | BRF_GRA },				// 12
	{ "ts_18.18g",		0x08000, 0x3b36948c, 5 | BRF_GRA },				// 13
	{ "ts_23.18j",		0x08000, 0xbbfbe58a, 5 | BRF_GRA },				// 14
	{ "ts_24.18k",		0x08000, 0xf156e564, 5 | BRF_GRA },				// 15

	{ "07.bin",			0x10000, 0xa0e78996, 6 | BRF_GRA },				// 16 Sprites 
	{ "11.bin",			0x10000, 0x37cb11c2, 6 | BRF_GRA },				// 17
	{ "15.bin",			0x10000, 0x4e0354ce, 6 | BRF_GRA },				// 18
	{ "16.bin",			0x10000, 0xac89e5cc, 6 | BRF_GRA },				// 19
	
	{ "63s141.18f",		0x00100, 0xb58d0023, 0 | BRF_OPT },				// 20 Priority (not used)
	
	{ "c8751h-88",     	0x01000, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },
};

STD_ROM_PICK(bioniccbl2)
STD_ROM_FN(bioniccbl2)

struct BurnDriver BurnDrvbioniccbl2 = {
	"bioniccbl2", "bionicc", NULL, NULL, "1987",
	"Bionic Commandos (bootleg, set 2)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM, 0,
	NULL, bioniccbl2RomInfo, bioniccbl2RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};
