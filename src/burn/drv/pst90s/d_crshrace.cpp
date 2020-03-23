// FinalBurn Neo Lethal Crash Race driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2610.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvVidRAM[2];
static UINT8 *DrvSprRAM[2];
static UINT8 *DrvSprBuf[2][2];
static UINT8 *DrvZ80RAM;
static UINT16 *DrvGfxCtrl;

static UINT8 DrvRecalc;

static UINT8 sound_bank;
static UINT8 roz_bank;
static UINT8 gfx_priority;
static UINT8 soundlatch[2];
static UINT8 flipscreen;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[4];
static UINT16 DrvInputs[6];
static UINT8 DrvReset;

static struct BurnInputInfo CrshraceInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy1 + 10,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p3 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 15,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 14,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Region",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Crshrace)

static struct BurnDIPInfo CrshraceDIPList[]=
{
	{0x1e, 0xff, 0xff, 0xff, NULL						},
	{0x1f, 0xff, 0xff, 0xff, NULL						},
	{0x20, 0xff, 0xff, 0xff, NULL						},
	{0x21, 0xff, 0xff, 0x01, NULL						},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
//	{0x1e, 0x01, 0x01, 0x01, "Off"						},
//	{0x1e, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x1e, 0x01, 0x02, 0x00, "Off"						},
	{0x1e, 0x01, 0x02, 0x02, "On"						},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x1e, 0x01, 0x04, 0x04, "Off"						},
	{0x1e, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x1e, 0x01, 0x08, 0x08, "Off"						},
	{0x1e, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x1e, 0x01, 0xc0, 0x80, "Easy"						},
	{0x1e, 0x01, 0xc0, 0xc0, "Normal"					},
	{0x1e, 0x01, 0xc0, 0x40, "Hard"						},
	{0x1e, 0x01, 0xc0, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Coin Slot"				},
	{0x1f, 0x01, 0x01, 0x01, "Same"						},
	{0x1f, 0x01, 0x01, 0x00, "Individual"				},

	// Coinage condition: Coin Slot Individual
	{0   , 0xfe, 0   ,    8, "Coinage"					},
	{0x1f, 0x02, 0x0e, 0x0a, "3 Coins 1 Credit"			},
	{0x1f, 0x02, 0x0e, 0x0c, "2 Coins 1 Credit"			},
	{0x1f, 0x02, 0x0e, 0x0e, "1 Coin  1 Credit"			},
	{0x1f, 0x02, 0x0e, 0x08, "1 Coin  2 Credits"		},
	{0x1f, 0x02, 0x0e, 0x06, "1 Coin  3 Credits"		},
	{0x1f, 0x02, 0x0e, 0x04, "1 Coin  4 Credits"		},
	{0x1f, 0x02, 0x0e, 0x02, "1 Coin  5 Credits"		},
	{0x1f, 0x02, 0x0e, 0x00, "1 Coin  6 Credits"		},

	// Coin A condition: Coin Slot Same
	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x1f, 0x02, 0x0e, 0x0a, "3 Coins 1 Credit"			},
	{0x1f, 0x02, 0x0e, 0x0c, "2 Coins 1 Credit"			},
	{0x1f, 0x02, 0x0e, 0x0e, "1 Coin  1 Credit"			},
	{0x1f, 0x02, 0x0e, 0x08, "1 Coin  2 Credits"		},
	{0x1f, 0x02, 0x0e, 0x06, "1 Coin  3 Credits"		},
	{0x1f, 0x02, 0x0e, 0x04, "1 Coin  4 Credits"		},
	{0x1f, 0x02, 0x0e, 0x02, "1 Coin  5 Credits"		},
	{0x1f, 0x02, 0x0e, 0x00, "1 Coin  6 Credits"		},

	// Coin B condition: Coin Slot Same
	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x1f, 0x02, 0x70, 0x50, "3 Coins 1 Credit"			},
	{0x1f, 0x02, 0x70, 0x60, "2 Coins 1 Credit"			},
	{0x1f, 0x02, 0x70, 0x70, "1 Coin  1 Credit"			},
	{0x1f, 0x02, 0x70, 0x40, "1 Coin  2 Credits"		},
	{0x1f, 0x02, 0x70, 0x30, "1 Coin  3 Credits"		},
	{0x1f, 0x02, 0x70, 0x20, "1 Coin  4 Credits"		},
	{0x1f, 0x02, 0x70, 0x10, "1 Coin  5 Credits"		},
	{0x1f, 0x02, 0x70, 0x00, "1 Coin  6 Credits"		},
	{0x1f, 0x00, 0x01, 0x01, NULL},

	{0   , 0xfe, 0   ,    2, "2C Start, 1C to Continue"	},
	{0x1f, 0x01, 0x80, 0x80, "Off"						},
	{0x1f, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Reset on P.O.S.T. Error"	},
	{0x20, 0x01, 0x80, 0x00, "No"						},
	{0x20, 0x01, 0x80, 0x80, "Yes"						},

	{0   , 0xfe, 0   ,    5, "Country"					},
	{0x21, 0x01, 0x0f, 0x01, "World"					},
	{0x21, 0x01, 0x0f, 0x08, "USA & Canada"				},
	{0x21, 0x01, 0x0f, 0x00, "Japan"					},
	{0x21, 0x01, 0x0f, 0x02, "Korea"					},
	{0x21, 0x01, 0x0f, 0x04, "Hong Kong & Taiwan"		},
};

