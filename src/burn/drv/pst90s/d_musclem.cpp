// FB Neo Inter Geo Muscle Master hardware driver module
// Based on MAME driver by Phil Bennett

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM[4];
static UINT8 *DrvSndROM[2];
static UINT8 *Drv68KRAM;
static UINT8 *DrvVidRAM[3];
static UINT8 *DrvSprRAM;

static INT32 oki_bank;
static UINT16 scroll[2][2];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[1];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static INT32 pump_target;
static INT32 pump_accu;

static struct BurnInputInfo MusclemInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 9,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Musclem)

static struct BurnDIPInfo MusclemDIPList[]=
{
	DIP_OFFSET(0x06)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x30, 0x10, "Very Easy"			},
	{0x00, 0x01, 0x30, 0x00, "Easy"					},
	{0x00, 0x01, 0x30, 0x30, "Normal"				},
	{0x00, 0x01, 0x30, 0x20, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Demo Lamp"			},
	{0x00, 0x01, 0x40, 0x00, "Off"					},
	{0x00, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x80, 0x00, "Off"					},
	{0x00, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Musclem)

static void sound_bankswitch(INT32 data)
{
	oki_bank = data & 3;

	MSM6295SetBank(0, DrvSndROM[0] + (oki_bank * 0x40000), 0x00000, 0x3ffff);
}

static UINT8 __fastcall musclem_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xc00000:
			return DrvDips[1];

		case 0xc00001:
			return DrvDips[0];

		case 0xc00088:
		case 0xc00089:
		case 0xc0008a:
		case 0xc0008b:
			return MSM6295Read((address >> 1) & 1);

		case 0xc00090:
		case 0xc00091:
		case 0xc00092:
		case 0xc00093:
			return DrvInputs[(address >> 1) & 1] >> ((~address & 1) * 8);
	}

	return 0;
}

static UINT16 __fastcall musclem_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xc00000:
			return (DrvDips[1] << 8) | DrvDips[0];

		case 0xc00088:
		case 0xc0008a:
			return MSM6295Read((address >> 1) & 1);

		case 0xc00090:
		case 0xc00092:
			return DrvInputs[(address >> 1) & 1];
	}

	return 0;
}

static void __fastcall musclem_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0xc00088:
		case 0xc00089:
		case 0xc0008a:
		case 0xc0008b:
			MSM6295Write((address & 0x02) >> 1, data);
		return;

		case 0xc0008c:
		case 0xc0008d:
			sound_bankswitch(data);
		return;

		case 0xc0008e: // ?
		case 0xc0008f:
		return;
		case 0xc00094: // lamps
		case 0xc00095:
		return;
	}
}

static void __fastcall musclem_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0xc00080:
		case 0xc00082:
		case 0xc00084:
		case 0xc00086:
			scroll[(address & 0x04) >> 2][(address & 0x02) >> 1] = data;
		return;

		case 0xc00088:
		case 0xc0008a:
			MSM6295Write((address & 0x02) >> 1, data);
		return;

		case 0xc0008c:
			sound_bankswitch(data);
		return;

		case 0xc0008e: // ?
		return;
		case 0xc00094: // lamps
		return;
	}
}

static tilemap_callback( layer0 )
{
	UINT16 data = *((UINT16*)(DrvVidRAM[0] + (offs * 2)));

	TILE_SET_INFO(0, data, 0, 0);
}

static tilemap_callback( layer1 )
{
	UINT16 data = *((UINT16*)(DrvVidRAM[1] + (offs * 2)));

	TILE_SET_INFO(1, data, 0, 0);
}

