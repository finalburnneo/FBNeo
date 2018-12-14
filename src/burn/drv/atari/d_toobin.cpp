// FB Alpha Atari Toobin' driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "atariic.h"
#include "atarimo.h"
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
static UINT8 *DrvPalRAM;
static UINT8 *DrvPfRAM;
static UINT8 *DrvMobRAM;
static UINT8 *DrvAlphaRAM;
static UINT8 *Drv68KRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 palette_brightness;
static INT32 playfield_scrollx;
static INT32 playfield_scrolly;

static INT32 scanline_interrupt;
static INT32 scanline_int_state;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT16 DrvInputs[2];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static UINT32 linecycles;
static UINT32 lastline;
static UINT32 scanline;

static void partial_update();

static struct BurnInputInfo ToobinInputList[] = {
	{"Coin 1",						BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"Coin 2",						BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"Coin 3",						BIT_DIGITAL,	DrvJoy2 + 2,	"p3 coin"	},

	{"P1 Throw",					BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 1"	},
	{"P1 Right.Paddle Forward",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},
	{"P1 Left Paddle Forward",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},
	{"P1 Left Paddle Backward",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 4"	},
	{"P1 Right Paddle Backward",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 5"	},

	{"P2 Throw",					BIT_DIGITAL,	DrvJoy1 + 9,	"p2 fire 1"	},
	{"P2 Right.Paddle Forward",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 fire 2"	},
	{"P2 Left Paddle Forward",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 3"	},
	{"P2 Left Paddle Backward",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 fire 4"	},
	{"P2 Right Paddle Backward",	BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 5"	},

	{"Reset",						BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",						BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Toobin)

static struct BurnDIPInfo ToobinDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x10, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0e, 0x01, 0x10, 0x10, "Off"				},
	{0x0e, 0x01, 0x10, 0x00, "On"				},
};

STDDIPINFO(Toobin)

static void update_interrupts()
{
	INT32 newstate = 0;
	if (scanline_int_state)	newstate |= 1;
	if (atarijsa_int_state)	newstate |= 2;

	if (newstate)
		SekSetIRQLine(newstate, CPU_IRQSTATUS_ACK);
	else
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
}

static void __fastcall toobin_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff800) == 0xc09800) {
		*((UINT16*)(DrvMobRAM + (address & 0x7fe))) = data;
		AtariMoWrite(0, (address / 2) & 0x3ff, data);
		return;
	}

	switch (address)
	{
		case 0xff8000:
			BurnWatchdogWrite();
		return;

		case 0xff8100:
			M6502Run(((double)SekTotalCycles()/4.47) - M6502TotalCycles());
			AtariJSAWrite(data);
		return;

		case 0xff8300:
			palette_brightness = ~data & 0x1f;
		return;

		case 0xff8340:
			scanline_interrupt = data & 0x1ff;
		return;

		case 0xff8380:
			{
				UINT8 old = atarimo_0_slipram[0];
				atarimo_0_slipram[0] = data;
				if (old != data)
					partial_update();
				return;
			}

		case 0xff83c0:
			scanline_int_state = 0;
			update_interrupts();
		return;

		case 0xff8400:
			AtariJSAResetWrite(data);
		return;

		case 0xff8500:
			AtariEEPROMUnlockWrite();
		return;

		case 0xff8600:
			partial_update();
			// iq_132 - partial update!!
			playfield_scrollx = (data >> 6) & 0x3ff;
		return;

		case 0xff8700:
			partial_update();
			// iq_132 - partial update!!
			playfield_scrolly = (data >> 6) & 0x1ff;
		return;
	}

	if (address != 0xff8000) bprintf (0, _T("MWW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall toobin_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff800) == 0xc09800) {
		DrvMobRAM[(address & 0x7ff)^1] = data;
		AtariMoWrite(0, (address / 2) & 0x3ff, *((UINT16*)(DrvMobRAM + (address & 0x7fe))));
		return;
	}

	switch (address)
	{
		case 0xff8301:
			palette_brightness = ~data & 0x1f;
		return;
	}
}

static UINT16 special_read()
{
	UINT16 ret = 0xefff | ((DrvDips[0] & 0x10) << 8);

	if (SekTotalCycles() - linecycles > 256) ret ^= 0x2000;
	if (vblank) ret ^= 0x4000;
	if (atarigen_cpu_to_sound_ready) ret ^= 0x8000;

	return ret;
}

static UINT16 __fastcall toobin_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xff6000:
			return 0; // nop

		case 0xff8800:
			return DrvInputs[0];

		case 0xff9000:
			return special_read();

		case 0xff9800:
			return AtariJSARead();
	}

	bprintf (0, _T("RW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall toobin_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xff6000:
		case 0xff6001:
			return 0; // nop

		case 0xff8800:
		case 0xff8801:
			return DrvInputs[0] >> ((~address & 1) * 8);

		case 0xff9000:
		case 0xff9001:
			return special_read() >> ((~address & 1) * 8);

		case 0xff9800:
		case 0xff9801:
			return AtariJSARead() >> ((~address & 1) * 8);
	}

	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static tilemap_callback( alpha )
{
	UINT16 data = *((UINT16*)(DrvAlphaRAM + (offs * 2)));

	TILE_SET_INFO(2, data, data >> 12, TILE_FLIPYX((data >> 10) & 1));
}

static tilemap_callback( bg )
{
	UINT16 data0 = *((UINT16*)(DrvPfRAM + (offs * 4 + 0)));
	UINT16 data1 = *((UINT16*)(DrvPfRAM + (offs * 4 + 2)));

	TILE_SET_INFO(0, data1, data0, TILE_FLIPYX(data1 >> 14) | TILE_GROUP((data0 >> 4) & 3));
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

	palette_brightness = 0;
	playfield_scrollx = 0;
	playfield_scrolly = 0;
	scanline_interrupt = 0;
	scanline_int_state = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x0c0000;
	DrvM6502ROM			= Next; Next += 0x010000;

	DrvGfxROM0			= Next; Next += 0x100000;
	DrvGfxROM1			= Next; Next += 0x400000;
	DrvGfxROM2			= Next; Next += 0x010000;

	DrvPalette			= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	AllRam				= Next;

	DrvPalRAM			= Next; Next += 0x000800;
	DrvPfRAM			= Next; Next += 0x008000;
	DrvMobRAM			= Next; Next += 0x000800;
	DrvAlphaRAM			= Next; Next += 0x001800;
	Drv68KRAM			= Next; Next += 0x004000;

	atarimo_0_slipram 	= (UINT16*)Next; Next += 0x000002; // only first entry used

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { 0x40000*8+0, 0x40000*8+4, 0, 4 };
	INT32 Plane1[4] = { 0x100000*8+0, 0x100000*8+4, 0, 4 };
	INT32 Plane2[2] = { 0, 4 };
	INT32 XOffs0[16] = { STEP4(0,1), STEP4(8,1), STEP4(16,1), STEP4(24,1) };
	INT32 YOffs0[8] = { STEP8(0,16) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x080000);

	GfxDecode(0x4000, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane1, XOffs0, YOffs1, 0x200, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x004000);

	GfxDecode(0x0400, 2,  8,  8, Plane2, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM2);

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
		1,					// render in swapped X/Y order?
		0,					// does the neighbor bit affect the next object?
		1024,				// pixels per SLIP entry (0 for no-slip)
		0,					// pixel offset for SLIPs
		0,					// maximum number of links to visit/scanline (0=all)

		0x100,				// base palette entry
		0x100,				// maximum number of colors
		0,					// transparent pen index

		{{ 0,0,0x00ff,0 }},	// mask for the link
		{{ 0 }},			// mask for the graphics bank
		{{ 0,0x3fff,0,0 }},	// mask for the code index
		{{ 0 }},			// mask for the upper code index
		{{ 0,0,0,0x000f }},	// mask for the color
		{{ 0,0,0,0xffc0 }},	// mask for the X position
		{{ 0x7fc0,0,0,0 }},	// mask for the Y position
		{{ 0x0007,0,0,0 }},	// mask for the width, in tiles*/
		{{ 0x0038,0,0,0 }},	// mask for the height, in tiles
		{{ 0,0x4000,0,0 }},	// mask for the horizontal flip
		{{ 0,0x8000,0,0 }},	// mask for the vertical flip
		{{ 0 }},			// mask for the priority
		{{ 0 }},			// mask for the neighbor
		{{ 0x8000,0,0,0 }},	// mask for absolute coordinates

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
		if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x060001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x060000, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6502ROM+ 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x030000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x050000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x070000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x090000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0b0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x120000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x140000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x160000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x180000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x190000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1b0000, k++, 1)) return 1;
		memcpy (DrvGfxROM1 + 0x0c0000, DrvGfxROM1 + 0x080000, 0x040000);
		memcpy (DrvGfxROM1 + 0x1c0000, DrvGfxROM1 + 0x180000, 0x040000);

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68010);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KROM,			0x080000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvPfRAM,			0xc00000, 0xc07fff, MAP_RAM);
	SekMapMemory(DrvAlphaRAM,		0xc08000, 0xc097ff, MAP_RAM);
	SekMapMemory(DrvMobRAM,			0xc09800, 0xc09fff, MAP_ROM); // handler
	SekMapMemory(DrvPalRAM,			0xc10000, 0xc107ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xffc000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,		toobin_main_write_word);
	SekSetWriteByteHandler(0,		toobin_main_write_byte);
	SekSetReadWordHandler(0,		toobin_main_read_word);
	SekSetReadByteHandler(0,		toobin_main_read_byte);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(1, 		0xffa000, 0xffafff);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	AtariJSAInit(DrvM6502ROM, &update_interrupts, NULL, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback,    8, 8, 128, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, alpha_map_callback, 8, 8,  64, 48);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4,  8,  8, 0x100000, 0x000, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x400000, 0x100, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM2, 2,  8,  8, 0x010000, 0x200, 0x3f);
	GenericTilemapSetTransparent(1, 0);

