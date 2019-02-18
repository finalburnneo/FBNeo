// FB Alpha Atari Rampart driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "atarimo.h"
#include "atariic.h"
#include "burn_ym2413.h"
#include "msm6295.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvSndROM;
static UINT8 *DrvMobRAM;
static UINT8 *DrvBmpRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT16 DrvInputs[3];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static struct BurnInputInfo RampartInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 8,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 9,	"p1 fire 2"	},

	// fake analog palceholder
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy2 + 12,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy2 + 12,	"p1 fire 6"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 fire 2"	},

	// fake analog palceholder
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 6"	},

	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy1 + 10,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy2 + 10,	"p3 fire 4"	},

	// fake analog palceholder
	{"P3 Button 5",		BIT_DIGITAL,	DrvJoy2 + 12,	"p3 fire 5"	},
	{"P3 Button 6",		BIT_DIGITAL,	DrvJoy2 + 12,	"p3 fire 6"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Rampart)

static struct BurnInputInfo Ramprt2pInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 8,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 9,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 fire 2"	},

	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 10,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 11,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 8,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 9,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy1 + 10,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy2 + 10,	"p3 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Ramprt2p)

static struct BurnInputInfo RampartjInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 9,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 8,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 10,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Rampartj)

static struct BurnDIPInfo RampartDIPList[]=
{
	{0x12, 0xff, 0xff, 0x0c, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x08, 0x00, "On"				},
	{0x12, 0x01, 0x08, 0x08, "Off"				},
};

STDDIPINFO(Rampart)

static struct BurnDIPInfo Ramprt2pDIPList[]=
{
	{0x18, 0xff, 0xff, 0x0c, NULL				},

	{0   , 0xfe, 0   ,    2, "Players"			},
	{0x18, 0x01, 0x04, 0x00, "2"				},
	{0x18, 0x01, 0x04, 0x04, "3"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x18, 0x01, 0x08, 0x00, "On"				},
	{0x18, 0x01, 0x08, 0x08, "Off"				},
};

STDDIPINFO(Ramprt2p)

static struct BurnDIPInfo RampartjDIPList[]=
{
	{0x12, 0xff, 0xff, 0x0c, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x08, 0x00, "On"				},
	{0x12, 0x01, 0x08, 0x08, "Off"				},
};

STDDIPINFO(Rampartj)

