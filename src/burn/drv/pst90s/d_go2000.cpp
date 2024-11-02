// FB Alpha Go 2000 driver module
// Based on MAME driver byb David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bankdata;
static UINT8 soundlatch;

static UINT16 DrvInputs[1];
static UINT8 DrvJoy1[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo Go2000InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 6"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Go2000)

static struct BurnDIPInfo Go2000DIPList[]=
{
	{0x0b, 0xff, 0xff, 0xff, NULL					},
	{0x0c, 0xff, 0xff, 0xdf, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin / Credits"		},
	{0x0b, 0x01, 0x03, 0x00, "1 Coin / 50 Credits"	},
	{0x0b, 0x01, 0x03, 0x03, "1 Coin / 100 Credits"	},
	{0x0b, 0x01, 0x03, 0x02, "1 Coin / 125 Credits"	},
	{0x0b, 0x01, 0x03, 0x01, "1 Coin / 150 Credits"	},

	{0   , 0xfe, 0   ,    4, "Minimum Coin"			},
	{0x0b, 0x01, 0x0c, 0x0c, "1"					},
	{0x0b, 0x01, 0x0c, 0x08, "2"					},
	{0x0b, 0x01, 0x0c, 0x04, "3"					},
	{0x0b, 0x01, 0x0c, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0b, 0x01, 0x80, 0x00, "On"					},
	{0x0b, 0x01, 0x80, 0x80, "Off"					},

	{0   , 0xfe, 0   ,    8, "Difficult-1"			},
	{0x0c, 0x01, 0x07, 0x07, "1"					},
	{0x0c, 0x01, 0x07, 0x06, "2"					},
	{0x0c, 0x01, 0x07, 0x05, "3"					},
	{0x0c, 0x01, 0x07, 0x04, "4"					},
	{0x0c, 0x01, 0x07, 0x03, "5"					},
	{0x0c, 0x01, 0x07, 0x02, "6"					},
	{0x0c, 0x01, 0x07, 0x01, "7"					},
	{0x0c, 0x01, 0x07, 0x00, "8"					},

	{0   , 0xfe, 0   ,    8, "M1 value"				},
	{0x0c, 0x01, 0x18, 0x08, "3000"					},
	{0x0c, 0x01, 0x18, 0x00, "3500"					},
	{0x0c, 0x01, 0x18, 0x18, "4000"					},
	{0x0c, 0x01, 0x18, 0x10, "4500"					},
	{0x0c, 0x01, 0x18, 0x08, "6000"					},
	{0x0c, 0x01, 0x18, 0x00, "7000"					},
	{0x0c, 0x01, 0x18, 0x18, "8000"					},
	{0x0c, 0x01, 0x18, 0x10, "9000"					},

	{0   , 0xfe, 0   ,    4, "Difficult-2"			},
	{0x0c, 0x01, 0xc0, 0xc0, "1"					},
	{0x0c, 0x01, 0xc0, 0x80, "2"					},
	{0x0c, 0x01, 0xc0, 0x40, "3"					},
	{0x0c, 0x01, 0xc0, 0x00, "4"					},
};

STDDIPINFO(Go2000)

static void __fastcall go2000_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x620002:
		case 0x620003:
			soundlatch = data & 0xff;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

static void __fastcall go2000_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x620002:
		case 0x620003:
			soundlatch = data;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

static UINT16 __fastcall go2000_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xa00000:
		case 0xa00001:
			return DrvInputs[0];

		case 0xa00002:
		case 0xa00003:
			return DrvDips[0] + (DrvDips[1] * 256);
	}

	return 0;
}

static UINT8 __fastcall go2000_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xa00000:
			return DrvInputs[0] >> 8;

		case 0xa00001:
			return DrvInputs[0];

		case 0xa00002:
			return DrvDips[1];

		case 0xa00003:
			return DrvDips[0];
	}

	return 0;
}

static void bankswitch(INT32 data)
{
	bankdata = data & 7;

	ZetMapMemory(DrvZ80ROM + 0x400 + (bankdata * 0x10000), 0x0400, 0xffff, MAP_ROM);
}

static void __fastcall go2000_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			DACSignedWrite(0, data);
		return;

		case 0x03:
			bankswitch(data);
		return;
	}
}

static UINT8 __fastcall go2000_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( bg )
{
	UINT16 *vram = (UINT16*)(DrvVidRAM0 + 0x0000);
	UINT16 *cram = (UINT16*)(DrvVidRAM1 + 0x0000);

	TILE_SET_INFO(0, BURN_ENDIAN_SWAP_INT16(vram[offs]), BURN_ENDIAN_SWAP_INT16(cram[offs]), 0);
}

