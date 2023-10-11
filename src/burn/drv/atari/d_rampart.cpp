// FB Neo Atari Rampart driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "atarimo.h"
#include "atariic.h"
#include "burn_ym2413.h"
#include "msm6295.h"
#include "watchdog.h"
#include "burn_gun.h"

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
static UINT8 *DrvEEPROM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoyF[1]; // p3 fake coin
static UINT16 DrvInputs[3];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static ButtonToggle Diag;

static INT16 Analog[6];

static INT32 has_trackball = 0;
static INT32 is_rampartj = 0;

static INT32 nCyclesExtra[1];

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo RampartInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 8,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 9,	"p1 fire 2"	},

	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 fire 2"	},

	A("P2 Trackball X", BIT_ANALOG_REL, &Analog[2],		"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &Analog[3],		"p2 y-axis"),

	{"P3 Coin",			BIT_DIGITAL,	DrvJoyF + 0,	"p3 coin"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy1 + 10,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy2 + 10,	"p3 fire 4"	},

	A("P3 Trackball X", BIT_ANALOG_REL, &Analog[4],		"p3 x-axis"),
	A("P3 Trackball Y", BIT_ANALOG_REL, &Analog[5],		"p3 y-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 11,	"diag"		},
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

	{"P3 Coin",			BIT_DIGITAL,	DrvJoyF + 0,	"p3 coin"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 10,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 11,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 8,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 9,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy1 + 10,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy2 + 10,	"p3 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 11,	"diag"		},
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
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 11,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Rampartj)

static struct BurnDIPInfo RampartDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0x04, NULL				},
};

STDDIPINFO(Rampart)

static struct BurnDIPInfo Ramprt2pDIPList[]=
{
	DIP_OFFSET(0x1a)
	{0x00, 0xff, 0xff, 0x04, NULL				},

	{0   , 0xfe, 0   ,    2, "Players"			},
	{0x00, 0x01, 0x04, 0x00, "2"				},
	{0x00, 0x01, 0x04, 0x04, "3"				},
};

STDDIPINFO(Ramprt2p)

static struct BurnDIPInfo RampartjDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0x04, NULL				},
};

STDDIPINFO(Rampartj)

