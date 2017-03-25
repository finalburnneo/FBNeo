// FB Alpha Canyon Bomber driver module
// Based on MAME driver by Mike Balfour

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "burn_led.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 watchdog;
static UINT8 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo CanyonInputList[] = {
	{"Highscore Reset",	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 7"	},
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Tilt",		BIT_DIGITAL,	DrvJoy2 + 7,	"tilt"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 5,	"service 1"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Canyon)

static struct BurnDIPInfo CanyonDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x80, NULL			},

	{0   , 0xfe, 0   ,    4, "Language"		},
	{0x0a, 0x01, 0x03, 0x00, "English"		},
	{0x0a, 0x01, 0x03, 0x01, "Spanish"		},
	{0x0a, 0x01, 0x03, 0x02, "French"		},
	{0x0a, 0x01, 0x03, 0x03, "German"		},

	{0   , 0xfe, 0   ,    4, "Misses Per Play"	},
	{0x0a, 0x01, 0x30, 0x00, "3"			},
	{0x0a, 0x01, 0x30, 0x10, "4"			},
	{0x0a, 0x01, 0x30, 0x20, "5"			},
	{0x0a, 0x01, 0x30, 0x30, "6"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0a, 0x01, 0xc0, 0xc0, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0xc0, 0x80, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x0a, 0x01, 0xc0, 0x00, "Free Play"		},
};

STDDIPINFO(Canyon)

static void canyon_write(UINT16 address, UINT8 /*data*/)
{
	address &= 0x3fff;

	switch (address & 0x3fff)
	{
		case 0x0400:
		case 0x0401:
			// motor
		return;

		case 0x0500:
			// explode
		return;

		case 0x0501:
			watchdog = 0;
		return;

		case 0x0600:
		case 0x0601:
		case 0x0602:
		case 0x0603:
			// whistle
		return;

		case 0x0680:
		case 0x0681:
		case 0x0682:
		case 0x0683:
			BurnLEDSetStatus(address & 1, address & 2);
		return;

		case 0x0700:
		case 0x0701:
		case 0x0702:
		case 0x0703:
			// attract
		return;
	}
}

static UINT8 canyon_inputs(UINT8 offset)
{
	UINT8 val = 0;
	UINT8 p2 = (DrvInputs[1] & 0xdf) | (vblank << 5);

	if ((p2 >> (offset & 7)) & 1)
		val |= 0x80;

	if ((DrvInputs[0] >> (offset & 3)) & 1)
		val |= 0x01;

	return val;
}



static UINT8 canyon_read(UINT16 address)
{
	if ((address & 0xf800) == 0x1000) {
		return canyon_inputs(address);
	}

	if ((address & 0xf800) == 0x1800) {
		return (DrvDips[0] >> (2 * (~address & 3))) & 3;
	}

	return 0;
}

static tilemap_callback( background )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], DrvVidRAM[offs] >> 7, 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	M6502Reset();
	M6502Close();

	BurnLEDReset();

	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x001000;
	DrvGfxROM1		= Next; Next += 0x000800;
	DrvGfxROM2		= Next; Next += 0x000004;

	DrvPalette		= (UINT32*)Next; Next += 0x0004 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x000100;
	DrvVidRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[1]   = { 0 };
	INT32 XOffs0[8]  = { STEP4(4,1), STEP4(12,1) };
	INT32 XOffs1[32] = { STEP8(7,-1), STEP8(15,-1), STEP8(256+7,-1), STEP8(256+15,-1) };
	INT32 YOffs[16]  = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x0400);

	GfxDecode(0x40, 1,  8,  8, Plane, XOffs0, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x0100);

	GfxDecode(0x04, 1, 32, 16, Plane, XOffs1, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static void FixNibbles(UINT8 *rom, INT32 len)
{
	for (INT32 i = 0; i < len; i+=2) {
		rom[i/2] = (rom[i+0] & 0xf) | (rom[i+1] << 4);
	}
}

static INT32 DrvInit(INT32 select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (select == 0)
	{
		if (BurnLoadRom(DrvM6502ROM + 0x0000, 0, 2)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0001, 1, 2)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0800, 2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000, 3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000, 4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0001, 5, 2)) return 1;

		FixNibbles(DrvM6502ROM, 0x800);
	}

	if (select == 1)
	{
		if (BurnLoadRom(DrvM6502ROM + 0x0000, 0, 2)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0001, 1, 2)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x1000, 2, 2)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x1001, 3, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000, 4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000, 5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0001, 6, 2)) return 1;

		FixNibbles(DrvM6502ROM, 0x2000);
	}

	memset (DrvGfxROM2, 1, 4); // bombs
	FixNibbles(DrvGfxROM1,  0x200);
	DrvGfxDecode();

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,	0x0000, 0x00ff, MAP_RAM);
	M6502MapMemory(DrvM6502RAM,	0x0100, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,	0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,	0x3000, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM,	0xf000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(canyon_write);
	M6502SetReadHandler(canyon_read);
	M6502Close();

	BurnLEDInit(2, LED_POSITION_BOTTOM_RIGHT, LED_SIZE_2x2, LED_COLOR_GREEN, 50);

	// descrete sound

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 1, 8, 8, 0x1000, 0, 1);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();
	BurnLEDExit();

	// descrete sound

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	DrvPalette[0] = BurnHighCol(0x80,0x80,0x80,0);
	DrvPalette[1] = BurnHighCol(0x00,0x00,0x00,0);
	DrvPalette[2] = DrvPalette[0];
	DrvPalette[3] = BurnHighCol(0xff,0xff,0xff,0);
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 2; i++)
	{
		INT32 sx = DrvVidRAM[0x3d1 + 2 * i];
		INT32 sy = DrvVidRAM[0x3d8 + 2 * i];
		INT32 code = DrvVidRAM[0x3d9 + 2 * i];
		INT32 flipx = ~code & 0x80;

		if (flipx) {
			RenderCustomTile_Mask_FlipX_Clip(pTransDraw, 32, 16, (code >> 3) & 3, 224 - sx, 240 - sy, i, 1, 0, 0, DrvGfxROM1);
		} else {
			RenderCustomTile_Mask_Clip(pTransDraw, 32, 16, (code >> 3) & 3, 224 - sx, 240 - sy, i, 1, 0, 0, DrvGfxROM1);
		}
	}
}

