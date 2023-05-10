// FB Alpha Silver Millennium driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "msm6295.h"
#include "burn_ym2151.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *tilebanks;
static UINT8 *soundlatch;
static UINT8 *tile_bank;
static UINT16 *fg_scroll_x;
static UINT16 *fg_scroll_y;
static UINT16 *bg_scroll_x;
static UINT16 *bg_scroll_y;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[3];

static INT32 puzzlove = 0;

static INT32 nCyclesExtra;

static struct BurnInputInfo SilvmilInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Silvmil)

static struct BurnDIPInfo SilvmilDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xfb, NULL			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x03, 0x03, "Easy"			},
	{0x13, 0x01, 0x03, 0x02, "Normal"		},
	{0x13, 0x01, 0x03, 0x01, "Hard"			},
	{0x13, 0x01, 0x03, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x14, 0x01, 0x04, 0x04, "Korean"		},
	{0x14, 0x01, 0x04, 0x00, "English"		},

	{0   , 0xfe, 0   ,    7, "Coinage"		},
	{0x14, 0x01, 0x38, 0x08, "4 Coins 4 Credits"	},
	{0x14, 0x01, 0x38, 0x18, "3 Coins 1 Credit"	},
	{0x14, 0x01, 0x38, 0x00, "4 Coins 2 Credits"	},
	{0x14, 0x01, 0x38, 0x28, "2 Coins 1 Credit"	},
	{0x14, 0x01, 0x38, 0x10, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0x38, 0x20, "2 Coins 2 Credits"	},
	{0x14, 0x01, 0x38, 0x38, "1 Coin  1 Credit"	},

	{0   , 0xfe, 0   ,    2, "Coin Box"		},
	{0x14, 0x01, 0x40, 0x40, "1"			},
	{0x14, 0x01, 0x40, 0x00, "2"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Silvmil)


static struct BurnDIPInfo PuzzloveDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Difficulty"	},
	{0x14, 0x01, 0x03, 0x03, "Very_Easy"	},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Very_Hard"	},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x14, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x1c, 0x14, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x1c, 0x00, "4 Coins 2 Credits"	},
	{0x14, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x1c, 0x04, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0x1c, 0x08, "2 Coins 2 Credits"	},
	{0x14, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Coin Box"		},
	{0x14, 0x01, 0x20, 0x20, "1"			},
	{0x14, 0x01, 0x20, 0x00, "2"			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x14, 0x01, 0x40, 0x00, "2"			},
	{0x14, 0x01, 0x40, 0x40, "3"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Puzzlove)

static struct BurnDIPInfo PuzzlovekDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Demo_Sounds"	},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"	},
	{0x14, 0x01, 0x03, 0x03, "Easy"	},
	{0x14, 0x01, 0x03, 0x02, "Very_Easy"			},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Very_Hard"	},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x14, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x1c, 0x14, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x1c, 0x00, "4 Coins 2 Credits"	},
	{0x14, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x1c, 0x04, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0x1c, 0x08, "2 Coins 2 Credits"	},
	{0x14, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Coin Box"		},
	{0x14, 0x01, 0x20, 0x20, "1"			},
	{0x14, 0x01, 0x20, 0x00, "2"			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x14, 0x01, 0x40, 0x00, "2"			},
	{0x14, 0x01, 0x40, 0x40, "3"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Puzzlovek)

static inline void DrvPaletteUpdate(INT32 offset)
{
	UINT8 r,g,b;
	UINT16 pal = *((UINT16*)(DrvPalRAM + offset));

	r = (BURN_ENDIAN_SWAP_INT16(pal) >> 10) & 0x1f;
	g = (BURN_ENDIAN_SWAP_INT16(pal) >>  5) & 0x1f;
	b = (BURN_ENDIAN_SWAP_INT16(pal) >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r, g, b, 0);
}

static void __fastcall silvmil_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x100000:
			*tile_bank = data & 0x03;
		return;

		case 0x10000e:
			tilebanks[*tile_bank] = data & 0x1f;
		return;
	}
}