STDDIPINFO(Crshrace)

static void __fastcall crshrace_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0xfff020 && address <= 0xfff03f) { // K053936_0_ctrl
		DrvGfxCtrl[(address & 0x1f)/2] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if ((address & 0xfffe000) == 0xd00000) {
		*((UINT16*)(DrvVidRAM[0] + (address & 0x1ffe))) = BURN_ENDIAN_SWAP_INT16(data);
		GenericTilemapSetTileDirty(1, (address & 0x1ffe)/2);
		return;
	}
}

static void __fastcall crshrace_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffe000) == 0xd00000) {
		DrvVidRAM[0][(address & 0x1fff) ^ 1] = data;
		GenericTilemapSetTileDirty(1, (address & 0x1ffe)/2);
		return;
	}

	switch (address)
	{
		case 0xffc001:
			if (roz_bank != data) {
				roz_bank = data;
				GenericTilemapAllTilesDirty(1);
			}
		return;

		case 0xfff001:
			gfx_priority = data & 0xdf;
			flipscreen = data & 0x20;
		return;

		case 0xfff009:
			soundlatch[1] = 1;
			soundlatch[0] = data;
			ZetNmi();
		return;
	}
}

static UINT8 __fastcall crshrace_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xfff000:
			return DrvInputs[0] >> 8;

		case 0xfff001:
			return DrvInputs[0];

		case 0xfff002:
			return DrvInputs[1] >> 8;

		case 0xfff003:
			return DrvInputs[1];

		case 0xfff004:
			return DrvDips[1];

		case 0xfff005:
			return DrvDips[0];

		case 0xfff00b:
			return DrvDips[2];

		case 0xfff00f:
			return DrvInputs[2];

		case 0xfff006:
			return DrvDips[3] | (soundlatch[1] << 7);
	}

	return 0;
}

static void bankswitch(INT32 data)
{
	sound_bank = data & 0x03;

	ZetMapMemory(DrvZ80ROM + (sound_bank * 0x8000), 0x8000, 0xffff, MAP_ROM);
}

static void __fastcall crshrace_sound_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			bankswitch(data);
		return;

		case 0x04:
			soundlatch[1] = 0;
		return;

		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			BurnYM2610Write(port & 3, data);
		return;
	}
}

