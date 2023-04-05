// FinalBurn Neo Goori Goori driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "eeprom.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;

static UINT8 DrvRecalc;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static struct BurnInputInfo GooriInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 6,	"service"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 5,	"diag"			},
};

STDINPUTINFO(Goori)

static void __fastcall goori_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x300000:
		case 0x300002:
			BurnYM2151Write((address / 2) & 1, data);
		return;

		case 0x300004:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x300008:
			// ?
		return;

		case 0x30000f:
			EEPROMWriteBit((data & 0x0004) >> 2);
			EEPROMSetCSLine((~data & 0x0001) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x0002) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;
	}
}

static UINT8 __fastcall goori_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x300002:
			return BurnYM2151Read();

		case 0x300004:
			return MSM6295Read(0);

		case 0x500000:
			return DrvInputs[0];

		case 0x500002:
			return DrvInputs[1];

		case 0x500004:
			return (DrvInputs[2] & ~0x80) | ((EEPROMRead()) ? 0x80 : 0x00);
	}

	return 0;
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(1, (DrvBgRAM[offs * 4 + 1] << 8) + DrvBgRAM[offs * 4 + 3], 0, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	EEPROMReset();

	MSM6295Reset(0);
	BurnYM2151Reset();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x100000;

	DrvGfxROM[0]	= Next; Next += 0x400000;
	DrvGfxROM[1]	= Next; Next += 0x400000;

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x040000;

	BurnPalette		= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	BurnPalRAM		= Next; Next += 0x004000;
	DrvBgRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x002000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes[8] = { STEP4(8,1), STEP4(0,1) };
	INT32 XOffs[16] = { 0,4,16,20,32,36,48,52, 512+0, 512+4, 512+16, 512+20, 512+32, 512+36, 512+48, 512+52 };
	INT32 YOffs[16] = { STEP8(0,8*8), STEP8(1024,8*8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x400000);

	GfxDecode(0x4000, 8, 16, 16, Planes, XOffs, YOffs, 0x800, tmp, DrvGfxROM[0]);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM    + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x200000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x200000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM    + 0x000000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x100000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvBgRAM,			0x400000, 0x400fff, MAP_RAM);
	SekMapMemory(BurnPalRAM,		0x600000, 0x603fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x700000, 0x701fff, MAP_RAM);
	SekSetWriteByteHandler(0,		goori_write_byte);
	SekSetReadByteHandler(0,		goori_read_byte);
	SekClose();

	EEPROMInit(&eeprom_interface_93C46);

	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.45, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.45, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 8, 16, 16, 0x400000, 0x0000, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 8, 16, 16, 0x400000, 0x1f00, 0x1f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM2151Exit();
	MSM6295Exit(0);
	SekExit();
	EEPROMExit();

	BurnFreeMemIndex();

	MSM6295ROM = NULL;

	return 0;
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x2000; i += 16)
	{
		INT32 attr  = DrvSprRAM[i + 0x06];
		INT32 code  = DrvSprRAM[i + 0x0c] | (DrvSprRAM[i + 0x0e] << 8);
		INT32 sx    = DrvSprRAM[i + 0x08] | ((attr & 1) << 8);
		INT32 sy    = DrvSprRAM[i + 0x0a];
		INT32 color = attr >> 3;
		INT32 flipx = DrvSprRAM[i + 0x0e] & 0x80;

		if (sx >= 0x3e0) sx -= 0x400;

		DrawGfxMaskTile(0, 0, code, sx, sy - 16, flipx, 0, color, 0xff);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_xBBBBBGGGGGRRRRR();
		DrvRecalc = 1; // force
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if ( nSpriteEnable & 1) draw_sprites();

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

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nSegment;
	INT32 nInterleave = 262;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[1] = { 16000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };
	
	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == 239) {
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		if (pBurnSoundOut) {
			nSegment = nBurnSoundLen / nInterleave;
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();

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

		BurnYM2151Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);
		EEPROMScan(nAction, pnMin);
	}

	return 0;
}


// Goori Goori

static struct BurnRomInfo gooriRomDesc[] = {
	{ "2",						0x040000, 0x82eae7bf, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "3",						0x040000, 0x39093929, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mx29f1610ml_obj_16m_l",	0x200000, 0xf26451b9, 2 | BRF_GRA },           //  2 Sprites
	{ "mx29f1610ml_obj_16m_h",	0x200000, 0x058ceaec, 2 | BRF_GRA },           //  3

	{ "mx29f1610ml_scr_16m_l",	0x200000, 0x8603a662, 3 | BRF_GRA },           //  4 Background Tiles
	{ "mx29f1610ml_scr_16m_h",	0x200000, 0x4223383e, 3 | BRF_GRA },           //  5

	{ "1",						0x040000, 0xc74351b9, 4 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(goori)
STD_ROM_FN(goori)

struct BurnDriver BurnDrvGoori = {
	"goori", NULL, NULL, NULL, "1999",
	"Goori Goori\0", NULL, "Unico", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, gooriRomInfo, gooriRomName, NULL, NULL, NULL, NULL, GooriInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 224, 4, 3
};

