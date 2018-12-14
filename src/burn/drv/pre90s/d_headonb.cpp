// FB Alpha Head On (Bootleg) driver module
// Based on MAME driver by Hap

#include "tiles_generic.h"
#include "z80_intf.h" // actually i8080A

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvInputs[1];
static UINT8 DrvReset;

static struct BurnInputInfo HeadonbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
};

STDINPUTINFO(Headonb)

static UINT8 __fastcall headonb_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x01:
			return DrvInputs[0] & ~0x40;

		case 0x04:
			return 0xff; // dips?
	}

	return 0;
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], 0, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x004000;

	DrvGfxROM		= Next; Next += 0x004000;

	DrvPalette		= (UINT32*)Next; Next += 0x0002 * sizeof(UINT32);
	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000100;
	DrvVidRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[1] = { 0 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x800);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x0800);

	GfxDecode(0x0100, 1, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree(tmp);

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
		if (BurnLoadRom(DrvZ80ROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0400,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0800,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0c00,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2400,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2800,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2c00,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0400,  9, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0); // actuall i8080A
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM,		0x4000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0xff00, 0xffff, MAP_RAM);
	ZetSetInHandler(headonb_read_port);
	ZetClose();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 1, 8, 8, 0x4000, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnFree(AllMem);

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPalette[0] = 0;
		DrvPalette[1] = BurnHighCol(0xff,0xff,0xff, 0);
		DrvRecalc = 0;
	}

	GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xbf;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 32;
	INT32 nCyclesTotal = 2000000 / 60;
	INT32 nCyclesDone = 0;

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone += ZetRun(nCyclesTotal / nInterleave);
		if (i == 30) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}

	ZetClose();

	if (pBurnSoundOut) {

	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
	}

	return 0;
}


// Head On (bootleg on dedicated hardware)

static struct BurnRomInfo headonbRomDesc[] = {
	{ "1.bin",	0x0400, 0x11586f44, 1 | BRF_PRG | BRF_ESS }, //  0 I8080A Code
	{ "2.bin",	0x0400, 0xc3449b99, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",	0x0400, 0x9c80b99e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.bin",	0x0400, 0xed5ecc4e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.bin",	0x0400, 0x13cdb6da, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6.bin",	0x0400, 0xe498d21b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.bin",	0x0400, 0xce2ef8d9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8.bin",	0x0400, 0x85f216e0, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "10.bin",	0x0400, 0x198f4671, 2 | BRF_GRA },           //  8 Graphics
	{ "9.bin",	0x0400, 0x2b4d3afe, 2 | BRF_GRA },           //  9
};

STD_ROM_PICK(headonb)
STD_ROM_FN(headonb)

struct BurnDriver BurnDrvHeadonb = {
	"headonb", "headon", NULL, NULL, "1979",
	"Head On (bootleg on dedicated hardware)\0", NULL, "bootleg (EFG Sanremo)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, headonbRomInfo, headonbRomName, NULL, NULL, NULL, NULL, HeadonbInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	256, 224, 4, 3
};
