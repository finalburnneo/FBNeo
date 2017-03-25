// FB Alpha Big Striker bootleg Hardware
// Based on MAME driver by David Haywood

// note:
//	the dots in place of the title are normal
//	note the glitches on the black on boot are normal

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
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *Drv68KRAM0;
static UINT8 *Drv68KRAM1;
static UINT8 *Drv68KRAM2;
static UINT8 *Drv68KRAM3;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvTxRAM;
static UINT8 *DrvSprRAM;

static UINT16 scroll[2][2];

static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static struct BurnInputInfo BigstrkbInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bigstrkb)

static struct BurnDIPInfo BigstrkbDIPList[]=
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

	{0   , 0xfe, 0   ,    2, "2 Players Game"	},
	{0x14, 0x01, 0x40, 0x00, "1 Credit"		},
	{0x14, 0x01, 0x40, 0x40, "2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Bigstrkb)

static void __fastcall bigstrkb_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x700020:
			scroll[0][0] = data;
		return;

		case 0x700022:
			scroll[0][1] = data;
		return;

		case 0x700030:
			scroll[1][0] = data;
		return;

		case 0x700032:
			scroll[1][1] = data;
		return;

		case 0xe00000:
			MSM6295Command(0, data);
		return;

		case 0xe00002:
			MSM6295Command(1, data);
		return;

	}
}

static void __fastcall bigstrkb_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("%5.5x, %2.2x\n"), address,data);
}