static void __fastcall silvmil_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x100002:
			*fg_scroll_x = data & 0x3ff;
		return;

		case 0x100004:
			*fg_scroll_y = (data + 8) & 0x1ff;
		return;

		case 0x100006:
			*bg_scroll_x = (data + 4) & 0x3ff;
		return;

		case 0x100008:
			*bg_scroll_y = (data + 8) & 0x1ff;
		return;

		case 0x270000:
			*soundlatch = data;
		return;
	}
}

static UINT8 __fastcall silvmil_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x280002:
			return DrvInputs[1] >> 8;
	}

	return 0;
}

static UINT16 __fastcall silvmil_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x280000:
		case 0x280002:
		case 0x280004:
			return DrvInputs[(address >> 1) & 3];
	}

	return 0;
}

static void __fastcall silvmil_palette_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffff800) == 0x200000) {
		DrvPalRAM[(address & 0x7ff) ^ 1] = data;
		DrvPaletteUpdate(address & 0x7fe);
		return;
	}
}

static void __fastcall silvmil_palette_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff800) == 0x200000) {
		*((UINT16*)(DrvPalRAM + (address & 0x7fe))) = BURN_ENDIAN_SWAP_INT16(data);
		DrvPaletteUpdate(address & 0x7fe);
		return;
	}
}

static void __fastcall silvmil_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
			BurnYM2151Write(address&1, data);
		return;

		case 0xc002:
		case 0xc003:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 __fastcall silvmil_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xc001:
			return BurnYM2151Read();

		case 0xc002:
		case 0xc003:
			return MSM6295Read(0);

		case 0xc006:
			return *soundlatch;
	}

	return 0;
}

