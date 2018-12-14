// FB Alpha Atari Batman driver module
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
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *DrvExtraRAM;
static UINT8 *DrvMobRAM;
static UINT8 *DrvAlphaRAM;
static UINT8 *Drv68KRAM;
static UINT16 *DrvEOFData;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 sound_cpu_halt;
static INT32 alpha_tile_bank;
static INT32 scanline_int_state;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT16 DrvInputs[3];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static void draw_scanline(INT32 line); // partials

static struct BurnInputInfo BatmanInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 15,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 14,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 13,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 2,	"tilt"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Batman)

static struct BurnDIPInfo BatmanDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x40, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x0b, 0x01, 0x40, 0x40, "Off"			},
	{0x0b, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Batman)


static void update_interrupts()
{
	INT32 newstate = 0;
	if (scanline_int_state) newstate |= 4;
	if (atarijsa_int_state)	newstate |= 6;

	if (newstate)
		SekSetIRQLine(newstate, CPU_IRQSTATUS_ACK);
	else
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
}

static void latch_write(UINT16 data)
{
	sound_cpu_halt = ~data & 0x10;

	if (sound_cpu_halt)
	{
		M6502Reset();
	}

	alpha_tile_bank = (data >> 12) & 7;
}

static void __fastcall batman_main_write_word(UINT32 address, UINT16 data)
{
	if (address & 0xc00000) {
		SekWriteWord(address & 0x3fffff, data);
		return;
	}

	if ((address & 0xefe000) == 0x2f6000) {
		*((UINT16*)(DrvMobRAM + (address & 0x1ffe))) = data;
		AtariMoWrite(0, (address / 2) & 0xfff, data);
		return;
	}

	switch (address)
	{
		case 0x260040:
			AtariJSAWrite(data);
		return;

		case 0x260050:
			latch_write(data);
		return;

		case 0x260060:
			AtariEEPROMUnlockWrite();
		return;

		case 0x2a0000:
			BurnWatchdogWrite();
		return;
	}
	bprintf(0, _T("mww %X %x\n"), address, data);
}

static void __fastcall batman_main_write_byte(UINT32 address, UINT8 data)
{
	if (address & 0xc00000) {
		SekWriteByte(address & 0x3fffff, data);
		return;
	}

	if ((address & 0xefe000) == 0x2f6000 ) {
		DrvMobRAM[(address & 0x1fff)^1] = data;
		AtariMoWrite(0, (address / 2) & 0xfff, *((UINT16*)(DrvMobRAM + (address & 0x1ffe))));
		return;
	}

	switch (address)
	{
		case 0x260040:
		case 0x260041:
			AtariJSAWrite(data);
		return;

		case 0x260050:
		case 0x260051:
			latch_write(data);
		return;

		case 0x260060:
		case 0x260061:
			AtariEEPROMUnlockWrite();
		return;

		case 0x2a0000:
		case 0x2a0001:
			BurnWatchdogWrite();
		return;
	}

	bprintf (0, _T("MWB: %5.5x, %2.2x!!!!\n"), address, data);
}

static inline UINT16 special_port_read()
{
	UINT16 ret = (0xffff & ~0x40) | (DrvDips[0] & 0x40);

	if (vblank) ret ^= 0x80;
	if (atarigen_cpu_to_sound_ready) ret ^= 0x20;
	if (atarigen_sound_to_cpu_ready) ret ^= 0x10;

	return ret;
}

static UINT16 __fastcall batman_main_read_word(UINT32 address)
{
	if (address & 0xc00000) {
		return SekReadWord(address & 0x3fffff);
	}

	switch (address)
	{
		case 0x260000:
			return DrvInputs[0];

		case 0x260002:
			return 0xffff;

		case 0x260010:
			return special_port_read();

		case 0x260030:
			return AtariJSARead();
	}

	return 0;
}

static UINT8 __fastcall batman_main_read_byte(UINT32 address)
{
	if (address & 0xc00000) {
		return SekReadByte(address & 0x3fffff);
	}

	switch (address)
	{
		case 0x260000:
		case 0x260001:
			return DrvInputs[0] >> ((~address & 1) * 8);

		case 0x260002:
		case 0x260003:
			return 0xff;

		case 0x260010:
		case 0x260011:
			return special_port_read() >> ((~address & 1) * 8);

		case 0x260030:
		case 0x260031:
			return AtariJSARead() >> ((~address & 1) * 8);
	}

	return 0;
}

