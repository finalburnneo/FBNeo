// FinalBurn Neo D-Con / SD Gundam Psycho Salamander no Kyoui driver module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "seibusnd.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[5];
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvMgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvTxRAM;
static UINT8 *DrvZ80RAM;

static UINT16 *scroll;

static INT32 gfx_bank;
static INT32 gfx_enable;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[3];
static UINT8 DrvReset;

static INT32 is_sdgndmps = 0;

static struct BurnInputInfo DconInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 12,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Dcon)

static struct BurnInputInfo SdgndmpsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 12,	"service"	},
	{"Service Mode",BIT_DIGITAL,	DrvJoy3 + 8,	"diag"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sdgndmps)

static struct BurnDIPInfo DconDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL					},
	{0x13, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x12, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x12, 0x01, 0x38, 0x00, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x08, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
};

STDDIPINFO(Dcon)

static struct BurnDIPInfo SdgndmpsDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL					},
	{0x14, 0xff, 0xff, 0xcf, NULL					},

	{0   , 0xfe, 0   ,   16, "Coin B"				},
	{0x13, 0x01, 0x0f, 0x04, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x0a, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x01, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x00, "5 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x02, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0x0f, 0x08, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x0f, 0x0c, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0x0f, 0x0e, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x13, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,   16, "Coin A"				},
	{0x13, 0x01, 0xf0, 0x40, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0xa0, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x20, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xf0, 0x80, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xf0, 0xc0, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0xf0, 0xe0, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x13, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x13, 0x01, 0xf0, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x14, 0x01, 0x03, 0x02, "2"					},
	{0x14, 0x01, 0x03, 0x03, "3"					},
	{0x14, 0x01, 0x03, 0x01, "4"					},
	{0x14, 0x01, 0x03, 0x00, "6"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x0c, 0x08, "Easy"					},
	{0x14, 0x01, 0x0c, 0x0c, "Normal"				},
	{0x14, 0x01, 0x0c, 0x04, "Difficult"			},
	{0x14, 0x01, 0x0c, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x10, 0x10, "Off"					},
	{0x14, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x20, 0x20, "No"					},
	{0x14, 0x01, 0x20, 0x00, "Yes"					},

};

STDDIPINFO(Sdgndmps)

static void __fastcall dcon_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffff0) == 0x0a0000) {
		seibu_main_word_write(address & 0xf, data);
		return;
	}

	if ((address & 0xfff800) == 0x09d000) {
		gfx_bank = (data & 1) << 12;
		return;
	}

	if ((address & 0xfffff0) == 0x0c0020) {
		scroll[(address & 0x0f) >> 1] = data;
		return;
	}

	if (address == 0x0c001c) {
		gfx_enable = data;
		return;
	}
}

static UINT16 __fastcall dcon_main_read_word(UINT32 address)
{
	if ((address & 0xfffff0) == 0x0a0000) {
		return seibu_main_word_read(address & 0xf);
	}

	switch (address)
	{
		case 0x0e0000:
			return (DrvDips[1] << 8) | DrvDips[0];

		case 0x0e0002:
			return DrvInputs[1];

		case 0x0e0004:
			return DrvInputs[2];

		case 0x0c001c:
			return gfx_enable;
	}

	return 0;
}

static tilemap_callback( tx )
{
	INT32 attr = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvTxRAM + offs * 2)));

	TILE_SET_INFO(0, attr, attr >> 12, 0);
}

static tilemap_callback( bg )
{
	INT32 attr = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvBgRAM + offs * 2)));

	TILE_SET_INFO(1, attr, attr >> 12, 0);
}

static tilemap_callback( mg )
{
	INT32 attr = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvMgRAM + offs * 2)));

	TILE_SET_INFO(2, (attr & 0xfff) | gfx_bank, attr >> 12, 0);
}

