// FB Alpha Legend of Silkroad driver
// Based on MAME driver by David Haywood, R.Belmont, and Stephh

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "burn_ym2151.h"
#include "burn_pal.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM[2];
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *Drv68KRAM;
static UINT16 *DrvSysRegs;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 okibank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 15,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 14,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 12,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 5,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Dip 1",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip 2",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x15, 0xff, 0xff, 0x1c, NULL					},
	{0x16, 0xff, 0xff, 0xf7, NULL					},

	{0,    0xfe, 0,    2,    "Lives"				},
	{0x15, 0x01, 0x01, 0x01, "1"					},
	{0x15, 0x01, 0x01, 0x00, "2"					},

	{0,    0xfe, 0,    2,    "Blood Effect"			},
	{0x15, 0x01, 0x02, 0x02, "Off"					},
	{0x15, 0x01, 0x02, 0x00, "On"					},
 
	{0,    0xfe, 0,    8,    "Difficulty"			},
	{0x15, 0x01, 0xe0, 0x60, "Easiest"				},
	{0x15, 0x01, 0xe0, 0x40, "Easier"				},
	{0x15, 0x01, 0xe0, 0x20, "Easy"					},
	{0x15, 0x01, 0xe0, 0x00, "Normal"				},
	{0x15, 0x01, 0xe0, 0xe0, "Medium"				},
	{0x15, 0x01, 0xe0, 0xc0, "Hard"					},
	{0x15, 0x01, 0xe0, 0xa0, "Harder"				},
	{0x15, 0x01, 0xe0, 0x80, "Hardest"				},

	{0,    0xfe, 0,    2,    "Free Play"			},
	{0x16, 0x01, 0x02, 0x02, "Off"					},
	{0x16, 0x01, 0x02, 0x00, "On"					},

	{0,    0xfe, 0,    2,    "Demo Sounds"			},
	{0x16, 0x01, 0x08, 0x08, "Off"					},
	{0x16, 0x01, 0x08, 0x00, "On"					},

	{0,    0xfe, 0,    2,    "Chute Type"			},
	{0x16, 0x01, 0x10, 0x10, "Single"				},
	{0x16, 0x01, 0x10, 0x00, "Multi"				},

	{0,    0xfe, 0,    8,    "Coinage"				},
	{0x16, 0x01, 0xe0, 0x00, "5 Coins 1 Credit"		},
	{0x16, 0x01, 0xe0, 0x20, "4 Coins 1 Credit"		},
	{0x16, 0x01, 0xe0, 0x40, "3 Coins 1 Credit"		},
	{0x16, 0x01, 0xe0, 0x60, "2 Coins 1 Credit"		},
	{0x16, 0x01, 0xe0, 0xe0, "1 Coin  1 Credit"		},
	{0x16, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"	},
};

STDDIPINFO(Drv)

static inline void palette_write(INT32 offset, UINT16 pal)
{
	DrvPalette[offset] = BurnHighCol(pal5bit(pal >> 10), pal5bit(pal >> 5), pal5bit(pal >> 0), 0);
}

static void set_okibank(INT32 data)
{
	if ((data & 0x03) < 2)
	{
		okibank = data & 3;
		MSM6295SetBank(0, DrvSndROM[0] + 0x20000 + (okibank * 0x20000), 0x20000, 0x3ffff);
	}
}

static UINT8 __fastcall silkroad_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xc00000:
			return DrvInputs[0] >> 8;

		case 0xc00001:
			return DrvInputs[0] & 0xff;

		case 0xc00002:
			return 0xff;

		case 0xc00003:
			return DrvInputs[1] & 0xff;

		case 0xc00004:
			return DrvDips[1];

		case 0xc00005:
			return DrvDips[0];

		case 0xc00025:
			return MSM6295Read(0);

		case 0xc0002d:
			return BurnYM2151Read();

		case 0xc00031:
			return MSM6295Read(1);
	}

	return 0;
}

static void __fastcall silkroad_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0xc00025:
			MSM6295Write(0, data);
		return;

		case 0xc00029:
			BurnYM2151SelectRegister(data);
		return;

		case 0xc0002d:
			BurnYM2151WriteRegister(data);
		return;

		case 0xc00031:
			MSM6295Write(1, data);
		return;

		case 0xc00034:
		case 0xc00035:
		case 0xc00036:
		case 0xc00037:
			set_okibank(data);
		return;
	}
}

