// FinalBurn Neo F-E1-32 drier module
// Based on MAME driver by Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "e132xs_intf.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "eeprom.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvBootROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 vblank;

static UINT8 DrvJoy1[32];
static UINT8 DrvJoy2[32];
static UINT8 DrvReset;
static UINT32 DrvInputs[2];

static struct BurnInputInfo Mosaicf2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 23,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 22,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 23,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 16,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 17,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 18,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 19,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 20,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 21,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 22,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 10,	"diag"		},
};

STDINPUTINFO(Mosaicf2)

static void f32_io_write(UINT32 port, UINT32 data)
{
	switch (port)
	{
		case 0x6000:
			MSM6295Write(0, data);
		return;

		case 0x6800:
		case 0x6810:
			BurnYM2151Write((port / 0x10) & 1, data);
		return;

		case 0x7000:
			EEPROMSetClockLine((~data & 1) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE );
		return;

		case 0x7200:
			EEPROMSetCSLine((data & 1) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE );
		return;

		case 0x7400:
			EEPROMWriteBit(data & 1);
		return;
	}
}

static UINT32 f32_io_read(UINT32 port)
{
	switch (port)
	{
		case 0x4000:
			return MSM6295Read(0);

		case 0x4810:
			return BurnYM2151Read();

		case 0x5000:
			return DrvInputs[0];

		case 0x5200:
		{
			UINT32 ret = DrvInputs[1] & ~0x8000;
			if (vblank == 0 && (E132XSGetPC(0) == 0x379de || E132XSGetPC(0) == 0x379cc)) {
				E132XSBurnCycles(100);
			}
			if (!vblank) ret |= 0x8000;
			return ret;
		}

		case 0x5400:
			return EEPROMRead();
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	E132XSOpen(0);
	E132XSReset();
	E132XSClose();

	EEPROMReset();

	BurnYM2151Reset();
	MSM6295Reset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	= Next; Next += 0x1000000;
	DrvBootROM	= Next; Next += 0x100000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x8000 * sizeof(UINT32);

	AllRam		= Next;

	DrvMainRAM	= Next; Next += 0x200000;
	DrvVidRAM	= Next; Next += 0x040000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRomExt(DrvBootROM + 0x0080000, k++, 1, LD_BYTESWAP)) return 1;

		if (BurnLoadRomExt(DrvMainROM + 0x0000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0000002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0400000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0400002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0800000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0800002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0c00000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x0c00002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRomExt(DrvSndROM  + 0x0000000, k++, 1, 0)) return 1;
	}

	E132XSInit(0, TYPE_E132XN, 80000000);
	E132XSOpen(0);
	E132XSMapMemory(DrvMainRAM,			0x00000000, 0x001fffff, MAP_RAM);
	E132XSMapMemory(DrvVidRAM,			0x40000000, 0x4003ffff, MAP_RAM);
	E132XSMapMemory(DrvMainROM,			0x80000000, 0x80ffffff, MAP_ROM);
	E132XSMapMemory(DrvBootROM,			0xfff00000, 0xffffffff, MAP_ROM);
	E132XSSetIOWriteHandler(f32_io_write);
	E132XSSetIOReadHandler(f32_io_read);
	E132XSClose();

	EEPROMInit(&eeprom_interface_93C46);

	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.45, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.45, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 1789773 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	E132XSExit();
	EEPROMExit();
	MSM6295Exit();
	BurnYM2151Exit();

	BurnFreeMemIndex();

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x8000; i++)
	{
		DrvPalette[i] = BurnHighCol(pal5bit(i >> 10), pal5bit(i >> 5), pal5bit(i), 0);
	}
}

static void draw_bitmap()
{
	UINT32 *ram = (UINT32*)DrvVidRAM;

	for (INT32 offs = 0; offs < 0x10000; offs++)
	{
		INT32 sy = offs >> 8;
		INT32 sx = offs & 0xff;

		if ((sx < 0xa0) && (sy < 0xe0))
		{
			pTransDraw[(sy * nScreenWidth) + sx * 2 + 1] = (BURN_ENDIAN_SWAP_INT32(ram[offs]) >> 16) & 0x7fff;
			pTransDraw[(sy * nScreenWidth) + sx * 2 + 0] = (BURN_ENDIAN_SWAP_INT32(ram[offs]) >>  0) & 0x7fff;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_bitmap();

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

		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { 80000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	E132XSOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, E132XS);

		if (i == 239) {
			vblank = 1; // ?
			E132XSSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}

		if (pBurnSoundOut) {
			INT32 nSegment = nBurnSoundLen / nInterleave;
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			MSM6295Render(0, pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			MSM6295Render(0, pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	E132XSClose();

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
		E132XSScan(nAction);

		MSM6295Scan(nAction, pnMin);
	}

	if (nAction & ACB_NVRAM) {
		EEPROMScan(nAction, pnMin);
	}

	return 0;
}


// Mosaic (F2 System)

static struct BurnRomInfo mosaicf2RomDesc[] = {
	{ "rom1.bin",	0x080000, 0xfceb6f83, 1 | BRF_GRA },           //  0 E132XN Boot ROM

	{ "u00.bin",	0x200000, 0xa2329675, 2 | BRF_GRA },           //  1 E132XN Main ROM
	{ "l00.bin",	0x200000, 0xd96fe93b, 2 | BRF_GRA },           //  2
	{ "u01.bin",	0x200000, 0x6379e73f, 2 | BRF_GRA },           //  3
	{ "l01.bin",	0x200000, 0xa269ea82, 2 | BRF_GRA },           //  4
	{ "u02.bin",	0x200000, 0xc17f95cd, 2 | BRF_GRA },           //  5
	{ "l02.bin",	0x200000, 0x69cd9c5c, 2 | BRF_GRA },           //  6
	{ "u03.bin",	0x200000, 0x0e47df20, 2 | BRF_GRA },           //  7
	{ "l03.bin",	0x200000, 0xd79f6ca8, 2 | BRF_GRA },           //  8

	{ "snd.bin",	0x040000, 0x4584589c, 3 | BRF_SND },           //  9 Samples
};

STD_ROM_PICK(mosaicf2)
STD_ROM_FN(mosaicf2)

struct BurnDriver BurnDrvMosaicf2 = {
	"mosaicf2", NULL, NULL, NULL, "1999",
	"Mosaic (F2 System)\0", NULL, "F2 System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mosaicf2RomInfo, mosaicf2RomName, NULL, NULL, NULL, NULL, Mosaicf2InputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};
