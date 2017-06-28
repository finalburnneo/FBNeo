// FB Alpha Dr. Tomy driver module
// Based on MAME driver by Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"

static UINT8 *Mem;
static UINT8 *MemEnd;
static UINT8 *RamStart;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT32 m6295bank;

static struct BurnInputInfo DrvInputList[] = {
	{"Coin 1"       , BIT_DIGITAL  , DrvJoy1 + 6, "p1 coin"  },
	{"Coin 2"       , BIT_DIGITAL  , DrvJoy1 + 7, "p2 coin"  },

	{"P1 Start"     , BIT_DIGITAL  , DrvJoy2 + 6, "p1 start" },
	{"P1 Up"        , BIT_DIGITAL  , DrvJoy1 + 0, "p1 up"    },
	{"P1 Down"      , BIT_DIGITAL  , DrvJoy1 + 1, "p1 down"  },
	{"P1 Left"      , BIT_DIGITAL  , DrvJoy1 + 3, "p1 left"  },
	{"P1 Right"     , BIT_DIGITAL  , DrvJoy1 + 2, "p1 right" },
	{"P1 Button 1"  , BIT_DIGITAL  , DrvJoy1 + 5, "p1 fire 1"},
	{"P1 Button 2"  , BIT_DIGITAL  , DrvJoy1 + 4, "p1 fire 2"},

	{"P2 Start"     , BIT_DIGITAL  , DrvJoy2 + 7, "p2 start" },
	{"P2 Up"        , BIT_DIGITAL  , DrvJoy2 + 0, "p2 up"    },
	{"P2 Down"      , BIT_DIGITAL  , DrvJoy2 + 1, "p2 down"  },
	{"P2 Left"      , BIT_DIGITAL  , DrvJoy2 + 3, "p2 left"  },
	{"P2 Right"     , BIT_DIGITAL  , DrvJoy2 + 2, "p2 right" },
	{"P2 Button 1"  , BIT_DIGITAL  , DrvJoy2 + 5, "p2 fire 1"},
	{"P2 Button 2"  , BIT_DIGITAL  , DrvJoy2 + 4, "p2 fire 2"},

	{"Reset",	  BIT_DIGITAL  , &DrvReset,   "reset"    },
	{"Dip 1",	  BIT_DIPSWITCH, DrvDips + 0, "dip"	 },
	{"Dip 2",	  BIT_DIPSWITCH, DrvDips + 1, "dip"	 },
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL 			},
	{0x12, 0xff, 0xff, 0xaf, NULL 			},

	{0   , 0xfe, 0   , 11  , "Coin A" },
	{0x11, 0x01, 0x0f, 0x0a, "2 Coins 1 Credit"	},
	{0x11, 0x01, 0x0f, 0x07, "3 Coins 2 Credits"	},
	{0x11, 0x01, 0x0f, 0x00, "5 Coins 4 Credits"	},
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"	},
	{0x11, 0x01, 0x0f, 0x06, "3 Coins 4 Credits"	},
	{0x11, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0f, 0x08, "2 Coins 5 Credits"	},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   , 11  , "Coin B" },
	{0x11, 0x01, 0xf0, 0xa0, "2 Coins 1 Credit" 	},
	{0x11, 0x01, 0xf0, 0x70, "3 Coins 2 Credits"	},
	{0x11, 0x01, 0xf0, 0x00, "5 Coins 4 Credits"	},
	{0x11, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"	},
	{0x11, 0x01, 0xf0, 0x60, "3 Coins 4 Credits"	},
	{0x11, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0xf0, 0x80, "2 Coins 5 Credits"	},
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   , 2   , "Time"			},
	{0x12, 0x01, 0x01, 0x00, "Less"			},
	{0x12, 0x01, 0x01, 0x01, "More"			},

	{0   , 0xfe, 0   , 2   , "Number of Virus"	},
	{0x12, 0x01, 0x02, 0x02, "Less"			},	
	{0x12, 0x01, 0x02, 0x00, "More"			},

	{0   , 0xfe, 0   , 2   , "Test Mode"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   , 2   , "Demo Sounds"		},
	{0x12, 0x01, 0x10, 0x10, "Off"			},
	{0x12, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   , 2   , "Language"		},
	{0x12, 0x01, 0x20, 0x20, "English"		},
	{0x12, 0x01, 0x20, 0x00, "Italian"		},

	{0   , 0xfe, 0   , 2   , "Allow Continue"	},
	{0x12, 0x01, 0x40, 0x40, "No"			},
	{0x12, 0x01, 0x40, 0x00, "Yes"			},
};

