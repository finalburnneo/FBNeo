// FB Alpha Pinball Action driver module
// Based on MAME driver by Nicola Salmoria

// move sega decrypt stuff to a file

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80Dec0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvColRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvColRAM1;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static INT32 scroll;
static UINT8 nmi_mask;
static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo PbactionInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Button 1 (L.Flipper)",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2 (R.Flipper)",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},
	{"P1 Button 3 (Plunger/Start)",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},
	{"P1 Button 4 (Shake)",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Button 1 (L.Flipper)",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2 (R.Flipper)",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},
	{"P2 Button 3 (Plunger/Start)",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"	},
	{"P2 Button 4 (Shake)",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pbaction)

static struct BurnDIPInfo PbactionDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x40, NULL					},
	{0x0e, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x0d, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},
	{0x0d, 0x01, 0x03, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x0d, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0d, 0x01, 0x30, 0x30, "2"					},
	{0x0d, 0x01, 0x30, 0x00, "3"					},
	{0x0d, 0x01, 0x30, 0x10, "4"					},
	{0x0d, 0x01, 0x30, 0x20, "5"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0d, 0x01, 0x40, 0x40, "Upright"				},
	{0x0d, 0x01, 0x40, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0d, 0x01, 0x80, 0x80, "Off"					},
	{0x0d, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x0e, 0x01, 0x07, 0x01, "70k 200k 1000k"		},
	{0x0e, 0x01, 0x07, 0x04, "100k 300k 1000k"		},
	{0x0e, 0x01, 0x07, 0x00, "70k 200k"				},
	{0x0e, 0x01, 0x07, 0x03, "100k 300k"			},
	{0x0e, 0x01, 0x07, 0x06, "200k 1000k"			},
	{0x0e, 0x01, 0x07, 0x02, "100k"					},
	{0x0e, 0x01, 0x07, 0x05, "200k"					},
	{0x0e, 0x01, 0x07, 0x07, "None"					},

	{0   , 0xfe, 0   ,    2, "Extra"				},
	{0x0e, 0x01, 0x08, 0x00, "Easy"					},
	{0x0e, 0x01, 0x08, 0x08, "Hard"					},

	{0   , 0xfe, 0   ,    4, "Difficulty (Flippers)"},
	{0x0e, 0x01, 0x30, 0x00, "Easy"					},
	{0x0e, 0x01, 0x30, 0x10, "Medium"				},
	{0x0e, 0x01, 0x30, 0x20, "Hard"					},
	{0x0e, 0x01, 0x30, 0x30, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Difficulty (Outlanes)"},
	{0x0e, 0x01, 0xc0, 0x00, "Easy"					},
	{0x0e, 0x01, 0xc0, 0x40, "Medium"				},
	{0x0e, 0x01, 0xc0, 0x80, "Hard"					},
	{0x0e, 0x01, 0xc0, 0xc0, "Hardest"				},
};

STDDIPINFO(Pbaction)

static void __fastcall pbaction_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe600:
			nmi_mask = data & 1;
		return;

		case 0xe604:
			flipscreen = data & 1;
		return;

		case 0xe606:
			scroll = data - 3;
		return;

		case 0xe800:
			soundlatch = data;
			ZetClose();
			ZetOpen(1);
			ZetSetVector(0);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
			ZetOpen(0);
		return;
	}
}

static UINT8 __fastcall pbaction_main_read(UINT16 address)
{
	if (address >= 0xc000 && address <= 0xcfff) { // pbaction3 prot
		if (address == 0xc000 && (ZetGetPC(-1) == 0xab80)) {
			return 0;
		}
		return DrvZ80RAM0[address & 0xfff];
	}

	switch (address)
	{
		case 0xe600:
			return DrvInputs[0];

		case 0xe601:
			return DrvInputs[1];

		case 0xe602:
			return DrvInputs[2];

		case 0xe604:
			return DrvDips[0];

		case 0xe605:
			return DrvDips[1];

		case 0xe606:
			return 0; // nop
	}

	return 0;
}

static void __fastcall pbaction_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xffff:
		return; // nop?
	}
}

static UINT8 __fastcall pbaction_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8000:
			return soundlatch;
	}

	return 0;
}

