// FB Alpha Wall Crash driver module
// Based on MAME driver by Jarek Burczynski

#include "tiles_generic.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[3];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static UINT8 Dial1;

static struct BurnInputInfo WallcInputList[] = {
	{"P1 Coin",	    BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},

	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 2"	},

	{"Service",     BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Reset",	BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip 1",	BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip 2",	BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Wallc)

static struct BurnDIPInfo WallcDIPList[]=
{
	{0x08, 0xff, 0xff, 0x55, NULL                     	},
	{0x09, 0xff, 0xff, 0x00, NULL                     	},

	{0   , 0xfe, 0   , 4   , "Lives"                  	},
	{0x08, 0x01, 0x03, 0x03, "5"       		  	},
	{0x08, 0x01, 0x03, 0x02, "4"       		  	},
	{0x08, 0x01, 0x03, 0x01, "3"       		  	},
	{0x08, 0x01, 0x03, 0x00, "2"       		  	},

	{0   , 0xfe, 0   , 4   , "Bonus Life"             	},
	{0x08, 0x01, 0x0c, 0x0c, "100K/200K/400K/800K"   	},
	{0x08, 0x01, 0x0c, 0x08, "80K/160K/320K/640K"     	},
	{0x08, 0x01, 0x0c, 0x04, "60K/120K/240K/480K"     	},
	{0x08, 0x01, 0x0c, 0x00, "Off"    		  	},
	
	{0   , 0xfe, 0   , 2   , "Curve Effect"           	},
	{0x08, 0x01, 0x10, 0x10, "Normal"     		  	},
	{0x08, 0x01, 0x10, 0x00, "More"		          	},

	{0   , 0xfe, 0   , 4   , "Timer Speed"            	},
	{0x08, 0x01, 0x60, 0x60, "Slow"     		  	},
	{0x08, 0x01, 0x60, 0x40, "Normal"    		  	},
	{0x08, 0x01, 0x60, 0x20, "Fast"     		  	},
	{0x08, 0x01, 0x60, 0x00, "Super Fast" 		  	},

	{0   , 0xfe, 0   , 2   , "Service" 	          	},
	{0x08, 0x01, 0x80, 0x80, "Free Play and Level Select"	},
	{0x08, 0x01, 0x80, 0x00, "Normal"    		  	},

	{0   , 0xfe, 0   , 4   , "Coin A" 	          	},
	{0x09, 0x01, 0x03, 0x03, "2C 1C"       		  	},
	{0x09, 0x01, 0x03, 0x00, "1C 1C"       		  	},
	{0x09, 0x01, 0x03, 0x01, "1C 2C"       		  	},
	{0x09, 0x01, 0x03, 0x02, "1C 5C"       		  	},
};

STDDIPINFO(Wallc)

void __fastcall wallc_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xb500:
			AY8910Write(0, 0, data);
		break;

		case 0xb600:
			AY8910Write(0, 1, data);
		break;
	}
}

UINT8 __fastcall wallc_read(UINT16 address)
{
	switch (address)
	{
		case 0xb000:
			return DrvDips[0];

		case 0xb200:
			return DrvInputs[0];

		case 0xb400:
			return DrvInputs[1];

		case 0xb600:
			return DrvDips[1];
	}

	return 0;
}

static INT32 DrvDoReset()
{
	DrvReset = 0;

	Dial1 = 0;

	memset(AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { 0x04000, 0x0c000, 0x14000 };
	INT32 XOffs[8] = { 7, 6,  5,  4,  3,  2,  1,  0 };
	INT32 YOffs[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x3000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x3000);

	GfxDecode(0x0100, 3, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree (tmp);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 8; i < 16; i++)
	{
		INT32 bit0,bit1,bit7,r,g,b;

		bit0 = (DrvColPROM[i] >> 5) & 0x01;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		r = ((77 * bit1) + (115 * bit0)) + 1;

		bit0 = (DrvColPROM[i] >> 2) & 0x01;
		bit1 = (DrvColPROM[i] >> 3) & 0x01;
		g = ((77 * bit1) + (115 * bit0)) + 1;

		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit7 = (DrvColPROM[i] >> 7) & 0x01;
		b = ((54 * bit7) + (84 * bit1) + (115 * bit0));

		DrvPalette[i-8] = BurnHighCol(r,g,b,0);
	}
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x008000;

	DrvGfxROM		= Next; Next += 0x004000;

	DrvColPROM		= Next; Next += 0x000020;

	DrvPalette		= (UINT32*)Next; Next += 0x0008 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit(INT32 incr)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM + 0x0000,        0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2000,        1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x0000 + incr, 2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x1000 + incr, 3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x2000 + incr, 4, 1)) return 1;

		if (BurnLoadRom(DrvColPROM,                5, 1)) return 1;

		DrvPaletteInit();
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);

	ZetMapMemory(DrvZ80ROM, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM, 0x8000, 0x83ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM, 0x8400, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM, 0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(DrvVidRAM, 0x8c00, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM, 0xa000, 0xa3ff, MAP_RAM);

	ZetSetWriteHandler(wallc_write);
	ZetSetReadHandler(wallc_read);
	ZetClose();

	AY8910Init(0, 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);

	BurnFree (AllMem);

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	for (INT32 offs = 0; offs < 0x400; offs ++)
	{
		INT32 sy   = (~offs & 0x1f) << 3;
		INT32 sx   = ( offs >> 5) << 3;
		INT32 code = DrvVidRAM[offs];

		Render8x8Tile_Clip(pTransDraw, code, sx, sy, 0, 0, 0, DrvGfxROM);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xff;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

		if (DrvJoy2[0]) Dial1 += 0x02;
		if (DrvJoy2[1]) Dial1 -= 0x02;

		// bounds 0x50 - 0xcf
		if (Dial1 > 0xcf) Dial1 = 0xcf;
		if (Dial1 < 0x50) Dial1 = 0x50;

		DrvInputs[1] = Dial1;

	}

	ZetOpen(0);
	ZetRun(3000000 / 60);
	ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
	ZetRun(72000 / 60);
	ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
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

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(Dial1);
	}

	return 0;
}


