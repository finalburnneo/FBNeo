// FinalBurn Neo Tank Battalion driver module
// Based on MAME driver by Brad Oliver

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "samples.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvM6502RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nmi_enable;
static INT32 engine_enable;
static INT32 engine_hi;
static INT32 engine_playing;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT32 nCyclesExtra;

static struct BurnInputInfo TankbattInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"		},

	{"Service",			BIT_DIGITAL,	DrvJoy1 + 7,	"service"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dips A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dips B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
};

STDINPUTINFO(Tankbatt)

static struct BurnDIPInfo TankbattDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x8f, NULL					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x01, 0x01, "Upright"				},
	{0x01, 0x01, 0x01, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x01, 0x01, 0x06, 0x04, "2 Coins/1 Credit"		},
	{0x01, 0x01, 0x06, 0x06, "1 Coin/1 Credit"		},
	{0x01, 0x01, 0x06, 0x02, "1 Coin/2 Credits"		},
	{0x01, 0x01, 0x06, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x00, "10000"				},
	{0x01, 0x01, 0x18, 0x10, "15000"				},
	{0x01, 0x01, 0x18, 0x08, "20000"				},
	{0x01, 0x01, 0x18, 0x18, "None"					},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x01, 0x01, 0x20, 0x20, "2"					},
	{0x01, 0x01, 0x20, 0x00, "3"					},
};

STDDIPINFO(Tankbatt)

static void update_engine()
{
	if (engine_enable) {
		if (engine_playing != 2 + engine_hi) {
			splaych(0, 2 + engine_hi, 0.50, 0, true);
		}
		engine_playing = 2 + engine_hi;
	} else {
		sstopch(0);
		engine_playing = 0;
	}
}

static void tankbatt_main_write(UINT16 address, UINT8 data)
{
//	if (address != 0xc18 && address != 0xc10 && address != 0xc02) bprintf(0, _T("%x:  %x\n"), address, data);
	switch (address & ~0xd3e0)
	{
		case 0x0c00: // led0
		case 0x0c01: // led1
		case 0x0c02: // coin counter
		case 0x0c03: // coin lockout
		case 0x0c04: // nc
		case 0x0c05: // nc
		case 0x0c06: // nc
		case 0x0c07: // nc
		return;

		case 0x0c08:
			if (data & 1) splay(0, 0.15, 0, 0);
		return;

		case 0x0c09:
			if (data & 1) splay(1, 0.15, 0, 0);
		return;

		case 0x0c0a: // /engine_lo
			engine_enable = ~data & 1;
			update_engine();
		return;

		case 0x0c0b: // engine_hi
			engine_hi = data & 1;
			update_engine();
		return;

		case 0x0c0c: // shoot
			if (data & 1) splay(4, 1.0, 0, 0);
		return;

		case 0x0c0d: // explode
			if (data & 1) splay(5, 1.0, 0, 0);
		return;

		case 0x0c0e: // nc
		return;

		case 0x0c0f:
			nmi_enable = data;
		return;

		case 0x0c10:
		case 0x0c11:
		case 0x0c12:
		case 0x0c13:
		case 0x0c14:
		case 0x0c15:
		case 0x0c16:
		case 0x0c17:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x0c18:
		case 0x0c19:
		case 0x0c1a:
		case 0x0c1b:
		case 0x0c1c:
		case 0x0c1d:
		case 0x0c1e:
		case 0x0c1f:
			BurnWatchdogWrite();
		return;
	}
}

static UINT8 tankbatt_main_read(UINT16 address)
{
	switch (address & ~0xd3e7)
	{
		case 0x0c00:
			return ((DrvInputs[0] >> (address & 7)) & 1) << 7;

		case 0x0c08:
			return ((DrvInputs[1] >> (address & 7)) & 1) << 7;

		case 0x0c18:
			return ((DrvDips[1] >> (address & 7)) & 1) << 7;
	}

	return 0;
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], DrvVidRAM[offs] | 1, 0);
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	M6502Reset();
	M6502Close();

	BurnSampleReset();

	BurnWatchdogReset();

	nmi_enable = 0;
	engine_enable = 0;
	engine_hi = 0;
	engine_playing = 0;
	nCyclesExtra = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x002000;

	DrvGfxROM		= Next; Next += 0x004000;

	DrvColPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x000400;
	DrvM6502RAM		= Next; Next += 0x000800;

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

	memcpy (tmp, DrvGfxROM, 0x800);

	GfxDecode(0x0100, 1, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvM6502ROM + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0800, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x1800, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM   + 0x0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x3fff);
	M6502MapMemory(DrvM6502RAM,				0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,				0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvM6502RAM,				0x1000, 0x17ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,				0x1800, 0x1bff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,				0x2000, 0x3fff, MAP_ROM);
	M6502SetReadHandler(tankbatt_main_read);
	M6502SetWriteHandler(tankbatt_main_write);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 16);

	BurnSampleInit(0);
	BurnSampleSetBuffered(M6502TotalCycles, 768000);
	BurnSampleSetAllRoutesAllSamples(1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 1, 8, 8, 0x4000, 0, 0xff);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();
	BurnSampleExit();
	BurnWatchdogExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = ((DrvColPROM[i] >> 0) & 1); // intensity
		INT32 bit1 = ((DrvColPROM[i] >> 1) & 1); // red
		INT32 bit2 = ((DrvColPROM[i] >> 2) & 1); // green
		INT32 bit3 = ((DrvColPROM[i] >> 3) & 1); // blue

		INT32 r = bit1 * (0xc0 + (0x3f * bit0));
		INT32 g = bit2 * (0xc0 + (0x3f * bit0));
		INT32 b = bit3 * (0xc0 + (0x3f * bit0));

		DrvPalette[i * 2 + 0] = 0;
		DrvPalette[i * 2 + 1] = BurnHighCol(r,g,b,0);
	}
}