static void silvmilYM2151IrqHandler(INT32 nStatus)
{
	if (ZetGetActive() != -1)
		ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
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
	MSM6295Reset(0);

	HiscoreReset();

	nCyclesExtra = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x100000;
	DrvZ80ROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x800000;
	DrvGfxROM1	= Next; Next += 0x400000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x010000;
	DrvPalRAM	= Next; Next += 0x000800;
	DrvFgRAM	= Next; Next += 0x001000;
	DrvBgRAM	= Next; Next += 0x001000;
	DrvSprRAM	= Next; Next += 0x000800;

	DrvZ80RAM	= Next; Next += 0x000800;

	soundlatch	= Next; Next += 0x000001;

	tile_bank	= Next; Next += 0x000001;
	tilebanks	= Next; Next += 0x000004;

	fg_scroll_x	= (UINT16*)Next; Next += 0x0001 * sizeof (UINT16);
	bg_scroll_x	= (UINT16*)Next; Next += 0x0001 * sizeof (UINT16);
	fg_scroll_y	= (UINT16*)Next; Next += 0x0001 * sizeof (UINT16);
	bg_scroll_y	= (UINT16*)Next; Next += 0x0001 * sizeof (UINT16);

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode(UINT8 *gfx, INT32 gfxlen)
{
	INT32 Planes[4] = { ((gfxlen / 2) * 8) + 8,  ((gfxlen / 2) * 8) + 0,  8,  0 };
	INT32 XOffs[16] = { STEP8(256, 1), STEP8(0, 1) };
	INT32 YOffs[16] = { STEP16(0, 16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(gfxlen);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, gfx, gfxlen);

	GfxDecode((gfxlen * 2) / (16 * 16), 4, 16, 16, Planes, XOffs, YOffs, 0x200, tmp, gfx);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	BurnAllocMemIndex();

	puzzlove = game_select; // 1 puzzlove, 0 silmil

	if (puzzlove)
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001,  5, 2)) return 1;

		for (INT32 i = 0; i < 0x400000; i++) {
			DrvGfxROM0[((i & 0x03ffff) ^ 0x20) | ((~i & 0x040000) << 3) | ((i & 0x380000) >> 1)] = DrvGfxROM1[i&0xfffff];
		}

		memset (DrvGfxROM1, 0, 0x200000);

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100001,  9, 2)) return 1;

		DrvGfxDecode(DrvGfxROM0, 0x400000);
		DrvGfxDecode(DrvGfxROM1, 0x200000);
	}
	else
	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x200000,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x200001,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x300000, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x300001, 11, 2)) return 1;

		for (INT32 i = 0; i < 0x400000; i++) {
			DrvGfxROM0[((i & 0x03ffff) ^ 0x20) | ((~i & 0x040000) << 3) | ((i & 0x380000) >> 1)] = DrvGfxROM1[i];
		}

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001, 13, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000, 14, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100001, 15, 2)) return 1;

		DrvGfxDecode(DrvGfxROM0, 0x400000);
		DrvGfxDecode(DrvGfxROM1, 0x200000);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvFgRAM,		0x120000, 0x120fff, MAP_RAM);
	SekMapMemory(DrvBgRAM,		0x122000, 0x122fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x200000, 0x2007ff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0x210000, 0x2107ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x300000, 0x30ffff, MAP_RAM);
	SekSetWriteByteHandler(0,	silvmil_write_byte);
	SekSetWriteWordHandler(0,	silvmil_write_word);
	SekSetReadByteHandler(0,	silvmil_read_byte);
	SekSetReadWordHandler(0,	silvmil_read_word);

	SekMapHandler(1,		0x200000, 0x2007ff, MAP_WRITE);
	SekSetWriteByteHandler(1,	silvmil_palette_write_byte);
	SekSetWriteWordHandler(1,	silvmil_palette_write_word);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xbfff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0xbfff, 2, DrvZ80ROM);
	ZetMapArea(0xd000, 0xd7ff, 0, DrvZ80RAM);
	ZetMapArea(0xd000, 0xd7ff, 1, DrvZ80RAM);
	ZetMapArea(0xd000, 0xd7ff, 2, DrvZ80RAM);
	ZetSetWriteHandler(silvmil_sound_write);
	ZetSetReadHandler(silvmil_sound_read);
	ZetClose();

	BurnYM2151InitBuffered(3579545, 1, NULL, 0);
	BurnYM2151SetIrqHandler(&silvmilYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachZet(4096000);

	MSM6295Init(0, 1024000 / 132, 1);
	MSM6295SetRoute(0, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM2151Exit();
	MSM6295Exit(0);

	MSM6295ROM = NULL;

	SekExit();
	ZetExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_layer(UINT8 *src, INT32 colbank, INT32 scrollx, INT32 scrolly, INT32 transp)
{
	UINT16 *vram = (UINT16*)src;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) << 4;
		INT32 sy = (offs >> 6) << 4;

		sx -= scrollx;
		if (sx < -15) sx += 0x400;
		sy -= scrolly;
		if (sy < -15) sy += 0x200;

		if (sy >= nScreenHeight || sx >= nScreenWidth) continue;

		INT32 data  = BURN_ENDIAN_SWAP_INT16(vram[(offs & 0x1f) + ((offs >> 6) << 5) + ((offs & 0x20) << 5)]);
		INT32 code  = (data & 0x3ff) + (tilebanks[(data & 0x0c00) >> 10] << 10);
		INT32 color = (data >> 12) + colbank;

		if (transp) {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM0);
		} else {
			Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
		}
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0;offs < 0x400;offs += 4)
	{
		INT32 sprite = BURN_ENDIAN_SWAP_INT16(ram[offs + 1]) & 0x3fff;
		if (!sprite) continue;

		INT32 y = BURN_ENDIAN_SWAP_INT16(ram[offs]);
		INT32 flash = (y & ((puzzlove) ? 0x0400 : 0x1000));
		if (flash && (GetCurrentFrame() & 1)) continue;

		INT32 x = BURN_ENDIAN_SWAP_INT16(ram[offs + 2]);
		INT32 color = ((x >> 9) & 0x7f) | ((y & 0x8000) >> 8);

		INT32 fx = y & 0x2000;
		INT32 fy = y & 0x4000;
		INT32 multi;

		if (puzzlove) {
			multi = ((y & 0x1000) >> 12) | ((y & 0x0200) >> 8);
		} else {
			multi = ((y & 0x0600) >>  9);
		}

		multi = (1 << multi) - 1;

		x &= 0x01ff;
		y &= 0x01ff;
		if (x >= 320) x -= 512;
		if (y >= 256) y -= 512;
		y = (240 - y) - 7;
		x = (304 - x) - 5;

		INT32 inc = -1;

		if (!fy)
		{
			sprite += multi;
			inc = 1;
		}

		while (multi >= 0)
		{
			Draw16x16MaskTile(pTransDraw, sprite - multi * inc, x, y - 16 * multi, fx, fy, color & 0x3f, 4, 0, 0, DrvGfxROM1);

			multi--;
		}
	}
}

