// FB Alpha Atari Klax driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "watchdog.h"
#include "msm6295.h"
#include "atariic.h"
#include "atarimo.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvMobRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 video_int_state;
static INT32 scanline_int_state;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[1];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo KlaxInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 15,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 14,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 12,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 15,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 14,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 12,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 8,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Klax)

static struct BurnDIPInfo KlaxDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x08, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0d, 0x01, 0x08, 0x08, "Off"				},
	{0x0d, 0x01, 0x08, 0x00, "On"				},
};

STDDIPINFO(Klax)

static void update_interrupts()
{
	INT32 newstate = (video_int_state || scanline_int_state) ? 4 : 0;

	SekSetIRQLine(newstate ? newstate : 7, newstate ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void __fastcall klax_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff800) == 0x3f2000) {
		*((UINT16*)(DrvMobRAM + (address & 0x7fe))) = BURN_ENDIAN_SWAP_INT16(data);
		AtariMoWrite(0, (address / 2) & 0x3ff, data);
		return;
	}

	if ((address & 0xff0000) == 0x1f0000) {
		AtariEEPROMUnlockWrite();
		return;
	}

	if ((address & 0xfff800) == 0x3e0000) {
		DrvPalRAM[(address / 2) & 0x3ff] = data >> 8;
		return;
	}

	switch (address)
	{
		case 0x260000:
			// klax_latch (ignore)
		return;

		case 0x270000:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x2e0000:
			BurnWatchdogWrite();
		return;

		case 0x360000:
			scanline_int_state = 0;
			video_int_state = 0;
			update_interrupts();
		return;
	}
}

static void __fastcall klax_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff800) == 0x3f2000) {
		DrvMobRAM[(address & 0x7ff) ^ 1] = data;
		AtariMoWrite(0, (address / 2) & 0x3ff, BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvMobRAM + (address & 0x7fe)))));
		return;
	}

	if ((address & 0xff0000) == 0x1f0000) {
		AtariEEPROMUnlockWrite();
		return;
	}

	if ((address & 0xfff800) == 0x3e0000) {
		DrvPalRAM[(address / 2) & 0x3ff] = data;
		return;
	}

	switch (address)
	{
		case 0x260001:
		case 0x260000:
			// klax_latch (ignore)
		return;

		case 0x270001:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x2e0000:
		case 0x2e0001:
			BurnWatchdogWrite();
		return;

		case 0x360001:
			scanline_int_state = 0;
			video_int_state = 0;
			update_interrupts();
		return;
	}
}

static UINT16 __fastcall klax_main_read_word(UINT32 address)
{
	if ((address & 0xfff800) == 0x3e0000) {
		UINT8 ret = DrvPalRAM[(address / 2) & 0x3ff];
		return ret + (ret * 256);
	}

	switch (address)
	{
		case 0x260000:
			return (DrvInputs[0] & 0xf7ff) | (vblank ? 0x0800 : 0);

		case 0x260002:
			return (DrvInputs[1] & 0xf7ff) | ((DrvDips[0] & 0x08) << 8);

		case 0x270000:
			return MSM6295Read(0);
	}

	return 0;
}

static UINT8 __fastcall klax_main_read_byte(UINT32 address)
{
	if ((address & 0xfff800) == 0x3e0000) {
		return DrvPalRAM[(address / 2) & 0x3ff];
	}

	switch (address)
	{
		case 0x260000:
		case 0x260001: {
			UINT16 ret = (DrvInputs[0] & 0xf7ff) | (vblank ? 0x0800 : 0);
			return ret >> ((~address & 1) * 8);
		}

		case 0x260002:
		case 0x260003: {
			UINT16 ret = (DrvInputs[1] & 0xf7ff) | ((DrvDips[0] & 0x08) << 8);
			return ret >> ((~address & 1) * 8);
		}

		case 0x270001:
			return MSM6295Read(0);
	}

	return 0;
}

