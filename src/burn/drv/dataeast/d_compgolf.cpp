// FB Neo Competition Golf Final Round driver module
// Based on MAME driver by Angelo Salese, Pierpaolo Prazzoli, and Bryan McPhail

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "burn_ym2203.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bankdata;
static UINT16 scrollx;
static UINT16 scrolly;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x1f, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x00, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x00, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Wind Force"			},
	{0x00, 0x01, 0x10, 0x10, "Normal"				},
	{0x00, 0x01, 0x10, 0x00, "Strong"				},

	{0   , 0xfe, 0   ,    2, "Grain of Turf"		},
	{0x00, 0x01, 0x20, 0x20, "Normal"				},
	{0x00, 0x01, 0x20, 0x00, "Strong"				},

	{0   , 0xfe, 0   ,    2, "Range of Ball"		},
	{0x00, 0x01, 0x40, 0x40, "Normal"				},
	{0x00, 0x01, 0x40, 0x00, "Less"					},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x04, 0x00, "Upright"				},
	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},
};

STDDIPINFO(Drv)

static void control_write(INT32 data)
{
	INT32 bank = (data >> 2) & 1;

	bankdata = data;

	M6809MapMemory(DrvM6809ROM + 0x8000 + (bank * 0x4000), 0x4000, 0x7fff, MAP_ROM);

	scrollx = (scrollx & 0x0ff) | ((data & 1) << 8);
	scrolly = (scrolly & 0x0ff) | ((data & 2) << 7);
}

static void compgolf_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3001:
			control_write(data);
		return;

		case 0x3800:
		case 0x3801:
			BurnYM2203Write(0, address & 1, data);
		return;
	}
}

static UINT8 compgolf_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			return DrvInputs[0];

		case 0x3001:
			return (DrvInputs[1] & 0x7f) | (vblank ? 0x80 : 0);

		case 0x3002:
			return DrvDips[0];

		case 0x3003:
			return (DrvInputs[2] & 0xe0) | (DrvDips[1] & 0x1f);

		case 0x3800:
		case 0x3801:
			return BurnYM2203Read(0, address & 1);
	}

	return 0;
}

static tilemap_callback( text )
{
	INT32 attr = DrvVidRAM[offs * 2 + 0];
	INT32 code = DrvVidRAM[offs * 2 + 1] | ((attr & 0x03) << 8);
	INT32 color = (attr >> 2) & 0x3f;

	TILE_SET_INFO(2, code, color, 0);
}

static tilemap_callback( background )
{
	INT32 attr = DrvBgRAM[offs * 2 + 0];
	INT32 code = DrvBgRAM[offs * 2 + 1] + ((attr & 1) << 8);
	INT32 color = (attr & 0x3e) >> 1;

	TILE_SET_INFO(1, code, color, 0);
}

static tilemap_scan( background )
{
	return (col & 0x0f) + ((row & 0x0f) << 4) + ((col & 0x10) << 4) + ((row & 0x10) << 5);
}

static void scrollx_write(UINT32, UINT32 data)
{
	scrollx = (scrollx & 0x100) | data;
}

static void scrolly_write(UINT32, UINT32 data)
{
	scrolly = (scrolly & 0x100) | data;
}

