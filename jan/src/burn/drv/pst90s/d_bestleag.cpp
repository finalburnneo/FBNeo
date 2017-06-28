// FB Alpha Best League (Big Striker bootleg) driver module
// Based on MAME driver by David Haywood & Angelo Salese

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvTxRAM;
static UINT8 *DrvSprRAM;

static UINT8 DrvRecalc;

static UINT8 okibank;
static UINT16 scroll[8];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo BestleagInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bestleag)

static struct BurnDIPInfo BestleagDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    11, "Coin A"		},
	{0x13, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    11, "Coin B"		},
	{0x13, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0xf0, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x06, 0x02, "Easy"			},
	{0x14, 0x01, 0x06, 0x06, "Normal"		},
	{0x14, 0x01, 0x06, 0x04, "Hard"			},
	{0x14, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Timer Speed"		},
	{0x14, 0x01, 0x18, 0x08, "Slow"			},
	{0x14, 0x01, 0x18, 0x18, "Normal"		},
	{0x14, 0x01, 0x18, 0x10, "Fast"			},
	{0x14, 0x01, 0x18, 0x00, "Fastest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x20, 0x00, "Off"			},
	{0x14, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "2 Players Game"	},
	{0x14, 0x01, 0x40, 0x40, "1 Credit"		},
	{0x14, 0x01, 0x40, 0x00, "2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Bestleag)

static void oki_bankswitch(UINT32 data)
{
	data--;

	if (data > 2) return;

	okibank = data + 1;

	MSM6295SetBank(0, MSM6295ROM + 0x20000 + (data * 0x20000), 0x20000, 0x3ffff);
}

static void __fastcall bestleag_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x0f8000:
		case 0x0f8002:
		case 0x0f8004:
		case 0x0f8006:
		case 0x0f8008:
		case 0x0f800a:
			scroll[(address / 2) & 7] = data;
		return;
	}
}

static void __fastcall bestleag_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x30001d:
			oki_bankswitch(data);
		return;

		case 0x30001f:
			MSM6295Command(0, data);
		return;

		case 0x304000:
		return; // nop
	}
}

static UINT16 __fastcall bestleag_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x300010:
			return DrvInputs[0];

		case 0x300012:
			return DrvInputs[1];

		case 0x300014:
			return DrvInputs[2];

		case 0x300016:
			return DrvDips[0];

		case 0x300018:
			return DrvDips[1];
	}

	return 0;
}

static UINT8 __fastcall bestleag_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x30001f:
			return MSM6295ReadStatus(0);
	}

	return 0;
}

static tilemap_scan( tilemap )
{
	int offset;

	offset = ((col&0xf)*16) + (row&0xf);
	offset += (col >> 4) * 0x100;
	offset += (row >> 4) * 0x800;

	return offset;
}

static tilemap_callback( text )
{
	UINT16 *ram = (UINT16*)DrvTxRAM;

	UINT16 code = ram[offs];

	TILE_SET_INFO(0, code & 0xfff, code >> 12, 0);
}

static tilemap_callback( background )
{
	UINT16 *ram = (UINT16*)DrvBgRAM;

	UINT16 code = ram[offs];

	TILE_SET_INFO(1, code & 0xfff, code >> 12, 0);
}