static tilemap_callback( fg )
{
	UINT16 *vram = (UINT16*)(DrvVidRAM0 + 0x1000);
	UINT16 *cram = (UINT16*)(DrvVidRAM1 + 0x1000);

	TILE_SET_INFO(0, BURN_ENDIAN_SWAP_INT16(vram[offs]), BURN_ENDIAN_SWAP_INT16(cram[offs]), 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	DACReset();
	ZetClose();

	soundlatch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x040000;
	DrvZ80ROM	= Next; Next += 0x080000;

	DrvGfxROM	= Next; Next += 0x080000;

	DrvPalette	= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x004000;
	DrvPalRAM	= Next; Next += 0x001000;
	DrvVidRAM0	= Next; Next += 0x010000;
	DrvVidRAM1	= Next; Next += 0x010000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4] = { STEP4(0,4) };
	INT32 XOffs[8] = { STEP4(3,-1), STEP4(16+3,-1) };
	INT32 YOffs[8] = { STEP8(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x40000; i++) tmp[i] = ~DrvGfxROM[i]; // copy & invert

	GfxDecode(0x2000, 4, 8, 8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM);

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
		if (BurnLoadRom(Drv68KROM + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x00000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x00001,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x00000,  4, 2)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x203fff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,		0x600000, 0x60ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x610000, 0x61ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x800000, 0x800fff, MAP_RAM);
	SekSetWriteWordHandler(0,		go2000_main_write_word);
	SekSetWriteByteHandler(0,		go2000_main_write_byte);
	SekSetReadWordHandler(0,		go2000_main_read_word);
	SekSetReadByteHandler(0,		go2000_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0xffff, MAP_ROM);
	ZetSetOutHandler(go2000_sound_write_port);
	ZetSetInHandler(go2000_sound_read_port);
	ZetClose();

	DACInit(0, 0, 0, ZetTotalCycles, 4000000);
	DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, fg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 8, 8, 0x080000, 0, 0x7f);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	ZetExit();

	DACExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x1000/2; i++)
	{
		UINT8 r = (BURN_ENDIAN_SWAP_INT16(p[i]) >>  0) & 0x1f;
		UINT8 g = (BURN_ENDIAN_SWAP_INT16(p[i]) >>  5) & 0x1f;
		UINT8 b = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	UINT16 *m_videoram = (UINT16*)DrvVidRAM0;
	UINT16 *m_videoram2 = (UINT16*)DrvVidRAM1;

	for (INT32 offs = 0; offs < 0x400; offs += 2)
	{
		INT32 dimx, dimy, tile_xinc, tile_xstart, flipx, y0;

		INT32 y = BURN_ENDIAN_SWAP_INT16(m_videoram[(0xf800/2) + offs + 0]);
		INT32 x = BURN_ENDIAN_SWAP_INT16(m_videoram[(0xf800/2) + offs + 1]);
		INT32 dim = BURN_ENDIAN_SWAP_INT16(m_videoram2[(0xf800/2) + offs + 0]);

		INT32 srcpg = ((y & 0xf000) >> 12) + ((x & 0x0200) >> 5); // src page
		INT32 srcx = ((y >> 8) & 0xf) * 2;                    // src col
		INT32 srcy = ((dim >> 0) & 0xf) * 2;                  // src row

		switch ((dim >> 4) & 0xc)
		{
			case 0x0:   dimx = 2;   dimy = 2;   y0 = 0x100; break;
			case 0x4:   dimx = 4;   dimy = 4;   y0 = 0x100; break;
			case 0x8:   dimx = 2;   dimy = 32;  y0 = 0x130; break;
			default:
			case 0xc:   dimx = 4;   dimy = 32;  y0 = 0x120; break;
		}

		if (dimx == 4)
		{
			flipx = srcx & 2;
			srcx &= ~2;
		}
		else
			flipx = 0;

		x = (x & 0xff) - (x & 0x100);
		y = (y0 - (y & 0xff) - dimy * 8) & 0xff;

		if (flipx)
		{
			tile_xstart = dimx - 1;
			tile_xinc = -1;
		}
		else
		{
			tile_xstart = 0;
			tile_xinc = +1;
		}

		INT32 tile_y = 0;
		INT32 tile_yinc = +1;

		for (INT32 dy = 0; dy < dimy * 8; dy += 8)
		{
			INT32 tile_x = tile_xstart;

			for (int dx = 0; dx < dimx * 8; dx += 8)
			{
				INT32 addr = (srcpg * 0x20 * 0x20) + ((srcx + tile_x) & 0x1f) * 0x20 + ((srcy + tile_y) & 0x1f);
				INT32 tile = BURN_ENDIAN_SWAP_INT16(m_videoram[addr]);
				INT32 attr = BURN_ENDIAN_SWAP_INT16(m_videoram2[addr]);

				INT32 sx = x + dx;
				INT32 sy = (y + dy) & 0xff;

				INT32 tile_flipx = tile & 0x4000;
				INT32 tile_flipy = tile & 0x8000;

				if (flipx)
					tile_flipx = !tile_flipx;

				Draw8x8MaskTile(pTransDraw, tile & 0x1fff, sx, sy - 16, tile_flipx, tile_flipy, attr, 4, 0xf, 0, DrvGfxROM);

				tile_x += tile_xinc;
			}

			tile_y += tile_yinc;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear();

	GenericTilemapDraw(0, pTransDraw, 0);
	GenericTilemapDraw(1, pTransDraw, 0);

	draw_sprites();

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
		DrvInputs[0] = 0xffff;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 10000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	
	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == (nInterleave - 1)) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);

		CPU_RUN(1, Zet);
	}

	if (pBurnSoundOut) {
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
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
		ZetScan(nAction);

		DACScan(nAction, pnMin);

		SCAN_VAR(bankdata);
		SCAN_VAR(soundlatch);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}


// Go 2000

static struct BurnRomInfo go2000RomDesc[] = {
	{ "3.bin",	0x20000, 0xfe1fb269, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "4.bin",	0x20000, 0xd6246ae3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "5.bin",	0x80000, 0xa32676ee, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "1.bin",	0x20000, 0x96e50aba, 3 | BRF_GRA },           //  3 Graphics
	{ "2.bin",	0x20000, 0xb0adf1cb, 3 | BRF_GRA },           //  4
};

STD_ROM_PICK(go2000)
STD_ROM_FN(go2000)

struct BurnDriver BurnDrvGo2000 = {
	"go2000", NULL, NULL, NULL, "2000",
	"Go 2000\0", NULL, "SunA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, go2000RomInfo, go2000RomName, NULL, NULL, NULL, NULL, Go2000InputInfo, Go2000DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};