static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	M6809SetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	control_write(0);
	M6809Reset();
	BurnYM2203Reset();
	M6809Close();

	scrollx = 0;
	scrolly = 0;
	bankdata = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6809RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvBgRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void expand_background()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);

	memcpy (tmp, DrvGfxROM1 + 0x8000, 0x4000);

	for (INT32 x = 0; x < 0x4000; x++)
	{
		DrvGfxROM1[0x8000 + x]  = tmp[x] << 4;
		DrvGfxROM1[0xc000 + x]  = tmp[x] & 0xf0;
	}

	BurnFree (tmp);
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3]  = { 0, (0x8000*8*1), (0x8000*8*2) };
	INT32 XOffs0[16] = { STEP8(8*8*2,1), STEP8(8*8*0,1) };
	INT32 YOffs0[16] = { STEP8(8*8*0,8), STEP8(8*8*1,8) };

	INT32 Plane1[3]  = { (0x8000*8), 0, 4 };
	INT32 XOffs1[16] = { STEP4(0,1), STEP4(128,1), STEP4(256,1), STEP4(384,1) };
	INT32 YOffs1[16] = { STEP16(0,8) };

	INT32 Plane2[3]  = { (0x4000*8)+4, 0, 4 };
	INT32 XOffs2[8]  = { STEP4(0,1), STEP4(64,1) };
	INT32 YOffs2[8]  = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x18000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x18000);

	GfxDecode(0x0400, 3, 16, 16, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x10000);

	GfxDecode(0x0200, 3, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x08000);

	GfxDecode(0x0400, 3,  8,  8, Plane2, XOffs2, YOffs2, 0x080, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x08000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x10000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x08000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2  + 0x00000,  7, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000,  8, 1)) return 1;

		expand_background();
		DrvGfxDecode();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,	0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,	0x1000, 0x17ff, MAP_RAM);
	M6809MapMemory(DrvBgRAM,	0x1800, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,	0x2000, 0x20ff, MAP_RAM); // 0-60
	M6809MapMemory(DrvM6809ROM,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(compgolf_write);
	M6809SetReadHandler(compgolf_read);
	M6809Close();

	BurnYM2203Init(1, 1500000, DrvYM2203IRQHandler, 0);
	BurnYM2203SetPorts(0, NULL, NULL, scrollx_write, scrolly_write);
	BurnTimerAttachM6809(2000000);
	BurnYM2203SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	GenericTilemapInit(0, background_map_scan, background_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, text_map_callback, 8, 8, 32, 32);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(0, -8, -8);
	GenericTilemapSetOffsets(1, -8, -8);

	GenericTilemapSetGfx(1, DrvGfxROM1, 3, 16, 16, 0x40000, 0, 0x1f);
	GenericTilemapSetGfx(2, DrvGfxROM2, 3,  8,  8, 0x10000, 0, 0x0f);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();

	BurnYM2203Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x60; offs += 4)
	{
		INT32 code  = DrvSprRAM[offs + 1] + ((DrvSprRAM[offs] & 0xc0) << 2);
		INT32 sx    = 240 - DrvSprRAM[offs + 3];
		INT32 sy    = DrvSprRAM[offs + 2];
		INT32 color = (DrvSprRAM[offs] & 0x08) >> 3;
		INT32 flipx = DrvSprRAM[offs] & 0x04;
		INT32 high  = DrvSprRAM[offs] & 0x10;

		sx -= 8;
		sy -= 8;

		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, 0, color, 3, 0, 0, DrvGfxROM0);
		if (high) Draw16x16MaskTile(pTransDraw, code + 1, sx, sy + 16, flipx, 0, color, 3, 0, 0, DrvGfxROM0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollY(0, scrolly);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 2000000 / 60 };

	M6809Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 248) {
			vblank = 1;
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		}

		CPU_RUN_TIMER(0);
	}

	M6809Close();

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(bankdata);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
	}

	if (nAction & ACB_WRITE)
	{
		M6809Open(0);
		control_write(bankdata);
		M6809Close();
	}

	return 0;
}


// Competition Golf Final Round (revision 3)

static struct BurnRomInfo compgolfRomDesc[] = {
	{ "cv05-3.bin",	0x8000, 0xaf9805bf, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "cv06.bin",	0x8000, 0x8f76979d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cv00.bin",	0x8000, 0xaa3d3b99, 2 | BRF_GRA },           //  2 Sprites
	{ "cv01.bin",	0x8000, 0xf68c2ff6, 2 | BRF_GRA },           //  3
	{ "cv02.bin",	0x8000, 0x979cdb5a, 2 | BRF_GRA },           //  4

	{ "cv03.bin",	0x8000, 0xcc7ed6d8, 3 | BRF_GRA },           //  5 Background Tiles
	{ "cv04.bin",	0x4000, 0xdf693a04, 3 | BRF_GRA },           //  6

	{ "cv07.bin",	0x8000, 0xed5441ba, 4 | BRF_GRA },           //  7 Characters

	{ "cv08-1.bpr",	0x0100, 0xb7c43db9, 5 | BRF_GRA },           //  8 Color data
};

STD_ROM_PICK(compgolf)
STD_ROM_FN(compgolf)

struct BurnDriver BurnDrvCompgolf = {
	"compgolf", NULL, NULL, NULL, "1986",
	"Competition Golf Final Round (revision 3)\0", NULL, "Data East", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, compgolfRomInfo, compgolfRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	248, 240, 4, 3
};


// Competition Golf Final Round (Japan, old version)

static struct BurnRomInfo compgolfoRomDesc[] = {
	{ "cv05.bin",	0x8000, 0x3cef62c9, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "cv06.bin",	0x8000, 0x8f76979d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cv00.bin",	0x8000, 0xaa3d3b99, 2 | BRF_GRA },           //  2 Sprites
	{ "cv01.bin",	0x8000, 0xf68c2ff6, 2 | BRF_GRA },           //  3
	{ "cv02.bin",	0x8000, 0x979cdb5a, 2 | BRF_GRA },           //  4

	{ "cv03.bin",	0x8000, 0xcc7ed6d8, 3 | BRF_GRA },           //  5 Background Tiles
	{ "cv04.bin",	0x4000, 0xdf693a04, 3 | BRF_GRA },           //  6

	{ "cv07.bin",	0x8000, 0xed5441ba, 4 | BRF_GRA },           //  7 Characters

	{ "cv08-1.bpr",	0x0100, 0xb7c43db9, 5 | BRF_GRA },           //  8 Color data
};

STD_ROM_PICK(compgolfo)
STD_ROM_FN(compgolfo)

struct BurnDriver BurnDrvCompgolfo = {
	"compgolfo", "compgolf", NULL, NULL, "1985",
	"Competition Golf Final Round (Japan, old version)\0", NULL, "Data East", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, compgolfoRomInfo, compgolfoRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0x100,
	248, 240, 4, 3
};