static tilemap_callback( foreground )
{
	UINT16 *ram = (UINT16*)DrvFgRAM;

	UINT16 code = ram[offs];

	TILE_SET_INFO(2, code & 0xfff, code >> 12, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset(0);
	oki_bankswitch(0);

	memset (scroll, 0, 8 * sizeof(INT16));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x040000;

	DrvGfxROM0	= Next; Next += 0x040000;
	DrvGfxROM1	= Next; Next += 0x200000;
	DrvGfxROM2	= Next; Next += 0x100000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x080000;

	BurnPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x020000;
	BurnPalRAM	= Next; Next += 0x001000;
	DrvBgRAM	= Next; Next += 0x004000;
	DrvFgRAM	= Next; Next += 0x004000;
	DrvTxRAM	= Next; Next += 0x004000;
	DrvSprRAM	= Next; Next += 0x001000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { (0x40000 * 8) * 3, (0x40000 * 8) * 2, (0x40000 * 8) * 1, (0x40000 * 8) * 0 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[16] = { STEP16(0,8) };
	INT32 YOffs0[8] = { 0, 16, 32, 48, 8, 24, 40, 56 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp + 0x000000, DrvGfxROM1 + 0x040000, 0x040000);
	memcpy (tmp + 0x040000, DrvGfxROM1 + 0x0c0000, 0x040000);
	memcpy (tmp + 0x080000, DrvGfxROM1 + 0x140000, 0x040000);
	memcpy (tmp + 0x0c0000, DrvGfxROM1 + 0x1c0000, 0x040000);

	GfxDecode(0x1000, 4,  8,  8, Plane, XOffs, YOffs0, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp + 0x000000, DrvGfxROM1 + 0x000000, 0x040000);
	memcpy (tmp + 0x040000, DrvGfxROM1 + 0x080000, 0x040000);
	memcpy (tmp + 0x080000, DrvGfxROM1 + 0x100000, 0x040000);
	memcpy (tmp + 0x0c0000, DrvGfxROM1 + 0x180000, 0x040000);

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x100000);

	GfxDecode(0x1000, 4, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x180000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x040000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0c0000,  9, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 10, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvBgRAM,		0x0e0000, 0x0e3fff, MAP_RAM);
	SekMapMemory(DrvFgRAM,		0x0e8000, 0x0ebfff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x0f0000, 0x0f3fff, MAP_RAM);
	SekMapMemory(BurnPalRAM,	0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x200000, 0x200fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xfe0000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,	bestleag_write_word);
	SekSetWriteByteHandler(0,	bestleag_write_byte);
	SekSetReadWordHandler(0,	bestleag_read_word);
	SekSetReadByteHandler(0,	bestleag_read_byte);
	SekClose();

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, text_map_callback,        8,  8, 256, 32);
	GenericTilemapInit(1, tilemap_map_scan,  background_map_callback, 16, 16, 128, 64);
	GenericTilemapInit(2, tilemap_map_scan,  foreground_map_callback, 16, 16, 128, 64);
	GenericTilemapSetGfx(0, DrvGfxROM0 + 0x000000, 4,  8,  8, 0x040000, 0x200, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1 + 0x000000, 4, 16, 16, 0x100000, 0x000, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM1 + 0x100000, 4, 16, 16, 0x100000, 0x100, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetTransparent(2, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);
	SekExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void draw_single_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	if (flipy) {
		if (flipx) {
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0xf, 0x300, DrvGfxROM2);
		} else {
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0xf, 0x300, DrvGfxROM2);
		}
	} else {
		if (flipx) {
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0xf, 0x300, DrvGfxROM2);
		} else {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0xf, 0x300, DrvGfxROM2);
		}
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	INT32 color_mask = (scroll[0] & 0x1000) ? 0x7 : 0xf;

	for (INT32 offs = 0x16/2;offs < 0x1000/2;offs += 4)
	{
		if (ram[offs] & 0x2000) break;

		INT32 code  = (ram[offs+3] & 0xfff);
		INT32 color = (ram[offs+2] >> 12) & color_mask;
		INT32 sx    = (ram[offs+2] & 0x1ff) - 20;
		INT32 sy    = (0xff - (ram[offs] & 0xff)) - (15 + 16);
		INT32 flipx = (ram[offs] & 0x4000) >> 14;

		draw_single_sprite(code+0, color, flipx ? (sx+16) : (sx), sy, flipx, 0);
		draw_single_sprite(code+1, color, flipx ? (sx) : (sx+16), sy, flipx, 0);
		draw_single_sprite(code+0, color, flipx ? (sx+16 - 512) : (sx - 512), sy, flipx, 0);
		draw_single_sprite(code+1, color, flipx ? (sx - 512) : (sx+16 - 512), sy, flipx, 0);
	}
}

static INT32 BestleagDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_RRRRGGGGBBBBRGBx();
		DrvRecalc = 1;
	}

	GenericTilemapSetScrollX(1, (scroll[0] & 0xfff) + ((scroll[4] & 7) - 3));
	GenericTilemapSetScrollY(1, scroll[1]);
	GenericTilemapSetScrollX(0, scroll[2]);
	GenericTilemapSetScrollY(0, scroll[3]);
	GenericTilemapSetScrollX(2, scroll[4] & 0xfff8);
	GenericTilemapSetScrollY(2, scroll[5]);

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(1, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(2, pTransDraw, 0);
	if (nBurnLayer & 4) draw_sprites();
	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 BestleawDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_RRRRGGGGBBBBRGBx();
		DrvRecalc = 1;
	}

	GenericTilemapSetScrollX(1, scroll[4]);
	GenericTilemapSetScrollY(1, scroll[5]);
	GenericTilemapSetScrollX(0, scroll[0]);
	GenericTilemapSetScrollY(0, scroll[1]);
	GenericTilemapSetScrollX(2, scroll[2]);
	GenericTilemapSetScrollY(2, scroll[3]);

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(1, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(2, pTransDraw, 0);
	if (nBurnLayer & 4) draw_sprites();
	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	SekOpen(0);
	SekRun(12000000 / 60);
	SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
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

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		MSM6295Scan(0, nAction);

		SCAN_VAR(scroll);
		SCAN_VAR(okibank);
	}

	if (nAction & ACB_WRITE) {
		oki_bankswitch(okibank);
	}

	return 0;
}


