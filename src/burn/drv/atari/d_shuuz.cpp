// FB Alpha Atari Shuzz driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "atariic.h"
#include "atarimo.h"
#include "atarivad.h"
#include "msm6295.h"
#include "watchdog.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvSprRAM;
static UINT16 *DrvEOFData;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT16 DrvInputs[2];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static INT16 DrvAnalogPortX = 0;
static INT16 DrvAnalogPortY = 0;

static INT32 track_inf[2];

static UINT32 linecycles;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo ShuuzInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPortX,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPortY,"p1 y-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Shuuz)

static struct BurnInputInfo Shuuz2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 15,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 14,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 13,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 12,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPortX,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPortY,"p1 y-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};
#undef A

STDINPUTINFO(Shuuz2)

static struct BurnDIPInfo ShuuzDIPList[]=
{
	{0x06, 0xff, 0xff, 0x08, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x06, 0x01, 0x08, 0x08, "Off"				},
	{0x06, 0x01, 0x08, 0x00, "On"				},
};

STDDIPINFO(Shuuz)

static struct BurnDIPInfo Shuuz2DIPList[]=
{
	{0x0a, 0xff, 0xff, 0x08, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0a, 0x01, 0x08, 0x08, "Off"				},
	{0x0a, 0x01, 0x08, 0x00, "On"				},
};

STDDIPINFO(Shuuz2)

static void __fastcall shuuz_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffc00) == 0x3fd000) {
		*((UINT16*)(DrvSprRAM + (address & 0x3fe))) = data;
		AtariMoWrite(0, (address / 2) & 0x1ff, data);
		return;
	}

	if ((address & 0xfff000) == 0x101000) {
		AtariEEPROMUnlockWrite();
		return;
	}

	switch (address)
	{
		case 0x102000:
			BurnWatchdogWrite();
		return;

		case 0x106000:
			MSM6295Write(0, data);
		return;
	}
}

static void __fastcall shuuz_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffc00) == 0x3fd000) {
		DrvSprRAM[(address & 0x3ff) ^ 1] = data;
		AtariMoWrite(0, (address / 2) & 0x1ff, *((UINT16*)(DrvSprRAM + (address & 0x3fe))));
		return;
	}

	if ((address & 0xfff000) == 0x101000) {
		AtariEEPROMUnlockWrite();
		return;
	}

	switch (address)
	{
		case 0x102000:
		case 0x102001:
			BurnWatchdogWrite();
		return;

		case 0x106000:
		case 0x106001:
			MSM6295Write(0, data);
		return;
	}
}

static inline UINT16 special_read()
{
	UINT16 ret = DrvInputs[0];

	if (vblank) ret ^= 0x800;
	if (!vblank && (SekTotalCycles() - linecycles >= 336)) ret &= ~0x0800;

	return ret;
}

static UINT16 leta_r(INT32 port)
{
	if (port == 0)
	{
		INT32 dx = (INT8)BurnTrackballRead(0, 0);
		INT32 dy = (INT8)BurnTrackballRead(0, 1);

		track_inf[0] = dx + dy;
		track_inf[1] = dx - dy;
	}

	return track_inf[port];
}

static UINT16 __fastcall shuuz_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x106000:
			return MSM6295Read(0);

		case 0x103000:
		case 0x103002:
			return leta_r((address >> 1) & 1); // leta_r

		case 0x105000:
			return special_read();

		case 0x105002:
			return (DrvInputs[1] & ~0x0800) | ((DrvDips[0] & 8) << 8);
	}

	return 0;
}

static UINT8 __fastcall shuuz_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x106000:
		case 0x106001:
			return MSM6295Read(0);

		case 0x103000:
		case 0x103001:
		case 0x103002:
		case 0x103003:
			return leta_r((address >> 1) & 1); // leta_r

		case 0x105000:
		case 0x105001:
			return special_read() >> ((~address & 1) * 8);

		case 0x105002:
		case 0x105003:
		{
			UINT16 ret = (DrvInputs[1] & ~0x0800) | ((DrvDips[0] & 8) << 8);
			return ret >> ((~address & 1) * 8);
		}
	}

	return 0;
}