// Wall Crash (set 1)

static struct BurnRomInfo wallcRomDesc[] = {
	{ "wac05.h7",	0x2000, 0xab6e472e, 0 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "wac1-52.h6",	0x2000, 0x988eaa6d, 0 | BRF_ESS | BRF_PRG }, //  1

	{ "wc1.e3",	0x1000, 0xca5c4b53, 1 | BRF_GRA },	     //  2 Graphics
	{ "wc2.e2",	0x1000, 0xb7f52a59, 1 | BRF_GRA },	     //  3
	{ "wc3.e1",	0x1000, 0xf6854b3a, 1 | BRF_GRA },	     //  4

	{ "74s288.c2",  0x0020, 0x83e3e293, 2 | BRF_GRA },	     //  5 Color Prom
};

STD_ROM_PICK(wallc)
STD_ROM_FN(wallc)

static void wallcDecode()
{
	for (INT32 i = 0; i < 0x4000; i++) {
		DrvZ80ROM[i] = BITSWAP08(DrvZ80ROM[i] ^ 0xaa, 4,2,6,0,7,1,3,5);
	}
}

static INT32 wallcInit()
{
	INT32 nRet = DrvInit(0);

	if (nRet == 0) {
		wallcDecode();
	}

	return nRet;
}

struct BurnDriver BurnDrvWallc = {
	"wallc", NULL, NULL, NULL, "1984",
	"Wall Crash (set 1)\0", NULL, "Midcoin", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, wallcRomInfo, wallcRomName, NULL, NULL, WallcInputInfo, WallcDIPInfo,
	wallcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x08,
	256, 256, 4, 3
};


// Wall Crash (set 2)

static struct BurnRomInfo wallcaRomDesc[] = {
	{ "rom4.rom",	0x2000, 0xce43af1b, 0 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "rom5.rom",	0x2000, 0xb789a705, 0 | BRF_ESS | BRF_PRG }, //  1

	{ "rom3.rom",	0x0800, 0x6634db73, 1 | BRF_GRA },	     //  2 Graphics
	{ "rom2.rom",	0x0800, 0x79f49c2c, 1 | BRF_GRA },	     //  3
	{ "rom1.rom",   0x0800, 0x3884fd4f, 1 | BRF_GRA },	     //  4

	{ "74s288.c2",	0x0020, 0x83e3e293, 2 | BRF_GRA },	     //  5 Color Prom
};

STD_ROM_PICK(wallca)
STD_ROM_FN(wallca)

static void wallcaDecode()
{
	for (INT32 i = 0; i < 0x4000; i++) {
		if (i & 0x100) {
			DrvZ80ROM[i] = BITSWAP08(DrvZ80ROM[i] ^ 0x4a, 4,7,1,3,2,0,5,6);
		} else {
			DrvZ80ROM[i] = BITSWAP08(DrvZ80ROM[i] ^ 0xa5, 0,2,3,6,1,5,7,4);
		}
	}
}

static INT32 wallcaInit()
{
	INT32 nRet = DrvInit(0x800);

	if (nRet == 0) {
		wallcaDecode();
	}

	return nRet;
}

struct BurnDriver BurnDrvWallca = {
	"wallca", "wallc", NULL, NULL, "1984",
	"Wall Crash (set 2)\0", NULL, "Midcoin", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, wallcaRomInfo, wallcaRomName, NULL, NULL, WallcInputInfo, WallcDIPInfo,
	wallcaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x08,
	256, 256, 4, 3
};
