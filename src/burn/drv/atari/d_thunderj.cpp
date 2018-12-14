// FB Alpha Atari ThunderJaws driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "atariic.h"
#include "atarimo.h"
#include "atarivad.h"
#include "atarijsa.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM0;
static UINT8 *Drv68KROM1;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *DrvMobRAM;
static UINT8 *DrvAlphaRAM;
static UINT8 *Drv68KRAM0;
static UINT8 *DrvShareRAM;
static UINT16 *DrvEOFData;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles[3];

static INT32 subcpu_halted;
static INT32 alpha_tile_bank;
static INT32 scanline_int_state;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT16 DrvInputs[3];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static struct BurnInputInfo ThunderjInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"	},

	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 15,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 14,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 13,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 2"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 15,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 14,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 13,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Thunderj)

static struct BurnDIPInfo ThunderjDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x0f, 0x01, 0x02, 0x02, "Off"			},
	{0x0f, 0x01, 0x02, 0x00, "On"			},
};

STDDIPINFO(Thunderj)

static void latch_write(UINT16 data)
{
	subcpu_halted = ~data & 0x01;

	if (subcpu_halted)
	{
		INT32 active = SekGetActive();
		if (active == 0) {
			SekClose();
			SekOpen(1);
			SekReset();
			SekClose();
			SekOpen(0);
		} else {
			SekRunEnd();
			SekReset();
		}
	}

	alpha_tile_bank = (data >> 2) & 7;
}

static void __fastcall thunderj_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffe000) == 0x3f6000) {
		*((UINT16*)(DrvMobRAM + (address & 0x1ffe))) = data;
		AtariMoWrite(0, (address / 2) & 0xfff, data);
		return;
	}

	if ((address & 0xff0000) == 0x1f0000) {
		AtariEEPROMUnlockWrite();
		return;
	}

	switch (address)
	{
		case 0x2e0000:
			BurnWatchdogWrite();
		return;

		case 0x360010:
			latch_write(data);
		return;

		case 0x360020:
			AtariJSAResetWrite(data);
		return;

		case 0x360030:
			AtariJSAWrite(data);
		return;
	}
}

static void __fastcall thunderj_main_write_byte(UINT32 address, UINT8 data)
{

	if ((address & 0xffe000) == 0x3f6000) {
		DrvMobRAM[(address & 0x1fff)^1] = data;
		AtariMoWrite(0, (address / 2) & 0xfff, *((UINT16*)(DrvMobRAM + (address & 0x1ffe))));
		return;
	}

	if ((address & 0xff0000) == 0x1f0000) {
		AtariEEPROMUnlockWrite();
		return;
	}

	switch (address)
	{
		case 0x2e0000:
		case 0x2e0001:
			BurnWatchdogWrite();
		return;

		case 0x360011:
			latch_write(data);
		return;

		case 0x360020:
		case 0x360021:
			AtariJSAResetWrite(data);
		return;

		case 0x360030:
		case 0x360031:
			AtariJSAWrite(data);
		return;
	}
}

static inline UINT16 special_read()
{
	UINT16 ret = (DrvInputs[1] & ~0x12) | (DrvDips[0] & 2);

	if (vblank) ret ^= 0x01;
	if (atarigen_sound_to_cpu_ready) ret ^= 0x04;
	if (atarigen_cpu_to_sound_ready) ret ^= 0x08;
	// 0x10 ??
	
	return ret;
}

static UINT16 __fastcall thunderj_main_read_word(UINT32 address)
{
	if ((address & 0xfffff0) == 0x260000) {
		return 0xffff;
	}

	switch (address)
	{
		case 0x260010:
			return DrvInputs[0];

		case 0x260012:
			return special_read();

		case 0x260030:
			return AtariJSARead();
	}

	return 0;
}

static UINT8 __fastcall thunderj_main_read_byte(UINT32 address)
{
	if ((address & 0xfffff0) == 0x260000) {
		return 0xff;
	}

	switch (address)
	{
		case 0x260010:
			return DrvInputs[0] >> 8;

		case 0x260011:
			return DrvInputs[0];

		case 0x260012:
			return special_read() >> 8;

		case 0x260013:
			return special_read();

		case 0x260030:
			return AtariJSARead() >> 8;

		case 0x260031:
			return AtariJSARead();
	}

	return 0;
}