static void __fastcall pbaction_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x10:
		case 0x11:
			AY8910Write(0, port & 1, data);
		return;

		case 0x20:
		case 0x21:
			AY8910Write(1, port & 1, data);
		return;

		case 0x30:
		case 0x31:
			AY8910Write(2, port & 1, data);
		return;
	}
}

static tilemap_callback( bg )
{
	INT32 attr  = DrvColRAM0[offs];
	INT32 code  = DrvVidRAM0[offs] + ((attr & 0x70) * 0x10);
	INT32 flipy = (attr & 0x80) ? TILE_FLIPY : 0;

	TILE_SET_INFO(0, code, attr, flipy);
}

static tilemap_callback( fg )
{
	INT32 attr = DrvColRAM1[offs];
	INT32 code = DrvVidRAM1[offs] + ((attr & 0x30) * 0x10);

	TILE_SET_INFO(1, code, attr, TILE_FLIPYX(attr >> 6));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);

	soundlatch = 0;
	flipscreen = 0;
	nmi_mask = 0;
	scroll = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x00c000;
	DrvZ80Dec0		= Next; Next += 0x00c000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x010000;
	DrvGfxROM3		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x000800;

	DrvSprRAM		= Next; Next += 0x000100;
	DrvPalRAM		= Next; Next += 0x000200;

	DrvVidRAM0		= Next; Next += 0x000400;
	DrvColRAM0		= Next; Next += 0x000400;
	DrvVidRAM1		= Next; Next += 0x000400;
	DrvColRAM1		= Next; Next += 0x000400;
	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void sega_decode(UINT8 *rom, UINT8 *dec, INT32 length, const UINT8 convtable[32][4])
{
	INT32 cryptlen = (length >= 0x8000) ? 0x8000 : length;

	for (INT32 A = 0; A < cryptlen; A++)
	{
		INT32 xor_ = 0;
		UINT8 src = rom[A];
		INT32 row = (A & 1) + (((A >> 4) & 1) << 1) + (((A >> 8) & 1) << 2) + (((A >> 12) & 1) << 3);
		INT32 col = ((src >> 3) & 1) + (((src >> 5) & 1) << 1);

		if (src & 0x80)
		{
			col = 3 - col;
			xor_ = 0xa8;
		}

		dec[A] = (src & ~0xa8) | (convtable[2*row][col] ^ xor_);

		rom[A] = (src & ~0xa8) | (convtable[2*row+1][col] ^ xor_);

		if (convtable[2*row+0][col] == 0xff) dec[A] = 0xee;
		if (convtable[2*row+1][col] == 0xff) rom[A] = 0xee;
	}

	if (length > 0x8000)
	{
		INT32 bytes = ((length - 0x8000) >= 0x4000) ? 0x4000 : (length - 0x8000);
		memcpy(&dec[0x8000], &rom[0x8000], bytes);
	}
}

static void pbaction_decode(UINT8 *rom, UINT8 *dec, INT32 len)
{
	static const UINT8 convtable[32][4] =
	{
		{ 0xa8,0xa0,0x88,0x80 }, { 0x28,0xa8,0x08,0x88 },
		{ 0x28,0x08,0xa8,0x88 }, { 0xa8,0xa0,0x88,0x80 },
		{ 0x28,0x20,0xa8,0xa0 }, { 0x28,0xa8,0x08,0x88 },
		{ 0x28,0x08,0xa8,0x88 }, { 0x28,0x20,0xa8,0xa0 },
		{ 0xa8,0xa0,0x88,0x80 }, { 0xa8,0xa0,0x88,0x80 },
		{ 0x28,0x20,0xa8,0xa0 }, { 0x28,0x20,0xa8,0xa0 },
		{ 0x28,0x20,0xa8,0xa0 }, { 0x28,0x20,0xa8,0xa0 },
		{ 0xa8,0xa0,0x88,0x80 }, { 0x28,0x20,0xa8,0xa0 },
		{ 0xa8,0xa0,0x88,0x80 }, { 0x28,0x20,0xa8,0xa0 },
		{ 0x28,0x20,0xa8,0xa0 }, { 0xa8,0xa0,0x88,0x80 },
		{ 0x28,0x20,0xa8,0xa0 }, { 0xa0,0x80,0xa8,0x88 },
		{ 0x28,0x08,0xa8,0x88 }, { 0x28,0x08,0xa8,0x88 },
		{ 0xa0,0x80,0xa8,0x88 }, { 0xa8,0xa0,0x88,0x80 },
		{ 0x28,0x20,0xa8,0xa0 }, { 0xa8,0x28,0xa0,0x20 },
		{ 0xa0,0x80,0xa8,0x88 }, { 0xa8,0xa0,0x88,0x80 },
		{ 0xa8,0xa0,0x88,0x80 }, { 0xa8,0x28,0xa0,0x20 }
	};

	sega_decode(rom, dec, len, convtable);
}