static INT32 lastline = 0;

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

static tilemap_callback( alpha )
{
	UINT16 data = *((UINT16*)(DrvAlphaRAM + offs * 2));
	int code = (data & 0x3ff);
	if (data & 0x400) code += alpha_tile_bank * 0x400;

	TILE_SET_INFO(3, code, (data >> 11), (data & 0x8000) ? TILE_OPAQUE : 0);
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
	AtariJSAReset();
	AtariVADReset();

	sound_cpu_halt = 0;
	alpha_tile_bank = 0;
	scanline_int_state = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x0c0000;
	DrvM6502ROM			= Next; Next += 0x010000;

	DrvGfxROM0			= Next; Next += 0x080000;
	DrvGfxROM1			= Next; Next += 0x200000;
	DrvGfxROM2			= Next; Next += 0x200000;

	DrvSndROM			= Next; Next += 0x080000;

	DrvPalette			= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam				= Next;

	DrvMobRAM			= Next; Next += 0x002000;
	DrvAlphaRAM			= Next; Next += 0x001000;
	Drv68KRAM			= Next; Next += 0x010000;
	DrvExtraRAM			= Next; Next += 0x008000*2;

	DrvEOFData			= (UINT16*)(DrvAlphaRAM + 0xf00);
	atarimo_0_slipram 	= (UINT16*)(DrvAlphaRAM + 0xf80);

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { STEP4(0, (0x40000*8)) };
	INT32 XOffs0[8] = { STEP8(0,1) };
	INT32 YOffs0[8] = { STEP8(0,8) };

	INT32 Plane1[2] = { 0, 4 };
	INT32 XOffs1[8] = { STEP4(0,1), STEP4(8,1) };
	INT32 YOffs1[8] = { STEP8(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x020000; i++) tmp[i] = DrvGfxROM0[i];

	GfxDecode(0x2000, 2, 8, 8, Plane1, XOffs1, YOffs1, 0x080, tmp, DrvGfxROM0);

	for (INT32 i = 0; i < 0x100000; i++) tmp[i] = DrvGfxROM1[i] ^ 0xff;

	GfxDecode(0x8000, 4, 8, 8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM1);

	for (INT32 i = 0; i < 0x100000; i++) tmp[i] = DrvGfxROM2[i] ^ 0xff;

	GfxDecode(0x8000, 4, 8, 8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM2);

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
		if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x080001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x080000, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6502ROM+ 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0e0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0c0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0e0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x060000, k++, 1)) return 1;

		if (BurnLoadRom(Drv68KRAM  + 0x000000, k++, 1)) return 1; // temp memory

		DrvGfxDecode();
	}

	GenericTilesInit();
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, alpha_map_callback, 8, 8, 64, 32);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapSetGfx(0, DrvGfxROM2, 4, 8, 8, 0x200000, 0x300, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM2, 4, 8, 8, 0x200000, 0x200, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM1, 4, 8, 8, 0x200000, 0x100, 0x0f);
	GenericTilemapSetGfx(3, DrvGfxROM0, 2, 8, 8, 0x080000, 0x000, 0x0f);

	AtariVADInit(0, 1, 0, scanline_timer, palette_write);
	AtariVADSetPartialCB(draw_scanline);
	AtariMoInit(0, &modesc);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0bffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x100000, 0x10ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x110000, 0x11ffff, MAP_RAM); // mirror

	SekMapMemory(DrvMobRAM,		0x2f6000, 0x2f7fff, MAP_ROM);
	SekMapMemory(DrvAlphaRAM,	0x2f8000, 0x2f8fff, MAP_RAM);
	SekMapMemory(DrvExtraRAM,	0x2f9000, 0x2fffff, MAP_RAM); // ??

	SekMapMemory(DrvMobRAM,		0x3f6000, 0x3f7fff, MAP_ROM); // mirror above
	SekMapMemory(DrvAlphaRAM,	0x3f8000, 0x3f8fff, MAP_RAM);
	SekMapMemory(DrvExtraRAM,	0x3f9000, 0x3fffff, MAP_RAM);
	SekSetWriteWordHandler(0,	batman_main_write_word);
	SekSetWriteByteHandler(0,	batman_main_write_byte);
	SekSetReadWordHandler(0,	batman_main_read_word);
	SekSetReadByteHandler(0,	batman_main_read_byte);

	AtariVADMap(0x2e0000, 0x2f5fff, 0);
	AtariVADMap(0x3e0000, 0x3f5fff, 0);

	AtariEEPROMInit(0x1000);
	for (INT32 i = 0; i < 0x20000; i+=0x1000) {
		AtariEEPROMInstallMap(1, 0x120000 + i, 0x120fff + i);
	}
	AtariEEPROMLoad(Drv68KROM); // temp memory

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

	AtariVADExit();
	AtariJSAExit();
	AtariMoExit();
	AtariEEPROMExit();

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

				if ((mopriority & 4) == 0)
				{
					if (pri[x] & 0x80)
					{
						INT32 pfpriority = (pri[x] >> 2) & 3;

						if (pfpriority != 3)
						{
							if ((pf[x] & 0x08) || (mopriority >= pfpriority))
								pf[x] = mo[x] & 0x7ff;
						}
					}
					else
					{
						if ((pri[x] & 3) != 3)
							pf[x] = mo[x] & 0x7ff;
					}
				}
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
				if ((mo[x] & 0x4002) == 0x4002)
				{
					AtariMoApplyStain(pf, mo, x);
				}

				mo[x] = 0xffff;
			}
		}
	}
}

