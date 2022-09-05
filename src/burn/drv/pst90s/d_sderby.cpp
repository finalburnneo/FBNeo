// FinalBurn Neo Playmark Super Derby Hardware Driver Module
// Based on MAME driver by David Haywood, Roberto Fresca

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *Drv68KRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvMgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvNVRAM;

static UINT16 *scroll;

static INT32 sprite_priority = 0;

static UINT8 DrvJoy1[16];
static UINT8 DrvDips[1];
static UINT8 DrvReset;
static UINT16 DrvInputs[1];

static struct BurnInputInfo SderbyInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Bet",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Collect",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Sderby)

static struct BurnInputInfo SderbyaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Bet",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Collect",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	}, // extra?

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Sderbya)

static struct BurnInputInfo SpacewinInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Hold 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Hold 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Hold 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"P1 Hold 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},
	{"P1 Hold 5",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 5"	},
	{"P1 Bet",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 6"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Spacewin)

static struct BurnInputInfo ShinygldInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Slot Stop 1",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Slot Stop 2",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Slot Stop 3",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"P1 Bet",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Shinygld)

static struct BurnInputInfo LuckboomInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Luckboom)

static struct BurnDIPInfo SderbyDIPList[]=
{
	{0x09, 0xff, 0xff, 0x01, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x09, 0x01, 0x01, 0x01, "Off"				},
	{0x09, 0x01, 0x01, 0x00, "On"				},
};

STDDIPINFO(Sderby)

static struct BurnDIPInfo SderbyaDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x01, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0a, 0x01, 0x01, 0x01, "Off"				},
	{0x0a, 0x01, 0x01, 0x00, "On"				},
};

STDDIPINFO(Sderbya)

static struct BurnDIPInfo ShinygldDIPList[]=
{
	{0x07, 0xff, 0xff, 0x01, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x07, 0x01, 0x01, 0x01, "Off"				},
	{0x07, 0x01, 0x01, 0x01, "Off"				},
};

STDDIPINFO(Shinygld)

static void __fastcall sderby_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x104010 && address <= 0x105fff) return; // junk write

	switch (address)
	{
		case 0x104000: // fgx
		case 0x104002: // fgy
		case 0x104004: // mgx
		case 0x104006: // mgy
		case 0x104008: // bgx
		case 0x10400a: // bgy
			scroll[(address/2) & 7] = data;
		return;

		case 0x10400c:
		case 0x10400e:
		case 0x300000:
		case 0x308008: // lamps, coin counters
		case 0x500000:
		return;
	}

//	bprintf (0, _T("WW: %5.5x %4.4x\n"), address, data);
}

static void __fastcall sderby_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x30800f:
			MSM6295Write(0, data);
		return;
	}

//	bprintf (0, _T("WB: %5.5x %2.2x\n"), address, data);
}

static UINT16 __fastcall sderby_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x308000:
			return (DrvInputs[0] & ~0x1000) | ((DrvDips[0] & 1) << 12);

		case 0x308002: // inputs read as LONG
			return 0xffff;
	}
	
//	bprintf (0, _T("RW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall sderby_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x30800f:
			return MSM6295Read(0);
	}

//	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static tilemap_callback( bg )
{
	UINT16 *ram = (UINT16*)DrvBgRAM;
	INT32 code  = ram[offs * 2];
	INT32 color = ram[offs * 2 + 1];

	TILE_SET_INFO(1, code, color, 0);
}

static tilemap_callback( mg )
{
	UINT16 *ram = (UINT16*)DrvMgRAM;
	INT32 code  = ram[offs * 2];
	INT32 color = ram[offs * 2 + 1];

	TILE_SET_INFO(2, code, color, 0);
}

