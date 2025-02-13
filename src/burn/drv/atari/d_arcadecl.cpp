// FB Alpha Atari Arcade Classics / Sparkz driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "atariic.h"
#include "atarimo.h"
#include "burn_gun.h"
#include "watchdog.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvBitmapRAM;
static UINT8 *DrvMobRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 scanline_int_state;
static INT32 oki_bank;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvDips[1];
static UINT16 DrvInputs[4];
static UINT8 DrvReset;

static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;
static INT16 DrvAnalogPort2 = 0;
static INT16 DrvAnalogPort3 = 0;

static INT32 is_joyver = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo ArcadeclInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 start"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 start"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 3"	},
	
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};
#undef A
STDINPUTINFO(Arcadecl)

static struct BurnInputInfo SparkzInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 15,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 14,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 13,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 15,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 14,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 13,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Sparkz)

static struct BurnDIPInfo ArcadeclDIPList[]=
{
	{0x11, 0xff, 0xff, 0x40, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x11, 0x01, 0x40, 0x40, "Off"			},
	{0x11, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Arcadecl)

static struct BurnDIPInfo SparkzDIPList[]=
{
	{0x15, 0xff, 0xff, 0x40, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x15, 0x01, 0x40, 0x40, "Off"			},
	{0x15, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Sparkz)

static void update_interrupts()
{
	SekSetIRQLine(4, scanline_int_state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void oki_bankswitch(INT32 data)
{
	oki_bank = data;
	
	MSM6295SetRoute(0, (data & 0x001f) / 31.0f, BURN_SND_ROUTE_BOTH);

	MSM6295SetBank(0, DrvSndROM + (data / 0x80) * 0x40000, 0, 0x3ffff);
}

static void __fastcall arcadecl_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff800) == 0x3e0000) {
		*((UINT16*)(DrvMobRAM + (address & 0x7fe))) = data;
		AtariMoWrite(0, (address / 2) & 0x3ff, data);
		return;
	}

	if ((address & 0xfff800) == 0x3c0000) {
		DrvPalRAM[(address / 2) & 0x3ff] = data >> 8;
		return;
	}

	if ((address & 0xfffff0) == 0x640040) {
		oki_bankswitch(data);
		return;
	}

	if ((address & 0xfffff0) == 0x640060) {
		AtariEEPROMUnlockWrite();
		return;
	}

	if ((address & 0xfff000) == 0x646000) {
		scanline_int_state = 0;
		update_interrupts();
		return;
	}

	if ((address & 0xfff000) == 0x647000) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0xfffffe) == 0x642000) {
		MSM6295Write(0, data >> 8);
		return;
	}
}

static void __fastcall arcadecl_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff800) == 0x3e0000) {
		DrvMobRAM[(address & 0x7ff) ^ 1] = data;
		AtariMoWrite(0, (address / 2) & 0x3ff, *((UINT16*)(DrvMobRAM + (address & 0x7fe))));
		return;
	}

	if ((address & 0xfff800) == 0x3c0000) {
		DrvPalRAM[(address / 2) & 0x3ff] = data;
		return;
	}

	if ((address & 0xfffff0) == 0x640040) {
		oki_bankswitch(data);
		return;
	}

	if ((address & 0xfffff0) == 0x640060) {
		AtariEEPROMUnlockWrite();
		return;
	}

	if ((address & 0xfff000) == 0x646000) {
		scanline_int_state = 0;
		update_interrupts();
		return;
	}

	if ((address & 0xfff000) == 0x647000) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0xfffffe) == 0x642000) {
		MSM6295Write(0, data);
		return;
	}
}

static UINT16 __fastcall arcadecl_read_word(UINT32 address)
{
	if ((address & 0xfff800) == 0x3c0000) {
		return DrvPalRAM[(address / 2) & 0x3ff];
	}

	switch (address)
	{
		case 0x640000:
			return DrvInputs[0];

		case 0x640002:
		case 0x640003:
			return DrvInputs[1];

		case 0x640010:
		{
			UINT16 ret = DrvInputs[2] & ~0x00c0;
			ret |= (vblank ? 0x0080 : 0x0000);
			ret |= DrvDips[0] & 0x40;
			return ret;
		}

		case 0x640012:
			return DrvInputs[3];

		case 0x640020:
			return 0xff00 | BurnTrackballRead(1, 0); // trackx p2

		case 0x640022:
			return 0xff00 | BurnTrackballRead(1, 1); // tracky p2

		case 0x640024:
			return 0xff00 | BurnTrackballRead(0, 0); // trackx p1

		case 0x640026:
			return 0xff00 | BurnTrackballRead(0, 1); // tracky p1

		case 0x642000:
			return (MSM6295Read(0) << 8) | 0xff;
	}

	return 0;
}