	AtariMoInit(0, &modesc);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();

	AtariJSAExit();
	AtariMoExit();
	AtariEEPROMExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 offs = 0; offs < 0x800/2; offs++)
	{
		INT32 r = ((p[offs] >> 10) & 0x1f);
		INT32 g = ((p[offs] >>  5) & 0x1f);
		INT32 b = ((p[offs] >>  0) & 0x1f);

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		if ((p[offs] & 0x8000) == 0) {
			r = (r * palette_brightness) / 31;
			g = (g * palette_brightness) / 31;
			b = (b * palette_brightness) / 31;
		}

		DrvPalette[offs] = BurnHighCol(r,g,b,0);
	}
}

static void copy_sprites()
{
	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);
		UINT8 *pr = BurnBitmapGetPrimapPosition(0, 0, y);

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			if (mo[x] != 0xffff)
			{
				if (pr[x] == 0 || (pf[x] & 8) == 0)
					pf[x] = mo[x] & 0x1ff;

				mo[x] = 0xffff;
			}
		}
	}
}

static void partial_update()
{
	if (scanline > 384 || scanline == lastline) return;
	GenericTilesSetClip(0, nScreenWidth, lastline, scanline);
	AtariMoRender(0);
	GenericTilesClearClip();
	lastline = scanline;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force!!
	}

	scanline = 384;
	partial_update();

	GenericTilemapSetScrollX(0, playfield_scrollx);
	GenericTilemapSetScrollY(0, playfield_scrolly);
	atarimo_set_xscroll(0, playfield_scrollx);
	atarimo_set_yscroll(0, playfield_scrolly);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0); // opaque!!
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 1 | TMAP_SET_GROUP(1));
	if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, 2 | TMAP_SET_GROUP(2));
	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 3 | TMAP_SET_GROUP(3));

	if (nSpriteEnable & 1) copy_sprites();

	if (nSpriteEnable & 2) GenericTilemapDraw(1, pTransDraw, 4);

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
		DrvInputs[1] = 0x0040;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		atarijsa_input_port = DrvInputs[1];
		atarijsa_test_mask = 0x10;
		atarijsa_test_port = DrvDips[0] & 0x10;
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 416;
	INT32 nCyclesTotal[2] = { (INT32)(8000000 / 60.00), (INT32)(1789773 / 60.00) };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	M6502Open(0);

	vblank = 0;
	lastline = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		if (scanline_interrupt == i) {
			partial_update();
			scanline_int_state = 1;
			update_interrupts();
		}

		linecycles = SekTotalCycles();

		nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		nCyclesDone[1] += M6502Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);

		if (i == 384) {
			vblank = 1;

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		AtariJSAInterruptUpdate(nInterleave);

		if (pBurnSoundOut && (i%4)==3) {
			INT32 nSegment = nBurnSoundLen / (nInterleave / 4);
			AtariJSAUpdate(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment >= 0) {
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
		AtariMoScan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		SCAN_VAR(palette_brightness);
		SCAN_VAR(playfield_scrollx);
		SCAN_VAR(playfield_scrolly);
		SCAN_VAR(scanline_interrupt);
		SCAN_VAR(scanline_int_state);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Toobin' (rev 3)

static struct BurnRomInfo toobinRomDesc[] = {
	{ "3133-1j.061",	0x10000, 0x79a92d02, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "3137-1f.061",	0x10000, 0xe389ef60, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3134-2j.061",	0x10000, 0x3dbe9a48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3138-2f.061",	0x10000, 0xa17fb16c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "3135-4j.061",	0x10000, 0xdc90b45c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "3139-4f.061",	0x10000, 0x6f8a719a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1136-5j.061",	0x10000, 0x5ae3eeac, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1140-5f.061",	0x10000, 0xdacbbd94, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "1141-2k.061",	0x10000, 0xc0dcce1a, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code

	{ "1101-1a.061",	0x10000, 0x02696f15, 3 | BRF_GRA },           //  9 Background Tiles
	{ "1102-2a.061",	0x10000, 0x4bed4262, 3 | BRF_GRA },           // 10
	{ "1103-4a.061",	0x10000, 0xe62b037f, 3 | BRF_GRA },           // 11
	{ "1104-5a.061",	0x10000, 0xfa05aee6, 3 | BRF_GRA },           // 12
	{ "1105-1b.061",	0x10000, 0xab1c5578, 3 | BRF_GRA },           // 13
	{ "1106-2b.061",	0x10000, 0x4020468e, 3 | BRF_GRA },           // 14
	{ "1107-4b.061",	0x10000, 0xfe6f6aed, 3 | BRF_GRA },           // 15
	{ "1108-5b.061",	0x10000, 0x26fe71e1, 3 | BRF_GRA },           // 16

	{ "1143-10a.061",	0x20000, 0x211c1049, 4 | BRF_GRA },           // 17 Sprites
	{ "1144-13a.061",	0x20000, 0xef62ed2c, 4 | BRF_GRA },           // 18
	{ "1145-16a.061",	0x20000, 0x067ecb8a, 4 | BRF_GRA },           // 19
	{ "1146-18a.061",	0x20000, 0xfea6bc92, 4 | BRF_GRA },           // 20
	{ "1125-21a.061",	0x10000, 0xc37f24ac, 4 | BRF_GRA },           // 21
	{ "1126-23a.061",	0x10000, 0x015257f0, 4 | BRF_GRA },           // 22
	{ "1127-24a.061",	0x10000, 0xd05417cb, 4 | BRF_GRA },           // 23
	{ "1128-25a.061",	0x10000, 0xfba3e203, 4 | BRF_GRA },           // 24
	{ "1147-10b.061",	0x20000, 0xca4308cf, 4 | BRF_GRA },           // 25
	{ "1148-13b.061",	0x20000, 0x23ddd45c, 4 | BRF_GRA },           // 26
	{ "1149-16b.061",	0x20000, 0xd77cd1d0, 4 | BRF_GRA },           // 27
	{ "1150-18b.061",	0x20000, 0xa37157b8, 4 | BRF_GRA },           // 28
	{ "1129-21b.061",	0x10000, 0x294aaa02, 4 | BRF_GRA },           // 29
	{ "1130-23b.061",	0x10000, 0xdd610817, 4 | BRF_GRA },           // 30
	{ "1131-24b.061",	0x10000, 0xe8e2f919, 4 | BRF_GRA },           // 31
	{ "1132-25b.061",	0x10000, 0xc79f8ffc, 4 | BRF_GRA },           // 32

	{ "1142-20h.061",	0x04000, 0xa6ab551f, 5 | BRF_GRA },           // 33 Characters
};

STD_ROM_PICK(toobin)
STD_ROM_FN(toobin)

struct BurnDriver BurnDrvToobin = {
	"toobin", NULL, NULL, NULL, "1988",
	"Toobin' (rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, toobinRomInfo, toobinRomName, NULL, NULL, NULL, NULL, ToobinInputInfo, ToobinDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	384, 512, 3, 4
};


// Toobin' (Europe, rev 3)

static struct BurnRomInfo toobineRomDesc[] = {
	{ "3733-1j.061",	0x10000, 0x286c7fad, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "3737-1f.061",	0x10000, 0x965c161d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3134-2j.061",	0x10000, 0x3dbe9a48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3138-2f.061",	0x10000, 0xa17fb16c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "3135-4j.061",	0x10000, 0xdc90b45c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "3139-4f.061",	0x10000, 0x6f8a719a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1136-5j.061",	0x10000, 0x5ae3eeac, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1140-5f.061",	0x10000, 0xdacbbd94, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "1141-2k.061",	0x10000, 0xc0dcce1a, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code

	{ "1101-1a.061",	0x10000, 0x02696f15, 3 | BRF_GRA },           //  9 Background Tiles
	{ "1102-2a.061",	0x10000, 0x4bed4262, 3 | BRF_GRA },           // 10
	{ "1103-4a.061",	0x10000, 0xe62b037f, 3 | BRF_GRA },           // 11
	{ "1104-5a.061",	0x10000, 0xfa05aee6, 3 | BRF_GRA },           // 12
	{ "1105-1b.061",	0x10000, 0xab1c5578, 3 | BRF_GRA },           // 13
	{ "1106-2b.061",	0x10000, 0x4020468e, 3 | BRF_GRA },           // 14
	{ "1107-4b.061",	0x10000, 0xfe6f6aed, 3 | BRF_GRA },           // 15
	{ "1108-5b.061",	0x10000, 0x26fe71e1, 3 | BRF_GRA },           // 16

	{ "1143-10a.061",	0x20000, 0x211c1049, 4 | BRF_GRA },           // 17 Sprites
	{ "1144-13a.061",	0x20000, 0xef62ed2c, 4 | BRF_GRA },           // 18
	{ "1145-16a.061",	0x20000, 0x067ecb8a, 4 | BRF_GRA },           // 19
	{ "1146-18a.061",	0x20000, 0xfea6bc92, 4 | BRF_GRA },           // 20
	{ "1125-21a.061",	0x10000, 0xc37f24ac, 4 | BRF_GRA },           // 21
	{ "1126-23a.061",	0x10000, 0x015257f0, 4 | BRF_GRA },           // 22
	{ "1127-24a.061",	0x10000, 0xd05417cb, 4 | BRF_GRA },           // 23
	{ "1128-25a.061",	0x10000, 0xfba3e203, 4 | BRF_GRA },           // 24
	{ "1147-10b.061",	0x20000, 0xca4308cf, 4 | BRF_GRA },           // 25
	{ "1148-13b.061",	0x20000, 0x23ddd45c, 4 | BRF_GRA },           // 26
	{ "1149-16b.061",	0x20000, 0xd77cd1d0, 4 | BRF_GRA },           // 27
	{ "1150-18b.061",	0x20000, 0xa37157b8, 4 | BRF_GRA },           // 28
	{ "1129-21b.061",	0x10000, 0x294aaa02, 4 | BRF_GRA },           // 29
	{ "1130-23b.061",	0x10000, 0xdd610817, 4 | BRF_GRA },           // 30
	{ "1131-24b.061",	0x10000, 0xe8e2f919, 4 | BRF_GRA },           // 31
	{ "1132-25b.061",	0x10000, 0xc79f8ffc, 4 | BRF_GRA },           // 32

	{ "1142-20h.061",	0x04000, 0xa6ab551f, 5 | BRF_GRA },           // 33 Characters
};

STD_ROM_PICK(toobine)
STD_ROM_FN(toobine)

struct BurnDriver BurnDrvToobine = {
	"toobine", "toobin", NULL, NULL, "1988",
	"Toobin' (Europe, rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, toobineRomInfo, toobineRomName, NULL, NULL, NULL, NULL, ToobinInputInfo, ToobinDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	384, 512, 3, 4
};


// Toobin' (German, rev 3)

static struct BurnRomInfo toobingRomDesc[] = {
	{ "3233-1j.061",	0x10000, 0xb04eb760, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "3237-1f.061",	0x10000, 0x4e41a470, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3234-2j.061",	0x10000, 0x8c60f1b4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3238-2f.061",	0x10000, 0xc251b3a2, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "3235-4j.061",	0x10000, 0x1121b5f4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "3239-4f.061",	0x10000, 0x385c5a80, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1136-5j.061",	0x10000, 0x5ae3eeac, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1140-5f.061",	0x10000, 0xdacbbd94, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "1141-2k.061",	0x10000, 0xc0dcce1a, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code

	{ "1101-1a.061",	0x10000, 0x02696f15, 3 | BRF_GRA },           //  9 Background Tiles
	{ "1102-2a.061",	0x10000, 0x4bed4262, 3 | BRF_GRA },           // 10
	{ "1103-4a.061",	0x10000, 0xe62b037f, 3 | BRF_GRA },           // 11
	{ "1104-5a.061",	0x10000, 0xfa05aee6, 3 | BRF_GRA },           // 12
	{ "1105-1b.061",	0x10000, 0xab1c5578, 3 | BRF_GRA },           // 13
	{ "1106-2b.061",	0x10000, 0x4020468e, 3 | BRF_GRA },           // 14
	{ "1107-4b.061",	0x10000, 0xfe6f6aed, 3 | BRF_GRA },           // 15
	{ "1108-5b.061",	0x10000, 0x26fe71e1, 3 | BRF_GRA },           // 16

	{ "1143-10a.061",	0x20000, 0x211c1049, 4 | BRF_GRA },           // 17 Sprites
	{ "1144-13a.061",	0x20000, 0xef62ed2c, 4 | BRF_GRA },           // 18
	{ "1145-16a.061",	0x20000, 0x067ecb8a, 4 | BRF_GRA },           // 19
	{ "1146-18a.061",	0x20000, 0xfea6bc92, 4 | BRF_GRA },           // 20
	{ "1125-21a.061",	0x10000, 0xc37f24ac, 4 | BRF_GRA },           // 21
	{ "1126-23a.061",	0x10000, 0x015257f0, 4 | BRF_GRA },           // 22
	{ "1127-24a.061",	0x10000, 0xd05417cb, 4 | BRF_GRA },           // 23
	{ "1128-25a.061",	0x10000, 0xfba3e203, 4 | BRF_GRA },           // 24
	{ "1147-10b.061",	0x20000, 0xca4308cf, 4 | BRF_GRA },           // 25
	{ "1148-13b.061",	0x20000, 0x23ddd45c, 4 | BRF_GRA },           // 26
	{ "1149-16b.061",	0x20000, 0xd77cd1d0, 4 | BRF_GRA },           // 27
	{ "1150-18b.061",	0x20000, 0xa37157b8, 4 | BRF_GRA },           // 28
	{ "1129-21b.061",	0x10000, 0x294aaa02, 4 | BRF_GRA },           // 29
	{ "1130-23b.061",	0x10000, 0xdd610817, 4 | BRF_GRA },           // 30
	{ "1131-24b.061",	0x10000, 0xe8e2f919, 4 | BRF_GRA },           // 31
	{ "1132-25b.061",	0x10000, 0xc79f8ffc, 4 | BRF_GRA },           // 32

	{ "1142-20h.061",	0x04000, 0xa6ab551f, 5 | BRF_GRA },           // 33 Characters
};

STD_ROM_PICK(toobing)
STD_ROM_FN(toobing)

struct BurnDriver BurnDrvToobing = {
	"toobing", "toobin", NULL, NULL, "1988",
	"Toobin' (German, rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, toobingRomInfo, toobingRomName, NULL, NULL, NULL, NULL, ToobinInputInfo, ToobinDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	384, 512, 3, 4
};


// Toobin' (Europe, rev 2)

static struct BurnRomInfo toobin2eRomDesc[] = {
	{ "2733-1j.061",	0x10000, 0xa6334cf7, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "2737-1f.061",	0x10000, 0x9a52dd20, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2134-2j.061",	0x10000, 0x2b8164c8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2138-2f.061",	0x10000, 0xc09cadbd, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "2135-4j.061",	0x10000, 0x90477c4a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "2139-4f.061",	0x10000, 0x47936958, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1136-5j.061",	0x10000, 0x5ae3eeac, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1140-5f.061",	0x10000, 0xdacbbd94, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "1141-2k.061",	0x10000, 0xc0dcce1a, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code

	{ "1101-1a.061",	0x10000, 0x02696f15, 3 | BRF_GRA },           //  9 Background Tiles
	{ "1102-2a.061",	0x10000, 0x4bed4262, 3 | BRF_GRA },           // 10
	{ "1103-4a.061",	0x10000, 0xe62b037f, 3 | BRF_GRA },           // 11
	{ "1104-5a.061",	0x10000, 0xfa05aee6, 3 | BRF_GRA },           // 12
	{ "1105-1b.061",	0x10000, 0xab1c5578, 3 | BRF_GRA },           // 13
	{ "1106-2b.061",	0x10000, 0x4020468e, 3 | BRF_GRA },           // 14
	{ "1107-4b.061",	0x10000, 0xfe6f6aed, 3 | BRF_GRA },           // 15
	{ "1108-5b.061",	0x10000, 0x26fe71e1, 3 | BRF_GRA },           // 16

	{ "1143-10a.061",	0x20000, 0x211c1049, 4 | BRF_GRA },           // 17 Sprites
	{ "1144-13a.061",	0x20000, 0xef62ed2c, 4 | BRF_GRA },           // 18
	{ "1145-16a.061",	0x20000, 0x067ecb8a, 4 | BRF_GRA },           // 19
	{ "1146-18a.061",	0x20000, 0xfea6bc92, 4 | BRF_GRA },           // 20
	{ "1125-21a.061",	0x10000, 0xc37f24ac, 4 | BRF_GRA },           // 21
	{ "1126-23a.061",	0x10000, 0x015257f0, 4 | BRF_GRA },           // 22
	{ "1127-24a.061",	0x10000, 0xd05417cb, 4 | BRF_GRA },           // 23
	{ "1128-25a.061",	0x10000, 0xfba3e203, 4 | BRF_GRA },           // 24
	{ "1147-10b.061",	0x20000, 0xca4308cf, 4 | BRF_GRA },           // 25
	{ "1148-13b.061",	0x20000, 0x23ddd45c, 4 | BRF_GRA },           // 26
	{ "1149-16b.061",	0x20000, 0xd77cd1d0, 4 | BRF_GRA },           // 27
	{ "1150-18b.061",	0x20000, 0xa37157b8, 4 | BRF_GRA },           // 28
	{ "1129-21b.061",	0x10000, 0x294aaa02, 4 | BRF_GRA },           // 29
	{ "1130-23b.061",	0x10000, 0xdd610817, 4 | BRF_GRA },           // 30
	{ "1131-24b.061",	0x10000, 0xe8e2f919, 4 | BRF_GRA },           // 31
	{ "1132-25b.061",	0x10000, 0xc79f8ffc, 4 | BRF_GRA },           // 32

	{ "1142-20h.061",	0x04000, 0xa6ab551f, 5 | BRF_GRA },           // 33 Characters
};

STD_ROM_PICK(toobin2e)
STD_ROM_FN(toobin2e)

struct BurnDriver BurnDrvToobin2e = {
	"toobin2e", "toobin", NULL, NULL, "1988",
	"Toobin' (Europe, rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, toobin2eRomInfo, toobin2eRomName, NULL, NULL, NULL, NULL, ToobinInputInfo, ToobinDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	384, 512, 3, 4
};


// Toobin' (rev 2)

static struct BurnRomInfo toobin2RomDesc[] = {
	{ "2133-1j.061",	0x10000, 0x2c3382e4, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "2137-1f.061",	0x10000, 0x891c74b1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2134-2j.061",	0x10000, 0x2b8164c8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2138-2f.061",	0x10000, 0xc09cadbd, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "2135-4j.061",	0x10000, 0x90477c4a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "2139-4f.061",	0x10000, 0x47936958, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1136-5j.061",	0x10000, 0x5ae3eeac, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1140-5f.061",	0x10000, 0xdacbbd94, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "1141-2k.061",	0x10000, 0xc0dcce1a, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code

	{ "1101-1a.061",	0x10000, 0x02696f15, 3 | BRF_GRA },           //  9 Background Tiles
	{ "1102-2a.061",	0x10000, 0x4bed4262, 3 | BRF_GRA },           // 10
	{ "1103-4a.061",	0x10000, 0xe62b037f, 3 | BRF_GRA },           // 11
	{ "1104-5a.061",	0x10000, 0xfa05aee6, 3 | BRF_GRA },           // 12
	{ "1105-1b.061",	0x10000, 0xab1c5578, 3 | BRF_GRA },           // 13
	{ "1106-2b.061",	0x10000, 0x4020468e, 3 | BRF_GRA },           // 14
	{ "1107-4b.061",	0x10000, 0xfe6f6aed, 3 | BRF_GRA },           // 15
	{ "1108-5b.061",	0x10000, 0x26fe71e1, 3 | BRF_GRA },           // 16

	{ "1143-10a.061",	0x20000, 0x211c1049, 4 | BRF_GRA },           // 17 Sprites
	{ "1144-13a.061",	0x20000, 0xef62ed2c, 4 | BRF_GRA },           // 18
	{ "1145-16a.061",	0x20000, 0x067ecb8a, 4 | BRF_GRA },           // 19
	{ "1146-18a.061",	0x20000, 0xfea6bc92, 4 | BRF_GRA },           // 20
	{ "1125-21a.061",	0x10000, 0xc37f24ac, 4 | BRF_GRA },           // 21
	{ "1126-23a.061",	0x10000, 0x015257f0, 4 | BRF_GRA },           // 22
	{ "1127-24a.061",	0x10000, 0xd05417cb, 4 | BRF_GRA },           // 23
	{ "1128-25a.061",	0x10000, 0xfba3e203, 4 | BRF_GRA },           // 24
	{ "1147-10b.061",	0x20000, 0xca4308cf, 4 | BRF_GRA },           // 25
	{ "1148-13b.061",	0x20000, 0x23ddd45c, 4 | BRF_GRA },           // 26
	{ "1149-16b.061",	0x20000, 0xd77cd1d0, 4 | BRF_GRA },           // 27
	{ "1150-18b.061",	0x20000, 0xa37157b8, 4 | BRF_GRA },           // 28
	{ "1129-21b.061",	0x10000, 0x294aaa02, 4 | BRF_GRA },           // 29
	{ "1130-23b.061",	0x10000, 0xdd610817, 4 | BRF_GRA },           // 30
	{ "1131-24b.061",	0x10000, 0xe8e2f919, 4 | BRF_GRA },           // 31
	{ "1132-25b.061",	0x10000, 0xc79f8ffc, 4 | BRF_GRA },           // 32

	{ "1142-20h.061",	0x04000, 0xa6ab551f, 5 | BRF_GRA },           // 33 Characters
};

STD_ROM_PICK(toobin2)
STD_ROM_FN(toobin2)

struct BurnDriver BurnDrvToobin2 = {
	"toobin2", "toobin", NULL, NULL, "1988",
	"Toobin' (rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, toobin2RomInfo, toobin2RomName, NULL, NULL, NULL, NULL, ToobinInputInfo, ToobinDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	384, 512, 3, 4
};


// Toobin' (rev 1)

static struct BurnRomInfo toobin1RomDesc[] = {
	{ "1133-1j.061",	0x10000, 0xcaeb5d1b, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "1137-1f.061",	0x10000, 0x9713d9d3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1134-2j.061",	0x10000, 0x119f5d7b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1138-2f.061",	0x10000, 0x89664841, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1135-4j.061",	0x10000, 0x90477c4a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1139-4f.061",	0x10000, 0xa9f082a9, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1136-5j.061",	0x10000, 0x5ae3eeac, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1140-5f.061",	0x10000, 0xdacbbd94, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "1141-2k.061",	0x10000, 0xc0dcce1a, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code

	{ "1101-1a.061",	0x10000, 0x02696f15, 3 | BRF_GRA },           //  9 Background Tiles
	{ "1102-2a.061",	0x10000, 0x4bed4262, 3 | BRF_GRA },           // 10
	{ "1103-4a.061",	0x10000, 0xe62b037f, 3 | BRF_GRA },           // 11
	{ "1104-5a.061",	0x10000, 0xfa05aee6, 3 | BRF_GRA },           // 12
	{ "1105-1b.061",	0x10000, 0xab1c5578, 3 | BRF_GRA },           // 13
	{ "1106-2b.061",	0x10000, 0x4020468e, 3 | BRF_GRA },           // 14
	{ "1107-4b.061",	0x10000, 0xfe6f6aed, 3 | BRF_GRA },           // 15
	{ "1108-5b.061",	0x10000, 0x26fe71e1, 3 | BRF_GRA },           // 16

	{ "1143-10a.061",	0x20000, 0x211c1049, 4 | BRF_GRA },           // 17 Sprites
	{ "1144-13a.061",	0x20000, 0xef62ed2c, 4 | BRF_GRA },           // 18
	{ "1145-16a.061",	0x20000, 0x067ecb8a, 4 | BRF_GRA },           // 19
	{ "1146-18a.061",	0x20000, 0xfea6bc92, 4 | BRF_GRA },           // 20
	{ "1125-21a.061",	0x10000, 0xc37f24ac, 4 | BRF_GRA },           // 21
	{ "1126-23a.061",	0x10000, 0x015257f0, 4 | BRF_GRA },           // 22
	{ "1127-24a.061",	0x10000, 0xd05417cb, 4 | BRF_GRA },           // 23
	{ "1128-25a.061",	0x10000, 0xfba3e203, 4 | BRF_GRA },           // 24
	{ "1147-10b.061",	0x20000, 0xca4308cf, 4 | BRF_GRA },           // 25
	{ "1148-13b.061",	0x20000, 0x23ddd45c, 4 | BRF_GRA },           // 26
	{ "1149-16b.061",	0x20000, 0xd77cd1d0, 4 | BRF_GRA },           // 27
	{ "1150-18b.061",	0x20000, 0xa37157b8, 4 | BRF_GRA },           // 28
	{ "1129-21b.061",	0x10000, 0x294aaa02, 4 | BRF_GRA },           // 29
	{ "1130-23b.061",	0x10000, 0xdd610817, 4 | BRF_GRA },           // 30
	{ "1131-24b.061",	0x10000, 0xe8e2f919, 4 | BRF_GRA },           // 31
	{ "1132-25b.061",	0x10000, 0xc79f8ffc, 4 | BRF_GRA },           // 32

	{ "1142-20h.061",	0x04000, 0xa6ab551f, 5 | BRF_GRA },           // 33 Characters
};

STD_ROM_PICK(toobin1)
STD_ROM_FN(toobin1)

struct BurnDriver BurnDrvToobin1 = {
	"toobin1", "toobin", NULL, NULL, "1988",
	"Toobin' (rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, toobin1RomInfo, toobin1RomName, NULL, NULL, NULL, NULL, ToobinInputInfo, ToobinDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	384, 512, 3, 4
};