static void __fastcall silkroad_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffc000) == 0x600000) {
		*((UINT16 *)(DrvPalRAM + (address & 0x3ffe))) = BURN_ENDIAN_SWAP_INT16(data);
		palette_write((address >> 2) & 0x0fff, data);
		return;
	}

	if (address >= 0xc0010c && address <= 0xc00123) {
		DrvSysRegs[(address - 0xc0010c) >> 1] = data;
		return;
	}
}

static void __fastcall silkroad_write_long(UINT32 address, UINT32 data)
{
	if ((address & 0xffffc000) == 0x600000) {
		*((UINT32 *)(DrvPalRAM + (address & 0x3ffc))) = BURN_ENDIAN_SWAP_INT32(data);
		palette_write((address >> 2) & 0x0fff, data >> 16);
		return;
	}
}

static tilemap_callback( layer0 )
{
	INT32 code = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM + 0x0000 + offs * 4 + 0)));
	INT32 attr = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM + 0x0000 + offs * 4 + 2)));

	TILE_SET_INFO(0, code, attr, (attr & 0x80) ? TILE_FLIPX : 0);
}

static tilemap_callback( layer1 )
{
	INT32 code = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM + 0x4000 + offs * 4 + 0)));
	INT32 attr = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM + 0x4000 + offs * 4 + 2)));

	TILE_SET_INFO(0, code, attr, (attr & 0x80) ? TILE_FLIPX : 0);
}