static tilemap_callback( bg )
{
	UINT16 code  = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM0 + offs * 2)));
	UINT16 color = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM1 + offs * 2))) >> 8;

	TILE_SET_INFO(0, code, color, TILE_FLIPYX(code >> 15));
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset();

	AtariEEPROMReset();

	BurnWatchdogReset();

	video_int_state = 0;
	scanline_int_state = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x040000;

	DrvGfxROM0			= Next; Next += 0x080000;
	DrvGfxROM1			= Next; Next += 0x040000;

	MSM6295ROM			= Next;
	DrvSndROM			= Next; Next += 0x040000;

	DrvPalette			= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam				= Next;

	DrvVidRAM0			= Next; Next += 0x001000;
	DrvVidRAM1			= Next; Next += 0x001000;

	atarimo_0_spriteram = (UINT16*)Next;
	DrvMobRAM			= Next; Next += 0x000800;
	Drv68KRAM			= Next; Next += 0x001800;
	DrvPalRAM			= Next; Next += 0x000400;

	atarimo_0_slipram	= (UINT16*)(DrvVidRAM0 + 0xf80);

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4] = { STEP4(0,1) };
	INT32 XOffs[8] = { STEP8(0,4) };
	INT32 YOffs[8] = { STEP8(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x40000);

	GfxDecode(0x2000, 4, 8, 8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000);

	GfxDecode(0x1000, 4, 8, 8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	static const struct atarimo_desc modesc =
	{
		1,					// index to which gfx system
		1,					// number of motion object banks
		1,					// are the entries linked?
		0,					// are the entries split?
		0,					// render in reverse order?
		0,					// render in swapped X/Y order?
		0,					// does the neighbor bit affect the next object?
		8,					// pixels per SLIP entry (0 for no-slip)
		0,					// pixel offset for SLIPs
		0,					// maximum number of links to visit/scanline (0=all)

		0x000,				// base palette entry
		0x100,				// maximum number of colors
		0,					// transparent pen index

		{{ 0x00ff,0,0,0 }},	// mask for the link
		{{ 0 }},			// mask for the graphics bank
		{{ 0,0x0fff,0,0 }},	// mask for the code index
		{{ 0 }},			// mask for the upper code index
		{{ 0,0,0x000f,0 }},	// mask for the color
		{{ 0,0,0xff80,0 }},	// mask for the X position
		{{ 0,0,0,0xff80 }},	// mask for the Y position
		{{ 0,0,0,0x0070 }},	// mask for the width, in tiles*/
		{{ 0,0,0,0x0007 }},	// mask for the height, in tiles
		{{ 0,0,0,0x0008 }},	// mask for the horizontal flip
		{{ 0 }},			// mask for the vertical flip
		{{ 0 }},			// mask for the priority
		{{ 0 }},			// mask for the neighbor
		{{ 0 }},			// mask for absolute coordinates

		{{ 0 }},			// mask for the special value
		0,					// resulting value to indicate "special"
		0					// callback routine for special entries
	};

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x000001,  k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020001,  k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020000,  k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020001,  k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001,  k++, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x010000,  k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvVidRAM0,		0x3f0000, 0x3f0fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,		0x3f1000, 0x3f1fff, MAP_RAM);
	SekMapMemory(DrvMobRAM,			0x3f2000, 0x3f27ff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x3f2800, 0x3f3fff, MAP_RAM);
	SekSetWriteWordHandler(0,		klax_main_write_word);
	SekSetWriteByteHandler(0,		klax_main_write_byte);
	SekSetReadWordHandler(0,		klax_main_read_word);
	SekSetReadByteHandler(0,		klax_main_read_byte);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(1,		0x0e0000, 0x0e0fff);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	MSM6295Init(0, 875000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x80000, 0x100, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 8, 0x40000, 0x000, 0xff);

	AtariMoInit(0, &modesc);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	AtariMoExit();
	AtariEEPROMExit();

	SekExit();
	MSM6295Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvRecalcPalette()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x400/2; i++)
	{
		UINT16 p0 = BURN_ENDIAN_SWAP_INT16((p[i] << 8) | (p[i] >> 8));
		INT32 intensity = (p0 >> 15) & 1;

		UINT8 r = ((p0 >> 9) & 0x3e) | intensity;
		UINT8 g = ((p0 >> 4) & 0x3e) | intensity;
		UINT8 b = ((p0 << 1) & 0x3e) | intensity;

		r = (r << 2) | (r >> 4);
		g = (g << 2) | (g >> 4);
		b = (b << 2) | (b >> 4);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void copy_sprites()
{
	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	for (INT32 y = miny; y < maxy; y++)
	{
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);

		for (INT32 x = minx; x < maxx; x++)
		{
			if (mo[x] != 0xffff)
			{
				if ((pf[x] & 0xf0) != 0xf0)
				{
					pf[x] = mo[x];
				}
				mo[x] = 0xffff; // clear bitmap!
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
		DrvRecalc = 1; // force update
	}

	GenericTilemapDraw(0, pTransDraw, 0);

	AtariMoRender(0);

	copy_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	BurnWatchdogUpdate();

	{
		memset (DrvInputs, 0xff, 2 * sizeof(UINT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { (INT32)(7159090 / 59.92) };
	INT32 nCyclesDone[1] = { 0 };

	SekOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if ((i & 0x1f) == 0x1f && (i & 0x20) == 0 && vblank == 0) {
			scanline_int_state = 1;
			update_interrupts();
		}

		if (i == 239) {
			vblank = 1;
			video_int_state = vblank;
			update_interrupts();
		}
	}

	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);

		AtariMoScan(nAction, pnMin);

		MSM6295Scan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		SCAN_VAR(video_int_state);
		SCAN_VAR(scanline_int_state);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Klax (version 6)
// internal ROM test date: 13FEB1990 11:10:09

static struct BurnRomInfo klaxRomDesc[] = {
	{ "136075-6006.3n",			0x10000, 0xe8991709, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136075-6005.1n",			0x10000, 0x72b8c510, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136075-6008.3k",			0x10000, 0xc7c91a9d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136075-6007.1k",			0x10000, 0xd2021a88, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136075-2010.17x",		0x10000, 0x15290a0d, 2 | BRF_GRA },           //  4 Sprites and Backgrounds
	{ "136075-2012.12x",		0x10000, 0xc0d9eb0f, 2 | BRF_GRA },           //  5
	{ "136075-2009.17u",		0x10000, 0x6368dbaf, 2 | BRF_GRA },           //  6
	{ "136075-2011.12u",		0x10000, 0xe83cca91, 2 | BRF_GRA },           //  7

	{ "136075-2014.17y",		0x10000, 0x5c551e92, 3 | BRF_GRA },           //  8 Sprites and Backgrounds
	{ "136075-2013.17w",		0x10000, 0x36764bbc, 3 | BRF_GRA },           //  9

	{ "136075-1015.14b",		0x10000, 0x4d24c768, 4 | BRF_SND },           // 10 Samples
	{ "136075-1016.12b",		0x10000, 0x12e9b4b7, 4 | BRF_SND },           // 11

	{ "136075-1000.11c.bin",	0x00117, 0xfb86e94a, 5 | BRF_GRA },           // 12 PALs
	{ "136075-1001.18l.bin",	0x00117, 0xcd21acfe, 5 | BRF_GRA },           // 13
	{ "136075-1002.8w.bin",		0x00117, 0x4a7b6c44, 5 | BRF_GRA },           // 14
	{ "136075-1003.9w.bin",		0x00117, 0x72f7f904, 5 | BRF_GRA },           // 15
	{ "136075-1004.6w.bin",		0x00117, 0x6cd3270d, 5 | BRF_GRA },           // 16
};

STD_ROM_PICK(klax)
STD_ROM_FN(klax)

struct BurnDriver BurnDrvKlax = {
	"klax", NULL, NULL, NULL, "1990",
	"Klax (version 6)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, klaxRomInfo, klaxRomName, NULL, NULL, NULL, NULL, KlaxInputInfo, KlaxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Klax (Germany, version 2)
// internal ROM test date: 13FEB1990 11:10:09

static struct BurnRomInfo klaxd2RomDesc[] = {
	{ "136075-2206.3n",			0x10000, 0x9d1a713b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136075-1205.1n",			0x10000, 0x45065a5a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136075-1208.3k",			0x10000, 0xb4019b32, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136075-1207.1k",			0x10000, 0x14550a75, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136075-2010.17x",		0x10000, 0x15290a0d, 2 | BRF_GRA },           //  4 Sprites and Backgrounds
	{ "136075-2012.12x",		0x10000, 0xc0d9eb0f, 2 | BRF_GRA },           //  5
	{ "136075-2009.17u",		0x10000, 0x6368dbaf, 2 | BRF_GRA },           //  6
	{ "136075-2011.12u",		0x10000, 0xe83cca91, 2 | BRF_GRA },           //  7

	{ "136075-2014.17y",		0x10000, 0x5c551e92, 3 | BRF_GRA },           //  8 Sprites and Backgrounds
	{ "136075-2013.17w",		0x10000, 0x36764bbc, 3 | BRF_GRA },           //  9

	{ "136075-1015.14b",		0x10000, 0x4d24c768, 4 | BRF_SND },           // 10 Samples
	{ "136075-1016.12b",		0x10000, 0x12e9b4b7, 4 | BRF_SND },           // 11

	{ "136075-1000.11c.bin",	0x00117, 0xfb86e94a, 5 | BRF_GRA },           // 12 PALs
	{ "136075-1001.18l.bin",	0x00117, 0xcd21acfe, 5 | BRF_GRA },           // 13
	{ "136075-1002.8w.bin",		0x00117, 0x4a7b6c44, 5 | BRF_GRA },           // 14
	{ "136075-1003.9w.bin",		0x00117, 0x72f7f904, 5 | BRF_GRA },           // 15
	{ "136075-1004.6w.bin",		0x00117, 0x6cd3270d, 5 | BRF_GRA },           // 16
};

STD_ROM_PICK(klaxd2)
STD_ROM_FN(klaxd2)

struct BurnDriver BurnDrvKlaxd2 = {
	"klaxd2", "klax", NULL, NULL, "1990",
	"Klax (Germany, version 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, klaxd2RomInfo, klaxd2RomName, NULL, NULL, NULL, NULL, KlaxInputInfo, KlaxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Klax (Japan, version 3)
// internal ROM test date: 20JAN1990 14:47:06

static struct BurnRomInfo klaxj3RomDesc[] = {
	{ "136075-3406.3n",			0x10000, 0xab2aa50b, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136075-3405.1n",			0x10000, 0x9dc9a590, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136075-2408.3k",			0x10000, 0x89d515ce, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136075-2407.1k",			0x10000, 0x48ce4edb, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136075-2010.17x",		0x10000, 0x15290a0d, 2 | BRF_GRA },           //  4 Sprites and Backgrounds
	{ "136075-2012.12x",		0x10000, 0xc0d9eb0f, 2 | BRF_GRA },           //  5
	{ "136075-2009.17u",		0x10000, 0x6368dbaf, 2 | BRF_GRA },           //  6
	{ "136075-2011.12u",		0x10000, 0xe83cca91, 2 | BRF_GRA },           //  7

	{ "136075-2014.17y",		0x10000, 0x5c551e92, 3 | BRF_GRA },           //  8 Sprites and Backgrounds
	{ "136075-2013.17w",		0x10000, 0x36764bbc, 3 | BRF_GRA },           //  9

	{ "136075-1015.14b",		0x10000, 0x4d24c768, 4 | BRF_SND },           // 10 Samples
	{ "136075-1016.12b",		0x10000, 0x12e9b4b7, 4 | BRF_SND },           // 11

	{ "136075-1000.11c.bin",	0x00117, 0xfb86e94a, 5 | BRF_GRA },           // 12 PALs
	{ "136075-1001.18l.bin",	0x00117, 0xcd21acfe, 5 | BRF_GRA },           // 13
	{ "136075-1002.8w.bin",		0x00117, 0x4a7b6c44, 5 | BRF_GRA },           // 14
	{ "136075-1003.9w.bin",		0x00117, 0x72f7f904, 5 | BRF_GRA },           // 15
	{ "136075-1004.6w.bin",		0x00117, 0x6cd3270d, 5 | BRF_GRA },           // 16
};

STD_ROM_PICK(klaxj3)
STD_ROM_FN(klaxj3)

struct BurnDriver BurnDrvKlaxj3 = {
	"klaxj3", "klax", NULL, NULL, "1990",
	"Klax (Japan, version 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, klaxj3RomInfo, klaxj3RomName, NULL, NULL, NULL, NULL, KlaxInputInfo, KlaxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Klax (Japan, version 4)
// internal ROM test date: 13FEB1990 11:10:09

static struct BurnRomInfo klaxj4RomDesc[] = {
	{ "136075-4406.3n",			0x10000, 0xfc4045ec, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136075-4405.1n",			0x10000, 0xf017461a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136075-4408.3k",			0x10000, 0x23231159, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136075-4407.1k",			0x10000, 0x8d8158b2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136075-2010.17x",		0x10000, 0x15290a0d, 2 | BRF_GRA },           //  4 Sprites and Backgrounds
	{ "136075-2012.12x",		0x10000, 0xc0d9eb0f, 2 | BRF_GRA },           //  5
	{ "136075-2009.17u",		0x10000, 0x6368dbaf, 2 | BRF_GRA },           //  6
	{ "136075-2011.12u",		0x10000, 0xe83cca91, 2 | BRF_GRA },           //  7

	{ "136075-2014.17y",		0x10000, 0x5c551e92, 3 | BRF_GRA },           //  8 Sprites and Backgrounds
	{ "136075-2013.17w",		0x10000, 0x36764bbc, 3 | BRF_GRA },           //  9

	{ "136075-1015.14b",		0x10000, 0x4d24c768, 4 | BRF_SND },           // 10 Samples
	{ "136075-1016.12b",		0x10000, 0x12e9b4b7, 4 | BRF_SND },           // 11

	{ "136075-1000.11c.bin",	0x00117, 0xfb86e94a, 5 | BRF_GRA },           // 12 PALs
	{ "136075-1001.18l.bin",	0x00117, 0xcd21acfe, 5 | BRF_GRA },           // 13
	{ "136075-1002.8w.bin",		0x00117, 0x4a7b6c44, 5 | BRF_GRA },           // 14
	{ "136075-1003.9w.bin",		0x00117, 0x72f7f904, 5 | BRF_GRA },           // 15
	{ "136075-1004.6w.bin",		0x00117, 0x6cd3270d, 5 | BRF_GRA },           // 16
};

STD_ROM_PICK(klaxj4)
STD_ROM_FN(klaxj4)

struct BurnDriver BurnDrvKlaxj4 = {
	"klaxj4", "klax", NULL, NULL, "1990",
	"Klax (Japan, version 4)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, klaxj4RomInfo, klaxj4RomName, NULL, NULL, NULL, NULL, KlaxInputInfo, KlaxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Klax (version 4)
// internal ROM test date: 20JAN1990 14:47:06

static struct BurnRomInfo klax4RomDesc[] = {
	{ "136075-5006.3n",			0x10000, 0x65eb9a31, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136075-5005.1n",			0x10000, 0x7be27349, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136075-4008.3k",			0x10000, 0xf3c79106, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136075-4007.1k",			0x10000, 0xa23cde5d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136075-2010.17x",		0x10000, 0x15290a0d, 2 | BRF_GRA },           //  4 Sprites and Backgrounds
	{ "136075-2012.12x",		0x10000, 0xc0d9eb0f, 2 | BRF_GRA },           //  5
	{ "136075-2009.17u",		0x10000, 0x6368dbaf, 2 | BRF_GRA },           //  6
	{ "136075-2011.12u",		0x10000, 0xe83cca91, 2 | BRF_GRA },           //  7

	{ "136075-2014.17y",		0x10000, 0x5c551e92, 3 | BRF_GRA },           //  8 Sprites and Backgrounds
	{ "136075-2013.17w",		0x10000, 0x36764bbc, 3 | BRF_GRA },           //  9

	{ "136075-1015.14b",		0x10000, 0x4d24c768, 4 | BRF_SND },           // 10 Samples
	{ "136075-1016.12b",		0x10000, 0x12e9b4b7, 4 | BRF_SND },           // 11

	{ "136075-1000.11c.bin",	0x00117, 0xfb86e94a, 5 | BRF_GRA },           // 12 PALs
	{ "136075-1001.18l.bin",	0x00117, 0xcd21acfe, 5 | BRF_GRA },           // 13
	{ "136075-1002.8w.bin",		0x00117, 0x4a7b6c44, 5 | BRF_GRA },           // 14
	{ "136075-1003.9w.bin",		0x00117, 0x72f7f904, 5 | BRF_GRA },           // 15
	{ "136075-1004.6w.bin",		0x00117, 0x6cd3270d, 5 | BRF_GRA },           // 16
};	

STD_ROM_PICK(klax4)
STD_ROM_FN(klax4)

struct BurnDriver BurnDrvKlax4 = {
	"klax4", "klax", NULL, NULL, "1990",
	"Klax (version 4)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, klax4RomInfo, klax4RomName, NULL, NULL, NULL, NULL, KlaxInputInfo, KlaxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Klax (version 5)
// internal ROM test date: 20JAN1990 14:47:06

static struct BurnRomInfo klax5RomDesc[] = {
	{ "13607-5006.3n",			0x10000, 0x05c98fc0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "13607-5005.1n",			0x10000, 0xd461e1ee, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "13607-5008.3k",			0x10000, 0xf1b8e588, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "13607-5007.1k",			0x10000, 0xadbe33a8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136075-2010.17x",		0x10000, 0x15290a0d, 2 | BRF_GRA },           //  4 Sprites and Backgrounds
	{ "136075-2012.12x",		0x10000, 0xc0d9eb0f, 2 | BRF_GRA },           //  5
	{ "136075-2009.17u",		0x10000, 0x6368dbaf, 2 | BRF_GRA },           //  6
	{ "136075-2011.12u",		0x10000, 0xe83cca91, 2 | BRF_GRA },           //  7

	{ "136075-2014.17y",		0x10000, 0x5c551e92, 3 | BRF_GRA },           //  8 Sprites and Backgrounds
	{ "136075-2013.17w",		0x10000, 0x36764bbc, 3 | BRF_GRA },           //  9

	{ "136075-1015.14b",		0x10000, 0x4d24c768, 4 | BRF_SND },           // 10 Samples
	{ "136075-1016.12b",		0x10000, 0x12e9b4b7, 4 | BRF_SND },           // 11

	{ "136075-1000.11c.bin",	0x00117, 0xfb86e94a, 5 | BRF_GRA },           // 12 PALs
	{ "136075-1001.18l.bin",	0x00117, 0xcd21acfe, 5 | BRF_GRA },           // 13
	{ "136075-1002.8w.bin",		0x00117, 0x4a7b6c44, 5 | BRF_GRA },           // 14
	{ "136075-1003.9w.bin",		0x00117, 0x72f7f904, 5 | BRF_GRA },           // 15
	{ "136075-1004.6w.bin",		0x00117, 0x6cd3270d, 5 | BRF_GRA },           // 16
};

STD_ROM_PICK(klax5)
STD_ROM_FN(klax5)

struct BurnDriver BurnDrvKlax5 = {
	"klax5", "klax", NULL, NULL, "1990",
	"Klax (version 5)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, klax5RomInfo, klax5RomName, NULL, NULL, NULL, NULL, KlaxInputInfo, KlaxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Klax (version 5, bootleg set 1)
// derived from 'klax5' set, internal ROM test date: 20JAN1990 14:47:06

static struct BurnRomInfo klax5blRomDesc[] = {
	{ "6.bin",					0x10000, 0x3cfd2748, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2.bin",					0x10000, 0x910e5bf9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5.bin",					0x10000, 0x4fcacf88, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1.bin",					0x10000, 0xed0e3585, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "9.bin",					0x10000, 0xebe4bd96, 2 | BRF_GRA },           //  4 Sprites and Backgrounds
	{ "10.bin",					0x10000, 0xe7ad1cbd, 2 | BRF_GRA },           //  5
	{ "11.bin",					0x10000, 0xef7712fd, 2 | BRF_GRA },           //  6
	{ "12.bin",					0x10000, 0x1e0c1262, 2 | BRF_GRA },           //  7

	{ "7.bin",					0x10000, 0x5c551e92, 3 | BRF_GRA },           //  8 Sprites and Backgrounds
	{ "8.bin",					0x10000, 0x36764bbc, 3 | BRF_GRA },           //  9

	{ "3.bin",					0x10000, 0xb0441f1c, 7 | BRF_PRG | BRF_ESS }, // 10 Audio CPU Code?
	{ "4.bin",					0x10000, 0xa245e005, 7 | BRF_PRG | BRF_ESS }, // 11
};

STD_ROM_PICK(klax5bl)
STD_ROM_FN(klax5bl)

struct BurnDriverD BurnDrvKlax5bl = {
	"klax5bl", "klax", NULL, NULL, "1990",
	"Klax (version 5, bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, klax5blRomInfo, klax5blRomName, NULL, NULL, NULL, NULL, KlaxInputInfo, KlaxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Klax (version 5, bootleg set 2)
// derived from 'klax5' set, closer than klax5bl, internal ROM test date: 20JAN1990 14:47:06

static struct BurnRomInfo klax5bl2RomDesc[] = {
	{ "3.ic31",					0x10000, 0xe43699f3, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "1.ic13",					0x10000, 0xdc67f13a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.ic30",					0x10000, 0xf1b8e588, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2.ic12",					0x10000, 0xadbe33a8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "8.ic116",				0x10000, 0xebe4bd96, 2 | BRF_GRA },           //  4 Sprites and Backgrounds
	{ "7.ic117",				0x10000, 0x3b79c0d3, 2 | BRF_GRA },           //  5
	{ "12.ic134",				0x10000, 0xef7712fd, 2 | BRF_GRA },           //  6
	{ "11.ic135",				0x10000, 0xc2d8ce0c, 2 | BRF_GRA },           //  7

	{ "10.ic101",				0x10000, 0x5c551e92, 3 | BRF_GRA },           //  8 Sprites and Backgrounds
	{ "9.ic102",				0x10000, 0x29708e34, 3 | BRF_GRA },           //  9

	{ "6.ic22",					0x10000, 0xedd4c42c, 7 | BRF_PRG | BRF_ESS }, // 10 Audio CPU Code?
	{ "5.ic23",					0x10000, 0xa245e005, 7 | BRF_PRG | BRF_ESS }, // 11
};

STD_ROM_PICK(klax5bl2)
STD_ROM_FN(klax5bl2)

struct BurnDriverD BurnDrvKlax5bl2 = {
	"klax5bl2", "klax", NULL, NULL, "1990",
	"Klax (version 5, bootleg set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, klax5bl2RomInfo, klax5bl2RomName, NULL, NULL, NULL, NULL, KlaxInputInfo, KlaxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};