static tilemap_callback( layer2 )
{
	UINT16 data = *((UINT16*)(DrvVidRAM[2] + (offs * 2)));

	TILE_SET_INFO(2, data, 0, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset(0);
	MSM6295Reset(1);

	sound_bankswitch(0); // ?

	memset (scroll, 0, sizeof(scroll));

	pump_target = 0;
	pump_accu = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x040000;

	DrvGfxROM[0]	= Next; Next += 0x200000;
	DrvGfxROM[1]	= Next; Next += 0x200000;
	DrvGfxROM[2]	= Next; Next += 0x100000;
	DrvGfxROM[3]	= Next; Next += 0x200000;

	MSM6295ROM		= Next;
	DrvSndROM[0]	= Next; Next += 0x100000;
	DrvSndROM[1]	= Next; Next += 0x040000;

	BurnPalette		= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	BurnPalRAM		= Next; Next += 0x001000;
	DrvVidRAM[0]	= Next; Next += 0x000800;
	DrvVidRAM[1]	= Next; Next += 0x000800;
	DrvVidRAM[2]	= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM    + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x000001, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x000001, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x000001, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[3] + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x000002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x000003, k++, 4)) return 1;

		if (BurnLoadRom(DrvSndROM[0] + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1] + 0x000000, k++, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM[0],		0x800000, 0x8007ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x840000, 0x840fff, MAP_RAM);
	SekMapMemory(DrvVidRAM[1],		0x880000, 0x8807ff, MAP_RAM);
	SekMapMemory(DrvVidRAM[2],		0x8c0000, 0x8c07ff, MAP_RAM);
	SekMapMemory(BurnPalRAM,		0x900000, 0x900fff, MAP_RAM); // 0-1ff, 400-5ff, 800-9ff, c00-dff
	SekSetWriteByteHandler(0,		musclem_write_byte);
	SekSetWriteWordHandler(0,		musclem_write_word);
	SekSetReadByteHandler(0,		musclem_read_byte);
	SekSetReadWordHandler(0,		musclem_read_word);
	SekClose();

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295Init(1, 1000000 / 132, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);
	MSM6295SetBank(0, DrvSndROM[0], 0x00000, 0x3ffff);
	MSM6295SetBank(1, DrvSndROM[1], 0x00000, 0x3ffff);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, layer2_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 8, 16, 16, 0x200000, 0x000, 0);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 8, 16, 16, 0x200000, 0x400, 0);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 8, 16, 16, 0x100000, 0x600, 0);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 8, 16, 16, 0x200000, 0x200, 0);
	GenericTilemapSetOffsets(0, 16-16, 0);
	GenericTilemapSetOffsets(1,  0-16, 0);
	GenericTilemapSetOffsets(2,  1-16, 0);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit();
	SekExit();

	BurnFreeMemIndex();

	MSM6295ROM = NULL;

	return 0;
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 i = 0; i < 0x200; i++)
	{
		UINT16 code = ram[0x000 + i];

		INT16 sy = (INT16)((ram[0x200 + i] & 0x1ff) << 7) >> 7;
		INT16 sx = (ram[0x400 + i] & 0x3ff);

		sx -= 1 + 16;

		DrawGfxMaskTile(0, 3, code, sx, sy, 0, 0, 0, 0);
		if (sx > 496) DrawGfxMaskTile(0, 3, code, sx - 512, sy, 0, 0, 0, 0);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		BurnPaletteUpdate_xBBBBBGGGGGRRRRR();
		BurnRecalc = 1;
	}

	GenericTilemapSetScrollX(1, scroll[0][0]);
	GenericTilemapSetScrollY(1, scroll[0][1]);
	GenericTilemapSetScrollX(2, scroll[1][0]);
	GenericTilemapSetScrollY(2, scroll[1][1]);

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(2, 0, 0);
	if ( nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);
	if ( nSpriteEnable & 1) draw_sprites();
	if ( nBurnLayer & 4) GenericTilemapDraw(0, 0, 0);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		pump_target = (DrvJoy3[0]) ? 7 : 0;
		UINT8 gray = pump_accu ^ (pump_accu >> 1);

		// Pump is 2-bit "reflected binary / Gray Code"-action!   - dink feb.2026
		DrvJoy1[6] = gray & 1;
		DrvJoy1[5] = (gray >> 1) & 1;
		DrvJoy1[4] = (gray >> 2) & 1;
		//bprintf(0, _T("%x: %x %x %x\n"), pump_accu, DrvJoy1[6], DrvJoy1[5], DrvJoy1[4]);
		// follow target
		if (pump_accu > pump_target) {
			pump_accu = 0;
		} else if (pump_accu < pump_target) {
			pump_accu++;
		}

		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 273;
	INT32 nCyclesTotal[1] = { 12000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (i == 239) SekSetIRQLine(CPU_IRQLINE4, CPU_IRQSTATUS_AUTO);
	}

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(AllRam, RamEnd-AllRam, "All Ram");
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(oki_bank);
		SCAN_VAR(scroll);
		SCAN_VAR(pump_target);
		SCAN_VAR(pump_accu);
	}

	if (nAction & ACB_WRITE) {
		sound_bankswitch(oki_bank);
	}

	return 0;
}



// Muscle Master

static struct BurnRomInfo musclemRomDesc[] = {
	{ "u-71.bin",	0x020000, 0xda0648ad, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "u-67.bin",	0x020000, 0x77f4b6ad, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u-148.bin",	0x100000, 0x97aa308a, 2 | BRF_GRA },           //  2 Foreground Tiles
	{ "u-150.bin",	0x100000, 0x0c7247bd, 2 | BRF_GRA },           //  3

	{ "u-129.bin",	0x100000, 0x8268bba6, 3 | BRF_GRA },           //  4 Midground Tiles
	{ "u-130.bin",	0x100000, 0xbe4755e6, 3 | BRF_GRA },           //  5

	{ "u-134.bin",	0x080000, 0x79c8f776, 4 | BRF_GRA },           //  6 Background Tiles
	{ "u-135.bin",	0x080000, 0x56b89595, 4 | BRF_GRA },           //  7

	{ "u-21.bin",	0x080000, 0xefc2ba0d, 5 | BRF_GRA },           //  8 Sprites
	{ "u-23.bin",	0x080000, 0xa8f96912, 5 | BRF_GRA },           //  9
	{ "u-25.bin",	0x080000, 0x711563c0, 5 | BRF_GRA },           // 10
	{ "u-27.bin",	0x080000, 0x5d6026f3, 5 | BRF_GRA },           // 11

	{ "u-173.bin",	0x100000, 0xd93bc7c2, 6 | BRF_SND },           // 12 Samples #0 (Banked)

	{ "u-177.bin",	0x040000, 0x622c7b2f, 7 | BRF_SND },           // 13 Samples #1
};

STD_ROM_PICK(musclem)
STD_ROM_FN(musclem)

struct BurnDriver BurnDrvMusclem = {
	"musclem", NULL, NULL, NULL, "1997",
	"Muscle Master\0", "Like Flappy Bird. Buttons 1,2 for HS entry, 3 for \"Pump\"-Action", "Inter Geo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_POST90S, GBF_MINIGAMES, 0,
	NULL, musclemRomInfo, musclemRomName, NULL, NULL, NULL, NULL, MusclemInputInfo, MusclemDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	312, 240, 4, 3
};