static void palette_write(INT32 offset, UINT16 data)
{
	UINT8 i = data >> 15;
	UINT8 r = ((data >>  9) & 0x3e) | i;
	UINT8 g = ((data >>  4) & 0x3e) | i;
	UINT8 b = ((data <<  1) & 0x3e) | i;

	r = (r << 2) | (r >> 4);
	g = (g << 2) | (g >> 4);
	b = (b << 2) | (b >> 4);

	DrvPalette[offset] = BurnHighCol(r,g,b,0);
}

static void scanline_timer(INT32 state)
{
	SekSetIRQLine(4, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	BurnWatchdogReset();

	AtariVADReset();
	AtariEEPROMReset();

	MSM6295Reset();

	track_inf[0] = track_inf[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x040000;

	DrvGfxROM0			= Next; Next += 0x100000;
	DrvGfxROM1			= Next; Next += 0x200000;

	MSM6295ROM			= Next;
	DrvSndROM			= Next; Next += 0x040000;

	DrvPalette			= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	AllRam				= Next;

	atarimo_0_spriteram = (UINT16*)Next;

	DrvSprRAM			= Next; Next += 0x001000;
	Drv68KRAM			= Next; Next += 0x008000;

	atarimo_0_slipram	= (UINT16*)Next; Next += 0x00080;
	DrvEOFData			= (UINT16*)Next; Next += 0x00080;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4] = { 0, 4, 0x80000*8+0, 0x80000*8+4 };
	INT32 XOffs[8] = { 0, 1, 2, 3,   8, 9, 10, 11  };
	INT32 YOffs[8] = { 0, 16, 32, 48, 64, 80, 96, 112 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x100000; i++) tmp[i] = DrvGfxROM0[i] ^ 0xff;

	GfxDecode(0x4000, 4, 8, 8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	for (INT32 i = 0; i < 0x100000; i++) tmp[i] = DrvGfxROM1[i] ^ 0xff;

	GfxDecode(0x8000, 4, 8, 8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM1);

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
		{{ 0,0x7fff,0,0 }},	// mask for the code index
		{{ 0 }},			// mask for the upper code index
		{{ 0,0,0x000f,0 }},	// mask for the color
		{{ 0,0,0xff80,0 }},	// mask for the X position
		{{ 0,0,0,0xff80 }},	// mask for the Y position
		{{ 0,0,0,0x0070 }},	// mask for the width, in tiles*/
		{{ 0,0,0,0x0007 }},	// mask for the height, in tiles
		{{ 0,0x8000,0,0 }},	// mask for the horizontal flip
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

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0a0000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x060000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0a0000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c0000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0e0000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x020000,  k++, 1)) return 1;

		DrvGfxDecode();
	}

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x100000, 0x100, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 8, 0x200000, 0x000, 0x0f);

	AtariVADInit(0, 1, 1, scanline_timer, palette_write);
	AtariMoInit(0, &modesc);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM + 0x0000,	0x3f8000, 0x3fcfff, MAP_RAM);
	SekMapMemory(DrvSprRAM,				0x3fd000, 0x3fd3ff, MAP_ROM); // handler
	SekMapMemory(Drv68KRAM + 0x5400,	0x3fd400, 0x3fffff, MAP_RAM);
	SekSetWriteWordHandler(0,			shuuz_write_word);
	SekSetWriteByteHandler(0,			shuuz_write_byte);
	SekSetReadWordHandler(0,			shuuz_read_word);
	SekSetReadByteHandler(0,			shuuz_read_byte);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(1,			0x100000, 0x100fff);

	AtariVADMap(0x3e0000, 0x3f7fff, 1);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	MSM6295Init(0, 894886 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	BurnTrackballInit(2);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	BurnFree (AllMem);

	GenericTilesExit();

	AtariVADExit();
	AtariMoExit();
	AtariEEPROMExit();

	MSM6295Exit();

	SekExit();

	BurnTrackballExit();

	return 0;
}