static UINT8 __fastcall crshrace_sound_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x04:
			return soundlatch[0];

		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			return BurnYM2610Read(port & 3);
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 code = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM[1] + offs * 2)));

	TILE_SET_INFO(0, code, 0, 0);
}

static tilemap_callback( rz )
{
	INT32 code = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM[0] + offs * 2)));

	TILE_SET_INFO(1, (code & 0xfff) | (roz_bank * 0x1000), code >> 12, 0);
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	bankswitch(1);
	ZetReset();
	ZetClose();

	BurnYM2610Reset();

	soundlatch[0] = 0;
	soundlatch[1] = 0;
	roz_bank = 0;
	flipscreen = 0;
	gfx_priority = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x300000;
	DrvZ80ROM			= Next; Next += 0x020000;

	DrvGfxROM[0]		= Next; Next += 0x100000;
	DrvGfxROM[1]		= Next; Next += 0x800000;
	DrvGfxROM[2]		= Next; Next += 0x800000;

	DrvSndROM			= Next; Next += 0x200000;

	BurnPalette			= (UINT32  *)Next; Next += 0x000401 * sizeof(UINT32);

	AllRam				= Next;

	Drv68KRAM			= Next; Next += 0x010000;
	BurnPalRAM			= Next; Next += 0x001000;
	DrvVidRAM[0]		= Next; Next += 0x002000;
	DrvVidRAM[1]		= Next; Next += 0x001000;

	DrvSprRAM[0]		= Next; Next += 0x002000;
	DrvSprRAM[1]		= Next; Next += 0x010000;

	DrvSprBuf[0][0]		= Next; Next += 0x002000;
	DrvSprBuf[0][1]		= Next; Next += 0x002000;
	DrvSprBuf[1][0]		= Next; Next += 0x010000;
	DrvSprBuf[1][1]		= Next; Next += 0x010000;

	DrvZ80RAM			= Next; Next += 0x000800;

	DrvGfxCtrl			= (UINT16*)Next; Next += 0x000010 * sizeof(UINT16);

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM    + 0x0000000, k++, 1)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x0100000, k++, 1)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x0200000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x0000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x0000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x0000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x0100000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x0200000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x0000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x0200000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM    + 0x0000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM    + 0x0100000, k++, 1)) return 1;

		BurnByteswap(DrvGfxROM[1], 0x300000);
		BurnNibbleExpand(DrvGfxROM[1], NULL, 0x300000, 0, 0);
		BurnNibbleExpand(DrvGfxROM[2], NULL, 0x400000, 1, 0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,				0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KROM + 0x100000,	0x300000, 0x3fffff, MAP_ROM);
	SekMapMemory(Drv68KROM + 0x200000,	0x400000, 0x4fffff, MAP_ROM);
	SekMapMemory(Drv68KROM + 0x200000,	0x500000, 0x5fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM[1],			0xa00000, 0xa0ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM[0],			0xd00000, 0xd01fff, MAP_ROM);
	SekMapMemory(DrvSprRAM[0],			0xe00000, 0xe01fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,				0xfe0000, 0xfeffff, MAP_RAM);
	SekMapMemory(DrvVidRAM[1],			0xffd000, 0xffdfff, MAP_RAM);
	SekMapMemory(BurnPalRAM,			0xffe000, 0xffefff, MAP_RAM);
	SekSetWriteWordHandler(0,			crshrace_write_word);
	SekSetWriteByteHandler(0,			crshrace_write_byte);
	SekSetReadByteHandler(0,			crshrace_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x77ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0x7800, 0x7fff, MAP_RAM);
	ZetSetOutHandler(crshrace_sound_out);
	ZetSetInHandler(crshrace_sound_in);
	ZetClose();

	INT32 DrvSndROMLen = 0x100000;
	BurnYM2610Init(8000000, DrvSndROM + 0x100000, &DrvSndROMLen, DrvSndROM, &DrvSndROMLen, &DrvFMIRQHandler, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback,  8,  8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, rz_map_callback, 16, 16, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 8,  8,  8, 0x100000, 0x000, 0);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x800000, 0x100, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x800000, 0x200, 0xf);
	GenericTilemapSetTransparent(0, 0xff);
	GenericTilemapUseDirtyTiles(1);

	BurnBitmapAllocate(1, 16 * 64, 16 * 64, true);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM2610Exit();
	SekExit();
	ZetExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites()
{
	INT32 offs = 0;
	UINT16 *sprbuf1 = (UINT16*)DrvSprBuf[0][0];
	UINT16 *sprbuf2 = (UINT16*)DrvSprBuf[1][0];

	static const INT32 zoomtable[16] = { 0,7,14,20,25,30,34,38,42,46,49,52,54,57,59,61 };

	while (offs < 0x0400 && (BURN_ENDIAN_SWAP_INT16(sprbuf1[offs]) & 0x4000) == 0)
	{
		INT32 attr_start = 4 * (BURN_ENDIAN_SWAP_INT16(sprbuf1[offs++]) & 0x03ff);

		INT32 ox        =  BURN_ENDIAN_SWAP_INT16(sprbuf1[attr_start + 1]) & 0x01ff;
		INT32 xsize     = (BURN_ENDIAN_SWAP_INT16(sprbuf1[attr_start + 1]) & 0x0e00) >> 9;
		INT32 zoomx     = (BURN_ENDIAN_SWAP_INT16(sprbuf1[attr_start + 1]) & 0xf000) >> 12;
		INT32 oy        =  BURN_ENDIAN_SWAP_INT16(sprbuf1[attr_start + 0]) & 0x01ff;
		INT32 ysize     = (BURN_ENDIAN_SWAP_INT16(sprbuf1[attr_start + 0]) & 0x0e00) >> 9;
		INT32 zoomy     = (BURN_ENDIAN_SWAP_INT16(sprbuf1[attr_start + 0]) & 0xf000) >> 12;
		INT32 flipx     =  BURN_ENDIAN_SWAP_INT16(sprbuf1[attr_start + 2]) & 0x4000;
		INT32 flipy     =  BURN_ENDIAN_SWAP_INT16(sprbuf1[attr_start + 2]) & 0x8000;
		INT32 color     = (BURN_ENDIAN_SWAP_INT16(sprbuf1[attr_start + 2]) & 0x1f00) >> 8;
		INT32 map_start =  BURN_ENDIAN_SWAP_INT16(sprbuf1[attr_start + 3]) & 0x7fff;

		zoomx = 16 - zoomtable[zoomx]/8;
		zoomy = 16 - zoomtable[zoomy]/8;

		if (BURN_ENDIAN_SWAP_INT16(sprbuf1[attr_start + 2]) & 0x20ff) color = 1; // what?? mame_rand? why?

		for (INT32 y = 0;y <= ysize;y++)
		{
			INT32 sx,sy;

			if (flipy) sy = ((oy + zoomy * (ysize - y) + 16) & 0x1ff) - 16;
			else sy = ((oy + zoomy * y + 16) & 0x1ff) - 16;

			for (INT32 x = 0;x <= xsize;x++)
			{
				if (flipx)
					sx = ((ox + zoomx * (xsize - x) + 16) & 0x1ff) - 16;
				else
					sx = ((ox + zoomx * x + 16) & 0x1ff) - 16;

				INT32 code = BURN_ENDIAN_SWAP_INT16(sprbuf2[map_start & 0x7fff]) & 0x7fff;
				map_start++;

				RenderZoomedTile(pTransDraw, DrvGfxROM[2], code, (color << 4) | 0x200, 0x0f, sx, sy, flipx, flipy, 16, 16, zoomx << 12, zoomy << 12);
			}
		}
	}
}