static void __fastcall rampart_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x220000 && address < 0x3e0000) return; // 220000-3bffff & 3c0800-3dffff NOP

	if ((address & 0xfff800) == 0x3e0000) {
		*((UINT16*)(DrvMobRAM + (address & 0x7fe))) = data;
		AtariMoWrite(0, (address & 0x7fe)/2, data);
		return;
	}

	switch (address & ~0xffff)
	{
		case 0x460000:
			MSM6295Write(0, data >> 8);
		return;

		case 0x480000:
			BurnYM2413Write((address / 2) & 1, data >> 8);
		return;

		case 0x5a0000:
			AtariEEPROMUnlockWrite();
		return;

		case 0x640000:
		//	atarigen_set_ym2413_vol(((data >> 1) & 7) * 100 / 7);
		//	atarigen_set_oki6295_vol((data & 0x0020) ? 100 : 0);
		return;

		case 0x720000:
			BurnWatchdogWrite();
		return;

		case 0x7e0000:
			SekSetIRQLine(4, CPU_IRQSTATUS_NONE);
		return;
	}

	bprintf (0, _T("MWW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall rampart_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0x220000 && address < 0x3e0000) return; // 220000-3bffff & 3c0800-3dffff NOP

	if ((address & 0xfff800) == 0x3e0000) {
		DrvMobRAM[(address & 0x7ff)^1] = data;
		AtariMoWrite(0, (address & 0x7fe)/2, *((UINT16*)(DrvMobRAM + (address & 0xffe))));
		return;
	}

	switch (address & ~0xffff)
	{
		case 0x460000:
			MSM6295Write(0, data);
		return;

		case 0x480000:
			BurnYM2413Write((address / 2) & 1, data);
		return;

		case 0x5a0000:
			AtariEEPROMUnlockWrite();
		return;

		case 0x640000:
		//	atarigen_set_ym2413_vol(((data >> 1) & 7) * 100 / 7);
		//	atarigen_set_oki6295_vol((data & 0x0020) ? 100 : 0);
		return;

		case 0x720000:
			BurnWatchdogWrite();
		return;

		case 0x7e0000:
			SekSetIRQLine(4, CPU_IRQSTATUS_NONE);
		return;
	}

	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall rampart_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x460000:
			return (MSM6295Read(0) << 8) | 0xff;

		case 0x640000:
			return DrvInputs[0] | (vblank ? 0x800 : 0);

		case 0x640002:
			return DrvInputs[1];

		case 0x6c0000: // trackball for most sets!!
			return DrvInputs[2];

		case 0x6c0002:
		case 0x6c0004:
		case 0x6c0006:
			return 0; // trackball
	}

	bprintf (0, _T("MRW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall rampart_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x460000:
		case 0x460001:
			return MSM6295Read(0);

		case 0x640000:
			return (DrvInputs[0] >> 8) | (vblank ? 0x8 : 0);

		case 0x640001:
			return DrvInputs[0];

		case 0x640002:
			return DrvInputs[1] >> 8;

		case 0x640003:
			return DrvInputs[1];

		// trackball for most sets!
		case 0x6c0000:
			return DrvInputs[2] >> 8;

		case 0x6c0001:
			return DrvInputs[2];

		case 0x6c0002:
		case 0x6c0003:
		case 0x6c0004:
		case 0x6c0005:
		case 0x6c0006:
		case 0x6c0007:
			return 0; // trackball
	}

	bprintf (0, _T("MRB %5.5x\n"), address);

	return 0;
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
	AtariEEPROMReset();
	AtariSlapsticReset();

	BurnYM2413Reset();
	MSM6295Reset(0);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x200000;

	DrvGfxROM0			= Next; Next += 0x040000;

	MSM6295ROM			= Next;
	DrvSndROM			= Next; Next += 0x0400009;

	DrvPalette			= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam				= Next;

	atarimo_0_spriteram = (UINT16*)(Next + 0x00000);
	atarimo_0_slipram	= (UINT16*)(Next + 0x03f40);

	DrvMobRAM			= Next; Next += 0x010000;

	DrvBmpRAM			= Next; Next += 0x020000;

	DrvPalRAM			= Next; Next += 0x000800;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4] = { STEP4(0,1) };
	INT32 XOffs[8] = { STEP8(0,4)  };
	INT32 YOffs[8] = { STEP8(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x20000; i++) tmp[i] = DrvGfxROM0[i] ^ 0xff;

	GfxDecode(0x1000, 4, 8, 8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 japan, INT32 joystick)
{
	static const struct atarimo_desc modesc =
	{
		0,					/* index to which gfx system */
		1,					/* number of motion object banks */
		1,					/* are the entries linked? */
		0,					/* are the entries split? */
		0,					/* render in reverse order? */
		0,					/* render in swapped X/Y order? */
		0,					/* does the neighbor bit affect the next object? */
		8,					/* pixels per SLIP entry (0 for no-slip) */
		0,					/* pixel offset for SLIPs */
		0,					/* maximum number of links to visit/scanline (0=all) */

		0x100,				/* base palette entry */
		0x100,				/* maximum number of colors */
		0,					/* transparent pen index */

		{{ 0x00ff,0,0,0 }},	/* mask for the link */
		{{ 0 }},			/* mask for the graphics bank */
		{{ 0,0x7fff,0,0 }},	/* mask for the code index */
		{{ 0 }},			/* mask for the upper code index */
		{{ 0,0,0x000f,0 }},	/* mask for the color */
		{{ 0,0,0xff80,0 }},	/* mask for the X position */
		{{ 0,0,0,0xff80 }},	/* mask for the Y position */
		{{ 0,0,0,0x0070 }},	/* mask for the width, in tiles*/
		{{ 0,0,0,0x0007 }},	/* mask for the height, in tiles */
		{{ 0,0x8000,0,0 }},	/* mask for the horizontal flip */
		{{ 0 }},			/* mask for the vertical flip */
		{{ 0 }},			/* mask for the priority */
		{{ 0 }},			/* mask for the neighbor */
		{{ 0 }},			/* mask for absolute coordinates */

		{{ 0 }},			/* mask for the special value */
		0,					/* resulting value to indicate "special" */
		0,					/* callback routine for special entries */
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

		if (japan)
		{
			if (BurnLoadRom(Drv68KROM  + 0x040001,  k++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x040000,  k++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x080001,  k++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x080000,  k++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x0c0001,  k++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x0c0000,  k++, 2)) return 1;
		}
		else
		{
			if (BurnLoadRom(Drv68KROM  + 0x000001,  k++, 2)) return 1; // loaded over!
			if (BurnLoadRom(Drv68KROM  + 0x000000,  k++, 2)) return 1;
		}

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x020000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvBmpRAM  + 0x000000,  k++, 1)) return 1; // tmp memory

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x0fffff, MAP_ROM);
	// slapstic 140000, 0x147fff
	SekMapMemory(DrvBmpRAM,			0x200000, 0x21ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x3c0000, 0x3c07ff, MAP_RAM);
	SekMapMemory(DrvMobRAM,			0x3e0000, 0x3e07ff, MAP_ROM); // handler
	SekMapMemory(DrvMobRAM + 0x800,	0x3e0800, 0x3effff, MAP_RAM);
	SekSetWriteWordHandler(0,		rampart_write_word);
	SekSetWriteByteHandler(0,		rampart_write_byte);
	SekSetReadWordHandler(0,		rampart_read_word);
	SekSetReadByteHandler(0,		rampart_read_byte);

	AtariSlapsticInit(Drv68KROM + 0x40000, 118);
	AtariSlapsticInstallMap(1, 0x140000);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(2, 0x500000, 0x500fff);
	AtariEEPROMLoad(DrvBmpRAM); // tmp memory
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	BurnYM2413Init(3579545);
	BurnYM2413SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1193182 / MSM6295_PIN7_LOW, 1);
	MSM6295SetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x040000, 0x100, 0x0f);

	AtariMoInit(0, &modesc);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	BurnYM2413Exit();
	MSM6295Exit();

	AtariMoExit();
	AtariSlapsticExit();
	AtariEEPROMExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;
	for (INT32 offs = 0; offs < 0x800/2; offs+=2)
	{
		UINT16 d = (p[offs] & 0xff00) | (p[offs+1] >> 8);
		UINT8 i = d >> 15;
		UINT8 r = ((d >> 9) & 0x3e) | i;
		UINT8 g = ((d >> 4) & 0x3e) | i;
		UINT8 b = ((d << 1) & 0x3e) | i;

		r = (r << 2) | (r >> 4);
		g = (g << 2) | (g >> 4);
		b = (b << 2) | (b >> 4);

		DrvPalette[offs/2] = BurnHighCol(r,g,b,0);
	}
}