static void DrvPaletteRecalc()
{
	UINT8 r,g,b;
	UINT16 *pal = (UINT16*)DrvPalRAM;
	for (INT32 i = 0; i < 0x300; i++) {
		r = (BURN_ENDIAN_SWAP_INT16(pal[i]) >> 10) & 0x1f;
		g = (BURN_ENDIAN_SWAP_INT16(pal[i]) >>  5) & 0x1f;
		b = (BURN_ENDIAN_SWAP_INT16(pal[i]) >>  0) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteRecalc();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) draw_layer(DrvBgRAM, 0x20, *bg_scroll_x, *bg_scroll_y, 0);
	if ( nBurnLayer & 2) draw_layer(DrvFgRAM, 0x10, *fg_scroll_x, *fg_scroll_y, 1);
	if ( nBurnLayer & 4) draw_sprites();

	BurnTransferCopy(DrvPalette);

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
		DrvInputs[0] = DrvInputs[1] = 0xffff;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		DrvInputs[2] = (DrvDips[1] << 8) | DrvDips[0];
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 12000000 / 60, 4096000 /*3579545?*/ / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		CPU_RUN_TIMER(1);
	}

	SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);

	ZetClose();
	SekClose();

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029698;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		ZetScan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// PuzzLove

static struct BurnRomInfo puzzloveRomDesc[] = {
	{ "3.u3",		0x40000, 0x826c8472, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "4.u2",		0x40000, 0x64ddc708, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.uz02",		0x20000, 0x3077e7f3, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "2.uz11",		0x40000, 0x4c06ec68, 3 | BRF_SND },           //  3 MSM6295 Samples

	{ "10.u41",		0x80000, 0x7200f878, 4 | BRF_GRA },           //  4 Background Tiles
	{ "9.u42",		0x80000, 0x21b1b297, 4 | BRF_GRA },           //  5

	{ "5.u53",		0x80000, 0x8707d5a0, 5 | BRF_GRA },           //  6 Sprites
	{ "6.u54",		0x80000, 0x60a6d614, 5 | BRF_GRA },           //  7
	{ "7.u55",		0x80000, 0x0f2ea5c4, 5 | BRF_GRA },           //  8
	{ "8.u56",		0x80000, 0x037dcd3d, 5 | BRF_GRA },           //  9
};

STD_ROM_PICK(puzzlove)
STD_ROM_FN(puzzlove)

static INT32 puzzloveInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvPuzzlove = {
	"puzzlove", NULL, NULL, NULL, "1994",
	"PuzzLove\0", NULL, "Para", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, puzzloveRomInfo, puzzloveRomName, NULL, NULL, NULL, NULL, SilvmilInputInfo, PuzzloveDIPInfo,
	puzzloveInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	320, 240, 4, 3
};


// PuzzLove (Korea)

static struct BurnRomInfo puzzlovekRomDesc[] = {
	{ "3.u3",		0x40000, 0x28f403f0, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "4.u2",		0x40000, 0x809371b9, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.uz02",		0x20000, 0x3077e7f3, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "2.uz11",		0x40000, 0x4c06ec68, 3 | BRF_SND },           //  3 MSM6295 Samples