static inline void copy_roz(UINT32 startx, UINT32 starty, INT32 incxx, INT32 incxy, INT32 incyx, INT32 incyy)
{
	UINT16 *dst = pTransDraw;
	UINT16 *src = BurnBitmapGetBitmap(1);

	GenericTilemapDraw(1, 1, 0); // update tilemap

	for (INT32 sy = 0; sy < nScreenHeight; sy++, startx+=incyx, starty+=incyy)
	{
		UINT32 cx = startx;
		UINT32 cy = starty;

		for (INT32 x = 0; x < nScreenWidth; x++, cx+=incxx, cy+=incxy, dst++)
		{
			INT32 p = BURN_ENDIAN_SWAP_INT16(src[(((cy >> 16) & 0x3ff) * 1024) + ((cx >> 16) & 0x3ff)]);

			if ((p & 0xf) != 0xf) {
				*dst = p;
			}
		}
	}
}

static void draw_foreground()
{
	UINT32 startx = 256 * (INT16)(BURN_ENDIAN_SWAP_INT16(DrvGfxCtrl[0x00]));
	UINT32 starty = 256 * (INT16)(BURN_ENDIAN_SWAP_INT16(DrvGfxCtrl[0x01]));
	INT32 incyx   =       (INT16)(BURN_ENDIAN_SWAP_INT16(DrvGfxCtrl[0x02]));
	INT32 incyy   =       (INT16)(BURN_ENDIAN_SWAP_INT16(DrvGfxCtrl[0x03]));
	INT32 incxx   =       (INT16)(BURN_ENDIAN_SWAP_INT16(DrvGfxCtrl[0x04]));
	INT32 incxy   =       (INT16)(BURN_ENDIAN_SWAP_INT16(DrvGfxCtrl[0x05]));

	if (BURN_ENDIAN_SWAP_INT16(DrvGfxCtrl[0x06]) & 0x4000) { incyx *= 256; incyy *= 256; }
	if (BURN_ENDIAN_SWAP_INT16(DrvGfxCtrl[0x06]) & 0x0040) { incxx *= 256; incxy *= 256; }

	startx -= -21 * incyx;
	starty -= -21 * incyy;
	startx -= -48 * incxx;
	starty -= -48 * incxy;

	copy_roz(startx << 5, starty << 5, incxx << 5, incxy << 5, incyx << 5, incyy << 5);
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_xGGGGGBBBBBRRRRR();
		BurnPalette[0x400] = 0; // black
		DrvRecalc = 1; // force update
	}

	if (gfx_priority & 0x04)
	{
		BurnTransferClear(0x400);
	}
	else
	{
		BurnTransferClear(0x1ff);

		switch (gfx_priority & 0xfb)
		{
			case 0x00:
				if (nSpriteEnable & 1) draw_sprites();
				if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);
				if (nBurnLayer & 2) draw_foreground();
			break;

			case 0x01:
			case 0x02:
				if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);
				if (nBurnLayer & 2) draw_foreground();
				if (nSpriteEnable & 1) draw_sprites();
			break;
		}
	}

	BurnTransferCopy(BurnPalette);

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
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		DrvInputs[3] = (DrvDips[0]) | (DrvDips[1] << 8);
		DrvInputs[4] = DrvDips[2];
		DrvInputs[5] = (DrvDips[3] << 8);
	}

	INT32 nCyclesTotal[2] = { 16000000 / 60, 4000000 / 60 };

	SekOpen(0);
	ZetOpen(0);

	SekRun(nCyclesTotal[0]);
	SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	// double buffer sprites
	memcpy (DrvSprBuf[0][1], DrvSprBuf[0][0], 0x002000);
	memcpy (DrvSprBuf[0][0], DrvSprRAM[0],    0x002000);
	memcpy (DrvSprBuf[1][1], DrvSprBuf[1][0], 0x010000);
	memcpy (DrvSprBuf[1][0], DrvSprRAM[1],    0x010000);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin =  0x029702;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		BurnYM2610Scan(nAction, pnMin);

		SCAN_VAR(sound_bank);
		SCAN_VAR(roz_bank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(gfx_priority);
	}

	if (nAction & ACB_WRITE) {
		GenericTilemapAllTilesDirty(1);

		ZetOpen(0);
		bankswitch(sound_bank);
		ZetClose();
	}

	return 0;
}