static void __fastcall rampart_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x220000 && address < 0x3e0000) return; // 220000-3bffff & 3c0800-3dffff NOP

	if ((address & 0xfff800) == 0x3e0000) {
		*((UINT16*)(DrvMobRAM + (address & 0x7fe))) = BURN_ENDIAN_SWAP_INT16(data);
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
			MSM6295SetRoute(0, (data & 0x20) ? 1.0 : 0.0, BURN_SND_ROUTE_BOTH);
			BurnYM2413SetAllRoutes(((data >> 1) & 7) / 7.0, BURN_SND_ROUTE_BOTH);
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
		AtariMoWrite(0, (address & 0x7fe)/2, BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvMobRAM + (address & 0xffe)))));
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
			if (address & 1) {
				MSM6295SetRoute(0, (data & 0x20) ? 1.0 : 0.0, BURN_SND_ROUTE_BOTH);
				BurnYM2413SetAllRoutes(((data >> 1) & 7) / 7.0, BURN_SND_ROUTE_BOTH);
			}
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
			if (has_trackball) {
				UINT16 ret  = BurnTrackballRead(1, 1) << 0; // p2 y
				       ret |= BurnTrackballRead(2, 1) << 8; // p3 y
				return ret;
			}
			return DrvInputs[2];

		case 0x6c0002:
			if (has_trackball) {
				UINT16 ret  = BurnTrackballRead(1, 0) << 0; // p2 x
				       ret |= BurnTrackballRead(2, 0) << 8; // p3 x
				return ret;
			}
			return 0xffff;
		case 0x6c0004:
			if (has_trackball) {
				UINT16 ret  = BurnTrackballRead(0, 1); // p1 y
				       ret |= 0xff00;
				return ret;
			}
			return 0xffff;
		case 0x6c0006:
			if (has_trackball) {
				UINT16 ret  = BurnTrackballRead(0, 0); // p1 x
				       ret |= 0xff00;
				return ret;
			}
			return 0xffff;
	}

	bprintf (0, _T("MRW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall rampart_read_byte(UINT32 address)
{
	return rampart_read_word(address & ~1) >> ((~address & 1) * 8);
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

	memset(nCyclesExtra, 0, sizeof(nCyclesExtra));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x200000;

	DrvGfxROM0			= Next; Next += 0x040000;

	MSM6295ROM			= Next;
	DrvSndROM			= Next; Next += 0x040000;

	DrvEEPROM           = Next; Next += 0x000800;

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

	BurnAllocMemIndex();

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

		if (BurnLoadRom(DrvEEPROM  + 0x000000,  k++, 1)) return 1;

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
	AtariEEPROMLoad(DrvEEPROM);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 480);

	BurnYM2413Init(3579545);
	BurnYM2413SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1193182 / MSM6295_PIN7_LOW, 1);
	MSM6295SetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x040000, 0x100, 0x0f);

	AtariMoInit(0, &modesc);

	BurnTrackballInit(3);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	BurnTrackballExit();

	BurnYM2413Exit();
	MSM6295Exit();

	AtariMoExit();
	AtariSlapsticExit();
	AtariEEPROMExit();

	BurnFreeMemIndex();

	has_trackball = 0;
	is_rampartj = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;
	for (INT32 offs = 0; offs < 0x800/2; offs+=2)
	{
		UINT16 d = BURN_ENDIAN_SWAP_INT16((p[offs] & 0xff00) | (p[offs+1] >> 8));
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
		Diag.Toggle(DrvJoy2[11]);
		DrvJoy2[2] |= (is_rampartj == 0) ? DrvJoyF[0] : 0; // p3 fake coin, all sets but rampartj

		DrvInputs[0] = 0xf7fb | (DrvDips[0] & 0x04);
		DrvInputs[1] = 0xffff;
		DrvInputs[2] = 0x0000; // active high

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (has_trackball) {
			BurnTrackballConfig(0, AXIS_REVERSED, AXIS_REVERSED);
			BurnTrackballFrame(0, Analog[0], Analog[1], 0x00, 0x3f);
			BurnTrackballUpdate(0);

			BurnTrackballConfig(1, AXIS_REVERSED, AXIS_REVERSED);
			BurnTrackballFrame(1, Analog[2], Analog[3], 0x00, 0x3f);
			BurnTrackballUpdate(1);

			BurnTrackballConfig(2, AXIS_REVERSED, AXIS_REVERSED);
			BurnTrackballFrame(2, Analog[4], Analog[5], 0x00, 0x3f);
			BurnTrackballUpdate(2);
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { (INT32)(7159090 / 59.92) }; // 59.92HZ
	INT32 nCyclesDone[1] = { nCyclesExtra[0] };

	vblank = 0;

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if ((i & 0x3f) == 0) {
			SekSetIRQLine(4, CPU_IRQSTATUS_ACK);
		}

		if ((i & 0x3f) == 0 && has_trackball) {
			BurnTrackballUpdate(0);
			BurnTrackballUpdate(1);
			BurnTrackballUpdate(2);
		}

		if (i == 239) {
			vblank = 1;

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}
	}

	SekClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];

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

		BurnTrackballScan();

		Diag.Scan();

		SCAN_VAR(nCyclesExtra);
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
	has_trackball = 1;

	return DrvInit(0, 0);
}

struct BurnDriver BurnDrvRampart = {
	"rampart", NULL, NULL, NULL, "1990",
	"Rampart (Trackball)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_MISC_POST90S, GBF_ACTION | GBF_STRATEGY, 0,
	NULL, rampartRomInfo, rampartRomName, NULL, NULL, NULL, NULL, RampartInputInfo, RampartDIPInfo,
	RampartInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Rampart (Joystick, bigger ROMs)

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
	return DrvInit(0, 1);
}