static void draw_bombs()
{
	for (INT32 i = 0; i < 2; i++)
	{
		INT32 sx = 254 - DrvVidRAM[0x3d5 + 2 * i];
		INT32 sy = 246 - DrvVidRAM[0x3dc + 2 * i];

		RenderCustomTile_Clip(pTransDraw, 4, 4, 0, sx, sy, i, 1, 0, DrvGfxROM2);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapDraw(0, pTransDraw, 0);

	draw_sprites();
	draw_bombs();

	BurnTransferCopy(DrvPalette);
	BurnLEDRender();

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nCyclesTotal = 756000 / 60;

	M6502Open(0);
	vblank = 0;
	M6502Run((nCyclesTotal * 240)/256);
	vblank = 1;
	M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
	M6502Run((nCyclesTotal *  16)/256);
	M6502Close();

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

		M6502Scan(nAction);
	}

	return 0;
}


// Canyon Bomber

static struct BurnRomInfo canyonRomDesc[] = {
	{ "9499-01.j1",		0x0400, 0x31800767, 1 | BRF_PRG | BRF_ESS }, //  0 m6502 Code
	{ "9503-01.p1",		0x0400, 0x1eddbe28, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "9496-01.d1",		0x0800, 0x8be15080, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "9492-01.n8",		0x0400, 0x7449f754, 2 | BRF_GRA },           //  3 Background Tiles

	{ "9506-01.m5",		0x0100, 0x0d63396a, 3 | BRF_GRA },           //  4 Sprites
	{ "9505-01.n5",		0x0100, 0x60507c07, 3 | BRF_GRA },           //  5

	{ "9491-01.j6",		0x0100, 0xb8094b4c, 0 | BRF_OPT },           //  6 Unused proms
};

STD_ROM_PICK(canyon)
STD_ROM_FN(canyon)

static INT32 CanyonInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvCanyon = {
	"canyon", NULL, NULL, NULL, "1977",
	"Canyon Bomber\0", "No sound", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, canyonRomInfo, canyonRomName, NULL, NULL, CanyonInputInfo, CanyonDIPInfo,
	CanyonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 4,
	256, 240, 4, 3
};


// Canyon Bomber (prototype)

static struct BurnRomInfo canyonpRomDesc[] = {
	{ "cbp3000l.j1",	0x0800, 0x49cf29a0, 1 | BRF_PRG | BRF_ESS }, //  0 m6502 Code
	{ "cbp3000m.p1",	0x0800, 0xb4385c23, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cbp3800l.h1",	0x0800, 0xc7ee4431, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cbp3800m.r1",	0x0800, 0x94246a9a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "9492-01.n8",		0x0400, 0x7449f754, 2 | BRF_GRA },           //  4 Background Tiles

	{ "9506-01.m5",		0x0100, 0x0d63396a, 3 | BRF_GRA },           //  5 Sprites
	{ "9505-01.n5",		0x0100, 0x60507c07, 3 | BRF_GRA },           //  6

	{ "9491-01.j6",		0x0100, 0xb8094b4c, 0 | BRF_OPT },           //  7 Unused proms
};

STD_ROM_PICK(canyonp)
STD_ROM_FN(canyonp)

static INT32 CanyonpInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvCanyonp = {
	"canyonp", "canyon", NULL, NULL, "1977",
	"Canyon Bomber (prototype)\0", "No sound", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, canyonpRomInfo, canyonpRomName, NULL, NULL, CanyonInputInfo, CanyonDIPInfo,
	CanyonpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 4,
	256, 240, 4, 3
};