static UINT16 __fastcall bigstrkb_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x700000:
			return DrvDips[0];

		case 0x700002:
			return DrvDips[1];

		case 0x700004:
			return DrvInputs[0];

		case 0x70000a:
			return DrvInputs[2];

		case 0x70000c:
			return DrvInputs[1];

		case 0xe00000:
			return MSM6295ReadStatus(0);

		case 0xe00002:
			return MSM6295ReadStatus(1);
	}


	bprintf (0, _T("Read unmapped word: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall bigstrkb_read_byte(UINT32 address)
{
	bprintf (0, _T("Read unmapped byte: %5.5x\n"), address);

	return 0;
}

static tilemap_scan( layer )
{
	static INT32 offset;

	offset = ((col & 0xf) * 16) + (row & 0xf);
	offset += (col >> 4) * 0x100;
	offset += (row >> 4) * 0x800;

	return offset;
}

static tilemap_callback( tx )
{
	UINT16 *ram = (UINT16*)DrvTxRAM;

	INT32 code = ram[offs];

	TILE_SET_INFO(0, code & 0xfff, code >> 12, 0);
}

static tilemap_callback( fg )
{
	UINT16 *ram = (UINT16*)DrvFgRAM;

	INT32 code = ram[offs];

	TILE_SET_INFO(1, code & 0xfff, code >> 12, 0);
}

static tilemap_callback( bg )
{
	UINT16 *ram = (UINT16*)DrvBgRAM;

	INT32 code = ram[offs];

	TILE_SET_INFO(2, code & 0xfff, code >> 12, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset(0);

	memset (scroll, 0, 2 * 2 * sizeof(UINT16));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x080000;

	DrvGfxROM0	= Next; Next += 0x080000;
	DrvGfxROM1	= Next; Next += 0x400000;
	DrvGfxROM2	= Next; Next += 0x100000;

	MSM6295ROM	= Next;
	DrvSndROM0	= Next; Next += 0x040000;
	DrvSndROM1	= Next; Next += 0x040000;

	BurnPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM0	= Next; Next += 0x010000;
	Drv68KRAM1	= Next; Next += 0x010000;
	Drv68KRAM2	= Next; Next += 0x010000;
	Drv68KRAM3	= Next; Next += 0x100000;
	DrvFgRAM	= Next; Next += 0x004000;
	DrvBgRAM	= Next; Next += 0x004000;
	DrvTxRAM	= Next; Next += 0x004000;

	BurnPalRAM	= Drv68KRAM1 + 0x8000;
	DrvSprRAM	= Drv68KRAM2 + 0x8000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { (0x10000 * 8 * 3), (0x10000 * 8 * 2), (0x10000 * 8 * 1), (0x10000 * 8 * 0) };
	INT32 Plane1[4] = { (0x80000 * 8 * 3), (0x80000 * 8 * 2), (0x80000 * 8 * 1), (0x80000 * 8 * 0) };
	INT32 Plane2[4] = { (0x20000 * 8 * 3), (0x20000 * 8 * 2), (0x20000 * 8 * 1), (0x20000 * 8 * 0) };
	INT32 XOffs[16] = { STEP16(0, 1) };
	INT32 YOffs0[8] = { STEP8(0, 8) };
	INT32 YOffs1[16] = { STEP16(0, 16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x040000);

	GfxDecode(0x2000, 4,  8,  8, Plane0, XOffs, YOffs0, 0x040, tmp, DrvGfxROM0);

	for (INT32 i = 0; i < 0x200000; i++) tmp[i] = DrvGfxROM1[i] ^ 0xff; //memcpy (tmp, DrvGfxROM1, 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane1, XOffs, YOffs1, 0x100, tmp, DrvGfxROM1);

	for (INT32 i = 0; i < 0x100000; i++) tmp[i] = DrvGfxROM2[i] ^ 0xff; //memcpy (tmp, DrvGfxROM2, 0x080000);

	GfxDecode(0x1000, 4, 16, 16, Plane2, XOffs, YOffs1, 0x100, tmp, DrvGfxROM2);

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
		if (BurnLoadRom(Drv68KROM  + 0x000000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000001,  1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x030000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x010000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x180000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x060000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x040000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x020000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 13, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 14, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 15, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM0,	0x0d0000, 0x0dffff, MAP_RAM);
	SekMapMemory(DrvFgRAM,		0x0e0000, 0x0e3fff, MAP_RAM);
	SekMapMemory(DrvBgRAM,		0x0e8000, 0x0ebfff, MAP_RAM);
	SekMapMemory(DrvTxRAM,		0x0ec000, 0x0effff, MAP_RAM);
	SekMapMemory(Drv68KRAM1,	0x0f0000, 0x0fffff, MAP_RAM);
//	SekMapMemory(BurnPalRAM,	0x0f8000, 0x0f87ff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,	0x1f0000, 0x1fffff, MAP_RAM);
//	SekMapMemory(DrvSprRAM,		0x1f8000, 0x1f87ff, MAP_RAM);
	SekMapMemory(Drv68KRAM3,	0xf00000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,	bigstrkb_write_word);
	SekSetWriteByteHandler(0,	bigstrkb_write_byte);
	SekSetReadWordHandler(0,	bigstrkb_read_word);
	SekSetReadByteHandler(0,	bigstrkb_read_byte);
	SekClose();

	MSM6295Init(0, 4000000 / 132, 1);
	MSM6295Init(1, 4000000 / 132, 1);
	MSM6295SetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.30, BURN_SND_ROUTE_BOTH);
	MSM6295SetBank(0, DrvSndROM0, 0, 0x3ffff);
	MSM6295SetBank(1, DrvSndROM1, 0, 0x3ffff);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, tx_map_callback,  8,  8,  64, 32);
	GenericTilemapInit(1, layer_map_scan,    bg_map_callback, 16, 16, 128, 64);
	GenericTilemapInit(2, layer_map_scan,    fg_map_callback, 16, 16, 128, 64);
	GenericTilemapSetGfx(0, DrvGfxROM0 + 0x000000, 4,  8,  8, 0x080000, 0x200, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1 + 0x000000, 4, 16, 16, 0x200000, 0x000, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM1 + 0x200000, 4, 16, 16, 0x200000, 0x100, 0xf);
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);
	MSM6295Exit(1);
	SekExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 i = 0; i < 0x800 / 2; i+=8)
	{
		INT32 code  = ram[i + 0] & 0x0fff;
		INT32 attr  = ram[i + 1];
		INT32 sx    = ram[i + 2] - 126;
		INT32 sy    =(ram[i + 3] ^ 0xffff) - 32;
		INT32 flipx = attr & 0x0100;
		INT32 color = attr & 0x000f;

		if (flipx) {
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0xf, 0x300, DrvGfxROM2);
		} else {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0xf, 0x300, DrvGfxROM2);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_RRRRGGGGBBBBRGBx();
		DrvRecalc = 1;
	}

	GenericTilemapSetScrollX(2, scroll[0][0] + (256 - 14));
	GenericTilemapSetScrollY(2, scroll[1][0]);
	GenericTilemapSetScrollX(1, scroll[0][1] + (256 - 14));
	GenericTilemapSetScrollY(1, scroll[1][1]);

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	if ((nBurnLayer & 1) == 1) GenericTilemapDraw(2, pTransDraw, 0);
	if ((nBurnLayer & 2) == 2) GenericTilemapDraw(1, pTransDraw, 0);
	if ((nBurnLayer & 4) == 4) draw_sprites();
	if ((nBurnLayer & 8) == 8) GenericTilemapDraw(0, pTransDraw, 0);

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

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	UINT32 nCycles = 12000000 / 60;

	SekOpen(0);
	SekRun((nCycles * 240)/256);
	SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
	SekRun((nCycles * 16 )/256);
	SekClose();

	if (pBurnSoundOut) {
		memset (pBurnSoundOut, 0, nBurnSoundLen * 2 * sizeof(INT16));
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
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
		MSM6295Scan(1, nAction);

		SCAN_VAR(scroll);
	}

	return 0;
}