static tilemap_callback( layer2 )
{
	INT32 code = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM + 0x8000 + offs * 4 + 0)));
	INT32 attr = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM + 0x8000 + offs * 4 + 2)));

	TILE_SET_INFO(0, code, attr, (attr & 0x80) ? TILE_FLIPX : 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	BurnYM2151Reset();

	MSM6295Reset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x0200000;

	DrvGfxROM 		= Next; Next += 0x2000000;

	MSM6295ROM		= Next;
	DrvSndROM[0]	= Next; Next += 0x0080000;
	DrvSndROM[1]	= Next; Next += 0x0040000;

	DrvPalette		= (UINT32*)Next; Next += 0x0001001 * sizeof (UINT32);

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x0001000;
	DrvPalRAM		= Next; Next += 0x0004000;
	DrvVidRAM		= Next; Next += 0x000c000;
	Drv68KRAM		= Next; Next += 0x0020000;

	DrvSysRegs		= (UINT16*)Next; Next += 0x0000020 * sizeof (UINT16);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes[6] = { 8, 0, 0x4000000+8, 0x4000000+0, 0x8000000+8,0x8000000+0 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(16,1) };
	INT32 YOffs[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x1800000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x1800000);

	GfxDecode(0x20000, 6, 16, 16, Planes, XOffs, YOffs, 0x200, tmp, DrvGfxROM);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 silkroada)
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRomExt(Drv68KROM + 0x0000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(Drv68KROM + 0x0000002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRomExt(DrvGfxROM + 0x0000000, k++, 1, LD_INVERT)) return 1; 
		if (BurnLoadRomExt(DrvGfxROM + 0x0800000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x1000000, k++, 1, LD_INVERT)) return 1;

		if (silkroada == 0) {
			if (BurnLoadRomExt(DrvGfxROM + 0x0200000, k++, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM + 0x0a00000, k++, 1, LD_INVERT)) return 1;
			if (BurnLoadRomExt(DrvGfxROM + 0x1200000, k++, 1, LD_INVERT)) return 1;
		}

		if (BurnLoadRomExt(DrvGfxROM + 0x0400000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0c00000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x1400000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0600000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x0e00000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM + 0x1600000, k++, 1, LD_INVERT)) return 1;

		if (BurnLoadRom(DrvSndROM[0] + 0x0000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1] + 0x0000000, k++, 1)) return 1;

		if (DrvGfxDecode()) return 1;
	}

	SekInit(0, 0x68EC020);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0x40c000, 0x40cfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x600000, 0x603fff, MAP_ROM);
	SekMapMemory(DrvVidRAM,		0x800000, 0x80bfff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xfe0000, 0xffffff, MAP_RAM);
	SekSetWriteByteHandler(0,	silkroad_write_byte);
	SekSetWriteWordHandler(0,	silkroad_write_word);
	SekSetWriteLongHandler(0,	silkroad_write_long);
	SekSetReadByteHandler(0,	silkroad_read_byte);
	SekClose();

	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295Init(1, 2000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetBank(0, DrvSndROM[0], 0, 0x3ffff);
	MSM6295SetBank(1, DrvSndROM[1], 0, 0x3ffff);
	MSM6295SetRoute(0, 0.45, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.45, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 16, 16, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 16, 16, 64, 64);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, layer2_map_callback, 16, 16, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM + 0x1800000, 6, 16, 16, 0x0800000, 0, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM + 0x0000000, 6, 16, 16, 0x2000000, 0, 0x3f);
	GenericTilemapBuildSkipTable(0, 0, 0);
	GenericTilemapBuildSkipTable(1, 0, 0);
	GenericTilemapBuildSkipTable(2, 0, 0);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -50, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	BurnYM2151Exit();
	MSM6295Exit();

	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites()
{
	UINT16 *source = (UINT16*)DrvSprRAM;

	for (INT32 i = 0; i < 0x1000/2; i += 4)
	{
		INT32 attr  = source[i + 3];
		if ((attr & 0xff00) == 0xff00) break;

		INT32 code  = source[i + 2] | ((attr & 0x8000) << 1);
		INT32 sy    = source[i + 1];
		INT32 sx    = source[i + 0] & 0x01ff;
		INT32 flipx =  (attr & 0x0080);
		INT32 width = ((attr & 0x0f00) >> 8) + 1;
		INT32 color =   attr & 0x003f;
		INT32 pri   = ((attr & 0x1000) >> 11) ^ 0xfe;

		if (flipx)
		{
			for (INT32 x = width; x > 0; x--)
			{
				RenderPrioSprite(pTransDraw, DrvGfxROM, (code + (width - x)) & 0x1ffff, color << 6, 0, sx+x*16-16+8-50, sy - 16, 1, 0, 16, 16, pri);
			}
		}
		else
		{
			for (INT32 x = 0; x < width; x++)
			{
				RenderPrioSprite(pTransDraw, DrvGfxROM, (code + x) & 0x1ffff, color << 6, 0, sx+x*16+8-50, sy - 16, 0, 0, 16, 16, pri);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		UINT16 *pal = (UINT16*)DrvPalRAM;
		for (INT32 i = 0; i < 0x2000; i+=2) {
			palette_write(i/2, pal[i]);
		}

		DrvPalette[0x1000] = BurnHighCol(0xff, 0x00, 0xff, 0);
		DrvRecalc = 0;
	}

	BurnTransferClear((nBurnLayer & 1) ? 0x7c0 : 0x1000);

	GenericTilemapSetScrollX(0, DrvSysRegs[ 0]);
	GenericTilemapSetScrollY(0, DrvSysRegs[ 1]);
	GenericTilemapSetScrollX(1, DrvSysRegs[ 5]);
	GenericTilemapSetScrollY(1, DrvSysRegs[10]);
	GenericTilemapSetScrollX(2, DrvSysRegs[ 4]);
	GenericTilemapSetScrollY(2, DrvSysRegs[ 2]);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0, 0xff);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 1, 0xff);
	if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 2, 0xff);
	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

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
			DrvInputs[0] ^= DrvJoy1[i] << i;
			DrvInputs[1] ^= DrvJoy2[i] << i;
		}

		DrvInputs[1] ^= DrvJoy2[4] << 6;

		if (!(DrvInputs[0] & 0x00c0)) DrvInputs[0] |= 0x00c0;
		if (!(DrvInputs[0] & 0x0030)) DrvInputs[0] |= 0x0030;
		if (!(DrvInputs[0] & 0xc000)) DrvInputs[0] |= 0xc000;
		if (!(DrvInputs[0] & 0x3000)) DrvInputs[0] |= 0x3000;
	}

	INT32 nTotalCycles = (INT32)((INT64)16000000 * nBurnCPUSpeedAdjust / (0x0100 * 60));

	SekOpen(0);
	SekRun(nTotalCycles);
	SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
	SekClose();

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029682;
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

		BurnYM2151Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(okibank);
	}

	if (nAction & ACB_WRITE) {
		set_okibank(okibank);
	}

	return 0;
}


// The Legend of Silkroad

