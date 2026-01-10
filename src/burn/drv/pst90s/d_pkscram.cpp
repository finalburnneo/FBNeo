// FB Alpha PK Scramble driver module
// Based on MAME driver by David Haywood and Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "burn_ym2203.h"

static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *AllRam;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvNVRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvMgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 irq_enable;
static INT32 irq_line_active;

static UINT8 DrvJoy1[8];
static UINT8 DrvDips[3];
static UINT16 DrvInputs[1];
static UINT8 DrvReset;

static struct BurnInputInfo PkscrambleInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 7,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Pkscramble)

static struct BurnDIPInfo PkscrambleDIPList[]=
{
	DIP_OFFSET(0x07)
	{0x00, 0xff, 0xff, 0xfb, NULL					},
	{0x01, 0xff, 0xff, 0x49, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    8, "Level"				},
	{0x00, 0x01, 0x07, 0x00, "0"					},
	{0x00, 0x01, 0x07, 0x01, "1"					},
	{0x00, 0x01, 0x07, 0x02, "2"					},
	{0x00, 0x01, 0x07, 0x03, "3"					},
	{0x00, 0x01, 0x07, 0x04, "4"					},
	{0x00, 0x01, 0x07, 0x05, "5"					},
	{0x00, 0x01, 0x07, 0x06, "6"					},
	{0x00, 0x01, 0x07, 0x07, "7"					},

	{0   , 0xfe, 0   ,    8, "Coin to Start"		},
	{0x01, 0x01, 0x07, 0x01, "1"					},
	{0x01, 0x01, 0x07, 0x02, "2"					},
	{0x01, 0x01, 0x07, 0x03, "3"					},
	{0x01, 0x01, 0x07, 0x04, "4"					},
	{0x01, 0x01, 0x07, 0x05, "5"					},
	{0x01, 0x01, 0x07, 0x06, "6"					},
	{0x01, 0x01, 0x07, 0x07, "7"					},
	{0x01, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x01, 0x01, 0x38, 0x08, "1 Coin  1 Credits"	},
	{0x01, 0x01, 0x38, 0x10, "1 Coin  2 Credits"	},
	{0x01, 0x01, 0x38, 0x18, "1 Coin  3 Credits"	},
	{0x01, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x01, 0x01, 0x38, 0x28, "1 Coin  5 Credits"	},
	{0x01, 0x01, 0x38, 0x30, "1 Coin  6 Credits"	},
	{0x01, 0x01, 0x38, 0x38, "1 Coin  7 Credits"	},
	{0x01, 0x01, 0x38, 0x00, "No Credit"			},

	{0   , 0xfe, 0   , 2   , "Service Mode"			},
	{0x01, 0x01, 0x80, 0x00, "Off"					},
	{0x01, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    2, "Coin Test"			},
	{0x02, 0x01, 0x40, 0x00, "Off"					},
	{0x02, 0x01, 0x40, 0x40, "On"					},
};

STDDIPINFO(Pkscramble)

static void __fastcall pkscramble_write_word(UINT32 address, UINT16 data)
{
	switch (address & 0x7fffe)
	{
		case 0x49008:
			irq_enable = data;
			if ((data & 0x2000) == 0 && irq_line_active) {
				SekSetIRQLine(1, CPU_IRQSTATUS_NONE);
				irq_line_active = 0;
			}
		return;

		case 0x4900c:
		case 0x4900e:
			BurnYM2203Write(0, (address / 2) & 1, data);
		return;

		case 0x49010:
		case 0x49014:
		case 0x49018:
		case 0x49020:
		case 0x52086:
			//	nop
		return;
	}
}

static void __fastcall pkscramble_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall pkscramble_read_word(UINT32 address)
{
	switch (address & 0x7ffff)
	{
		case 0x49000:
		case 0x49001:
			return (DrvDips[1] * 256) + DrvDips[0];

		case 0x49004:
		case 0x49005:                    // 0x20 hopper status sensor(?)
			return (DrvInputs[0] & ~0x60) + 0x20 + (DrvDips[2] & 0x40);

		case 0x4900c:
		case 0x4900d:
		case 0x4900e:
		case 0x4900f:
			return BurnYM2203Read(0, (address / 2) & 1);
	}

	return 0;
}

static UINT8 __fastcall pkscramble_read_byte(UINT32 address)
{
	switch (address & 0x7ffff)
	{
		case 0x49000:
			return DrvDips[1];

		case 0x49001:
			return DrvDips[0];

		case 0x49004:
			return 0;

		case 0x49005:
			return (DrvInputs[0] & ~0x60) + 0x20 + (DrvDips[2] & 0x40);

		case 0x4900c:
		case 0x4900d:
		case 0x4900e:
		case 0x4900f:
			return BurnYM2203Read(0, (address / 2) & 1);
	}

	return 0;
}

static tilemap_callback( bg )
{
	UINT16 *ram = (UINT16*)DrvBgRAM;

	UINT16 code  = BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 0]);
	UINT16 color = BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 1]) & 0x7f;

	TILE_SET_INFO(0, code, color, 0);
}