// Best League (bootleg of Big Striker, Italian Serie A)

static struct BurnRomInfo bestleagRomDesc[] = {
	{ "2.bin",			0x20000, 0xd2be3431, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "3.bin",			0x20000, 0xf29c613a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "4.bin",			0x80000, 0x47f7c9bc, 2 | BRF_GRA },           //  2 Layer tiles
	{ "5.bin",			0x80000, 0x6a6f499d, 2 | BRF_GRA },           //  3
	{ "6.bin",			0x80000, 0x0c3d2609, 2 | BRF_GRA },           //  4
	{ "7.bin",			0x80000, 0xdcece871, 2 | BRF_GRA },           //  5

	{ "27_27c010.u86",	0x20000, 0xa463422a, 3 | BRF_GRA },           //  6 Sprites
	{ "28_27c010.u85",	0x20000, 0xebec74ed, 3 | BRF_GRA },           //  7
	{ "29_27c010.u84",	0x20000, 0x7ea4e22d, 3 | BRF_GRA },           //  8
	{ "30_27c010.u83",	0x20000, 0x283d9ba6, 3 | BRF_GRA },           //  9

	{ "20_27c040.u16",	0x80000, 0xe152138e, 4 | BRF_SND },           // 10 MSM6295 samples
};

STD_ROM_PICK(bestleag)
STD_ROM_FN(bestleag)

struct BurnDriver BurnDrvBestleag = {
	"bestleag", "bigstrik", NULL, NULL, "1993",
	"Best League (bootleg of Big Striker, Italian Serie A)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, bestleagRomInfo, bestleagRomName, NULL, NULL, BestleagInputInfo, BestleagDIPInfo,
	DrvInit, DrvExit, DrvFrame, BestleagDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Best League (bootleg of Big Striker, World Cup)

static struct BurnRomInfo bestleawRomDesc[] = {
	{ "21_27c101.u67",	0x20000, 0xab5abd37, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "22_27c010.u66",	0x20000, 0x4abc0580, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "23_27c040.u36",	0x80000, 0xdcd53a97, 2 | BRF_GRA },           //  2 Layer tiles
	{ "24_27c040.u42",	0x80000, 0x2984c1a0, 2 | BRF_GRA },           //  3
	{ "25_27c040.u38",	0x80000, 0x8bb5d73a, 2 | BRF_GRA },           //  4
	{ "26_27c4001.u45",	0x80000, 0xa82c905d, 2 | BRF_GRA },           //  5

	{ "27_27c010.u86",	0x20000, 0xa463422a, 3 | BRF_GRA },           //  6 Sprites
	{ "28_27c010.u85",	0x20000, 0xebec74ed, 3 | BRF_GRA },           //  7
	{ "29_27c010.u84",	0x20000, 0x7ea4e22d, 3 | BRF_GRA },           //  8
	{ "30_27c010.u83",	0x20000, 0x283d9ba6, 3 | BRF_GRA },           //  9

	{ "20_27c040.u16",	0x80000, 0xe152138e, 4 | BRF_SND },           // 10 MSM6295 samples

	{ "85c060.bin",		0x0032f, 0x537100ac, 5 | BRF_OPT },           // 11 plds
	{ "gal16v8-25hb1.u182",	0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 12
	{ "gal16v8-25hb1.u183",	0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 13
	{ "gal16v8-25hb1.u58",	0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 14
	{ "palce20v8h-15pc-4.u38",0x157, 0x00000000, 5 | BRF_NODUMP | BRF_OPT },           // 15
};

STD_ROM_PICK(bestleaw)
STD_ROM_FN(bestleaw)

struct BurnDriver BurnDrvBestleaw = {
	"bestleaw", "bigstrik", NULL, NULL, "1993",
	"Best League (bootleg of Big Striker, World Cup)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, bestleawRomInfo, bestleawRomName, NULL, NULL, BestleagInputInfo, BestleagDIPInfo,
	DrvInit, DrvExit, DrvFrame, BestleawDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};