static UINT8 __fastcall arcadecl_read_byte(UINT32 address)
{
	if ((address & 0xfff800) == 0x3c0000) {
		return DrvPalRAM[(address / 2) & 0x3ff];
	}

	return arcadecl_read_word(address&~1) >> ((~address & 1) * 8);
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	oki_bankswitch(0);

	AtariEEPROMReset();

	MSM6295Reset();

	BurnWatchdogReset();

	scanline_int_state = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x100000;

	DrvGfxROM			= Next; Next += 0x100000;

	MSM6295ROM			= Next;
	DrvSndROM			= Next; Next += 0x080000;

	DrvPalette			= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam				= Next;

	DrvBitmapRAM		= Next; Next += 0x020000;
	DrvMobRAM			= Next; Next += 0x010000;
	DrvPalRAM			= Next; Next += 0x000400;

	atarimo_0_slipram 	= (UINT16*)(DrvMobRAM + 0xffc0);

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static void graphics_expand(UINT8 *src, INT32 len)
{
	for (INT32 i = len-1; i >= 0; i--)
	{
		INT32 p = DrvGfxROM[i] ^ 0xff; // invert

		src[i*2+0] = p >> 4;
		src[i*2+1] = p & 0xf;
	}
}

static INT32 DrvInit()
{
	static const struct atarimo_desc modesc =
	{
		0,					// index to which gfx system
		1,					// number of motion object banks
		1,					// are the entries linked?
		0,					// are the entries split?
		0,					// render in reverse order?
		0,					// render in swapped X/Y order?
		0,					// does the neighbor bit affect the next object?
		0,					// pixels per SLIP entry (0 for no-slip)
		0,					// pixel offset for SLIPs
		0,					// maximum number of links to visit/scanline (0=all)

		0x100,				// base palette entry
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
		0,					// callback routine for special entries
	};

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM + 0x0000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvSndROM + 0x0000000, k++, 1)) return 1;

		BurnLoadRom(DrvGfxROM + 0x0000000, k++, 1);	// arcadecl only!
		graphics_expand(DrvGfxROM, 0x80000);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,				0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvBitmapRAM,			0x200000, 0x21ffff, MAP_RAM);
	SekMapMemory(DrvMobRAM,				0x3e0000, 0x3e07ff, MAP_ROM);
	SekMapMemory(DrvMobRAM + 0x800,		0x3e0800, 0x3effff, MAP_RAM);
	SekSetWriteWordHandler(0,			arcadecl_write_word);
	SekSetWriteByteHandler(0,			arcadecl_write_byte);
	SekSetReadWordHandler(0,			arcadecl_read_word);
	SekSetReadByteHandler(0,			arcadecl_read_byte);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(1,			0x641000, 0x641fff);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	MSM6295Init(0, 1193182 / MSM6295_PIN7_LOW, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 8, 8, 0x100000, 0x000, 0xff);

	AtariMoInit(0, &modesc);
//	atarimo_set_xscroll(0, 4);
	atarimo_set_yscroll(0, 0x110);

	BurnTrackballInit(2);

	is_joyver = (!!strstr(BurnDrvGetTextA(DRV_NAME), "sparkz"));

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	MSM6295Exit();

	AtariMoExit();
	AtariEEPROMExit();

	BurnTrackballExit();

	BurnFree(AllMem);

	is_joyver = 0;

	return 0;
}

static void DrvRecalcPalette()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x400/2; i++)
	{
		UINT16 p0 = (p[i] << 8) | (p[i] >> 8);
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

static void copy_bitmap_and_sprite()
{
	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		UINT8 *src = DrvBitmapRAM + (y * 512) + 4;
		UINT16 *mo  = BurnBitmapGetPosition(31, 0, y);
		UINT16 *dst = BurnBitmapGetPosition(0, 0, y);

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			if (mo[x] != 0xffff)
			{
				dst[x] = mo[x] & 0x1ff;
				mo[x] = 0xffff;
			}
			else
			{
				dst[x] = src[x^1];
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

	AtariMoRender(0);

	if (nBurnLayer & 1) copy_bitmap_and_sprite();

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
		memset (DrvInputs, 0xff, 4 * sizeof(UINT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (!is_joyver) {
			BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
			BurnTrackballFrame(0, DrvAnalogPort0, DrvAnalogPort1, 0x06, 0x0a);
			BurnTrackballUpdate(0);

			BurnTrackballConfig(1, AXIS_REVERSED, AXIS_NORMAL);
			BurnTrackballFrame(1, DrvAnalogPort2, DrvAnalogPort3, 0x06, 0x0a);
			BurnTrackballUpdate(1);
		}

	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { 14318180 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	SekOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if ((i & 0x1f) == 0x00 && (i & 0x20) == 0) {
			scanline_int_state = 1;
			update_interrupts();
		}

		if ((i%42) == 41 && !is_joyver) {
			BurnTrackballUpdate(0);
			BurnTrackballUpdate(1);
		}

		if (i == 239) {
			vblank = 1;
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
		if (!is_joyver)	BurnTrackballScan();

		SCAN_VAR(scanline_int_state);
		SCAN_VAR(oki_bank);
	}

	if (nAction & ACB_WRITE) {
		oki_bankswitch(oki_bank);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Arcade Classics (prototype)

static struct BurnRomInfo arcadeclRomDesc[] = {
	{ "pgm0",		0x80000, 0xb5b93623, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "prog1",		0x80000, 0xe7efef85, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "adpcm",		0x80000, 0x03ca7f03, 2 | BRF_SND },           //  2 Samples

	{ "atcl_mob",	0x80000, 0x0e9b3930, 3 | BRF_GRA },           //  3 Sprites
};

STD_ROM_PICK(arcadecl)
STD_ROM_FN(arcadecl)

struct BurnDriver BurnDrvArcadecl = {
	"arcadecl", NULL, NULL, NULL, "1992",
	"Arcade Classics (prototype)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, arcadeclRomInfo, arcadeclRomName, NULL, NULL, NULL, NULL, ArcadeclInputInfo, ArcadeclDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Sparkz (prototype)

static struct BurnRomInfo sparkzRomDesc[] = {
	{ "sparkzpg.0",	0x80000, 0xa75c331c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sparkzpg.1",	0x80000, 0x1af1fc04, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sparkzsn",	0x80000, 0x87097ce2, 2 | BRF_SND },           //  2 Samples
};

STD_ROM_PICK(sparkz)
STD_ROM_FN(sparkz)

struct BurnDriver BurnDrvSparkz = {
	"sparkz", NULL, NULL, NULL, "1992",
	"Sparkz (prototype)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, sparkzRomInfo, sparkzRomName, NULL, NULL, NULL, NULL, SparkzInputInfo, SparkzDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};