static void sprite_copy()
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
				INT32 o13 = ((pf[x] & 0xf0) == 0xf0);

				if ((!(pf[x] & 0x80) && ((mo[x] & 0xc0) != 0xc0) && ((mo[x] & 0x0e) != 0x00) && !o13) ||
					((pf[x] & 0x80) && ((mo[x] & 0xc0) == 0xc0) && ((mo[x] & 0x0e) != 0x00) && !o13))
					pf[x] = mo[x];

				mo[x] = 0xffff;
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		AtariVADRecalcPalette();
		DrvRecalc = 0;
	}

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	AtariVADDraw(pTransDraw, 0);

	AtariMoRender(0);

	if (nSpriteEnable & 1) sprite_copy();

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

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		{
			BurnTrackballConfig(0, AXIS_NORMAL, AXIS_REVERSED);
			BurnTrackballFrame(0, DrvAnalogPortX, DrvAnalogPortY, 0x09, 0x0a);
			BurnTrackballUpdate(0);
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { 7159090 / 60 }; // 59.92HZ
	INT32 nCyclesDone[1] = { 0 };

	vblank = 0;

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		atarivad_scanline = i;

		linecycles = SekTotalCycles(); // for hblank

		nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);

		AtariVADTimerUpdate();

		if ((i%64) == 63) {
			BurnTrackballUpdate(0);
		}

		if (i == 239) {
			vblank = 1;
		}

		if (i == 261) {
			for (INT32 e = 0; e < 0x80; e+=2) {
				DrvEOFData[e/2] = SekReadWord(0x3f5f00 + e);
				atarimo_0_slipram[e/2] = SekReadWord(0x3f5f80 + e);
			}
			AtariVADEOFUpdate(DrvEOFData);
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

		MSM6295Scan(nAction, pnMin);

		AtariVADScan(nAction, pnMin);
		AtariMoScan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		BurnTrackballScan();
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Shuuz (version 8.0)

static struct BurnRomInfo shuuzRomDesc[] = {
	{ "136083-4010.23p",	0x20000, 0x1c2459f8, 1 | BRF_PRG | BRF_ESS }, 	 //  0 68k Code
	{ "136083-4011.13p",	0x20000, 0x6db53a85, 1 | BRF_PRG | BRF_ESS }, 	 //  1

	{ "136083-2030.43x",	0x20000, 0x8ecf1ed8, 2 | BRF_GRA },           	 //  2 Background Tiles
	{ "136083-2032.20x",	0x20000, 0x5af184e6, 2 | BRF_GRA },           	 //  3
	{ "136083-2031.87x",	0x20000, 0x72e9db63, 2 | BRF_GRA },           	 //  4
	{ "136083-2033.65x",	0x20000, 0x8f552498, 2 | BRF_GRA },           	 //  5

	{ "136083-1020.43u",	0x20000, 0xd21ad039, 3 | BRF_GRA },          	 //  6 Sprites and Background Tiles
	{ "136083-1022.20u",	0x20000, 0x0c10bc90, 3 | BRF_GRA },          	 //  7
	{ "136083-1024.43m",	0x20000, 0xadb09347, 3 | BRF_GRA },          	 //  8
	{ "136083-1026.20m",	0x20000, 0x9b20e13d, 3 | BRF_GRA },          	 //  9
	{ "136083-1021.87u",	0x20000, 0x8388910c, 3 | BRF_GRA },           	 // 10
	{ "136083-1023.65u",	0x20000, 0x71353112, 3 | BRF_GRA },          	 // 11
	{ "136083-1025.87m",	0x20000, 0xf7b20a64, 3 | BRF_GRA },          	 // 12
	{ "136083-1027.65m",	0x20000, 0x55d54952, 3 | BRF_GRA },          	 // 13

	{ "136083-1040.75b",	0x20000, 0x0896702b, 4 | BRF_SND },          	 // 14 Samples
	{ "136083-1041.65b",	0x20000, 0xb3b07ce9, 4 | BRF_SND },          	 // 15

	{ "136083-1050.55c",	0x00001, 0x00000000, 5 | BRF_NODUMP | BRF_GRA }, // 16 PALs
	{ "136083-1051.45e",	0x00001, 0x00000000, 5 | BRF_NODUMP | BRF_GRA }, // 17
	{ "136083-1052.32l",	0x00001, 0x00000000, 5 | BRF_NODUMP | BRF_GRA }, // 18
	{ "136083-1053.85n",	0x00001, 0x00000000, 5 | BRF_NODUMP | BRF_GRA }, // 19
	{ "136083-1054.97n",	0x00001, 0x00000000, 5 | BRF_NODUMP | BRF_GRA }, // 20
};

STD_ROM_PICK(shuuz)
STD_ROM_FN(shuuz)

struct BurnDriver BurnDrvShuuz = {
	"shuuz", NULL, NULL, NULL, "1990",
	"Shuuz (version 8.0)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, shuuzRomInfo, shuuzRomName, NULL, NULL, NULL, NULL, ShuuzInputInfo, ShuuzDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};


// Shuuz (version 7.1)

static struct BurnRomInfo shuuz2RomDesc[] = {
	{ "136083-23p.rom",	0x20000, 0x98aec4e7, 1 | BRF_PRG | BRF_ESS },        //  0 68k Code
	{ "136083-13p.rom",	0x20000, 0xdd9d5d5c, 1 | BRF_PRG | BRF_ESS }, 	     //  1

	{ "136083-2030.43x",	0x20000, 0x8ecf1ed8, 2 | BRF_GRA },              //  2 Background Tiles
	{ "136083-2032.20x",	0x20000, 0x5af184e6, 2 | BRF_GRA },              //  3
	{ "136083-2031.87x",	0x20000, 0x72e9db63, 2 | BRF_GRA },              //  4
	{ "136083-2033.65x",	0x20000, 0x8f552498, 2 | BRF_GRA },              //  5

	{ "136083-1020.43u",	0x20000, 0xd21ad039, 3 | BRF_GRA },              //  6 Sprites and Background Tiles
	{ "136083-1022.20u",	0x20000, 0x0c10bc90, 3 | BRF_GRA },              //  7
	{ "136083-1024.43m",	0x20000, 0xadb09347, 3 | BRF_GRA },              //  8
	{ "136083-1026.20m",	0x20000, 0x9b20e13d, 3 | BRF_GRA },              //  9
	{ "136083-1021.87u",	0x20000, 0x8388910c, 3 | BRF_GRA },              // 10
	{ "136083-1023.65u",	0x20000, 0x71353112, 3 | BRF_GRA },              // 11
	{ "136083-1025.87m",	0x20000, 0xf7b20a64, 3 | BRF_GRA },              // 12
	{ "136083-1027.65m",	0x20000, 0x55d54952, 3 | BRF_GRA },              // 13

	{ "136083-1040.75b",	0x20000, 0x0896702b, 4 | BRF_SND },              // 14 Samples
	{ "136083-1041.65b",	0x20000, 0xb3b07ce9, 4 | BRF_SND },              // 15

	{ "136083-1050.55c",	0x00001, 0x00000000, 5 | BRF_NODUMP | BRF_GRA }, // 16 PALs
	{ "136083-1051.45e",	0x00001, 0x00000000, 5 | BRF_NODUMP | BRF_GRA }, // 17
	{ "136083-1052.32l",	0x00001, 0x00000000, 5 | BRF_NODUMP | BRF_GRA }, // 18
	{ "136083-1053.85n",	0x00001, 0x00000000, 5 | BRF_NODUMP | BRF_GRA }, // 19
	{ "136083-1054.97n",	0x00001, 0x00000000, 5 | BRF_NODUMP | BRF_GRA }, // 20
};

STD_ROM_PICK(shuuz2)
STD_ROM_FN(shuuz2)

struct BurnDriver BurnDrvShuuz2 = {
	"shuuz2", "shuuz", NULL, NULL, "1990",
	"Shuuz (version 7.1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, shuuz2RomInfo, shuuz2RomName, NULL, NULL, NULL, NULL, Shuuz2InputInfo, Shuuz2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};
