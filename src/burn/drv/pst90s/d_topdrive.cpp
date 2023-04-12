// FinalBurn Neo Proyesel Top Driving driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "eeprom.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM[2];
static UINT8 *DrvSprRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM[2];
static UINT8 *DrvScrRAM;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[1];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo TopdriveInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 12,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Topdrive)

static struct BurnDIPInfo TopdriveDIPList[]=
{
	{0x13, 0xff, 0xff, 0x08, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},
};

STDDIPINFO(Topdrive)

static void __fastcall write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x900000:
			EEPROMSetCSLine((data & 1) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
		return;

		case 0x900002:
			EEPROMSetClockLine((data & 1) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x900004:
			EEPROMWriteBit(data & 1);
		return;

		case 0xe00004: // sound bank?
		return;
	}

	bprintf (0, _T("MW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x900001:
			EEPROMSetCSLine((data & 1) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
		return;

		case 0x900003:
			EEPROMSetClockLine((data & 1) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x900005:
			EEPROMWriteBit(data & 1);
		return;

		case 0xe00003:
			MSM6295Write(0, data);
		return;

	}
}

static UINT16 __fastcall read_word(UINT32 address)
{
	switch (address)
	{
		case 0x800000:
			return DrvInputs[0];

		case 0x800002:
			return (DrvInputs[1] & ~0x0800) | ((DrvDips[0] & 0x08) << 8);
	}

	bprintf (0, _T("RW: %5.5x\n"), address);
	return 0;
}

static UINT8 __fastcall read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x800000:
			return ((DrvInputs[0] >> 8) & ~0x08)  | (DrvDips[0] & 0x08);

		case 0x800001:
			return DrvInputs[0];

		case 0x800002:
			return DrvInputs[1] >> 8;

		case 0x800003:
			return DrvInputs[1];

		case 0x900007:
			return EEPROMRead();

		case 0xe00003:
			return MSM6295Read(0);
	}

	return 0;
}

static tilemap_callback( bg1 )
{
	INT32 attr = *((UINT16*)(DrvBgRAM[1] + offs * 2));

	TILE_SET_INFO(0, attr, attr >> 13, 0);
}

static tilemap_callback( bg0 )
{
	INT32 attr = *((UINT16*)(DrvBgRAM[0] + offs * 2));

	TILE_SET_INFO(1, attr, attr >> 13, 0);
}

static tilemap_callback( fg )
{
	INT32 attr = *((UINT16*)(DrvFgRAM + offs * 2));

	TILE_SET_INFO(2, attr, attr >> 13, 0);
}

static tilemap_scan( layer )
{
	return (row & 0xf) | ((col & 0x3f) << 4) | ((row & 0x30) << 6);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	EEPROMReset();

	MSM6295Reset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x100000;

	DrvGfxROM		= Next; Next += 0x800000;

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x080000;

	BurnPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(INT32);

	AllRam			= Next;

	Drv68KRAM[0]	= Next; Next += 0x030000;
	Drv68KRAM[1]	= Next; Next += 0x010000;
	BurnPalRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvFgRAM		= Next; Next += 0x004000;
	DrvBgRAM[0]		= Next; Next += 0x004000;
	DrvBgRAM[1]		= Next; Next += 0x008000;
	DrvScrRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { (((0x400000*8) / 4) * 3), (((0x400000*8) / 4) * 2), (((0x400000*8) / 4) * 1), (((0x400000*8) / 4) * 0)};
	INT32 XOffs[16] = { STEP16(0,1) };
	INT32 YOffs[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x400000);

	GfxDecode(0x8000, 4, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(58.75);

	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x100000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x180000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x200000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x280000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x300000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x380000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM + 0x000000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvFgRAM,		0xa00000, 0xa03fff, MAP_RAM);
	SekMapMemory(DrvBgRAM[0],	0xa04000, 0xa07fff, MAP_RAM);
	SekMapMemory(DrvBgRAM[1],	0xa08000, 0xa0ffff, MAP_RAM);
	SekMapMemory(DrvScrRAM,		0xa10000, 0xa103ff, MAP_RAM); // 0-f
	SekMapMemory(DrvSprRAM,		0xb00000, 0xb007ff, MAP_RAM);
	SekMapMemory(BurnPalRAM,	0xc00000, 0xc007ff, MAP_RAM);
	SekMapMemory(Drv68KRAM[0],	0xf00000, 0xf2ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM[1],	0xff0000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,	write_word);
	SekSetWriteByteHandler(0,	write_byte);
	SekSetReadWordHandler(0,	read_word);
	SekSetReadByteHandler(0,	read_byte);
	SekClose();
	
	EEPROMInit(&eeprom_interface_93C46);

	MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH); 

	GenericTilesInit();
	GenericTilemapInit(0, layer_map_scan, bg1_map_callback, 16, 16, 32, 16);
	GenericTilemapInit(1, layer_map_scan, bg0_map_callback, 16, 16, 32, 16);
	GenericTilemapInit(2, layer_map_scan, fg_map_callback,  16, 16, 32, 16);
	GenericTilemapSetGfx(0, DrvGfxROM + 0x300000, 4, 16, 16, 0x200000, 0x200, 0x07); // bg1
	GenericTilemapSetGfx(1, DrvGfxROM + 0x600000, 4, 16, 16, 0x200000, 0x100, 0x07); // bg0
	GenericTilemapSetGfx(2, DrvGfxROM + 0x400000, 4, 16, 16, 0x200000, 0x000, 0x07); // fg
	GenericTilemapSetGfx(3, DrvGfxROM + 0x000000, 4, 16, 16, 0x400000, 0x300, 0x3f); // spr
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	MSM6295Exit();

	EEPROMExit();

	BurnFreeMemIndex();

	MSM6295ROM = NULL;

	return 0;
}