// Lethal Crash Race (set 1)

static struct BurnRomInfo crshraceRomDesc[] = {
	{ "1",				0x080000, 0x21e34fb7, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "w21",			0x100000, 0xa5df7325, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "w22",			0x100000, 0xfc9d666d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "2",				0x020000, 0xe70a900f, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "h895",			0x100000, 0x36ad93c3, 3 | BRF_GRA },           //  4 Background Tiles

	{ "w18",			0x100000, 0xb15df90d, 4 | BRF_GRA },           //  5 Foreground Tiles
	{ "w19",			0x100000, 0x28326b93, 4 | BRF_GRA },           //  6
	{ "w20",			0x100000, 0xd4056ad1, 4 | BRF_GRA },           //  7

	{ "h897",			0x200000, 0xe3230128, 5 | BRF_GRA },           //  8 Sprites
	{ "h896",			0x200000, 0xfff60233, 5 | BRF_GRA },           //  9

	{ "h894",			0x100000, 0xd53300c1, 6 | BRF_SND },           // 10 YM2610 Samples
	{ "h893",			0x100000, 0x32513b63, 6 | BRF_SND },           // 11
};

STD_ROM_PICK(crshrace)
STD_ROM_FN(crshrace)

struct BurnDriver BurnDrvCrshrace = {
	"crshrace", NULL, NULL, NULL, "1993",
	"Lethal Crash Race (set 1)\0", NULL, "Video System Co.", "Miscellaneous",
	L"Lethal Crash Race\0\u7206\u70C8 \u30AF\u30E9\u30C3\u30B7\u30E5 \u30EC\u30FC\u30B9 (set 1)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, crshraceRomInfo, crshraceRomName, NULL, NULL, NULL, NULL, CrshraceInputInfo, CrshraceDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 320, 3, 4
};


