// FB Alpha Alien Invaders driver module
// Based on MAME driver by David Haywood, Mariusz Wojcieszek

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv6502ROM;
static UINT8 *DrvProtPROM;
static UINT8 *Drv6502RAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 previrqmask;
static UINT8 irqmask;

static UINT8 DrvJoy1[6];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static struct BurnInputInfo AlinvadeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dips",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Alinvade)

static struct BurnDIPInfo AlinvadeDIPList[]=
{
	{0x06, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"	},
	{0x06, 0x01, 0x03, 0x00, "2"		},
	{0x06, 0x01, 0x03, 0x01, "3"		},
	{0x06, 0x01, 0x03, 0x02, "4"		},
	{0x06, 0x01, 0x03, 0x03, "5"		},

	{0   , 0xfe, 0   ,    2, "×"		},
	{0x06, 0x01, 0x04, 0x00, "Off"		},
	{0x06, 0x01, 0x04, 0x04, "On"		},
};

STDDIPINFO(Alinvade)

static UINT8 alinvade_read(UINT16 address)
{
	switch (address)
	{
		case 0x4000: return (~DrvJoy1[0] & 1) * 0x10;
		case 0x6000: return DrvDips[0];
		case 0x8000: return ( DrvJoy1[1] & 1) * 0x20;
		case 0x8001: return ( DrvJoy1[2] & 1) * 0x20;
		case 0x8002: return ( DrvJoy1[3] & 1) * 0x20;
		case 0x8003: return ( DrvJoy1[4] & 1) * 0x20;
		case 0x8004: return ( DrvJoy1[5] & 1) * 0x20;

		case 0xe800: return 0;
	}

	return 0;
}

static void alinvade_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000: // definitely sound writes of some sort
			DACWrite(0, data);
		return;

		case 0xa000:
		return;		// nop

		case 0xe400:
		return;		// nop

		case 0xe800:
			if (previrqmask == 0 && (data & 1) == 1)
				irqmask ^= 1;
			previrqmask = data & 1;
		return;
	}
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (M6502TotalCycles() / (2000000.000 / 60)));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	DACReset();

	previrqmask = 0;
	irqmask = 1;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv6502ROM		= Next; Next += 0x002000;

	DrvProtPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x0002 * sizeof(UINT32);

	AllRam			= Next;

	Drv6502RAM		= Next; Next += 0x000200;
	DrvVidRAM		= Next; Next += 0x000c00;

	RamEnd			= Next;

	MemEnd			= Next;

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
		if (BurnLoadRom(Drv6502ROM + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x0c00, 1, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x1000, 2, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x1400, 3, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x1800, 4, 1)) return 1;
		if (BurnLoadRom(Drv6502ROM + 0x1c00, 5, 1)) return 1;
	}

	// Protection hack, since prom isn't dumped we replace it with this
	{
		for (INT32 i = 0; i < 0x100; i++) {
			DrvProtPROM[i] = ((i & 0xf) == 0x0f) ? 0x60 : 0xea;
		}
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(Drv6502RAM,	0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,	0x0400, 0x0fff, MAP_RAM);
	for (INT32 i = 0; i < 0x1000; i+=0x100)
		M6502MapMemory(DrvProtPROM,	0xc000+i, 0xc0ff+i, MAP_ROM); // mirrored
	M6502MapMemory(Drv6502ROM,	0xe000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(alinvade_write);
	M6502SetReadHandler(alinvade_read);
	M6502Close();

	DACInit(0, 0, 0, DrvSyncDAC);
	DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	DACExit();

	M6502Exit();

	BurnFree(AllMem);

	return 0;
}

static void draw_bitmap()
{
	for (INT32 offs = 0; offs < (128 * 128)/8; offs++)
	{
		UINT8 sx = (offs << 3) & 0x78;
		INT32 sy = (offs >> 4) & 0x7f;
		UINT8 data = DrvVidRAM[offs];
		UINT16 *dst = pTransDraw + (sy * nScreenWidth) + sx;

		for (INT32 x = 0; x < 8; x++)
		{
			dst[x] = data & 1;
			data >>= 1;
		}
	}
}

static INT32 DrvDraw()
{
	DrvPalette[0] = BurnHighCol(0x00,0x00,0x00, 0);
	DrvPalette[1] = BurnHighCol(0xff,0xff,0xff, 0);

	draw_bitmap();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();

	INT32 nTotalCycles = 2000000 / 60;

	M6502Open(0);
	M6502Run(nTotalCycles);
	if (irqmask) M6502SetIRQLine(0, CPU_IRQSTATUS_AUTO);
	M6502Close();

	if (pBurnSoundOut) {
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		DACScan(nAction,pnMin);

		SCAN_VAR(previrqmask);
		SCAN_VAR(irqmask);
	}

	return 0;
}


// Alien Invaders

static struct BurnRomInfo alinvadeRomDesc[] = {
	{ "alien28.708",	0x0400, 0xde376295, 1 | BRF_PRG | BRF_ESS }, //  0 m6502 Code
	{ "alien29.708",	0x0400, 0x20212977, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "alien30.708",	0x0400, 0x734b691c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "alien31.708",	0x0400, 0x5a70535c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "alien32.708",	0x0400, 0x332dd234, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "alien33.708",	0x0400, 0xe0d57fc7, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "prom",		0x0010, 0x00000000, 2 | BRF_NODUMP        }, //  6 Protection prom
};

STD_ROM_PICK(alinvade)
STD_ROM_FN(alinvade)

struct BurnDriver BurnDrvAlinvade = {
	"alinvade", NULL, NULL, NULL, "198?",
	"Alien Invaders\0", "preliminary sound", "Forbes?", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 1, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, alinvadeRomInfo, alinvadeRomName, NULL, NULL, AlinvadeInputInfo, AlinvadeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	128, 128, 1, 1
};