static void draw_sprites(INT32 priority)
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0; offs < 0x800/2; offs += 4)
	{
		if (ram[offs + 0] & 0x0100) break;

		INT32 sy    =  ram[offs + 0] & 0x00ff;
		INT32 attr  =  ram[offs + 1];
		INT32 code  =  ram[offs + 2];
		INT32 sx    =  ram[offs + 3] & 0x01ff;

		INT32 color =  attr & 0x000f;
		INT32 prio  = (attr & 0x0010) >> 4;

		if (priority != prio) continue;

		DrawGfxMaskTile(0, 3, code, sx - 64 + 2, (272 - sy) - 31, 0, 0, color, 0xf);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		BurnPaletteUpdate_xBBBBBGGGGGRRRRR();
		BurnRecalc = 1;
	}

	BurnTransferClear();

	UINT16 *scroll = (UINT16 *)DrvScrRAM;
	GenericTilemapSetScrollX(2, BURN_ENDIAN_SWAP_INT16(scroll[0]) + 50);
	GenericTilemapSetScrollY(2, BURN_ENDIAN_SWAP_INT16(scroll[1]));
	GenericTilemapSetScrollX(1, BURN_ENDIAN_SWAP_INT16(scroll[2]) + 50);
	GenericTilemapSetScrollY(1, BURN_ENDIAN_SWAP_INT16(scroll[3]));
	GenericTilemapSetScrollX(0, BURN_ENDIAN_SWAP_INT16(scroll[4]) + 50);
	GenericTilemapSetScrollY(0, BURN_ENDIAN_SWAP_INT16(scroll[5]));

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);
	if (nSpriteEnable & 1) draw_sprites(0);
	if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 0);
	if (nSpriteEnable & 2) draw_sprites(1);

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
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 12000000 / 50 };
	INT32 nCyclesDone[1] = { 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (i == 239) {
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}
	}

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();



	return 0;
}

static INT32 DrvScan(int nAction,int *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029697;
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

		MSM6295Scan(nAction, pnMin);
	}

	return 0;
}


// Top Driving (version 1.1)

static struct BurnRomInfo topdriveRomDesc[] = {
	{ "2-27c040.bin",	0x80000, 0x37798c4e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "1-27c040.bin",	0x80000, 0xe2dc5096, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "4-27c040.bin",	0x80000, 0xa81ca7f7, 2 | BRF_GRA },           //  2 Graphics
	{ "5-27c040.bin",	0x80000, 0xa756d2b2, 2 | BRF_GRA },           //  3
	{ "6-27c040.bin",	0x80000, 0x90c778a2, 2 | BRF_GRA },           //  4
	{ "7-27c040.bin",	0x80000, 0xdb219087, 2 | BRF_GRA },           //  5
	{ "8-27c040.bin",	0x80000, 0x0e5f4419, 2 | BRF_GRA },           //  6
	{ "9-27c040.bin",	0x80000, 0x159a7426, 2 | BRF_GRA },           //  7
	{ "10-27c040.bin",	0x80000, 0x54c1617a, 2 | BRF_GRA },           //  8
	{ "11-27c040.bin",	0x80000, 0x6b3c3c73, 2 | BRF_GRA },           //  9

	{ "3-27c040.bin",	0x80000, 0x2894b89b, 3 | BRF_SND },           // 10 Samples
};

STD_ROM_PICK(topdrive)
STD_ROM_FN(topdrive)

struct BurnDriver BurnDrvTopdrive = {
	"topdrive", NULL, NULL, NULL, "1995",
	"Top Driving (version 1.1)\0", NULL, "Proyesel", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, topdriveRomInfo, topdriveRomName, NULL, NULL, NULL, NULL, TopdriveInputInfo, TopdriveDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	384, 240, 4, 3
};