static tilemap_callback( fg )
{
	UINT16 *ram = (UINT16*)DrvFgRAM;
	INT32 code  = ram[offs * 2];
	INT32 color = ram[offs * 2 + 1];

	TILE_SET_INFO(0, code, color, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset(0);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x040000;

	DrvGfxROM0	= Next; Next += 0x200000;
	DrvGfxROM1	= Next; Next += 0x200000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x080000;

	BurnPalette	= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	DrvNVRAM	= Next; Next += 0x000800;

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x010000;
	BurnPalRAM	= Next; Next += 0x001000;
	DrvBgRAM	= Next; Next += 0x001000;
	DrvMgRAM	= Next; Next += 0x001000;
	DrvFgRAM	= Next; Next += 0x002000;
	DrvSprRAM	= Next; Next += 0x001400;

	scroll		= (UINT16*)Next; Next += 0x000006 * sizeof(UINT16);

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 sprite_size)
{
	INT32 Planes[5] = { ((sprite_size * 5) * 8 * 4) / 5, ((sprite_size * 5) * 8 * 3) / 5, ((sprite_size * 5) * 8 * 2) / 5, ((sprite_size * 5) * 8 * 1) / 5, ((sprite_size * 5) * 8 * 0) / 5 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(64,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc((sprite_size * 5));
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, (sprite_size * 5));

	GfxDecode((((sprite_size * 5) * 8) / 5) / ( 8 *  8), 5,  8,  8, Planes, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);
	GfxDecode((((sprite_size * 5) * 8) / 5) / (16 * 16), 5, 16, 16, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static void sderby_map()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvBgRAM,				0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(DrvMgRAM,				0x101000, 0x101fff, MAP_RAM);
	SekMapMemory(DrvFgRAM,				0x102000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,				0x200000, 0x2013ff, MAP_RAM);
	SekMapMemory(BurnPalRAM,			0x380000, 0x380fff, MAP_RAM);
	SekMapMemory(DrvNVRAM,				0xcf0000, 0xcf07ff, MAP_RAM); // sderbya
	SekMapMemory(Drv68KRAM,				0xcfc000, 0xcfffff, MAP_RAM); // sderbya
	SekMapMemory(DrvNVRAM,				0xd00000, 0xd007ff, MAP_RAM); // sderby
	SekMapMemory(DrvNVRAM,				0xe00000, 0xe007ff, MAP_RAM); // luckboom
	SekMapMemory(Drv68KRAM,				0xff0000, 0xffffff, MAP_RAM); // luckboom
	SekSetWriteWordHandler(0,			sderby_write_word);
	SekSetWriteByteHandler(0,			sderby_write_byte);
	SekSetReadWordHandler(0,			sderby_read_word);
	SekSetReadByteHandler(0,			sderby_read_byte);
	SekClose();
}

static void spacewin_map()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvBgRAM,				0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(DrvMgRAM,				0x101000, 0x101fff, MAP_RAM);
	SekMapMemory(DrvFgRAM,				0x102000, 0x103fff, MAP_RAM);
	SekMapMemory(BurnPalRAM,			0x380000, 0x380fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,				0x800000, 0x8013ff, MAP_RAM);
	SekMapMemory(DrvNVRAM,				0x8f0000, 0x8f07ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,				0x8fc000, 0x8fffff, MAP_RAM);
	SekMapMemory(Drv68KRAM + 0x4000,	0xd00000, 0xd003ff, MAP_RAM);
	SekSetWriteWordHandler(0,			sderby_write_word);
	SekSetWriteByteHandler(0,			sderby_write_byte);
	SekSetReadWordHandler(0,			sderby_read_word);
	SekSetReadByteHandler(0,			sderby_read_byte);
	SekClose();
}

static void shinygld_map()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvBgRAM,				0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(DrvMgRAM,				0x101000, 0x101fff, MAP_RAM);
	SekMapMemory(DrvFgRAM,				0x102000, 0x103fff, MAP_RAM);
	SekMapMemory(BurnPalRAM,			0x380000, 0x380fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,				0x400000, 0x4013ff, MAP_RAM);
	SekMapMemory(DrvNVRAM,				0x700000, 0x7007ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,				0x7f0000, 0x7fffff, MAP_RAM);
	SekSetWriteWordHandler(0,			sderby_write_word);
	SekSetWriteByteHandler(0,			sderby_write_byte);
	SekSetReadWordHandler(0,			sderby_read_word);
	SekSetReadByteHandler(0,			sderby_read_byte);
	SekClose();
}

static INT32 CommonInit(void (*pCpuMap)(), INT32 spriterom_size, INT32 sprite_layer_priority)
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + (spriterom_size * 0), k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + (spriterom_size * 1), k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + (spriterom_size * 2), k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + (spriterom_size * 3), k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + (spriterom_size * 4), k++, 1)) return 1;

		DrvGfxDecode(spriterom_size);
	}

	if (pCpuMap) pCpuMap();

	MSM6295Init(0, 1056000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM0, 5,  8,  8, ((spriterom_size * 5) * 8) / 5, 0x400, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 5, 16, 16, ((spriterom_size * 5) * 8) / 5, 0x000, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM1, 5, 16, 16, ((spriterom_size * 5) * 8) / 5, 0x200, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM1, 5, 16, 16, ((spriterom_size * 5) * 8) / 5, 0x600, 0xf);
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, mg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fg_map_callback,  8,  8, 64, 32);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -32, -24);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	sprite_priority = sprite_layer_priority;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);
	SekExit();

	BurnFreeMemIndex();

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)BurnPalRAM;
	for (INT32 i = 0; i < 0x1000/2; i++)
	{
		INT32 r = (p[i] >> 11) & 0x1f;
		INT32 g = (p[i] >>  6) & 0x1f;
		INT32 b = (p[i] >>  1) & 0x1f;

		BurnPalette[i] = BurnHighCol(pal5bit(r), pal5bit(g), pal5bit(b), 0);
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 4; offs < 0x1000 / 2; offs += 4)
	{
		INT32 sy = ram[offs + 3 - 4];
		if (sy == 0x2000) break;   // end of list marker

		INT32 flipx = sy & 0x4000;
		INT32 sx = (ram[offs + 1] & 0x01ff) - 16 - 7;
		sy = (256 - 8 - 16 - sy) & 0xff;
		INT32 code = ram[offs + 2];
		INT32 color = (ram[offs + 1] & 0x3e00) >> 9;

		DrawGfxMaskTile(0, 3, code, sx - 32, sy - 16, flipx, 0, color >> 1, 0);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteUpdate();
		BurnRecalc = 1; // force update
	}

	GenericTilemapSetScrollX(0, scroll[4] + 6);
	GenericTilemapSetScrollY(0, scroll[5] - 8);
	GenericTilemapSetScrollX(1, scroll[2] + 4);
	GenericTilemapSetScrollY(1, scroll[3] - 8);
	GenericTilemapSetScrollX(2, scroll[0] + 2);
	GenericTilemapSetScrollY(2, scroll[1] - 8);

	if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);

	if (sprite_priority == 0 && nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);

	if (sprite_priority == 1 && nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 4) GenericTilemapDraw(2, 0, 0);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}
	
	SekOpen(0);
	SekRun(12000000 / 60);
	SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x000800;
		ba.nAddress	= 0xe00000;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		MSM6295Scan(nAction, pnMin);
	}

	return 0;
}


