// FB Alpha Enerdyne Technologies Triva hardware driver module
// Based on MAME driver by Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvQstROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 b000_ret;
static UINT8 b800_prev;
static UINT8 b000_val;
static UINT8 palreg;
static UINT8 gfx_bank;
static UINT8 question_bank;
static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[1];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo EttriviaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 4"	},

	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Ettrivia)

static struct BurnDIPInfo EttriviaDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x0a, 0x01, 0x01, 0x01, "Off"			},
	{0x0a, 0x01, 0x01, 0x00, "On"			},
};

STDDIPINFO(Ettrivia)

static void b800_write(UINT8 data)
{
	switch (data)
	{
		case 0xc4: b000_ret = AY8910Read(0); break;
		case 0x94: b000_ret = AY8910Read(1); break;
		case 0x86: b000_ret = AY8910Read(2); break;

		case 0x80:
			switch (b800_prev)
			{
				case 0xe0: AY8910Write(0,0,b000_val); break;
				case 0x98: AY8910Write(1,0,b000_val); break;
				case 0x83: AY8910Write(2,0,b000_val); break;
				case 0xa0: AY8910Write(0,1,b000_val); break;
				case 0x88: AY8910Write(1,1,b000_val); break;
				case 0x81: AY8910Write(2,1,b000_val); break;

			}
		break;
	}

	b800_prev = data;
}

static void __fastcall ettrivia_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9000:
			palreg = (data >> 1) & 3;
			gfx_bank = (data >> 2) & 1;
			question_bank = (data >> 3) & 3;
			// coin counter = data & 0x80
			flipscreen = data & 1;
		return;

		case 0x9800:
		case 0xa000:
		return; // nop

		case 0xb000:
			b000_val = data;
		return;

		case 0xb800:
			b800_write(data);
		return;
	}
}

static UINT8 __fastcall ettrivia_read(UINT16 address)
{
	switch (address)
	{
		case 0xb000:
			return (b800_prev) ? b000_ret : b000_val;
	}

	return 0;
}

static UINT8 __fastcall ettrivia_read_port(UINT16 port)
{
	return DrvQstROM[port + (0x10000 * question_bank)];

}

static tilemap_callback( bg )
{
	INT32 code = DrvBgRAM[offs];

	TILE_SET_INFO(0, code + (gfx_bank * 0x100), (code >> 5) + (palreg * 8), 0);
}

static tilemap_callback( fg )
{
	INT32 code = DrvFgRAM[offs];

	TILE_SET_INFO(1, code + (gfx_bank * 0x100), (code >> 5) + (palreg * 8), 0);
}

static UINT8 AY8910_1_portA_read(UINT32)
{
	return DrvInputs[1];
}