// Big Striker (bootleg)

static struct BurnRomInfo bigstrkbRomDesc[] = {
	{ "footgaa.015",	0x40000, 0x33b1d7f3, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "footgaa.016",	0x40000, 0x1c6b8709, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "footgaa.005",	0x10000, 0xd97c9bfe, 2 | BRF_GRA },           //  2 Characters
	{ "footgaa.006",	0x10000, 0x1ae56e8b, 2 | BRF_GRA },           //  3
	{ "footgaa.007",	0x10000, 0xa45fa6b6, 2 | BRF_GRA },           //  4
	{ "footgaa.008",	0x10000, 0x2700888c, 2 | BRF_GRA },           //  5

	{ "footgaa.001",	0x80000, 0x0e440841, 3 | BRF_GRA },           //  6 Tiles
	{ "footgaa.002",	0x80000, 0x92a15164, 3 | BRF_GRA },           //  7
	{ "footgaa.003",	0x80000, 0xda127b89, 3 | BRF_GRA },           //  8
	{ "footgaa.004",	0x80000, 0x3e6b0d92, 3 | BRF_GRA },           //  9

	{ "footgaa.011",	0x20000, 0xc3924fea, 4 | BRF_GRA },           // 10 Sprites
	{ "footgaa.012",	0x20000, 0xa581e9d7, 4 | BRF_GRA },           // 11
	{ "footgaa.013",	0x20000, 0x26ce4b7f, 4 | BRF_GRA },           // 12
	{ "footgaa.014",	0x20000, 0xc3cfc500, 4 | BRF_GRA },           // 13

	{ "footgaa.010",	0x40000, 0x53014576, 5 | BRF_SND },           // 14 MSM6295 #0 Samples

	{ "footgaa.009",	0x40000, 0x19bf0896, 6 | BRF_SND },           // 15 MSM6295 #1 Samples
};

STD_ROM_PICK(bigstrkb)
STD_ROM_FN(bigstrkb)

struct BurnDriver BurnDrvBigstrkb = {
	"bigstrkb", "bigstrik", NULL, NULL, "1992",
	"Big Striker (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, bigstrkbRomInfo, bigstrkbRomName, NULL, NULL, BigstrkbInputInfo, BigstrkbDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Big Striker (bootleg w/Italian teams)

static struct BurnRomInfo bigstrkbaRomDesc[] = {
	{ "16.cpu17",		0x40000, 0x3ba6997b, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "15.cpu16",		0x40000, 0x204551b5, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "5.bin",		0x10000, 0xf51ea151, 2 | BRF_GRA },           //  2 Characters
	{ "6.bin",		0x10000, 0x754d750e, 2 | BRF_GRA },           //  3
	{ "7.bin",		0x10000, 0xfbc52546, 2 | BRF_GRA },           //  4
	{ "8.bin",		0x10000, 0x62c63eaa, 2 | BRF_GRA },           //  5

	{ "1.bin",		0x80000, 0xc4eb9746, 3 | BRF_GRA },           //  6 Tiles
	{ "2.bin",		0x80000, 0xaa0beb78, 3 | BRF_GRA },           //  7
	{ "3.bin",		0x80000, 0xd02298c5, 3 | BRF_GRA },           //  8
	{ "4.bin",		0x80000, 0x069ac008, 3 | BRF_GRA },           //  9

	{ "footgaa.011",	0x20000, 0xc3924fea, 4 | BRF_GRA },           // 10 Sprites
	{ "12.bin",		0x20000, 0x8e15ea09, 4 | BRF_GRA },           // 11
	{ "footgaa.013",	0x20000, 0x26ce4b7f, 4 | BRF_GRA },           // 12
	{ "footgaa.014",	0x20000, 0xc3cfc500, 4 | BRF_GRA },           // 13

	{ "footgaa.010",	0x40000, 0x53014576, 5 | BRF_SND },           // 14 MSM6295 #0 Samples

	{ "footgaa.009",	0x40000, 0x19bf0896, 6 | BRF_SND },           // 15 MSM6295 #1 Samples
};

STD_ROM_PICK(bigstrkba)
STD_ROM_FN(bigstrkba)

struct BurnDriver BurnDrvBigstrkba = {
	"bigstrkba", "bigstrik", NULL, NULL, "1992",
	"Big Striker (bootleg w/Italian teams)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, bigstrkbaRomInfo, bigstrkbaRomName, NULL, NULL, BigstrkbInputInfo, BigstrkbDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};