// Super Derby (Playmark, v.07.03)

static struct BurnRomInfo sderbyRomDesc[] = {
	{ "22.bin",				0x20000, 0xa319f1e0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "23.bin",				0x20000, 0x1d6e2321, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "21.bin",				0x80000, 0x6f9f2f2b, 2 | BRF_SND },           //  2 Samples

	{ "24.bin",				0x20000, 0x93c917df, 3 | BRF_GRA },           //  3 Graphics
	{ "25.bin",				0x20000, 0x7ba485cd, 3 | BRF_GRA },           //  4
	{ "26.bin",				0x20000, 0xbeabe4f7, 3 | BRF_GRA },           //  5
	{ "27.bin",				0x20000, 0x672ce5df, 3 | BRF_GRA },           //  6
	{ "28.bin",				0x20000, 0x39ca3b52, 3 | BRF_GRA },           //  7
};

STD_ROM_PICK(sderby)
STD_ROM_FN(sderby)

static INT32 SderbyInit()
{
	return CommonInit(sderby_map, 0x20000, 0);
}

struct BurnDriver BurnDrvSderby = {
	"sderby", NULL, NULL, NULL, "1996",
	"Super Derby (Playmark, v.07.03)\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_CASINO, 0,
	NULL, sderbyRomInfo, sderbyRomName, NULL, NULL, NULL, NULL, SderbyInputInfo, SderbyDIPInfo,
	SderbyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	320, 240, 4, 3
};


// Super Derby (Playmark, v.10.04)

static struct BurnRomInfo sderbyaRomDesc[] = {
	{ "22.u16",				0x20000, 0x5baadc33, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "23.u15",				0x20000, 0x04518b8c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "274001.u147",		0x80000, 0x6f9f2f2b, 2 | BRF_SND },           //  2 Samples

	{ "24.u141",			0x40000, 0x54234f72, 3 | BRF_GRA },           //  3 Graphics
	{ "25.u142",			0x40000, 0x1bd98cf7, 3 | BRF_GRA },           //  4
	{ "26.u143",			0x40000, 0x21bf2004, 3 | BRF_GRA },           //  5
	{ "27.u144",			0x40000, 0x097e0b27, 3 | BRF_GRA },           //  6
	{ "28.u145",			0x40000, 0x580daff7, 3 | BRF_GRA },           //  7
};

STD_ROM_PICK(sderbya)
STD_ROM_FN(sderbya)

static INT32 SderbyaInit()
{
	return CommonInit(sderby_map, 0x40000, 0);
}

struct BurnDriver BurnDrvSderbya = {
	"sderbya", "sderby", NULL, NULL, "1996",
	"Super Derby (Playmark, v.10.04)\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_CASINO, 0,
	NULL, sderbyaRomInfo, sderbyaRomName, NULL, NULL, NULL, NULL, SderbyaInputInfo, SderbyaDIPInfo,
	SderbyaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	320, 240, 4, 3
};


// Scacco Matto / Space Win