static tilemap_callback( fg )
{
	INT32 attr = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvFgRAM + offs * 2)));

	TILE_SET_INFO(3, attr, attr >> 12, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekReset(0);

	seibu_sound_reset();

	gfx_bank = 0;
	gfx_enable = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;
	SeibuZ80ROM		= Next;
	DrvZ80ROM		= Next; Next += 0x020000;

	DrvGfxROM[0]	= Next; Next += 0x040000;
	DrvGfxROM[1]	= Next; Next += 0x100000;
	DrvGfxROM[2]	= Next; Next += 0x100000;
	DrvGfxROM[3]	= Next; Next += 0x200000;
	DrvGfxROM[4]	= Next; Next += 0x400000;

	BurnPalette		= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x040000;

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x000800;
	DrvBgRAM		= Next; Next += 0x000800;
	DrvMgRAM		= Next; Next += 0x000800;
	DrvFgRAM		= Next; Next += 0x000800;
	DrvTxRAM		= Next; Next += 0x001000;
	Drv68KRAM		= Next; Next += 0x00c000;
	BurnPalRAM		= Next; Next += 0x001000;

	scroll			= (UINT16*)Next; Next += 0x000010;

	SeibuZ80RAM		= Next;
	DrvZ80RAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0x00000, 0x00004, 0x80000, 0x80004 };
	INT32 Plane1[4]  = { 0x008, 0x00c, 0x000, 0x004 };
	INT32 XOffs0[8]  = { STEP4(3,-1), STEP4(8+3,-1) };
	INT32 XOffs1[16] = { STEP4(3,-1), STEP4(16+3,-1), STEP4(512+3,-1), STEP4(512+16+3,-1) };
	INT32 YOffs0[8]  = { STEP8(0,16) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x020000);

	GfxDecode(0x1000, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x80000);

	GfxDecode(0x1000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x80000);

	GfxDecode(0x1000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM[2]);

	memcpy (tmp, DrvGfxROM[3], 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM[3]);

	memcpy (tmp, DrvGfxROM[4], 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM[4]);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	is_sdgndmps = strcmp(BurnDrvGetTextA(DRV_NAME), "sdgndmps") == 0;

	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040000, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000, k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0x10000, DrvZ80ROM + 0x08000, 0x08000);
		memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x08000, 0x08000);

		if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x010000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[3] + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[4] + 0x000000, k++, 1)) return 1;

		if (is_sdgndmps) {
			if (BurnLoadRom(DrvGfxROM[4] + 0x100000, k++, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvGfxROM[4] + 0x080000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM[4] + 0x100000, k++, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM[4] + 0x180000, k++, 1)) return 1;
		}

		if (BurnLoadRom(DrvSndROM  + 0x000000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x080000, 0x08bfff, MAP_RAM);
	SekMapMemory(DrvBgRAM,		0x08c000, 0x08c7ff, MAP_RAM);
	SekMapMemory(DrvFgRAM,		0x08c800, 0x08cfff, MAP_RAM);
	SekMapMemory(DrvMgRAM,		0x08d000, 0x08d7ff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x08d800, 0x08e7ff, MAP_RAM);
	SekMapMemory(BurnPalRAM,	0x08e800, 0x08f7ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x08f800, 0x08ffff, MAP_RAM);
	SekSetWriteWordHandler(0,	dcon_main_write_word);
	SekSetReadWordHandler(0,	dcon_main_read_word);
	SekClose();

	seibu_sound_init(is_sdgndmps, 0, 3579545, 3579545, 1320000/132);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, tx_map_callback,  8,  8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, mg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, fg_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x040000, 0x700, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x100000, 0x400, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[3], 4, 16, 16, 0x200000, 0x500, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM[2], 4, 16, 16, 0x100000, 0x600, 0xf);
	GenericTilemapSetGfx(4, DrvGfxROM[4], 4, 16, 16, 0x400000, 0x000, 0x3f);
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetTransparent(2, 0xf);
	GenericTilemapSetTransparent(3, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, (is_sdgndmps) ? -128 : 0, (is_sdgndmps) ? -16 : 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();

	seibu_sound_exit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites()
{
	INT32 yoffset = is_sdgndmps ? 16 : 0;
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0; offs < 0x400; offs += 4)
	{
		INT32 attr = BURN_ENDIAN_SWAP_INT16(ram[offs]);
		if (~attr & 0x8000) continue;

		INT32 sprite = BURN_ENDIAN_SWAP_INT16(ram[offs+1]);

		INT32 primask = 0;

		switch ((sprite >> 14) & 3) {
			case 0: primask = 0xf0; break;
			case 1: primask = 0xfc; break;
			case 2: primask = 0xfe; break;
			case 3: primask = 0x00; break;
		}

		INT32 y = BURN_ENDIAN_SWAP_INT16(ram[offs+3]);
		INT32 x = BURN_ENDIAN_SWAP_INT16(ram[offs+2]);

		INT32 color = attr & 0x003f;
		INT32 flipx = attr & 0x4000;
		INT32 flipy = attr & 0x2000;
		INT32 dy    = (attr & 0x0380) >>  7;
		INT32 dx    = (attr & 0x1c00) >> 10;

		x = ((x & 0x8000) ? (0 - (0x200 - (x & 0x1ff))) : (x & 0x1ff));
		y = ((y & 0x8000) ? (0 - (0x200 - (y & 0x1ff))) : (y & 0x1ff)) - yoffset;

		sprite &= 0x3fff;

		for (INT32 ax = 0; ax <= dx; ax++)
		{
			for (INT32 ay = 0; ay <= dy; ay++)
			{
				INT32 sx = x + (flipx ? (dx - ax) : ax) * 16;
				INT32 sy = y + (flipy ? (dy - ay) : ay) * 16;

				RenderPrioSprite(pTransDraw, DrvGfxROM[4], sprite, color << 4, 0xf, sx, sy, flipx, flipy, 16, 16, primask);
				RenderPrioSprite(pTransDraw, DrvGfxROM[4], sprite, color << 4, 0xf, sx, sy + 512, flipx, flipy, 16, 16, primask);
				RenderPrioSprite(pTransDraw, DrvGfxROM[4], sprite, color << 4, 0xf, sx, sy - 512, flipx, flipy, 16, 16, primask);
				sprite = (sprite + 1) & 0x3fff;
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		BurnPaletteUpdate_xBBBBBGGGGGRRRRR();
		BurnRecalc = 1; // force update
	}

	GenericTilemapSetEnable(0, nBurnLayer & 8);

	GenericTilemapSetScrollX(1, scroll[0]);
	GenericTilemapSetScrollY(1, scroll[1]);
	GenericTilemapSetEnable(1, (~gfx_enable & 1) && (nBurnLayer & 1));

	GenericTilemapSetScrollX(2, scroll[2]);
	GenericTilemapSetScrollY(2, scroll[3]);
	GenericTilemapSetEnable(2, (~gfx_enable & 2) && (nBurnLayer & 2));

	GenericTilemapSetScrollX(3, scroll[4]);
	GenericTilemapSetScrollY(3, scroll[5]);
	GenericTilemapSetEnable(3, (~gfx_enable & 4) && (nBurnLayer & 2));

	BurnTransferClear(0xf);

	GenericTilemapDraw(1, 0, 0);
	GenericTilemapDraw(2, 0, 1);
	GenericTilemapDraw(3, 0, 2);
	GenericTilemapDraw(0, 0, 4);

	if (nSpriteEnable & 8) draw_sprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		seibu_coin_input = (DrvJoy1[1] << 1) | DrvJoy1[0];
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 10000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (i == 255) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		if (is_sdgndmps) {
			CPU_RUN_TIMER(1);
		} else {
			CPU_RUN_TIMER_YM3812(1);
		}
	}

	if (pBurnSoundOut) {
		seibu_sound_update(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029706;
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

		seibu_sound_scan(nAction,pnMin);

		SCAN_VAR(gfx_bank);
		SCAN_VAR(gfx_enable);
	}

	return 0;
}


// D-Con

static struct BurnRomInfo dconRomDesc[] = {
	{ "p0-0",	 0x020000, 0xa767ec15, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "p0-1",	 0x020000, 0xa7efa091, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p1-0",	 0x020000, 0x3ec1ef7d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p1-1",	 0x020000, 0x4b8de320, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fmsnd",	 0x010000, 0x50450faa, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "fix0",	 0x010000, 0xab30061f, 3 | BRF_GRA },           //  5 Characters
	{ "fix1",	 0x010000, 0xa0582115, 3 | BRF_GRA },           //  6

	{ "bg1",	 0x080000, 0xeac43283, 4 | BRF_GRA },           //  7 Background Tiles

	{ "bg3",	 0x080000, 0x1408a1e0, 5 | BRF_GRA },           //  8 Foreground Tiles

	{ "bg2",	 0x080000, 0x01864eb6, 6 | BRF_GRA },           //  9 Midground Tiles

	{ "obj0",	 0x080000, 0xc3af37db, 7 | BRF_GRA },           // 10 Sprites
	{ "obj1",	 0x080000, 0xbe1f53ba, 7 | BRF_GRA },           // 11
	{ "obj2",	 0x080000, 0x24e0b51c, 7 | BRF_GRA },           // 12
	{ "obj3",	 0x080000, 0x5274f02d, 7 | BRF_GRA },           // 13

	{ "pcm",	 0x020000, 0xd2133b85, 8 | BRF_SND },           // 14 OKIM6295 Samples
};

STD_ROM_PICK(dcon)
STD_ROM_FN(dcon)

struct BurnDriver BurnDrvDcon = {
	"dcon", NULL, NULL, NULL, "1992",
	"D-Con\0", NULL, "Success", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, dconRomInfo, dconRomName, NULL, NULL, NULL, NULL, DconInputInfo, DconDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	320, 224, 4, 3
};


// SD Gundam Psycho Salamander no Kyoui

static struct BurnRomInfo sdgndmpsRomDesc[] = {
	{ "911-a01.25",	 0x020000, 0x3362915d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "911-a02.29",	 0x020000, 0xfbc78285, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "911-a03.27",	 0x020000, 0x6c24b4f2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "911-a04.28",	 0x020000, 0x6ff9d716, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "911-a05.010", 0x010000, 0x90455406, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "911-a08.66",	 0x010000, 0xe7e04823, 3 | BRF_GRA },           //  5 Characters
	{ "911-a07.73",	 0x010000, 0x6f40d4a9, 3 | BRF_GRA },           //  6

	{ "911-a12.63",	 0x080000, 0x8976bbb6, 4 | BRF_GRA },           //  7 Background Tiles

	{ "911-a11.65",	 0x080000, 0x3f3b7810, 5 | BRF_GRA },           //  8 Foreground Tiles

	{ "911-a13.64",	 0x100000, 0xf38a584a, 6 | BRF_GRA },           //  9 Midground Tiles

	{ "911-a10.73",	 0x100000, 0x80e341fb, 7 | BRF_GRA },           // 10 Sprites
	{ "911-a09.74",	 0x100000, 0x98f34519, 7 | BRF_GRA },           // 11

	{ "911-a06.97",	 0x040000, 0x12c79440, 8 | BRF_SND },           // 12 OKIM6295 Samples

	{ "bnd-007.88",	 0x000200, 0x96f7646e, 0 | BRF_OPT },           // 13 Priority Prom (not used)
};

STD_ROM_PICK(sdgndmps)
STD_ROM_FN(sdgndmps)

struct BurnDriver BurnDrvSdgndmps = {
	"sdgndmps", NULL, NULL, NULL, "1991",
	"SD Gundam Psycho Salamander no Kyoui\0", NULL, "Banpresto / Bandai", "Miscellaneous",
	L"\u30AC\u30F3\u30C0\u30E0 \u30B5\u30A4\u30B3\u30B5\u30E9\u30DE\uF303\u30C0\u30FC\u306E\u8105\u5A01\0SD Gundam Psycho Salamander no Kyoui\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, sdgndmpsRomInfo, sdgndmpsRomName, NULL, NULL, NULL, NULL, SdgndmpsInputInfo, SdgndmpsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	320, 224, 4, 3
};