	{ "10.u41",		0x80000, 0x3f952c54, 4 | BRF_GRA },           //  4 Background Tiles
	{ "9.u42",		0x80000, 0x4f71ee93, 4 | BRF_GRA },           //  5

	{ "5.u53",		0x80000, 0xc5732995, 5 | BRF_GRA },           //  6 Sprites
	{ "6.u54",		0x80000, 0xa3b98fd1, 5 | BRF_GRA },           //  7
	{ "7.u55",		0x80000, 0xa4c73b48, 5 | BRF_GRA },           //  8
	{ "8.u56",		0x80000, 0x95b5f049, 5 | BRF_GRA },           //  9
};

STD_ROM_PICK(puzzlovek)
STD_ROM_FN(puzzlovek)

struct BurnDriver BurnDrvPuzzlovek = {
	"puzzlovek", "puzzlove", NULL, NULL, "1994",
	"PuzzLove (Korea)\0", NULL, "Para", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, puzzlovekRomInfo, puzzlovekRomName, NULL, NULL, NULL, NULL, SilvmilInputInfo, PuzzlovekDIPInfo,
	puzzloveInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	320, 240, 4, 3
};


// Silver Millennium

static struct BurnRomInfo silvmilRomDesc[] = {
	{ "d-13_u3.bin",	0x40000, 0x46106c59, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "d-14_u2.bin",	0x40000, 0x65421eca, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "d-11_uz02.bin",	0x10000, 0xb41289e6, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "d-12_uz1.bin",	0x40000, 0xa170d8a9, 3 | BRF_SND },           //  3 MSM6295 Samples

	{ "d-16_u41.bin",	0x80000, 0xff5ea605, 4 | BRF_GRA },           //  4 Background Tiles
	{ "d-15_u42.bin",	0x80000, 0x5f72d6cc, 4 | BRF_GRA },           //  5
	{ "d-17_pat11.bin",	0x80000, 0x2facb3f1, 4 | BRF_GRA },           //  6
	{ "d-20_pat01.bin",	0x80000, 0xcc4d3b49, 4 | BRF_GRA },           //  7
	{ "d-18_pat12.bin",	0x80000, 0xa429b237, 4 | BRF_GRA },           //  8
	{ "d-17_pat02.bin",	0x80000, 0xaddc5261, 4 | BRF_GRA },           //  9
	{ "d-19_pat13.bin",	0x80000, 0xaf2507ce, 4 | BRF_GRA },           // 10
	{ "d-18_pat03.bin",	0x80000, 0x36b9c407, 4 | BRF_GRA },           // 11

	{ "d-17_u53.bin",	0x80000, 0x4d177bda, 5 | BRF_GRA },           // 12 Sprites
	{ "d-18_u54.bin",	0x80000, 0x218c4471, 5 | BRF_GRA },           // 13
	{ "d-19_u55.bin",	0x80000, 0x59507521, 5 | BRF_GRA },           // 14
	{ "d-20_u56.bin",	0x80000, 0xe67c2c7d, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(silvmil)
STD_ROM_FN(silvmil)

static INT32 silmilInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvSilvmil = {
	"silvmil", NULL, NULL, NULL, "1995",
	"Silver Millennium\0", NULL, "Para", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, silvmilRomInfo, silvmilRomName, NULL, NULL, NULL, NULL, SilvmilInputInfo, SilvmilDIPInfo,
	silmilInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	240, 320, 3, 4
};