static INT32 DrvGfxDecode() // 0, 0x80, 0, 0 - 0xf, 7, 0xf, 0xf
{
	INT32 Plane0[3] = { 0, 0x2000*8*1, 0x2000*8*2 };
	INT32 Plane1[4] = { STEP4(0,0x4000*8) };
	INT32 XOffs[32] = { STEP8(0,1), STEP8(64,1), STEP8(256,1), STEP8(256+64,1) };
	INT32 YOffs[32] = { STEP8(0,8), STEP8(128,8), STEP8(512,8), STEP8(512+128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x06000);

	GfxDecode(0x0400, 3,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x10000);

	GfxDecode(0x0800, 4,  8,  8, Plane1, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	memset (tmp, 0, 0x10000);
	memcpy (tmp, DrvGfxROM2, 0x06000);

	GfxDecode(0x0080, 3, 16, 16, Plane0, XOffs, YOffs, 0x100, tmp + 0x0000, DrvGfxROM2);
	GfxDecode(0x0020, 3, 32, 32, Plane0, XOffs, YOffs, 0x400, tmp + 0x1000, DrvGfxROM3);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 type)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (type == 2) // set 4
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  1, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  2, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x0000,  3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x2000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x4000,  5, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x0000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x4000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x8000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0xc000,  9, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x0000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x2000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x4000, 12, 1)) return 1;
		}
		else // everything else
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x8000,  2, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  3, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x2000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x4000,  6, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x0000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x4000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x8000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0xc000, 10, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x0000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x2000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x4000, 13, 1)) return 1;
		}

		if (type == 3) { // pbaction3
			for (INT32 i = 0; i < 0xc000; i++)
				DrvZ80ROM0[i] = BITSWAP08(DrvZ80ROM0[i], 7, 6, 5, 4, 1, 2, 3, 0);
		}

		DrvGfxDecode();

		if (type == 0) memcpy (DrvZ80Dec0, DrvZ80ROM0, 0xc000);
		if (type != 0) pbaction_decode(DrvZ80ROM0, DrvZ80Dec0, 0xc000);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,			0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80Dec0,			0x0000, 0x7fff, MAP_FETCHOP);
	ZetMapMemory(DrvZ80RAM0,			0xc000, 0xcfff, (type == 3) ? MAP_WRITE : MAP_RAM);
	ZetMapMemory(DrvVidRAM1,			0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(DrvColRAM1,			0xd400, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,			0xd800, 0xdbff, MAP_RAM);
	ZetMapMemory(DrvColRAM0,			0xdc00, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0xe000, 0xe0ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,				0xe400, 0xe5ff, MAP_RAM);
	ZetSetWriteHandler(pbaction_main_write);
	ZetSetReadHandler(pbaction_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,			0x4000, 0x47ff, MAP_RAM);
	ZetSetWriteHandler(pbaction_sound_write);
	ZetSetReadHandler(pbaction_sound_read);
	ZetSetOutHandler(pbaction_sound_write_port);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910Init(2, 1500000, 1);
	AY8910SetAllRoutes(0, 0.13, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.13, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.13, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3072000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 4, 8, 8, 0x20000, 0x80, 0x7); // bg
	GenericTilemapSetGfx(1, DrvGfxROM0, 3, 8, 8, 0x10000, 0x00, 0xf); // fg
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);
	AY8910Exit(2);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x200; i+=2)
	{
		UINT8 r = DrvPalRAM[i + 0] & 0xf; // xxxxBBBBGGGGRRRR
		UINT8 g = DrvPalRAM[i + 0] >> 4;
		UINT8 b = DrvPalRAM[i + 1] & 0xf;

		DrvPalette[i/2] = BurnHighCol(r+r*16,g+g*16,b+b*16,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x80 - 4; offs >= 0; offs -= 4)
	{
		if (offs > 0 && DrvSprRAM[offs - 4] & 0x80)
			continue;

		INT32 size = (DrvSprRAM[offs + 0] & 0x80) ? 32 : 16;

		INT32 sy,sx = DrvSprRAM[offs + 3];

		if (DrvSprRAM[offs] & 0x80)
			sy = 225 - DrvSprRAM[offs + 2];
		else
			sy = 241 - DrvSprRAM[offs + 2];

		INT32 flipx = DrvSprRAM[offs + 1] & 0x40;
		INT32 flipy = DrvSprRAM[offs + 1] & 0x80;

		if (flipscreen)
		{
			if (DrvSprRAM[offs] & 0x80)
			{
				sx = 224 - sx;
				sy = 225 - sy;
			}
			else
			{
				sx = 240 - sx;
				sy = 241 - sy;
			}
			flipx = !flipx;
			flipy = !flipy;

			sy -= 15;
		} else {
			sy -= 16;
		}

		INT32 code = DrvSprRAM[offs + 0] & ((size == 32) ? 0x1f : 0x7f);
		INT32 color = DrvSprRAM[offs + 1] & 0xf;

		if (size == 32) {
			Draw32x32MaskTile(pTransDraw, code, sx + ((flipscreen) ? scroll : -scroll), sy, flipx, flipy, color, 3, 0, 0, DrvGfxROM3);
		} else {
			Draw16x16MaskTile(pTransDraw, code, sx + ((flipscreen) ? scroll : -scroll), sy, flipx, flipy, color, 3, 0, 0, DrvGfxROM2);
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	GenericTilemapSetScrollX(0, scroll);
	GenericTilemapSetScrollX(1, scroll);
	GenericTilemapSetFlip(TMAP_GLOBAL, (flipscreen) ? TMAP_FLIPXY : 0);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0x00;
		DrvInputs[1] = 0x00;
		DrvInputs[2] = 0x00;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	ZetNewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (nmi_mask && i == (nInterleave - 1)) ZetNmi();
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == ((nInterleave / 2) - 1) || i == (nInterleave - 1)) {
			ZetSetVector(2);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();
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

		SCAN_VAR(scroll);
		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_mask);
		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Pinball Action (set 1)

static struct BurnRomInfo pbactionRomDesc[] = {
	{ "b-p7.bin",			0x4000, 0x8d6dcaae, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b-n7.bin",			0x4000, 0xd54d5402, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "b-l7.bin",			0x2000, 0xe7412d68, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a-e3.bin",			0x2000, 0x0e53a91f, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a-s6.bin",			0x2000, 0x9a74a8e1, 3 | BRF_GRA },           //  4 Foreground Tiles
	{ "a-s7.bin",			0x2000, 0x5ca6ad3c, 3 | BRF_GRA },           //  5
	{ "a-s8.bin",			0x2000, 0x9f00b757, 3 | BRF_GRA },           //  6

	{ "a-j5.bin",			0x4000, 0x21efe866, 4 | BRF_GRA },           //  7 Background Tiles
	{ "a-j6.bin",			0x4000, 0x7f984c80, 4 | BRF_GRA },           //  8
	{ "a-j7.bin",			0x4000, 0xdf69e51b, 4 | BRF_GRA },           //  9
	{ "a-j8.bin",			0x4000, 0x0094cb8b, 4 | BRF_GRA },           // 10

	{ "b-c7.bin",			0x2000, 0xd1795ef5, 5 | BRF_GRA },           // 11 Sprites
	{ "b-d7.bin",			0x2000, 0xf28df203, 5 | BRF_GRA },           // 12
	{ "b-f7.bin",			0x2000, 0xaf6e9817, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(pbaction)
STD_ROM_FN(pbaction)

static INT32 PbactionInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvPbaction = {
	"pbaction", NULL, NULL, NULL, "1985",
	"Pinball Action (set 1)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PINBALL, 0,
	NULL, pbactionRomInfo, pbactionRomName, NULL, NULL, NULL, NULL, PbactionInputInfo, PbactionDIPInfo,
	PbactionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Pinball Action (set 2, encrypted)

static struct BurnRomInfo pbaction2RomDesc[] = {
	{ "14.bin",				0x4000, 0xf17a62eb, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "12.bin",				0x4000, 0xec3c64c6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "13.bin",				0x4000, 0xc93c851e, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pba1.bin",			0x2000, 0x8b69b933, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a-s6.bin",			0x2000, 0x9a74a8e1, 3 | BRF_GRA },           //  4 Foreground Tiles
	{ "a-s7.bin",			0x2000, 0x5ca6ad3c, 3 | BRF_GRA },           //  5
	{ "a-s8.bin",			0x2000, 0x9f00b757, 3 | BRF_GRA },           //  6

	{ "a-j5.bin",			0x4000, 0x21efe866, 4 | BRF_GRA },           //  7 Background Tiles
	{ "a-j6.bin",			0x4000, 0x7f984c80, 4 | BRF_GRA },           //  8
	{ "a-j7.bin",			0x4000, 0xdf69e51b, 4 | BRF_GRA },           //  9
	{ "a-j8.bin",			0x4000, 0x0094cb8b, 4 | BRF_GRA },           // 10

	{ "b-c7.bin",			0x2000, 0xd1795ef5, 5 | BRF_GRA },           // 11 Sprites
	{ "b-d7.bin",			0x2000, 0xf28df203, 5 | BRF_GRA },           // 12
	{ "b-f7.bin",			0x2000, 0xaf6e9817, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(pbaction2)
STD_ROM_FN(pbaction2)

static INT32 Pbaction2Init()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvPbaction2 = {
	"pbaction2", "pbaction", NULL, NULL, "1985",
	"Pinball Action (set 2, encrypted)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PINBALL, 0,
	NULL, pbaction2RomInfo, pbaction2RomName, NULL, NULL, NULL, NULL, PbactionInputInfo, PbactionDIPInfo,
	Pbaction2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Pinball Action (set 3, encrypted)

static struct BurnRomInfo pbaction3RomDesc[] = {
	{ "pinball_09.bin",		0x8000, 0xc8e81ece, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pinball_10.bin",		0x8000, 0x04b56c7c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pinball_01.bin",		0x2000, 0x8b69b933, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "pinball_06.bin",		0x2000, 0x9a74a8e1, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "pinball_07.bin",		0x2000, 0x5ca6ad3c, 3 | BRF_GRA },           //  4
	{ "pinball_08.bin",		0x2000, 0x9f00b757, 3 | BRF_GRA },           //  5

	{ "pinball_02.bin",		0x8000, 0x01ba32c9, 4 | BRF_GRA },           //  6 Background Tiles
	{ "pinball_03.bin",		0x8000, 0xf605ae40, 4 | BRF_GRA },           //  7
	{ "pinball_04.bin",		0x8000, 0x9e23a780, 4 | BRF_GRA },           //  8
	{ "pinball_05.bin",		0x8000, 0xc9a4dfea, 4 | BRF_GRA },           //  9

	{ "pinball_14.bin",		0x2000, 0xd1795ef5, 5 | BRF_GRA },           // 10 Sprites
	{ "pinball_13.bin",		0x2000, 0xf28df203, 5 | BRF_GRA },           // 11
	{ "pinball_12.bin",		0x2000, 0xaf6e9817, 5 | BRF_GRA },           // 12
};

STD_ROM_PICK(pbaction3)
STD_ROM_FN(pbaction3)

static INT32 Pbaction3Init()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvPbaction3 = {
	"pbaction3", "pbaction", NULL, NULL, "1985",
	"Pinball Action (set 3, encrypted)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PINBALL, 0,
	NULL, pbaction3RomInfo, pbaction3RomName, NULL, NULL, NULL, NULL, PbactionInputInfo, PbactionDIPInfo,
	Pbaction3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Pinball Action (set 4, encrypted)

static struct BurnRomInfo pbaction4RomDesc[] = {
	{ "p16.bin",			0x4000, 0xad20b360, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "c15.bin",			0x4000, 0x057acfe3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p14.bin",			0x2000, 0xe7412d68, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "p1.bin",				0x2000, 0x8b69b933, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "p7.bin",				0x2000, 0x9a74a8e1, 3 | BRF_GRA },           //  4 Foreground Tiles
	{ "p8.bin",				0x2000, 0x5ca6ad3c, 3 | BRF_GRA },           //  5
	{ "p9.bin",				0x2000, 0x9f00b757, 3 | BRF_GRA },           //  6

	{ "p2.bin",				0x4000, 0x21efe866, 4 | BRF_GRA },           //  7 Background Tiles
	{ "p3.bin",				0x4000, 0x7f984c80, 4 | BRF_GRA },           //  8
	{ "p4.bin",				0x4000, 0xdf69e51b, 4 | BRF_GRA },           //  9
	{ "p5.bin",				0x4000, 0x0094cb8b, 4 | BRF_GRA },           // 10

	{ "p11.bin",			0x2000, 0xd1795ef5, 5 | BRF_GRA },           // 11 Sprites
	{ "p12.bin",			0x2000, 0xf28df203, 5 | BRF_GRA },           // 12
	{ "p13.bin",			0x2000, 0xaf6e9817, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(pbaction4)
STD_ROM_FN(pbaction4)

static INT32 Pbaction4Init()
{
	return DrvInit(5);
}

struct BurnDriver BurnDrvPbaction4 = {
	"pbaction4", "pbaction", NULL, NULL, "1985",
	"Pinball Action (set 4, encrypted)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PINBALL, 0,
	NULL, pbaction4RomInfo, pbaction4RomName, NULL, NULL, NULL, NULL, PbactionInputInfo, PbactionDIPInfo,
	Pbaction4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Pinball Action (Tecfi License)

static struct BurnRomInfo pbactiontRomDesc[] = {
	{ "pba16.bin",			0x4000, 0x4a239ebd, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pba15.bin",			0x4000, 0x3afef03a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pba14.bin",			0x2000, 0xc0a98c8a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pba1.bin",			0x2000, 0x8b69b933, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a-s6.bin",			0x2000, 0x9a74a8e1, 3 | BRF_GRA },           //  4 Foreground Tiles
	{ "a-s7.bin",			0x2000, 0x5ca6ad3c, 3 | BRF_GRA },           //  5
	{ "a-s8.bin",			0x2000, 0x9f00b757, 3 | BRF_GRA },           //  6

	{ "a-j5.bin",			0x4000, 0x21efe866, 4 | BRF_GRA },           //  7 Background Tiles
	{ "a-j6.bin",			0x4000, 0x7f984c80, 4 | BRF_GRA },           //  8
	{ "a-j7.bin",			0x4000, 0xdf69e51b, 4 | BRF_GRA },           //  9
	{ "a-j8.bin",			0x4000, 0x0094cb8b, 4 | BRF_GRA },           // 10

	{ "b-c7.bin",			0x2000, 0xd1795ef5, 5 | BRF_GRA },           // 11 Sprites
	{ "b-d7.bin",			0x2000, 0xf28df203, 5 | BRF_GRA },           // 12
	{ "b-f7.bin",			0x2000, 0xaf6e9817, 5 | BRF_GRA },           // 13

	{ "pba17.bin",			0x4000, 0x2734ae60, 0 | BRF_PRG | BRF_OPT }, // 14 Z80 #2 Code (not emulated)
};

STD_ROM_PICK(pbactiont)
STD_ROM_FN(pbactiont)

struct BurnDriver BurnDrvPbactiont = {
	"pbactiont", "pbaction", NULL, NULL, "1985",
	"Pinball Action (Tecfri License)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PINBALL, 0,
	NULL, pbactiontRomInfo, pbactiontRomName, NULL, NULL, NULL, NULL, PbactionInputInfo, PbactionDIPInfo,
	PbactionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