static UINT8 AY8910_2_portA_read(UINT32)
{
	return DrvInputs[0];
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);

	b000_ret = 0;
	b800_prev = 0;
	b000_val = 0;
	palreg = 0;
	gfx_bank = 0;
	question_bank = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x008000;

	DrvQstROM		= Next; Next += 0x040000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x008000;
	
	DrvColPROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000800;

	AllRam			= Next;

	DrvBgRAM		= Next; Next += 0x000800;
	DrvFgRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { 0x1000*8, 0 };
	INT32 XOffs[8] = { STEP8(7,-1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x2000);

	GfxDecode(0x0200, 2, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

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
		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  1, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x01000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x01000,  4, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x01000,  6, 1)) return 1;

		if (BurnLoadRom(DrvQstROM  + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvQstROM  + 0x08000,  8, 1)) return 1;
		if (BurnLoadRom(DrvQstROM  + 0x10000,  9, 1)) return 1;
		if (BurnLoadRom(DrvQstROM  + 0x18000, 10, 1)) return 1;
		if (BurnLoadRom(DrvQstROM  + 0x20000, 11, 1)) return 1;
		if (BurnLoadRom(DrvQstROM  + 0x28000, 12, 1)) return 1;
		if (BurnLoadRom(DrvQstROM  + 0x30000, 13, 1)) return 1;
		if (BurnLoadRom(DrvQstROM  + 0x38000, 14, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvNVRAM,			0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,			0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,			0xe000, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(ettrivia_write);
	ZetSetReadHandler(ettrivia_read);
	ZetSetInHandler(ettrivia_read_port);
	ZetClose();

	AY8910Init(0, 2000000, 0);
	AY8910Init(1, 2000000, 1);
	AY8910Init(1, 2000000, 1);
	AY8910SetPorts(1, &AY8910_1_portA_read, NULL, NULL, NULL);
	AY8910SetPorts(2, &AY8910_2_portA_read, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x8000, 0x00, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 2, 8, 8, 0x8000, 0x80, 0x1f);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);
	AY8910Exit(2);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i+0x000] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i+0x100] >> 0) & 0x01;
		INT32 r = (((bit1 * 130) + (bit0 * 270)) * 255) / 400;

		bit0 = (DrvColPROM[i+0x000] >> 2) & 0x01;
		bit1 = (DrvColPROM[i+0x100] >> 2) & 0x01;
		INT32 g = (((bit1 * 130) + (bit0 * 270)) * 255) / 400;

		bit0 = (DrvColPROM[i+0x000] >> 1) & 0x01;
		bit1 = (DrvColPROM[i+0x100] >> 1) & 0x01;
		INT32 b = (((bit1 * 130) + (bit0 * 270)) * 255) / 400;

		DrvPalette[BITSWAP08(i,5,7,6,2,1,0,4,3)] = BurnHighCol(r, g, b, 0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapDraw(0, pTransDraw, 0);
	GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xfe | (DrvDips[0] & 0x01);
		DrvInputs[1] = 0xff;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nCyclesTotal = 2952000 / 60;

	ZetOpen(0);
	ZetRun(nCyclesTotal);
	if (DrvJoy3[0]) {
		ZetNmi();
	} else {
		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}
	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
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

		SCAN_VAR(b000_ret);
		SCAN_VAR(b800_prev);
		SCAN_VAR(b000_val);
		SCAN_VAR(palreg);
		SCAN_VAR(gfx_bank);
		SCAN_VAR(question_bank);
		SCAN_VAR(flipscreen);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00800;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Progressive Music Trivia (Question set 1)

static struct BurnRomInfo promutrvRomDesc[] = {
	{ "u16.u16",			0x8000, 0xe37d48be, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code

	{ "mt44ic.44",			0x1000, 0x8d543ea4, 2 | BRF_GRA },           //  1 Background Tiles
	{ "mt46ic.46",			0x1000, 0x6d6e1f68, 2 | BRF_GRA },           //  2

	{ "mt48ic.48",			0x1000, 0xf2efe300, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "mt50ic.50",			0x1000, 0xee89d24e, 3 | BRF_GRA },           //  4

	{ "ic64.prm",			0x0100, 0x1cf9c914, 4 | BRF_GRA },           //  5 Color Data
	{ "ic63.prm",			0x0100, 0x749da5a8, 4 | BRF_GRA },           //  6

	{ "movie-tv.lo0",		0x8000, 0xdbf03e62, 5 | BRF_GRA },           //  7 Question ROMs
	{ "movie-tv.hi0",		0x8000, 0x77f09aab, 5 | BRF_GRA },           //  8
	{ "scifi.lo1",			0x8000, 0xb5595f81, 5 | BRF_GRA },           //  9
	{ "enter3.hi1",			0x8000, 0xa8cf603b, 5 | BRF_GRA },           // 10
	{ "sports3.lo2",		0x8000, 0xbb28fa92, 5 | BRF_GRA },           // 11
	{ "life-sci.hi2",		0x8000, 0x975d48f4, 5 | BRF_GRA },           // 12
	{ "wars.lo3",			0x8000, 0xc437f9a8, 5 | BRF_GRA },           // 13
	{ "soaps.hi3",			0x8000, 0x9e20614d, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(promutrv)
STD_ROM_FN(promutrv)

struct BurnDriver BurnDrvPromutrv = {
	"promutrv", NULL, NULL, NULL, "1985",
	"Progressive Music Trivia (Question set 1)\0", NULL, "Enerdyne Technologies Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, promutrvRomInfo, promutrvRomName, NULL, NULL, NULL, NULL, EttriviaInputInfo, EttriviaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Progressive Music Trivia (Question set 2)

static struct BurnRomInfo promutrvaRomDesc[] = {
	{ "u16.u16",			0x8000, 0xe37d48be, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code

	{ "mt44ic.44",			0x1000, 0x8d543ea4, 2 | BRF_GRA },           //  1 Background Tiles
	{ "mt46ic.46",			0x1000, 0x6d6e1f68, 2 | BRF_GRA },           //  2

	{ "mt48ic.48",			0x1000, 0xf2efe300, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "mt50ic.50",			0x1000, 0xee89d24e, 3 | BRF_GRA },           //  4

	{ "ic64.prm",			0x0100, 0x1cf9c914, 4 | BRF_GRA },           //  5 Color Data
	{ "ic63.prm",			0x0100, 0x749da5a8, 4 | BRF_GRA },           //  6

	{ "movie-tv.lo0",		0x8000, 0xdbf03e62, 5 | BRF_GRA },           //  7 Question ROMs
	{ "movie-tv.hi0",		0x8000, 0x77f09aab, 5 | BRF_GRA },           //  8
	{ "rock-pop.lo1",		0x8000, 0x4252bc23, 5 | BRF_GRA },           //  9
	{ "rock-pop.hi1",		0x8000, 0x272aba66, 5 | BRF_GRA },           // 10
	{ "country.lo2",		0x8000, 0x44673138, 5 | BRF_GRA },           // 11
	{ "country.hi2",		0x8000, 0x3d35a612, 5 | BRF_GRA },           // 12
	{ "sex.lo3",			0x8000, 0x397b9c47, 5 | BRF_GRA },           // 13
	{ "enter3.hi3",			0x8000, 0xa8cf603b, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(promutrva)
STD_ROM_FN(promutrva)

struct BurnDriver BurnDrvPromutrva = {
	"promutrva", "promutrv", NULL, NULL, "1985",
	"Progressive Music Trivia (Question set 2)\0", NULL, "Enerdyne Technologies Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, promutrvaRomInfo, promutrvaRomName, NULL, NULL, NULL, NULL, EttriviaInputInfo, EttriviaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Progressive Music Trivia (Question set 3)

static struct BurnRomInfo promutrvbRomDesc[] = {
	{ "u16.u16",			0x8000, 0xe37d48be, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code

	{ "mt44.ic44",			0x1000, 0x8d543ea4, 2 | BRF_GRA },           //  1 Background Tiles
	{ "mt46.ic46",			0x1000, 0x6d6e1f68, 2 | BRF_GRA },           //  2

	{ "mt48.ic48",			0x1000, 0xf2efe300, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "mt50.ic50",			0x1000, 0xee89d24e, 3 | BRF_GRA },           //  4

	{ "dm74s287n.ic64",		0x0100, 0x1cf9c914, 4 | BRF_GRA },           //  5 Color Data
	{ "dm74s287n.ic63",		0x0100, 0x749da5a8, 4 | BRF_GRA },           //  6
	
	{ "movie-tv.lo0.u8",	0x8000, 0xdbf03e62, 5 | BRF_GRA },           //  7 Question ROMs
	{ "movie-tv.hi0.u7",	0x8000, 0x77f09aab, 5 | BRF_GRA },           //  8
	{ "rock-pop.lo1.u6",	0x8000, 0x4252bc23, 5 | BRF_GRA },           //  9
	{ "rock-pop.hi1.u5",	0x8000, 0x272aba66, 5 | BRF_GRA },           // 10
	{ "country.lo2.u4",		0x8000, 0x44673138, 5 | BRF_GRA },           // 11
	{ "country.hi2.u3",		0x8000, 0x3d35a612, 5 | BRF_GRA },           // 12
	{ "enter3.lo3.u2",		0x8000, 0xa8cf603b, 5 | BRF_GRA },           // 13
	{ "geninfo.hi3.u1",		0x8000, 0x2747fd74, 5 | BRF_GRA },           // 14

	{ "pal16l8a-ep-0.u10",	0x0104, 0xccbd5f41, 0 | BRF_OPT },           // 15 PLDs
	{ "pal16l8a-ep-0.u9",	0x0104, 0x180e95ad, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(promutrvb)
STD_ROM_FN(promutrvb)

struct BurnDriver BurnDrvPromutrvb = {
	"promutrvb", "promutrv", NULL, NULL, "1985",
	"Progressive Music Trivia (Question set 3)\0", NULL, "Enerdyne Technologies Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, promutrvbRomInfo, promutrvbRomName, NULL, NULL, NULL, NULL, EttriviaInputInfo, EttriviaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Progressive Music Trivia (Question set 4)

static struct BurnRomInfo promutrvcRomDesc[] = {
	{ "u16.u16",			0x8000, 0xe37d48be, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code

	{ "mt44.ic44",			0x1000, 0x8d543ea4, 2 | BRF_GRA },           //  1 Background Tiles
	{ "mt46.ic46",			0x1000, 0x6d6e1f68, 2 | BRF_GRA },           //  2

	{ "mt48.ic48",			0x1000, 0xf2efe300, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "mt50.ic50",			0x1000, 0xee89d24e, 3 | BRF_GRA },           //  4

	{ "dm74s287n.ic64",		0x0100, 0x1cf9c914, 4 | BRF_GRA },           //  5 Color Data
	{ "dm74s287n.ic63",		0x0100, 0x749da5a8, 4 | BRF_GRA },           //  6

	{ "sports.lo0.u8",		0x8000, 0xbb28fa92, 5 | BRF_GRA },           //  7 Question ROMs
	{ "sports2.hi0.u7",		0x8000, 0x4d0107d7, 5 | BRF_GRA },           //  8
	{ "expert.lo1.u6",		0x8000, 0x19153d1a, 5 | BRF_GRA },           //  9
	{ "potpouri.hi1.u5",	0x8000, 0xcbfa6491, 5 | BRF_GRA },           // 10
	{ "country.lo2.u4",		0x8000, 0x44673138, 5 | BRF_GRA },           // 11
	{ "country.hi2.u3",		0x8000, 0x3d35a612, 5 | BRF_GRA },           // 12
	{ "sex3.lo3.u2",		0x8000, 0x1a2322be, 5 | BRF_GRA },           // 13
	{ "geninfo.hi3.u1",		0x8000, 0x2747fd74, 5 | BRF_GRA },           // 14

	{ "pal16l8a-ep-0.u10",	0x0104, 0xccbd5f41, 0 | BRF_OPT },           // 15 PLDs
	{ "pal16l8a-ep-0.u9",	0x0104, 0x180e95ad, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(promutrvc)
STD_ROM_FN(promutrvc)

struct BurnDriver BurnDrvPromutrvc = {
	"promutrvc", "promutrv", NULL, NULL, "1985",
	"Progressive Music Trivia (Question set 4)\0", NULL, "Enerdyne Technologies Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, promutrvcRomInfo, promutrvcRomName, NULL, NULL, NULL, NULL, EttriviaInputInfo, EttriviaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Super Trivia Master

static struct BurnRomInfo strvmstrRomDesc[] = {
	{ "stm16.u16",			0x8000, 0xae734db9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code

	{ "stm44.rom",			0x1000, 0xe69da710, 2 | BRF_GRA },           //  1 Background Tiles
	{ "stm46.rom",			0x1000, 0xd927a1f1, 2 | BRF_GRA },           //  2

	{ "stm48.rom",			0x1000, 0x51719714, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "stm50.rom",			0x1000, 0xcfc1a1d1, 3 | BRF_GRA },           //  4

	{ "stm64.prm",			0x0100, 0x69ebc0b8, 4 | BRF_GRA },           //  5 Color Data
	{ "stm63.prm",			0x0100, 0x305271cf, 4 | BRF_GRA },           //  6

	{ "sex2.lo0",			0x8000, 0x9c68b277, 5 | BRF_GRA },           //  7 Question ROMs
	{ "sports.hi0",			0x8000, 0x3678fb79, 5 | BRF_GRA },           //  8
	{ "movies.lo1",			0x8000, 0x16cba1b7, 5 | BRF_GRA },           //  9
	{ "rock-pop.hi1",		0x8000, 0xe2954db6, 5 | BRF_GRA },           // 10
	{ "sci-fi.lo2",			0x8000, 0xb5595f81, 5 | BRF_GRA },           // 11
	{ "cars.hi2",			0x8000, 0x50310557, 5 | BRF_GRA },           // 12
	{ "potprri.lo3",		0x8000, 0x427eada9, 5 | BRF_GRA },           // 13
	{ "entrtn.hi3",			0x8000, 0xa8cf603b, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(strvmstr)
STD_ROM_FN(strvmstr)

struct BurnDriver BurnDrvStrvmstr = {
	"strvmstr", NULL, NULL, NULL, "1986",
	"Super Trivia Master\0", NULL, "Enerdyne Technologies Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, strvmstrRomInfo, strvmstrRomName, NULL, NULL, NULL, NULL, EttriviaInputInfo, EttriviaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