STDDIPINFO(Drv)

static inline void drtomy_okibank(UINT8 data)
{
	m6295bank = data & 3;

	MSM6295SetBank(0, DrvSndROM + (m6295bank * 0x20000), 0x20000, 0x3ffff);
}

static inline void palette_write(INT32 offset, UINT16 pal)
{
	INT32 r = (pal >> 10) & 0x1f;
	INT32 g = (pal >>  5) & 0x1f;
	INT32 b = (pal >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset] = BurnHighCol(r, g, b, 0);
}

static UINT8 __fastcall drtomy_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x700001:
			return DrvDips[0]; 

		case 0x700003:
			return DrvDips[1]; 

		case 0x700005:
			return DrvInputs[0]; 

		case 0x700007:
			return DrvInputs[1];

		case 0x70000f:
			return MSM6295ReadStatus(0);
	}

	return 0;
}

static void __fastcall drtomy_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x70000d:
			drtomy_okibank(data);
		return;

		case 0x70000f:
			MSM6295Command(0, data);
		return;
	}
}

static void __fastcall drtomy_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff800) == 0x200000) {
		*((UINT16*)(DrvPalRAM + (address & 0x7fe))) = data;
		if (address < 0x200600) palette_write((address / 2) & 0x3ff, data);
		return;
	}
}

static tilemap_callback( background )
{
	UINT16 *ram = (UINT16*)(DrvVidRAM + 0x1000);

	INT32 attr = BURN_ENDIAN_SWAP_INT16(ram[offs]);

	TILE_SET_INFO(0, attr, attr >> 12, 0);
}

static tilemap_callback( foreground )
{
	UINT16 *ram = (UINT16*)(DrvVidRAM + 0x0000);

	INT32 attr = BURN_ENDIAN_SWAP_INT16(ram[offs]);

	INT32 flags = ((attr & 0xfff) == 0) ? TILE_SKIP : 0;

	TILE_SET_INFO(1, attr, attr >> 12, flags);
}

static INT32 DrvDoReset()
{
	memset (RamStart, 0, RamEnd - RamStart);

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset(0);

	drtomy_okibank(0);

	return 0;
}