static void draw_bullets()
{
	for (INT32 offs = 0; offs < 0x10; offs += 2)
	{
		INT32 sx = DrvM6502RAM[offs + 1];
		INT32 sy = 255 - DrvM6502RAM[offs] - 2 - 16;

		for (INT32 y = 0; y < 3; y++)
		{
			for (INT32 x = 0; x < 3; x++)
			{
				if ((sy + y) >= 0 && (sy + y) < nScreenHeight && (sx + x) >= 0 && (sx + x) < nScreenWidth)
				{
					pTransDraw[((sy + y) * nScreenWidth) + sx + x] = 0x1ff;
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_bullets();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		DrvInputs[1] = (DrvInputs[1] & 0x7f) | (DrvDips[0] & 0x80);
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[1] = { 768000 / 60 };
	INT32 nCyclesDone[1] = { nCyclesExtra };

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
        CPU_RUN(0, M6502);
		if (i == 239 && nmi_enable) {
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		}

		if ((i & 0x1f) == 0x1f) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		}
	}

	M6502Close();

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029727;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		M6502Scan(nAction);
		BurnSampleScan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		SCAN_VAR(nmi_enable);
		SCAN_VAR(engine_enable);
		SCAN_VAR(engine_hi);
		SCAN_VAR(engine_playing);
		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


static struct BurnSampleInfo TankbattSampleDesc[] = {
	{ "boop", 		SAMPLE_NOLOOP },
	{ "beep", 		SAMPLE_NOLOOP },
	{ "engine_lo", 	SAMPLE_NOLOOP },
	{ "engine_hi", 	SAMPLE_NOLOOP },
	{ "shoot", 		SAMPLE_NOLOOP },
	{ "explode", 	SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Tankbatt)
STD_SAMPLE_FN(Tankbatt)


// Tank Battalion

static struct BurnRomInfo tankbattRomDesc[] = {
	{ "tb1-1.1a",		0x000800, 0x278a0b8c,  1 | BRF_PRG | BRF_ESS },	//  0 M6502 Code
	{ "tb1-2.1b",		0x000800, 0xe0923370,  1 | BRF_PRG | BRF_ESS },	//  1 
	{ "tb1-3.1c",		0x000800, 0x85005ea4,  1 | BRF_PRG | BRF_ESS },	//  2 
	{ "tb1-4.1d",		0x000800, 0x3dfb5bcf,  1 | BRF_PRG | BRF_ESS },	//  3 

	{ "tb1-5.2k",		0x000800, 0xaabd4fb1,  2 | BRF_GRA },			//  4 Characters

	{ "bct1-1.l3",		0x000100, 0xd17518bc,  3 | BRF_GRA },			//  5 Color PROM
};

STD_ROM_PICK(tankbatt)
STD_ROM_FN(tankbatt)

struct BurnDriver BurnDrvTankbatt = {
	"tankbatt", NULL, NULL, "tankbatt", "1980",
	"Tank Battalion\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, tankbattRomInfo, tankbattRomName, NULL, NULL, TankbattSampleInfo, TankbattSampleName, TankbattInputInfo, TankbattDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Tank Battalion (bootleg)

static struct BurnRomInfo tankbattbRomDesc[] = {
	{ "tb1-1.1a",		0x000800, 0x278a0b8c,  1 | BRF_PRG | BRF_ESS },	//  0 M6502 Code
	{ "tb1-2.1b",		0x000800, 0xe0923370,  1 | BRF_PRG | BRF_ESS },	//  1 
	{ "tb1-3.1c",		0x000800, 0x85005ea4,  1 | BRF_PRG | BRF_ESS },	//  2 
	{ "tb1-4.1d",		0x000800, 0x3dfb5bcf,  1 | BRF_PRG | BRF_ESS },	//  3 

	{ "e.2k",			0x000800, 0x249f4e1b,  2 | BRF_GRA },			//  4 Characters

	{ "bct1-1.l3",		0x000100, 0xd17518bc,  3 | BRF_GRA },			//  5 Color PROM
};

STD_ROM_PICK(tankbattb)
STD_ROM_FN(tankbattb)

struct BurnDriver BurnDrvTankbattb = {
	"tankbattb", "tankbatt", NULL, "tankbatt", "1980",
	"Tank Battalion (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, tankbattbRomInfo, tankbattbRomName, NULL, NULL, TankbattSampleInfo, TankbattSampleName, TankbattInputInfo, TankbattDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