// Lethal Crash Race (set 2)

static struct BurnRomInfo crshrace2RomDesc[] = {
	{ "01-ic10.bin",	0x080000, 0xb284aacd, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "w21",			0x100000, 0xa5df7325, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "w22",			0x100000, 0xfc9d666d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "2",				0x020000, 0xe70a900f, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "h895",			0x100000, 0x36ad93c3, 3 | BRF_GRA },           //  4 Background Tiles

	{ "w18",			0x100000, 0xb15df90d, 4 | BRF_GRA },           //  5 Foreground Tiles
	{ "w19",			0x100000, 0x28326b93, 4 | BRF_GRA },           //  6
	{ "w20",			0x100000, 0xd4056ad1, 4 | BRF_GRA },           //  7

	{ "h897",			0x200000, 0xe3230128, 5 | BRF_GRA },           //  8 Sprites
	{ "h896",			0x200000, 0xfff60233, 5 | BRF_GRA },           //  9

	{ "h894",			0x100000, 0xd53300c1, 6 | BRF_SND },           // 10 YM2610 Samples
	{ "h893",			0x100000, 0x32513b63, 6 | BRF_SND },           // 11
};

STD_ROM_PICK(crshrace2)
STD_ROM_FN(crshrace2)

struct BurnDriver BurnDrvCrshrace2 = {
	"crshrace2", "crshrace", NULL, NULL, "1993",
	"Lethal Crash Race (set 2)\0", NULL, "Video System Co.", "Miscellaneous",
	L"Lethal Crash Race\0\u7206\u70C8 \u30AF\u30E9\u30C3\u30B7\u30E5 \u30EC\u30FC\u30B9 (set 2)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, crshrace2RomInfo, crshrace2RomName, NULL, NULL, NULL, NULL,  CrshraceInputInfo,  CrshraceDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 320, 3, 4
};