static INT32 DrvGfxDecode()
{
	static INT32 Planes[4] = { STEP4(0,0x200000) };
	static INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	static INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x100000);

	GfxDecode(0x8000, 4,  8,  8, Planes, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);
	GfxDecode(0x1000, 4, 16, 16, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KROM	= Next; Next += 0x040000;
	DrvGfxROM0	= Next; Next += 0x200000;
	DrvGfxROM1	= Next; Next += 0x100000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x080000;

	DrvPalette	= (UINT32*)Next; Next += 0x00300 * sizeof(UINT32);

	RamStart	= Next;

	DrvVidRAM	= Next; Next += 0x002000;
	DrvPalRAM	= Next; Next += 0x000800;
	DrvSprRAM	= Next; Next += 0x001000;
	Drv68KRAM	= Next; Next += 0x004000;

	RamEnd		= Next;

	MemEnd		= Next;

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
		if (BurnLoadRom(Drv68KROM + 0x000000, 0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000001, 1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, 2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x40000, 3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x80000, 4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0xc0000, 5, 1)) return 1;

		if (BurnLoadRom(DrvSndROM + 0x000000, 6, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,	0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvVidRAM,	0x100000, 0x101fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,	0x200000, 0x1007ff, MAP_ROM);
	SekMapMemory(DrvSprRAM,	0x440000, 0x440fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,	0xffc000, 0xffffff, MAP_RAM);
	SekSetWriteByteHandler(0, drtomy_write_byte);
	SekSetWriteWordHandler(0, drtomy_write_word);
	SekSetReadByteHandler(0, drtomy_read_byte);
	SekClose();

	MSM6295Init(0, 1625000 / 132, 0);
	MSM6295SetBank(0, DrvSndROM, 0, 0x3ffff);
	MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 4, 16, 16, 0x100000, 0x000, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x100000, 0x200, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	GenericTilesExit();
	MSM6295Exit(0);

	BurnFree (Mem);
	
	MSM6295ROM = NULL;

	return 0;
}

static void draw_sprites()
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	for (INT32 i = 3; i < 0x1000/2; i+=4)
	{
		INT32 sx    = BURN_ENDIAN_SWAP_INT16(spriteram[i+2]) & 0x01ff;
		INT32 sy    = (240 - (BURN_ENDIAN_SWAP_INT16(spriteram[i]) & 0x00ff)) & 0x00ff;
		INT32 number= BURN_ENDIAN_SWAP_INT16(spriteram[i+3]);
		INT32 color = (BURN_ENDIAN_SWAP_INT16(spriteram[i+2]) & 0x1e00) >> 9;
		INT32 attr  = (BURN_ENDIAN_SWAP_INT16(spriteram[i]) & 0xfe00) >> 9;

		INT32 xflip = attr & 0x20;
		INT32 yflip = attr & 0x40;
		INT32 spr_size;

		if (attr & 0x04){
			spr_size = 1;
		} else {
			spr_size = 2;
			number &= (~3);
		}

		for (INT32 y = 0; y < spr_size; y++)
		{
			for (INT32 x = 0; x < spr_size; x++)
			{
				INT32 ex = xflip ? (spr_size-1-x) : x;
				INT32 ey = yflip ? (spr_size-1-y) : y;

				INT32 sxx = sx-0x09+x*8;
				INT32 syy = sy+y*8;

				syy -= 0x10;

				INT32 code = number + (ex * 2) + ey;

				if (yflip) {
					if (xflip) {
						Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sxx, syy, color, 4, 0, 0x100, DrvGfxROM0);
					} else {
						Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sxx, syy, color, 4, 0, 0x100, DrvGfxROM0);
					}
				} else {
					if (xflip) {
						Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sxx, syy, color, 4, 0, 0x100, DrvGfxROM0);
					} else {
						Render8x8Tile_Mask_Clip(pTransDraw, code, sxx, syy, color, 4, 0, 0x100, DrvGfxROM0);
					}
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		UINT16 *ram = (UINT16*)DrvPalRAM;
		for (INT32 i = 0; i < 0x600/2; i++) {
			palette_write(i,ram[i]);
		}
	}

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

	{
		DrvInputs[0] = DrvInputs[1] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
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
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_MEMORY_RAM) {	
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd - RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		SekScan(nAction);
		MSM6295Scan(0, nAction);

		SCAN_VAR(m6295bank);

		if (nAction & ACB_WRITE)
			drtomy_okibank(m6295bank);
	}

	return 0;
}


// Dr. Tomy

static struct BurnRomInfo drtomyRomDesc[] = {
	{ "15.u21", 0x20000, 0x0b8d763b, 1 | BRF_PRG | BRF_ESS }, // 0 68k code
	{ "16.u22", 0x20000, 0x206f4d65, 1 | BRF_PRG | BRF_ESS }, // 1

	{ "20.u80", 0x40000, 0x4d4d86ff, 2 | BRF_GRA },           // 2 Graphics
	{ "19.u81", 0x40000, 0x49ecbfe2, 2 | BRF_GRA },           // 3
	{ "18.u82", 0x40000, 0x8ee5c921, 2 | BRF_GRA },           // 4
	{ "17.u83", 0x40000, 0x42044b1c, 2 | BRF_GRA },           // 5

	{ "14.u23", 0x80000, 0x479614ec, 3 | BRF_SND },           // 6 Samples
};

STD_ROM_PICK(drtomy)
STD_ROM_FN(drtomy)

struct BurnDriver BurnDrvDrtomy = {
	"drtomy", NULL, NULL, NULL, "1993",
	"Dr. Tomy\0", NULL, "Playmark", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, drtomyRomInfo, drtomyRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	320, 240, 4, 3
};