static tilemap_callback( mg )
{
	UINT16 *ram = (UINT16*)DrvMgRAM;

	UINT16 code  = BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 0]);
	UINT16 color = BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 1]) & 0x7f;

	TILE_SET_INFO(0, code, color, 0);
}

static tilemap_callback( fg )
{
	UINT16 *ram = (UINT16*)DrvFgRAM;

	UINT16 code  = BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 0]);
	UINT16 color = BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 1]) & 0x7f;

	TILE_SET_INFO(0, code, color, 0);
}

static void DrvIRQHandler(INT32, INT32 nStatus)
{
	if (irq_enable & 0x10)
	{
		SekSetIRQLine(2, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	}
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	BurnYM2203Reset();
	SekClose();

	irq_enable = 0;
	irq_line_active = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x020000;

	DrvGfxROM		= Next; Next += 0x080000;

	DrvPalette		= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000100;

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x003000;
	DrvFgRAM		= Next; Next += 0x001000;
	DrvMgRAM		= Next; Next += 0x001000;
	DrvBgRAM		= Next; Next += 0x002000;
	DrvPalRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxExpand()
{
	for (INT32 i = 0x40000; i>=0; i--)
	{
		DrvGfxROM[i*2+1] = DrvGfxROM[i] >> 4;
		DrvGfxROM[i*2+0] = DrvGfxROM[i] & 0xf;
	}
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x000001, 0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000, 1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x000001, 2, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x000000, 3, 2)) return 1;

		DrvGfxExpand();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(DrvNVRAM,			0x040000, 0x0403ff, MAP_RAM); // 0-ff
	SekMapMemory(Drv68KRAM,			0x041000, 0x043fff, MAP_RAM);
	SekMapMemory(DrvFgRAM,			0x044000, 0x044fff, MAP_RAM);
	SekMapMemory(DrvMgRAM,			0x045000, 0x045fff, MAP_RAM);
	SekMapMemory(DrvBgRAM,			0x046000, 0x047fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x048000, 0x048fff, MAP_RAM);
	SekSetWriteWordHandler(0,		pkscramble_write_word);
	SekSetWriteByteHandler(0,		pkscramble_write_byte);
	SekSetReadWordHandler(0,		pkscramble_read_word);
	SekSetReadByteHandler(0,		pkscramble_read_byte);
	SekClose();

	BurnYM2203Init(1, 3000000, &DrvIRQHandler, 0);
	BurnTimerAttachSek(8000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, mg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 8, 8, 0x080000, 0, 0x7f);
	GenericTilemapSetTransparent(1, 15);
	GenericTilemapSetTransparent(2, 15);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	BurnYM2203Exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x1000/2; i++)
	{
		UINT8 r = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 10) & 0x1f;
		UINT8 g = (BURN_ENDIAN_SWAP_INT16(p[i]) >>  5) & 0x1f;
		UINT8 b = (BURN_ENDIAN_SWAP_INT16(p[i]) >>  0) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	DrvPaletteUpdate();

	GenericTilemapDraw(0, pTransDraw, 0);
	GenericTilemapDraw(1, pTransDraw, 0);
	GenericTilemapDraw(2, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		DrvInputs[0] = 0;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] =  { 8000000 / 60 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {

		CPU_RUN_TIMER(0);

		if (i == 192) {
			if (irq_enable & 0x2000) {
				SekSetIRQLine(1, CPU_IRQSTATUS_ACK);
				irq_line_active = 1;
			}
		}
		else if (i == 193)
		{
			if (irq_line_active) {
				SekSetIRQLine(1, CPU_IRQSTATUS_NONE);
				irq_line_active = 0;
			}
		}
	}

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE)
	{
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(irq_line_active);
		SCAN_VAR(irq_enable);
	}

	if (nAction & ACB_NVRAM)
	{
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = DrvNVRAM;
		ba.nLen	  = 0x100;
		ba.szName = "NV Ram";
		BurnAcb(&ba);
	}

	return 0;
}


// PK Scramble

static struct BurnRomInfo pkscramRomDesc[] = {
	{ "pk1.6e",	0x10000, 0x80e972e5, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "pk2.6j",	0x10000, 0x752c86d1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pk3.1c",	0x20000, 0x0b18f2bc, 2 | BRF_GRA },           //  2 Graphics Tiles
	{ "pk4.1e",	0x20000, 0xa232d993, 2 | BRF_GRA },           //  3
};

STD_ROM_PICK(pkscram)
STD_ROM_FN(pkscram)

struct BurnDriver BurnDrvPkscram = {
	"pkscram", NULL, NULL, NULL, "1993",
	"PK Scramble\0", NULL, "Cosmo Electronics Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pkscramRomInfo, pkscramRomName, NULL, NULL, NULL, NULL, PkscrambleInputInfo, PkscrambleDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 192, 4, 3
};