static tilemap_callback( alpha )
{
	UINT16 data = *((UINT16*)(DrvAlphaRAM + offs * 2));
	INT32 code = (data & 0x1ff);
	if (data & 0x200) code += alpha_tile_bank * 0x200;
	INT32 color = ((data >> 10) & 0xf) | ((data >> 9) & 0x20);

	TILE_SET_INFO(3, code, color, (data & 0x8000) ? TILE_OPAQUE : 0);
}

static void update_interrupts()
{
	int newstate = 0;
	int newstate2 = 0;
	INT32 active = SekGetActive();

	if (scanline_int_state)
		newstate |= 4, newstate2 |= 4;
	if (atarijsa_int_state)
		newstate |= 6;

	if (active == 1)
	{
		SekClose();
		SekOpen(0);
	}

	if (newstate)
		SekSetIRQLine(newstate, CPU_IRQSTATUS_ACK);
	else	
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);

	SekClose();
	SekOpen(1);

	if (newstate2)
		SekSetIRQLine(newstate2, CPU_IRQSTATUS_ACK);
	else	
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
	
	if (active == 0)
	{
		SekClose();
		SekOpen(0);
	}
}

static void scanline_timer(INT32 state)
{
	scanline_int_state = state;
	update_interrupts();
}

static void palette_write(INT32 offset, UINT16 data)
{
	UINT8 i = data >> 15;
	UINT8 r = ((data >> 9) & 0x3e) | i;
	UINT8 g = ((data >> 4) & 0x3e) | i;
	UINT8 b = ((data << 1) & 0x3e) | i;

	r = (r << 2) | (r >> 4);
	g = (g << 2) | (g >> 4);
	b = (b << 2) | (b >> 4);

	DrvPalette[offset] = BurnHighCol(r,g,b,0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	SekOpen(1);
	SekReset();
	SekClose();

	BurnWatchdogReset();

	AtariJSAReset();
	AtariVADReset();
	AtariEEPROMReset();

	subcpu_halted = 0;
	alpha_tile_bank = 0;
	scanline_int_state = 0;

	nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = 0;

	SetCurrentFrame(0);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM0			= Next; Next += 0x0a0000;
	Drv68KROM1			= Next; Next += 0x020000;

	DrvM6502ROM			= Next; Next += 0x010000;

	DrvGfxROM0			= Next; Next += 0x200000;
	DrvGfxROM1			= Next; Next += 0x200000;
	DrvGfxROM2			= Next; Next += 0x040000;

	DrvSndROM			= Next; Next += 0x080000; // oki_bankswitch in jsadev can map up to this amount. -dink (crash prev.)

	DrvPalette			= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam				= Next;

	DrvMobRAM			= Next; Next += 0x002000;
	DrvAlphaRAM			= Next; Next += 0x001000;
	Drv68KRAM0			= Next; Next += 0x007000;
	DrvShareRAM			= Next; Next += 0x010000;

	DrvEOFData			= (UINT16*)(DrvAlphaRAM + 0xf00);
	atarimo_0_slipram	= (UINT16*)(DrvAlphaRAM + 0xf80);

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { STEP4((0x40000*8)*3, -(0x40000*8)) };
	INT32 XOffs0[8] = { STEP8(0,1) };
	INT32 YOffs0[8] = { STEP8(0,8) };

	INT32 Plane1[2] = { 0, 4 };
	INT32 XOffs1[8] = { STEP4(0,1), STEP4(8,1) };
	INT32 YOffs1[8] = { STEP8(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x100000; i++) tmp[i] = DrvGfxROM0[i] ^ 0xff;

	GfxDecode(0x8000, 4, 8, 8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

	for (INT32 i = 0; i < 0x100000; i++) tmp[i] = DrvGfxROM1[i] ^ 0xff;

	GfxDecode(0x8000, 4, 8, 8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM1);

	for (INT32 i = 0; i < 0x010000; i++) tmp[i] = DrvGfxROM2[i];

	GfxDecode(0x1000, 2, 8, 8, Plane1, XOffs1, YOffs1, 0x080, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	static const struct atarimo_desc modesc =
	{
		2,					/* index to which gfx system */
		1,					/* number of motion object banks */
		1,					/* are the entries linked? */
		0,					/* are the entries split? */
		1,					/* render in reverse order? */
		0,					/* render in swapped X/Y order? */
		0,					/* does the neighbor bit affect the next object? */
		8,					/* pixels per SLIP entry (0 for no-slip) */
		0,					/* pixel offset for SLIPs */
		0,					/* maximum number of links to visit/scanline (0=all) */

		0x100,				/* base palette entry */
		0x100,				/* maximum number of colors */
		0,					/* transparent pen index */

		{{ 0x03ff,0,0,0 }},	/* mask for the link */
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
		{{ 0,0,0x0070,0 }},	/* mask for the priority */
		{{ 0 }},			/* mask for the neighbor */
		{{ 0 }},			/* mask for absolute coordinates */

		{{ 0 }},			/* mask for the special value */
		0,					/* resulting value to indicate "special" */
		NULL				/* callback routine for special entries */
	};

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM0 + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x020000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x040000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x060001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x060000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x080001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x080000, k++, 2)) return 1;

		if (BurnLoadRom(Drv68KROM1 + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM1 + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6502ROM+ 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x030000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x050000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x070000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x090000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0b0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0d0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0e0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0f0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x030000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x050000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x070000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x090000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0b0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0d0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0e0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0f0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x070000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	GenericTilesInit();
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, alpha_map_callback, 8, 8, 64, 32);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x200000, 0x300, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM0, 4, 8, 8, 0x200000, 0x200, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM1, 4, 8, 8, 0x200000, 0x100, 0xff);
	GenericTilemapSetGfx(3, DrvGfxROM2, 2, 8, 8, 0x040000, 0x000, 0x3f);

	AtariVADInit(0, 1, 0, scanline_timer, palette_write);
	AtariMoInit(0, &modesc);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM0,			0x000000, 0x09ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM,			0x160000, 0x16ffff, MAP_RAM); // shared!!
	SekMapMemory(DrvMobRAM,				0x3f6000, 0x3f7fff, MAP_ROM); // handler
	SekMapMemory(DrvAlphaRAM,			0x3f8000, 0x3f8fff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,			0x3f9000, 0x3fffff, MAP_RAM);
	SekSetWriteWordHandler(0,			thunderj_main_write_word);
	SekSetWriteByteHandler(0,			thunderj_main_write_byte);
	SekSetReadWordHandler(0,			thunderj_main_read_word);
	SekSetReadByteHandler(0,			thunderj_main_read_byte);

	AtariVADMap(0x3e0000, 0x3f5fff, 0);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(1, 0x0e0000, 0x0e0fff);
	SekClose();

	SekInit(1, 0x68000);
	SekOpen(1);
	SekMapMemory(Drv68KROM1,			0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(Drv68KROM0 + 0x60000,	0x060000, 0x07ffff, MAP_ROM); //shared
	SekMapMemory(DrvShareRAM,			0x160000, 0x16ffff, MAP_RAM); //shared
	SekSetWriteWordHandler(0,			thunderj_main_write_word);
	SekSetWriteByteHandler(0,			thunderj_main_write_byte);
	SekSetReadWordHandler(0,			thunderj_main_read_word);
	SekSetReadByteHandler(0,			thunderj_main_read_byte);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	AtariJSAInit(DrvM6502ROM, &update_interrupts, DrvSndROM, NULL);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();

	AtariEEPROMExit();
	AtariVADExit();
	AtariJSAExit();
	AtariMoExit();

	BurnFree(AllMem);

	return 0;
}

static void copy_sprites_step1()
{
	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	for (INT32 y = miny; y < maxy; y++)
	{
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);
		UINT8 *pri = BurnBitmapGetPrimapPosition(0, 0, y);

		for (INT32 x = minx; x < maxx; x++)
		{
			if (mo[x] != 0xffff)
			{
				INT32 mopriority = mo[x] >> 12;
				INT32 pfm = 0;

				if (mopriority & 4)
					continue;

				if ((mo[x] & 0xff) == 1)
					pfm = 1;
				else if (pf[x] & 8)
				{
					INT32 pfpriority = (pri[x] & 0x80) ? ((pri[x] >> 2) & 3) : (pri[x] & 3);
					if (((pfpriority == 3) && !(mopriority & 1)) ||
						((pfpriority & 1) && (mopriority == 0)) ||
						((pfpriority & 2) && !(mopriority & 2)))
						pfm = 1;
				}

				if (!pfm)
					pf[x] = mo[x] & 0x7ff;
			}
		}
	}
}

static void copy_sprites_step2()
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
				INT32 mopriority = mo[x] >> 12;

				if (mopriority & 4)
				{
					if (mo[x] & 2)
						atarimo_apply_stain(pf, mo, x, y, maxx);
				}

				mo[x] = 0xffff; // clean!
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

	AtariVADDraw(pTransDraw, 1);

	AtariMoRender(0);

	if (nSpriteEnable & 1) copy_sprites_step1();

	GenericTilemapDraw(2, pTransDraw, 0);

	if (nSpriteEnable & 2) copy_sprites_step2();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	SekNewFrame();
	M6502NewFrame();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		DrvInputs[0] = 0xffff;
		DrvInputs[1] = 0xffff;
		DrvInputs[2] = 0x0040;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		atarijsa_input_port = DrvInputs[2]; // coin
		atarijsa_test_mask = 0x02;
		atarijsa_test_port = (DrvDips[0] & atarijsa_test_mask);
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 262;
	INT32 nCyclesTotal[3] = { (INT32)(7159090 / 59.92), (INT32)(7159090 / 59.92), (INT32)(1789773 / 59.92) };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2] };

	M6502Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		atarivad_scanline = i;

		if (i == 261) AtariVADEOFUpdate(DrvEOFData);

		SekOpen(0);
		if (atarivad_scanline_timer_enabled) {
			if (atarivad_scanline_timer == atarivad_scanline) {
				scanline_timer(CPU_IRQSTATUS_ACK);
			}
		}
		nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		nCyclesDone[2] += M6502Run(((i + 1) * nCyclesTotal[2] / nInterleave) - nCyclesDone[2]);
		SekClose();

		SekOpen(1);
		if (subcpu_halted == 0) {
			nCyclesDone[1] += SekRun(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		} else {
			nCyclesDone[1] += SekIdle(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		}
		SekClose();

		if (i <= 240) {
			AtariVADTileRowUpdate(i, (UINT16*)DrvAlphaRAM);
		}

		if (i == 239) {
			vblank = 1;

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		AtariJSAInterruptUpdate(nInterleave);

		if (pBurnSoundOut && i&1) {
			INT32 nSegment = nBurnSoundLen / (nInterleave / 2);
			SekOpen(0);
			AtariJSAUpdate(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			SekClose();
			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			SekOpen(0);
			AtariJSAUpdate(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			SekClose();
		}
	}

	M6502Close();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];

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

		AtariJSAScan(nAction, pnMin);
		AtariVADScan(nAction, pnMin);
		AtariMoScan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		SCAN_VAR(subcpu_halted);
		SCAN_VAR(alpha_tile_bank);
		SCAN_VAR(scanline_int_state);
		SCAN_VAR(nExtraCycles);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// ThunderJaws (rev 3)

static struct BurnRomInfo thunderjRomDesc[] = {
	{ "136076-3001.14e",			0x10000, 0xcda7f027, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 #0 Code
	{ "136076-3002.14c",			0x10000, 0x752cead3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136076-2003.15e",			0x10000, 0x6e155469, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136076-2004.15c",			0x10000, 0xe9ff1e42, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136076-2005.16e",			0x10000, 0xa40242e7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136076-2006.16c",			0x10000, 0xaa18b94c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136076-1009.15h",			0x10000, 0x05474ebb, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136076-1010.16h",			0x10000, 0xccff21c8, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136076-1007.17e",			0x10000, 0x9c2a8aba, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136076-1008.17c",			0x10000, 0x22109d16, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "136076-1011.17l",			0x10000, 0xbbbbca45, 2 | BRF_GRA },           // 10 M68000 #1 Code
	{ "136076-1012.17n",			0x10000, 0x53e5e638, 2 | BRF_GRA },           // 11

	{ "136076-2015.1b",				0x10000, 0xd8feb7fb, 3 | BRF_PRG | BRF_ESS }, // 12 M6502 Code

	{ "136076-1021.5s",				0x10000, 0xd8432766, 4 | BRF_GRA },           // 13 VAD Tiles
	{ "136076-1025.5r",				0x10000, 0x839feed5, 4 | BRF_GRA },           // 14
	{ "136076-1029.3p",				0x10000, 0xfa887662, 4 | BRF_GRA },           // 15
	{ "136076-1033.6p",				0x10000, 0x2addda79, 4 | BRF_GRA },           // 16
	{ "136076-1022.9s",				0x10000, 0xdcf50371, 4 | BRF_GRA },           // 17
	{ "136076-1026.9r",				0x10000, 0x216e72c8, 4 | BRF_GRA },           // 18
	{ "136076-1030.10s",			0x10000, 0xdc51f606, 4 | BRF_GRA },           // 19
	{ "136076-1034.10r",			0x10000, 0xf8e35516, 4 | BRF_GRA },           // 20
	{ "136076-1023.13s",			0x10000, 0xb6dc3f13, 4 | BRF_GRA },           // 21
	{ "136076-1027.13r",			0x10000, 0x621cc2ce, 4 | BRF_GRA },           // 22
	{ "136076-1031.14s",			0x10000, 0x4682ceb5, 4 | BRF_GRA },           // 23
	{ "136076-1035.14r",			0x10000, 0x7a0e1b9e, 4 | BRF_GRA },           // 24
	{ "136076-1024.17s",			0x10000, 0xd84452b5, 4 | BRF_GRA },           // 25
	{ "136076-1028.17r",			0x10000, 0x0cc20245, 4 | BRF_GRA },           // 26
	{ "136076-1032.14p",			0x10000, 0xf639161a, 4 | BRF_GRA },           // 27
	{ "136076-1036.16p",			0x10000, 0xb342443d, 4 | BRF_GRA },           // 28

	{ "136076-1037.2s",				0x10000, 0x07addba6, 5 | BRF_GRA },           // 29 MOB Tiles
	{ "136076-1041.2r",				0x10000, 0x1e9c29e4, 5 | BRF_GRA },           // 30
	{ "136076-1045.34s",			0x10000, 0xe7235876, 5 | BRF_GRA },           // 31
	{ "136076-1049.34r",			0x10000, 0xa6eb8265, 5 | BRF_GRA },           // 32
	{ "136076-1038.6s",				0x10000, 0x2ea543f9, 5 | BRF_GRA },           // 33
	{ "136076-1042.6r",				0x10000, 0xefabdc2b, 5 | BRF_GRA },           // 34
	{ "136076-1046.7s",				0x10000, 0x6692151f, 5 | BRF_GRA },           // 35
	{ "136076-1050.7r",				0x10000, 0xad7bb5f3, 5 | BRF_GRA },           // 36
	{ "136076-1039.11s",			0x10000, 0xcb563a40, 5 | BRF_GRA },           // 37
	{ "136076-1043.11r",			0x10000, 0xb7565eee, 5 | BRF_GRA },           // 38
	{ "136076-1047.12s",			0x10000, 0x60877136, 5 | BRF_GRA },           // 39
	{ "136076-1051.12r",			0x10000, 0xd4715ff0, 5 | BRF_GRA },           // 40
	{ "136076-1040.15s",			0x10000, 0x6e910fc2, 5 | BRF_GRA },           // 41
	{ "136076-1044.15r",			0x10000, 0xff67a17a, 5 | BRF_GRA },           // 42
	{ "136076-1048.16s",			0x10000, 0x200d45b3, 5 | BRF_GRA },           // 43
	{ "136076-1052.16r",			0x10000, 0x74711ef1, 5 | BRF_GRA },           // 44

	{ "136076-1020.4m",				0x10000, 0x65470354, 6 | BRF_GRA },           // 45 Alpha Tiles

	{ "136076-1016.7k",				0x10000, 0xc10bdf73, 7 | BRF_GRA },           // 46 Samples
	{ "136076-1017.7j",				0x10000, 0x4e5e25e8, 7 | BRF_GRA },           // 47
	{ "136076-1018.7e",				0x10000, 0xec81895d, 7 | BRF_GRA },           // 48
	{ "136076-1019.7d",				0x10000, 0xa4009037, 7 | BRF_GRA },           // 49

	{ "gal16v8a-136076-1070.12e",	0x00117, 0x0e96e51e, 8 | BRF_OPT },           // 50 PLDs
	{ "gal16v8a-136076-1071.13j",	0x00117, 0xad7beb34, 8 | BRF_OPT },           // 51
	{ "gal16v8a-136076-1072.5n",	0x00117, 0x2827620a, 8 | BRF_OPT },           // 52
	{ "gal16v8a-136076-1073.2h",	0x00117, 0xa2fe4402, 8 | BRF_OPT },           // 53
	{ "gal16v8a-136076-2074.3l",	0x00117, 0x0e8bc5d7, 8 | BRF_OPT },           // 54
	{ "gal20v8a-136076-1075.2l",	0x00157, 0xe75e1e86, 8 | BRF_OPT },           // 55
	{ "gal20v8a-136076-1076.1l",	0x00157, 0xf36f873a, 8 | BRF_OPT },           // 56
};

STD_ROM_PICK(thunderj)
STD_ROM_FN(thunderj)

struct BurnDriver BurnDrvThunderj = {
	"thunderj", NULL, NULL, NULL, "1990",
	"ThunderJaws (rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, thunderjRomInfo, thunderjRomName, NULL, NULL, NULL, NULL, ThunderjInputInfo, ThunderjDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};


// ThunderJaws (rev 2)

static struct BurnRomInfo thunderjaRomDesc[] = {
	{ "136076-2001.14e",			0x10000, 0xf6a71532, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 #0 Code
	{ "136076-2002.14c",			0x10000, 0x173ec10d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136076-2003.15e",			0x10000, 0x6e155469, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136076-2004.15c",			0x10000, 0xe9ff1e42, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136076-2005.16e",			0x10000, 0xa40242e7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136076-2006.16c",			0x10000, 0xaa18b94c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136076-1009.15h",			0x10000, 0x05474ebb, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136076-1010.16h",			0x10000, 0xccff21c8, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136076-1007.17e",			0x10000, 0x9c2a8aba, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136076-1008.17c",			0x10000, 0x22109d16, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "136076-1011.17l",			0x10000, 0xbbbbca45, 2 | BRF_GRA },           // 10 M68000 #1 Code
	{ "136076-1012.17n",			0x10000, 0x53e5e638, 2 | BRF_GRA },           // 11

	{ "136076-2015.1b",				0x10000, 0xd8feb7fb, 3 | BRF_PRG | BRF_ESS }, // 12 M6502 Code

	{ "136076-1021.5s",				0x10000, 0xd8432766, 4 | BRF_GRA },           // 13 VAD Tiles
	{ "136076-1025.5r",				0x10000, 0x839feed5, 4 | BRF_GRA },           // 14
	{ "136076-1029.3p",				0x10000, 0xfa887662, 4 | BRF_GRA },           // 15
	{ "136076-1033.6p",				0x10000, 0x2addda79, 4 | BRF_GRA },           // 16
	{ "136076-1022.9s",				0x10000, 0xdcf50371, 4 | BRF_GRA },           // 17
	{ "136076-1026.9r",				0x10000, 0x216e72c8, 4 | BRF_GRA },           // 18
	{ "136076-1030.10s",			0x10000, 0xdc51f606, 4 | BRF_GRA },           // 19
	{ "136076-1034.10r",			0x10000, 0xf8e35516, 4 | BRF_GRA },           // 20
	{ "136076-1023.13s",			0x10000, 0xb6dc3f13, 4 | BRF_GRA },           // 21
	{ "136076-1027.13r",			0x10000, 0x621cc2ce, 4 | BRF_GRA },           // 22
	{ "136076-1031.14s",			0x10000, 0x4682ceb5, 4 | BRF_GRA },           // 23
	{ "136076-1035.14r",			0x10000, 0x7a0e1b9e, 4 | BRF_GRA },           // 24
	{ "136076-1024.17s",			0x10000, 0xd84452b5, 4 | BRF_GRA },           // 25
	{ "136076-1028.17r",			0x10000, 0x0cc20245, 4 | BRF_GRA },           // 26
	{ "136076-1032.14p",			0x10000, 0xf639161a, 4 | BRF_GRA },           // 27
	{ "136076-1036.16p",			0x10000, 0xb342443d, 4 | BRF_GRA },           // 28

	{ "136076-1037.2s",				0x10000, 0x07addba6, 5 | BRF_GRA },           // 29 MOB Tiles
	{ "136076-1041.2r",				0x10000, 0x1e9c29e4, 5 | BRF_GRA },           // 30
	{ "136076-1045.34s",			0x10000, 0xe7235876, 5 | BRF_GRA },           // 31
	{ "136076-1049.34r",			0x10000, 0xa6eb8265, 5 | BRF_GRA },           // 32
	{ "136076-1038.6s",				0x10000, 0x2ea543f9, 5 | BRF_GRA },           // 33
	{ "136076-1042.6r",				0x10000, 0xefabdc2b, 5 | BRF_GRA },           // 34
	{ "136076-1046.7s",				0x10000, 0x6692151f, 5 | BRF_GRA },           // 35
	{ "136076-1050.7r",				0x10000, 0xad7bb5f3, 5 | BRF_GRA },           // 36
	{ "136076-1039.11s",			0x10000, 0xcb563a40, 5 | BRF_GRA },           // 37
	{ "136076-1043.11r",			0x10000, 0xb7565eee, 5 | BRF_GRA },           // 38
	{ "136076-1047.12s",			0x10000, 0x60877136, 5 | BRF_GRA },           // 39
	{ "136076-1051.12r",			0x10000, 0xd4715ff0, 5 | BRF_GRA },           // 40
	{ "136076-1040.15s",			0x10000, 0x6e910fc2, 5 | BRF_GRA },           // 41
	{ "136076-1044.15r",			0x10000, 0xff67a17a, 5 | BRF_GRA },           // 42
	{ "136076-1048.16s",			0x10000, 0x200d45b3, 5 | BRF_GRA },           // 43
	{ "136076-1052.16r",			0x10000, 0x74711ef1, 5 | BRF_GRA },           // 44

	{ "136076-1020.4m",				0x10000, 0x65470354, 6 | BRF_GRA },           // 45 Alpha Tiles

	{ "136076-1016.7k",				0x10000, 0xc10bdf73, 7 | BRF_GRA },           // 46 Samples
	{ "136076-1017.7j",				0x10000, 0x4e5e25e8, 7 | BRF_GRA },           // 47
	{ "136076-1018.7e",				0x10000, 0xec81895d, 7 | BRF_GRA },           // 48
	{ "136076-1019.7d",				0x10000, 0xa4009037, 7 | BRF_GRA },           // 49

	{ "gal16v8a-136076-1070.12e",	0x00117, 0x0e96e51e, 8 | BRF_OPT },           // 50 PLDs
	{ "gal16v8a-136076-1071.13j",	0x00117, 0xad7beb34, 8 | BRF_OPT },           // 51
	{ "gal16v8a-136076-1072.5n",	0x00117, 0x2827620a, 8 | BRF_OPT },           // 52
	{ "gal16v8a-136076-1073.2h",	0x00117, 0xa2fe4402, 8 | BRF_OPT },           // 53
	{ "gal16v8a-136076-2074.3l",	0x00117, 0x0e8bc5d7, 8 | BRF_OPT },           // 54
	{ "gal20v8a-136076-1075.2l",	0x00157, 0xe75e1e86, 8 | BRF_OPT },           // 55
	{ "gal20v8a-136076-1076.1l",	0x00157, 0xf36f873a, 8 | BRF_OPT },           // 56
};

STD_ROM_PICK(thunderja)
STD_ROM_FN(thunderja)

struct BurnDriver BurnDrvThunderja = {
	"thunderja", "thunderj", NULL, NULL, "1990",
	"ThunderJaws (rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, thunderjaRomInfo, thunderjaRomName, NULL, NULL, NULL, NULL, ThunderjInputInfo, ThunderjDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};