static struct BurnRomInfo silkroadRomDesc[] = {
	{ "rom02.bin",			0x100000, 0x4e5200fc, 1 | BRF_PRG }, //  0 Motorola '020 Code
	{ "rom03.bin",			0x100000, 0x73ccc78c, 1 | BRF_PRG }, //  1

	{ "rom12.rom12",		0x200000, 0x96393d04, 2 | BRF_GRA }, //  2 Graphics
	{ "rom08.rom08",		0x200000, 0x23f1d462, 2 | BRF_GRA }, //  3
	{ "rom04.rom04",		0x200000, 0xd9f0bbd7, 2 | BRF_GRA }, //  4
	{ "rom13.rom13",		0x200000, 0x4ca1698e, 2 | BRF_GRA }, //  5
	{ "rom09.rom09",		0x200000, 0xef0b5bf4, 2 | BRF_GRA }, //  6
	{ "rom05.rom05",		0x200000, 0x512d6e25, 2 | BRF_GRA }, //  7
	{ "rom14.rom14",		0x200000, 0xd00b19c4, 2 | BRF_GRA }, //  8
	{ "rom10.rom10",		0x200000, 0x7d324280, 2 | BRF_GRA }, //  9
	{ "rom06.rom06",		0x200000, 0x3ac26060, 2 | BRF_GRA }, // 10
	{ "rom07.rom07",		0x200000, 0x9fc6ff9d, 2 | BRF_GRA }, // 11
	{ "rom11.rom11",		0x200000, 0x11abaf1c, 2 | BRF_GRA }, // 12
	{ "rom15.rom15",		0x200000, 0x26a3b168, 2 | BRF_GRA }, // 13

	{ "rom00.bin",  		0x080000, 0xb10ba7ab, 3 | BRF_SND }, // 14 OKI #0 Samples

	{ "rom01.bin",			0x040000, 0xdb8cb455, 4 | BRF_SND }, // 15 OKI #1 Samples
};

STD_ROM_PICK(silkroad)
STD_ROM_FN(silkroad)

static INT32 SilkroadInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvSilkroad = {
	"silkroad", NULL, NULL, NULL, "1999",
	"The Legend of Silkroad\0", NULL, "Unico", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, silkroadRomInfo, silkroadRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	SilkroadInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1001,
	380, 224, 4, 3
};


// The Legend of Silkroad (larger roms)

static struct BurnRomInfo silkroadaRomDesc[] = {
	{ "rom02.bin",			0x100000, 0x4e5200fc, 1 | BRF_PRG }, //  0 Motorola '020 Code
	{ "rom03.bin",			0x100000, 0x73ccc78c, 1 | BRF_PRG }, //  1

	{ "unico_sr13.rom13",	0x400000, 0xd001c3df, 2 | BRF_GRA }, //  2 Graphics
	{ "unico_sr09.rom09",	0x400000, 0x696d908d, 2 | BRF_GRA }, //  3
	{ "unico_sr05.rom05",	0x400000, 0x00f638c1, 2 | BRF_GRA }, //  4
	{ "rom14.rom14",		0x200000, 0xd00b19c4, 2 | BRF_GRA }, //  5
	{ "rom10.rom10",		0x200000, 0x7d324280, 2 | BRF_GRA }, //  6
	{ "rom06.rom06",		0x200000, 0x3ac26060, 2 | BRF_GRA }, //  7
	{ "rom07.rom07",		0x200000, 0x9fc6ff9d, 2 | BRF_GRA }, //  8
	{ "rom11.rom11",		0x200000, 0x11abaf1c, 2 | BRF_GRA }, //  9
	{ "rom15.rom15",		0x200000, 0x26a3b168, 2 | BRF_GRA }, // 10

	{ "rom00.bin",  		0x080000, 0xb10ba7ab, 3 | BRF_SND }, // 11 OKI #0 Samples

	{ "rom01.bin",			0x040000, 0xdb8cb455, 4 | BRF_SND }, // 12 OKI #1 Samples
};

STD_ROM_PICK(silkroada)
STD_ROM_FN(silkroada)

static INT32 SilkroadaInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvSilkroada = {
	"silkroada", "silkroad", NULL, NULL, "1999",
	"The Legend of Silkroad (larger roms)\0", NULL, "Unico", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, silkroadaRomInfo, silkroadaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	SilkroadaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1001,
	380, 224, 4, 3
};