static void bitmap_copy() // copy bitmap & sprites in one pass
{
	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	for (INT32 y = miny; y < maxy; y++)
	{
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);
		UINT8 *bm = DrvBmpRAM + y * 512 + 4;

		for (INT32 x = minx; x < maxx; x++)
		{
			if (mo[x] != 0xffff)
			{
				pf[x] = mo[x] & 0x1ff;
				mo[x] = 0xffff;
			}
			else
			{
				pf[x] = bm[x^1];
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	AtariMoRender(0);

	bitmap_copy();

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
		DrvInputs[0] = (DrvInputs[0] & ~0x0804) | ((DrvDips[0] & 0x04) << 0);;
		DrvInputs[1] = (DrvInputs[1] & ~0x0800) | ((DrvDips[0] & 0x08) << 8);
		DrvInputs[2] = 0;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 262;
//	INT32 nCyclesTotal[1] = { (INT32)(7159090 / 59.92) }; // 59.92HZ
	INT32 nCyclesDone[1] = { 0 };

	vblank = 0;

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += SekRun(456);

		if ((i & 0x3f) == 0x1f) {
			SekSetIRQLine(4, CPU_IRQSTATUS_ACK);
		}

		if (i == 239) {
			vblank = 1;
			SekSetIRQLine(4, CPU_IRQSTATUS_ACK);

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}
	}

	SekClose();

	if (pBurnSoundOut) {
		BurnYM2413Render(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
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

		SekScan(nAction);

		BurnYM2413Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		AtariSlapsticScan(nAction, pnMin);
		AtariMoScan(nAction, pnMin);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Rampart (Trackball)

static struct BurnRomInfo rampartRomDesc[] = {
	{ "136082-1033.13l",			0x80000, 0x5c36795f, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "136082-1032.13j",			0x80000, 0xec7bc38c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136082-2031.13l",			0x10000, 0x07650c7e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136082-2030.13h",			0x10000, 0xe2bf2a26, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136082-1009.2n",				0x20000, 0x23b95f59, 2 | BRF_GRA },           //  4 Sprites

	{ "136082-1007.2d",				0x20000, 0xc96a0fc3, 3 | BRF_SND },           //  5 Samples
	{ "136082-1008.1d",				0x20000, 0x518218d9, 3 | BRF_SND },           //  6

	{ "rampart-eeprom.bin",			0x00800, 0x0be57615, 4 | BRF_PRG | BRF_ESS }, //  7 Default EEPROM Data

	{ "gal16v8-136082-1000.1j",		0x00117, 0x18f82b38, 5 | BRF_OPT },           //  8 PLDs
	{ "gal16v8-136082-1001.4l",		0x00117, 0x74d75d68, 5 | BRF_OPT },           //  9
	{ "gal16v8-136082-1002.7k",		0x00117, 0xf593401f, 5 | BRF_OPT },           // 10
	{ "gal20v8-136082-1003.8j",		0x00157, 0x67bb9705, 5 | BRF_OPT },           // 11
	{ "gal20v8-136082-1004.8m",		0x00157, 0x0001ed7d, 5 | BRF_OPT },           // 12
	{ "gal16v8-136082-1006.12c",	0x00117, 0x1f3b735d, 5 | BRF_OPT },           // 13
};

STD_ROM_PICK(rampart)
STD_ROM_FN(rampart)

static INT32 RampartInit()
{
	return DrvInit(0,0);
}

struct BurnDriverD BurnDrvRampart = {
	"rampart", NULL, NULL, NULL, "1990",
	"Rampart (Trackball)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rampartRomInfo, rampartRomName, NULL, NULL, NULL, NULL, RampartInputInfo, RampartDIPInfo,
	RampartInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Rampart (Joystick)

static struct BurnRomInfo rampart2pRomDesc[] = {
	{ "136082-1033.13l",			0x80000, 0x5c36795f, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "136082-1032.13j",			0x80000, 0xec7bc38c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136082-2051.13kl",			0x20000, 0xd4e26d0f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136082-2050.13h",			0x20000, 0xed2a49bd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136082-1019.2n",				0x20000, 0xefa38bef, 2 | BRF_GRA },           //  4 Sprites

	{ "136082-1007.2d",				0x20000, 0xc96a0fc3, 3 | BRF_SND },           //  5 Samples
	{ "136082-1008.1d",				0x20000, 0x518218d9, 3 | BRF_SND },           //  6

	{ "rampart-eeprom.bin",			0x00800, 0x0be57615, 4 | BRF_PRG | BRF_ESS }, //  7 Default EEPROM Data

	{ "gal16v8-136082-1000.1j",		0x00117, 0x18f82b38, 5 | BRF_OPT },           //  8 PLDs
	{ "gal16v8-136082-1001.4l",		0x00117, 0x74d75d68, 5 | BRF_OPT },           //  9
	{ "gal16v8-136082-1002.7k",		0x00117, 0xf593401f, 5 | BRF_OPT },           // 10
	{ "gal20v8-136082-1003.8j",		0x00157, 0x67bb9705, 5 | BRF_OPT },           // 11
	{ "gal20v8-136082-1004.8m",		0x00157, 0x0001ed7d, 5 | BRF_OPT },           // 12
	{ "gal16v8-136082-1056.12c",	0x00117, 0xbd70bf25, 5 | BRF_OPT },           // 13
};

STD_ROM_PICK(rampart2p)
STD_ROM_FN(rampart2p)

static INT32 Rampart2pInit()
{
	return DrvInit(0,1);
}

struct BurnDriverD BurnDrvRampart2p = {
	"rampart2p", "rampart", NULL, NULL, "1990",
	"Rampart (Joystick)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 3, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rampart2pRomInfo, rampart2pRomName, NULL, NULL, NULL, NULL, Ramprt2pInputInfo, Ramprt2pDIPInfo,
	Rampart2pInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Rampart (Japan, Joystick)

static struct BurnRomInfo rampartjRomDesc[] = {
	{ "136082-3451.bin",			0x20000, 0xc6596d32, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "136082-3450.bin",			0x20000, 0x563b33cc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136082-1463.bin",			0x20000, 0x65fe3491, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136082-1462.bin",			0x20000, 0xba731652, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136082-1465.bin",			0x20000, 0x9cb87d1b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136082-1464.bin",			0x20000, 0x2ff75c40, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136082-1467.bin",			0x20000, 0xe0cfcda5, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136082-1466.bin",			0x20000, 0xa7a5a951, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136082-2419.bin",			0x20000, 0x456a8aae, 2 | BRF_GRA },           //  8 Sprites

	{ "136082-1007.2d",				0x20000, 0xc96a0fc3, 3 | BRF_SND },           //  9 Samples
	{ "136082-1008.1d",				0x20000, 0x518218d9, 3 | BRF_SND },           // 10

	{ "rampartj-eeprom.bin",		0x00800, 0x096cacdc, 4 | BRF_PRG | BRF_ESS }, // 11 Default EEPROM Data

	{ "gal16v8-136082-1000.1j",		0x00117, 0x18f82b38, 5 | BRF_OPT },           // 12 PLDs
	{ "gal16v8-136082-1001.4l",		0x00117, 0x74d75d68, 5 | BRF_OPT },           // 13
	{ "gal16v8-136082-1002.7k",		0x00117, 0xf593401f, 5 | BRF_OPT },           // 14
	{ "gal20v8-136082-1003.8j",		0x00157, 0x67bb9705, 5 | BRF_OPT },           // 15
	{ "gal20v8-136082-1004.8m",		0x00157, 0x0001ed7d, 5 | BRF_OPT },           // 16
	{ "gal16v8-136082-1005.12c",	0x00117, 0x42c05114, 5 | BRF_OPT },           // 17
};

STD_ROM_PICK(rampartj)
STD_ROM_FN(rampartj)

static INT32 RampartjInit()
{
	return DrvInit(1, 1);
}

struct BurnDriverD BurnDrvRampartj = {
	"rampartj", "rampart", NULL, NULL, "1990",
	"Rampart (Japan, Joystick)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rampartjRomInfo, rampartjRomName, NULL, NULL, NULL, NULL, RampartjInputInfo, RampartjDIPInfo,
	RampartjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};