static void draw_scanline(INT32 line)
{
	if (!pBurnDraw) return;

	GenericTilesSetClip(-1, -1, lastline, line + 1);

	if (nSpriteEnable & 4) AtariMoRender(0);

	AtariVADDraw(pTransDraw, 1); // nBurnLayer 1,2 check is internal

	if (nSpriteEnable & 1) copy_sprites_step1();

	if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 0);

	if (nSpriteEnable & 2) copy_sprites_step2();

	GenericTilesClearClip();

	lastline = line + 1;
}

static void DrvDrawBegin()
{
	if (DrvRecalc) {
		AtariVADRecalcPalette();
		DrvRecalc = 0;
	}

	if (pBurnDraw)
		BurnTransferClear();

	lastline = 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) { // bpp changed, let's not present a blank screen
		DrvDrawBegin();
		draw_scanline(239); // 0 - 240
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	SekNewFrame();
	M6502NewFrame();

	{
		DrvInputs[0] = 0xffff;
		//DrvInputs[1] = 0xffbf | (DrvDips[0] & 0x40);
		DrvInputs[2] = 0x0040;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy2[i] & 1) << i;
		}

		atarijsa_input_port = DrvInputs[2];
		atarijsa_test_mask = 0x40;
		atarijsa_test_port = DrvDips[0] & atarijsa_test_mask;
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { (INT32)(14318180 / 59.92), (INT32)(1789773 / 59.92) };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	M6502Open(0);

	vblank = 0;

	DrvDrawBegin();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		atarivad_scanline = i;

		if (i == 0) AtariVADEOFUpdate(DrvEOFData);

		if (atarivad_scanline_timer_enabled) {
			if (atarivad_scanline_timer == atarivad_scanline) {
				scanline_timer(CPU_IRQSTATUS_ACK);
			}
		}

		nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);

		if (sound_cpu_halt == 0) {
			nCyclesDone[1] += M6502Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		} else {
			nCyclesDone[1] += M6502Idle(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		}

		if (i <= 240) {
			AtariVADTileRowUpdate(i, (UINT16*)DrvAlphaRAM);
		}

		if (i == 239) {
			draw_scanline(i);
			vblank = 1;

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		AtariJSAInterruptUpdate(nInterleave);

		if (pBurnSoundOut && i&1) {
			INT32 nSegment = nBurnSoundLen / (nInterleave / 2);

			AtariJSAUpdate(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);

			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			AtariJSAUpdate(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	SekClose();
	M6502Close();

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

		SCAN_VAR(sound_cpu_halt);
		SCAN_VAR(alpha_tile_bank);
		SCAN_VAR(scanline_int_state);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Batman

static struct BurnRomInfo batmanRomDesc[] = {
	{ "136085-2030.10r",			0x20000, 0x7cf4e5bf, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "136085-2031.7r",				0x20000, 0x7d7f3fc4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136085-2032.91r",			0x20000, 0xd80afb20, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136085-2033.6r",				0x20000, 0x97efa2b8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136085-2034.9r",				0x20000, 0x05388c62, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136085-2035.5r",				0x20000, 0xe77c92dd, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136085-1040.12c",			0x10000, 0x080db83c, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code

	{ "136085-2009.10m",			0x20000, 0xa82d4923, 3 | BRF_GRA },           //  7 Characters

	{ "136085-1010.13r",			0x20000, 0x466e1365, 4 | BRF_GRA },           //  8 VAD Tiles
	{ "136085-1014.14r",			0x20000, 0xef53475a, 4 | BRF_GRA },           //  9
	{ "136085-1011.13m",			0x20000, 0x8cda5efc, 4 | BRF_GRA },           // 10
	{ "136085-1015.14m",			0x20000, 0x043e7f8b, 4 | BRF_GRA },           // 11
	{ "136085-1012.13f",			0x20000, 0xb017f2c3, 4 | BRF_GRA },           // 12
	{ "136085-1016.14f",			0x20000, 0x70aa2360, 4 | BRF_GRA },           // 13
	{ "136085-1013.13c",			0x20000, 0x68b64975, 4 | BRF_GRA },           // 14
	{ "136085-1017.14c",			0x20000, 0xe4af157b, 4 | BRF_GRA },           // 15

	{ "136085-1018.15r",			0x20000, 0x4c14f1e5, 5 | BRF_GRA },           // 16 Sprites
	{ "136085-1022.16r",			0x20000, 0x7476a15d, 5 | BRF_GRA },           // 17
	{ "136085-1019.15m",			0x20000, 0x2046d9ec, 5 | BRF_GRA },           // 18
	{ "136085-1023.16m",			0x20000, 0x75cac686, 5 | BRF_GRA },           // 19
	{ "136085-1020.15f",			0x20000, 0xcc4f4b94, 5 | BRF_GRA },           // 20
	{ "136085-1024.16f",			0x20000, 0xd60d35e0, 5 | BRF_GRA },           // 21
	{ "136085-1021.15c",			0x20000, 0x9c8ef9ba, 5 | BRF_GRA },           // 22
	{ "136085-1025.16c",			0x20000, 0x5d30bcd1, 5 | BRF_GRA },           // 23

	{ "136085-1041.19e",			0x20000, 0xd97d5dbb, 6 | BRF_GRA },           // 24 Samples
	{ "136085-1042.17e",			0x20000, 0x8c496986, 6 | BRF_GRA },           // 25
	{ "136085-1043.15e",			0x20000, 0x51812d3b, 6 | BRF_GRA },           // 26
	{ "136085-1044.12e",			0x20000, 0x5e2d7f31, 6 | BRF_GRA },           // 27
	
	{ "batman-eeprom.bin",			0x00800, 0xc859b535, 7 | BRF_PRG | BRF_ESS }, // 28 NVRAM Data

	{ "gal16v8a-136085-1001.m9",	0x00117, 0x45dfc0cf, 8 | BRF_OPT },           // 29 PLDs
	{ "gal16v8a-136085-1002.l9",	0x00117, 0x35c221ae, 8 | BRF_OPT },           // 30
	{ "gal20v8a-136085-1003.c9",	0x00157, 0xcbfb2b4f, 8 | BRF_OPT },           // 31
	{ "gal20v8a-136085-1004.b9",	0x00157, 0x227cd23a, 8 | BRF_OPT },           // 32
	{ "gal16v8a-136085-1005.d11",	0x00117, 0xa2fe4402, 8 | BRF_OPT },           // 33
	{ "gal16v8a-136085-1006.d9",	0x00117, 0x87d1c2dc, 8 | BRF_OPT },           // 34
	{ "gal16v8a-136085-1038.c17",	0x00117, 0x0a591138, 8 | BRF_OPT },           // 35
	{ "gal16v8a-136085-1039.c19",	0x00117, 0x47565e09, 8 | BRF_OPT },           // 36
};

STD_ROM_PICK(batman)
STD_ROM_FN(batman)

struct BurnDriver BurnDrvBatman = {
	"batman", NULL, NULL, NULL, "1991",
	"Batman\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, batmanRomInfo, batmanRomName, NULL, NULL, NULL, NULL, BatmanInputInfo, BatmanDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};