static struct BurnRomInfo spacewinRomDesc[] = {
	{ "2.u16",				0x20000, 0x2d17c2ab, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "3.u15",				0x20000, 0xfd6f93ef, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.u147",				0x40000, 0xeedc7090, 2 | BRF_SND },           //  2 Samples

	{ "4.u141",				0x20000, 0xce20c599, 3 | BRF_GRA },           //  3 Graphics
	{ "5.u142",				0x20000, 0xae4f8e06, 3 | BRF_GRA },           //  4
	{ "6.u143",				0x20000, 0xb99afc96, 3 | BRF_GRA },           //  5
	{ "7.u144",				0x20000, 0x30f212ad, 3 | BRF_GRA },           //  6
	{ "8.u145",				0x20000, 0x541a73fd, 3 | BRF_GRA },           //  7
};

STD_ROM_PICK(spacewin)
STD_ROM_FN(spacewin)

static INT32 SpacewinInit()
{
	return CommonInit(spacewin_map, 0x20000, 1);
}

struct BurnDriver BurnDrvSpacewin = {
	"spacewin", NULL, NULL, NULL, "1996",
	"Scacco Matto / Space Win\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_CASINO, 0,
	NULL, spacewinRomInfo, spacewinRomName, NULL, NULL, NULL, NULL, SpacewinInputInfo, SderbyDIPInfo,
	SpacewinInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	320, 240, 4, 3
};


// Shiny Golds

static struct BurnRomInfo shinygldRomDesc[] = {
	{ "16.u16",				0x20000, 0x4b08a668, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "17.u15",				0x20000, 0xeb126e19, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "15.u147",			0x40000, 0x66db6b15, 2 | BRF_SND },           //  2 Samples

	{ "18.u141",			0x20000, 0xa905c02d, 3 | BRF_GRA },           //  3 Graphics
	{ "19.u142",			0x20000, 0x8db53f3b, 3 | BRF_GRA },           //  4
	{ "20.u143",			0x20000, 0xbe54ae86, 3 | BRF_GRA },           //  5
	{ "21.u144",			0x20000, 0xde3be666, 3 | BRF_GRA },           //  6
	{ "22.u145",			0x20000, 0x113ccf81, 3 | BRF_GRA },           //  7

	{ "gal22v10.bin",		0x002e5, 0x00000000, 4 | BRF_NODUMP | BRF_OPT },           //  8 PLDs
	{ "palce22v10_1.bin",	0x002dd, 0x00000000, 4 | BRF_NODUMP | BRF_OPT },           //  9
	{ "palce22v10_2.bin",	0x002dd, 0x00000000, 4 | BRF_NODUMP | BRF_OPT },           // 10
	{ "palce22v10_3.bin",	0x002dd, 0x00000000, 4 | BRF_NODUMP | BRF_OPT },           // 11
	{ "palce16v8h.bin",		0x00117, 0x00000000, 4 | BRF_NODUMP | BRF_OPT },           // 12
};

STD_ROM_PICK(shinygld)
STD_ROM_FN(shinygld)

static INT32 ShinygldInit()
{
	return CommonInit(shinygld_map, 0x20000, 0);
}

struct BurnDriver BurnDrvShinygld = {
	"shinygld", NULL, NULL, NULL, "1996",
	"Shiny Golds\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_CASINO, 0,
	NULL, shinygldRomInfo, shinygldRomName, NULL, NULL, NULL, NULL, ShinygldInputInfo, ShinygldDIPInfo,
	ShinygldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	320, 240, 4, 3
};


// Lucky Boom

static struct BurnRomInfo luckboomRomDesc[] = {
	{ "2.u16",				0x20000, 0x0a20eaca, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "3.u15",				0x20000, 0x0d3bb24c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.u147",				0x40000, 0x0d42c0a3, 2 | BRF_SND },           //  2 Samples

	{ "4.u141",				0x20000, 0x3aeccad7, 3 | BRF_GRA },           //  3 Graphics
	{ "5.u142",				0x20000, 0x4e4f9ac6, 3 | BRF_GRA },           //  4
	{ "6.u143",				0x20000, 0xd1b4910e, 3 | BRF_GRA },           //  5
	{ "7.u144",				0x20000, 0x00334bad, 3 | BRF_GRA },           //  6
	{ "8.u145",				0x20000, 0xdc12df50, 3 | BRF_GRA },           //  7
};

STD_ROM_PICK(luckboom)
STD_ROM_FN(luckboom)

struct BurnDriver BurnDrvLuckboom = {
	"luckboom", NULL, NULL, NULL, "1996",
	"Lucky Boom\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_ACTION, 0,
	NULL, luckboomRomInfo, luckboomRomName, NULL, NULL, NULL, NULL, LuckboomInputInfo, SderbyDIPInfo,
	SderbyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	320, 240, 4, 3
};