struct BurnDriver BurnDrvRampart2p = {
	"rampart2p", "rampart", NULL, NULL, "1990",
	"Rampart (Joystick, bigger ROMs)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_MISC_POST90S, GBF_ACTION | GBF_STRATEGY, 0,
	NULL, rampart2pRomInfo, rampart2pRomName, NULL, NULL, NULL, NULL, Ramprt2pInputInfo, Ramprt2pDIPInfo,
	Rampart2pInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Rampart (Joystick, smaller ROMs)
// original Atari PCB but with mostly hand-written labels, uses smaller ROMs for the main CPU

static struct BurnRomInfo rampart2paRomDesc[] = {
	{ "0h.13k-l",					0x20000, 0xd4e26d0f, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "0l.13h",						0x20000, 0xed2a49bd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1h.13l",						0x20000, 0xb232b807, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1l.13h-j",					0x20000, 0xa2db78b1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "2h.13m",						0x20000, 0x37b32b7e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "2l.13j",						0x20000, 0x00cd567b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "3h.13n",						0x20000, 0xc23b1c98, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "3l.13k",						0x20000, 0x0a12ca83, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "atr.2n",						0x20000, 0xefa38bef, 2 | BRF_GRA },           //  8 Sprites

	{ "arom0_2_player_136082-1007.2d",	0x20000, 0xc96a0fc3, 3 | BRF_SND },       //  9 Samples
	{ "arom1_2_player_136082-1006.1d",	0x20000, 0x518218d9, 3 | BRF_SND },       // 10

	{ "rampart-eeprom.bin",			0x00800, 0x0be57615, 4 | BRF_PRG | BRF_ESS }, // 11 Default EEPROM Data

	{ "gal16v8-136082-1000.1j",		0x00117, 0x18f82b38, 5 | BRF_OPT },           // 12 PLDs
	{ "gal16v8-136082-1001.4l",		0x00117, 0x74d75d68, 5 | BRF_OPT },           // 13
	{ "gal16v8-136082-1002.7k",		0x00117, 0xf593401f, 5 | BRF_OPT },           // 14
	{ "gal20v8-136082-1003.8j",		0x00157, 0x67bb9705, 5 | BRF_OPT },           // 15
	{ "gal20v8-136082-1004.8m",		0x00157, 0x0001ed7d, 5 | BRF_OPT },           // 16
	{ "gal16v8-136082-1005.12c",	0x00117, 0x42c05114, 5 | BRF_OPT },           // 17
};

STD_ROM_PICK(rampart2pa)
STD_ROM_FN(rampart2pa)

static INT32 Rampart2paInit()
{
	return DrvInit(1, 1);
}

struct BurnDriver BurnDrvRampart2pa = {
	"rampart2pa", "rampart", NULL, NULL, "1990",
	"Rampart (Joystick, smaller ROMs)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_MISC_POST90S, GBF_ACTION | GBF_STRATEGY, 0,
	NULL, rampart2paRomInfo, rampart2paRomName, NULL, NULL, NULL, NULL, Ramprt2pInputInfo, Ramprt2pDIPInfo,
	Rampart2paInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};


// Rampart (Japan, Joystick)

static struct BurnRomInfo rampartjRomDesc[] = {
	{ "136082-3451.13kl",			0x20000, 0xc6596d32, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "136082-3450.13h",			0x20000, 0x563b33cc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136082-1463.13l",			0x20000, 0x65fe3491, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136082-1462.13hj",			0x20000, 0xba731652, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136082-1465.13m",			0x20000, 0x9cb87d1b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136082-1464.13j",			0x20000, 0x2ff75c40, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136082-1467.13n",			0x20000, 0xe0cfcda5, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136082-1466.13k",			0x20000, 0xa7a5a951, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136082-2419.2m",				0x20000, 0x456a8aae, 2 | BRF_GRA },           //  8 Sprites

	{ "136082-1007.2d",				0x20000, 0xc96a0fc3, 3 | BRF_SND },           //  9 Samples
	{ "136082-1008.1d",				0x20000, 0x518218d9, 3 | BRF_SND },           // 10

	{ "rampartj-eeprom.13f",		0x00800, 0x096cacdc, 4 | BRF_PRG | BRF_ESS }, // 11 Default EEPROM Data

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
	is_rampartj = 1;

	return DrvInit(1, 1);
}

struct BurnDriver BurnDrvRampartj = {
	"rampartj", "rampart", NULL, NULL, "1990",
	"Rampart (Japan, Joystick)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_ACTION | GBF_STRATEGY, 0,
	NULL, rampartjRomInfo, rampartjRomName, NULL, NULL, NULL, NULL, RampartjInputInfo, RampartjDIPInfo,
	RampartjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};
